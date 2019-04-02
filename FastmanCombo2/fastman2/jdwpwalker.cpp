#include "jdwpwalker.h"
#include "adbclient.h"
#include "zlog.h"

JdwpMessage::JdwpMessage() :
    data(true) {
    id = 0;
    flags = 0;
    cmdset = 0;
    cmd = 0;
    errcode = 0;
}

bool JdwpMessage::parse(ZByteArray &source) {
    DBG("-- recv <<< --\n");
    DBG_HEX(source.data(), source.size());
    int i = 0;
    if (source.length() < 11) {
        return false;
    }

    int len = source.getNextInt(i);
    if (source.length() < len) {
        DBG("parse fail!, length mismatch!\n");
        return false;
    }

    data.clear();
    id = source.getNextInt(i);
    flags = source.getNextByte(i);
    cmdset = source.getNextByte(i);
    cmd = source.getNextByte(i);
    i -= 2;
    errcode = source.getNextShort(i);

    char *p = source.data() + 11;
    data.append(p, len - 11);
    source.remove(0, len);

    return true;
}

ZByteArray JdwpMessage::getPacket() {
    ZByteArray ret(true);
    ret.putInt(11 + data.length());
    ret.putInt(id);
    ret.putByte(flags);
    ret.putByte(cmdset);
    ret.putByte(cmd);
    ret.append(data);
    DBG("-- >>> send --\n");
    DBG_HEX(ret.data(), ret.size());
    return ret;
}

JdwpWalker::JdwpWalker() {
    socket = NULL;
    msgId = 0;
}

JdwpWalker::~JdwpWalker() {
    vm_dettach();
}

void JdwpWalker::sendAndRecvMsg(JdwpMessage &msg) {
    ZByteArray data(true);
    char buf[4096];
    int n = -1;

    msg.setId(msgId++);
    socket->write(msg.getPacket());
    do {
        socket->waitForReadyRead(5000);
        n = socket->read(buf, 4096);
        if (n <= 0) {
            break;
        }
        data.append(buf, n);
    } while (!msg.parse(data));
}

bool JdwpWalker::vm_attach(VMInfo &info, int port) {
    vm_dettach();
    char *handshake = "JDWP-Handshake";
    char helomsg[] = { 'H', 'E', 'L', 'O', 0, 0, 0, 4, 0, 0, 0, 1 };
    JdwpMessage msg;
    socket = new QTcpSocket();

    socket->connectToHost("127.0.0.1", port);
    if (!socket->waitForConnected(2000)) {
        DBG("connect fail!\n");
        socket->abort();
        delete socket;
        socket = NULL;
        return false;
    }

    socket->write(handshake, strlen(handshake));
    socket->waitForReadyRead(1000);
    QByteArray resp = socket->readAll();
    DBG("resp: <%s>\n", resp.data());
    if (resp != "JDWP-Handshake") {
        return false;
    }

    msg.setCmd(CMDSET_DDM_VERSION);
    msg.data.append(helomsg, sizeof(helomsg));
    sendAndRecvMsg(msg);

    if (!msg.data.startsWith("HELO")) {
        DBG("error resp !\n");
        return false;
    }
    int i = 4; // after HELO
    int len = msg.data.getNextInt(i);
    if (msg.data.length() < len + 8) {
        return false;
    }

    info.ver = msg.data.getNextInt(i);
    info.pid = msg.data.getNextInt(i);
    int vmLen = msg.data.getNextInt(i);
    int pkgLen = msg.data.getNextInt(i);
    info.vm = msg.data.getNextUtf16(i, vmLen);
    info.pkg = msg.data.getNextUtf16(i, pkgLen);
    DBG("vm_attach success, vm '%s'\n", info.vm.toLocal8Bit().data());
    DBG("pid %d, name '%s'\n", info.pid, info.pkg.toLocal8Bit().data());
    return true;
}

//TODO deadcode for shell ps
bool JdwpWalker::vm_attach(VMInfo &info, const QString& pkg, const QString adb_serial, int port) {
    AdbClient adb(adb_serial);
    int n = -1;
    QMap < QString, QString > processNames;
    QString ret_pid;

    QByteArray psData = adb.adb_cmd("shell:ps");
    QList < QByteArray > processes = psData.split('\n');
    foreach(QByteArray process, processes) {
        if(process.isEmpty()) {
            continue;
        }

        while(process.endsWith('\r') || process.endsWith('\n')) {
            process.chop(1);
        }
        n = process.indexOf(' ', 10);
        QString pid = process.mid(10, n-10);
        QString name = process.mid(55);
        processNames.insert(pid, name);
    }

    QByteArray jdwpData = adb.adb_cmd("jdwp");
    QList < QByteArray > pids = jdwpData.split('\n');
    foreach(QByteArray pid, pids) {
        if(pid.isEmpty()) {
            continue;
        }

        while(pid.endsWith('\r') || pid.endsWith('\n')) {
            pid.chop(1);
        }
        QString name = processNames.value(pid);
        if(name == pkg) {
            DBG("pid '%s' name = '%s'\n", pid.data(), name.toLocal8Bit().data());
            ret_pid = pid;
            break;
        }
    }

    if (ret_pid.isEmpty()) {
        DBG("no matching pid found!\n");
        return NULL;
    }

    QString local = QString("tcp:%1").arg(port);
    QString remote = QString("jdwp:%1").arg(ret_pid);
    adb.adb_forward(local, remote);

    return vm_attach(info, port);
}

void JdwpWalker::vm_dettach() {
    if (socket != NULL) {
        socket->abort();
        delete socket;
        socket = NULL;
    }
}

bool JdwpWalker::vm_suspend() {
    JdwpMessage msg;
    msg.setCmd(CMDSET_VM_SUSPEND);
    sendAndRecvMsg(msg);
    return msg.errcode == 0;
}

bool JdwpWalker::vm_resume() {
    JdwpMessage msg;
    msg.setCmd(CMDSET_VM_RESUME);
    sendAndRecvMsg(msg);
    return msg.errcode == 0;
}

u64 JdwpWalker::getThreadID(const QString& name) {
    u64 ret = 0;
    JdwpMessage msg;
    JdwpMessage subMsg;

    msg.setCmd(CMDSET_LIST_THREAD);
    subMsg.setCmd(CMDSET_THREAD_NAME);

    sendAndRecvMsg(msg);

    int i = 0;
    int count = msg.data.getNextInt(i);
    while (count-- > 0) {
        u64 tid = msg.data.getNextInt64(i);
        int j = 0;
        subMsg.data.clear();
        subMsg.data.putInt64(tid);
        sendAndRecvMsg(subMsg);

        QString threadName = subMsg.data.getNextUtf8(j);
        if (threadName == name) {
            ret = tid;
            DBG("found threadID %llx -> %s\n", ret, name.toLocal8Bit().data());
            break;
        }
    }
    return ret;
}

u64 JdwpWalker::getClassID(const QString &name) {
    u64 ret = 0;
    JdwpMessage msg;

    msg.setCmd(CMDSET_LOOKUP_REF);
    msg.data.putUtf8(name);
    sendAndRecvMsg(msg);

    int i = 0;
    int count = msg.data.getNextInt(i);
    while (count-- > 0) {
        msg.data.getNextByte(i); // drop the tag, no need
        ret = msg.data.getNextInt64(i);
        DBG("found classID %llx -> %s\n", ret, name.toLocal8Bit().data());
        break; // no need anymore
    }

    return ret;
}

u32 JdwpWalker::getMethodID(u64 classID, const QString &name, const QString &signature) {
    u32 ret = 0;
    JdwpMessage msg;

    msg.setCmd(CMDSET_LOOKUP_METHOD);
    msg.data.putInt64(classID);
    sendAndRecvMsg(msg);

    int i = 0;
    int count = msg.data.getNextInt(i);
    while (count-- > 0) {
        u32 mId = msg.data.getNextInt(i);
        QString mName = msg.data.getNextUtf8(i);
        QString mSig = msg.data.getNextUtf8(i);
        msg.data.getNextUtf8(i); // drop general signature
        msg.data.getNextInt(i); // drop modbit

        if (mName == name && mSig == signature) {
            ret = mId;
            DBG("found methodID %x -> %s\n", ret, name.toLocal8Bit().data());
            break;
        }
    }
    return ret;
}

u32 JdwpWalker::getMethodID(const QString& cls_name, const QString& name, const QString& signature) {
    u64 cls_id = getClassID(cls_name);
    u32 ret = 0;
    if (cls_id != 0) {
        ret = getMethodID(cls_id, name, signature);
    }
    return ret;
}

u32 JdwpWalker::setBreakPoint(u64 threadID, u64 classID, u32 methodID) {
    u32 ret = 0;
    JdwpMessage msg;

    msg.setCmd(CMDSET_EVENT_SET);
    msg.data.putByte(2); // eventKind = EventKind.BREAKPOINT
    msg.data.putByte(1); // suspendPolicy
    msg.data.putInt(2); // modifiers count

    // modifier 1
    msg.data.putByte(3); // modKind 3 thread
    msg.data.putInt64(threadID);

    // modifier 2
    msg.data.putByte(7); // modKind 7 location // one byte type tag followed by a a classID followed by a methodID followed by an unsigned eight-byte index
    // location:
    // [1 type tag]
    // [8 classID]
    // [4 methodID]
    // [8 uint64 index]
    msg.data.putByte(1); // TypeTag.CLASS
    msg.data.putInt64(classID);
    msg.data.putInt(methodID);
    msg.data.putInt64(0);

    sendAndRecvMsg(msg);
    if (msg.data.length() == 4) {
        int i = 0;
        ret = msg.data.getNextInt(i);
    }
    DBG("==> setBreakPoint returned 0x%x\n", ret);
    return ret;
}

u32 JdwpWalker::getBreakPointEvent() {
    u32 ret = 0;
    JdwpMessage msg;

    ZByteArray data(true);
    char buf[4096];
    int n = -1;

    do {
        socket->waitForReadyRead(15000);
        n = socket->read(buf, 4096);
        if (n <= 0) {
            break;
        }
        data.append(buf, n);
    } while (!msg.parse(data));

    if (!msg.cmdsetEquals(CMDSET_EVENT_RESP) || msg.data.isEmpty()) {
        DBG("getBreakPointEvent failed!\n");
        return ret;
    }

    int i = 0;
    msg.data.getNextByte(i); // suspendPolicy
    int count = msg.data.getNextInt(i); // events
    while (count-- > 0) {
        u8 kind = msg.data.getNextByte(i); // eventKind
        if (kind == 2) { //EventKind.BREAKPOINT
            ret = msg.data.getNextInt(i);
        }
        break; // no need anymore
    }

    DBG("==> getBreakPointEvent returned 0x%x\n", ret);
    return ret;
}

//TODO deadcode for args and return value
u64 JdwpWalker::invokeStaticMethod(u64 threadID, u64 classID, u32 methodID, QList<JdwpValue *> &args) {
    u64 ret = 0;
    JdwpMessage msg;

    msg.setCmd(CMDSET_INVOKE_STATIC_METHOD);
    msg.data.putInt64(classID);
    msg.data.putInt64(threadID);
    msg.data.putInt(methodID);
    msg.data.putInt(args.count()); // arguments
    foreach(JdwpValue * arg, args)
    {
        msg.data.putByte(arg->tag);
        msg.data.putInt64(arg->objID);
    }
    msg.data.putInt(0); // options 0
    sendAndRecvMsg(msg);

    int i = 1;
    ret = msg.data.getNextInt64(i);
    DBG("==> invokeStaticMethod got 0x%llx\n", ret);
    return ret;
}

//TODO deadcode for args and return value
u64 JdwpWalker::invokeObjectMethod(u64 threadID, u64 classID, u32 methodID, u64 objID, QList<JdwpValue *>& args) {
    u64 ret = 0;
    JdwpMessage msg;

    msg.setCmd(CMDSET_INVOKE_OBJ_METHOD);
    msg.data.putInt64(objID);
    msg.data.putInt64(threadID);
    msg.data.putInt64(classID);
    msg.data.putInt(methodID);
    msg.data.putInt(args.count());
    foreach(JdwpValue * arg, args)
    {
        msg.data.putByte(arg->tag);
        msg.data.putInt64(arg->objID);
    }
    msg.data.putInt(0); // options 0
    sendAndRecvMsg(msg);

    int i = 1;
    ret = msg.data.getNextInt64(i);
    DBG("==> invokeObjectMethod got 0x%llx\n", ret);
    return ret;
}

u64 JdwpWalker::newStringObject(const QString &str) {
    u64 ret = 0;
    JdwpMessage msg;

    msg.setCmd(CMDSET_NEW_STRINGOBJ);
    msg.data.putUtf8(str);
    sendAndRecvMsg(msg);

    int i = 0;
    ret = msg.data.getNextInt64(i);
    DBG("==> newStringObject got 0x%llx\n", ret);
    return ret;
}

//TODO deadcode for return value
u64 JdwpWalker::newArrayInstance(u64 typeID, int length) {
    u64 ret = 0;
    JdwpMessage msg;

    msg.setCmd(CMDSET_ARRAY_NEWINSTANCE);
    msg.data.putInt64(typeID);
    msg.data.putInt(length);
    sendAndRecvMsg(msg);

    int i = 1;
    ret = msg.data.getNextInt64(i);
    DBG("==> newArrayInstance got 0x%llx\n", ret);
    return ret;
}

void JdwpWalker::setArrayValues(u64 arrayID, int from, QList<JdwpValue *> &values) {
    JdwpMessage msg;

    msg.setCmd(CMDSET_SET_ARRAY_VALUE);
    msg.data.putInt64(arrayID);
    msg.data.putInt(from);
    msg.data.putInt(values.size());
    foreach(JdwpValue * val, values)
    {
        msg.data.putInt64(val->objID);
    }

    sendAndRecvMsg(msg);
}

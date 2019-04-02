#ifndef JDWPWALKER_H
#define JDWPWALKER_H

#include "zbytearray.h"
#include <QTcpSocket>

#define CMDSET_DDM_VERSION   0xc7,0x01
#define CMDSET_VM_SUSPEND    0x01,0x08
#define CMDSET_VM_RESUME     0x01,0x09
#define CMDSET_LIST_THREAD   0x01,0x04
#define CMDSET_THREAD_NAME   0x0b,0x01
#define CMDSET_LOOKUP_REF    0x01,0x02
#define CMDSET_LOOKUP_METHOD 0x02,0x0f
#define CMDSET_LINETABLE     0x06,0x01
#define CMDSET_EVENT_SET     0x0f,0x01
#define CMDSET_EVENT_RESP    0x40,0x64
#define CMDSET_INVOKE_STATIC_METHOD  0x03,0x03
#define CMDSET_INVOKE_OBJ_METHOD     0x09,0x06
#define CMDSET_ARRAY_NEWINSTANCE 0x04,0x01
#define CMDSET_NEW_STRINGOBJ 0x01,0x0b
#define CMDSET_SET_ARRAY_VALUE 0x0d,0x03

class JdwpMessage {
public:
    u32 id;
    u8 flags;
    u8 cmdset;
    u8 cmd;
    u16 errcode;
    ZByteArray data;

    inline void setId(u32 id) {
        this->id = id;
    }

    inline void setFlags(u8 flags) {
        this->flags = flags;
    }

    inline void setCmd(u8 cmdset, u8 cmd) {
        this->cmdset = cmdset;
        this->cmd = cmd;
        u8 tmp[2];
        tmp[1] = cmdset;
        tmp[0] = cmd;
        memcpy(&errcode, tmp, 2);
    }

    inline bool cmdsetEquals(u8 cmdset, u8 cmd) {
        return (this->cmdset == cmdset && this->cmd == cmd);
    }

    inline void setErrorCode(u16 errcode) {
        this->errcode = errcode;
        u8 tmp[2];
        memcpy(tmp, &errcode, 2);
        this->cmdset = tmp[1];
        this->cmd = tmp[0];
    }

    JdwpMessage();
    bool parse(ZByteArray &source);
    ZByteArray getPacket();
};

class VMInfo {
public:
    u32 ver;
    u32 pid;
    QString vm;
    QString pkg;
};

typedef struct JdwpValue {
    u8 tag; // JDWP.Tag http://docs.oracle.com/javase/6/docs/platform/jpda/jdwp/jdwp-protocol.html#JDWP_Tag
    quint64 objID;
} JdwpValue;

class JdwpWalker {
    QTcpSocket *socket;
    int msgId;

    void sendAndRecvMsg(JdwpMessage &msg);
public:
    JdwpWalker();
    ~JdwpWalker();

    bool vm_attach(VMInfo &info, int port);
    bool vm_attach(VMInfo &info, const QString &pkg, const QString adb_serial = QString(), int port = 18201);
    void vm_dettach();

    bool vm_suspend();
    bool vm_resume();

    u64 getThreadID(const QString& name);
    u64 getClassID(const QString& name);
    u32 getMethodID(u64 classID, const QString& name, const QString& signature);
    u32 getMethodID(const QString& cls_name, const QString& name, const QString& signature);

    u32 setBreakPoint(u64 threadID, u64 classID, u32 methodID);
    u32 getBreakPointEvent();

    u64 invokeStaticMethod(u64 threadID, u64 classID, u32 methodID, QList<JdwpValue*> &args);
    u64 invokeObjectMethod(u64 threadID, u64 classID, u32 methodID, u64 objID, QList<JdwpValue *> &args);

    u64 newStringObject(const QString& str);
    u64 newArrayInstance(u64 typeID, int length);

    void setArrayValues(u64 arrayID, int from, QList<JdwpValue*> &values);
};

#endif // JDWPWALKER_H

#include "fastmansocket.h"
#include "adbclient.h"
#include <QTime>
#include "zlog.h"

FastManSocket::FastManSocket(const QString &adb_serial, int port)
{
    this->adb_serial = adb_serial;
    this->port = port;

    AdbClient adb(adb_serial);
    adb.adb_forward(QString("tcp:%1").arg(port), QString("tcp:%1").arg(ZM_REMOTE_PORT));
}

FastManSocket::~FastManSocket() {
    if(state() == QTcpSocket::ConnectedState) {
        disconnectFromHost();
    }
    abort();
}

bool FastManSocket::connectToZMaster() {
    if(state() == QTcpSocket::ConnectedState) {
        return true;
    }
    connectToHost("127.0.0.1", port);
    if(!waitForConnected(500)) {
        abort();
        return false;
    }
    return true;
}

QByteArray FastManSocket::readx(int len, int timeout) {
    QByteArray ret;
    char buf[4096];
	int n = -1;
    int left = len;
    QTime time;
    time.start();

    while(left > 0) {
        n = read(buf, left > 4096 ? 4096 : left);
        if(n > 0) {
            ret.append(buf, n);
            left -= n;
            if(left <= 0) {
                break;
            }
        } else if(this->state() != QTcpSocket::ConnectedState) {
            break;
        } else if(timeout > 0 && time.elapsed() > timeout) {
            break;
        }
        waitForReadyRead(200);
    }
    //DBG("read %d/%d\n", ret.size(), len);
    return ret;
}

bool FastManSocket::recvMsg(ZMsg2 &msg, int timeout) {
    if(!connectToZMaster()) {
        DBG("connect to zm failed!\n");
        return false;
    }

    ZByteArray packet(false);
    QByteArray data = readx(8, timeout);
    if(data.size() != 8) {
        DBG("read head error!\n");
        return false;
    }
    packet.append(data);

    int i = 0;
    u32 magic = packet.getNextInt(i);
    u32 left = packet.getNextInt(i);
    if(magic != ZMSG2_MAGIC) {
        DBG("invalid magic!\n");
        DBG_HEX(packet.data(), 8);
        return false;
    }
    left += 2; // + crc32
    data = readx(left, timeout * 3);
    if(data.size() != left) {
        DBG("invalid read left\n");
        return false;
    }
    packet.append(data);

    return msg.parse(packet);
}

bool FastManSocket::sendAndRecvMsg(ZMsg2 &msg, int timeout) {
    if(!connectToZMaster()) {
        DBG("once connect to zm failed!\n");
        return false;
    }

    ZByteArray packet = msg.getPacket();
    write(packet);

    return recvMsg(msg, timeout);
}

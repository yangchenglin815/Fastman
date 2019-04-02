#ifndef FASTMANSOCKET_H
#define FASTMANSOCKET_H

#include <QString>
#include <QTcpSocket>
#include "zmsg2.h"

#define ZM_REMOTE_PORT 19001

class FastManSocket : public QTcpSocket {
    QString adb_serial;
    int port;
public:
    FastManSocket(const QString& adb_serial, int port);
    ~FastManSocket();

    bool connectToZMaster();
    QByteArray readx(int len, int timeout = 5000);
    bool recvMsg(ZMsg2& msg, int timeout = 5000);
    bool sendAndRecvMsg(ZMsg2& msg, int timeout = 5000);
};

#endif // FASTMANSOCKET_H

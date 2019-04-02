#ifndef ZLOCALSERVER_H
#define ZLOCALSERVER_H

#include <QObject>

class QLocalServer;
class QLocalSocket;
class QByteArray;
class ZLocalServer : public QObject
{
    Q_OBJECT
signals:
    void newConnection();

public:
    explicit ZLocalServer(QString const &name, QObject *parent = 0);
    ~ZLocalServer();

    int listen();
    bool write(const QByteArray &data);
    bool read(QByteArray &out);

private:
    QLocalServer *m_server;
    QString m_serverName;
    QLocalSocket *m_client;
};

#endif // ZLOCALSERVER_H

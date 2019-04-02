#ifndef ACTIVATEWINDOW_H
#define ACTIVATEWINDOW_H

#include <QObject>

// client
class QLocalServer;
class QLocalSocket;
class CLocalClient : public QObject
{
    Q_OBJECT

public:
    explicit CLocalClient(QObject *parent = 0);
    bool connectToServer(QLocalSocket *socket, const QString serverName);

public slots:
    void slot_sendMsg(QLocalSocket *socket, const QString &msg);
    void slot_exitHelper();
};


// server
class CLocalServer : public QObject
{
    Q_OBJECT

    QLocalServer *m_server;

public:
    explicit CLocalServer(QObject *parent = 0);
    ~CLocalServer();

    bool listen(const QString &serverName);
    void slot_sendMsg(QLocalSocket *socket, const QString &msg);

signals:
    void signal_activateWindow();

private slots:
    void slot_dealConnection();
    void slot_readMsg();
};
#endif // ACTIVATEWINDOW_H

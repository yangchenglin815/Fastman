#include "activatewindow.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QMutex>
#include <QApplication>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include "zlog.h"

#define SERVERNAME_MMQT2 "MMQT2_SERVER"
#define SERVERNAME_TRACKER "TRACKER_SERVER"

static long Mmqt2Pid =  -1;
static long HelperPid = -1;
static bool IsMmqtRunning = true;
static bool IsHelperRunning = true;

#ifdef _WIN32
#include <cstdlib>
#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

// client
CLocalClient::CLocalClient(QObject *parent) :
    QObject(parent)
{
}

bool CLocalClient::connectToServer(QLocalSocket *socket, const QString serverName) {
    bool ret = false;
    socket->abort();
    socket->connectToServer(serverName);
    if (socket->waitForConnected(500)) {
        ret = true;
    }
    return ret;
}

void CLocalClient::slot_sendMsg(QLocalSocket *socket, const QString &msg) {
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    out << msg;
    out.device()->seek(0);
    socket->write(block);
    socket->flush();
}

void CLocalClient::slot_exitHelper() {
    QLocalSocket *socketHelper = new QLocalSocket;
    if (connectToServer(socketHelper, SERVERNAME_TRACKER)) {
        slot_sendMsg(socketHelper, "QUIT_HELPER");
    }
}

// server
CLocalServer::CLocalServer(QObject *parent) :
    QObject(parent)
  , m_server(0)
{
    listen(SERVERNAME_MMQT2);
}

CLocalServer::~CLocalServer() {
    m_server->close();
    delete m_server;
}

bool CLocalServer::listen(const QString &serverName) {
    QLocalSocket ls;
    ls.connectToServer(serverName);
    if (ls.waitForConnected(500)) {
        DBG("mmqt2 Server has run.\n");
        ls.disconnectFromServer();
        ls.close();
        return true;
    }

    m_server = new QLocalServer();
    m_server->removeServer(serverName);
    if (!m_server->listen(serverName)) {
        DBG("mmqt2 Server listen error: %s\n", m_server->errorString().toLocal8Bit().data());
        return false;
    }

    connect(m_server, SIGNAL(newConnection()), this, SLOT(slot_dealConnection()));
    return true;
}

void CLocalServer::slot_dealConnection() {
    QLocalSocket *socket = m_server->nextPendingConnection();
    connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(slot_readMsg()));
}

void CLocalServer::slot_readMsg() {
    QLocalSocket *socket = (QLocalSocket *)sender();
    if (socket) {
        QDataStream in(socket);
        in.setVersion(QDataStream::Qt_4_0);
        if (socket->bytesAvailable() < (int)sizeof(quint16)) {
            return;
        }

        QString msg;
        in >> msg;
        if (msg == "DEVICE_CHANGED") {
            socket->disconnectFromServer();
            emit signal_activateWindow();
        } else if (msg == "QUIT_ALL") {
            DBG("Send mmqt2's pid.\n");
            ulong currentPid;
#ifdef _WIN32
            currentPid = GetCurrentProcessId();
#else
            currentPid = getpid();
#endif
            QString pidMsg = QString("%1%2").arg("PID_MMQT2_").arg(QString::number(currentPid));
            slot_sendMsg(socket, pidMsg);

            DBG("Quit mmqt2.\n");
            qApp->quit();
        }
    }
}

void CLocalServer::slot_sendMsg(QLocalSocket *socket, const QString &msg) {
    QByteArray block;
    QDataStream out(&block, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_4_0);
    out << msg;
    out.device()->seek(0);
    socket->write(block);
    socket->flush();
}

#include "zlocalserver.h"
#include "zlocalclient.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QByteArray>
#include <QTimer>
#include "zlog.h"

ZLocalServer::ZLocalServer(const QString &name, QObject *parent) :
    QObject(parent)
  , m_serverName(name)
  , m_server(0)
  , m_client(0)
{
    m_server = new QLocalServer(this);
    QObject::connect(m_server, SIGNAL(newConnection()), this, SIGNAL(newConnection()));
}

ZLocalServer::~ZLocalServer() {
    m_server->close();
}

int ZLocalServer::listen() {
    m_server->removeServer(m_serverName);
    if (!m_server->listen(m_serverName)) {
        DBG("Server listen error: %s\n", m_server->errorString().toLocal8Bit().data());
        return m_server->serverError();
    }
    return 0;
}

bool ZLocalServer::write(const QByteArray &data) {
    if (!m_client) {
        return false;
    }

    m_client->write(data);
    return m_client->waitForBytesWritten(3000);
}

bool ZLocalServer::read(QByteArray &out) {
    m_client =  m_server->nextPendingConnection();
    if (!m_client) {
        return false;
    }

    if (m_client->bytesAvailable() < (int)sizeof(quint16)) {
        m_client->waitForReadyRead(3000);
    }
    out = m_client->readAll();
    if (out.isEmpty()){
        return false;
    } else {
        return true;
    }
}


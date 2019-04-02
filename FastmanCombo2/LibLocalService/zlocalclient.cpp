#include "zlocalclient.h"
#include <QLocalSocket>
#include <QByteArray>
#include "zlog.h"

ZLocalClient::ZLocalClient(const QString &name, QObject *parent) :
    QObject(parent)
  , m_serverName(name)
  , m_socket(0)
{
    m_socket = new QLocalSocket(this);
    QObject::connect(m_socket, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
}

int ZLocalClient::connect() {
    m_socket->abort();
    m_socket->connectToServer(m_serverName);
    if (!m_socket->waitForConnected(500)) {
        m_socket->abort();
        DBG("Connect to server error: %d\n", m_socket->error());
        return m_socket->error();
    }
    return 0;
}

bool ZLocalClient::write(const QByteArray &data) {
    if (m_socket->state() != QLocalSocket::ConnectedState) {
        DBG("Client write error: %d\n", m_socket->state());
        return false;
    }

    m_socket->write(data);
    return m_socket->waitForBytesWritten(3000);
}

bool ZLocalClient::read(QByteArray &out) {
    if (m_socket->state() != QLocalSocket::ConnectedState)
        return false;

    if (m_socket->bytesAvailable() < (int)sizeof(quint16)) {
        m_socket->waitForReadyRead(3000);
    }
    out = m_socket->readAll();
    if (out.isEmpty()){
        return false;
    } else {
        return true;
    }
}

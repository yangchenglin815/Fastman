#ifndef ZLOCALCLIENT_H
#define ZLOCALCLIENT_H

#include <QObject>

class QLocalSocket;
class QByteArray;
class ZLocalClient : public QObject
{
    Q_OBJECT
signals:
    void readyRead();

public:
    explicit ZLocalClient(const QString &name, QObject *parent = 0);

    int connect();

    bool write(const QByteArray &data);
    bool read(QByteArray &out);

private:
    QLocalSocket *m_socket;
    QString m_serverName;
};

#endif // ZLOCALCLIENT_H

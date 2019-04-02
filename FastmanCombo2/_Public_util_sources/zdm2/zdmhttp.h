#ifndef ZDMHTTP_H
#define ZDMHTTP_H

#include <QDataStream>
#include <QByteArray>
#include <QStringList>

class ZDMHttp
{
public:    
    static bool global_init();
    static bool url_escape(const QByteArray &in, QString &out, bool preserve = false);

    static int getContentLength(char *url, long& resp_code, qint64 &content_length);

    static int get(char *url, QDataStream &out);
    static int get(char *url, QByteArray &out);

    static int post(char *url, char *params, QDataStream &out);
    static int post(char *url, char *params, QByteArray &out);

    static int postFile(char *url, const QString& path, QDataStream &out);
    static int postFile(char *url, const QString& path, QByteArray &out);

    static int postData(char *url, QByteArray &in, QStringList &headers, QByteArray &out);
};

#endif // ZDMHTTP_H

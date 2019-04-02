#ifndef ZDMHTTP_H
#define ZDMHTTP_H

#include <QDataStream>
#include <QByteArray>
#include <QStringList>

class ZDMHttp
{
    typedef size_t ZDMHTTP_WRITE_FUNC(const char *buf, size_t size, size_t nmemb, void *p);
public:    
    static bool global_init();
    static bool url_escape(const QByteArray &in, QString &out, bool preserve = false);

    static int getContentLength(char *url, long& resp_code, qint64 &content_length);
    static int getDownloadData(char *url, qint64 from, qint64 to, ZDMHTTP_WRITE_FUNC func, void *p);

    static int get(char *url, QDataStream &out);
    static int get(char *url, QByteArray &out);

    static int post(char *url, char *params, QDataStream &out);
    static int post(char *url, char *params, QByteArray &out);

    static int postFile(char *url, const QString& path, QDataStream &out);
    static int postFile(char *url, const QString& path, QByteArray &out);
    static int postFileByMutltiformpost(char *url, const QString &path, QByteArray &out);
    static int postJson(char *url, char *jsonData, QByteArray &out);
    static int postJson(char *url, char *jsonData, QDataStream &out);

    static int postData(char *url, QByteArray &in, QStringList &headers, QByteArray &out);
};

#endif // ZDMHTTP_H

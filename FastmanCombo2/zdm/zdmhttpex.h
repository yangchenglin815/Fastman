#ifndef ZDMHTTPEX_H
#define ZDMHTTPEX_H

#include <QObject>
#include <QDataStream>
#include <QByteArray>
#include <QStringList>

class ZDMHttpEx : public QObject {
    Q_OBJECT

    bool _isPost;
    void *_arg;
public:
    static bool global_init();
    static bool url_escape(const QByteArray &in, QString &out, bool preserve = false);

    ZDMHttpEx(QObject *parent = 0);

    inline bool isPost() {
        return _isPost;
    }

    inline void setArg(void *p) {
        _arg = p;
    }

    inline void emit_progress(int value, int total) {
        emit signal_progress(_arg, value, total);
    }

    int getContentLength(const QString &url, long& resp_code, qint64 &content_length);

    int get(const QString &url, QDataStream &out);
    int get(const QString &url, QByteArray &out);
    int getFile(const QString &url, const QString& path);

    int post(const QString &url, const QString &params, QDataStream &out);
    int post(const QString &url, const QString &params, QByteArray &out);

    int postFile(const QString &url, const QString& path, QDataStream &out);
    int postFile(const QString &url, const QString& path, QByteArray &out);

    int postData(const QString &url, QByteArray &in, QStringList &headers, QByteArray &out);
signals:
    void signal_progress(void *p, int value, int total);
    void signal_result(void *p, bool ret, const QString& hint);
};

#endif

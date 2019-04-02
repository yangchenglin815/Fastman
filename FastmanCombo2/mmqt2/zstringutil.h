#ifndef ZSTRINGUTIL_H
#define ZSTRINGUTIL_H

#include <QString>
#include <QByteArray>

class ZStringUtil
{
public:
    static QString getSizeStr(quint64 size);
    static QString getTimeStr(quint64 time);
    static QString getPinyin(const QString& name);

    static QString fromGBK(const QByteArray& data);
    static QByteArray toGBK(const QString& in);

    static QString fromUTF8(const QByteArray& data);
    static QByteArray toUTF8(const QString& in);
};

#endif // ZSTRINGUTIL_H

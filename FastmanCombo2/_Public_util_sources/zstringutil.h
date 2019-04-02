#ifndef ZSTRINGUTIL_H
#define ZSTRINGUTIL_H

#include <QString>
#include <QByteArray>

#define tohex(x) ((x)>9 ? (x)+87 : (x)+48)

#define SAFE_DELETE_ARRAY(p)	\
    {	\
        if ((p))	\
        {	\
            delete [] (p);	\
            (p) = NULL;	\
        }	\
    }

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

    static QString getBaseName(const QString& in);
    static QStringList getEntryBaseNames(const QStringList &entries);
    static QString getEntryName(const QStringList &entries, const QString baseName);

    static QString urlEncode(const QString &in);

    static QString base64Encode(const QString &in);
    static QString base64Decode(const QString &in);

};

#endif // ZSTRINGUTIL_H

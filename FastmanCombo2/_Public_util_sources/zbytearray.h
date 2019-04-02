#ifndef ZBYTEARRAY_H
#define ZBYTEARRAY_H

#include <QByteArray>
#include <QString>
#include <QTextCodec>
#define TR_G(x) QTextCodec::codecForLocale()->toUnicode(x)
#define TR_C(x) (x).toLocal8Bit().data()

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef quint64 u64;

class ZByteArray: public QByteArray {
    bool reverse;
public:
    ZByteArray(bool bigEndian);
    u16 checksum();
    u16 checksum(int from, int length);
    int indexOf(void *data, int len);

    u8 getNextByte(int& i);
    u16 getNextShort(int& i);
    u32 getNextInt(int& i);
    u64 getNextInt64(int &i);
    QString getNextUtf8(int& i);
    QString getNextUtf8(int &i, int len);
    QString getNextUtf16(int &i);
    QString getNextUtf16(int &i, int len);

    void putByte(u8 b);
    void putShort(u16 s);
    void putInt(u32 i32);
    void putInt64(u64 i64);
    void putUtf8(const QString& str, bool addHead = true);
    void putUtf16(const QString& str, bool addHead = true);

    void encode(ZByteArray& out);
    bool decode(ZByteArray& out);
    static QString encode(const QString& in);
    static QString decode(const QString& in);
    static void encode_bytearray(QByteArray& in);
    static void decode_bytearray(QByteArray& in);
};

#endif // ZBYTEARRAY_H

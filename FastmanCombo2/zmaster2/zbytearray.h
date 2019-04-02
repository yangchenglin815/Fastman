#ifndef ZBYTEARRAY_H
#define ZBYTEARRAY_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;

class ZByteArray
{
    char *_data;
    int _size;

    bool reverse;
public:
    ZByteArray(bool bigEndian);
    ~ZByteArray();

    inline int length() {
        return _size;
    }

    inline int size() {
        return _size;
    }

    inline char at(int i) {
        return *(_data + i);
    }

    inline char operator [](int i) {
        return *(_data + i);
    }

    inline char *data() {
        return _data;
    }

    void append(char c);
    void append(const char *str);
    void append(void *data, int len);
    void append(ZByteArray& other);
    void clear();
    void remove(int from, int len);
    void resize(int len);
    void chop(int n);

    int indexOf(const void *data, int len);
    inline int indexOf(ZByteArray& key) {
        return indexOf(key._data, key._size);
    }

    int indexOf(int from, const void *data, int len);
    inline int indexOf(int from, ZByteArray& key) {
        return indexOf(from, key._data, key._size);
    }

    bool endsWith(const void *data, int len);
    inline bool endsWith(ZByteArray& key) {
        return endsWith(key._data, key._size);
    }

    u16 checksum();
    u16 checksum(int from, int length);

    u8 getNextByte(int& i);
    u16 getNextShort(int& i);
    u32 getNextInt(int& i);
    u64 getNextInt64(int &i);
    char *getNextUtf8(int &i);
    char *getNextUtf8(int &i, int len);
    char *getNextUtf16(int &i, int len);

    void putByte(u8 b);
    void putShort(u16 s);
    void putInt(u32 i32);
    void putInt64(u64 i64);
    void putUtf8(char *src, bool addHead = true);
};

#endif // ZBYTEARRAY_H

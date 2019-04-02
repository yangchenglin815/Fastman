#ifndef ZIPHELPER_H
#define ZIPHELPER_H

#include <QString>
#include <QList>

class ZipEntry {
public:
    ZipEntry();
    int compress_level;
    QString name;
    QByteArray data;
};

class ZipHelper
{
public:
    QList<ZipEntry *> entries;
    ZipHelper();
    ~ZipHelper();

    bool hasExistEntry(const QString &name);
    bool readZipFile(const QString& path);
    bool writeZipFile(const QString& path, bool append = false);
    
    static bool readEntryList(const QString& path, QStringList& list);
    static bool readFileInZip(const QString& path, const QString& entryname, QByteArray& out);
    static bool extractZip(const QString& path, const QString& dest);
};

#endif // ZIPHELPER_H

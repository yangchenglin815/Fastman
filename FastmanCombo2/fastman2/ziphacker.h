#ifndef ZIPHACKER_H
#define ZIPHACKER_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QFile>

// http://en.wikipedia.org/wiki/ZIP_%28file_format%29#Standardization
// http://www.pkware.com/documents/casestudies/APPNOTE.TXT

class ZipHackerEntry {
public:
    int compress_level;
    QString name;
    QByteArray data;
};

class ZipHacker
{
    bool hasExistEntry(const QString &name);
    bool hackEOCDfor9695860(const QString &path);
public:
    enum {
        MODE_DEFAULT = 0,
        MODE_9950697,
        MODE_9695860
    };

    QList<ZipHackerEntry *> entries;
    QList<ZipHackerEntry *> hackedEntries; // ignored when hackMode == MODE_DEFAULT
    ZipHacker();
    ~ZipHacker();

    // normal behaviors
    bool readZipFile(const QString& path, bool followbug = false, bool *hasBug = NULL);
    bool writeZipFile(const QString& path, int hackMode = MODE_DEFAULT, bool append = false);
};

#endif // ZIPHACKER_H

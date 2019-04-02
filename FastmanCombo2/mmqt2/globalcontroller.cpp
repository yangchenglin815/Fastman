#include "globalcontroller.h"

#include "adbdevicenode.h"

#include <QStringList>
#include <QCryptographicHash>
#include <QByteArray>
#include <QFile>
#include <stdio.h>

#include "ziphelper.h"
#include "zlog.h"

GlobalController::GlobalController()
{
    DBG("+GlobalController %p\n", this);
    needsQuit = false;
    threadpool = NULL;
    appconfig = NULL;
    downloadmanager = NULL;
    adbTracker = NULL;
    loginManager = NULL;
}

GlobalController::~GlobalController() {
    DBG("~GlobalController %p\n", this);
}

GlobalController* GlobalController::getInstance() {
    static GlobalController instance;
    return &instance;
}

bool GlobalController::getFileMd5(const QString& path, QString& dest_md5, quint64& dest_size) {
    QFile f(path);
    QByteArray d;
    dest_size = 0;

    QCryptographicHash md5(QCryptographicHash::Md5);
    if(f.open(QIODevice::ReadOnly)) {
        while((d = f.read(8192)).size() > 0) {
            md5.addData(d);
            dest_size += d.size();
        }
        f.close();

        dest_md5 = md5.result().toHex().toUpper();
        return true;
    }
    return false;
}

bool GlobalController::getApkRsaMd5(const QString &path, QString &dest_md5) {
    QStringList entries;
    QString retEntry;

    if(!ZipHelper::readEntryList(path, entries)) {
        DBG("readEntryList fail\n");
        return false;
    }

    foreach(const QString& entry, entries) {
        if(entry.startsWith("META-INF/") && entry.endsWith(".RSA")) {
            DBG("found entry <%s>\n", entry.toLocal8Bit().data());
            retEntry = entry;
            break;
        }
    }

    if(retEntry.isEmpty()) {
        DBG("cannot find valid rsa entry\n");
        return false;
    }

    QByteArray d;
    if(!ZipHelper::readFileInZip(path, retEntry, d)) {
        DBG("cannot read rsa entry\n");
        return false;
    }
    d.resize(512);

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(d);
    dest_md5 = md5.result().toHex();
    return true;
}

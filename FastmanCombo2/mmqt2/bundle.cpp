#include "bundle.h"
#include "zdownloadmanager.h"
#include "zstringutil.h"
#include "adbprocess.h"
#ifndef NO_GUI
#include <QIcon>
#include <QPixmap>
#include <QImage>
#endif
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include "ziphelper.h"
#include "globalcontroller.h"
#include "zlog.h"

BundleItem::BundleItem()
    : isHiddenItem(false) {
    size = 0;
    versionCode = -1;
    instLocation = -1;
    minSdk = 0;
    isIme = false;
    download_status = ZDownloadTask::STAT_PENDING;
    mtime = 0;
}

static bool copy_aapt_fields(const QByteArray src, int count, ...) {
    int L = 0, R = 0;
    bool ret = true;

    va_list args;
    va_start(args, count);
    for(int i=0; i<count; i++) {
        QByteArray* dest = va_arg(args, QByteArray*);
        L = src.indexOf('\'', L);
        if(L == -1) {
            ret = false;
            break;
        }
        R = src.indexOf('\'', ++L);
        if(R == -1) {
            ret = false;
            break;
        }

        dest->clear();
        dest->append(src.mid(L, R-L));
        L = ++R;
    }
    va_end(args);
    return ret;
}

bool BundleItem::parseApk(const QString &apk_path, bool full_parse) {
    AdbProcess p;
    QByteArray stdOut, stdErr;
    p.exec_cmd("aapt d badging \""+apk_path+"\"", stdOut, stdErr);

    QList<QByteArray> linesData = stdOut.split('\n');
    QByteArray b1,b2,b3;
    QString icon_path;
    foreach(const QByteArray& line, linesData) {
        if(line.startsWith("package:")) {
            copy_aapt_fields(line, 3, &b1, &b2, &b3);
            pkg = QString::fromUtf8(b1);
            versionCode = b2.toInt();
            version = QString::fromUtf8(b3);
        } else if(line.startsWith("sdkVersion:")) {
            copy_aapt_fields(line, 1, &b1);
            minSdk = b1.toInt();
        } else if(line.startsWith("application-label:")
                  || line.startsWith("application-label-zh:")
                  || line.startsWith("application-label-zh_CN:")) {
            copy_aapt_fields(line, 1, &b1);
            name = QString::fromUtf8(b1);
        } else if(line.startsWith("application-icon-160:")
                  || line.startsWith("application-icon-240:")
                  || line.startsWith("application-icon-320:")) {
            copy_aapt_fields(line, 1, &b1);
            icon_path = QString::fromUtf8(b1);
        }
    }

    p.exec_cmd("aapt d xmlstrings \""+apk_path+"\" AndroidManifest.xml", stdOut, stdErr);
    isIme = stdOut.contains("android.permission.BIND_INPUT_METHOD");
    if(icon_path.isEmpty()
            || !ZipHelper::readFileInZip(apk_path, icon_path, iconData)) {
        return false;
    }
#ifndef NO_GUI
    icon = QImage::fromData(iconData);
#endif
    if(full_parse) {
        path = apk_path;
        mtime = QDateTime::currentDateTime().toTime_t();
        GlobalController::getFileMd5(path, md5, size);
    }
    return true;
}

Bundle::Bundle() {
    size = 0;
    enable = true;
    readonly = false;
    type = TYPE_APK;
    leastMem = 0;
    leastStore = 0;
}

Bundle::~Bundle() {
    // items are shared, delete in globalBundle
    while(!bundleList.isEmpty()) {
        delete bundleList.takeFirst();
    }
}
#ifndef NO_GUI
QPixmap Bundle::getIcon(int index) {
    static char *icons[] = {
        ":/skins/skins/icon_bundle_0.png",
        ":/skins/skins/icon_bundle_1.png",
        ":/skins/skins/icon_bundle_2.png",
        ":/skins/skins/icon_bundle_3.png",
        ":/skins/skins/icon_bundle_4.png"
    };

    QPixmap icon(icons[index%5]);
    return icon;
}
#endif
QString Bundle::getDescription() {
    return QObject::tr("%1 apps, %2")
            .arg(appList.count())
            .arg(ZStringUtil::getSizeStr(size));
}

BundleItem *Bundle::getApp(const QString& id) {
    BundleItem *ret = NULL;
    appMutex.lock();
    foreach(BundleItem *a, appList) {
        if(a->id == id) {
            ret = a;
            break;
        }
    }
    appMutex.unlock();
    return ret;
}

Bundle *Bundle::getBundle(const QString& id) {
    Bundle *ret = NULL;
    bundleMutex.lock();
    foreach(Bundle *b, bundleList) {
        if(b->id == id) {
            ret = b;
            break;
        }
    }
    bundleMutex.unlock();
    return ret;
}

QStringList Bundle::getForceItemIds() {
    QStringList ret;
    bundleMutex.lock();
    foreach(Bundle *b, bundleList) {
        if(b->type == TYPE_FORCE) {
            selBundleID = b->id ;
            ret.append(b->getAppIds());
        }
    }
    bundleMutex.unlock();
    return ret;
}

Bundle *Bundle::getAggBundle(int memFreeMB, int storeFreeMB) {
    Bundle *ret = NULL;
    quint64 cur_size = 0; // get the biggest bundle at given conditions
    bundleMutex.lock();
    foreach(Bundle *b, bundleList) {
        if(b->type == TYPE_AGG) {
            if(memFreeMB >= b->leastMem && storeFreeMB >= b->leastStore) {
                if(b->size > cur_size) {
                    ret = b;
                    cur_size = b->size;
                }
            }
        }
    }
    bundleMutex.unlock();
    DBG("getAggBundle %s\n", ret != NULL ? ret->name.toLocal8Bit().data() : "(null)");
    return ret;
}

QList<BundleItem*> Bundle::getAppList() {
    QList<BundleItem*> list;
    appMutex.lock();
    list.append(appList);
    appMutex.unlock();
    return list;
}

QList<BundleItem*> Bundle::getAppList(const QStringList& ids) {
    QList<BundleItem*> list;
    appMutex.lock();
    foreach(const QString& id, ids) {
        foreach(BundleItem *a, appList) {
            if(a->id == id) {
                list.append(a);
                break;
            }
        }
    }
    appMutex.unlock();
    return list;
}

QStringList Bundle::getAppIds() {
    QStringList ret;
    foreach(BundleItem *a, appList) {
        ret.append(a->id);
    }
    return ret;
}

bool Bundle::addApp(BundleItem* app) {
    bool exist = false;
    appMutex.lock();
    exist = getAppIds().contains(app->id);
    if(!exist) {
        appList.append(app);
        size += app->size;
    }
    appMutex.unlock();
    return !exist;
}

int Bundle::addApps(const QList<BundleItem*>& apps) {
    int count = 0;
    appMutex.lock();
    foreach(BundleItem *a, apps) {
        if(!getAppIds().contains(a->id)) {
            appList.append(a);
            size += a->size;
            count ++;
        }
    }
    appMutex.unlock();
    return count;
}

bool Bundle::removeApp(BundleItem* app) {
    bool r = false;
    appMutex.lock();
    if (appList.removeOne(app)) {
        size -= app->size;
        r = true;
    }
    appMutex.unlock();
    return r;
}

int Bundle::removeApps(const QList<BundleItem*>& apps) {
    int count = 0;
    appMutex.lock();
    foreach(BundleItem *a, apps) {
        if(appList.removeOne(a)) {
            size -= a->size;
            count ++;
        }
    }
    appMutex.unlock();
    return count;
}

bool Bundle::removeApp(const QString& id) {
    bool r = false;
    appMutex.lock();
    for(int i=0; i<appList.size(); i++) {
        if(appList[i]->id == id) {
            size -= appList[i]->size;
            appList.removeAt(i);
            r = true;
            break;
        }
    }
    appMutex.unlock();
    return r;
}

int Bundle::removeApps(const QStringList& ids) {
    int count = 0;
    appMutex.lock();
    foreach(const QString& id, ids) {
        for(int i=0; i<appList.size(); i++) {
            if(appList[i]->id == id) {
                size -= appList[i]->size;
                appList.removeAt(i);
                count ++;
                break;
            }
        }
    }
    appMutex.unlock();
    return count;
}

QList<Bundle*> Bundle::getBundleList() {
    QList<Bundle*> list;
    bundleMutex.lock();
    list.append(bundleList);
    bundleMutex.unlock();
    return list;
}

QList<Bundle*> Bundle::getBundleList(const QStringList& ids) {
    QList<Bundle*> list;
    bundleMutex.lock();
    foreach(const QString& id, ids) {
        foreach(Bundle *b, bundleList) {
            if(b->id == id) {
                list.append(b);
                break;
            }
        }
    }
    bundleMutex.unlock();
    return list;
}

QStringList Bundle::getBundleIds() {
    QStringList ret;
    bundleMutex.lock();
    foreach(Bundle *b, bundleList) {
        ret.append(b->id);
    }
    bundleMutex.unlock();
    return ret;
}

bool Bundle::addBundle(Bundle* bundle) {
    bool exist = false;
    bundleMutex.lock();
    exist = bundleList.contains(bundle);
    if(!exist) {
        bundleList.append(bundle);
    }
    bundleMutex.unlock();
    return !exist;
}

int Bundle::addBundles(const QList<Bundle*>& bundles) {
    int count = 0;
    bundleMutex.lock();
    foreach(Bundle *b, bundles) {
        if(!bundleList.contains(b)) {
            bundleList.append(b);
            count ++;
        }
    }
    bundleMutex.unlock();
    return count;
}

bool Bundle::removeBundle(Bundle* bundle) {
    bool r = false;
    bundleMutex.lock();
    r = bundleList.removeOne(bundle);
    bundleMutex.unlock();
    return r;
}

int Bundle::removeBundles(const QList<Bundle*>& bundles) {
    int count = 0;
    bundleMutex.lock();
    foreach(Bundle *b, bundles) {
        if(bundleList.removeOne(b)) {
            count ++;
        }
    }
    bundleMutex.unlock();
    return count;
}

bool Bundle::removeBundle(const QString& id) {
    bool r = false;
    bundleMutex.lock();
    for(int i=0; i<bundleList.size(); i++) {
        if(bundleList[i]->id == id) {
            bundleList.removeAt(i);
            r = true;
            break;
        }
    }
    bundleMutex.unlock();
    return r;
}

int Bundle::removeBundles(const QStringList& ids) {
    int count = 0;
    bundleMutex.lock();
    foreach(const QString& id, ids) {
        for(int i=0; i<bundleList.size(); i++) {
            if(bundleList[i]->id == id) {
                bundleList.removeAt(i);
                count ++;
                break;
            }
        }
    }
    bundleMutex.unlock();
    return count;
}

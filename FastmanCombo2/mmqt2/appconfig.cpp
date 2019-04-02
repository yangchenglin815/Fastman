#include "appconfig.h"

#include "bundle.h"
#include "zbytearray.h"
#include <QBuffer>
#include <QSqlDatabase>
#include <QUrl>
#include <QDomDocument>
#include <QDir>
#include <QSettings>
#include <QDateTime>
#include <stdlib.h>
#include <QDebug>
#include <QMutexLocker>

#include "globalcontroller.h"
#include "fastman2.h"
#include "loginmanager.h"
#include "zdownloadmanager.h"
#include "zdmhttp.h"
#include "zdmprivate.h"
#include "zbytearray.h"
#include "zlog.h"
#include "qapplication.h"
#include "QJson/parser.h"
#include "zstringutil.h"
#include "ThreadBase.h"

DoNotCleanDb::DoNotCleanDb()
    : ZDatabaseUtil<DoNotClean*>("DoNotClean") {

}

QByteArray DoNotCleanDb::toByteArray(DoNotClean *obj) {
    ZByteArray d(false);
    d.putUtf8(obj->pkgList.join(","));
    return d;
}

DoNotClean *DoNotCleanDb::fromByteArray(const QByteArray &data) {
    ZByteArray d(false);
    DoNotClean *obj = new DoNotClean();
    int i = 0;

    d.append(data);
    obj->pkgList = d.getNextUtf8(i).split(',', QString::SkipEmptyParts);
    return obj;
}

QString DoNotCleanDb::getId(DoNotClean *obj) {
    return obj->fingerprint;
}

void DoNotCleanDb::setId(DoNotClean *obj, const QString &id) {
    obj->fingerprint = id;
}

BundleDb::BundleDb(Bundle *globalBundle)
    : ZDatabaseUtil<Bundle*>("Bundle") {
    this->globalBundle = globalBundle;
}

QByteArray BundleDb::toByteArray(Bundle *obj) {
    ZByteArray d(false);
    d.putUtf8(obj->name);
    d.putByte(obj->enable);
    d.putByte(obj->readonly);
    d.putUtf8(obj->getAppIds().join(","));
    d.putInt(obj->type);
    d.putInt(obj->leastMem);
    d.putInt(obj->leastStore);
    d.putUtf8(obj->ispublic);
    d.putInt(obj->ostype);
    return d;
}

Bundle *BundleDb::fromByteArray(const QByteArray &data) {
    ZByteArray d(false);
    Bundle *obj = new Bundle();
    int i = 0;

    d.append(data);
    obj->name = d.getNextUtf8(i);
    obj->enable = d.getNextByte(i);
    obj->readonly = d.getNextByte(i);
    obj->itemIDsForOfflineMode = d.getNextUtf8(i).split(',', QString::SkipEmptyParts);
    obj->type = d.getNextInt(i);
    obj->leastMem = d.getNextInt(i);
    obj->leastStore = d.getNextInt(i);
    obj->ispublic = d.getNextUtf8(i);
    obj->ostype = d.getNextInt(i);

    return obj;
}

QString BundleDb::getId(Bundle *obj) {
    return obj->id;
}

void BundleDb::setId(Bundle *obj, const QString &id) {
    obj->id = id;
}

BundleItemDb::BundleItemDb(Bundle *globalBundle)
    : ZDatabaseUtil<BundleItem*>("BundleItem") {
    this->globalBundle = globalBundle;
}

QByteArray BundleItemDb::toByteArray(BundleItem *obj) {
    ZByteArray d(false);
    d.putUtf8(obj->tcid);
    d.putUtf8(obj->name);
    d.putInt64(obj->size);
    d.putUtf8(obj->pkg);
    d.putUtf8(obj->version);
    d.putInt(obj->versionCode);
    d.putInt(obj->instLocation);
    d.putInt(obj->minSdk);
    d.putInt(obj->isIme);
    d.putUtf8(obj->path);
    d.putUtf8(obj->down_url);
    d.putInt(obj->download_status);
    d.putInt(obj->mtime);
    d.putUtf8(obj->md5);
    d.putInt(obj->iconData.size());
    d.append(obj->iconData);
    d.putInt(obj->isHiddenItem);
    d.putUtf8(obj->platform);
    return d;
}

BundleItem *BundleItemDb::fromByteArray(const QByteArray &data) {
    ZByteArray d(false);
    BundleItem *obj = new BundleItem();
    int i = 0;

    d.append(data);
    obj->tcid = d.getNextUtf8(i);
    obj->name = d.getNextUtf8(i);
    obj->size = d.getNextInt64(i);
    obj->pkg = d.getNextUtf8(i);
    obj->version = d.getNextUtf8(i);
    obj->versionCode = d.getNextInt(i);
    obj->instLocation = d.getNextInt(i);
    obj->minSdk = d.getNextInt(i);
    obj->isIme = d.getNextInt(i);
    obj->path = d.getNextUtf8(i);
    obj->down_url = d.getNextUtf8(i);
    obj->download_status = d.getNextInt(i);
    obj->mtime = d.getNextInt(i);
    obj->md5 = d.getNextUtf8(i);

    int iconLen = d.getNextInt(i);
    if(iconLen > 0) {
        obj->iconData = d.mid(i, iconLen);
#ifndef NO_GUI
        obj->icon.loadFromData((const uchar*)obj->iconData.data(), iconLen);
#endif
    }
    obj->isHiddenItem = d.getNextInt(i);
    obj->platform = d.getNextUtf8(i);
    return obj;
}

QString BundleItemDb::getId(BundleItem *obj) {
    return obj->id;
}

void BundleItemDb::setId(BundleItem *obj, const QString &id) {
    obj->id = id;
}

InstLogDb::InstLogDb(const QString &tablename)
    : ZDatabaseUtil<InstLog*>(tablename) {

}

QByteArray InstLogDb::toByteArray(InstLog *obj) {
    ZByteArray d(false);
    d.putInt(obj->time);
    d.putUtf8(obj->json);
    d.putUtf8(obj->url);
    d.putUtf8(obj->result);

    d.putShort(obj->isInstalled);
    d.putShort(obj->canReplace);
    d.putUtf8(obj->installHint);

    d.putUtf8(obj->adbSerial);

    return d;
}

InstLog *InstLogDb::fromByteArray(const QByteArray &data) {
    ZByteArray d(false);
    InstLog *obj = new InstLog();
    int i = 0;

    d.append(data);
    obj->time = d.getNextInt(i);
    obj->json = d.getNextUtf8(i);
    obj->url = d.getNextUtf8(i);
    obj->result = d.getNextUtf8(i);

    obj->isInstalled = d.getNextShort(i);
    obj->canReplace = d.getNextShort(i);
    obj->installHint = d.getNextUtf8(i);

    obj->adbSerial = d.getNextUtf8(i);

    return obj;
}

QString InstLogDb::getId(InstLog *obj) {
    return obj->id;
}

void InstLogDb::setId(InstLog *obj, const QString &id) {
    obj->id = id;
}

AppConfig::AppConfig(QObject *parent)
    : QObject(parent) {
    controller = GlobalController::getInstance();
    bundleDb = NULL;
    bundleItemDb = NULL;
    doNotCleanDb = NULL;
    instCacheDb = NULL;
    instLogDb = NULL;
    CMCCFirst = false;
    spInstall = false;
    printfLog = false;
    offlineMode = false;
    YAMUser = false;
    CustomDesktop = false;
    loggedIn = false;
    globalBundle = new Bundle();
    bAsyncInst = true;
    bNeedZAgentInstallAndUninstall = false;
    bTryRoot = false;
    bUninstUsrApp = true;
    bUninstSysApp = false;
    bPauseInstall = false;
    MZDesktop = false;
    bInstallGamehouse = false;
    bNeedYunOSDrag = false;
    bNeedFastinstall = false;
    bAllowYunOSReplaceIcon = false;
    desktopAdaptiveList = QStringList();
    selfRunAppsStr = "";
    themesMap = QMap<QString, QVariant>();
    themesCodeStr = QString();
    appActivateMap = QMap<QString, QVariant>();
    installFailureMaxRate = 100;
    reddotFilePath = "";
    alertType = ZMSG2_ALERT_SOUND|ZMSG2_ALERT_VIBRATE;
    rootFlag = FastManRooter::FLAG_FRAMA_ROOT | FastManRooter::FLAG_MASTERKEY_ROOT
            | FastManRooter::FLAG_VROOT | FastManRooter::FLAG_TOWEL_ROOT;
}

AppConfig::~AppConfig() {
    QList<BundleItem *> allItems = globalBundle->getAppList();
    while(!allItems.isEmpty()) {
        delete allItems.takeFirst();
    }
    delete globalBundle;

    if(bundleDb != NULL) {
        delete bundleDb;
    }
    if(bundleItemDb != NULL) {
        delete bundleItemDb;
    }
    if(doNotCleanDb != NULL) {
        delete doNotCleanDb;
    }
    if(instCacheDb != NULL) {
        delete instCacheDb;
    }
    if(instLogDb != NULL) {
        delete instLogDb;
    }
}

void AppConfig::loadCfg() {
    QSettings cfg("mmqt2.ini", QSettings::IniFormat);
    username = cfg.value("Account/User").toString();
    opername = cfg.value("Account/Oper").toString();
    encpasswd = ZByteArray::decode(cfg.value("Account/Pass").toString());
    CMCCFirst = cfg.value("Account/CMCCFirst").toBool();
    offlineMode = cfg.value("Account/OfflineMode").toBool();
    YAMUser = cfg.value("Account/YAMUser").toBool();
    CustomDesktop = cfg.value("Account/CustomDesktop").toBool();
    autoInstBundles = cfg.value("Bundle/AutoInst").toString().split(",", QString::SkipEmptyParts);
    bAsyncInst = cfg.value("CFG/AsyncInst", bAsyncInst).toBool();
    bTryRoot = cfg.value("CFG/TryRoot", bTryRoot).toBool();
    bUninstUsrApp = cfg.value("CFG/UninstUsrApp").toBool();
    bUninstSysApp = cfg.value("CFG/UninstSysApp").toBool();
    printfLog = cfg.value("CFG/PrintfLog").toBool();
    alertType = cfg.value("CFG/AlertType", alertType).toInt();
    rootFlag = cfg.value("CFG/RootFlag", rootFlag).toInt();
    DelXML = cfg.value("CFG/DelXML",DelXML).toBool();
    MZDesktop = cfg.value("Install/MZDesktop", MZDesktop).toBool();
    bInstallGamehouse = cfg.value("Install/InstallGamehouse", bInstallGamehouse).toBool();
    bNeedYunOSDrag = cfg.value("Install/NeedYunOSDrag", bNeedYunOSDrag).toBool();
    bNeedFastinstall = cfg.value("Install/NeedFastinstall", bNeedFastinstall).toBool();
    bAllowYunOSReplaceIcon = cfg.value("Install/AllowYunOSReplaceIcon", bAllowYunOSReplaceIcon).toBool();
    desktopAdaptiveList = cfg.value("Install/DesktopAdaptiveList", desktopAdaptiveList).toString().split(",", QString::SkipEmptyParts);
    selfRunAppsStr = cfg.value("Install/SelfRunAppsStr", selfRunAppsStr).toString();
    themesMap = cfg.value("Install/ThemesMap").toMap();
    themesCodeStr = cfg.value("Install/ThemesCodeStr").toString();
    appActivateMap = cfg.value("Install/AppActivateMap").toMap();
    installFailureMaxRate = cfg.value("Install/InstallFailureMaxRate").toInt();
    reddotFilePath = cfg.value("Install/ReddotFilePath").toString();
    useLAN = cfg.value("LAN/UseLAN").toBool();
    LANUrl = cfg.value("LAN/Url").toString();
    LANPort = cfg.value("LAN/Port").toString();
#ifndef NO_GUI
    cfg.beginGroup("hint");
    cfg.remove("");
    cfg.endGroup();
    cfg.sync();
#endif
}

void AppConfig::saveCfg() {
    QSettings cfg("mmqt2.ini", QSettings::IniFormat);
    cfg.setValue("Account/User", username);
    cfg.setValue("Account/Oper", opername);
    cfg.setValue("Account/Pass", ZByteArray::encode(encpasswd));
    cfg.setValue("Account/CMCCFirst", CMCCFirst);
    cfg.setValue("Account/OfflineMode", offlineMode);
    cfg.setValue("Account/YAMUser", YAMUser);
    cfg.setValue("Account/CustomDesktop", CustomDesktop);
    cfg.setValue("Bundle/AutoInst", autoInstBundles.join(","));
    cfg.setValue("CFG/AsyncInst", bAsyncInst);
    cfg.setValue("CFG/TryRoot", bTryRoot);
    cfg.setValue("CFG/UninstUsrApp", bUninstUsrApp);
    cfg.setValue("CFG/UninstSysApp", bUninstSysApp);
    cfg.setValue("CFG/PrintfLog", printfLog);
    cfg.setValue("CFG/AlertType", alertType);
    cfg.setValue("CFG/RootFlag", rootFlag);
    cfg.setValue("CFG/DelXML",DelXML);
    cfg.setValue("Install/MZDesktop", MZDesktop);
    cfg.setValue("Install/InstallGamehouse", bInstallGamehouse);
    cfg.setValue("Install/NeedYunOSDrag", bNeedYunOSDrag);
    cfg.setValue("Install/NeedFastinstall", bNeedFastinstall);
    cfg.setValue("Install/AllowYunOSReplaceIcon", bAllowYunOSReplaceIcon);
    cfg.setValue("Install/DesktopAdaptiveList", desktopAdaptiveList.join(","));
    cfg.setValue("Install/SelfRunAppsStr", selfRunAppsStr);
    cfg.setValue("Install/ThemesMap", themesMap);
    cfg.setValue("Install/ThemesCodeStr", themesCodeStr);
    cfg.setValue("Install/AppActivateMap", appActivateMap);
    cfg.setValue("Install/InstallFailureMaxRate", installFailureMaxRate);
    cfg.setValue("Install/ReddotFilePath", reddotFilePath);
    cfg.setValue("LAN/UseLAN", useLAN);
    cfg.setValue("LAN/Url", LANUrl);
    cfg.setValue("LAN/Port", LANPort);
    cfg.sync();
}

QStringList AppConfig::getAutoInstItemIds() {
    QStringList ret;
    autoInstBundlesMutex.lock();
    if(!autoInstBundles.isEmpty()) {
        QList<Bundle *> bundles = globalBundle->getBundleList(autoInstBundles);
        foreach(Bundle *b, bundles) {
            ret.append(b->getAppIds());
            bundleType = b->ostype;
        }
    }
    autoInstBundlesMutex.unlock();
    return ret;
}

void AppConfig::setAutoInstBundles(const QStringList &list) {
    DBG("setAutoInstBundles <%s>\n", list.join(",").toLocal8Bit().data());
    autoInstBundlesMutex.lock();
    autoInstBundles = list;
    autoInstBundlesMutex.unlock();
}

void AppConfig::addAutoInstBundles(const QStringList &list)
{
    DBG("addAutoInstBundles <%s>\n", list.join(",").toLocal8Bit().data());
    autoInstBundlesMutex.lock();
    foreach (const QString &bundle, list) {
        if (!autoInstBundles.contains(bundle)) {
            autoInstBundles.append(bundle);
        }
    }
    autoInstBundlesMutex.unlock();
}

void AppConfig::removeAutoInstBundle(const QStringList &list)
{
    DBG("removeAutoInstBundles <%s>\n", list.join(",").toLocal8Bit().data());
    autoInstBundlesMutex.lock();
    foreach (const QString &bundle, list) {
        autoInstBundles.removeOne(bundle);
    }
    autoInstBundlesMutex.unlock();
}

void AppConfig::clearAutoInstBundles() {
    DBG("clearAutoInstBundles\n");
    autoInstBundlesMutex.lock();
    autoInstBundles.clear();
    autoInstBundlesMutex.unlock();
}

bool AppConfig::hasAutoInstBundles() {
    return !autoInstBundles.isEmpty();
}

bool AppConfig::isAutoInstBundle(const QString &id) {
    bool ret = false;
    autoInstBundlesMutex.lock();
    ret = autoInstBundles.contains(id);
    autoInstBundlesMutex.unlock();
    return ret;
}

bool AppConfig::initDb() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("mmqt2.db");
    if(!db.open()) {
        DBG("error: %s\n", db.lastError().text().toLocal8Bit().data());
        return false;
    }

    bundleDb = new BundleDb(globalBundle);
    bundleItemDb = new BundleItemDb(globalBundle);
    doNotCleanDb = new DoNotCleanDb();
    instCacheDb = new InstLogDb("instCache");
    instLogDb = new InstLogDb("instLog");
    return true;
}

bool AppConfig::core_loadAllData() {
    DBG("load all bundel and bundleitem datas\n");
    QDir dir;
    // NOTE: note use qApp->applicationDirPath() for Dual version
#ifdef DOWNLOAD_DIR_USE_USERNAME
    downloadDirPath = "DOWNLOAD_APPS/" + username;
#else
    downloadDirPath = "DOWNLOAD_APPS";
#endif
    dir.mkpath(downloadDirPath);

    // bundles
    QList<Bundle *> bundlesInDB = bundleDb->getAllData();
    QList<Bundle *> bundlesInWeb;
    QList<Bundle *> bundlesInGlobal = globalBundle->getBundleList();
    if (!offlineMode) {
        loadWebBundles(bundlesInWeb);
        if (!bundlesInWeb.isEmpty()) {
            // 只保存当前用户的套餐及自定义的套餐，方便离线安装使用
            QStringList delIDs;
            for (int i = 0; i < bundlesInDB.size(); ++i) {
                Bundle *bundle = bundlesInDB[i];
                if (bundle->readonly) {
                    DBG("del bundle in db:%s\n", qPrintable(bundle->name));
                    delIDs.append(bundle->id);
                    bundlesInDB.removeAt(i--);
                    delete bundle;
                }
            }
            bundleDb->removeAllData(delIDs);

            foreach (Bundle *bundle, bundlesInWeb) {
                bundleDb->addData(bundle);
            }
        } else {
            return false;
        }
    }

    // NOTE: clear first?
    foreach (Bundle *bundle, bundlesInGlobal) {
        DBG("clear global bundles first:%s\n", qPrintable(bundle->name));
        globalBundle->removeBundle(bundle);
    }

    if(!offlineMode){
        globalBundle->addBundles(bundlesInWeb);
    }else{
        globalBundle->addBundles(bundlesInDB);
    }
    bundlesInGlobal = globalBundle->getBundleList();

    // clear autoInstallBundle
    foreach (const QString &ID, autoInstBundles) {
        if (!globalBundle->getBundle(ID)) {
            autoInstBundles.removeOne(ID);
        }
    }

    DBG("bundels in Web, size=%d\n", bundlesInWeb.size());
    foreach (Bundle *bundle, bundlesInWeb) {
        DBG("bundels in Web:%s\n", qPrintable(bundle->name));
    }
    DBG("bundels in DB, size=%d\n", bundlesInDB.size());
    foreach (Bundle *bundle, bundlesInDB) {
        DBG("bundels in DB:%s\n", qPrintable(bundle->name));
    }
    DBG("bundels in global, size=%d\n", bundlesInGlobal.size());
    foreach (Bundle *bundle, bundlesInGlobal) {
        DBG("bundels in global:%s\n", qPrintable(bundle->name));
    }

    if (globalBundle->getBundleList().size() == 0) {
        DBG("No bundle found\n");
        return false;
    }

    //clear bundleitem
    if(!offlineMode){
        bundleItemDb->clearData();
    }

    // bundleItems
    QList<BundleItem *> itemsInWeb;
    foreach (Bundle *bundle, bundlesInWeb) {
        itemsInWeb.append(bundle->getAppList());
    }
    QList<BundleItem *> itemsInDB = bundleItemDb->getAllData();
    QList<BundleItem *> itemsInGlobal;

    if (!offlineMode) {
        foreach (Bundle *bundle, bundlesInGlobal) {
            globalBundle->addApps(bundle->getAppList());
        }
    } else {
        // NOTE: use item id when offline mode
        foreach (Bundle *bundle, bundlesInGlobal) {
            for(int i=0; i<bundle->itemIDsForOfflineMode.size();i++){
                QString id = bundle->itemIDsForOfflineMode.at(i);
                QString gid = bundle->id;
                foreach (BundleItem *item, itemsInDB) {
                    if(item->id == id && item->tcid == gid){
                        bundle->addApp(item);
                        globalBundle->addApp(item);
                    }
                }
            }
        }
    }
    itemsInGlobal = globalBundle->getAppList();

    if (!offlineMode) {
#ifdef DOWNLOAD_DIR_USE_USERNAME
        foreach (BundleItem *item, itemsInWeb) {
            bundleItemDb->setData(item);
        }
#else
        // clean db items
        QList<BundleItem *> delItems;
        foreach (BundleItem *itemInDB, itemsInDB) {
            bool found = false;
            foreach (BundleItem *itemInGloabl, itemsInGlobal) {
                if (itemInDB->id == itemInGloabl->id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                delItems.append(itemInDB);
            }
        }

        foreach (BundleItem *item, delItems) {
            bundleItemDb->removeData(item);
        }
        QList<BundleItem *> items = globalBundle->getAppList();
        bundleItemDb->setAllData(items);
#endif
    }

    DBG("bundelitem in Web, size=%d\n", itemsInWeb.size());
    foreach (BundleItem *item, itemsInWeb) {
        DBG("bundelitem in DB:%s\n", qPrintable(item->name));
    }
    DBG("bundelitem in DB, size=%d\n", itemsInDB.size());
    foreach (BundleItem *item, itemsInDB) {
        DBG("bundelitem in DB:%s\n", qPrintable(item->name));
    }
    DBG("bundelitem in global, size=%d\n", itemsInGlobal.size());
    foreach (BundleItem *item, itemsInGlobal) {
        DBG("bundelitem in global:%s\n", qPrintable(item->name));
    }

    // others
    QList<DoNotClean*> wl = doNotCleanDb->getAllData();
    while (!wl.isEmpty()) {
        DoNotClean *w = wl.takeFirst();
        doNotCleanMap.insert(w->fingerprint, w->pkgList);
        delete w;
    }

    instLogDb->deleteOldData(5000);

    // first bundle to auto install
    //    setAutoInstBundles(QStringList() << globalBundle->getBundleList().at(0)->id);

    return true;
}

bool AppConfig::checkAndCleanDownload() {
    DBG("check and clean download\n");

    QList<BundleItem *> itemsInGlobal = globalBundle->getAppList();
    QList<BundleItem *> itemsInDB = bundleItemDb->getAllData();

    // check finished
    bool allFinished = true;
    foreach (BundleItem *item, itemsInGlobal) {
        do {
            // find in username dir
#if 0
            // use MD5
            QString MD5;
            quint64 size;
            if (GlobalController::getFileMd5(item->path, MD5, size)  // 耗时
                    && item->md5.toUpper() == MD5.toUpper())
#else
            // use size
            QFile file(item->path);
            if (file.size() == item->size)
#endif
            {
                item->mtime = QFileInfo(item->path).lastModified().toTime_t();
                item->download_status = ZDownloadTask::STAT_FINISHED;
                QFile::remove(item->path + ".dm!");
                break;
            }

            // find in other username dir
            bool foundInOtherUsername = false;
            BundleItem *foundItem = NULL;
            foreach (BundleItem *itemInDB, itemsInDB) {
                if (itemInDB->id.contains(username)) {
                    continue;
                }

                QString id = item->id;
                id.remove(username);
                if (itemInDB->id.contains(id)) {
                    foundItem = itemInDB;
                    foundInOtherUsername = true;
                    break;
                }
            }
            if (foundInOtherUsername) {
                DBG("found in other username, path=%s\n", qPrintable(foundItem->path));
#if 0
                // use MD5
                QString MD5;
                quint64 size;
                if (GlobalController::getFileMd5(foundItem->path, MD5, size)  // 耗时
                        && item->md5.toUpper() == MD5.toUpper())
#else
                // use size
                QFile file(foundItem->path);
                if (file.size() == item->size)
#endif
                {
                    bool ok = QFile::copy(foundItem->path, item->path);
                    if (ok) {
                        item->mtime = QFileInfo(item->path).lastModified().toTime_t();
                        item->download_status = ZDownloadTask::STAT_FINISHED;
                        QFile::remove(item->path + ".dm!");
                        break;
                    } else {
                        DBG("copy failed!\n");
                    }
                } else {
                    DBG("size or MD5 not matched!\n");
                }
            }

            allFinished = false;
        } while (0);
    }

    if (offlineMode && !allFinished) {
        DBG("all items must be exsits in local if offline install\n");
        return false;
    }

    // clear local apps not in globalBundle
    QDir dir(downloadDirPath);
    QStringList localApps = dir.entryList(QStringList() << "*.apk" << "*.dm!", QDir::Files);
    QStringList globalApps;
    foreach (BundleItem *item, itemsInGlobal) {
        QFileInfo info(item->path);
        globalApps.append(info.fileName());
    }
    foreach (const QString app, localApps) {
        QString filename = app;
        if (app.left(4) == ".dm!") {
            filename.remove(".dm!");
        }
        if (!globalApps.contains(filename, Qt::CaseInsensitive)) {
            QString path = downloadDirPath + "/" + app;
            DBG("remove local apk/dm:%s\n", qPrintable(path));
            QFile::remove(path);
        }
    }

    // clear mmqt2_dm.xml
    cleanDownlaodConfig();

    return true;
}

void AppConfig::cleanDownlaodConfig()
{
    DBG("clear mmqt2_dm.xml\n");

    QString dmCfg = qApp->applicationDirPath() + "/mmqt2_dm.xml";
    QFile file(dmCfg);
    if (!file.open(QIODevice::ReadOnly)) {
        DBG("%s open failed, %s\n", qPrintable(dmCfg), qPrintable(file.errorString()));
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        DBG("doc set content failed\n");
        file.close();
        file.remove();
        return;
    }
    file.close();

    QList<BundleItem *> items = globalBundle->getAppList();

    QStringList itemUrls;
    foreach (BundleItem *item, items) {
        itemUrls.append(item->down_url);
    }

    QDomNodeList nodes = doc.elementsByTagName("task");
    for (int i = nodes.size(); i >= 0; --i) {
        QDomElement element = nodes.at(i).toElement();
        QString url = element.attribute("url");
        QString path = element.attribute("path");

#ifdef DOWNLOAD_DIR_USE_USERNAME
        // 只清理当前帐户的数据
        if (!path.contains(username, Qt::CaseInsensitive)) {
            continue;
        }

        bool needClean = false;
        QList<BundleItem *> items = globalBundle->getAppList();
        foreach (BundleItem *item, items) {
            if (item->down_url.compare(url, Qt::CaseInsensitive) == 0) {
                needClean = true;
                break;
            }
        }
        if (!needClean) {
            continue;
        }
#endif

        bool needRemove = false;
        do {
            if (!itemUrls.contains(url, Qt::CaseInsensitive)) {
                needRemove = true;
                break;
            }

            foreach (BundleItem *item, items) {
                if (item->down_url == url) {
                    if (ZDownloadTask::STAT_FINISHED == item->download_status) {
                        needRemove = true;
                    }
                    break;
                }
            }
        } while (0);

        if (needRemove) {
            DBG("remove child element:%s\n", qPrintable(url));
            QDomElement root = doc.documentElement();
            root.removeChild(element);

            QFile file(dmCfg);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&file);
                doc.save(out, 4);
                file.close();
            } else {
                DBG("%s open failed, %s\n", qPrintable(dmCfg), qPrintable(file.errorString()));
            }
        }  // needRemove
    }  // for
}

void AppConfig::loadDonwloadConfig()
{
    DBG("load mmqt2_dm.xml\n");

    QString dmCfg = qApp->applicationDirPath() + "/mmqt2_dm.xml";
    QFile file(dmCfg);
    if (!file.open(QIODevice::ReadOnly)) {
        DBG("%s open failed, %s\n", qPrintable(dmCfg), qPrintable(file.errorString()));
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        DBG("doc set content failed\n");
        file.close();
        file.remove();
        return;
    }
    file.close();

    QDomNodeList nodes = doc.elementsByTagName("task");
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement element = nodes.at(i).toElement();

        ZDMTask *task = new ZDMTask((ZDM*)controller->downloadmanager);
        task->id = element.attribute("id");
        task->url = QUrl::fromEncoded(element.attribute("url").toLocal8Bit());
        task->path = element.attribute("path");
        task->status = element.attribute("status").toInt();
        task->mtime = element.attribute("mtime").toUInt();
        task->progress = element.attribute("progress").toLongLong();
        task->size = element.attribute("size").toLongLong();

        if (task->status != ZDMTask::STAT_STOPPED) {
            task->status = ZDMTask::STAT_PENDING;
        }

        QDomNodeList subs = element.elementsByTagName("part");
        for (int j = 0; j < subs.count(); ++j) {
            QDomElement element = subs.at(j).toElement();
            ZDMPart *part = new ZDMPart();
            part->begin = element.attribute("begin").toLongLong();
            part->end = element.attribute("end").toLongLong();
            part->progress = element.attribute("progress").toLongLong();
            task->parts.append(part);
        }

#ifdef DOWNLOAD_DIR_USE_USERNAME
        // 只下载当前帐户的数据
        if (!task->path.contains(username, Qt::CaseInsensitive)) {
            continue;
        }

        QList<BundleItem *> items = globalBundle->getAppList();
        foreach (BundleItem *item, items) {
            if (item->down_url.compare(task->url.toString(), Qt::CaseInsensitive) == 0) {
                DBG("add task:%s\n", qPrintable(task->url.toString()));
                controller->downloadmanager->addTask(task->id, task->url.toString(), task->path);
                break;
            }
        }
#else
        controller->downloadmanager->addTask(task->id, task->url.toString(), task->path);
#endif
    }
}

void AppConfig::startDownloadItems()
{
    QList<BundleItem *> items = globalBundle->getAppList();
    foreach (BundleItem *item, items) {
        QStringList list = item->id.split("_");
        item->id = list.at(0);
        if (item->download_status != ZDownloadTask::STAT_FINISHED) {
            if(CMCCFirst){
                item->down_url.replace("zjadmin", "ydzjadmin");
            }
            controller->loginManager->revokeUrlString(item->down_url);
            ZDownloadTask *task = controller->downloadmanager->getTask(item->id);
            if (task == NULL) {
                DBG("add task:%s\n", qPrintable(item->down_url));
                controller->downloadmanager->addTask(item->id, item->down_url, item->path);
            } else {
                task->url = item->down_url;
            }
        }
    }  // foreach
}

bool AppConfig::loadWebBundles(QList<Bundle *> &bundles) {
    QUrl url(sUrl + "/list_bundle.aspx");
    controller->loginManager->revokeUrl(url);
    controller->loginManager->fillUrlParams(url);

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail!\n");
        free(uri);
        return false;
    }
    DBG("load web bundles url=<%s>\n", uri);
    free(uri);

    QDomDocument dom;
    if (!dom.setContent(out)) {
        DBG("invalid xml!\n");
        return false;
    }

    QDomNodeList nodes = dom.elementsByTagName("app_list");
    for (int i = 0; i < nodes.count(); ++i) {
        QString gid = nodes.at(i).firstChildElement("gid").text();
        QString ispublic = nodes.at(i).firstChildElement("ispublic").text();
        if (!gid.isEmpty()) {
            bundles.append(loadWebBundles(gid, ispublic));
        }
    }

    return true;
}

QList<BundleItem *> AppConfig::loadWebHiddenBundleItems(const QString &gid, const QString &ispublic) {
    QList<BundleItem *> appList;
    QUrl url(sUrl + "/gethiddenlisttest.aspx");
    controller->loginManager->revokeUrl(url);
    controller->loginManager->fillUrlParams(url, QString("&tcid=%1&ispublic=%2").arg(gid, ispublic));

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail!\n");
        free(uri);
        return appList;
    }
    DBG("load web hidden bundle url=<%s>\n", uri);
    free(uri);

    QDomDocument dom;
    if (!dom.setContent(out)) {
        DBG("invalid xml!\n");
        return appList;
    }

    // get bundle items
    QDomNodeList appNodes = dom.elementsByTagName("app");
    for (int j = 0; j < appNodes.count(); ++j) {
        QDomNode appNode = appNodes.at(j);

        // check bundle item
        QString itemId = appNode.firstChildElement("appid").text();
        BundleItem *item = globalBundle->getApp(itemId);
        if (item == NULL) {
            item = new BundleItem();
#ifdef DOWNLOAD_DIR_USE_USERNAME
            item->id = itemId + "_" + gid + username;
#else
            item->id = itemId + "_" + gid;
#endif
        }
        appList.append(item);
        item->tcid = gid;
        item->isHiddenItem = true;  // NOTE:
        item->name = appNode.firstChildElement("appname").text();
        item->size = appNode.firstChildElement("filesize").text().toUInt();
        item->pkg = appNode.firstChildElement("packageName").text();
        item->version = appNode.firstChildElement("versionName").text();
        item->versionCode = appNode.firstChildElement("versionCode").text().toInt();
        item->path = downloadDirPath + "/" + appNode.firstChildElement("filename").text();
        item->down_url = appNode.firstChildElement("down_url").text();
        item->md5 = appNode.firstChildElement("md5").text();
        QDomElement e = appNode.firstChildElement("installsite");
        if (e.isNull()) {
            item->instLocation = -1;
        } else {
            item->instLocation = e.text().toInt();
        }
        item->platform = appNode.firstChildElement("platform").text();
    }

    return appList;
}

QList<Bundle *> AppConfig::loadWebBundles(const QString &gid, QString ispublic) {
    QList<Bundle *> ret;
    QUrl url(sUrl + "/list_bundle.aspx");
    controller->loginManager->revokeUrl(url);
    controller->loginManager->fillUrlParams(url, QString("&gid=%1&ispublic=%2").arg(gid, ispublic));

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail!\n");
        free(uri);
        return ret;
    }
    DBG("load web bundle url=<%s>\n", uri);
    free(uri);

    QDomDocument dom;
    if (!dom.setContent(out)) {
        DBG("invalid xml!\n");
        return ret;
    }

    QDomNodeList nodes = dom.elementsByTagName("app_list");  // NOTE: only one node generally
    for (int i = 0; i < nodes.count(); ++i) {
        QDomNode node = nodes.at(i);

        // get bundle properties
        Bundle *bundle = new Bundle();
        bundle->readonly = true;
        bundle->id = node.firstChildElement("gid").text();
        bundle->name = node.firstChildElement("applistname").text();
        bundle->type = node.firstChildElement("type").text().toInt();
        bundle->leastMem = node.firstChildElement("spaceLimit").text().toInt();
        bundle->leastStore = node.firstChildElement("storageLimit").text().toInt();
        bundle->ispublic = node.firstChildElement("ispublic").text();
        bundle->ostype = node.firstChildElement("TCOSType").text().toInt();

        // get bundle items
        QList<BundleItem *> appList;
        QDomNodeList appNodes = node.toElement().elementsByTagName("app");
        for (int j = 0; j < appNodes.count(); ++j) {
            QDomNode appNode = appNodes.at(j);

            // check bundle item
            QString itemId = appNode.firstChildElement("appid").text();
            BundleItem *item = globalBundle->getApp(itemId);
            if (item == NULL) {
                item = new BundleItem();
#ifdef DOWNLOAD_DIR_USE_USERNAME
                item->id = itemId + "_" + gid + username;
#else
                item->id = itemId + "_" + gid;
#endif
            }
            appList.append(item);
            item->tcid = gid;
            item->name = appNode.firstChildElement("appname").text();
            item->size = appNode.firstChildElement("filesize").text().toUInt();
            item->pkg = appNode.firstChildElement("packageName").text();
            item->version = appNode.firstChildElement("versionName").text();
            item->versionCode = appNode.firstChildElement("versionCode").text().toInt();
            item->path = downloadDirPath + "/" + appNode.firstChildElement("filename").text();
            item->down_url = appNode.firstChildElement("down_url").text();
            item->md5 = appNode.firstChildElement("md5").text();

            DBG("gid=%s,packname:%s\n", qPrintable(gid),qPrintable(item->pkg));
            QDomElement e = appNode.firstChildElement("installsite");
            if (e.isNull()) {
                item->instLocation = -1;
            } else {
                item->instLocation = e.text().toInt();
            }
        }

        // get hidden items
        appList.append(loadWebHiddenBundleItems(gid, ispublic));

        bundle->addApps(appList);
        ret.append(bundle);

        break;  // NOTE: 只取第一个套餐，服务器正常情况只应返回一个套餐信息
    }

    return ret;
}

void AppConfig::saveBundle(Bundle *bundle) {
    if(bundle == globalBundle) {
        return;
    }
    bundleDb->setData(bundle);
}

void AppConfig::saveBundleItem(BundleItem *item) {
    bundleItemDb->setData(item);
}

void AppConfig::deleteBundles(const QStringList &ids) {
    bundleDb->removeAllData(ids);
}

void AppConfig::deleteBundleItems(const QStringList &ids) {
    bundleItemDb->removeAllData(ids);
}

void AppConfig::saveSysWhiteList(const QString &fingerprint, const QStringList &whiteList) {
    doNotCleanMutex.lock();
    doNotCleanMap.insert(fingerprint, whiteList);
    doNotCleanMutex.unlock();

    DoNotClean *data = new DoNotClean();
    data->fingerprint = fingerprint;
    data->pkgList = whiteList;
    doNotCleanDb->setData(data);
    delete data;
}

QStringList AppConfig::getSysWhiteList(const QString &fingerprint) {
    QStringList ret;
    doNotCleanMutex.lock();
    if(doNotCleanMap.contains(fingerprint)) {
        ret = doNotCleanMap.value(fingerprint);
    }
    doNotCleanMutex.unlock();
    return ret;
}

QList<DoNotClean *> AppConfig::getSysWhiteLists() {
    QList<DoNotClean *> list;
    doNotCleanMutex.lock();
    QMapIterator<QString, QStringList> it(doNotCleanMap);
    while(it.hasNext()) {
        it.next();
        DoNotClean *d = new DoNotClean();
        d->fingerprint = it.key();
        d->pkgList = it.value();
        list.append(d);
    }
    doNotCleanMutex.unlock();
    return list;
}

void AppConfig::deleteSysWhiteList(const QString& fingerprint) {
    doNotCleanMutex.lock();
    doNotCleanMap.remove(fingerprint);
    doNotCleanMutex.unlock();
    doNotCleanDb->removeDataById(fingerprint);
}

bool AppConfig::getInstallFlags()
{
    QUrl url(sUrl + "/GetDesktop.ashx");
    controller->loginManager->revokeUrl(url);

    QString encUserID;
    ZDMHttp::url_escape(controller->appconfig->username.toUtf8(), encUserID);
    QString query = QString("action=getnotuninstalllist&username=%1").arg(encUserID);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("get install flags url=<%s>\n", uri, out.data());
    free(uri);

    QJson::Parser p;
    QVariantMap map;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();
    int code = map.value("code").toInt();
    if (1000 == code) {
        bInstallGamehouse = map.value("installcloudgame").toBool();
        bNeedYunOSDrag = map.value("isdrag").toBool();
        bNeedFastinstall = map.value("onlyrun").toBool();
        uninstallBlacklist.clear();
        uninstallBlacklist.append(map.value("appnames").toString().split(","));
        return true;
    }

    return false;
}

bool AppConfig::getDesktopAdaptiveList()
{
    QUrl url(sUrl + "/GetDesktop.ashx");
    controller->loginManager->revokeUrl(url);

    QString query = QString("action=getadaptivelist");
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("get desktop adaptive url=<%s>\n", uri);
    free(uri);

    QJson::Parser p;
    QVariantMap map;
    QVariantList list;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();
    list = map.find("brands").value().toList();
    desktopAdaptiveList.clear();
    foreach (const QVariant &va, list) {
        desktopAdaptiveList.append(va.toString());
    }

    return true;
}

bool AppConfig::getSelfRunApps()
{
    QUrl url(sUrl + "/Rom.ashx?action=selfrunapp");
    controller->loginManager->revokeUrl(url);

    QString encUserID;
    ZDMHttp::url_escape(controller->appconfig->username.toUtf8(), encUserID);
    QString query = QString("action=selfrunapp&un=%1").arg(encUserID);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail!\n");
        free(uri);
        return false;
    }
    DBG("get self auto apps url=<%s>\n", uri);
    free(uri);

    QDomDocument dom;
    if (!dom.setContent(out)) {
        DBG("invalid xml!\n");
        return false;
    }

    // check limit counts
    int limit = -1;
    bool ok;
    QDomNodeList limitNodes = dom.elementsByTagName("response");
    for (int i = 0; i < limitNodes.size(); ++i) {
        QDomNode node = limitNodes.at(i);
        int limitValue = node.firstChildElement("Limit").text().toInt(&ok);
        if (ok) {
            limit = limitValue;
        }
        break;
    }

    // get packagenames
    QDomNodeList nodes = dom.elementsByTagName("selfrunapps");
    QStringList packagenames;
    for (int i = 0; i < nodes.count(); ++i) {
        QDomNode node = nodes.at(i);
        QDomNodeList appNodes = node.toElement().elementsByTagName("selfrunapp");
        for (int j = 0; j < appNodes.count(); ++j) {
            QDomNode appNode = appNodes.at(j);
            QString packageName = appNode.firstChildElement("packagename").text();
            packagenames.append(packageName);
        }
    }

    selfRunAppsStr.clear();
    int count = 0;
    foreach (const QString &packagename, packagenames) {
        if (limit == -1 || count < limit) {
            if (count == 0) {
                selfRunAppsStr.append(packagename);
            } else {
                selfRunAppsStr.append("]").append(packagename);
            }
            count++;
        }
    }

    return true;
}

bool AppConfig::getThemes()
{
    QUrl url(sUrl + "/GetDesktop.ashx");
    controller->loginManager->revokeUrl(url);

    QString encUserID;
    ZDMHttp::url_escape(controller->appconfig->username.toUtf8(), encUserID);
    QString query = QString("action=getthemes&username=%1").arg(encUserID);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("get themes url=<%s>\n", uri);
    free(uri);

    QJson::Parser p;
    QVariantMap map;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();

    int code = map.find("code").value().toInt();
    if (1000 != code) {
        DBG("do nothing, beacuse code=%d\n", code);
        return false;
    }

    themesMap.clear();

    QVariantList list;
    list = map.find("themelist").value().toList();
    foreach (const QVariant &va, list) {
        QVariantMap map = va.toMap();
        QString theme = map.find("theme").value().toString();

        QStringList modelList;
        QVariantList models = map.find("models").value().toList();
        foreach (const QVariant &model, models) {
            modelList.append(model.toString());
        }

        if (!theme.isEmpty() && !modelList.isEmpty()) {
            themesMap.insert(theme, modelList);
        }
    }

    themesCodeStr.clear();
    QVariantList codeList = map.find("iconcodes").value().toList();
    foreach (const QVariant &var, codeList) {
        themesCodeStr.append(var.toString());
    }

    return true;
}

bool AppConfig::getAppActivate()
{
    QUrl url(sUrl + "/GetDesktop.ashx");
    controller->loginManager->revokeUrl(url);

    QString query = QString("action=getdeviceapp&un=%1").arg(username);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("get app activate url=<%s>\n", uri);
    free(uri);

    QJson::Parser p;
    QVariantMap map;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();

    int code = map.find("code").value().toInt();
    if (1000 != code) {
        DBG("do nothing, beacuse code=%d\n", code);
        return false;
    }

    appActivateMap.clear();

    QVariantList list;
    list = map.find("deviceapps").value().toList();
    foreach (const QVariant &va, list) {
        QVariantMap map = va.toMap();
        QString packname = map.find("packname").value().toString();
        QString activity = map.find("activity").value().toString();
        appActivateMap.insert(packname, activity);
    }

    return true;
}

bool AppConfig::getInstallFailureMaxRate()
{
    QUrl url(sUrl + "/GetDesktop.ashx");
    controller->loginManager->revokeUrl(url);

    QString query = QString("action=getappinstallrate&username=%1").arg(username);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("get max allow install failure rate url=<%s>\n", uri);
    free(uri);

    QJson::Parser p;
    QVariantMap map;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();

    int code = map.find("code").value().toInt();
    if (1000 != code) {
        DBG("do nothing, beacuse code=%d\n", code);
        return false;
    }

    installFailureMaxRate = (map.find("appinstallrate").value().toInt());

    return true;
}

bool AppConfig::getReddot()
{
    // get data
    QUrl url(sUrl + "/GetDesktop.ashx");
    controller->loginManager->revokeUrl(url);

    QString query = QString("action=getreddot&username=%1").arg(username);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("get red dot url=<%s>\n", uri);
    free(uri);

    // save data to local
    QString dirPath = qApp->applicationDirPath() + "/DataFromServer";
    QDir dir(dirPath);
    dir.mkpath(dirPath);

    QFile file(reddotFilePath = dirPath + "/reddot");
    file.remove();
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(ZStringUtil::fromGBK(out).toUtf8());
        file.flush();
        file.close();
    }

    return true;
}

void AppConfig::cleanupFile()
{
    QString path = qApp->applicationDirPath();
    QDir dir(path);
    // remove common xml
    QStringList files = dir.entryList(QStringList() << "*_config.xml", QDir::Files);
    foreach (const QString &file, files) {
        DBG("remove config.xml, %s\n", qPrintable(file));
        QFile::remove(path + "/" + file);
    }
    files = dir.entryList(QStringList() << "*_workspace.xml", QDir::Files);
    foreach (const QString &file, files) {
        DBG("remove workspace.xml, %s\n", qPrintable(file));
        QFile::remove(path + "/" + file);
    }
}

bool AppConfig::getDesktop()
{
    // get brand and model list
    QStringList params;
    if(!controller->appconfig->MZDesktop){
        return true;
    }
    bool ret = getDesktopParams(params);

    if (!ret) {
        return false;
    }

    foreach (const QString &param, params) {
        QStringList list = param.split("|");
        if (list.size() != 4) {
            DBG("invalid parma, parma=%s\n", qPrintable(param));
            continue;
        }

        QString brand = list.at(0);
        QString model = list.at(1);
        QString tcid = list.at(2);
        QString ispublic = list.at(3);

        QString sysType;
#if defined(Q_OS_WIN)
        sysType = "win";
#elif defined(Q_OS_LINUX)
        sysType = "linux";
#elif defined(Q_OS_OSX)
        sysType = "osx";
#endif

        QUrl url(sUrl + "/GetDesktop.ashx");
        controller->loginManager->revokeUrl(url);

        QString encUsername;
        QString encBrand;
        QString encModel;
        QString encTcid;
        QString encIspublic;
        QString encSys;
        ZDMHttp::url_escape(username.toUtf8(), encUsername);
        ZDMHttp::url_escape(brand.toUtf8(), encBrand);
        ZDMHttp::url_escape(model.toUtf8(), encModel);
        ZDMHttp::url_escape(tcid.toUtf8(), encTcid);
        ZDMHttp::url_escape(ispublic.toUtf8(), encIspublic);
        ZDMHttp::url_escape(sysType.toUtf8(), encSys);

        QString query = QString("action=getdesktop&customerid=%1&brand=%2&model=%3&tcid=%4&ispublic=%5&sys=%6")
                .arg(encUsername)
                .arg(encBrand)
                .arg(encModel)
                .arg(encTcid)
                .arg(encIspublic)
                .arg(encSys);
#ifdef USE_QT5
        url.setQuery(checkInstalledQuery);
#else
        url.setEncodedQuery(query.toLocal8Bit());
#endif

        // get data
        QByteArray out;
        char *uri = strdup(url.toEncoded().data());
        if (0 != ZDMHttp::get(uri, out)) {
            DBG("connect server fail\n");
            DBG("connect error:%s\n",out.data());
            free(uri);
            return false;
        }
        DBG("get desktop config url=<%s>\n", uri);
        free(uri);

        // save data to local
        QString dirPath = qApp->applicationDirPath() + "/DataFromServer/Desktop";
        QDir dir(dirPath);
        dir.mkpath(dirPath);

        QFile file(dirPath + "/" + QString(param).replace("|", "_").replace(QString::fromUtf8("默认"), "default"));
        file.remove();
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            file.write(ZStringUtil::fromGBK(out).toUtf8());
            file.flush();
            file.close();
        }
    }

    return true;
}

bool AppConfig::getDesktopParams(QStringList &parmas)
{
    QUrl url(sUrl + "/GetDesktop.ashx");
    controller->loginManager->revokeUrl(url);

    QString query = QString("action=getdesktopbyuser&customerid=%1").arg(username);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if (0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("get brand and model list for desktop url=<%s>\n", uri);
    free(uri);

    QJson::Parser p;
    QVariantMap map;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();

    int code = map.find("code").value().toInt();
    if (1000 != code) {
        DBG("do nothing, beacuse code=%d\n", code);
        return false;
    }

    parmas.clear();

    QVariantList list;
    list = map.find("desktopbrandmodel").value().toList();
    foreach (const QVariant &va, list) {
        QVariantMap map = va.toMap();
        QString brand = map.find("brand").value().toString();
        QString model = map.find("model").value().toString();
        QString tcid = map.find("tcid").value().toString();
        QString ispublic = map.find("ispublic").value().toString();
        parmas.append((QStringList() << brand << model << tcid << ispublic).join("|"));
    }

    return true;
}

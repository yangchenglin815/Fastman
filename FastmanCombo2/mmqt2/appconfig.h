#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QObject>
#include <QList>
#include <QMutex>
#include "zdatabaseutil.h"

#include <QHostInfo> 

class Bundle;
class BundleItem;

class DoNotClean {
public:
    QString fingerprint;
    QStringList pkgList;

};


class DoNotCleanDb : public ZDatabaseUtil<DoNotClean*> {
public:
    DoNotCleanDb();
    QByteArray toByteArray(DoNotClean *obj);
    DoNotClean *fromByteArray(const QByteArray &data);
    QString getId(DoNotClean *obj);
    void setId(DoNotClean *obj, const QString& id);
};


class BundleDb : public ZDatabaseUtil<Bundle*> {
    Bundle *globalBundle;
public:
    BundleDb(Bundle *globalBundle);
    QByteArray toByteArray(Bundle *obj);
    Bundle *fromByteArray(const QByteArray &data);
    QString getId(Bundle *obj);
    void setId(Bundle *obj, const QString& id);
};


class BundleItemDb : public ZDatabaseUtil<BundleItem*> {
    Bundle *globalBundle;
public:
    BundleItemDb(Bundle *globalBundle);
    QByteArray toByteArray(BundleItem *obj);
    BundleItem *fromByteArray(const QByteArray &data);
    QString getId(BundleItem *obj);
    void setId(BundleItem *obj, const QString& id);
};


class InstLog {
public:
    QString id;
    uint time;
    QString json;
    QString url;
    QString result;
    QString adbSerial;

    bool isInstalled;
    bool canReplace;
    QString installHint;
};

class InstLogDb : public ZDatabaseUtil<InstLog *> {
public:
    InstLogDb(const QString& tablename);
    QByteArray toByteArray(InstLog *obj);
    InstLog *fromByteArray(const QByteArray &data);
    QString getId(InstLog *obj);
    void setId(InstLog *obj, const QString &id);
};

class GlobalController;
class AppConfig : public QObject {
    GlobalController *controller;
    BundleDb *bundleDb;
    BundleItemDb *bundleItemDb;
    DoNotCleanDb *doNotCleanDb;


    QMutex doNotCleanMutex;
    QMap<QString, QStringList> doNotCleanMap;

    bool loadWebBundles(QList<Bundle *> &bundles);
    QList<Bundle *> loadWebBundles(const QString &gid, QString ispublic = "true");  // NOTE: get bundle items from bundle
    QList<BundleItem *> loadWebHiddenBundleItems(const QString &gid, const QString &ispublic);

    QStringList autoInstBundles;
    QMutex autoInstBundlesMutex;
public:
    InstLogDb *instCacheDb;
    InstLogDb *instLogDb;
    AppConfig(QObject *parent = 0);
    ~AppConfig();

    void loadCfg();
    void saveCfg();

    bool CMCCFirst;
    bool offlineMode;
    bool YAMUser;  // 判断是否是柚安米定制帐号
    bool CustomDesktop; //判断是否用新桌面
    bool loggedIn;
    QString username;
    QString opername;
    QString encpasswd;
    QString sUrl;
    QString platform;

    bool useLAN;
    bool spInstall;
    QString LANUrl;
    QString LANPort;

    Bundle *globalBundle;
    QStringList getAutoInstItemIds();
    void setAutoInstBundles(const QStringList &list);
    inline QStringList getAutoInstBundles() {return autoInstBundles;}
    void addAutoInstBundles(const QStringList &list);
    void removeAutoInstBundle(const QStringList &list);
    void clearAutoInstBundles();
    bool hasAutoInstBundles();
    bool isAutoInstBundle(const QString& id);

    bool printfLog;
    bool bAsyncInst;
    bool bNeedZAgentInstallAndUninstall;
    bool bTryRoot;
    bool bUninstUsrApp;
    bool bUninstSysApp;
    unsigned short alertType;
    int rootFlag;
    int bundleType;
	bool DelXML;
    bool bPauseInstall;

    bool initDb();
    bool core_loadAllData();  // from database

    QString downloadDirPath;
    bool checkAndCleanDownload();
    void cleanDownlaodConfig();
    void loadDonwloadConfig();
    void startDownloadItems();

    void saveBundle(Bundle *bundle);
    void saveBundleItem(BundleItem *item);
    void deleteBundles(const QStringList& ids);
    void deleteBundleItems(const QStringList& ids);

    void saveSysWhiteList(const QString& fingerprint, const QStringList& whiteList);
    QStringList getSysWhiteList(const QString& fingerprint);
    QList<DoNotClean *> getSysWhiteLists();
    void deleteSysWhiteList(const QString& fingerprint);

    // install prop from web
    bool bAllowPCUploadInstallResultWhenAsync;
    bool hiddenTCShowFlag;
    QString showHiddenTCTime;
    bool MZDesktop;
    bool bInstallGamehouse;
    bool bNeedYunOSDrag;
    bool bNeedFastinstall;
    bool bAllowYunOSReplaceIcon;
    QStringList uninstallBlacklist;
    bool getInstallFlags();
    QStringList desktopAdaptiveList;
    bool getDesktopAdaptiveList();  // DX桌面适配机型
    QString selfRunAppsStr;
    bool getSelfRunApps();  // 有界面应用启动列表
    QMap<QString, QVariant> themesMap;
    bool getThemes();
    QString themesCodeStr;
    QMap<QString, QVariant> appActivateMap;  // 在设备管理器中激活应用列表
    bool getAppActivate();
    int installFailureMaxRate;  // 安装失败率的上限值
    bool getInstallFailureMaxRate();
    QString reddotFilePath;  // 红点本地Json文件
    bool getReddot();
    bool getDesktop();
    bool getDesktopParams(QStringList &parmas);  // str=brand|model|tcid|ispublic
    void cleanupFile();

private slots:
	void lookedUp(const QHostInfo &host);
};

#endif // APPCONFIG_H

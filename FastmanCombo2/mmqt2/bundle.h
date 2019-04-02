#ifndef BUNDLE_H
#define BUNDLE_H

#include <QString>
#include <QList>
#ifndef NO_GUI
#include <QPixmap>
#endif
#include <QMutex>

class BundleItem {
public:
    QString id;
    QString tcid;
    QString name;
    quint64 size;
    QString pkg;
    QString version;
    int versionCode;
    bool isHiddenItem;
    QString platform;

    // from dotNet, use system only, others don't care
    enum {
        INST_LOCATION_SYSTEM = 0,
        INST_LOCATION_USER = 1,
        INST_LOCATION_TF = 2
    };
    int instLocation;

    int minSdk;
    bool isIme;

    QString path;
    QString down_url;
    int download_status;
    uint mtime;
#ifndef NO_GUI
    QImage icon;
#endif
    QByteArray iconData;
    QString md5;

    BundleItem();
    bool parseApk(const QString& apk_path, bool full_parse = false);
};

class Bundle {
    QList<BundleItem*> appList;
    QMutex appMutex;

    QList<Bundle*> bundleList;
    QMutex bundleMutex;
public:
    Bundle();
    ~Bundle();

    // from dotNet
    enum {
        TYPE_APK = 1,
        TYPE_AGG = 2,
        TYPE_FORCE = 3
    };

    QString id;
    QStringList itemIDsForOfflineMode;
	QString selBundleID;
    QString name;
    quint64 size;

    bool enable;
    bool readonly;  // true: web bundle, false: local bundle
    int type;
    int leastMem; // MB
    int leastStore; // MB
    QString ispublic;  // 1:true; 0:false
    int ostype; //bundle varity: 0 默认; 1 安卓; 2 阿里云

#ifndef NO_GUI
    static QPixmap getIcon(int index);
#endif
    QString getDescription();

    BundleItem *getApp(const QString& id);
    Bundle *getBundle(const QString& id);

    QStringList getForceItemIds();
    Bundle *getAggBundle(int memFreeMB, int storeFreeMB);

    QList<BundleItem*> getAppList();
    QList<BundleItem*> getAppList(const QStringList& ids);
    QStringList getAppIds();

    bool addApp(BundleItem* app);
    int addApps(const QList<BundleItem*>& apps);

    bool removeApp(BundleItem* app);
    int removeApps(const QList<BundleItem*>& apps);

    bool removeApp(const QString& id);
    int removeApps(const QStringList& ids);

    QList<Bundle*> getBundleList();
    QList<Bundle*> getBundleList(const QStringList& ids);
    QStringList getBundleIds();

    bool addBundle(Bundle* bundle);
    int addBundles(const QList<Bundle*>& bundles);

    bool removeBundle(Bundle* bundle);
    int removeBundles(const QList<Bundle*>& bundles);

    bool removeBundle(const QString& id);
    int removeBundles(const QStringList& ids);
};

#endif // BUNDLE_H

#ifndef DRIVERHELPER_H
#define DRIVERHELPER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QStringList>
#include <QThread>
#include <QWidget>
#include <QTextStream>

#include <Windows.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <regstr.h>

class DriverDeviceNode {
public:
    QString name;
    QString device_id;
    QString device_id_long;
    QString parent_devId;
    QString vid;
    QString pid;

    unsigned long status;
    unsigned long problem;

    DriverDeviceNode();
};

class WinOSInfo {
public:
    WinOSInfo();
    bool detect();

    bool isWow64;
    uint platform;
    uint majorVer;
    uint minorVer;
    uint buildNum;
};

class DriverInfo {
public:
    DriverInfo() { bsigned = 1;}
    QString name;
    QString device_id;
    QString vid;
    QString pid;

    QString infPath;
    QString drvPath;
    QString drvVersion;

    QString oemFile;
    QStringList files;
    QStringList filesInWindir;

    int bsigned;

    QByteArray toXml(WinOSInfo &osInfo);
    bool fromXml(const QByteArray &data, WinOSInfo &osInfo);
};

class UsbDbInfo {
public:
    QString vid;
    QString pid;
    QString brand;
    QString model;
};

class DriverHelper : public QObject
{
    Q_OBJECT
    QList<DriverDeviceNode *> deviceList;
    QMutex mutex;
    QString devcon;
    QString dpinst;
    QList<QString> patchInfList;

    QString GetDeviceStringProperty(__in HDEVINFO Devs, __in PSP_DEVINFO_DATA DevInfo, __in DWORD Prop);
    bool getDriverFilesByInf(const QString& inf, DriverInfo& drvInfo, QString &errmsg);

public:
#ifdef USE_DDK
    void enumerateUsbControllers();
#endif
    void loadDeviceList(GUID *guid, int flags);

    QList<UsbDbInfo *> dbList;
    QMutex dbMutex;
public:
    WinOSInfo osInfo;

    DriverHelper(QObject *parent = 0);
    ~DriverHelper();

    void reload(bool phonesOnly, bool presentOnly);
    QList<DriverDeviceNode *> getDeviceList();
    QList<DriverDeviceNode *> getDeviceList(const QStringList& ids);
    DriverDeviceNode *getDeviceById(const QString& id);
	void GetDeviceID_Name(QString infNative,QString &strDeviceID,QString &strDeviceName);
	bool InstallYunDevice(QString infNative,QString &strDeviceID,QString &strDeviceName);

    bool getDriverInfo(const QString& zip, DriverInfo& info, QString& errmsg);
    bool updateDrv(DriverInfo& info, const QString& path, const QString& device_id, QString& errmsg);
    bool updateDrvYun(DriverInfo &info, const QString &path, const QString &device_id, QString &errmsg,QString &str_info);
    void patchDrvYun(QString inf);

    bool getDriverInfo(DriverDeviceNode *node, DriverInfo& info, QString& errmsg);
    bool patchGetDriverInfo(DriverDeviceNode *node, DriverInfo& info, QString& errmsg);
    bool isPatchInf(QString name);
    bool toZip(DriverInfo& info, const QString& path, QString &errmsg);
    bool uninstallDrv(DriverDeviceNode *node, DriverInfo& info, QString &errmsg, bool deleteInf);

    static QString getZipFileSum(const QString& path);
    static QString findFileSysPath(QString &name);

    bool loadUsbDB(QTextStream &stream);
    bool loadUsbDB(const QString& path);
    bool getNameInUsbDB(const QString& vid, const QString& pid, QString& brand, QString& model);

    bool uninstallDriver(const QString& path, QString &errmsg);
    bool installDriver(const QString& path, QString &errmsg);

    static QString getDriverDirCustomName(const QString& dir);
    static QString getDriverDirPlatformName(const QString& dir);
    static QString getDriverInfCustomName(const QString& inf);
    bool refreshDriver(const QStringList &infList, const QStringList &infDirList, QString &errmsg);
	bool refreshDriverYun(const QStringList &infList, const QStringList &infDirList, QString &errmsg);
signals:
    void signal_deviceListClear();
    void signal_gotDevice(DriverDeviceNode *device);

    void signal_error(int code, const QString& hint);
    void signal_progress(int progress, int total);
    void signal_addLog(const QString& log);
};

class RemoteDrvInfo {
public:
    int driver_id;
    QString url;
    QString version;

    int code;
    QString msg;
};

class DriverUploadThread : public QThread {
    Q_OBJECT
    DriverHelper *drv;
public:
    static bool checkRemoteDriverExists(WinOSInfo& os, const QString& deviceName, const QString& deviceId, RemoteDrvInfo &remoteInfo, const QString &filesum, const QString& version, int bsigned);
    static bool uploadRemoteDriver(DriverHelper &drv, DriverInfo& info, const QString &fileName, const QString &filesum);

    DriverUploadThread(DriverHelper *drv, QObject *parent = 0);
    void run();
signals:
    void signal_addLog(DriverDeviceNode *device, const QString& log);
};

class DriverInstallThread : public QThread {
    Q_OBJECT
    DriverHelper *drv;
public:
    DriverInstallThread(DriverHelper *drv, QObject *parent = 0);
    void run();
private slots:
    void slot_downloadProgress(void *p, int value, int total);

signals:
    void signal_addLog(DriverDeviceNode *device, const QString& log);
    void signal_foundDriver(DriverDeviceNode *device, bool found);
    void signal_downloadProgress(DriverDeviceNode *device, int value, int total);
    void signal_downloadDriver(DriverDeviceNode *device, bool success, const QString& hint);
    void signal_installDriver(DriverDeviceNode *device, bool success, const QString& hint);
};

class ProblemCollectThread : public QThread {
    Q_OBJECT

    QMutex mutex;
    QMap<QString, unsigned long> problemList;
public:
    ProblemCollectThread(QObject *parent = 0);
    void run();

    QString getProblemList();
};

class DriverManager : public QObject {
    Q_OBJECT

    QWidget *_parent;

    DriverUploadThread *tUpload;
    ProblemCollectThread *tCollect;
    DriverInstallThread *tInst;
public:
    DriverManager(QWidget *parent);
    ~DriverManager();

    inline QString getProblemList() {
        return tCollect->getProblemList();
    }

    enum {
        TYPE_INFO = 0,
        TYPE_ERR,
        TYPE_PROGRESS
    };

    inline DriverUploadThread* uploadThread() {
        return tUpload;
    }

    inline DriverInstallThread* installThread() {
        return tInst;
    }

private slots:
    void slot_drvError(DriverDeviceNode *device, int code, const QString& hint);
    void slot_drvLog(DriverDeviceNode *device, const QString& log);
    void slot_downloadProgress(DriverDeviceNode* device, int value, int max);

public slots:
    void slot_startWork();
    void slot_usbToggled();

signals:
    void signal_hint(int type, const QString& title, const QString& msg);
};

#endif // DRIVERHELPER_H

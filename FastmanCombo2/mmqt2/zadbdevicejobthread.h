#ifndef ZADBDEVICEJOBTHREAD_H
#define ZADBDEVICEJOBTHREAD_H

#include "adbdevicejobthread.h"
#include "adbclient.h"
#include "fastman2.h"
#include "formdevicelog.h"

class GlobalController;
class ZAdbDeviceJobThread;
class ZAdbDeviceNode;
class BundleItem;
class LauncherEditor;
class LauncherMaker;

class RootAndCleanThread : public QThread {
    Q_OBJECT
    GlobalController *controller;
    AdbDeviceNode *node;
    ZAdbDeviceJobThread *jobThread;
    QStringList uninstallBlacklist;
    bool bNeedZAgentInstallAndUninstall;

    void uninst_userapp(AdbClient &adb, FastManAgentClient &agentCli);
    void uninst_sysapp(AdbClient &adb, FastManAgentClient &agentCli);

    int core_hint(int type, const QString& hint, const QString& tag);
    int core_chooseToClean(void *data);

public:
    LauncherEditor *_editor;
    RootAndCleanThread(ZAdbDeviceJobThread *jobThread, AdbDeviceNode *node, bool bNeedZAgentInstallAndUninstall = false);
    ~RootAndCleanThread();
    void run();

signals:
    void sigDeviceRefresh(AdbDeviceNode *node);
    void sigDeviceStat(AdbDeviceNode *node, const QString& status);
    void sigDeviceProgress(AdbDeviceNode *node, int progress, int max);
    void sigDeviceLog(AdbDeviceNode *node, const QString& log, int type = FormDeviceLog::LOG_NORMAL);

    void signal_hint(void *ret, int type, const QString& hint, const QString& tag);
    void signal_chooseToClean(void *ret, void *data);
    void sig_checkHint();
    void sig_checkChoice();
};

class KeepAliveThread : public QThread {
    Q_OBJECT
    GlobalController *controller;
    AdbDeviceNode *node;
    ZAdbDeviceJobThread *jobThread;
public:
    KeepAliveThread(ZAdbDeviceJobThread *jobThread, AdbDeviceNode *node);
    ~KeepAliveThread();
    void run();
    int failCysCnt;
};

class BreakHeart : public QThread {
    Q_OBJECT
    GlobalController *controller;
    AdbDeviceNode *node;
    ZAdbDeviceJobThread *jobThread;
    QList<BundleItem *> items;
public:
    BreakHeart(ZAdbDeviceJobThread *jobThread, AdbDeviceNode *node, QList<BundleItem *> items);
    ~BreakHeart();
    void run();
private:
    bool killProcess(const QString &name, bool shellCmdNeedAddTaskForQueue = false);
};

class ZAdbDeviceJobThread : public AdbDeviceJobThread
{
    Q_OBJECT
    friend class RootAndCleanThread;
    friend class KeepAliveThread;

    GlobalController *controller;

    bool bRootAndCleanFinished;
    bool bKeepAliveFinished;

    int core_hint(int type, const QString& hint, const QString& tag);
    int core_UIAutomator(void *p, void *q);

public:
    ZAdbDeviceJobThread(AdbDeviceNode *node, QObject *parent = 0);
    ~ZAdbDeviceJobThread();

    void core_run();
    void core_run_real();

private:
    // steps for mmqt2
    void init();
    void initCustomItems();
    bool judgeBundle();

    bool startDXClick();
    bool startUIClick(bool shellCmdNeedAddTaskForQueue = false);

    bool initZMaster();
    bool desktopisbool();

    bool initZAgent();
    void getdeviceinfo();
    bool checkDeviceInfo();
    bool checkInstallLog();

    void uninstallApk();
    bool pushApk();
    bool installApk();
    bool specialInstall();
    bool uploadInstallFailed();
    bool uploadInstallLog();
    bool doFinsalTask();

    bool checkYunOSDrag();
    bool checkJobs(bool waitForNullJobs = true);
    bool checkNeedReplaceDesktopTheme();
    void setDesktop();
    bool setAutoStart();

    QStringList uninstallBlacklist;
    QStringList getUninstallBlacklist();
    QString unshowList;

    // common api
    void setScreenTimeout();
    bool killProcess(const QString &name, bool shellCmdNeedAddTaskForQueue = false);
    int getSDKVersion();
    bool installByZAgent(const QString &path, const QString &packageName);
    bool uninstallByZAgent(const QString &packageName);

    // some properties
    ZAdbDeviceNode *znode;
    bool useAsync;
    bool uninstallUserApp;

    bool isYunOSDevice;
    bool needInstallDesktop;
    bool needReplaceTheme;
    bool needInstallGamehouse;
    bool needYunOSDrag;
    bool needFastinstall;
    bool bNeedZAgentInstallAndUninstall;
    bool setScreenTimeoutResult;

    // install bundle info
    QStringList installBundleIDs;
    QStringList installBundleNames;
    QStringList installBundleItemIDs;
    QList<BundleItem *> customItems;

    // do for install result
    QString installFailedPkgs;  // common apps only
    QStringList installFailedDowngradeIDs;  // all apps
    QString uploadResultHint;
    int copyTotal;
    int copySuccessCounts;
    int installTotal;
    int installSuccessCounts;
    int failureRate;

    // other funcs
    void DoWithRedDot(QList<BundleItem *> items);
    void DoWithHZRedDot(QList<BundleItem *> items);
    void ReplaceIcons(LauncherMaker maker);

private slots:
    void slot_startRootAndCleanThread();
    void slot_startKeepAliveThread();

signals:
    void signal_startRootAndCleanThread();
    void signal_startKeepAliveThread();
    void signal_finished(bool result);

    void signal_hint(void *ret, int type, const QString& hint, const QString& tag);
    void signal_chooseToClean(void *ret, void *data);
    void sig_checkHint();
    void sig_checkChoice();

    void sigDeviceLog(AdbDeviceNode *node, const QString& log, int type = FormDeviceLog::LOG_NORMAL, bool needShow = true);
    void signal_initThread(AdbDeviceNode *node);
};

#endif // ZADBDEVICEJOBTHREAD_H                  

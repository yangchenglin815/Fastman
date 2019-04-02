
#include "zadbdevicejobthread.h"
#include "globalcontroller.h"
#include "zthreadpool.h"

#include "zadbdevicenode.h"
#include "bundle.h"
#include "appconfig.h"
#include "loginmanager.h"
#include "zdownloadmanager.h"
#include "zstringutil.h"
#include "dialogseltoclean.h"
#include "fastman2.h"
#include "zdmhttp.h"
#include "adbprocess.h"
#include "ThreadBase.h"
#include "QJson/json_scanner.h"
#include "dialoghint.h"
#include "zLaucherManager.h"
#include "launchermaker.h"
#include "launcherconfig.h"
#include "launchereditor.h"
#include "zlog.h"

#include <QUuid>
#include <QDir>
#include <QStringList>
#include <QSettings>
#include <QFile>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QDomDocument>
#include <QBuffer>
#include <QTime>
#include <QApplication>

#ifdef USE_JUNBO
#include "junbointerface.h"
#endif

#define DEVICE_LOG_USETYPE

#define ADD_LOG(x) do {\
    emit sigDeviceLog(node, x, FormDeviceLog::LOG_NORMAL);\
    agentCli.addLog(x); \
    } while(0);

#ifdef DEVICE_LOG_USETYPE
#define ADD_LOG_USETYPE(x, logType) do {\
    emit sigDeviceLog(node, x, logType);\
    agentCli.addLog(x); \
    } while(0);
#else
#define ADD_LOG_USETYPE(x, logType) ADD_LOG(x)
#endif

#define ADD_LOG_AND_HINT(x) do {\
    emit sigDeviceStat(node, x);\
    emit sigDeviceLog(node, x, FormDeviceLog::LOG_NORMAL);\
    agentCli.addLog(x); \
    } while(0);

#ifdef DEVICE_LOG_USETYPE
#define ADD_LOG_AND_HINT_USETYPE(x, logType) do {\
    emit sigDeviceStat(node, x);\
    emit sigDeviceLog(node, x, logType);\
    agentCli.addLog(x); \
    } while(0);
#else
#define ADD_LOG_AND_HINT_USETYPE(x, logType) ADD_LOG_AND_HINT(x)
#endif

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

static QString fromGBK(const QByteArray& data) {
    QTextCodec *c = QTextCodec::codecForName("GB2312");
    if(c == NULL) {
        return QString();
    }
    return c->toUnicode(data);
}

static QString calcMd5(const QByteArray& in) {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(in);
    QByteArray d = hash.result().toHex().toUpper();
    return QString(d);
}

RootAndCleanThread::RootAndCleanThread(ZAdbDeviceJobThread *jobThread, AdbDeviceNode *node, bool bNeedZAgentInstallAndUninstall) {
    DBG("+RootAndCleanThread %p\n", this);
    controller = GlobalController::getInstance();
    this->node = node;
    this->jobThread = jobThread;
    this->bNeedZAgentInstallAndUninstall = bNeedZAgentInstallAndUninstall;
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
    jobThread->bRootAndCleanFinished = false;
    controller->threadpool->addThread(this, "RootAndCleanThread_"+node->adb_serial);
}

RootAndCleanThread::~RootAndCleanThread() {
    DBG("~RootAndCleanThread %p\n", this);
    jobThread->bRootAndCleanFinished = true;
    controller->threadpool->removeThread(this);
}

int RootAndCleanThread::core_hint(int type, const QString &hint, const QString &tag) {
#ifndef NO_GUI
    int ret = -1;
    emit signal_hint(&ret, type, hint, tag);
    QEventLoop e;
    connect(this, SIGNAL(sig_checkHint()), &e, SLOT(quit()));
    for(;;) {
        e.exec();
        if(ret != -1) {
            break;
        }
    }
    DBG("ret = %d\n", ret);
    return ret;
#else
    if(!tag.isEmpty()) {
        QSettings config("mmqt2.ini", QSettings::IniFormat);
        int retvalue = config.value("hint/" + tag, -1).toInt();
        if ( 1 == retvalue ) {
            return QDialog::Accepted;
        }
    }
    return QDialog::Rejected;
#endif
}

int RootAndCleanThread::core_chooseToClean(void *data) {
#ifndef NO_GUI
    int ret = -1;
    DBG("ret = %d, addr = %p\n", ret, &ret);
    emit signal_chooseToClean(&ret, data);
    QEventLoop e;
    connect(this, SIGNAL(sig_checkChoice()), &e, SLOT(quit()));
    for(;;) {
        e.exec();
        DBG("ret = %d, addr = %p\n", ret, &ret);
        if(ret != -1) {
            break;
        }
    }
    DBG("ret = %d\n", ret);
    return ret;
#else
    return QDialog::Rejected;
#endif
}

void RootAndCleanThread::uninst_userapp(AdbClient &adb, FastManAgentClient &agentCli) {
    QString hint;
    ADD_LOG_USETYPE(tr("start uninstall user app"), FormDeviceLog::LOG_STEP);
    bool flag = false;
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)node;
    FastManClient cli(znode->adb_serial, znode->zm_port);
    if(!cli.getZMInfo(znode->zm_info)) {
        flag = true;
    }

    QByteArray out;
    QList<AGENT_APP_INFO*> appList;
    agentCli.getAppList(appList, false);

    QString strList;
    QStringList sList;
    QString strApp;
    QString sApk;
    QString sPackage;
    int tmp = 0;
    int a = 0;
    int appCount = appList.size();
    if  ( 0 == appCount ) {
        out = adb.pm_cmd("list package -f ");
        strList = out;
        sList = strList.split("package:");
        for(int i=0;i<sList.size();i++){
            strApp = sList.at(i);
            if (strApp == "") continue;
            if (strApp.contains("/system/")) continue;
            if (strApp.contains("com.dx.agent2")) continue;
            if (strApp.contains("com.mediatek.fwk.plugin")) continue;
            if (strApp.contains("com.baidu.input_huawei")) continue;
            a = strApp.indexOf("=");
            sApk = strApp.left(a);
            sPackage = strApp.right(strApp.length() - a - 1);
            if(strApp.contains("\n")){
                sPackage = sPackage.left(sPackage.length() - 2);
            }

            out = adb.pm_cmd("uninstall "+sPackage);
            tmp = 5;
        }
    }

    sigDeviceStat(node, tr("uninstalling user app"));
    agentCli.setHint(tr("uninstalling user app"));

    // uninstall
    QStringList blacklist;
    blacklist.append(controller->appconfig->uninstallBlacklist);

    bool hasRoot = adb.adb_cmd("shell:su -c id", 15000).contains("uid=0");

    int totalItems = appList.size();
    int needUninstallTotalItems = 0;
    int failedItems = 0;
    for (int i = 0; i < appList.size(); i++) {
        if (node->connect_stat != AdbDeviceNode::CS_DEVICE) {
            ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
            break;
        }

        emit sigDeviceProgress(node, i, totalItems);
        agentCli.setProgress(-1, i, totalItems);

        AGENT_APP_INFO *app = appList[i];
        if (app->packageName == "com.dx.agent2") {
            continue;
        }

        bool needUninstall = false;
        int appType = app->getType();
        if (!hasRoot) {
            // 没有Root时除了以下路径均尝试卸载
            if (!app->sourceDir.startsWith("/system/app/") && !app->sourceDir.startsWith("/system/priv-app/")
                    && !app->sourceDir.startsWith("/system/vendor/operator/app")) {
                needUninstall = true;
            }

            if (appType == AGENT_APP_INFO::TYPE_USER_APP) {
                needUninstall = true;
            }
        } else {
            // 已经Root时只尝试卸载以下类型app
            if (appType == AGENT_APP_INFO::TYPE_UPDATED_SYSTEM_APP || appType == AGENT_APP_INFO::TYPE_USER_APP) {
                needUninstall = true;
            }
        }
        if (needUninstall && !blacklist.contains(app->name)) {
            needUninstallTotalItems++;
            emit sigDeviceLog(node, tr("uninstalling [%1]").arg(app->name), FormDeviceLog::LOG_NORMAL);
            if (!bNeedZAgentInstallAndUninstall) {
                DBG("unisntall, pkg=%s\n", qPrintable(app->packageName));
                out = adb.pm_cmd("uninstall " + app->packageName);
                if (out.trimmed() != "Success") {
                    ++failedItems;
                    ADD_LOG_USETYPE(QString("%1 %2").arg(QString(out.trimmed())).arg(app->sourceDir), FormDeviceLog::LOG_WARNING);
                }
            } else {
                DBG("unisntall by ZAgent, pkg=%s\n", qPrintable(app->packageName));
                agentCli.uninstallApp(app->packageName, hint);
                sleep(5);
            }
        }
    }
    emit sigDeviceProgress(node, totalItems, totalItems);
    agentCli.setProgress(-1, totalItems, totalItems);
    agentCli.setHint(tr("uninstall user app finished"));

    if (failedItems > 0) {
        ADD_LOG_USETYPE(tr("uninstall total:%1, success:%2, failure:%3").arg(needUninstallTotalItems).arg(needUninstallTotalItems - failedItems).arg(failedItems),
                        FormDeviceLog::LOG_WARNING);
    } else {
        ADD_LOG(tr("uninstall total:%1, success:%2, failure:%3").arg(needUninstallTotalItems).arg(needUninstallTotalItems - failedItems).arg(failedItems));
    }

    qDeleteAll(appList.begin(), appList.end());
    appList.clear();
}

QMutex whiteListMutex;
void RootAndCleanThread::uninst_sysapp(AdbClient &adb, FastManAgentClient &agentCli) {
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)node;
    FastManClient rootCli(znode->adb_serial, znode->zm_port);
    QList<AGENT_APP_INFO*> appList;
    QString hint;
    ADD_LOG(tr("start uninstall system app"));
    QByteArray out;
    //out = adb.adb_cmd("shell:am start -n com.dx.agent2/com.dx.agent2.LogEmptyActivity -d \" " + tr("start uninstall system app") + "  \"");

    if(!rootCli.switchRootCli()) {
        ADD_LOG(tr("switch rootCli failed"));
        //out = adb.adb_cmd("shell:am start -n com.dx.agent2/com.dx.agent2.LogEmptyActivity -d \" " + tr("switch rootCli failed") + "  \"");
        return;
    }

    agentCli.getAppList(appList, true);

    DlgSelCleanParam *param = new DlgSelCleanParam();
    param->node = znode;
    param->list = &appList;

    whiteListMutex.lock();
    if((param->result = controller->appconfig->getSysWhiteList(znode->ag_info.fingerprint)).isEmpty()) {
        int r = core_chooseToClean(param);
        if(r != QDialog::Accepted) {
            ADD_LOG(tr("user canceled uninstall system app"));
            //out = adb.adb_cmd("shell:am start -n com.dx.agent2/com.dx.agent2.LogEmptyActivity -d \" " + tr("user canceled uninstall system app") + "  \"");
            delete param;
            whiteListMutex.unlock();
            return;
        }
    }
    whiteListMutex.unlock();

    // clean update sys app first
    QStringList blacklist;
    blacklist.append(controller->appconfig->uninstallBlacklist);
    foreach(AGENT_APP_INFO* app, appList) {
        if(node->connect_stat != AdbDeviceNode::CS_DEVICE) {
            break;
        }

        if (blacklist.contains(app->name)) {
            continue;
        }

        if(!param->result.contains(app->packageName)) {
            if(app->getType() == AGENT_APP_INFO::TYPE_UPDATED_SYSTEM_APP) {
                ADD_LOG(tr("uninstalling %1").arg(app->name));
                //out = adb.adb_cmd("shell:am start -n com.dx.agent2/com.dx.agent2.LogEmptyActivity -d \" " + tr("uninstalling %1").arg(app->name) + "  \"");
                adb.pm_cmd("uninstall "+app->packageName);

                AGENT_APP_INFO* rest_app = agentCli.getAppInfo(app->packageName, false);
                if(rest_app != NULL) {
                    rootCli.remove(rest_app->sourceDir);
                    delete rest_app;
                }
            } else if(app->getType() == AGENT_APP_INFO::TYPE_SYSTEM_APP) {
                ADD_LOG(tr("removing %1").arg(app->name));
                //out = adb.adb_cmd("shell:am start -n com.dx.agent2/com.dx.agent2.LogEmptyActivity -d \" " + tr("removing %1").arg(app->name) + "  \"");
                rootCli.exec(out, 3, "/system/bin/am", "force-stop", app->packageName.toUtf8().data());
                rootCli.remove(app->sourceDir);
            }
        }
    }
    while(!appList.isEmpty()) {
        delete appList.takeFirst();
    }

    ADD_LOG(tr("done uninstall system app"));
    //out = adb.adb_cmd("shell:am start -n com.dx.agent2/com.dx.agent2.LogEmptyActivity -d \" " + tr("done uninstall system app") + "  \"");
    delete param;
}

void RootAndCleanThread::run() {
    QString hint;
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)node;
    AdbClient adb(znode->adb_serial);
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);

    DBG("before uninst_userapp\n");
    if(controller->appconfig->bUninstUsrApp) {
        uninst_userapp(adb, agentCli);
        cli.getZMInfo(znode->zm_info);
        emit sigDeviceRefresh(node);
    }

    znode->hasRoot = false;
    znode->hasAgg = false;
    DBG("before getRoot\n");
    if(controller->appconfig->bTryRoot) {
        QStringList blist;
        ADD_LOG(tr("check root blacklist"));
        blist.append(QString::fromUtf8("系统已被"));
        blist.append(QString::fromUtf8("刷机"));
        if(cli.searchApkStr("/system/framework/framework-res.apk", blist)) {
            ADD_LOG(tr("system may not work after root"));
            if(QDialog::Accepted != core_hint(DialogHint::TYPE_YESNO,
                                              znode->getName() + tr("\ndevice might not work after root, continue?"),
                                              "ign_str_bl")) {
                ADD_LOG(tr("root aborted"));
                return;
            }
        }

        ADD_LOG(tr("start root"));
        FastManRooter rooter(znode->adb_serial, znode->zm_port, znode->jdwp_port);
        int rootFlag = controller->appconfig->rootFlag;
        if(znode->zm_info.cpu_abi != ZMINFO::CPU_ABI_ARM) {
            rootFlag &= FastManRooter::FLAG_MASTERKEY_ROOT;
            ADD_LOG(tr("not arm device, use only masterkey root"));
        }

        if(znode->zm_info.sdk_int < 20) {
            znode->hasRoot = rooter.getRoot(hint, rootFlag);
            if(!znode->hasRoot && (rootFlag & FastManRooter::FLAG_TOWEL_ROOT) != 0) {
                ADD_LOG(tr("tr method left, but may reboot the device if failed"))
                        if(QDialog::Accepted == core_hint(DialogHint::TYPE_YESNO,
                                                          znode->getName() + tr("\nthere is a new method but might reboot the device, have a try?"),
                                                          "ign_tr_reboot")) {
                    znode->hasRoot = rooter.getTowelRoot(hint);
                    if(!znode->hasRoot){
                        znode->hasRoot = rooter.getPocRoot(hint); // getPocRoot 是一个新的root方案
                    }
                }
            }
        } else {
            ADD_LOG(tr("unsupported device, ensure it's apiLevel < 20 (below android 4.4.4)"));
            znode->hasRoot = false;
            znode->hasAgg = false;
        }

        if(znode->hasRoot) {
            if(!rooter.checkSystemWriteable()) {
                znode->hasRoot = false;
                hint = tr("system readonly");
            }
        }

        ADD_LOG(tr("root %1, %2").arg(znode->hasRoot ? tr("Success") : tr("Failure"), hint));
    }

    DBG("before uninst_sysapp\n");
    if(znode->hasRoot && controller->appconfig->bUninstSysApp) {
        uninst_sysapp(adb, agentCli);
    }

    DBG("before instAgg\n");
    if(znode->hasRoot) {
        do {
            FastManClient instAggCli(znode->adb_serial, znode->zm_port);
            if(!instAggCli.switchRootCli()) {
                ADD_LOG(tr("install angougou failed, switch root fail"));
                break;
            }

            quint64 tsize;
            if(!instAggCli.getFileSize("/system/app/ZAgent2.apk", tsize)) {
                instAggCli.installApkSys("ZAgent2.apk", znode->zm_info.store_dir+"/ZAgent2.apk");
            }

            AGENT_APP_INFO *agg = agentCli.getAppInfo("com.zxly.assist", false); //获取不成功
            if(agg != NULL) {
                delete agg;
                znode->hasAgg = true;
                ADD_LOG(tr("angougou exists"));
                break;
            }
        } while(0);
    }

    cli.getZMInfo(znode->zm_info);
    emit sigDeviceRefresh(node);
}

BreakHeart::BreakHeart(ZAdbDeviceJobThread *jobThread, AdbDeviceNode *node, QList<BundleItem *> items) : QThread(jobThread){
    controller = GlobalController::getInstance();
    this->jobThread = jobThread;
    this->items = items;
    this->node = node;
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

BreakHeart::~BreakHeart() {
    this->items.clear();
    controller->threadpool->removeThread(this);
}

void BreakHeart::run() {
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *) node;
    QList<BundleItem *> items = this->items;
    AdbClient adb(node->adb_serial);
    QByteArray out;
    int totalsize = items.size();
    int total;
    while(1) {
        if (znode->connect_stat != AdbDeviceNode::CS_DEVICE) {
            DBG("device offline, stop install");
            return;
        }
        total = totalsize;
        foreach (BundleItem *item, items) {
            out = adb.pm_cmd("path " + item->pkg, 10000);
            if(!out.startsWith("package:")) {
                total--;
            }
        }
        //DBG("totalsize:%d...total:%d\n",totalsize, total);
        if (totalsize == total) {
            killProcess("uiautomator");
            break;
        }

        if (items.size() == 1) {
            msleep(30000);
        } else {
            msleep(60000);
        }
    }
}

bool BreakHeart::killProcess(const QString &name, bool shellCmdNeedAddTaskForQueue)
{
    AdbClient adb(node->adb_serial);
    QString hint;

    QByteArray out = adb.adb_cmd(QString("shell:ps | grep %1").arg(name));
    DBG("ps %s, out=<%s>\n", name.data(), out.constData());
    QTextStream stream(out);
    QStringList cmds;
    QString line;
    do {
        line = stream.readLine();
        QStringList list = line.split(" ");
        foreach (const QString str, list) {
            if (str.isEmpty()) {
                list.removeOne(str);
            }
        }
        if ((list.contains("shell") || list.contains("root")) && list.size() >= 2) {
            QString pid = list.at(1);
            // shell
            if (list.contains("shell")) {
                DBG("get shell pid: %s\n", qPrintable(pid));
                cmds.append(QString("kill %1").arg(pid));
            }
            // root
            if (list.contains("root")) {
                DBG("get root pid: %s\n", qPrintable(pid));
                cmds.append(QString("su -c 'kill %1'").arg(pid));  // NOTE: 不一定成功？
                cmds.append(QString("kill %1").arg(pid));
            }
        }
    } while (!line.isNull());

    if (!shellCmdNeedAddTaskForQueue) {
        foreach (const QString &cmd, cmds) {
            out = adb.adb_cmd("shell:" + cmd);
            DBG("kill uiautomator, out=<%s>\n", out.constData());
        }
    }
    return true;
}

KeepAliveThread::KeepAliveThread(ZAdbDeviceJobThread *jobThread, AdbDeviceNode *node)
    : QThread(jobThread) {
    controller = GlobalController::getInstance();
    this->jobThread = jobThread;
    this->node = node;
    jobThread->bKeepAliveFinished = false;
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
    failCysCnt = 0;
}

KeepAliveThread::~KeepAliveThread() {
    jobThread->bKeepAliveFinished = true;
}

void KeepAliveThread::run() {
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *) node;
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);

    // keepAlive
    while(!controller->needsQuit && znode->connect_stat == AdbDeviceNode::CS_DEVICE) {
        agentCli.setConnType(znode->node_index, ZMSG2_CONNTYPE_USB);
        QThread::msleep(2000);
    }
}

ZAdbDeviceJobThread::ZAdbDeviceJobThread(AdbDeviceNode *node, QObject *parent)
    : AdbDeviceJobThread(node, parent),
      bRootAndCleanFinished(true),
      bKeepAliveFinished(true),
      useAsync(false),
      uninstallUserApp(false),
      isYunOSDevice(false),
      needInstallDesktop(false),
      needReplaceTheme(false),
      needInstallGamehouse(false),
      needYunOSDrag(false),
      needFastinstall(false),
      setScreenTimeoutResult(false),
      installFailedPkgs(""),
      installFailedDowngradeIDs(QStringList()),
      uploadResultHint(""),
      copyTotal(0),
      copySuccessCounts(0),
      installTotal(0),
      installSuccessCounts(0),
      failureRate(99)
{
    controller = GlobalController::getInstance();

    node->jobThread = this;
    controller->threadpool->addThread(this, "AdbDeviceJobThread_"+node->adb_serial);
    connect(this, SIGNAL(signal_startRootAndCleanThread()), SLOT(slot_startRootAndCleanThread()));
    connect(this, SIGNAL(signal_startKeepAliveThread()), SLOT(slot_startKeepAliveThread()));
}

ZAdbDeviceJobThread::~ZAdbDeviceJobThread() {
    qDeleteAll(customItems.begin(), customItems.end());
    customItems.clear();

    node->jobThread = NULL;
    controller->threadpool->removeThread(this);
}

int ZAdbDeviceJobThread::core_hint(int type, const QString &hint, const QString &tag) {
#ifndef NO_GUI
    int ret = -1;
    emit signal_hint(&ret, type, hint, tag);
    QEventLoop e;
    connect(this, SIGNAL(sig_checkHint()), &e, SLOT(quit()));
    for(;;) {
        e.exec();
        if(ret != -1) {
            break;
        }
    }
    DBG("ret = %d\n", ret);
    return ret;
#else
    if(!tag.isEmpty()) {
        QSettings config("mmqt2.ini", QSettings::IniFormat);
        int retvalue = config.value("hint/" + tag, -1).toInt();
        if ( 1 == retvalue ) {
            return QDialog::Accepted;
        }
    }
    return QDialog::Rejected;
#endif
}

void ZAdbDeviceJobThread::slot_startRootAndCleanThread() {
    RootAndCleanThread *t = new RootAndCleanThread(this, node);
    connect(t, SIGNAL(sigDeviceRefresh(AdbDeviceNode*)), this, SIGNAL(sigDeviceRefresh(AdbDeviceNode*)));
    connect(t, SIGNAL(sigDeviceStat(AdbDeviceNode*,QString)), this, SIGNAL(sigDeviceStat(AdbDeviceNode*,QString)));
    connect(t, SIGNAL(sigDeviceProgress(AdbDeviceNode*,int,int)), this, SIGNAL(sigDeviceProgress(AdbDeviceNode*,int,int)));
    connect(t, SIGNAL(sigDeviceLog(AdbDeviceNode*,QString,int)), this, SIGNAL(sigDeviceLog(AdbDeviceNode*,QString,int)));

    connect(t, SIGNAL(signal_hint(void*,int,QString,QString)), this, SIGNAL(signal_hint(void*,int,QString,QString)));
    connect(t, SIGNAL(signal_chooseToClean(void*,void*)), this, SIGNAL(signal_chooseToClean(void*,void*)));
    connect(this, SIGNAL(sig_checkHint()), t, SIGNAL(sig_checkHint()));
    connect(this, SIGNAL(sig_checkChoice()), t, SIGNAL(sig_checkChoice()));
    t->start();
}

void ZAdbDeviceJobThread::slot_startKeepAliveThread() {
    KeepAliveThread *t = new KeepAliveThread(this, node);
    t->start();
}

void ZAdbDeviceJobThread::core_run() {
    core_run_real();
    while(!bRootAndCleanFinished || !bKeepAliveFinished) {
        xsleep(500);
    }
}

void ZAdbDeviceJobThread::core_run_real() {
    if (GlobalController::getInstance()->appconfig->bPauseInstall) {
        DBG("install is pause by user.\n");
        return;
    }

    znode = (ZAdbDeviceNode *)node;

    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    emit signal_initThread(node);
    ADD_LOG(tr("device plug in"));

    bool installSucceed = false;
    do {
        QTime time;
        time.start();

        emit sigDeviceProgress(node, 0, 100);
        emit sigDeviceStat(node, tr("init install thread..."));

        /// init
        init();

        /// judge the bundle
        if(!judgeBundle()){
            ADD_LOG_USETYPE(tr("select bundle error"), FormDeviceLog::LOG_ERROR);
            break;
        }

        /// start ZM
        if (!initZMaster()) {
            ADD_LOG_USETYPE(tr("init ZM failed"), FormDeviceLog::LOG_ERROR);
            break;
        }
        emit signal_startKeepAliveThread();

        /// start click
        startDXClick();
        startUIClick();

        /// start ZAgent
        if (!initZAgent()) {
            break;
        }

        /// check device info and update UI
        if (!checkDeviceInfo()) {
            break;
        }

        /// check upload history
        if (!checkInstallLog()) {
            break;
        }

        if (node->connect_stat != AdbDeviceNode::CS_DEVICE) {
            ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
            break;
        }

        do {
            /// check taks
            if (!checkJobs()) {
                break;
            }

            /// push
            if (!pushApk()) {
                break;
            }

            /// YunOS拖拽
            /// NOTE: 放在此步，不需要异步，请注意！
            //checkYunOSDrag();

            // 替换主题/拖拽“应用中心”
           // checkNeedReplaceDesktopTheme();
            if (isYunOSDevice && controller->appconfig->bAllowYunOSReplaceIcon) {
                // drag app icon
                DBG("need drag app icon\n");
                emit sigDeviceLog(node, tr("need drag app icon"), FormDeviceLog::LOG_NORMAL);
                bool result = false;
                do {
                    bool ret;
                    ret = adb.adb_push("dx.jar", "data/local/tmp/dx.jar", hint, 0777);
                    if (!ret) {
                        hint = QString("push dx.jar failed: %1").arg(hint.trimmed());
                        break;
                    }
                    xsleep(1000);
                    killProcess("uiautomator");
                    QString cmd;
                    QByteArray out;
                    int num = 3;
                    cmd = "uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#DargAliAppIcon";
                    out = adb.adb_cmd("shell:" + cmd);
                    DBG("drag app icon, out=<%s>\n", out.constData());
                    if(out.contains("/data/local/tmp/dx.jar does not exist")) {
                                            while(num){
                                                out = adb.adb_cmd("shell:" + cmd);
                                                DBG("redrag app market, out=<%s>\n", out.constData());
                                                if(!out.contains("/data/local/tmp/dx.jar does not exist")) {
                                                    break;
                                                }
                                                num--;
                                            }
                                        }
                    startUIClick();

                    result = true;
                } while (0);

                adb.adb_cmd("shell:am start -n com.dx.agent2/.LoadActivity -f 0x10400000");  // 激活精灵，避免长时间精灵没有置顶造成与ZM通信失败！！
                if (!result) {
                    DBG("drag app icon failed: %s\n", qPrintable(hint));
                    ADD_LOG_USETYPE(tr("drag app icon failed: %1").arg(hint), FormDeviceLog::LOG_WARNING);
                }
            }

            /// uninstall
            if (uninstallUserApp) {
                QTime time;
                time.start();
                uninstallApk();
                ADD_LOG(tr("uninstall elapsed:%1s").arg(time.elapsed() / 1000.0));
            }

            /// install
            if (controller->appconfig->spInstall) {
                if (!specialInstall()) {
                    break;
                }
            } else {
                if (!installApk()) {
                    break;
                }
            }

            /// upload
            if (!useAsync) {
                uploadInstallFailed();
            }

            if (!(controller->appconfig->bAllowPCUploadInstallResultWhenAsync && useAsync) && !uploadInstallLog()) {
                break;
            }

            /// final task
            if (!doFinsalTask()) {
                break;
            }

            installSucceed = true;

            ADD_LOG_USETYPE(tr("this time elapsed:%1s").arg(time.elapsed() / 1000.0), FormDeviceLog::LOG_FINISHED);
            time.restart();

            // 确保精灵心跳已结束
            while (!bKeepAliveFinished) {
                msleep(500);
            }

            emit signal_finished(installSucceed);
            return;
        } while (0);

    } while (0);

    emit signal_finished(installSucceed);

    agentCli.setScreenOffTimeout(1 * 60 * 1000);
    ADD_LOG_USETYPE(tr("install failed, please retry by reset the device state"), FormDeviceLog::LOG_ERROR);
}

int ZAdbDeviceJobThread::core_UIAutomator(void *p, void *q) {
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    QByteArray out = adb.adb_cmd("shell:uiautomator runtest uiclick.jar -c com.zx.uitest.run.TestAutoInstall");

    return 0;
}

void ZAdbDeviceJobThread::getdeviceinfo(){
    AdbClient adb(node->adb_serial);
    QByteArray out;
    out = adb.adb_cmd("shell:getprop ro.product.brand; getprop ro.product.model; getprop ro.csc.sales_code;");
    QList<QByteArray> lines = out.split('\n');
    for( int i = 0; i < lines.count(); i++) {
        QByteArray line = lines[i];
        while (line.endsWith('\r') || line.endsWith('\n')) {
            line.chop(1);
        }

        switch (i) {
        case 0:
            znode->brand = QString::fromUtf8(line).trimmed();
            break;
        case 1:
            znode->model = QString::fromUtf8(line).trimmed();
            break;
        case 2:
            znode->sales_code = QString::fromUtf8(line).trimmed();
            break;
        }
    }
    DBG("##brand=<%s>, model=<%s>\n", qPrintable(znode->brand), qPrintable(znode->model));
    emit sigDeviceRefresh(node);
}

void ZAdbDeviceJobThread::init()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QByteArray out;
    QString hint;
    unshowList = "com.tencent.mm,com.tencent.mobileqq,net.dx.cloudgame";
    ADD_LOG_USETYPE(tr("init install thread..."), FormDeviceLog::LOG_NORMAL);

    // znode
    getdeviceinfo();

    // check logFile
    znode->logFileName = QString("%1_%2_%3.log")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"))
            .arg(znode->brand + "," + znode->model)
            .arg(QString(znode->adb_serial).replace("#", "").replace("*", "").replace("_", ""));
    DBG("init log file: %s\n", qPrintable(znode->logFileName));
    QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
    QString dirPath = qApp->applicationDirPath() + "/log/" + currentDate;
    // TODO:
    //    ADD_LOG_USETYPE("<a href=\"" + dirPath + "/" + znode->logFileName + "\">" + znode->logFileName + "</a>", FormDeviceLog::LOG_WARNING);

    // check jobs
    installBundleIDs.clear();
    installBundleIDs = controller->appconfig->getAutoInstBundles();
    if (installBundleIDs.isEmpty()) {
        ADD_LOG_USETYPE(tr("please choose bundles for installing!"), FormDeviceLog::LOG_WARNING);
    }
    forever {
        installBundleIDs = controller->appconfig->getAutoInstBundles();
        if (!installBundleIDs.isEmpty()) {
            foreach (const QString &ID, installBundleIDs) {
                Bundle *bundle = controller->appconfig->globalBundle->getBundle(ID);
                if (bundle) {
                    installBundleNames.append(bundle->name);
                }
            }
            ADD_LOG_USETYPE(tr("has choose bundles: %1").arg(installBundleNames.join(",")), FormDeviceLog::LOG_NORMAL);
            break;
        }
        msleep(500);
    }
    installBundleItemIDs = controller->appconfig->getAutoInstItemIds();
    DBG("init bundles: %s, bundleItems: %s\n", qPrintable(installBundleIDs.join(",")), qPrintable(installBundleItemIDs.join(",")));

    // check properties
    useAsync = controller->appconfig->bAsyncInst;
    if(controller->appconfig->spInstall) {
        useAsync = false;
    }
    uninstallUserApp = controller->appconfig->bUninstUsrApp;
    QString properties = QString("%1,%2").arg(useAsync ? tr("use async") : tr("use sync")).arg(uninstallUserApp ? tr("uninstall user app") : tr("no uninstall user app"));
    DBG("init install properties: %s\n", qPrintable(properties));
    ADD_LOG_USETYPE(properties, FormDeviceLog::LOG_NORMAL);

    // check YunOS
    out = adb.adb_cmd("shell:getprop ro.yunos.version");
    DBG("check yunos version, out=<%s>\n", out.data());
    isYunOSDevice = !out.trimmed().isEmpty();
    znode->platform = (isYunOSDevice ? "YunOS" : "Android");
    controller->appconfig->platform = znode->platform;

#if 0
    out = adb.adb_cmd("shell:getprop ro.yunos.build.version");  // 3.2.0-F-20160613.9106
    DBG("check yunos build versin, out=<%s>\n", out.data());
    char date[100];
    sscanf(out.trimmed(), "%*[^-]-%*[^-]-%[^.]", date);
    QDate buildDate = QDate::fromString(QString(date), "yyyyMMdd");
    QDate borderDate = QDate::fromString("20160801", "yyyyMMdd");
    if (buildDate.isValid() && buildDate.daysTo(borderDate) <= 0) {
        bNeedZAgentInstallAndUninstall = true;
    } else {
        bNeedZAgentInstallAndUninstall = false;
    }
#else
    bNeedZAgentInstallAndUninstall = controller->appconfig->bNeedZAgentInstallAndUninstall;
#endif
    // TODO: 暂时只对小辣椒进行了处理，如果遇到其他机型需要类似处理，则bNeedZAgentInstallAndUninstall相关代码需要确认！！
//     if ("xiaolajiao" == znode->brand && "gm-t3" == znode->model) {
//         bNeedZAgentInstallAndUninstall = true;
//     }

    // clean up ZAgent and ZM, and so on
    adb.pm_cmd("clear com.android.settings");  // 隐藏开发者选项

    out = adb.adb_cmd("shell:am force-stop com.dx.agent2");
    killProcess("uiautomator");  // NOTE: 启动ZM时一定要kill掉之前异步安装存留在手机上的进程，不然ZM启动会失败？？
    adb.adb_cmd("shell:pm set-install-location 0");
    adb.adb_cmd("shell:settings put secure install_non_market_apps 1");
    adb.adb_cmd("shell:settings put system screen_off_timeout 1800000");

    // custom items
    initCustomItems();
    if (!needInstallDesktop) {
        // 去掉隐藏应用
        QList<BundleItem*> items = controller->appconfig->globalBundle->getAppList(installBundleItemIDs);
        foreach (BundleItem *item, items) {
            if (item->isHiddenItem) {
                DBG("not install desktop, so remove the hidden item, %s\n", qPrintable(item->id));
                installBundleItemIDs.removeOne(item->id);
            }
        }
    }

    // history job
    znode->clearJobHistory();
}

bool ZAdbDeviceJobThread::judgeBundle()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    int ostype = controller->appconfig->bundleType;
    if(ostype != 0 && !znode->brand.contains("Meizu",Qt::CaseInsensitive)){
        if(isYunOSDevice && ostype != 2){
            hint = tr("please select aliyun bundle");
            if (QDialog::Accepted == core_hint(DialogHint::TYPE_INFORMATION,
                                               znode->getName() +" "+ znode->ag_info.imei+ "\n" + hint,
                                               "")) {
                ADD_LOG_USETYPE(tr("please select aliyun bundle"), FormDeviceLog::LOG_WARNING);
                return false;
            }
        }
        if(!isYunOSDevice && ostype == 2){
            hint = tr("please select not aliyun bundle");
            if (QDialog::Accepted == core_hint(DialogHint::TYPE_INFORMATION,
                                               znode->getName() +" "+ znode->ag_info.imei+ "\n" + hint,
                                               "")) {
                ADD_LOG_USETYPE(tr("please select not aliyun bundle"), FormDeviceLog::LOG_WARNING);
                return false;
            }
        }
    }
    return true;
}

bool ZAdbDeviceJobThread::desktopisbool()
{
    QString _systype;
    QByteArray respData;
#if defined(Q_OS_WIN)
    _systype = "win";
#elif defined(Q_OS_LINUX)
    _systype = "linux";
#elif defined(Q_OS_OSX)
    _systype = "osx";
#endif
    Bundle *bundle = controller->appconfig->globalBundle->getBundle(installBundleIDs.first());
    QString Action = "getdesktop";
    QString Brand = znode->brand;
    QString Model = znode->model;
    QString Ispublic = bundle->ispublic;
    QString Tc = bundle->id;
    QString User = controller->appconfig->username;
    QString sys = _systype;
    if(!controller->appconfig->offlineMode){
        ZDMHttp::url_escape(Brand.toUtf8(),Brand);
        ZDMHttp::url_escape(Model.toUtf8(),Model);
        ZDMHttp::url_escape(User.toUtf8(),User);
        DBG("Model= %s,Brand= %s\n",Model.toUtf8().data(),Brand.toUtf8().data());
        DBG("ispublic= %s,Tc =%s\n",Ispublic.toUtf8().data(),Tc.toUtf8().data());
        if(Tc ==""){
            Tc = "228";
        }
        if(sys == ""){
            sys = "win";
        }
        if(Ispublic==""){
            Ispublic="1";
        }
        QString query = QString("action=%1&brand=%2&model=%3&customerid=%4&tcid=%5&sys=%6&ispublic=%7")
                .arg(Action, Brand, Model, User, Tc, sys, Ispublic);
        DBG("%s\n",query.toUtf8().data());
        QUrl url = QUrl(controller->appconfig->sUrl + "/GetDesktop.ashx");
        controller->loginManager->revokeUrl(url);
        url.setEncodedQuery(query.toLocal8Bit());
        QNetworkReply* reply = NULL;
        QNetworkAccessManager* manager = new QNetworkAccessManager();
        QNetworkRequest request;
        request.setUrl(url);
        reply = manager->get(request);
        QEventLoop eventLoop;
        connect(manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
        eventLoop.exec(); //block until finish
        respData = reply->readAll();
        QJson::Parser parser;
        QVariantMap map;
        map = parser.parse(respData).toMap();
        QString sCode = map.value("code").toString();
        if(sCode == "1000"){
            return true;
        }
        if(installBundleIDs.at(0) == "228" || installBundleIDs.at(0) == "206"){
            return true;
        }
    }
    return false;
}

void ZAdbDeviceJobThread::initCustomItems()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    qDeleteAll(customItems.begin(), customItems.end());
    customItems.clear();

    QStringList paths;
    if(controller->appconfig->bInstallGamehouse){
        paths << qApp->applicationDirPath() + "/GameHouse.apk";
    }else{
        ADD_LOG_USETYPE(tr("Get setting from server, not install GameHouse"), FormDeviceLog::LOG_WARNING);
    }

    // check install MZDesktop
    QSettings settings("mmqt2.ini", QSettings::IniFormat);
    if (settings.contains("CFG/installDesktop")) {
        needInstallDesktop = settings.value("CFG/installDesktop").toBool();
    } else {
        do {
            needInstallDesktop = false;

            // check server setting
            if (!controller->appconfig->MZDesktop) {
                ADD_LOG_USETYPE(tr("Get setting from server, not install SafetyDesktop"), FormDeviceLog::LOG_WARNING);
                break;
            }

            // brand adaptive
            QStringList desktopAdaptiveList = controller->appconfig->desktopAdaptiveList;
            bool bDesktopAdaptive = desktopAdaptiveList.isEmpty() || desktopAdaptiveList.contains(znode->brand, Qt::CaseInsensitive);
            if (!bDesktopAdaptive) {
                ADD_LOG_USETYPE(tr("Desktop adaptive list not contains the brand %1, not install SafetyDesktop").arg(znode->brand), FormDeviceLog::LOG_WARNING);
                break;
            }
            needInstallDesktop = true;
        } while (0);
    }

    if(isYunOSDevice && !znode->brand.contains("Meizu",Qt::CaseInsensitive)){
        needInstallDesktop = false;
    }
    if(!desktopisbool() && !controller->appconfig->offlineMode){
        needInstallDesktop = false;
        ADD_LOG_USETYPE(tr("not fit the desktopini, will not install SafetyDesktop"), FormDeviceLog::LOG_WARNING);
    }
    if (needInstallDesktop) {
        if(!controller->appconfig->CustomDesktop) {
            DBG("install SafetyDesktop.apk\n");
            paths << qApp->applicationDirPath() + "/SafetyDesktop.apk";
        }else{
            DBG("install HZSafetyDesktop.apk\n");
            paths << qApp->applicationDirPath() + "/HZSafetyDesktop.apk";
        }
    }

    foreach (const QString &path, paths) {
        DBG("add custom items:%s\n", qPrintable(path));
        BundleItem *item = new BundleItem;
        if (item->parseApk(path, true)) {
            item->id = QUuid::createUuid().toString();
            item->download_status = ZDownloadTask::STAT_FINISHED;
            customItems.append(item);
        } else {
            delete item;
        }
    }
}

bool ZAdbDeviceJobThread::startDXClick()
{
    // 点击市场类应用的窗口等
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QString cmd;
    QByteArray out;

    bool ret;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    //
    ret = adb.adb_push("autohelper", "data/local/tmp/autohelper", hint, 0777);
    if (!ret) {
        ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
    }

    ADD_LOG_USETYPE(tr("exec DXClick cmd"), FormDeviceLog::LOG_STEP);

    ret = adb.adb_push("dx.jar", "data/local/tmp/dx.jar", hint, 0777);
    if (!ret) {
        ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
    }
    cmd = "uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#MAIN -e applist justdisableapp";
    out = adb.adb_cmd("shell:" + cmd);
    DBG("out=<%s>", out.constData());

    return ret;
}

bool ZAdbDeviceJobThread::startUIClick(bool shellCmdNeedAddTaskForQueue)
{
    // Explain: 通用点击，辅助卸载、安装等时的弹窗点击
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QString cmd;
    QByteArray out;

    bool ret;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_USETYPE(tr("exec UIClick cmd"), FormDeviceLog::LOG_STEP);

    ret = adb.adb_push("uiclick.jar", "data/local/tmp/uiclick.jar", hint, 0777);
    if (!ret) {
        ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
    }

    // kill old uiautomator
    killProcess("uiautomator", shellCmdNeedAddTaskForQueue);

    // check async
    if (shellCmdNeedAddTaskForQueue && !cli.switchQueueCli()) {
        ADD_LOG_USETYPE(tr("async switch error"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    cmd = "uiautomator runtest uiclick.jar -c com.zx.uitest.run.TestAutoInstall";
    if (!useAsync) {
        ThreadBase<ZAdbDeviceJobThread> *t = new ThreadBase<ZAdbDeviceJobThread>(this, &ZAdbDeviceJobThread::core_UIAutomator);
        t->start(NULL, NULL, NULL);
    } else {
        if (!shellCmdNeedAddTaskForQueue) {
            cli.execShellCommandInThread(cmd);
        } else {
            cli.addShellCommandQueue("THREAD" + cmd);
        }
    }

    // exit async
    if (shellCmdNeedAddTaskForQueue && !cli.quitQueue()) {
        ADD_LOG_USETYPE(tr("async quit error"), FormDeviceLog::LOG_ERROR);
    }

    return ret;
}

bool ZAdbDeviceJobThread::initZMaster()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_USETYPE(tr("start ZMaster"), FormDeviceLog::LOG_STEP);
    bool startResult = FastManRooter::startZMaster(znode->adb_serial, true);
    if (!startResult) {
        ADD_LOG_USETYPE("Failure", FormDeviceLog::LOG_WARNING);
    }
    int times = 5;
    while (times-- > 0) {
        sleep(2);
        if (cli.getZMInfo(znode->zm_info)) {
            cli.setClientInfo(MMQT2_VER, bNeedZAgentInstallAndUninstall);
            return true;
        } else {
            FastManRooter::startZMaster(znode->adb_serial, true);
        }
    }

    return false;
}

bool ZAdbDeviceJobThread::initZAgent()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QByteArray out;
    bool ret = true;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_USETYPE(tr("start daemon"), FormDeviceLog::LOG_STEP);

    do {
        // install
        if (controller->appconfig->spInstall) {
            DBG("old driver to install ZAgent\n");
            adb.adb_cmd("shell:rm -rf /sdcard/noflash");
            adb.adb_cmd("shell:mkdir /sdcard/noflash");
            out = adb.pm_cmd("path com.dx.agent2", 10000);
            if (!out.startsWith("package:")) {
                if (cli.push(qApp->applicationDirPath() + "/ZAgent2.apk", "/sdcard/noflash/ZAgent2.apk", hint)) {
                    killProcess("uiautomator");
                    QList<BundleItem *> items;
                    BundleItem *item = new BundleItem;
                    item->pkg = "com.dx.agent2";
                    item->name = "GoogleUSB";
                    items.append(item);
                    BreakHeart *t = new BreakHeart(this, node, items);
                    t->start();
                    adb.adb_cmd("shell:uiautomator runtest /data/local/tmp/uiclick.jar   -c com.zx.uitest.run.TestAutoInstall#DONEWOPPO");
                    out = adb.pm_cmd("path com.dx.agent2", 10000);
                    if (out.startsWith("package:")) {
                        ret = true;
                    } else {
                        DBG("install ZAgent.apk failed\n");
                        ret = false;
                    }
                } else {
                    DBG("push ZAgent.apk to /sdcard/ failed\n");
                    ret = false;
                }
            }
        } else {
            out = adb.pm_cmd("path com.dx.agent2", 10000);
            if (out.startsWith("package:")) {  // NOTE: "open: Permission denied"
                // ZAgent has exsit
                quint64 agentSize = 0;
                QString loacalMD5;
                QString remoteMD5;
                GlobalController::getFileMd5("ZAgent2.apk", loacalMD5, agentSize);
                while (out.endsWith('\r') || out.endsWith('\n')) {
                    out.chop(1);
                }
                cli.getFileMd5(out.mid(8), remoteMD5);
                ret = (remoteMD5.toUpper() == loacalMD5.toUpper());
                if (!ret) {
                    adb.pm_cmd("uninstall com.dx.agent2");
                    ret = adb.adb_install("ZAgent2.apk", "-r", hint);
                }
            } else {
                ret = adb.adb_install("ZAgent2.apk", "-r", hint);
            }
            if (!ret) {
#ifdef _WIN32
                QString cmd = QString("\"" + qApp->applicationDirPath() + "/zxlycon.exe\" -s %1 install -r \"%2\"").arg(node->adb_serial).arg("ZAgent2.apk");
#else
                QString cmd = QString("\"" + qApp->applicationDirPath() + "/zxlycon\" -s %1 install -r \"%2\"").arg(node->adb_serial).arg("ZAgent2.apk");
#endif
                DBG("install ZAgent2 by zxlycon, cmd=<%s>\n", qPrintable(cmd));
                QProcess ps;
                ps.execute(cmd);
                ps.waitForFinished(10000);
                out = adb.pm_cmd("path com.dx.agent2", 10000);
                ret = out.startsWith("package:");
            }
            if(!ret){
                DBG("bNeedZAgentInstallAndUninstall, install ZAgent by dx.jar\n");
                ADD_LOG_USETYPE(tr("install ZAgent by dx.jar"), FormDeviceLog::LOG_NORMAL);
                if (!out.startsWith("package:")) {
                    if (cli.push(qApp->applicationDirPath() + "/ZAgent2.apk", "/data/local/tmp/ZAgent2.apk", hint)) {
                        out = adb.adb_cmd("shell:am start -a android.intent.action.INSTALL_PACKAGE -d file:///data/local/tmp/ZAgent2.apk");
                        // check install
                        int timeout = 30;
                        while (timeout--) {
                            if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
                                ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
                                return false;
                            }

                            out = adb.pm_cmd("path com.dx.agent2", 10000);
                            DBG("path result: %s\n", out.data());
                            if (ret = out.startsWith("package:")) {
                                break;
                            }
                            sleep(1);
                        }
                    } else {
                        DBG("push ZAgent.apk to /sdcard/ failed\n");
                        ret = false;
                    }
                }
            }
        }
        if (!ret) {
            ADD_LOG_USETYPE(tr("install ZAgent failed"), FormDeviceLog::LOG_ERROR);
            break;
        }

        if (controller->appconfig->spInstall) {
            startUIClick();
        }
        // kill
        out = adb.adb_cmd("shell:am force-stop com.dx.agent2");

        // start
        out = adb.adb_cmd("shell:am start -n com.dx.agent2/.LoadActivity");
        DBG("start agent2, out=<%s>\n", out.constData());

        int tryCnts;
        for (tryCnts = 0; tryCnts < 4; ++tryCnts) {
            sleep(2);
            int needRing = ((controller->appconfig->alertType & ZMSG2_ALERT_SOUND) != 0);
            int needVibrate = ((controller->appconfig->alertType & ZMSG2_ALERT_VIBRATE) != 0);
            int rate = controller->appconfig->installFailureMaxRate;
            int allowZAgentUploadInstallResultWhenAsync;
            if(controller->appconfig->offlineMode || controller->appconfig->bAllowPCUploadInstallResultWhenAsync){
                allowZAgentUploadInstallResultWhenAsync = 0;
            }else{
                allowZAgentUploadInstallResultWhenAsync = 1;
            }
            QString LANUrlWithPort = controller->appconfig->useLAN ? QString("%1:%2").arg(controller->appconfig->LANUrl).arg(controller->appconfig->LANPort) : "";
            DBG("reset status args, needRing=%d, needVibrate=%d, allowZAgentUploadInstallResultWhenAsync=%d, failureRate=%d, LANUrlWithPort=%s\n",
                needRing, needVibrate, allowZAgentUploadInstallResultWhenAsync, rate, qPrintable(LANUrlWithPort));
            if (agentCli.resetStatus("MZ",
                                     needRing,
                                     needVibrate,
                                     failureRate,
                                     allowZAgentUploadInstallResultWhenAsync,
                                     LANUrlWithPort)) {  // 用于确定与精灵通信正常
                break;
            } else {
                // NOTE: ZM will be killed while resetStatus, why?
                DBG("resetStatus failed times=<%d>, restart ZM and startUIClick\n", tryCnts);
                killProcess("uiautomator");
                FastManRooter::startZMaster(znode->adb_serial, true);
                startUIClick();
            }
        }
        if (4 == tryCnts) {
            hint = "resetStatus failed";
            ret = false;
        } else {
            ret = true;
        }
    } while (0);

    agentCli.addLog(tr("bundleNames:%1, install step:%2, %3, install upload path:%4, log file name:%5")
                    .arg(installBundleNames.join(","))
                    .arg(useAsync ? tr("use async") : tr("use sync"))
                    .arg(uninstallUserApp ? tr("uninstall user app") : tr("no uninstall user app"))
                    .arg(controller->appconfig->useLAN ? tr("use LAN") : tr("use WAN"))
                    .arg(znode->logFileName));

    return ret;
}

bool ZAdbDeviceJobThread::checkDeviceInfo()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_USETYPE(tr("check device info"), FormDeviceLog::LOG_STEP);

    setScreenTimeout();

    // check storeage
    if (znode->zm_info.store_free < 512 * 1024 * 1024) {
        cli.remove(znode->zm_info.store_dir, false);
    }
    emit sigDeviceRefresh(node);

    // check IMEI
    agentCli.setConnType(znode->node_index, ZMSG2_CONNTYPE_USB);
    agentCli.getBasicInfo(znode->ag_info);
    emit sigDeviceLog(node, tr("IMEI:[%1]").arg(znode->ag_info.imei), FormDeviceLog::LOG_NORMAL);
    if (znode->ag_info.imei.isEmpty()) {
        ADD_LOG_USETYPE(tr("Failure: check IMEI is empty"), FormDeviceLog::LOG_ERROR);
        core_hint(DialogHint::TYPE_INFORMATION, tr("check IMEI is empty"), "");
        return false;
    }

    // check time
    qint64 timeGap;
    bool ret = (agentCli.getTimeGap(timeGap) && abs(timeGap) > 120 * 1000);
    if (!ret) {
        emit sigDeviceLog(node, tr("device time not accurate, please adjust it later"), FormDeviceLog::LOG_NORMAL);
    }

    return true;
}

bool ZAdbDeviceJobThread::checkInstallLog()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_USETYPE(tr("check install log"), FormDeviceLog::LOG_STEP);

    setScreenTimeout();

    DevInstLog instLog;
    if (controller->appconfig->offlineMode) {
        // offline install
        ADD_LOG_USETYPE(tr("offline mode, continue without check install log"), FormDeviceLog::LOG_WARNING);
    } else {
        // online install
        if (controller->loginManager->core_getInstLog(instLog, znode->ag_info.imei)) {
            if (instLog.pkgList.size() > 0) {
                if (instLog.user.isEmpty()) {
                    hint = tr("this phone has been installed, continue to install will be unpaid, continue to install?");

                    emit sigDeviceLog(node, hint, FormDeviceLog::LOG_UNPID);
                } else if (instLog.user != controller->appconfig->username) {
                    hint = tr("found install log by others, you will be unpaid, continue to install?");
                    emit sigDeviceLog(node, hint, FormDeviceLog::LOG_NORMAL);
                } else {
                    ADD_LOG_USETYPE(tr("found install log by yourself, continue"), FormDeviceLog::LOG_WARNING);
                    hint.clear();
                }

                if (!hint.isEmpty()) {
                    if (QDialog::Accepted != core_hint(DialogHint::TYPE_YESNO,
                                                       znode->getName() +" "+ znode->ag_info.imei+ "\n" + hint,
                                                       "")) {
                        ADD_LOG_USETYPE(tr("abort by user"), FormDeviceLog::LOG_ERROR);
                        return false;
                    } else {
                        if (node->connect_stat != AdbDeviceNode::CS_DEVICE) {
                            ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
                            return false;
                        }
                    }
                    ADD_LOG(tr("found install log, continue anyway"));
                }
            }
        } else {
            ADD_LOG_USETYPE(tr("get install log failed, continue anyway"), FormDeviceLog::LOG_WARNING);
        }
    }

    return true;
}

void ZAdbDeviceJobThread::uninstallApk()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QByteArray out;
    QString hint;
    bool result = false;
    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return;
    }

    ADD_LOG_USETYPE(tr("wait for uninstall..."), FormDeviceLog::LOG_STEP);

    setScreenTimeout();

    sigDeviceStat(node, tr("uninstalling user app"));
    agentCli.setHint(tr("uninstalling user app"));

    QStringList blacklist;
    blacklist.append(controller->appconfig->uninstallBlacklist + getUninstallBlacklist());

#if 0  // NOTE: 开线程去清理
    bRootAndCleanFinished = false;
    emit signal_startRootAndCleanThread();
    while (!bRootAndCleanFinished) {
        xsleep(1000);
    }
#else  // 函数处理

    bool hasRoot = adb.adb_cmd("shell:su -c id", 15000).contains("uid=0");

    // get app list
    QList<AGENT_APP_INFO*> appList;
    agentCli.getAppList(appList, false);

    // uninstall
    QList<AGENT_APP_INFO*> needUninstallAppList;
    int totalItems = appList.size();
    int needUninstallTotalItems = 0;
    int failedItems = 0;
    for (int i = 0; i < appList.size(); ++i) {
        if (node->connect_stat != AdbDeviceNode::CS_DEVICE) {
            ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
            break;
        }

        emit sigDeviceProgress(node, i, totalItems);
        agentCli.setProgress(-1, i, totalItems);

        AGENT_APP_INFO *app = appList[i];
        if (app->packageName == "com.dx.agent2") {
            continue;
        }

        bool needUninstall = false;
        int appType = app->getType();
        if (!hasRoot) {
            // 没有Root时除了以下路径均尝试卸载
            if (!app->sourceDir.startsWith("/system/app/") && !app->sourceDir.startsWith("/system/priv-app/")
                    && !app->sourceDir.startsWith("/system/vendor/operator/app")) {
                needUninstall = true;
            }
            if (appType == AGENT_APP_INFO::TYPE_USER_APP) {
                needUninstall = true;
            }
        } else {
            // 已经Root时只尝试卸载以下类型app
            if (appType == AGENT_APP_INFO::TYPE_UPDATED_SYSTEM_APP || appType == AGENT_APP_INFO::TYPE_USER_APP) {
                needUninstall = true;
            }
        }

        if (needUninstall && !blacklist.contains(app->name)) {
            needUninstallAppList.append(app);
            needUninstallTotalItems++;
            emit sigDeviceLog(node, tr("uninstalling [%1]").arg(app->name), FormDeviceLog::LOG_NORMAL);
            DBG("unisntall, pkg=%s\n", qPrintable(app->packageName));
            if(!bNeedZAgentInstallAndUninstall){
                out = adb.pm_cmd("uninstall " + app->packageName);
                DBG("pm uninstall out:%s\n",out.data());
                if (out.trimmed() != "Success") {
                    ++failedItems;
                    ADD_LOG_USETYPE(QString("%1 %2").arg(QString(out.trimmed())).arg(app->sourceDir), FormDeviceLog::LOG_WARNING);
                    ADD_LOG_USETYPE(tr("unisntall by ZAgent:[%1]").arg(app->packageName),FormDeviceLog::LOG_NORMAL);
                    result = agentCli.uninstallApp(app->packageName, hint);
                    if(result){
                        failedItems--;
                    }
                    xsleep(5000);
                }
            }else{
                ADD_LOG_USETYPE(tr("unisntall by ZAgent:[%1]").arg(app->packageName),FormDeviceLog::LOG_NORMAL);
                result = agentCli.uninstallApp(app->packageName, hint);
                if(!result){
                    ++failedItems;
                }
                xsleep(5000);
            }
        }
    }
    // check uninstall result finaly
    if (bNeedZAgentInstallAndUninstall) {
        failedItems = 0;
        foreach (AGENT_APP_INFO *item, needUninstallAppList) {
            out = adb.pm_cmd(QString("path %1").arg(item->packageName), 10000);
            DBG("path result: %s\n", out.data());
            if (out.startsWith("package:")) {
                ADD_LOG_USETYPE(tr("uninstall failed: %1").arg(item->name), FormDeviceLog::LOG_WARNING);
                failedItems++;
            }
        }  // if
    }

    qDeleteAll(appList.begin(), appList.end());
    appList.clear();

    emit sigDeviceProgress(node, totalItems, totalItems);
    agentCli.setProgress(-1, totalItems, totalItems);
    agentCli.setHint(tr("uninstall user app finished"));

    if (failedItems > 0) {
        ADD_LOG_USETYPE(tr("uninstall total:%1, success:%2, failure:%3").arg(needUninstallTotalItems).arg(needUninstallTotalItems - failedItems).arg(failedItems),
                        FormDeviceLog::LOG_WARNING);
    } else {
        ADD_LOG(tr("uninstall total:%1, success:%2, failure:%3").arg(needUninstallTotalItems).arg(needUninstallTotalItems - failedItems).arg(failedItems));
    }
#endif
}

bool ZAdbDeviceJobThread::checkYunOSDrag()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QByteArray out;

    if (!controller->appconfig->bNeedYunOSDrag || !isYunOSDevice) {
        return true;
    }

    emit sigDeviceLog(node, tr("start YunOS drag"));

    killProcess("uiautomator");
    do {
        bool ret = adb.adb_push("YunOSDrag.jar", "data/local/tmp/YunOSDrag.jar", hint, 0777);
        if (!ret) {
            DBG("Push YunOSDrag failed, ret=<%s>\n", qPrintable(hint));
            return false;
        }

        out = adb.adb_cmd("shell:uiautomator runtest /data/local/tmp/YunOSDrag.jar -c com.zx.uitest.interpeter.RUN#DargAliAppIcon");
        DBG("Exec YunOSDrag out=<%s>\n", out.data());
    } while (0);
    startUIClick();

    return true;
}

bool ZAdbDeviceJobThread::checkJobs(bool waitForNullJobs)
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    if (znode->ag_info.imei != znode->lastImei) {
        znode->clearJobsAndHistory();
        znode->lastImei = znode->ag_info.imei;
    }

    setScreenTimeout();

    // get jobs
    znode->addJobs(installBundleItemIDs);

    while (waitForNullJobs && znode->getJobsCount() < 1) {  // wait for add jobs: choose the bundle from UI and so on
        if (node->connect_stat != AdbDeviceNode::CS_DEVICE) {
            DBG("device offline, stop install");
            return false;
        }
        xsleep(500);
    };

    QStringList forceJobs = controller->appconfig->globalBundle->getForceItemIds();
    if (!forceJobs.isEmpty()) {
        znode->addJobs(forceJobs);
        forceJobs.clear();
    }

    return true;
}

bool ZAdbDeviceJobThread::pushApk()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    QTime time;
    time.start();
    znode->sBundleID = installBundleIDs.first();
    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_AND_HINT_USETYPE(tr("start push"), FormDeviceLog::LOG_STEP);

    setScreenTimeout();

    // WiFi config
    adb.adb_cmd("shell:rm -f /data/local/tmp/WiFiConfiguration.ini");
    bool ret = cli.push(qApp->applicationDirPath() + "/WiFiConfiguration.ini", "/data/local/tmp/WiFiConfiguration.ini", hint);
    DBG("push WiFiConfiguration.ini ret=<%d>, hint=<%s>\n", ret, qPrintable(hint));
    if (!ret) {
        ADD_LOG_USETYPE(tr("push WiFiConfiguration.ini failed: %1").arg(hint), FormDeviceLog::LOG_WARNING);
    }

    // push
    agentCli.setHint(tr("pushing, pelase check the progress"));
    QList<BundleItem *> items;
    BundleItem *typewriting = controller->appconfig->globalBundle->getAppList().first();
    unshowList.append(unshowList + "," + typewriting->pkg);
    items.append(customItems);
    QStringList jobs = znode->getNextJobs();
    items.append(controller->appconfig->globalBundle->getBundle(installBundleIDs.first())->getAppList());
    short indexItem = 0;
    short totalItems = items.size();
    short failedItems = 0;
    if(controller->appconfig->MZDesktop){
        totalItems--;
    }

    copyTotal = totalItems;
    copySuccessCounts = 0;
    foreach (BundleItem *item, items) {
        DBG("item platform:%s\n",qPrintable(item->platform));
        if (item->isHiddenItem ) {
            if (!item->platform.contains(controller->appconfig->platform)) {
                totalItems--;
                continue;
            }
        }
        if (!needInstallDesktop) {
            if (item->isHiddenItem == true) {
                totalItems--;
                continue;
            }
        }
        if (node->connect_stat != AdbDeviceNode::CS_DEVICE) {
            ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
            return false;
        }
        if(item->isHiddenItem || unshowList.contains(item->pkg)){
            emit sigDeviceLog(node, tr("pushing [%1]").arg(item->name), FormDeviceLog::LOG_NORMAL, false);
            totalItems--;
        }else{
            emit sigDeviceLog(node, tr("pushing [%1]").arg(item->name), FormDeviceLog::LOG_NORMAL, true);
        }
        emit sigDeviceProgress(node, indexItem, totalItems);
        agentCli.setProgress(-1, indexItem, totalItems);
        QString remotePath;
        if(controller->appconfig->spInstall) {
            remotePath  = "/sdcard/noflash/" + calcMd5(item->pkg.toLocal8Bit()) + "_test.apk";
        } else {
            remotePath  = znode->zm_info.store_dir + "/" + calcMd5(item->pkg.toLocal8Bit()) + "/test.apk";
        }
        QString hint;
        bool ret = cli.push(item->path, remotePath, hint);
        if (!ret) {
            ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
            if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                failedItems++;
            }
        } else {
            copySuccessCounts++;
        }
        ++indexItem;

        agentCli.addInstLog(item->name, item->id, item->md5, item->pkg);
    }

    // refresh device info
    cli.getZMInfo(znode->zm_info);
    emit sigDeviceRefresh(node);

    emit sigDeviceProgress(node, totalItems, totalItems);
    agentCli.setProgress(-1, totalItems, totalItems);
    agentCli.setHint(tr("push finished"));

    if (failedItems > 0) {
        ADD_LOG_USETYPE(tr("push total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - failedItems).arg(failedItems),
                        FormDeviceLog::LOG_WARNING);
    } else {
        ADD_LOG(tr("push total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - failedItems).arg(failedItems));
    }
    ADD_LOG(tr("this time elapsed:%1s").arg(time.elapsed() / 1000.0));

    if (failedItems > 0) {
        agentCli.setAlert(ZMSG2_ALERT_ASYNC_FAIL | controller->appconfig->alertType);
    } else {
        agentCli.setAlert(ZMSG2_ALERT_ASYNC_DONE);
    }

    return true;
}

bool ZAdbDeviceJobThread::checkNeedReplaceDesktopTheme()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QByteArray out;

    setScreenTimeout();

    bool needReplaceTheme = false;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    QString theme;
    do {
        // check allow
        if (!controller->appconfig->bAllowYunOSReplaceIcon) {
            DBG("check server not allow\n");
            break;
        }

        // check Agent2
        if (!agentCli.checkYunOSDesktopReplaceIcon()) {
            DBG("check Agent2 not allow\n");
            break;
        }

        // check prop
        out = adb.adb_cmd("shell:getprop ro.yunos.model");
        DBG("check yunos model, out=<%s>\n", out.data());
        QString model = out.trimmed();
        if (model.isEmpty()) {
            DBG("check prop, model is empty\n");
            break;
        }

        // check themes
        QMap<QString, QVariant>::const_iterator i = controller->appconfig->themesMap.constBegin();
        while (i != controller->appconfig->themesMap.constEnd()) {
            QStringList models = i.value().toStringList();
            if (models.contains(model)) {
                theme = i.key();
                break;
            }
            ++i;
        }
        if (theme.isEmpty()) {
            DBG("check themes not contains\n");
            theme = "default";
        }

        // check themes code
        if (controller->appconfig->themesCodeStr.isEmpty()) {
            DBG("check themes code empty\n");
            break;
        }

        QString localApk = QString("%1/ThemeApks/%2.apk").arg(qApp->applicationDirPath()).arg(theme);
        bool ret = adb.adb_install(localApk, "-r", hint);
        if (!ret) {
            DBG("install theme apk failed, hint=%s\n", qPrintable(hint));
        }
        needReplaceTheme = true;

    }while(0);
    return (this->needReplaceTheme = needReplaceTheme);
}

bool ZAdbDeviceJobThread::specialInstall()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QString pkgs;
    QStringList pkglist;
    QByteArray out;
    bool flag;

    QTime time;
    time.start();
    QList<BundleItem *> items;
    items.append(customItems);
    QStringList jobs = znode->getNextJobs();
    items.append(controller->appconfig->globalBundle->getBundle(installBundleIDs.first())->getAppList());

    int totalItems = items.size();
    int installFailedCounts = 0;
    if(controller->appconfig->MZDesktop){
        totalItems--;
    }

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_AND_HINT_USETYPE(tr("start special install"), FormDeviceLog::LOG_STEP);
    setScreenTimeout();

    //advanced upload
    if ((controller->appconfig->bAllowPCUploadInstallResultWhenAsync && useAsync) && !uploadInstallLog()) {
        return false;
    }

    // check async
    if (useAsync && !cli.switchQueueCli()) {
        bool flag = false;
        ADD_LOG_USETYPE(tr("async switch error"), FormDeviceLog::LOG_ERROR);
        for(int i=0; i<3; i++){
            msleep(1000);
            if(cli.switchQueueCli()){
                flag = true;
                break;
            }
        }
        if(!flag){
            return false;
        }
    }

    if (useAsync) {
        foreach (BundleItem *item, items) {
            pkglist.append(item->pkg);
        }
        pkgs = pkglist.join(",");
    }

    killProcess("uiautomator");
    if (useAsync) {
        cli.oldDriverMode(pkgs, hint);
        foreach (BundleItem *item, items) {
            if (item->isHiddenItem) {
                if (!item->platform.contains(controller->appconfig->platform)) {
                    totalItems--;
                    continue;
                }
            }
            if(item->isHiddenItem || unshowList.contains(item->pkg)) {
                totalItems--;
                continue;
            }
        }
        QString cmd = "uiautomator runtest /data/local/tmp/uiclick.jar -c com.zx.uitest.run.TestAutoInstall#DONEWOPPO";
        cli.addShellCommandQueue(cmd);
    } else {
        BreakHeart *t = new BreakHeart(this, node, items);
        t->start();
        //install
        out = adb.adb_cmd("shell:uiautomator runtest /data/local/tmp/uiclick.jar -c com.zx.uitest.run.TestAutoInstall#DONEWOPPO");
        foreach (BundleItem *item, items) {
            if (item->isHiddenItem) {
                if (item->platform != controller->appconfig->platform) {
                    totalItems--;
                    continue;
                }
            }
            if(item->isHiddenItem || unshowList.contains(item->pkg)) {
                totalItems--;
                continue;
            }
            out = adb.pm_cmd("path " + item->pkg, 10000);
            if(!out.startsWith("package:")) {
                ADD_LOG_USETYPE(tr("install failure:[%1]").arg(item->name), FormDeviceLog::LOG_WARNING);
                DBG("install failure:%s\n", qPrintable(item->pkg));
                installFailedCounts++;
            }
        }
    }

    emit sigDeviceProgress(node, totalItems, totalItems);
    agentCli.setProgress(totalItems, -1, totalItems);

    if (!useAsync) {
        agentCli.setHint(tr("install finished"));
    } else {
        agentCli.setHint(tr("async install finished"));
    }

    DBG("install result: succeed=%d, failed=%d\n", totalItems - installFailedCounts, installFailedCounts);

    // exit async
    if (useAsync && !cli.quitQueue()) {
        ADD_LOG_USETYPE(tr("async quit error"), FormDeviceLog::LOG_ERROR);
    }

    if (!useAsync) {
        if (installFailedCounts > 0) {
            ADD_LOG_USETYPE(tr("install total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - installFailedCounts).arg(installFailedCounts),
                            FormDeviceLog::LOG_WARNING);
        } else {
            ADD_LOG(tr("install total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - installFailedCounts).arg(installFailedCounts));

        }
        agentCli.setAlert((installFailedCounts == 0) ? ZMSG2_ALERT_INSTALL_DONE : ZMSG2_ALERT_INSTALL_FAIL);  // if useAsync, alert by ZMaster
    } else {
        ADD_LOG(tr("async install total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - installFailedCounts).arg(installFailedCounts));
    }
    ADD_LOG(tr("this time elapsed:%1s").arg(time.elapsed() / 1000.0));

    return true;
}

bool ZAdbDeviceJobThread::installApk()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QByteArray out;
    bool flag;

    QTime time;
    time.start();

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    if (!useAsync) {
        ADD_LOG_AND_HINT_USETYPE(tr("start install"), FormDeviceLog::LOG_STEP);
    } else {
        ADD_LOG_AND_HINT_USETYPE(tr("start async install"), FormDeviceLog::LOG_STEP);
    }

    setScreenTimeout();

    //handel typewriting
    BundleItem *typewriting = controller->appconfig->globalBundle->getAppList().first();
    emit sigDeviceLog(node, tr("installing [%1]").arg(typewriting->name), FormDeviceLog::LOG_NORMAL, false);
    QString remotePath = znode->zm_info.store_dir + "/" + calcMd5(typewriting->pkg.toLocal8Bit()) + "/test.apk";
    DBG("remotePath typewriting pkg:%s,path:%s",qPrintable(typewriting->pkg),qPrintable(remotePath));
    QString id = typewriting->id;
    bool ret;
#ifdef DOWNLOAD_DIR_USE_USERNAME
    id.remove(controller->appconfig->username);
#endif
    QStringList list = id.split("_");
    id = list.at(0);

    if(bNeedZAgentInstallAndUninstall) {
        ret = installByZAgent(remotePath, typewriting->pkg);
    }else{
        out = adb.adb_cmd("shell:pm install -r " + remotePath);
        DBG("install typewriting:%s",out.data());
        if(out.trimmed().contains("Success")){
            ret = true;
        }else{
            ret = false;
        }
    }
    if(ret){
        flag = true;
    }else{
        flag = false;
    }
    //
    if ((controller->appconfig->bAllowPCUploadInstallResultWhenAsync && useAsync) && !uploadInstallLog()) {
        return false;
    }

    // check async
    if (useAsync && !cli.switchQueueCli()) {
        bool flag = false;
        ADD_LOG_USETYPE(tr("async switch error"), FormDeviceLog::LOG_ERROR);
        for(int i=0; i<3; i++){
            msleep(1000);
            if(cli.switchQueueCli()){
                flag = true;
                break;
            }
        }
        if(!flag){
            return false;
        }
    }

    // install
    if (!useAsync) {
        agentCli.setHint(tr("installing"));
    } else {
        agentCli.setHint(tr("async installing"));
    }
    QList<BundleItem *> items;
    items.append(customItems);
    QStringList jobs = znode->getNextJobs();
    items.append(controller->appconfig->globalBundle->getBundle(installBundleIDs.first())->getAppList());
    QList<AGENT_APP_INFO *> appList;
    agentCli.getAppList(appList, false);

    int indexItem = 0;
    int totalItems = items.size();
    int installFailedCounts = 0;
    bool isshow;
    if(controller->appconfig->MZDesktop){
        totalItems--;
    }

    installTotal = totalItems;
    installSuccessCounts = 0;
    installFailedPkgs.clear();
    installFailedDowngradeIDs.clear();
    foreach (BundleItem *item, items) {
        if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
            ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
            return false;
        }
        if (item->isHiddenItem) {
            if (!item->platform.contains(controller->appconfig->platform)) {
                totalItems--;
                continue;
            }
        }
        if(!needInstallDesktop){
            if(item->isHiddenItem == true){
                totalItems--;
                continue;
            }
        }
        if(item->isHiddenItem || unshowList.contains(item->pkg)){
            isshow = false;
            totalItems--;
        }else{
            isshow = true;
        }

        if (!useAsync) {
            emit sigDeviceLog(node, tr("installing [%1]").arg(item->name), FormDeviceLog::LOG_NORMAL, isshow);
        } else {
            emit sigDeviceLog(node, tr("async installing [%1]").arg(item->name), FormDeviceLog::LOG_NORMAL, isshow);
        }
        emit sigDeviceProgress(node, indexItem, totalItems);
        agentCli.setProgress(indexItem, -1, totalItems);

        // check need install by MD5
        bool needsInstall = true;
        QString remoteMD5;
        foreach (AGENT_APP_INFO *app, appList) {
            if (app->packageName == "com.iflytek.inputmethod") {
                continue;
            }
            if (app->packageName == item->pkg) {
                if(app->packageName == "com.iflytek.inputmethod"){
                    DBG("sourcedir:%s",qPrintable(app->sourceDir));
                }
                bool flag = cli.getFileMd5(app->sourceDir, remoteMD5);
                DBG("remoteMD5:%s,itemMD5:%s", qPrintable(remoteMD5.toUpper()),qPrintable(item->md5.toUpper()));
                if (remoteMD5.toUpper() == item->md5.toUpper()) {
                    if (!useAsync) {  // 异步的时候仍然需要安装，方便ZM进行数据统计
                        needsInstall = false;
                        QString remotePath = znode->zm_info.store_dir + "/" + calcMd5(item->pkg.toLocal8Bit()) + "/test.apk";
                        cli.remove(remotePath);
                    }
                } else if (app->getType() != AGENT_APP_INFO::TYPE_SYSTEM_APP) {
                    if(!app->sourceDir.startsWith("/system/")){
                        if(bNeedZAgentInstallAndUninstall) {
                            agentCli.uninstallApp(app->packageName, hint);
                        }else{
                            adb.pm_cmd("uninstall "+ app->packageName);
                        }
                        DBG("uninstall %s", qPrintable(app->packageName));
                    }
                }
                break;
            }
        }

        // install
        bool result;
        if (needsInstall) {
            bool isLocalApk = (customItems.contains(item) || item->isHiddenItem);
            QString remotePath = znode->zm_info.store_dir + "/" + calcMd5(item->pkg.toLocal8Bit()) + "/test.apk";
            QString id = item->id;
#ifdef DOWNLOAD_DIR_USE_USERNAME
            id.remove(controller->appconfig->username);
#endif
            QStringList list = id.split("_");
            id = list.at(0);

            QString hint;
            if (bNeedZAgentInstallAndUninstall && !useAsync) {
                result = installByZAgent(remotePath, item->pkg);
            } else {
                if(flag && item->pkg == "com.iflytek.inputmethod"){
                    result = cli.installApk(remotePath,
                                            item->pkg,
                                            item->name,
                                            hint,
                                            FastManClient::LOCATION_AUTO,
                                            item->instLocation == BundleItem::INST_LOCATION_SYSTEM,
                                            false,
                                            item->isHiddenItem,
                                            id);
                }else{
                    result = cli.installApk(remotePath,
                                            item->pkg,
                                            item->name,
                                            hint,
                                            FastManClient::LOCATION_AUTO,
                                            item->instLocation == BundleItem::INST_LOCATION_SYSTEM,
                                            isLocalApk,
                                            item->isHiddenItem,
                                            id);
                }
            }
            if (!result && !useAsync) {  // TODO: async how to check result
                // TODO: for OPPO
                if (znode->brand == "OPPO") {
                    sleep(10);
                }

                ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
                if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                    installFailedCounts++;
                }
            }
            if (result) {
                if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                    installSuccessCounts++;
                }
            }
        } else {
            result = true;
            if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                installSuccessCounts++;
            }
            ADD_LOG(tr("%1 no need install(the same MD5)").arg(item->name));
        }
        ++indexItem;

        // check result
        if (!useAsync && !result && !customItems.contains(item)) {  // for install log
            if (installFailedPkgs.isEmpty()){
                installFailedPkgs = item->pkg;
            } else {
                installFailedPkgs = installFailedPkgs + "," + item->pkg;
            }

            if (hint.contains("INSTALL_FAILED_VERSION_DOWNGRADE")) {  // for lower version
                installFailedDowngradeIDs.append(item->id);
            }
        }
    }
    emit sigDeviceProgress(node, totalItems, totalItems);
    agentCli.setProgress(totalItems, -1, totalItems);

    if (!useAsync) {
        agentCli.setHint(tr("install finished"));
    } else {
        agentCli.setHint(tr("async install finished"));
    }

    // check install result finaly
    if (!useAsync && bNeedZAgentInstallAndUninstall) {
        installSuccessCounts = 0;
        installFailedCounts= 0;
        installFailedPkgs.clear();
        foreach (BundleItem *item, items) {
            if(!needInstallDesktop && item->isHiddenItem){
                continue;
            }
            if (!customItems.contains(item)) {
                out = adb.pm_cmd(QString("path %1").arg(item->pkg), 10000);
                DBG("path result: %s\n", out.data());
                if (!out.startsWith("package:")) {
                    ADD_LOG_USETYPE(tr("install failed: %1").arg(item->name), FormDeviceLog::LOG_WARNING);
                    if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                        installFailedCounts++;
                    }
                    if (installFailedPkgs.isEmpty()){
                        installFailedPkgs = item->pkg;
                    } else {
                        installFailedPkgs = installFailedPkgs + "," + item->pkg;
                    }
                } else {
                    if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                        installSuccessCounts++;
                    }
                }
            }  // if
        }
    }
    DBG("install result: succeed=%d, failed=%d\n", totalItems - installFailedCounts, installFailedCounts);

    // TODO: for OPPO
    if (!useAsync && znode->brand == "OPPO") {
        sleep(30);

        installFailedCounts = 0;
        installSuccessCounts = 0;
        installFailedPkgs.clear();
        foreach (BundleItem *item, items) {
            if(item->pkg == typewriting->pkg){
                continue;
            }
            if(!needInstallDesktop){
                if(item->isHiddenItem == true){
                    continue;
                }
            }
            if (!customItems.contains(item)) {
                out = adb.pm_cmd(QString("path %1").arg(item->pkg), 10000);
                DBG("~~~~%s\n", out.data());
                if (!out.startsWith("package:")) {
                    if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                        installFailedCounts--;
                    }
                    if (installFailedPkgs.isEmpty()){
                        installFailedPkgs = item->pkg;
                    } else {
                        installFailedPkgs = installFailedPkgs + "," + item->pkg;
                    }
                } else {
                    if(!item->isHiddenItem && !unshowList.contains(item->pkg)){
                        installSuccessCounts++;
                    }
                }
            }  // if
        }
    }

    // exit async
    if (useAsync && !cli.quitQueue()) {
        ADD_LOG_USETYPE(tr("async quit error"), FormDeviceLog::LOG_ERROR);
    }

    while (!appList.isEmpty()) {
        delete appList.takeFirst();
    }

    installTotal = totalItems;
    copyTotal = totalItems;
    installSuccessCounts = totalItems - installFailedCounts;
    copySuccessCounts = installSuccessCounts;

    // set alert
    if (!useAsync) {
        if (installFailedCounts > 0) {
            ADD_LOG_USETYPE(tr("install total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - installFailedCounts).arg(installFailedCounts),
                            FormDeviceLog::LOG_WARNING);
        } else {
            ADD_LOG(tr("install total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - installFailedCounts).arg(installFailedCounts));

        }
        agentCli.setAlert((installFailedCounts == 0) ? ZMSG2_ALERT_INSTALL_DONE : ZMSG2_ALERT_INSTALL_FAIL);  // if useAsync, alert by ZMaster
    } else {
        ADD_LOG(tr("async install total:%1, success:%2, failure:%3").arg(totalItems).arg(totalItems - installFailedCounts).arg(installFailedCounts));
    }
    ADD_LOG(tr("this time elapsed:%1s").arg(time.elapsed() / 1000.0));

    return true;
}

bool ZAdbDeviceJobThread::uploadInstallFailed()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QByteArray out;
    QString hint;


    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_USETYPE(tr("start upload install failed for downgrade") + hint, FormDeviceLog::LOG_STEP);

    setScreenTimeout();

    if (installFailedDowngradeIDs.isEmpty()) {
        ADD_LOG(tr("all succeed, no need upload"));
        return true;
    }

    QStringList newVersions;
    QStringList jobs = znode->getNextJobs();
    QList<BundleItem *> items = controller->appconfig->globalBundle->getAppList(jobs);
    foreach (BundleItem *item, items) {
        if (item->isHiddenItem) {
            if (!item->platform.contains(controller->appconfig->platform)) {
                continue;
            }
        }
        QString id = item->id;
        if (installFailedDowngradeIDs.contains(id)) {
            out = adb.adb_cmd(QString("shell:dumpsys package %1 | grep versionName").arg(item->pkg));
            DBG("get pkg versionName=%s\n", out.data());
            newVersions.append(QString(out).remove("versionName=").trimmed());
        }
    }

    if (!useAsync) {
        QUrl url(controller->appconfig->sUrl + "/app.ashx");
        controller->loginManager->revokeUrl(url);

        QString params = QString("action=lowversionadd&apkid=%1&newversion=%2&clienttool=SI&clientversion=%3")
                .arg(installFailedDowngradeIDs.join(",")).arg(newVersions.join(",")).arg(MMQT2_VER);
        DBG("url=<%s>, out=<%s>\n", qPrintable(url.toString()), out.data());

        QByteArray out;
        if (0 != ZDMHttp::post(url.toString().toUtf8().data(), params.toLocal8Bit().data(), out)) {
            hint = QObject::tr("network error");
            ADD_LOG_USETYPE(tr("upload install failed for downgrade failed!,") + hint, FormDeviceLog::LOG_WARNING);
            return false;
        }
        DBG("out=<%s>\n", out.data());

        QJson::Parser p;
        QVariantMap map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();
        int code = map.find("code").value().toInt();
        if (code != 0 || code != 200) {
            hint = map.find("msg").value().toString();
            ADD_LOG_USETYPE(tr("upload install failed for downgrade failed!,") + hint, FormDeviceLog::LOG_WARNING);
            return false;
        }
    }

    return true;
}

bool ZAdbDeviceJobThread::uploadInstallLog()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    bool ret;

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_AND_HINT_USETYPE(tr("start upload install log") + hint, FormDeviceLog::LOG_STEP);

    setScreenTimeout();

    QStringList uploadIDList;  // ids for upload
    QStringList uploadHIDList;
    QStringList tmpList;
    QStringList jobs = znode->getNextJobs();

    //QList<BundleItem *> items = controller->appconfig->globalBundle->getAppList(jobs);
    QList<BundleItem *> items = controller->appconfig->globalBundle->getBundle(installBundleIDs.first())->getAppList();
    foreach (BundleItem *item, items) {
        if (item->isHiddenItem) {
            if (!item->platform.contains(controller->appconfig->platform)) {
                continue;
            }
        }
        if (installFailedPkgs.contains(item->pkg)) {
            continue;
        }

        tmpList = item->id.split("_");
        item->id = tmpList.at(0);

        if (!item->isHiddenItem) {
            uploadIDList.append(item->id);
        } else {
            uploadHIDList.append(item->id);
        }
    }

    // 同步或者允许PC预上报时处理
    if (!useAsync || controller->appconfig->bAllowPCUploadInstallResultWhenAsync) {  // 允许PC预上报，则当成同步处理就行
        if (uploadIDList.isEmpty()) {
            ADD_LOG_USETYPE(tr("no need upload, all local files") + hint, FormDeviceLog::LOG_WARNING);
        } else {
            DBG("what is the fuck?");
            QString aggIds = "";
            QString uploadIds = uploadIDList.join(",");
            QString hiddenIds = uploadHIDList.join(",");
            DBG("uploadIDS:%s\n",qPrintable(uploadIds));
            QMap<QString, QString> uploadDataMap;
            InstLog *instLog = controller->loginManager->prepareInstLog(znode, aggIds, uploadIds, hiddenIds, uploadDataMap);

            if (controller->appconfig->offlineMode) {  // offline mode
                ADD_LOG_USETYPE(tr("offline mode, install log cached"), FormDeviceLog::LOG_WARNING);
                controller->appconfig->instCacheDb->addData(instLog);
                if (!useAsync) {
                    bool ret = agentCli.checkUploadResult(tr("OK, offline mode, install log cached"), installTotal, installSuccessCounts);
                    DBG("check upload result=<%d>\n", ret);
                }
            } else {  // online mode
                int uploadRet = -1;
                QString hint;
                if ((uploadRet = controller->loginManager->core_uploadInstLog(instLog->url, hint))
                        != LoginManager::ERR_NOERR) {
                    if (uploadRet == LoginManager::ERR_NETWORK) {
                        if (core_hint(DialogHint::TYPE_YESNO,
                                      znode->getName() + tr(" upload fail: ") + hint + tr("\ndo you want to retry?"),
                                      "") != QDialog::Accepted) {
                            // 用户取消安装
                            delete instLog;
                            return false;
                        }
                    } else {
                        ADD_LOG_USETYPE(tr("this phone has been installed, continue to install will be unpaid!"), FormDeviceLog::LOG_ERROR);
                    }
                }
                if (uploadRet == LoginManager::ERR_NETWORK) {  // 网络故障保存到本地，并提示成功
                    controller->appconfig->instCacheDb->addData(instLog);
                    ADD_LOG_USETYPE(tr("upload failed, but install log cached"), FormDeviceLog::LOG_WARNING);
                }else{
                    instLog->result = hint;
                    controller->appconfig->instLogDb->addData(instLog);
                    if(uploadRet != LoginManager::ERR_NOERR) {
                        ADD_LOG_USETYPE(tr("upload result:") + hint, FormDeviceLog::LOG_WARNING);
                    }
                }

                if (!useAsync) {  // 同步时让精灵提示上报结果
                    ret = agentCli.checkUploadResult(hint, installTotal, installSuccessCounts);
                    DBG("check checkUploadResult: hint=<%s>, installTotal=<%d>, installSuccessCounts=<%d>, result=<%d>\n",
                        qPrintable(hint), installTotal, installSuccessCounts, ret);
                } else {
                    if (controller->appconfig->bAllowPCUploadInstallResultWhenAsync) {  // 异步时向精灵报告PC预上报结果
                        ret = agentCli.notifyAsyncPseudoUploadResult(hint, copyTotal, copySuccessCounts);
                        DBG("check notifyAsyncPseudoUploadResult: hint=<%s>, copyTotal=<%d>, copySuccessCounts=<%d>, result=<%d>\n",
                            qPrintable(hint), copyTotal, copySuccessCounts, ret);
                    }
                }
            }

            delete instLog;
        }
    }

    if (useAsync) {  // 异步并且不允许PC端预上报
        QString aggIds = "";
        QString uploadIds = uploadIDList.join(",");
        DBG("uploadIDS:%s\n",qPrintable(uploadIds));
        QString hiddenIds = uploadHIDList.join(",");
        QMap<QString, QString> uploadDataMap;

        if (!controller->appconfig->bAllowPCUploadInstallResultWhenAsync && controller->appconfig->offlineMode) {  // offline mode
            InstLog *instLog = controller->loginManager->prepareInstLog(znode, aggIds, uploadIds, hiddenIds, uploadDataMap);
            controller->appconfig->instCacheDb->addData(instLog);
            ADD_LOG_USETYPE(tr("offline mode, install log cached"), FormDeviceLog::LOG_WARNING);
            delete instLog;
        }

        InstLog *instLog = controller->loginManager->prepareInstLog(znode, aggIds, uploadIds, hiddenIds, uploadDataMap);
        if (!controller->appconfig->offlineMode) {  // online mode, add data to instLogDb
            controller->appconfig->instLogDb->addData(instLog);
        }
        delete instLog;

        if (!cli.switchQueueCli()) {
            ADD_LOG_USETYPE(tr("async switch error"), FormDeviceLog::LOG_ERROR);
            return false;
        }

        if (!cli.saveUploadData(uploadDataMap)) {
            ADD_LOG_USETYPE(tr("save upload url failed"), FormDeviceLog::LOG_ERROR);
            return false;
        }

        ret = true;
    }

    return ret;
}

bool ZAdbDeviceJobThread::doFinsalTask()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QString cmd;
    QByteArray out;
    QStringList jobs = znode->getNextJobs();
    QList<BundleItem *> items = controller->appconfig->globalBundle->getAppList(jobs);

    if (znode->connect_stat != ZAdbDeviceNode::CS_DEVICE) {
        ADD_LOG_USETYPE(tr("device offline, stop install"), FormDeviceLog::LOG_ERROR);
        return false;
    }

    ADD_LOG_AND_HINT_USETYPE(tr("start exec other command...") + hint, FormDeviceLog::LOG_STEP);

    // TODO: lock USB...
    QByteArray protectPkgList;  // pkgs for pm/agg protect
    foreach (BundleItem *item, items) {
        if (installFailedPkgs.contains(item->pkg)){
            continue;
        }
        protectPkgList.append(item->pkg);
        protectPkgList.append('\n');
    }

    cli.switchRootCli();
    cli.pushData(protectPkgList, "/system/etc/.List");
    cli.pushData(protectPkgList, "/system/etc/.applist");
    cli.pushData(protectPkgList, "/data/local/tmp/.applist");

    // set auto start apps
    if (!setAutoStart()) {
        return false;
    }

    // set Desktop
    setDesktop();

    // clear jobs
    znode->finishJobs(jobs);
    qDeleteAll(customItems.begin(), customItems.end());
    customItems.clear();

    // restart UIClick
    startUIClick(true);

    // alert
    if (useAsync){
        bool execSucceed;
        int retryTimes = 5;
        while (retryTimes--) {
            execSucceed = cli.execQueue();  // NOTE: 某些机型需要尝试多次才能成功？
            if (execSucceed) {
                break;
            }
        }
        if (!execSucceed) {
            ADD_LOG_USETYPE(tr("can't exec queue"), FormDeviceLog::LOG_ERROR);
            return false;
        }
        ADD_LOG_USETYPE(tr("using async install, check log on the phone"), FormDeviceLog::LOG_FINISHED);
    } else {
        int time = 5;
        while(time--){
            sleep(1);
            if(agentCli.setAlert(ZMSG2_ALERT_FINISH_POWEROFF)){
                break;
            }
        }
    }

    //clear
    QDir dir;
    dir.remove(qApp->applicationDirPath() + "/DesktopConfiguration.ini");
    if(!useAsync){
        adb.adb_cmd("shell:rm -f /data/local/tmp/dx.jar");
        adb.adb_cmd("shell:rm -f /data/local/tmp/uiclick.jar");
        adb.adb_cmd("shell:rm -f /data/local/tmp/YunOSDrag.jar");
        adb.adb_cmd("shell:rm -f /data/local/tmp/ZAgent2.apk");
        adb.adb_cmd("shell:rm -f /data/lcoal/ZAgent2.apk");
        //        adb.adb_cmd(QString("shell:rm -rf %1").arg(znode->zm_info.store_dir));
    }

    ADD_LOG_AND_HINT_USETYPE(tr("finished, ok to unplug"), FormDeviceLog::LOG_FINISHED);

    //    if (!useAsync) {
    //        adb.adb_cmd("shell:rm -rf "ZM_BASE_DIR);
    //    }
    return true;
}

bool ZAdbDeviceJobThread::setAutoStart()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    ADD_LOG_USETYPE(tr("set auto start"), FormDeviceLog::LOG_STEP);

    setScreenTimeout();

    QString strList = controller->appconfig->selfRunAppsStr;
    strList = "net.dx.cloudgame]com.android.activate]" + strList;
    bool ret = agentCli.setStartApps(strList);
    int count = 0;
    while((!ret)&&(count<5)){
        ret = agentCli.setStartApps(strList);
        sleep(100);
        count++;
    }
    if (!ret) {
        ADD_LOG("set start apps failure");
    }

    return ret;
}

QStringList ZAdbDeviceJobThread::getUninstallBlacklist()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    QStringList blacklist;

    QString tcid;
    QString ispublic;
    Bundle *bundle = controller->appconfig->globalBundle->getBundle(installBundleIDs.first());
    if (bundle) {
        tcid = bundle->id;
        ispublic = bundle->ispublic;
    }

    // check file exists
    QStringList filePaths;
    filePaths.append(qApp->applicationDirPath() + "/DataFromServer/Desktop/" + (QStringList() << znode->brand << znode->model << tcid << ispublic).join("_"));
    filePaths.append(qApp->applicationDirPath() + "/DataFromServer/Desktop/" + (QStringList() << znode->brand << "default" << tcid << ispublic).join("_"));
    filePaths.append(qApp->applicationDirPath() + "/DataFromServer/Desktop/" + (QStringList() << "default" << "default" << tcid << ispublic).join("_"));
    QString filePath;
    foreach (const QString &path, filePaths) {
        if (QFile::exists(path)) {
            filePath = path;
            break;
        } else {
            DBG("local desktop config file not exsist! filePath=%s\n", qPrintable(path));
        }
    }

    if (filePath.isEmpty()) {
        return blacklist;
    }

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        do {
            QJson::Parser p;
            QVariantMap map;
            map = p.parse(file.readAll().data()).toMap();

            int code = map.find("code").value().toInt();
            if (1000 != code) {
                DBG("do nothing, beacuse code=%d\n", code);
                break;
            }

            map = map.value("Desktop").toMap();
            QVariantList list;

            // notuninstalls
            list = map.value("notuninstalls").toList();
            foreach (const QVariant &va, list) {
                blacklist.append(va.toString());
            }

            // cusnotuninstalls
            list = map.value("cusnotuninstalls").toList();
            foreach (const QVariant &va, list) {
                blacklist.append(va.toString());
            }
        } while(0);

        file.close();
    }

    return blacklist;
}

void ZAdbDeviceJobThread::setScreenTimeout()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    if (!setScreenTimeoutResult) {
        setScreenTimeoutResult = agentCli.setScreenOffTimeout(30 * 60 * 1000);
    }
}

bool ZAdbDeviceJobThread::killProcess(const QString &name, bool shellCmdNeedAddTaskForQueue)
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;

    QByteArray out = adb.adb_cmd(QString("shell:ps | grep %1").arg(name));
    DBG("ps %s, out=<%s>\n", name.data(), out.constData());
    QTextStream stream(out);
    QStringList cmds;
    QString line;
    do {
        line = stream.readLine();
        QStringList list = line.split(" ");
        foreach (const QString str, list) {
            if (str.isEmpty()) {
                list.removeOne(str);
            }
        }
        if ((list.contains("shell") || list.contains("root")) && list.size() >= 2) {
            QString pid = list.at(1);
            // shell
            if (list.contains("shell")) {
                DBG("get shell pid: %s\n", qPrintable(pid));
                cmds.append(QString("kill %1").arg(pid));
            }
            // root
            if (list.contains("root")) {
                DBG("get root pid: %s\n", qPrintable(pid));
                cmds.append(QString("su -c 'kill %1'").arg(pid));  // NOTE: 不一定成功？
                cmds.append(QString("kill %1").arg(pid));
            }
        }
    } while (!line.isNull());

    // check async
    if (shellCmdNeedAddTaskForQueue && !cli.switchQueueCli()) {
        ADD_LOG_USETYPE(tr("async switch error"), FormDeviceLog::LOG_ERROR);
    }

    if (!shellCmdNeedAddTaskForQueue) {
        foreach (const QString &cmd, cmds) {
            out = adb.adb_cmd("shell:" + cmd);
            DBG("kill uiautomator, out=<%s>\n", out.constData());
        }
    } else {
        foreach (const QString &cmd, cmds) {
            cli.addShellCommandQueue(cmd);
            sleep(1);
        }
    }

    // exit async
    if (shellCmdNeedAddTaskForQueue && !cli.quitQueue()) {
        ADD_LOG_USETYPE(tr("async quit error"), FormDeviceLog::LOG_ERROR);
    }

    return true;
}

int ZAdbDeviceJobThread::getSDKVersion()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QByteArray out;

    int ret = -1;

#if 0 //
    out = adb.adb_cmd("shell:grep ro.build.version.sdk= system/build.prop");
    DBG("getSDBVersion, out=<%s>\n", qPrintable(out));
    QStringList list = QString(out.trimmed()).split("=");
    if (list.size() == 2) {
        ret = list.at(1).toInt();
    }
#endif

    int times = 3;
    while (times--) {
        out = adb.adb_cmd("shell:getprop ro.build.version.sdk");
        DBG("get sdk version=<%s>", out.data());
        if (!out.isEmpty()) {
            ret = QString(out).toInt();
            break;
        }
    }

    return ret;
}

bool ZAdbDeviceJobThread::installByZAgent(const QString &path, const QString &packageName)
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QString cmd;
    QByteArray out;

    bool result = false;

    int times = 2;
    while (times--) {
        result = agentCli.installApp(path, hint);
        if (result) {
            break;
        }

        if ("Failed2" == hint) {
            startDXClick();
        }
    }
    if (!result) {
        DBG("invoking ZAgent func failed\n");
        return false;
    }

    times = 30;
    while (times--) {
        sleep(1);
        out = adb.pm_cmd(QString("path %1").arg(packageName), 10000);
        DBG("pm path times: %d, result: %s\n", 30 - times, out.data());
        if (out.startsWith("package:")) {
            result = true;
            break;
        }
    }

    return result;
}

bool ZAdbDeviceJobThread::uninstallByZAgent(const QString &packageName)
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QString cmd;
    QByteArray out;

    bool result = false;

    int times = 2;
    while (times--) {
        result = agentCli.uninstallApp(packageName, hint);
        if (result) {
            break;
        }

        if ("Failed2" == hint) {
            startDXClick();
        }
    }
    if (!result) {
        DBG("invoking ZAgent func failed\n");
        return false;
    }

    times = 30;
    while (times--) {
        sleep(1);
        out = adb.pm_cmd(QString("path %1").arg(packageName), 10000);
        DBG("pm path times: %d, result: %s\n", 30 - times, out.data());
        if (!out.startsWith("package:")) {
            result = true;
            break;
        }
    }

    return result;
}

void ZAdbDeviceJobThread::setDesktop()
{
    FastManClient cli(znode->adb_serial, znode->zm_port);
    FastManAgentClient agentCli(znode->adb_serial, znode->zm_port);
    AdbClient adb(node->adb_serial);
    QString hint;
    QString cmd;
    QByteArray out;

    QStringList jobs = znode->getNextJobs();
    QList<BundleItem *> items = controller->appconfig->globalBundle->getBundle(installBundleIDs.first())->getAppList();


    ADD_LOG_USETYPE(tr("set desktop and set auto app, and so on"), FormDeviceLog::LOG_STEP);

    // TODO: check device info
    if (znode->brand == "" || znode->model == ""){
        DBG("brand or model is NULL!\n");
        getdeviceinfo();
    }

    // red dot
    if (needInstallDesktop) {
        if(!controller->appconfig->CustomDesktop) {
            DoWithRedDot(items);
        }else{
            DoWithHZRedDot(items);
        }
    }

    // launcher
    if (needInstallDesktop) {
        QString _systype;
#if defined(Q_OS_WIN)
        _systype = "win";
#elif defined(Q_OS_LINUX)
        _systype = "linux";
#elif defined(Q_OS_OSX)
        _systype = "osx";
#endif
        Bundle *bundle = controller->appconfig->globalBundle->getBundle(installBundleIDs.first());
        LauncherConfig config(NULL);
        config.flag = false;
        config.standardFlag = false;
        config.Brand = znode->brand;
        config.Model = znode->model;
        config.Ispublic = bundle->ispublic;
        config.Tc = bundle->id;
        config.User = controller->appconfig->username;
        config.sys = _systype;
        DBG("read desktop json,model= %s,brand= %s\n",config.Model.toUtf8().data(),config.Brand.toUtf8().data());
        if(controller->appconfig->offlineMode){
            config.offLine = true;
            config.init(NULL);
        }else{
            config.offLine = false;
            QString urlStr = controller->appconfig->sUrl + "/GetDesktop.ashx";
            controller->loginManager->revokeUrlString(urlStr);
            config.init(urlStr.toUtf8());
        }
        if(!config.flag){
            DBG("read sqlite desktop json");
            DBG("TCID:%s\n",config.Tc.toLocal8Bit().data());
            config.readini(config.Model,config.Tc);  // TODO: ?
        }
        LauncherMaker maker(&config, node->adb_serial.toUtf8().data());
        maker._editor->standard = config.standardFlag;
        maker._editor->installGameHouse = controller->appconfig->bInstallGamehouse;
        // init
        QList<AGENT_FILTER_INFO *> filterList;
        agentCli.getFilterAppInfos("com.zxly.assist", filterList, false);
        maker.init();
        maker._editor->m_filter = filterList;
        maker._editor->Serial = node->adb_serial;
        maker._editor->hiddenShowTime = controller->appconfig->showHiddenTCTime;
        maker.InsertAPK("net.dx.cloudgame", "net.dx.cloudgame.SplashActivity");  //游戏之家要提前加入
        maker._editor->useAsync = useAsync;

        // config
        foreach (BundleItem *item, items) {
            maker._editor->ListPkg.append(item->pkg);
            if (installFailedPkgs.contains(item->pkg)){
                continue;
            }
            LOCAL_APP_INFO *localAppInfo = new LOCAL_APP_INFO;
            localAppInfo = agentCli.getLocalAppInfo(item->path);
            if (localAppInfo != NULL) {
                if(item->pkg == "com.baidu.BaiduMap")
                {
                    localAppInfo->className = "com.baidu.baidumaps.WelcomeScreen";
                }
                maker.InsertAPK(item->pkg, localAppInfo->className);
            }
        }

        // 处理隐藏应用
        foreach (BundleItem *item, items) {
            if (!item->isHiddenItem) {
                continue;
            }
            DBG("handle hiddenImtem pkg:%s",item->pkg.toUtf8().data());
            LOCAL_APP_INFO *appInfo = new LOCAL_APP_INFO;
            appInfo = agentCli.getLocalAppInfo(item->path);
            if (appInfo != NULL){
                HiddenNode node;
                if (item->pkg.contains(".")){
                    if (item->pkg == "com.baidu.BaiduMap"){
                        appInfo->className = "com.baidu.baidumaps.WelcomeScreen";
                    }
                    node.pkgname = item->pkg;
                } else {
                    if (item->pkg == ZStringUtil::fromGBK("百度地图")){
                        appInfo->className = "com.baidu.baidumaps.WelcomeScreen";
                    }
                    node.pkgname = item->pkg;
                }
                if(node.pkgname == "com.nhzw.bingdu"){
                    node.classname = "com.nhzw.bingdu.MainActivity";
                }else{
                    node.classname = appInfo->className;
                }
                maker._hiddenOrder.append(node);
            }
        }

        maker.sync();

        ReplaceIcons(maker);

        // push
        ADD_LOG(tr("push [zx_default_workspace]"));
        bool result = adb.adb_push(maker.m_configName, "data/local/tmp/zx_default_workspace.xml", hint, 0777);
        if (!result) {
            ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
        }
        QFile::remove(maker.m_configName);

        if(config.standardFlag) {
            ADD_LOG(tr("push [desktop_device_profile]"));
            result = adb.adb_push(maker.m_ini, "data/local/tmp/desktop_device_profile.xml", hint, 0777);
            if(!result){
                ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
            }
            QFile::remove(maker.m_ini);
        }

        ADD_LOG(tr("push [desktop_module_config]"));
        result = adb.adb_push(maker.m_hiddenConf, "data/local/tmp/desktop_module_config.xml", hint, 0777);
        if(!result){
            ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
        }

        for(int i=0; i<maker._editor->appmarket.size(); i++){
            QString pkg = maker._editor->appmarket.at(i);
            cmd = QString("pm clear %1").arg(pkg);
            if (!useAsync) {
                out = adb.adb_cmd("shell:" + cmd);
            } else {
                cli.addShellCommandQueue(cmd);
            }
            DBG("pm clear appmarket:cmd=%s, out=%s\n", qPrintable(cmd), qPrintable(out));
            if (!znode->brand.contains("xiaomi", Qt::CaseInsensitive)) {
                cmd = QString("pm hide %1").arg(pkg);
                if (!useAsync) {
                    out = adb.adb_cmd("shell:" + cmd);
                } else {
                    cli.addShellCommandQueue(cmd);
                }
                DBG("pm hide appmarket:cmd=%s, out=%s\n", qPrintable(cmd), qPrintable(out));
            }
        }
    }

    // end UIClick（先关闭才能设置桌面）
    ADD_LOG(tr("kill UIClick"));
    xsleep(5000);
    killProcess("uiautomator", useAsync);

    // check async
    if (useAsync && !cli.switchQueueCli()) {
        ADD_LOG_USETYPE(tr("async switch error"), FormDeviceLog::LOG_ERROR);
    }

    bool ret = adb.adb_push(controller->appconfig->YAMUser ? "dx_yam.jar" : "dx.jar", "data/local/tmp/dx.jar", hint, 0777);
    if (!ret) {
        ADD_LOG_USETYPE("Failure:" + hint.trimmed(), FormDeviceLog::LOG_WARNING);
    }

    // replace theme
//    if (needReplaceTheme) {
//        DBG("need replace desktop theme\n");
//        cmd = "uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#MAIN -e replace " + controller->appconfig->themesCodeStr;
//        if (!useAsync) {
//            out = adb.adb_cmd("shell:" + cmd);
//            DBG("replace desktop theme, out=<%s>\n", out.constData());
//        } else {
//            cli.addShellCommandQueue(cmd);
//        }
//    }

    // check set-active-admin
    bool canSetActive = false;
    if (controller->appconfig->appActivateMap.size() > 0) {
        QString cmd = QString("dpm set-active-admin com.dx.agent2/.AdminListener");
        out = adb.adb_cmd("shell:" + cmd);
        DBG("cmd=%s, out=%s\n", qPrintable(cmd), qPrintable(out));
        canSetActive = out.trimmed().contains("Success");
    }
//    if (canSetActive) {
//        QMap<QString, QVariant>::const_iterator i = controller->appconfig->appActivateMap.constBegin();
//        while (i != controller->appconfig->appActivateMap.constEnd()) {
//            QString cmd = QString("dpm set-active-admin %1/%2").arg(i.key()).arg(i.value().toString());
//            if (!useAsync) {
//                out = adb.adb_cmd("shell:" + cmd);
//                DBG("cmd=%s, out=%s\n", qPrintable(cmd), qPrintable(out));
//            } else {
//                cli.addShellCommandQueue(cmd);
//            }
//            ++i;
//        }
//    }

    // 设置默认桌面，设置自启应用，取消激活等
    if (needInstallDesktop) {
        ADD_LOG(tr("exec auto start and default desktop cmd"));
    } else {
        ADD_LOG(tr("exec auto start cmd"));
    }
    cmd = QString("uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#MAIN -e applist all,donotdisableapp%1")
            .arg(canSetActive ? "" : " -e setAdmin setAdmin");
    DBG("cmd=%s\n", qPrintable(cmd));
    if (!useAsync) {
        out = adb.adb_cmd("shell:" + cmd);
    } else {
        cli.addShellCommandQueue(cmd);
    }

    if (canSetActive) {
        QMap<QString, QVariant>::const_iterator i = controller->appconfig->appActivateMap.constBegin();
        while (i != controller->appconfig->appActivateMap.constEnd()) {
            QString cmd = QString("dpm set-active-admin %1/%2").arg(i.key()).arg(i.value().toString());
            if (!useAsync) {
                out = adb.adb_cmd("shell:" + cmd);
                DBG("cmd=%s, out=%s\n", qPrintable(cmd), qPrintable(out));
            } else {
                cli.addShellCommandQueue(cmd);
            }
            ++i;
        }
    }

    // start
    if (needInstallDesktop) {
        ADD_LOG(tr("exec start Desktop cmd"));
        cmd = "am start -n com.shyz.desktop/.Launcher";
        if (!useAsync) {
            out = adb.adb_cmd("shell:" + cmd);
        } else {
            cli.addShellCommandQueue(cmd);
        }
    }

    // reset
    if (needInstallDesktop) {
        ADD_LOG(tr("exec reset desktop cmd"));
        cmd = "pm clear com.shyz.desktop";
        if (!useAsync) {
            out = adb.adb_cmd("shell:" + cmd);
        } else {
            cli.addShellCommandQueue(cmd);
        }
    }

    // exit async
    if (useAsync && !cli.quitQueue()) {
        ADD_LOG_USETYPE(tr("async quit error"), FormDeviceLog::LOG_ERROR);
    }
}

void ZAdbDeviceJobThread::DoWithHZRedDot(QList<BundleItem *> items)
{
    QFile reddotFile(controller->appconfig->reddotFilePath);
    if (!reddotFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        DBG("local red dot file not exsist! fileName=%s\n", qPrintable(reddotFile.fileName()));
        return;
    }

    QByteArray out = reddotFile.readAll();
    DBG("read red dot file out=<%s>\n", out.data());

    QString pkg, md5;
    foreach (BundleItem *item, customItems) {
        if(item->pkg == "net.dx.cloudgame") {
            pkg = item->pkg;
            md5 = item->md5;
        }
    }

    QJson::Parser p;
    QString noRedot = "com.tencent.mm,com.tencent.mobileqq,com.iflytek.inputmethod";
    QVariantMap map;
    QVariantList list;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();
    bool isshow = map.find("isshow").value().toBool();
    if(!isshow){
        return;
    }
    QStringList hideapppkgList;
    QString hideapppkgs;
    QString firstruntrigger = map.find("firstruntrigger").value().toString();
    QString cycleintervaltime = map.find("cycleintervaltime").value().toString();
    QString onceintervaltime = map.find("onceintervaltime").value().toString();
    QString oncedisplaycount = map.find("oncedisplaycount").value().toString();
    QString appusedflow = map.find("appusedflow").value().toString();
    QString clickedcount = map.find("clickedcount").value().toString();
    QString sCode = map.value("code").toString();
    QString sMsg = map.value("msg").toString();
    DBG("code = %s,msg = %s\n",sCode.toUtf8().data(),sMsg.toUtf8().data());
    if (sCode!="1000"){
        return;
    }

    //生成xml
    QString configname = QUuid::createUuid().toString() + "_hz_redot_display_config.xml";
    QFile file(configname);
    file.remove();
    if(!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QTextStream text_out(&file);
    text_out.setCodec("UTF-8");
    QDomDocument doc;
    QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild(instruction);
    QDomElement root = doc.createElement("redotinfos");
    doc.appendChild(root);

    QDomElement e = doc.createElement("firstruntrigger");
    QDomText vValueText = doc.createTextNode(firstruntrigger);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("cycleintervaltime");
    vValueText = doc.createTextNode(cycleintervaltime);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("onceintervaltime");
    vValueText = doc.createTextNode(onceintervaltime);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("oncedisplaycount");
    vValueText = doc.createTextNode(oncedisplaycount);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("appusedflow");
    vValueText = doc.createTextNode(appusedflow);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("clickedcount");
    vValueText = doc.createTextNode(clickedcount);
    e.appendChild(vValueText);
    root.appendChild(e);

    foreach(BundleItem *item, items) {
        if(item->isHiddenItem) {
            hideapppkgList.append(item->pkg);
        }
    }

    hideapppkgs = hideapppkgList.join(",");
    e = doc.createElement("hideapppkgs");
    vValueText = doc.createTextNode(hideapppkgs);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("redotinfo");
    QDomElement g = doc.createElement("packagename");
    vValueText = doc.createTextNode(pkg);
    g.appendChild(vValueText);
    e.appendChild(g);
    g = doc.createElement("md5");
    vValueText = doc.createTextNode(md5);
    g.appendChild(vValueText);
    e.appendChild(g);
    root.appendChild(e);

    foreach(BundleItem *item, items) {
        if(noRedot.contains(item->pkg)){
            continue;
        }
        e = doc.createElement("redotinfo");
        QDomElement f = doc.createElement("packagename");
        vValueText = doc.createTextNode(item->pkg);
        f.appendChild(vValueText);
        e.appendChild(f);
        f = doc.createElement("md5");
        vValueText = doc.createTextNode(item->md5);
        f.appendChild(vValueText);
        e.appendChild(f);
        root.appendChild(e);
    }

    doc.save(text_out, 4, QDomNode::EncodingFromTextStream);
    file.close();

    AdbClient adb(node->adb_serial);
    adb.adb_push(configname,"data/local/tmp/hz_redot_display_config.xml",0777);
    QFile::remove(configname);
}

void ZAdbDeviceJobThread::DoWithRedDot(QList<BundleItem *> items)
{
    // 得到数据
    QFile reddotFile(controller->appconfig->reddotFilePath);
    if (!reddotFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        DBG("local red dot file not exsist! fileName=%s\n", qPrintable(reddotFile.fileName()));
        return;
    }

    QByteArray out = reddotFile.readAll();
    DBG("read red dot file out=<%s>\n", out.data());

    QString pkg;
    foreach (BundleItem *item, customItems) {
        if(item->pkg == "net.dx.cloudgame") {
            pkg = item->pkg;
        }
    }

    QJson::Parser p;
    QString noRedot = "com.tencent.mm,com.tencent.mobileqq,com.iflytek.inputmethod";
    QVariantMap map;
    QVariantList list;
    map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();
    bool isshow = map.find("isshow").value().toBool();
    if(!isshow){
        return;
    }

    QString intervaltime = map.find("intervaltime").value().toString();
    QString oncedisplaynum = map.find("oncedisplaynum").value().toString();
    QString sCode = map.value("code").toString();
    QString sMsg = map.value("msg").toString();
    DBG("code = %s,msg = %s\n",sCode.toUtf8().data(),sMsg.toUtf8().data());
    if (sCode!="1000"){
        return;
    }

    //生成xml
    QString configname = QUuid::createUuid().toString() + "_hz_redot_display_config.xml";
    QFile file(configname);
    file.remove();
    if(!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QTextStream text_out(&file);
    text_out.setCodec("UTF-8");
    QDomDocument doc;
    QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild(instruction);
    QDomElement root = doc.createElement("redotinfos");
    doc.appendChild(root);

    QDomElement e = doc.createElement("intervaltime");
    QDomText vValueText = doc.createTextNode(intervaltime);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("oncedisplaynum");
    vValueText = doc.createTextNode(oncedisplaynum);
    e.appendChild(vValueText);
    root.appendChild(e);

    e = doc.createElement("redotinfo");
    QDomElement g = doc.createElement("packagename");
    vValueText = doc.createTextNode(pkg);
    g.appendChild(vValueText);
    e.appendChild(g);
    g = doc.createElement("redotnum");
    vValueText = doc.createTextNode("1");
    g.appendChild(vValueText);
    e.appendChild(g);
    root.appendChild(e);

    foreach(BundleItem *item, items) {
        if(noRedot.contains(item->pkg)){
            continue;
        }
        e = doc.createElement("redotinfo");
        QDomElement f = doc.createElement("packagename");
        vValueText = doc.createTextNode(item->pkg);
        f.appendChild(vValueText);
        e.appendChild(f);
        f = doc.createElement("redotnum");
        vValueText = doc.createTextNode("1");
        f.appendChild(vValueText);
        e.appendChild(f);
        root.appendChild(e);
    }

    doc.save(text_out, 4, QDomNode::EncodingFromTextStream);
    file.close();

    AdbClient adb(node->adb_serial);
    adb.adb_push(configname,"data/local/tmp/redot_display_config.xml",0777);
    QFile::remove(configname);

}

void ZAdbDeviceJobThread::ReplaceIcons(LauncherMaker maker) //图标替换
{
    AdbClient adb(node->adb_serial);
    QByteArray out;
    QString brand = "KOOBEE,LETV,LEECO,MEIZU,OPPO,SAMSUNG,ZTE,GIONEE";
    bool ret;
    ret = adb.adb_push("unzip_pie","/data/local/tmp/unzip",0777);
    //判断是华为 还是小米
    if(
            (znode->brand.contains("huawei",Qt::CaseInsensitive))
            ||(znode->brand.contains("honor",Qt::CaseInsensitive))//华为 荣耀等
            ){
        //判断当前主题
        out = adb.adb_cmd("shell:ls system/themes");
        QList<QByteArray> linesData = out.split('\n');
        QString CurTheme = "";
        foreach(QByteArray va,linesData){
            if(va.contains("Default")){
                CurTheme = QLatin1String(va.data());
                break;
            }
        }

        if(CurTheme == ""){
            foreach(QByteArray va,linesData){
                if(va.contains("lock")){
                    CurTheme = QLatin1String(va.data());
                    break;
                }
            }
        }

        if(CurTheme == ""){
            foreach(QByteArray va,linesData){
                if(va.contains("magazine")){
                    CurTheme = QLatin1String(va.data());
                    break;
                }
            }
        }

        if(CurTheme == ""){
            foreach(QByteArray va,linesData){
                if(va.contains("Golden")){
                    CurTheme = QLatin1String(va.data());
                    break;
                }
            }
        }

        if(CurTheme == ""){
            foreach(QByteArray va,linesData){
                if(va.contains("Colorful")){
                    CurTheme = QLatin1String(va.data());
                    break;
                }
            }
        }


        if(CurTheme == ""){
            foreach(QByteArray va,linesData){
                if(va!=""){
                    CurTheme = QLatin1String(va.data());
                    break;
                }
            }
        }

        //解压图标到相关目录
        CurTheme = CurTheme.trimmed();
        QString cmd = QString("shell:/data/local/tmp/unzip list \'/system/themes/%1\'").arg(CurTheme);
        out = adb.adb_cmd(cmd);
        cmd = QString("shell:/data/local/tmp/unzip unzip \'/system/themes/%1\' /data/local/tmp/icons~  icons").arg(CurTheme);
        out = adb.adb_cmd(cmd);

        for(int it = 0;it != maker._editor->_ReplaceOrder.size();++it){
            QString fromIcon = maker._editor->_ReplaceOrder.at(it)->frompkgname;
            fromIcon.append(".png");
            QString toIcon = maker._editor->_ReplaceOrder.at(it)->topkgname;
            toIcon.replace(".","_");
            toIcon.append(".png");
            QString s = "shell:/data/local/tmp/unzip unzip /data/local/tmp/icons~  /data/local/tmp/" + fromIcon +" " +fromIcon;
            out = adb.adb_cmd(s.arg(CurTheme));
            s = "shell:cp /data/local/tmp/" + fromIcon +" " +"/data/local/tmp/desktop_icon/" +toIcon;
            out = adb.adb_cmd(s);
        }
        ret = adb.adb_push("ini.txt","/data/local/tmp/desktop_icon/ini.txt",0777);
    }

    if(znode->brand.contains("xiaomi",Qt::CaseInsensitive)){// 小米
        for(int it = 0;it != maker._editor->_ReplaceOrder.size();++it){
            QString fromIcon = maker._editor->_ReplaceOrder.at(it)->frompkgname;
            fromIcon.append(".png");
            QString toIcon = maker._editor->_ReplaceOrder.at(it)->topkgname;
            toIcon.replace(".","_");
            toIcon.append(".png");
            QString s = "shell:cp /system/media/theme/miui_mod_icons/" +fromIcon +" " +"/data/local/tmp/desktop_icon/" +toIcon;
            out = adb.adb_cmd(s);
        }
        ret = adb.adb_push("ini.txt","/data/local/tmp/desktop_icon/ini.txt",0777);
    }

    if(znode->brand.contains("Coolpad",Qt::CaseInsensitive)){
        for(int it = 0;it != maker._editor->_ReplaceOrder.size();++it){
            QString fromIcon = maker._editor->_ReplaceOrder.at(it)->frompkgname;
            if(fromIcon == "com.yulong.android.security"){
                fromIcon = "com.yulong.android.seccenter.png";
            }
            else if(fromIcon == "com.coolpad.music"){
                fromIcon = "com.android.music.png";
            }
            else if(fromIcon == "com.yulong.android.gamecenter"){
                fromIcon = "com.egame.png";
            }
            else{
                fromIcon.append(".png");
            }
            QString toIcon = maker._editor->_ReplaceOrder.at(it)->topkgname;
            toIcon.replace(".","_");
            toIcon.append(".png");
            QString s = "shell:cp /system/media/theme/coollife_ui_icons/" +fromIcon +" " +"/data/local/tmp/desktop_icon/" +toIcon;
            out = adb.adb_cmd(s);
        }
        ret = adb.adb_push("ini.txt","/data/local/tmp/desktop_icon/ini.txt",0777);
    }

    QStringList blist = brand.split(",");
    if(blist.size()>0){
        for(int i=0; i<blist.size(); i++){
            QString Brand = blist.at(i);
            if(znode->brand.contains(Brand,Qt::CaseInsensitive)){
                QProcess p;
                QString tool;
#ifdef _WIN32
                tool = QString("\"" +qApp->applicationDirPath()+"/zxlycon.exe\"");
#else
                tool = QString("\"" +qApp->applicationDirPath()+"/zxlycon\"");
#endif
                QString program = QString("\"" +qApp->applicationDirPath()+"/theme/%1\"").arg(Brand);
                QString cmd = QString("%1 push %2 /data/local/tmp/%3").arg(tool).arg(program).arg(Brand);
                p.start(cmd);
                p.waitForFinished();
                for(int it = 0;it != maker._editor->_ReplaceOrder.size();++it){
                    QString fromIcon = maker._editor->_ReplaceOrder.at(it)->frompkgname;
                    fromIcon.replace(".","_");
                    fromIcon.append(".png");
                    QString toIcon = maker._editor->_ReplaceOrder.at(it)->topkgname;
                    toIcon.replace(".","_");
                    toIcon.append(".png");
                    QString s = QString("shell:cp /data/local/tmp/%1/%2 /data/local/tmp/desktop_icon/%3").arg(Brand).arg(fromIcon).arg(toIcon);
                    out = adb.adb_cmd(s);
                }
                ret = adb.adb_push("ini.txt","/data/local/tmp/desktop_icon/ini.txt",0777);
            }
        }
    }
}

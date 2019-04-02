#include "coremain.h"
#include "globalcontroller.h"
#include "zthreadpool.h"
#include "appconfig.h"
#include "bundle.h"
#include "zdownloadmanager.h"
#include "adbtracker.h"
#include "loginmanager.h"
#include "dialoglogin.h"

#include "zadbdevicenode.h"
#include "zadbdevicejobthread.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QTimer>
#include <QFile>
#include <QDateTime>

ItemParseThread::ItemParseThread(BundleItem *item) {
    this->item = item;
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

void ItemParseThread::run() {
    int ret = ZDownloadTask::STAT_FAILED;

    QCryptographicHash md5(QCryptographicHash::Md5);
    QFile f(item->path);
    char buf[4096];
    int n;
    item->mtime = QDateTime::currentDateTime().toTime_t();

    do {
        if(!f.open(QIODevice::ReadOnly)) {
            DBG("%s md5 fopen failed!\n", item->name.toLocal8Bit().data());
            break;
        }
        while((n = f.read(buf, sizeof(buf))) > 0) {
            md5.addData(buf, n);
        }
        f.close();

        if(item->md5.toUpper() != md5.result().toHex().toUpper()) {
            DBG("%s md5 check failed!\n", item->name.toLocal8Bit().data());
            break;
        }

        if(!item->parseApk(item->path)) {
            DBG("%s apk parse failed!\n", item->name.toLocal8Bit().data());
            break;
        }
        ret = ZDownloadTask::STAT_FINISHED;
    } while(0);
    item->download_status = ret;
    emit signal_itemParseDone(item);
}

CoreMain::CoreMain(QObject *parent)
    : QObject(parent)
{
    controller = GlobalController::getInstance();
    // init managers
    controller->threadpool = new ZThreadPool(this);
    controller->appconfig = new AppConfig(this);
    controller->downloadmanager = ZDownloadManager::newDownloadManager("mmqt2_dm", this);
    controller->adbTracker = new AdbTracker("zxlycon", this);
    controller->loginManager = LoginManager::getInstance();

    connect(controller->adbTracker, SIGNAL(sigAdbError(QString)), SLOT(slot_critical(QString)));
    connect(controller->adbTracker, SIGNAL(sigDone(QObject*)), controller->threadpool, SLOT(slot_remove(QObject*)));

    // init userspace
    if(!controller->appconfig->initDb()) {
        slot_critical("db init failed!\n");
        return;
    }
    controller->appconfig->loadCfg();
    controller->loginManager->initDb();

    QString loginHint;
    QString username;
    QString encpass;
    if(!controller->appconfig->encpasswd.isEmpty()) {
        username = controller->appconfig->username;
        encpass = controller->appconfig->encpasswd;
    } else {
        slot_critical(tr("no valid stored password!"));
        return;
    }

    if(!LoginThread::core_login(username, encpass, true, false, loginHint)) {
        slot_critical(tr("log in failed, ") + loginHint);
        return;
    }

    controller->appconfig->loggedIn = true;
    controller->appconfig->saveCfg();

    connect(controller->adbTracker, SIGNAL(sigDevListChanged()), SLOT(slot_refreshDevList()));
    connect(controller->adbTracker, SIGNAL(sigDevListChanged()), SLOT(slot_updateCount()));

    connect(controller->downloadmanager, SIGNAL(signal_status(ZDownloadTask*)), SLOT(slot_updateItemStatus(ZDownloadTask*)));
    connect(controller->downloadmanager, SIGNAL(signal_status(ZDownloadTask*)), SLOT(slot_updateCount()));

    // start work
    DBG("before start tracker\n");
    controller->threadpool->addThread(controller->adbTracker, "adbTracker");
    controller->adbTracker->start();

    DBG("before global_init\n");
    ZDownloadManager::global_init();
    controller->appconfig->checkAndDownloadItems();
    controller->downloadmanager->startWork();
}

CoreMain::~CoreMain() {
    controller->adbTracker->freeMem();
    controller->appconfig->saveCfg();
}

void CoreMain::slot_quitApp() {
    controller->needsQuit = true;
    controller->adbTracker->stop();
    controller->downloadmanager->stopWork();

    if(controller->downloadmanager->getRunningCount() > 0) {
        controller->threadpool->addThread((QThread *)controller->downloadmanager, "download_manager");
        connect(controller->downloadmanager, SIGNAL(signal_idle()), SLOT(slot_dmIdle()));
    }

    if(controller->threadpool->size() == 0) {
        qApp->quit();
    } else {
        connect(controller->threadpool, SIGNAL(sigPoolEmpty()), qApp, SLOT(quit()));

        // force quit after 10 seconds
        QTimer tm;
        connect(&tm, SIGNAL(timeout()), qApp, SLOT(quit()));
        tm.start(10000);

        PRT(tr("please wait..."));
    }
}

void CoreMain::slot_dmIdle() {
    controller->threadpool->removeThread((QThread *)controller->downloadmanager);
}

void CoreMain::slot_critical(const QString &hint) {
    PRT(tr("slot_critical ")+hint);
    slot_quitApp();
}

void CoreMain::slot_refreshBundleList() {

}

void CoreMain::slot_refreshDevList() {
    DBG("slot_refreshDevList\n");
    if(controller->needsQuit) {
        return;
    }

    devUIListMutex.lock();
    QList<AdbDeviceNode *> devices = controller->adbTracker->getDeviceList();
    foreach(AdbDeviceNode * n, devices) {
        if (n->connect_stat == AdbDeviceNode::CS_DEVICE && n->jobThread == NULL) {
            ZAdbDeviceJobThread *t = new ZAdbDeviceJobThread(n, this);
            connect(t, SIGNAL(sigDeviceRefresh(AdbDeviceNode*)), this, SLOT(slot_refreshUI(AdbDeviceNode*)));
            connect(t, SIGNAL(sigDeviceStat(AdbDeviceNode*,QString)), this, SLOT(slot_setHint(AdbDeviceNode*,QString)));
            connect(t, SIGNAL(sigDeviceProgress(AdbDeviceNode*,int,int)), this, SLOT(slot_setProgress(AdbDeviceNode*,int,int)));
            connect(t, SIGNAL(sigDeviceLog(AdbDeviceNode*,QString)), this, SLOT(slot_deviceLog(AdbDeviceNode*,QString)));

            n->jobThread = t;
            t->start();
        }
    }
    devUIListMutex.unlock();
}

void CoreMain::slot_updateItemStatus(ZDownloadTask *task) {
    BundleItem *item = controller->appconfig->globalBundle->getApp(task->id);
    if(item != NULL) {
        if(task->status == ZDownloadTask::STAT_FINISHED) {
            ItemParseThread *t = new ItemParseThread(item);
            connect(t, SIGNAL(signal_itemParseDone(BundleItem*)),
                    SLOT(slot_itemParseDone(BundleItem*)));
            t->start();
        } else {
            item->download_status = task->status;
            controller->appconfig->saveBundleItem(item);
        }
    }
}

void CoreMain::slot_itemParseDone(BundleItem *item) {
    QStringList list = item->id.split("_");
    item->id = list.at(0);
    ZDownloadTask *task = controller->downloadmanager->getTask(item->id);
    if(item->download_status != ZDownloadTask::STAT_FINISHED) {
        task->status = ZDownloadTask::STAT_FAILED;
        PRT(tr("download %1 failed, retry").arg(item->name));
        controller->downloadmanager->startTask(task);
    }
    controller->appconfig->saveBundleItem(item);
}

void CoreMain::slot_updateCount() {
    static int lastCount = 0;
    int devCount = controller->adbTracker->getActiveCount();
    int downCount = controller->downloadmanager->getUnfinishedCount();
    PRT(tr("active device count %1, unfinished download task count %2").arg(devCount).arg(downCount));
    if((0 == devCount) && (lastCount != 0) ) {
        lastCount = devCount;
        PRT(tr("automatically clean up"));
        controller->adbTracker->cleanUp();
    }
    lastCount = devCount;
}

void CoreMain::slot_refreshUI(AdbDeviceNode *node) {
    ZAdbDeviceNode *n = (ZAdbDeviceNode *)node;
    PRT(tr("Conn     %1 [%2]").arg(n->getName(), n->connect_statname));
}

void CoreMain::slot_setHint(AdbDeviceNode* node, const QString& hint) {
    ZAdbDeviceNode *n = (ZAdbDeviceNode *)node;
    PRT(tr("Status   %1 [%2]").arg(n->getName(), hint));
}

void CoreMain::slot_setProgress(AdbDeviceNode* node, int progress, int max) {
    ZAdbDeviceNode *n = (ZAdbDeviceNode *)node;
    PRT(tr("Progress %1 [%2/%3]").arg(n->getName()).arg(progress).arg(max));
}

void CoreMain::slot_deviceLog(AdbDeviceNode* node, const QString& log) {
    ZAdbDeviceNode *n = (ZAdbDeviceNode *)node;
    PRT(tr("Log      %1 [%2]").arg(n->getName(), log));
}

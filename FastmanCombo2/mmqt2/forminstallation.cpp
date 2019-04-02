#include "forminstallation.h"
#include "ui_forminstallation.h"

#include "formbundlenode.h"
#include "formdevicenode.h"

#include "globalcontroller.h"
#include "adbtracker.h"
#include "appconfig.h"
#include "bundle.h"
#include "zadbdevicenode.h"
#include "zadbdevicejobthread.h"
#include "formdevicelog.h"
#include "dialogseltoclean.h"
#include "dialoghint.h"
#include "loginmanager.h"
#include "dialogrootflags.h"
#include "dialogconfigwifi.h"
#include <QFileDialog>
#include <QSettings>
#include <QApplication>

#ifdef USE_JUNBO
#include "junbointerface.h"
#include "ziphelper.h"
#include "dialogjunbologin.h"
#include <QMessageBox>
#endif

RefreshAgentThread::RefreshAgentThread(const QList<AdbDeviceNode *> &list) {
    devices.append(list);
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

void RefreshAgentThread::run() {
    foreach(AdbDeviceNode *n, devices) {
        ZAdbDeviceNode *zn = (ZAdbDeviceNode *)n;
        if(zn->connect_stat == AdbDeviceNode::CS_DEVICE) {
            FastManAgentClient cli(zn->adb_serial, zn->zm_port);
            cli.setConnType(zn->node_index, ZMSG2_CONNTYPE_USB);
        }
    }
}

FormInstallation::FormInstallation(QWidget *parent) :
    QWidget(parent), ui(new Ui::FormInstallation) {
    ui->setupUi(this);
    controller = GlobalController::getInstance();
    initFinished = false;

    if (controller->loginManager->disableAsync) {
        ui->chk_async_install->hide();
        controller->appconfig->bAsyncInst = false;
    } else {
        ui->chk_async_install->setChecked(controller->appconfig->bAsyncInst);
        // check offline mode
        if (controller->appconfig->offlineMode) {
            ui->chk_async_install->setChecked(false);
            ui->chk_async_install->setEnabled(false);
        }
    }
    ui->chk_uninst_userapps->setChecked(controller->appconfig->bUninstUsrApp);

    ui->chk_installByZAgent->setChecked(controller->appconfig->bNeedZAgentInstallAndUninstall);

    if (/*controller->loginManager->disableRoot*/true) {  // TODO: temp disable root
        ui->chk_try_root->hide();
        ui->chk_uninst_sysapps->hide();
        controller->appconfig->bTryRoot = false;
        controller->appconfig->bUninstSysApp = false;
    } else {
        ui->chk_try_root->setChecked(controller->appconfig->bTryRoot);
        ui->chk_uninst_sysapps->setEnabled(controller->appconfig->bTryRoot);

        if (controller->appconfig->bTryRoot) {
            ui->chk_uninst_sysapps->setChecked(controller->appconfig->bUninstSysApp);
        } else {
            ui->chk_uninst_sysapps->setChecked(false);
        }
    }

    ui->chk_auto_inst_bundle->setChecked(true);
    ui->chk_auto_inst_bundle->setEnabled(false);
    ui->btn_sel_all->hide();
    ui->btn_sel_rev->hide();
    ui->btn_install->hide();
    //ui->chk_installByZAgent->hide();
    slot_refreshBundleList();

    ui->chk_use_LAN->setChecked(controller->appconfig->spInstall);

    connect(ui->chk_auto_inst_bundle, SIGNAL(toggled(bool)), SLOT(slot_toggledAutoInstBundle(bool)));
    connect(ui->chk_async_install, SIGNAL(toggled(bool)), SLOT(slot_toggledAsyncInst(bool)));
    connect(ui->chk_installByZAgent, SIGNAL(toggled(bool)), SLOT(slot_toggledInstByZAgent(bool)));
    connect(ui->chk_try_root, SIGNAL(toggled(bool)), SLOT(slot_toggledTryRoot(bool)));
    connect(ui->chk_uninst_userapps, SIGNAL(toggled(bool)), SLOT(slot_toggledUninstUsrApp(bool)));
    connect(ui->chk_uninst_sysapps, SIGNAL(toggled(bool)), SLOT(slot_toggledUninstSysApp(bool)));
    connect(ui->chk_use_LAN, SIGNAL(toggled(bool)), SLOT(slot_toggledUseLAN(bool)));

    connect(ui->tv_devices, SIGNAL(clicked(QModelIndex)), SLOT(slot_deviceClicked(QModelIndex)));
    connect(ui->tv_bundles, SIGNAL(clicked(QModelIndex)), SLOT(slot_bundleClicked(QModelIndex)));
#ifdef USE_JUNBO
    connect(ui->btn_load_mm_bundle, SIGNAL(clicked()), SLOT(slot_add_mm_bundle()));
#else
    ui->btn_load_mm_bundle->hide();
#endif
    connect(ui->btn_sel_all, SIGNAL(clicked()), SLOT(slot_bundleSelAll()));
    connect(ui->btn_sel_rev, SIGNAL(clicked()), SLOT(slot_bundleSelRev()));
    connect(ui->btn_install, SIGNAL(clicked()), SLOT(slot_install()));
    connect(ui->btn_cleanup, SIGNAL(clicked()), SLOT(slot_cleanup()));
    connect(ui->chk_pauseInstall, SIGNAL(toggled(bool)), SLOT(slot_toggledPauseInstall(bool)));
    initFinished = true;

    slot_checkWiFiConfig();
    QSettings settings(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    bool ret = settings.value("CFG/NotHintWiFiConfigNextTime").toBool();
    if (!ret) {
        DialogConfigWiFi *dlg = new DialogConfigWiFi(this);
        connect(dlg, SIGNAL(signal_WiFiConfigChanged()), SLOT(slot_checkWiFiConfig()));
        dlg->show();
    }

    //slot_checkLANConfig();

}

FormInstallation::~FormInstallation() {
    delete ui;
}

QStringList FormInstallation::getCheckedBundles() {
    QStringList checkedIds;
    int count = ui->tv_bundles->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = ui->tv_bundles->item(i);
        FormBundleNode *bundle_ui = (FormBundleNode *) ui->tv_bundles->itemWidget(item);
        if (bundle_ui->isChecked()) {
            checkedIds.append(bundle_ui->bundle->id);
        }
    }
    return checkedIds;
}

void FormInstallation::slot_bundleClicked(Bundle *b, bool checked) {
    if(!initFinished) {
        return;
    }

    // keep only one bundle be choosed
    int count = ui->tv_bundles->count();
    for (int i = 0; i < count; ++i) {
        QListWidgetItem *item = ui->tv_bundles->item(i);
        FormBundleNode *bundle_ui = (FormBundleNode *) ui->tv_bundles->itemWidget(item);
        if ((bundle_ui->bundle == b) && checked) {
            bundle_ui->setChecked(true);
            controller->appconfig->addAutoInstBundles(QStringList() << bundle_ui->bundle->id);
        } else {
            bundle_ui->setChecked(false);
            controller->appconfig->removeAutoInstBundle(QStringList() << bundle_ui->bundle->id);
        }
    }
}

void FormInstallation::slot_toggledAutoInstBundle(bool checked) {
    if(!initFinished) {
        return;
    }

    if(checked) {
        controller->appconfig->setAutoInstBundles(getCheckedBundles());
    } else {
        controller->appconfig->clearAutoInstBundles();
    }
}

void FormInstallation::slot_toggledAsyncInst(bool checked) {
    controller->appconfig->bAsyncInst = checked;
}

void FormInstallation::slot_toggledInstByZAgent(bool checked)
{
    controller->appconfig->bNeedZAgentInstallAndUninstall = checked;
}

void FormInstallation::slot_toggledTryRoot(bool checked) {
    if(checked) {
        DialogRootFlags *dlg = new DialogRootFlags(this);
        dlg->exec();
        delete dlg;

        if(controller->appconfig->rootFlag == 0) {
            ui->chk_try_root->setChecked(false);
            return;
        }
    }
    controller->appconfig->bTryRoot = checked;

    ui->chk_uninst_sysapps->setEnabled(checked);
    ui->chk_uninst_sysapps->setChecked(checked && controller->appconfig->bUninstSysApp);
}

void FormInstallation::slot_toggledUninstUsrApp(bool checked) {
    controller->appconfig->bUninstUsrApp = checked;
}

void FormInstallation::slot_toggledUninstSysApp(bool checked) {
    controller->appconfig->bUninstSysApp = checked;
}

void FormInstallation::slot_toggledPauseInstall(bool checked)
{
    controller->appconfig->bPauseInstall = checked;
    if (!checked) {
        slot_refreshDevList();
    }
}

void FormInstallation::slot_toggledUseLAN(bool checked)
{
    controller->appconfig->spInstall = checked;
}

void FormInstallation::slot_deviceClicked(const QModelIndex &index) {
    QListWidgetItem *item = ui->tv_devices->item(index.row());
    FormDeviceNode *nui = (FormDeviceNode *) ui->tv_devices->itemWidget(item);
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        FormDeviceLog *w = (FormDeviceLog *) ui->tabWidget->widget(i);
        if(w->node == nui->node) {
            ui->tabWidget->setCurrentWidget(w);
            break;
        }
    }
}

void FormInstallation::slot_bundleClicked(const QModelIndex &index) {
    QListWidgetItem *item = ui->tv_bundles->item(index.row());
    FormBundleNode *bundle_ui = (FormBundleNode *) ui->tv_bundles->itemWidget(item);
    slot_bundleClicked(bundle_ui->bundle, !bundle_ui->isChecked());
}

#ifdef USE_JUNBO
void FormInstallation::slot_add_mm_bundle() {
    JunboInterface *interf = JunboInterface::getInstance();
    if(!interf->isLoggedIn) {
        DialogJunboLogin *dlg = new DialogJunboLogin(this);
        if(QDialog::Accepted != dlg->exec()) {
            delete dlg;
            return;
        }
        delete dlg;
    }

    QString zip = QFileDialog::getOpenFileName(this, tr("Choose MM bundle to load"),
                                               QString(), "*.mdx");
    if(zip.isEmpty()) {
        return;
    }

    DBG("zip = %s\n", zip.toLocal8Bit().data());
    if(!interf->loadAppPackage(zip, "DOWNLOAD_APPS/mmpkgs")) {
        QMessageBox::information(this, tr("hint"), tr("invalid mm package!"), QMessageBox::Ok);
    }
    emit sig_mmpackage_added(interf->app_list_name);
}
#endif

void FormInstallation::slot_bundleSelAll() {
    int count = ui->tv_bundles->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = ui->tv_bundles->item(i);
        FormBundleNode *bundle_ui = (FormBundleNode *) ui->tv_bundles->itemWidget(item);
        bundle_ui->setChecked(true);
    }
}

void FormInstallation::slot_bundleSelRev() {
    int count = ui->tv_bundles->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = ui->tv_bundles->item(i);
        FormBundleNode *bundle_ui = (FormBundleNode *) ui->tv_bundles->itemWidget(item);
        bundle_ui->setChecked(!bundle_ui->isChecked());
    }
}

void FormInstallation::slot_install() {
    QStringList checkedIds;
    QString hint;
    QString sBundleID;
    int count = ui->tv_bundles->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = ui->tv_bundles->item(i);
        FormBundleNode *bundle_ui = (FormBundleNode *) ui->tv_bundles->itemWidget(item);
        if (bundle_ui->isChecked()) {
            checkedIds.append(bundle_ui->bundle->getAppIds());
            sBundleID = bundle_ui->bundle->id;
        }
    }

    if (checkedIds.isEmpty()) {
        return;
    }

    count = ui->tv_devices->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = ui->tv_devices->item(i);
        FormDeviceNode *nui = (FormDeviceNode *) ui->tv_devices->itemWidget(item);
        ZAdbDeviceNode *znode = (ZAdbDeviceNode *) nui->node;
        int r = znode->addJobs(checkedIds);
        znode->sBundleID = sBundleID;
        hint.append(tr("%1 added %2 tasks\n").arg(znode->getName()).arg(r));
    }

    DialogHint::exec_hint(this, tr("hint"), hint, "info_addJobs", DialogHint::TYPE_INFORMATION);
}

void FormInstallation::slot_cleanup() {
    ui->btn_cleanup->setText(tr("cleaning up"));
    ui->btn_cleanup->setEnabled(false);
    QMutexLocker locker(&cleanupMutex);

    int count = ui->tv_devices->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = ui->tv_devices->item(i);
        FormDeviceNode *nui = (FormDeviceNode *) ui->tv_devices->itemWidget(item);
        ZAdbDeviceNode *znode = (ZAdbDeviceNode *) nui->node;
        znode->clearJobHistory();
    }
    controller->adbTracker->cleanUp();
    ui->btn_cleanup->setText(tr("clean up"));
    ui->btn_cleanup->setEnabled(true);
}

void FormInstallation::slot_installFinished(bool result)
{
    if (result) {
        slot_cleanup();
    }
}

void FormInstallation::slot_checkWiFiConfig()
{
    QFileInfo info(qApp->applicationDirPath() + "/WiFiConfiguration.ini");
    bool hasWiFiConfig = info.exists();
    ui->chk_async_install->setChecked(hasWiFiConfig);
    ui->chk_async_install->setEnabled(hasWiFiConfig);
    ui->chk_async_install->setToolTip(tr("enable when has WiFi configuration!"));
}

void FormInstallation::slot_checkLANConfig()
{
    QSettings settings(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    QString url = settings.value("LAN/Url").toString();
    QString port = settings.value("LAN/Port").toString();

    bool enable = (!url.isEmpty() && !port.isEmpty());
    //ui->chk_use_LAN->setEnabled(enable);
    ui->chk_use_LAN->setChecked(enable && controller->appconfig->spInstall);
    ui->chk_use_LAN->setToolTip(tr("enable when url and port is not empty!"));
}

void FormInstallation::slot_refreshBundleList() {
    DBG("slot_refreshBundleList\n");
    bundleUIListMutex.lock();
    QList<Bundle *> bundles = controller->appconfig->globalBundle->getBundleList();
    ui->tv_bundles->clear();

    int index = 0;
    foreach(Bundle * b, bundles) {
        if (!b->enable || b->type != Bundle::TYPE_APK) {
            continue;
        }

        FormBundleNode *bundle_ui = new FormBundleNode(b);
        QListWidgetItem *item = new QListWidgetItem();
        item->setSizeHint(QSize(0, 40));
        ui->tv_bundles->addItem(item);
        ui->tv_bundles->setItemWidget(item, bundle_ui);
        connect(bundle_ui, SIGNAL(signal_bundleClicked(Bundle*,bool)), SLOT(slot_bundleClicked(Bundle*,bool)));

        bundle_ui->setIcon(index++);
        bundle_ui->slot_refreshUI(b);
        bundle_ui->setChecked(controller->appconfig->isAutoInstBundle(b->id));
    }

    bundleUIListMutex.unlock();
}

void FormInstallation::slot_refreshDevList() {
    DBG("slot_refreshDevList\n");
    if(controller->needsQuit) {
        return;
    }

    devUIListMutex.lock();
    QList<AdbDeviceNode *> devices = controller->adbTracker->getDeviceList();
    QList<FormDeviceNode *> uilist;
    QList<FormDeviceLog *> tblist;
    bool found = false;

    // remove cleaned up items
    for (int i = 0; i < ui->tv_devices->count(); i++) {
        QListWidgetItem *item = ui->tv_devices->item(i);
        FormDeviceNode *nui = (FormDeviceNode *) ui->tv_devices->itemWidget(item);

        found = false;
        foreach(AdbDeviceNode * n, devices) {
            if (nui->node == n) {
                found = true;
                uilist.append(nui);
                break;
            }
        }

        if (!found) {
            i--;
            ui->tv_devices->removeItemWidget(item);
            delete nui;
            delete item;
        }
    }

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        FormDeviceLog *tui = (FormDeviceLog *) ui->tabWidget->widget(i);
        found = false;
        foreach(AdbDeviceNode * n, devices) {
            if (tui->node == n) {
                found = true;
                ui->tabWidget->setTabText(i, QString::number(n->node_index));
                tblist.append(tui);
                break;
            }
        }

        if (!found) {
            ui->tabWidget->removeTab(i--);
            delete tui;
        }
    }

    // check the same adb serial
    if (devices.size() > 1) {
        QStringList adbSerials;
        foreach (AdbDeviceNode *device, devices) {
            adbSerials << device->adb_serial;
        }
        foreach (AdbDeviceNode *device, devices) {
            if (adbSerials.contains(device->adb_serial)) {
                foreach (AdbDeviceNode *device, devices) {
                    DBG("Found the same adb serial:%s\n", qPrintable(device->adb_serial));
                }
                break;
            }
        }
    }

    FormDeviceNode *node_ui;
    FormDeviceLog *log_ui;
    foreach(AdbDeviceNode * n, devices) {
        node_ui = NULL;
        log_ui = NULL;
        foreach(FormDeviceNode * nui, uilist) {
            if (nui->node == n) {
                node_ui = nui;
                break;
            }
        }

        foreach(FormDeviceLog *tui, tblist) {
            if(tui->node == n) {
                log_ui = tui;
                break;
            }
        }

        if (node_ui == NULL) {
            node_ui = new FormDeviceNode(n);
            QListWidgetItem *item = new QListWidgetItem();
            item->setSizeHint(QSize(0, 40));
            ui->tv_devices->addItem(item);
            ui->tv_devices->setItemWidget(item, node_ui);
        }

        if(log_ui == NULL) {
            log_ui = new FormDeviceLog(n, this);
            ui->tabWidget->addTab(log_ui, QString::number(n->node_index));
        }

        node_ui->slot_refreshUI(n);

        if (n->connect_stat == AdbDeviceNode::CS_DEVICE && n->jobThread == NULL) {
            DBG("##new ZAdbDeviceJobThread, adbSerial=<%s>\n", qPrintable(n->adb_serial));
            ZAdbDeviceJobThread *t = new ZAdbDeviceJobThread(n, this);
            n->jobThread = t;
            connect(t, SIGNAL(sigDeviceRefresh(AdbDeviceNode*)), node_ui, SLOT(slot_refreshUI(AdbDeviceNode*)));
            connect(t, SIGNAL(sigDeviceStat(AdbDeviceNode*,QString)), node_ui, SLOT(slot_setHint(AdbDeviceNode*,QString)));
            connect(t, SIGNAL(sigDeviceProgress(AdbDeviceNode*,int,int)), node_ui, SLOT(slot_setProgress(AdbDeviceNode*,int,int)));
            connect(t, SIGNAL(sigDeviceLog(AdbDeviceNode*,QString,int,bool)), log_ui, SLOT(slotDeviceLog(AdbDeviceNode*,QString,int,bool)));
            connect(t, SIGNAL(signal_initThread(AdbDeviceNode*)), log_ui, SLOT(slot_clearLog(AdbDeviceNode*)));
            connect(t, SIGNAL(signal_hint(void*,int,QString,QString)), this, SLOT(slot_hint(void*,int,QString,QString)));
            connect(t, SIGNAL(signal_chooseToClean(void*,void*)), this, SLOT(slot_chooseToClean(void*,void*)));
            connect(t, SIGNAL(signal_finished(bool)), this, SLOT(slot_installFinished(bool)));

            connect(this, SIGNAL(sig_checkHint()), t, SIGNAL(sig_checkHint()));
            connect(this, SIGNAL(sig_checkChoice()), t, SIGNAL(sig_checkChoice()));
            t->start();
        }
    }
    RefreshAgentThread *t = new RefreshAgentThread(devices);
    t->start();
    devUIListMutex.unlock();
}

void FormInstallation::slot_hint(void *ret, int type, const QString &hint, const QString& tag) {
    int r = DialogHint::exec_hint(this, tr("hint"), hint, tag, type);
    *((int *)ret) = r;
    emit sig_checkHint();
}

void FormInstallation::slot_chooseToClean(void *ret, void *data) {
    int r = -255;
    DialogSelToClean *dlg = new DialogSelToClean(data, this);
    r = dlg->exec();
    DBG("ret = %d, addr = %p\n", r, ret);
    *((int *)ret) = r;
    emit sig_checkChoice();
}

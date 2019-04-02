#include "dialogsetup.h"
#include "ui_dialogsetup.h"

#include <QMouseEvent>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QMessageBox>
#include <QLocalSocket>

#include "globalcontroller.h"
#include "appconfig.h"
#include "fastman2.h"
#include "dialogtidyapk.h"
#include "dialoglogshow.h"
#include "zsingleprocesshelper.h"
#include "dialogdelwhitelist.h"
#include "dialogconfigwifi.h"
#include "dialogconfiglan.h"
#include "zlog.h"
#include "dialogdesktopini.h"

extern const QString SERVERNAME_HELPER;

DialogSetup::DialogSetup(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogSetup)
{
    setAttribute(Qt::WA_TranslucentBackground);
    ui->setupUi(this);
    config = GlobalController::getInstance()->appconfig;
    if(!config->loggedIn) {
        ui->groupBox_tools->hide();
    }

    ui->chk_alert_ring->setChecked((config->alertType & ZMSG2_ALERT_SOUND) != 0);
    ui->chk_alert_vibr->setChecked((config->alertType & ZMSG2_ALERT_VIBRATE) != 0);

    ui->chk_adbtrack->setChecked(getAutoStart("mmqt2_helper", qApp->applicationDirPath() + "/mmqt2_helper"));
    ui->chk_autostart->setChecked(getAutoStart("mmqt2", qApp->applicationFilePath()));

    connect(ui->btn_ok, SIGNAL(clicked()), SLOT(slot_ok()));
    connect(ui->btn_tidy_apk, SIGNAL(clicked()), SLOT(slot_tidyApk()));
    connect(ui->btn_delete_whitelist, SIGNAL(clicked()), this, SLOT(slot_delete_whitelist()));
    connect(ui->btn_delete_hintini, SIGNAL(clicked()), this, SLOT(slot_delete_hintini()));
    connect(ui->btn_logview, SIGNAL(clicked()), this, SLOT(slot_logview()));
    connect(ui->btn_delete_cachelog, SIGNAL(clicked()), this, SLOT(slot_delete_Cachelog()));
    connect(ui->btn_config_WiFi, SIGNAL(clicked()), this, SLOT(slot_config_WiFi()));
    connect(ui->btn_config_LAN, SIGNAL(clicked()), this, SLOT(slot_config_LAN()));
    connect(ui->btn_desktopini, SIGNAL(clicked()), this, SLOT(slot_desktopini()));
}

DialogSetup::~DialogSetup()
{
    delete ui;
}

void DialogSetup::mousePressEvent(QMouseEvent *event) {
    // current
    int x = event->pos().x();
    int y = event->pos().y();

    // top left
    int x1 = ui->layout_setupheader->pos().x();
    int y1 = ui->layout_setupheader->pos().y();

    // bottom right
    int x2 = x1 + ui->layout_setupheader->width();
    int y2 = y1 + ui->layout_setupheader->height();

    if(x > x1 && x < x2 && y > y1 && y < y2) {
        lastMousePos = event->pos();
        isDraggingWindow = true;
    }
}

void DialogSetup::mouseMoveEvent(QMouseEvent *event) {
    if(isDraggingWindow) {
        this->move(event->globalPos()-lastMousePos);
    }
}

void DialogSetup::mouseReleaseEvent(QMouseEvent *event) {
    isDraggingWindow = false;
}

bool DialogSetup::getAutoStart(const QString& key, const QString& file) {
#ifdef _WIN32
    QString appPath = file;
    appPath = appPath.replace('/', '\\');
    QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    if(reg.allKeys().contains(key)) {
        return reg.value(key).toString() == appPath;
    }
#else
    QString link = QDir::homePath() + "/.kde/Autostart/" + key;
    QFileInfo f(link);
    if(f.exists() && f.isSymLink()) {
        return f.symLinkTarget() == file;
    }
#endif
    return false;
}

void DialogSetup::setAutoStart(const QString& key, const QString& file, bool start) {
#ifdef _WIN32
    QString appPath = file;
    appPath = appPath.replace('/', '\\');
    QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    if(start) {
        reg.setValue(key, appPath);
    } else {
        reg.remove(key);
    }
#else
    QString link = QDir::homePath() + "/.kde/Autostart/" + key;
    QFile::remove(link);
    if(start) {
        QFile::link(file, link);
    }
#endif
}

void DialogSetup::slot_ok() {
    unsigned short alertType = 0;
    if(ui->chk_alert_ring->isChecked()) {
        alertType |= ZMSG2_ALERT_SOUND;
    }
    if(ui->chk_alert_vibr->isChecked()) {
        alertType |= ZMSG2_ALERT_VIBRATE;
    }
    config->alertType = alertType;

    setAutoStart("mmqt2_helper", qApp->applicationDirPath() + "/mmqt2_helper", ui->chk_adbtrack->isChecked());
    setAutoStart("mmqt2", qApp->applicationFilePath(), ui->chk_autostart->isChecked());

    config->saveCfg();
    accept();

    QString holder;
    bool bHelperStarted = ZSingleProcessHelper::hasRemote(SERVERNAME_HELPER, holder);
    //bool bHelperStarted = /*ZSingleProcessHelper::hasRemote(SERVERNAME_HELPER, holder)*/false;
    if (!bHelperStarted && ui->chk_adbtrack->isChecked()) {
        DBG("Launch mmqt2_helper...\n");
        QProcess ps;
#ifdef _WIN32
        ps.startDetached(qApp->applicationDirPath() + "/mmqt2_helper.exe");
#else
        ps.startDetached(qApp->applicationDirPath() + "/mmqt2_helper");
#endif
    }

    if (bHelperStarted && !ui->chk_adbtrack->isChecked()) {
        DBG("Exit mmqt2_helper...\n");
        ZSingleProcessHelper::killRemote(SERVERNAME_HELPER);
    }
}

void DialogSetup::slot_tidyApk() {
    DialogTidyApk *dlg = new DialogTidyApk(this);
    dlg->exec();
    delete dlg;
}

void DialogSetup::slot_delete_whitelist() {
    dialogdelwhitelist *dlg = new dialogdelwhitelist(this);
    dlg->exec();
    delete dlg;
}

void DialogSetup::slot_delete_hintini() {
    QSettings *config = new QSettings("mmqt2.ini", QSettings::IniFormat);
    config->beginGroup("hint");
    config->remove("");
    config->endGroup();
    config->sync();
    delete config;
    ui->btn_delete_hintini->setEnabled(false);
}

void DialogSetup::slot_logview() {
    DialogLogshow *dlg = new DialogLogshow(this);
    dlg->exec();
}

void DialogSetup::slot_delete_Cachelog() {
    int ret = QMessageBox::question(this, tr("delete cached logs"),
                                    tr("Do you want to clear the cached logs?"), QMessageBox::Yes, QMessageBox::No);
    if(ret == QMessageBox::Yes) {
        config->instCacheDb->clearData();
    }
}

void DialogSetup::slot_config_WiFi()
{
    DialogConfigWiFi *dlg = new DialogConfigWiFi(this);
    connect(dlg, SIGNAL(signal_WiFiConfigChanged()), SIGNAL(signal_WiFiConfigChanged()));
    dlg->exec();
}

void DialogSetup::slot_config_LAN()
{
    DialogConfigLAN *dlg = new DialogConfigLAN(this);
    connect(dlg, SIGNAL(signal_LANConfigChanged()), SIGNAL(signal_LANConfigChanged()));
    dlg->exec();
}

void DialogSetup::slot_desktopini()
{
    Dialogdesktopini *dlg = new Dialogdesktopini(this);
    connect(dlg, SIGNAL(signal_DesktopConfigChanged()), SIGNAL(signal_DesktopConfigChanged()));
    dlg->exec();
}


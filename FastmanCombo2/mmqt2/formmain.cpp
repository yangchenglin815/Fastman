#include "formmain.h"
#include "ui_formmain.h"

#include "globalcontroller.h"

#include "adbtracker.h"
#include "zadbdevicenode.h"
#include "zadbdevicejobthread.h"

#include "zthreadpool.h"
#include "appconfig.h"
#include "loginmanager.h"
#include "zdownloadmanager.h"
#include "ThreadBase.h"

#include "formweb.h"
#include "formwebindex.h"
#include "forminstallation.h"
#include "formbundlelist.h"
#include "formdownload.h"
#include "dialogprogress.h"

#include "dialogwait.h"

#include "dialoglogin.h"
#include "dialogfeedback.h"
#include "dialogsetup.h"
#include "popup.h"
#include "zlocalserver.h"

#ifdef USE_DRIVERS
#include "ztimer.h"
#include "driverhelper.h"
#include "formdriverhint.h"
#endif

#include <QLabel>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QTimer>
#include <QDesktopWidget>
#include <QDir>
#include <QProcess>

#include "zdmhttp.h"
#include "ziphelper.h"
#include "zlog.h"

#ifdef _WIN32
#include <cstdlib>
#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

FormMain::FormMain(QWidget *parent) :
    QWidget(parent, Qt::FramelessWindowHint),
    ui(new Ui::FormMain) {
    setAttribute(Qt::WA_TranslucentBackground);
    ui->setupUi(this);
    controller = GlobalController::getInstance();

    isMaxWindow = false;
    isDraggingWindow = false;
    connect(ui->btn_win_max, SIGNAL(clicked()), SLOT(slot_winMaxRestore()));
    connect(ui->btn_win_feedback, SIGNAL(clicked()), SLOT(slot_feedback()));
    connect(ui->btn_win_settings, SIGNAL(clicked()), SLOT(slot_setup()));

    connect(ui->btn_versionText, SIGNAL(clicked()), SLOT(slot_showBuildTime()));
    connect(ui->btn_checkUpdates, SIGNAL(clicked()), SLOT(slot_checkUpdates()));
    connect(this, SIGNAL(singal_checkUpdateResult(bool)), SLOT(slot_checkUpdatesResult(bool)));

    // flip icons
    connect(ui->nav_btn_board, SIGNAL(toggled(bool)), SLOT(slot_flipNavBoard(bool)));
    connect(ui->nav_btn_installation, SIGNAL(toggled(bool)), SLOT(slot_flipNavInstallation(bool)));
    connect(ui->nav_btn_bundles, SIGNAL(toggled(bool)), SLOT(slot_flipNavBundles(bool)));
    connect(ui->nav_btn_download, SIGNAL(toggled(bool)), SLOT(slot_flipNavDownloads(bool)));
    connect(ui->nav_btn_account, SIGNAL(toggled(bool)), SLOT(slot_flipNavMyAccount(bool)));
    connect(ui->nav_btn_tools, SIGNAL(toggled(bool)), SLOT(slot_flipNavTools(bool)));

    ui->nav_btn_bundles->hide();
    ui->label_total_level->hide();
    ui->label_week_level->hide();
    ui->label_user_level->hide();
    ui->nav_btn_tools->hide();
    ui->label_buildTime->hide();

    QTimer::singleShot(20, this, SLOT(slot_initManagers()));
}

void FormMain::slot_initManagers() {
    controller->threadpool = new ZThreadPool(this);
    controller->appconfig = new AppConfig(this);
    controller->downloadmanager = ZDownloadManager::newDownloadManager("mmqt2_dm", this);
    controller->adbTracker = new AdbTracker("zxlycon", this);
    controller->loginManager = LoginManager::getInstance();

    connect(controller->adbTracker, SIGNAL(sigAdbError(QString,int)), SLOT(slot_adbError(QString,int)));
    connect(controller->adbTracker, SIGNAL(sigDone(QObject*)), controller->threadpool, SLOT(slot_remove(QObject*)));

    QTimer::singleShot(20, this, SLOT(slot_initUI()));
}

void FormMain::slot_initUI() {
    if(!controller->appconfig->initDb()) {
        slot_critical("db init failed!\n");
        return;
    }
    controller->appconfig->loadCfg();
    controller->loginManager->initDb();

    // Login DLG
    m_loginDlg = new DialogLogin(this);
    connect(m_loginDlg, SIGNAL(signal_loginSucceed()), SLOT(slot_loadInstallUI()));
    connect(this, SIGNAL(signal_closeLoginDlg()), m_loginDlg, SLOT(accept()));
    int r = m_loginDlg->exec();
    if(r != QDialog::Accepted) {
        slot_quitApp();
        return;
    }

    show();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();

    if (controller->appconfig->offlineMode) {
        QMessageBox::information(this, tr("hint"),
                                 tr("offline mode will cache flash statistic log and upload when online"),
                                 QMessageBox::Ok);
    }

    ThreadBase<FormMain> *t = new ThreadBase<FormMain>(this, &FormMain::cleanupLogFiles);
    t->start(NULL, NULL, NULL);
}

void FormMain::slot_loadInstallUI()
{
    // status label
    QString style = "QLabel{font: bold; color: #FFFFFF; background-color: #f95657; border-radius: 10px;}";
    m_devIdx = new QLabel(ui->nav_btn_installation);
    m_unfinishedIdx = new QLabel(ui->nav_btn_download);
    m_devIdx->resize(26, 20);
    m_unfinishedIdx->resize(26, 20);
    m_devIdx->setAlignment(Qt::AlignCenter);
    m_unfinishedIdx->setAlignment(Qt::AlignCenter);
    m_devIdx->move(120, 10);
    m_unfinishedIdx->move(120, 10);
    m_devIdx->setStyleSheet(style);
    m_unfinishedIdx->setStyleSheet(style);

    connect(controller->adbTracker, SIGNAL(sigDevListChanged()), this, SLOT(slot_updateCntStatus()));
    connect(controller->downloadmanager, SIGNAL(signal_status(ZDownloadTask*)), this, SLOT(slot_updateCntStatus()));
    slot_updateCntStatus();

    // apply user info
    ui->label_userid->setText(controller->appconfig->username);
    if(!controller->appconfig->opername.isEmpty()) {
        ui->label_operator->setText(tr("operator:")+controller->appconfig->opername);
    }
    ui->btn_versionText->setText(tr("current version:%1").arg(QString(MMQT2_VER).toUpper()));

    // main pages
    DBG("load main pages\n");
    Formwebindex *w_board = new Formwebindex(this);

    FormInstallation *w_installation = new FormInstallation(this);
    connect(this, SIGNAL(signal_WiFiConfigChanged()), w_installation, SLOT(slot_checkWiFiConfig()));
    //connect(this, SIGNAL(signal_LANConfigChanged()), w_installation, SLOT(slot_checkLANConfig()));

    FormDownload *w_downloads = new FormDownload(controller->downloadmanager, this);

    FormBundleList *w_bundles = new FormBundleList(this);
    connect(w_downloads, SIGNAL(signal_itemStatusChanged(BundleItem*)), w_bundles, SIGNAL(signal_itemStatusChanged(BundleItem*)));

    FormWeb *w_account = new FormWeb(this);
    QUrl url("http://zj.dengxian.net/v2/money.aspx");
    controller->loginManager->revokeUrl(url);
    controller->loginManager->fillUrlParams(url);
    w_account->setHomeUrl(url);

    QLabel *w_tools = new QLabel("tools", this);

    connect(controller->adbTracker, SIGNAL(sigDevListChanged()), w_installation, SLOT(slot_refreshDevList()));
    connect(w_bundles, SIGNAL(signal_refreshBundleList()), w_installation, SLOT(slot_refreshBundleList()));
#ifdef USE_JUNBO
    connect(w_installation, SIGNAL(sig_mmpackage_added(QString)), SLOT(slot_mmpackage_added(QString)));
#endif

    ui->stw_main->addWidget(w_board);
    ui->stw_main->addWidget(w_installation);
    ui->stw_main->addWidget(w_bundles);
    ui->stw_main->addWidget(w_downloads);
    ui->stw_main->addWidget(w_account);
    ui->stw_main->addWidget(w_tools);

    nav_pages_map.insert(ui->nav_btn_board, w_board);
    nav_pages_map.insert(ui->nav_btn_installation, w_installation);
    nav_pages_map.insert(ui->nav_btn_bundles, w_bundles);
    nav_pages_map.insert(ui->nav_btn_download, w_downloads);
    nav_pages_map.insert(ui->nav_btn_account, w_account);
    nav_pages_map.insert(ui->nav_btn_tools, w_tools);

    connect(ui->nav_btn_board, SIGNAL(clicked()), SLOT(slot_switchBoard()));
    connect(ui->nav_btn_installation, SIGNAL(clicked()), SLOT(slot_switchInstallation()));
    connect(ui->nav_btn_bundles, SIGNAL(clicked()), SLOT(slot_switchBundles()));
    connect(ui->nav_btn_download, SIGNAL(clicked()), SLOT(slot_switchDownloads()));
    connect(ui->nav_btn_account, SIGNAL(clicked()), SLOT(slot_switchMyAccount()));
    connect(ui->nav_btn_account, SIGNAL(clicked()), w_account, SLOT(slot_refresh()));
    connect(ui->nav_btn_tools, SIGNAL(clicked()), SLOT(slot_switchTools()));

    ui->nav_btn_board->hide();
    if(controller->loginManager->disableNavAccount) {
        ui->nav_btn_account->hide();
    }

    switchNav(ui->nav_btn_installation);

    // adb tracker
    DBG("start adb tracker\n");
    controller->threadpool->addThread(controller->adbTracker, "adbTracker");
    controller->adbTracker->start();

    // download
    DBG("start download\n");
    if (!controller->appconfig->offlineMode) {
        ZDownloadManager::global_init();
        controller->appconfig->loadDonwloadConfig();
        controller->appconfig->startDownloadItems();
    }

    // driver
#ifdef USE_DRIVERS
    ZTimer *zTimer = new ZTimer(this);
    DriverManager *drvm = new DriverManager(this);
    connect(zTimer, SIGNAL(signal_timeout()), drvm, SLOT(slot_usbToggled()));
    QTimer::singleShot(2000, drvm, SLOT(slot_startWork()));
#endif
    QTimer *timer = new QTimer;
    timer->setInterval(86400000);
    connect(timer, SIGNAL(timeout()), SLOT(slot_autoCheckUpdates()));
    timer->start();

    QTimer *timer1 = new QTimer;
    timer1->setInterval(86400000);
    connect(timer1, SIGNAL(timeout()), SLOT(slot_checkTC()));
    timer1->start();

    emit signal_closeLoginDlg();
}

void FormMain::slot_updateCntStatus() {
    int devCnts = controller->adbTracker->getActiveCount();
    int unfinishedCnts = controller->downloadmanager->getUnfinishedCount();
    m_devIdx->setText(QString::number(devCnts));
    m_unfinishedIdx->setText(QString::number(unfinishedCnts));
    m_devIdx->setVisible(devCnts <= 0 ? false : true);
    m_unfinishedIdx->setVisible(unfinishedCnts <= 0 ? false : true);
}

int FormMain::checkUpdates(void *p, void *q)
{
    checkUpdatesFailed = false;
    bool updatesAvailable = false;
    do {
        QProcess process;

        process.start(QString("\"%1\" -v --checkupdates").arg(getMaintenceTool()));

        process.waitForFinished();

        if (process.error() != QProcess::UnknownError) {
            DBG("Checking updates error. %s\n", qPrintable(process.errorString()));
            checkUpdatesFailed = true;
            break;
        }

        QByteArray out = process.readAllStandardOutput();
        DBG("Checking updates out=<%s>\n", out.data());

        if (out.isEmpty() || !out.contains("Operations sanity check succeed")){
            DBG("Check Failed.\n");
            break;
        }

        if (out.contains("Network error while downloading")) {
            DBG("Network error.\n");
            break;
        }

        if (out.contains("There are currently no updates available")) {
            DBG("No updates available.\n");
            break;
        }

        updatesAvailable = true;
    } while (0);

    emit singal_checkUpdateResult(updatesAvailable);

    return updatesAvailable;
}

int FormMain::cleanupLogFiles(void *p, void *q)
{
#ifdef LOG_FILENAME_USE_DATE
    QString logRootDir = qApp->applicationDirPath() + "/log";
    QFile::remove(logRootDir + "/DBG.log");

    QDir dir(logRootDir);
    // remove common log
    QStringList files = dir.entryList(QStringList() << "*.log", QDir::Files);
    foreach (const QString &file, files) {
        QDate d = QDate::fromString(file.left(file.length() - 4), "yyyy-MM-dd");
        if (d.isValid() && QDate::currentDate().daysTo(d) < -7) {
            DBG("remove common log 7 days before, %s\n", qPrintable(file));
            QFile::remove(logRootDir + "/" + file);
        }
    }

    // upload install thread log
    if (!controller->appconfig->offlineMode) {
        QStringList dirs = dir.entryList(QDir::Dirs);
        foreach (const QString &dir, dirs) {
            QDate d = QDate::fromString(dir, "yyyy-MM-dd");
            if (d.isValid() && QDate::currentDate().daysTo(d) < 0) {  // 不传当前日期的，避免当前日期文件重复传
                DBG("to zip file for upload, %s\n", qPrintable(dir));
                QDir dirTmp(logRootDir + "/" + dir);
                QStringList logs = dirTmp.entryList(QStringList() << "*.log", QDir::Files);
                ZipHelper zh;
                foreach (const QString &log, logs) {
                    QString file = logRootDir + "/" + dir + "/" + log;
                    DBG("append entry to zip: %s\n", qPrintable(file));
                    QFile f(file);
                    if (!f.open(QIODevice::ReadOnly)) {
                        DBG("open file failed: %s %s\n", qPrintable(file), qPrintable(f.errorString()));
                        continue;
                    }
                    ZipEntry *entry = new ZipEntry();
                    entry->name = log;
                    entry->data = f.readAll();
                    f.close();
                    zh.entries.append(entry);
                }

                QString zipFile = logRootDir + "/" + QString("%1_%2.zip").arg(d.toString("yyyyMMdd"))
                        .arg(controller->loginManager->getMAC());
                DBG("zip file: %s\n", qPrintable(zipFile));
                if (zh.writeZipFile(zipFile)) {
                    // upload
                    QByteArray out;
                    int ret;
                    QUrl url("http://api.dengxian.net/api/UploadLog/Post");
                    controller->loginManager->revokeUrl(url);
                    char *uri = strdup(url.toEncoded().data());
                    if ((ret = ZDMHttp::postFileByMutltiformpost(uri, zipFile, out)) == 0) {
                        DBG("upload zip file succeed, file= %s, out=%s\n", qPrintable(zipFile), out.data());
                        QProcess process;
                        QString path = logRootDir + "/" + dir;
#ifdef _WIN32
                        QStringList args;
                        args << "/c" << "rd" << "/s" << "/q" << path.replace("/", "\\");
                        process.start("cmd", args);
                        process.waitForFinished(-1);
#else
                        process.start(QString("rm -rf %1").arg(path));
                        process.waitForFinished(-1);
#endif
                    } else {
                        DBG("upload zip file failed, ret=%d, out=%s\n", ret, out.data());
                    }

                    QFile::remove(zipFile);
                }
            }
        }
    }

    // cleanup install thread log
    QStringList dirs = dir.entryList(QDir::Dirs);
    foreach (const QString &dir, dirs) {
        QDate d = QDate::fromString(dir, "yyyy-MM-dd");
        if (d.isValid() && QDate::currentDate().daysTo(d) < -7) {  // 一周以前的日志没必要回传！
            DBG("remove install thread log 7 days before, %s\n", qPrintable(dir));
            QProcess process;
            QString path = logRootDir + "/" + dir;
#ifdef _WIN32
            QStringList args;
            args << "/c" << "rd" << "/s" << "/q" << path.replace("/", "\\");
            process.start("cmd", args);
            process.waitForFinished(-1);
#else
            process.start(QString("rm -rf %1").arg(path));
            process.waitForFinished(-1);
#endif
        }
    }
#else
    //
#endif

    return 1;
}

QString FormMain::getMaintenceTool()
{
    QDir dir = QDir(qApp->applicationDirPath());
    dir.cdUp();
    QString tool;
#ifdef _WIN32
    tool = dir.path() + "/MaintenanceTool_Windows.exe";
#else
    tool = dir.path() + "/MaintenanceTool_Linux";
#endif
    DBG("getMaintenceTool=%s\n", qPrintable(tool));
    return tool;
}

bool FormMain::checkUpdate(QString &hint)
{
    hint.clear();
    bool updatesAvailable = false;
    do {
        QProcess process;

        process.start(QString("\"%1\" -v --checkupdates").arg(getMaintenceTool()));

        process.waitForFinished();

        if (process.error() != QProcess::UnknownError) {
            DBG("Checking updates error. %s\n", qPrintable(process.errorString()));
            hint = process.errorString();
            break;
        }

        QByteArray out = process.readAllStandardOutput();
        DBG("Checking updates out=<%s>\n", out.data());

        if (out.isEmpty() || !out.contains("Operations sanity check succeeded")) {
            DBG("Check failed.\n");
            hint = out;
            break;
        }

        if (out.contains("Network error while downloading")) {
            DBG("Network error.\n");
            break;
        }

        if (out.contains("There are currently no updates available")) {
            DBG("No updates available.\n");
            break;
        }

        updatesAvailable = true;
    } while (0);

    return updatesAvailable;
}

#ifdef USE_JUNBO
void FormMain::slot_mmpackage_added(const QString &packageName) {
    QString ret;
    if(!controller->appconfig->opername.isEmpty()) {
        ret += tr("operator:")+controller->appconfig->opername;
    }

    if(!packageName.isEmpty()) {
        ret += tr(" Loaded MM Package: ");
        ret += packageName;
    }
    ui->label_operator->setText(ret);
}
#endif

FormMain::~FormMain()
{
    controller->adbTracker->freeMem();
    controller->appconfig->saveCfg();
    delete ui;
}

void FormMain::mousePressEvent(QMouseEvent *event) {
    if(isMaxWindow) {
        return;
    }

    // current
    int x = event->pos().x();
    int y = event->pos().y();

    // top left
    int x1 = ui->layout_header->pos().x();
    int y1 = ui->layout_header->pos().y();

    // bottom right
    int x2 = x1 + ui->layout_header->width();
    int y2 = y1 + ui->layout_header->height();

    if(x > x1 && x < x2 && y > y1 && y < y2) {
        lastMousePos = event->pos();
        isDraggingWindow = true;
    }
}

void FormMain::mouseMoveEvent(QMouseEvent *event) {
    if(isDraggingWindow) {
        this->move(event->globalPos()-lastMousePos);
    }
}

void FormMain::mouseReleaseEvent(QMouseEvent *event) {
    isDraggingWindow = false;
}

void FormMain::closeEvent(QCloseEvent *e) {
    static bool isQuiting = false;
    e->ignore();
    if(!isQuiting) {
        isQuiting = true;
        slot_quitApp();
    }
}

void FormMain::slot_winMaxRestore() {
    static QPoint lastPos;
    static QSize lastSize;
    if(isMaxWindow) {
        move(lastPos);
        resize(lastSize);
    } else {
        lastPos = mapToGlobal(rect().topLeft());
        lastSize = size();

        int screen = qApp->desktop()->screenNumber(this);
        QRect rect = qApp->desktop()->availableGeometry(screen);
        move(rect.topLeft());
        resize(rect.size());
    }
    isMaxWindow = !isMaxWindow;
}

void FormMain::slot_feedback() {
    DialogFeedback *dlg = new DialogFeedback(this);
    dlg->exec();
    delete dlg;
}

void FormMain::slot_setup() {
    DialogSetup *dlg = new DialogSetup(this);
    connect(dlg, SIGNAL(signal_WiFiConfigChanged()), SIGNAL(signal_WiFiConfigChanged()));
    connect(dlg, SIGNAL(signal_LANConfigChanged()), SIGNAL(signal_LANConfigChanged()));
    dlg->exec();
    delete dlg;
}

void FormMain::slot_quitApp() {
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

        DialogWait *dlg = new DialogWait(this);
        dlg->setText(tr("please wait..."));
        dlg->exec();
    }
}

void FormMain::slot_dmIdle() {
    controller->threadpool->removeThread((QThread *)controller->downloadmanager);
}

void FormMain::slot_autoCheckUpdates()
{
    isAutoCheckUpdates = true;
    ui->btn_checkUpdates->setEnabled(false);

    ThreadBase<FormMain> *t = new ThreadBase<FormMain>(this, &FormMain::checkUpdates);
    t->start(NULL, NULL, NULL);
}

void FormMain::slot_checkTC(){
    AppConfig *config = GlobalController::getInstance()->appconfig;
    if(!config->core_loadAllData()){
        DBG("error load bundles");
        return;
    }
    config->checkAndCleanDownload();
}

void FormMain::slot_showBuildTime()
{
    ui->label_buildTime->setText(MMQT2_DATE_TIME);
    ui->label_buildTime->show();
}

void FormMain::slot_checkUpdates()
{
    ui->btn_checkUpdates->setEnabled(false);
    isAutoCheckUpdates = false;

    DialogProgress *dlg = new DialogProgress(this);
    connect(this, SIGNAL(singal_checkUpdateResult(bool)), dlg, SLOT(close()));
    dlg->slot_setState(tr("fastman2 checking updates now"));
    dlg->setStopable(false);
    dlg->slot_setProgress(0, 0);

    ThreadBase<FormMain> *t = new ThreadBase<FormMain>(this, &FormMain::checkUpdates);
    t->start(NULL, NULL, NULL);

    dlg->exec();
}

void FormMain::slot_checkUpdatesResult(bool succeed)
{
    ui->btn_checkUpdates->setEnabled(true);

    if (isAutoCheckUpdates && !succeed) {
        DBG("Auto check updates failed.\n");
        return;
    }

    if (succeed) {
        int ret = QMessageBox::question(this, tr("fastman2"), tr("Updates available, do you update now?"),
                                        QMessageBox::Ok, QMessageBox::No);
        if (QMessageBox::Ok == ret) {
            QStringList args("--updater");
#ifdef _Win32
            bool success = QProcess::startDetached(QString("\"%1\"").arg(getMaintenceTool()), args);
#else
            bool success = QProcess::startDetached(QString("%1").arg(getMaintenceTool()), args);
#endif
            DBG("Start updater ret=<%d>\n", success);
            if (success) {
                qApp->closeAllWindows();
                slot_quitApp();
            } else {
                QMessageBox::information(this, tr("fastman2"), tr("Start updater failed."));
            }
        }
    } else {
        if (!checkUpdatesFailed) {
            QMessageBox::information(this, tr("fastman2"), tr("No updates available. you are using the latest fastman2."));
        } else {
            QMessageBox::information(this, tr("fastman2"), tr("Check updates failed"));
        }
    }
}

void FormMain::slot_adbError(const QString &hint, int pid) {
    if(pid < 0) {
        //QMessageBox::warning(this, tr("Adb error"), hint, QMessageBox::Ok);
    } else {
        // if(QMessageBox::Yes == QMessageBox::question(this, tr("Adb error"), hint,
        //                                              QMessageBox::Yes,
        //                                              QMessageBox::No)) {
#ifdef _WIN32
        HANDLE p = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, pid);
        if(p != NULL) TerminateProcess(p, 0);
#else
        kill(pid, SIGTERM);
#endif
        //}
    }
    QTimer::singleShot(1000, controller->adbTracker, SLOT(start()));
}

void FormMain::slot_critical(const QString &hint) {
    QMessageBox::warning(this, tr("Critical error"), hint, QMessageBox::Ok);
    slot_quitApp();
}

void FormMain::switchNav(QToolButton *btn) {
    btn->setChecked(true);
    QWidget *curPage = ui->stw_main->currentWidget();
    QToolButton *curBtn = nav_pages_map.key(curPage);
    if(curBtn != btn) {
        curBtn->setChecked(false);
        ui->stw_main->setCurrentWidget(nav_pages_map.value(btn));
    }
}

void FormMain::slot_switchBoard() {
    switchNav(ui->nav_btn_board);
}

void FormMain::slot_switchInstallation() {
    switchNav(ui->nav_btn_installation);
}

void FormMain::slot_switchBundles() {
    switchNav(ui->nav_btn_bundles);
}

void FormMain::slot_switchDownloads() {
    switchNav(ui->nav_btn_download);
}

void FormMain::slot_switchMyAccount() {
    switchNav(ui->nav_btn_account);
}

void FormMain::slot_switchTools() {
    switchNav(ui->nav_btn_tools);
}

void FormMain::slot_flipNavBoard(bool checked) {
    if(checked) {
        ui->nav_btn_board->setIcon(QIcon(":/skins/skins/nav_ic_board_sel.png"));
    } else {
        ui->nav_btn_board->setIcon(QIcon(":/skins/skins/nav_ic_board.png"));
    }
}

void FormMain::slot_flipNavInstallation(bool checked) {
    if(checked) {
        ui->nav_btn_installation->setIcon(QIcon(":/skins/skins/nav_ic_installation_sel.png"));
    } else {
        ui->nav_btn_installation->setIcon(QIcon(":/skins/skins/nav_ic_installation.png"));
    }
}

void FormMain::slot_flipNavBundles(bool checked) {
    if(checked) {
        ui->nav_btn_bundles->setIcon(QIcon(":/skins/skins/nav_ic_bundles_sel.png"));
    } else {
        ui->nav_btn_bundles->setIcon(QIcon(":/skins/skins/nav_ic_bundles.png"));
    }
}

void FormMain::slot_flipNavDownloads(bool checked) {
    if(checked) {
        ui->nav_btn_download->setIcon(QIcon(":/skins/skins/nav_ic_downloads_sel.png"));
    } else {
        ui->nav_btn_download->setIcon(QIcon(":/skins/skins/nav_ic_downloads.png"));
    }
}

void FormMain::slot_flipNavMyAccount(bool checked) {
    if(checked) {
        ui->nav_btn_account->setIcon(QIcon(":/skins/skins/nav_ic_account_sel.png"));
    } else {
        ui->nav_btn_account->setIcon(QIcon(":/skins/skins/nav_ic_account.png"));
    }
}

void FormMain::slot_flipNavTools(bool checked) {
    if(checked) {
        ui->nav_btn_tools->setIcon(QIcon(":/skins/skins/nav_ic_tools_sel.png"));
    } else {
        ui->nav_btn_tools->setIcon(QIcon(":/skins/skins/nav_ic_tools.png"));
    }
}

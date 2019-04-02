#include "dialoglogin.h"

#ifndef NO_GUI
#include "ui_dialoglogin.h"
#include <QKeyEvent>
#include <QMessageBox>
#include <QMovie>
#include <QApplication>
#include <QProcess>
#include "dialogsetup.h"
#endif

#include <QEventLoop>
#include "loginmanager.h"
#include "globalcontroller.h"
#include "appconfig.h"
#include "formmain.h"

static QString fromGBK(const QByteArray& data) {
    QTextCodec *c = QTextCodec::codecForName("GB2312");
    if(c == NULL) {
        return QString();
    }
    return c->toUnicode(data);
}

LoginThread::LoginThread(const QString &username, const QString &password, bool remember, bool offline, bool CMCCFirst) {
    this->username = username;
    this->password = password;
    this->remember = remember;
    this->offline = offline;
    this->CMCCFirst = CMCCFirst;

    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

void LoginThread::run() {
    AppConfig *config = GlobalController::getInstance()->appconfig;
    LoginManager *loginmanager = LoginManager::getInstance();
    QString hint;
    bool ret = false;

    config->offlineMode = offline;
    config->CMCCFirst = CMCCFirst;

    if(config->CMCCFirst){
        config->sUrl = "http://ydsjapk.dengxian.net";
    }else{
        config->sUrl = "http://sjapk.dengxian.net";
    }

    do {
        // network
        if (!config->offlineMode) {
            emit signal_loginProgress(1, 10, tr("detecting network..."));
            if (!loginmanager->initUrlRevoke(hint, CMCCFirst)) {
                break;
            }
        }

        // update
        if (!config->offlineMode) {
            emit signal_loginProgress(2, 10, tr("detecting update..."));
            QString checkHint;
            if (FormMain::checkUpdate(checkHint)) {
                emit signal_hasUpdate();
            } else {
                if (!checkHint.isEmpty()) {
                    emit signal_showHint(tr("Check updates failed, please reinstall.\n"
                                            "%1").arg(checkHint));
                    QEventLoop e;
                    connect(this, SIGNAL(signal_showHintFinisehd()), &e, SLOT(quit()));
                    e.exec();
                }
            }
        }

        // login
        if (!config->offlineMode) {
            emit signal_loginProgress(3, 10, tr("logging..."));
            if (password == config->encpasswd) {
                if (!loginmanager->core_login2(username, password, hint)) {
                    loginmanager->resetUrlRevoke();
                    break;
                }
            } else {
                if (!loginmanager->core_login(username, password, hint)) {
                    loginmanager->resetUrlRevoke();
                    break;
                }
            }
            loginmanager->initUrlRevoke(hint);  // 登录成功后再次寻找最优host

            loginmanager->getLoginInfo(config->username, config->encpasswd);
            if (!remember) {
                config->encpasswd.clear();
            }
        } else {
            if (!loginmanager->core_login_offline(username, password, hint)) {
                break;
            }
            loginmanager->getLoginInfo(config->username, config->encpasswd);
        }

        //clean xml file
        config->cleanupFile();

        // bundle
        emit signal_loginProgress(4, 10, tr("loading bundle data..."));
        if (!config->core_loadAllData()) {
            hint = tr("error load bundles!");
            break;
        }

        // properties
        if (!config->offlineMode) {
            emit signal_loginProgress(5, 10, tr("loading install properties..."));

            QProcess process;
            QString path = qApp->applicationDirPath() + "/DataFromServer";
        #ifdef _WIN32
            QStringList args;
            args << "/c" << "rd" << "/s" << "/q" << path.replace("/", "\\");
            process.start("cmd", args);
            process.waitForFinished(-1);
        #else
            process.start(QString("rm -rf %1").arg(path));
            process.waitForFinished(-1);
        #endif

            if (!(config->getInstallFlags()
                  && config->getDesktopAdaptiveList()
                  && config->getSelfRunApps()
                  && config->getThemes()
                  && config->getAppActivate()
                  && config->getInstallFailureMaxRate()
                  && config->getReddot()
                  && config->getDesktop())) {
                hint = tr("error load install flags!");
                break;
            }
        }

        // upload
        if (!config->offlineMode) {
            QList<InstLog *> list = config->instCacheDb->getAllData();
            int i = 0;
            int valid = 0;
            int fail = 0;
            int total = list.size();
            int r = -1;
            QString resp;
            while (!list.isEmpty()) {
                InstLog *d = list.takeFirst();
                emit signal_loginProgress(6, 10, tr("uploading cached install log %1 of %2").arg(++i).arg(total));
                r = loginmanager->core_uploadInstLog(d->url, resp);
                if ((r == LoginManager::ERR_NOERR)
                        || (r == LoginManager::ERR_CUSTOM)) {
                    d->result = resp;
                    config->instLogDb->addData(d);
                    config->instCacheDb->removeData(d);
                    valid++;
                } else {
                    DBG("offline upload faile:%s\n", qPrintable(resp));
                    fail++;
                }
                delete d;
            }
            if (total > 0) {
                hint += tr("\nuploaded %1 of %2 cached install log, %3 failed, use log viewer to check")
                        .arg(valid).arg(total).arg(fail);
            }
        }

        // download
        emit signal_loginProgress(7, 10, tr("checking and cleaning download ..."));
        if (!config->checkAndCleanDownload()) {
            hint = tr("all items must be exsits in local if offline install");
            break;
        }

        ret = true;
    } while(0);

    emit signal_loginDone(ret, hint);
}

#ifdef NO_GUI
bool LoginThread::core_login(const QString &username, const QString &password, bool remember, bool offline, QString &hint) {
    LoginThread *t = new LoginThread(username, password, remember, offline);
    QEventLoop e;
    t->_ret = false;
    t->_hint.clear();

    connect(t, SIGNAL(signal_loginProgress(int,int,QString)), t, SLOT(slot_loginProgress(int,int,QString)));
    connect(t, SIGNAL(signal_loginDone(bool,QString)), t, SLOT(slot_loginDone(bool,QString)));
    connect(t, SIGNAL(finished()), &e, SLOT(quit()));
    t->start();
    e.exec();

    hint = t->_hint;
    return t->_ret;
}

void LoginThread::slot_loginProgress(int value, int max, const QString &hint) {
    PRT(tr("slot_loginProgress %1/%2 %3").arg(value).arg(max).arg(hint));
}

void LoginThread::slot_loginDone(bool ret, const QString &hint) {
    _ret = ret;
    _hint = hint;
    PRT(tr("slot_loginDone: %1, %2").arg(ret).arg(hint));
}
#else

DialogLogin::DialogLogin(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint),
    ui(new Ui::DialogLogin)
{
    setAttribute(Qt::WA_TranslucentBackground);
    ui->setupUi(this);

    config = GlobalController::getInstance()->appconfig;
    loading = false;
    isDraggingWindow = false;

    ui->horizontalLayout_header->setAlignment(ui->btn_feedback,Qt::AlignTop);
    ui->horizontalLayout_header->setAlignment(ui->btn_settings, Qt::AlignTop);
    ui->horizontalLayout_header->setAlignment(ui->btn_close, Qt::AlignTop);

    ui->label_icon->setParent(this);
    ui->label_icon->resize(78, 78);
    ui->label_icon->move(32, 28);
    ui->label_version->setText(QString(MMQT2_VER_DATE_TIME).toUpper());
    mov = new QMovie(":/skins/skins/bg_loading.gif");
    mov->setSpeed(50);
    ui->label_loading->setMovie(mov);

#if 1
    ui->btn_feedback->hide();
#else
    connect(ui->btn_feedback,SIGNAL(clicked()),SLOT(slot_feedback()));
#endif
    connect(ui->btn_settings, SIGNAL(clicked()), SLOT(slot_settings()));
    connect(ui->btn_login, SIGNAL(clicked()), SLOT(slot_login()));

    connect(ui->lineEdit_password, SIGNAL(returnPressed()), SLOT(slot_login()));

    ui->lineEdit_username->setText(config->username);
    if (!config->encpasswd.isEmpty()) {
        ui->lineEdit_password->setText(config->encpasswd);
        ui->chk_remember->setChecked(true);
        ui->btn_login->setFocus();
    } else {
        ui->lineEdit_username->selectAll();
        ui->lineEdit_username->setFocus();
    }

    ui->chk_offline->setEnabled(ui->chk_remember->isChecked());
    ui->chk_offline->setChecked(ui->chk_remember->isChecked() && config->offlineMode);
    toggleOffline(ui->chk_offline->isChecked());
    connect(ui->chk_offline, SIGNAL(toggled(bool)), SLOT(toggleOffline(bool)));

    ui->chk_CMCCServer->setChecked(config->CMCCFirst);

    // TEMP
    ui->btn_settings->hide();
}

DialogLogin::~DialogLogin()
{
    delete ui;
}

void DialogLogin::mousePressEvent(QMouseEvent *event) {
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

void DialogLogin::mouseMoveEvent(QMouseEvent *event) {
    if(isDraggingWindow) {
        this->move(event->globalPos()-lastMousePos);
    }
}

void DialogLogin::mouseReleaseEvent(QMouseEvent *event) {
    isDraggingWindow = false;
}

void DialogLogin::keyPressEvent(QKeyEvent *e) {
    if(loading) {
        e->ignore();
    } else {
        e->accept();
    }
}

void DialogLogin::closeEvent(QCloseEvent *e) {
    if(loading) {
        e->ignore();
    } else {
        e->accept();
    }
}

void DialogLogin::slot_showHint(const QString &hint)
{
    QMessageBox::question(this, tr("fastman2"), hint, QMessageBox::Ok);
    emit signal_showHintFinished();
}

void DialogLogin::slot_hasUpdate()
{
    int ret = QMessageBox::question(this, tr("fastman2"), tr("Updates available, do you update now?"),
                                    QMessageBox::Ok, QMessageBox::No);
    if (QMessageBox::Ok == ret) {
        QStringList args("--updater");
#ifdef _WIN32
        bool success = QProcess::startDetached(QString("\"%1\"").arg(FormMain::getMaintenceTool()), args);
#else
        bool success = QProcess::startDetached(QString("%1").arg(FormMain::getMaintenceTool()), args);
#endif
        DBG("Start updater ret=<%d>\n", success);
        if (success) {
            qApp->closeAllWindows();
            qApp->quit();
        } else {
            QMessageBox::information(this, tr("fastman2"), tr("Start updater failed."));
        }
    }
}

void DialogLogin::slot_loginProgress(int value, int max, const QString &hint) {
    ui->pb_loading->setValue(value);
    ui->pb_loading->setMaximum(max);
    ui->label_hint->setText(hint);
}

void DialogLogin::slot_loginDone(bool ret, const QString& hint) {
    mov->stop();
    loading = false;

    if (ret) {
        if(!hint.isEmpty()) {
            QMessageBox::information(this, tr("hint"), hint, QMessageBox::Ok);
        }
        emit signal_loginSucceed();
        config->loggedIn = true;
        config->saveCfg();
    } else {
        ui->stw_login->setCurrentIndex(0);
        if(!hint.isEmpty()) {
            QMessageBox::information(this, tr("hint"), hint, QMessageBox::Ok);
        }
    }
}

static bool isOperNameValid(const QString& name) {
    if(name.isEmpty()) {
        return true;
    }
    QRegExp reg("[0-9a-z]+", Qt::CaseInsensitive);
    return reg.exactMatch(name);
}

void DialogLogin::slot_login() {
    QString user = ui->lineEdit_username->text().trimmed();
    QString pass = ui->lineEdit_password->text().trimmed();
    QString opname = ui->lineEdit_opername->text().trimmed();

    if(user.isEmpty() || pass.isEmpty()) {
        QMessageBox::information(this, tr("hint"),
                                 tr("username or password should not be empty!"),
                                 QMessageBox::Ok);
        return;
    }

    if(!isOperNameValid(opname)) {
        QMessageBox::information(this, tr("hint"),
                                 tr("opname should be empty, alphabet or number!"),
                                 QMessageBox::Ok);
        return;
    } else {
        config->opername = opname;
    }

    bool remember = ui->chk_remember->isChecked();
    bool offline = ui->chk_offline->isChecked();
    bool CMCCFirst = ui->chk_CMCCServer->isChecked();

    ui->stw_login->setCurrentIndex(1);
    mov->start();
    loading = true;

    LoginThread *t = new LoginThread(user, pass, remember, offline, CMCCFirst);
    connect(t, SIGNAL(signal_showHint(QString)), SLOT(slot_showHint(QString)));
    connect(this, SIGNAL(signal_showHintFinished()), t, SIGNAL(signal_showHintFinisehd()));
    connect(t, SIGNAL(signal_hasUpdate()), SLOT(slot_hasUpdate()), Qt::BlockingQueuedConnection);
    connect(t, SIGNAL(signal_loginProgress(int,int,QString)), SLOT(slot_loginProgress(int,int,QString)));
    connect(t, SIGNAL(signal_loginDone(bool,QString)), SLOT(slot_loginDone(bool,QString)));
    t->start();
}

void DialogLogin::slot_settings() {
    DialogSetup *dlg = new DialogSetup(this);
    dlg->exec();
    delete dlg;
}

void DialogLogin::slot_feedback(){
    DialogFeedback *dlg = new DialogFeedback(this);
    dlg->exec();
    delete dlg;
}

void DialogLogin::toggleOffline(bool checked) {
    if (checked) {
        ui->lineEdit_password->setEnabled(false);
        ui->chk_remember->setChecked(true);
        ui->chk_remember->setEnabled(false);
    } else {
        ui->lineEdit_password->setEnabled(true);
        ui->chk_remember->setEnabled(true);
    }
}

#endif

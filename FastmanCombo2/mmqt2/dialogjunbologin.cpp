#include "dialogjunbologin.h"
#include "ui_dialogjunbologin.h"
#include "junbointerface.h"
#include <QCloseEvent>
#include <QMessageBox>
#include "dialogwait.h"

JunboLoginThread::JunboLoginThread(const QString &user, const QString &password, bool remember, QObject *parent)
    : QThread(parent) {
    this->user = user;
    this->pass = password;
    this->remember = remember;

    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

void JunboLoginThread::run() {
    bool ret = JunboInterface::getInstance()->mmLogin(
                user, pass, remember);
    emit signalDone(ret);
}

DialogJunboLogin::DialogJunboLogin(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogJunboLogin)
{
    ui->setupUi(this);
    connect(ui->btn_login, SIGNAL(clicked()), SLOT(slot_login()));

    JunboInterface *interf = JunboInterface::getInstance();
    interf->loadCfg();
    ui->lineEdit_username->setText(interf->savedUsername);
    ui->lineEdit_password->setText(interf->savedPassword);
    ui->chk_remember->setChecked(!interf->savedPassword.isEmpty());
}

DialogJunboLogin::~DialogJunboLogin()
{
    delete ui;
}

void DialogJunboLogin::slot_login() {
    QString username = ui->lineEdit_username->text();
    QString password = ui->lineEdit_password->text();

    if(username.isEmpty() || password.isEmpty()) {
        QMessageBox::information(this, tr("hint"), tr("username or password should not be empty!"));
        return;
    }

    DialogWait *dlg = new DialogWait(this);
    dlg->setText(tr("please wait..."));

    JunboLoginThread *t = new JunboLoginThread(username, password,
                                               ui->chk_remember->isChecked(),
                                               this);
    connect(t, SIGNAL(signalDone(bool)), SLOT(slot_loginDone(bool)));
    connect(t, SIGNAL(finished()), dlg, SLOT(accept()));
    t->start();
    dlg->exec();
    delete dlg;
}

void DialogJunboLogin::slot_loginDone(bool ret) {
    if(ret) {
        accept();
        return;
    }
    QMessageBox::information(this, tr("hint"), tr("login failed!"));
}

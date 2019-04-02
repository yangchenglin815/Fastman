#include "dialogfeedback.h"
#include "ui_dialogfeedback.h"
#include "globalcontroller.h"
#include "loginmanager.h"
#include "zlog.h"

DialogFeedback::DialogFeedback(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogFeedback)
{
    ui->setupUi(this);

    QUrl url("http://zj.dengxian.net/v2/feedback.aspx");
    GlobalController::getInstance()->loginManager->revokeUrl(url);
    LoginManager::getInstance()->fillUrlParams(url);
    ui->formWeb->setHomeUrl(url);
}

DialogFeedback::~DialogFeedback()
{
    delete ui;
}


#include "formwebapps.h"
#include "ui_formwebapps.h"
#include "loginmanager.h"
#include "appconfig.h"
#include "zstringutil.h"
#include "zdownloadmanager.h"
#include <QMessageBox>
#include <QTimer>
#include <QFileInfo>
#include "qapplication.h"

FormWebApps::FormWebApps(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormWebApps),
    sum(0),
    sumsize(0)
{
    ui->setupUi(this);
    controller = GlobalController::getInstance();

    QUrl url("http://zj.dengxian.net/v2/app.aspx");
    controller->loginManager->revokeUrl(url);
    controller->loginManager->fillUrlParams(url);
    ui->formWeb->setHomeUrl(url);

    QTimer *t = new QTimer(this);
    t->start(300);

    connect(t,SIGNAL(timeout()),this,SLOT(slot_displaySum()));
    connect(ui->btn_download,SIGNAL(clicked()),this,SLOT(slot_downApps()));
    connect(this,SIGNAL(signal_notice(int,int,int)),this,SLOT(slot_notice(int,int,int)));
}

void FormWebApps::GetSumSize(const QWebElement& element) {
    QStringList attrs = element.attributeNames();
    foreach(QString attr, attrs) {
        if(attr == QString::fromUtf8("file_size")) {
            sumsize = sumsize + element.attribute(attr).toInt();
        }
    }
}

void FormWebApps::slot_displaySum(){
    QWebFrame *frame = ui->formWeb->getWebView()
            ->page()->currentFrame();
    if(frame == NULL) {
        return ;
    }
    sumsize = 0;
    QList<QWebElement> elements;
    QWebElementCollection elementlist = frame->findAllElements("img[resource_id]");
    foreach (QWebElement tmp, elementlist) {
        if (tmp.attribute("resource_checked").toInt() != 1 || !tmp.hasAttribute("resource_id")) {
            continue;
        }
        elements.append(tmp);
        GetSumSize(tmp);
    }
    sum = elements.size();
    ui->label_summary->setText(tr("selected %1 apps, total size %2").arg(sum).arg(ZStringUtil::getSizeStr(sumsize)));

    slot_getwebInfo(elements);
}

void FormWebApps::slot_getwebInfo(QList<QWebElement> tmp){
    elementinfo.clear();
    elementinfo.append(tmp);
}

void FormWebApps::slot_downApps() {
    QString qstr;
    int sum = 0,filesize = 0;
    foreach (QWebElement tmp, elementinfo) {
        qstr = tmp.attribute("resource_id");
        BundleItem *item = controller->appconfig->globalBundle->getApp(qstr);
        QStringList list = item->id.split("_");
        item->id = list.at(0);
        if(item == NULL) {
            item = new BundleItem();
            item->id = qstr;
            controller->appconfig->globalBundle->addApp(item);
        }
        item->name = tmp.attribute("resource_name");
        item->down_url = tmp.attribute("download_url");
        item->instLocation = tmp.attribute("installsite").toInt();
        item->md5 = tmp.attribute("md5_sum");
        item->pkg = tmp.attribute("package_name");
        item->size = tmp.attribute("file_size").toULongLong();
        item->version = tmp.attribute("version_name");
        item->versionCode = tmp.attribute("version_code").toInt();

        DBG("<%s> status %d\n", item->name.toLocal8Bit().data(), item->download_status);       
        item->path = controller->appconfig->downloadDirPath + "/" + item->pkg + item->version + ".apk";
        if(item->download_status != ZDownloadTask::STAT_FINISHED) {
            ZDownloadTask *task = controller->downloadmanager->getTask(item->id);
            controller->loginManager->revokeUrlString(item->down_url);
            if(task == NULL) {
                sum++;
                filesize+=item->size;
                controller->downloadmanager->addTask(item->id, item->down_url, item->path);
            } else {
                task->url = item->down_url;
            }
        } else {
            QFileInfo f(item->path);
            if(f.size() != item->size && !item->down_url.isEmpty()) {
                item->download_status = ZDownloadTask::STAT_PENDING;
                controller->loginManager->revokeUrlString(item->down_url);
                controller->downloadmanager->addTask(item->id, item->down_url, item->path);
            }
        }
    }
    emit signal_notice(elementinfo.size(),elementinfo.size()-sum,filesize);
}

void FormWebApps::slot_notice(int a, int b, int c) {
    QMessageBox::about(this,tr("notice"),
                       tr("selected %1 apps, %2 already exist, added %3 apps, total size %4.")
                                            .arg(a).arg(b).arg(a - b).arg(ZStringUtil::getSizeStr(c)));
}

FormWebApps::~FormWebApps()
{
    delete ui;
}

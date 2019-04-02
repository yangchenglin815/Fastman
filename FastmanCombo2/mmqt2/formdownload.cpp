#include "formdownload.h"
#include "ui_formdownload.h"
#include "dialogadddownload.h"
#include "globalcontroller.h"
#include "zstringutil.h"
#include "zdmhttp.h"
#include "bundle.h"
#include "appconfig.h"
#include <QDomDocument>
#include <QCryptographicHash>
#include <QFile>
#include <QTimer>
#include <QPainter>
#include <QModelIndex>
#include <QStyleOptionProgressBarV2>
#include <QRect>
#include <QDateTime>
#include <stdio.h>
ProgressDelegate::ProgressDelegate(QTreeView *tv, QObject *parent) : QStyledItemDelegate(parent)
{
    m_tv = tv;
}

void ProgressDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {

    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() == DownloadModel::COL_PROGRESS) {
        const QAbstractItemModel *itemModel = index.model();

        int progress = itemModel->data(index, Qt::UserRole + 2).toLongLong() / 4096;
        int maximum = itemModel->data(index, Qt::UserRole + 3).toLongLong() / 4096;
        int ratio = 0;
        if (progress == 0 || maximum == 0) {
            maximum = 100;
        } else {
            ratio = 100 * ((float)progress / maximum);
        }

        QStyleOptionProgressBarV2  progressBarOption;
        QRect cRect = option.rect;
        cRect.setHeight(20);
        cRect.setY(option.rect.y() + (option.rect.height() - cRect.height()) / 2);
        cRect.setHeight(20); // Is confused ?
        progressBarOption.rect = cRect;
        progressBarOption.minimum = 0;
        progressBarOption.maximum = maximum;
        progressBarOption.progress = progress;
        progressBarOption.text = QString::number(ratio > 100 ? 100 : ratio) + "%";
        progressBarOption.textAlignment = Qt::AlignCenter;
        progressBarOption.textVisible = true;

        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
    }
}

DownloadModel::DownloadModel(Bundle *baseBundle, ZDownloadManager *dm, QObject *parent) :
    QStandardItemModel(parent){
    this->baseBundle = baseBundle;
    this->dm = dm;

    setColumnCount(COL_ACTION + 1);
    setSortRole(Qt::UserRole+1);

    QStringList labels;
    labels.append(tr("name"));
    labels.append(tr("size"));
    labels.append(tr("progress"));
    labels.append(tr("speed"));
    labels.append(tr("status"));
    labels.append(tr("action"));
    setHorizontalHeaderLabels(labels);
    //testThread.setPriority(QThread::LowestPriority);
    //testThread.start();
}
void DownloadModel::setDownloadData(ZDownloadTask *task) {
    mutex.lock();
    int row = -1;
    int i = 0;
    QStandardItem *item = NULL;

    while((item = this->item(i++, COL_NAME)) != NULL) {
        if(item->data(Qt::UserRole).toString() == task->id) {
            row = --i;
            break;
        }
    }
    if(row == -1) {
        row = rowCount();
        insertRow(row);
    }

    if(checkList.contains(task->id)) {
        setData(index(row, COL_NAME), Qt::Checked, Qt::CheckStateRole);
    } else {
        setData(index(row, COL_NAME), Qt::Checked, Qt::CheckStateRole);
    }

    BundleItem *app = baseBundle->getApp(task->id);
    int status_int = app != NULL ? app->download_status : task->status;
    QString status_str;
    int action_mode = ACTION_NONE;
    switch(status_int) {
    case ZDownloadTask::STAT_PENDING:
        status_str = tr("pending");
        action_mode = ACTION_PAUSE;
        break;
    case ZDownloadTask::STAT_DOWNLOADING:
        status_str = tr("downloading");
        action_mode = ACTION_PAUSE;
        break;
    case ZDownloadTask::STAT_STOPPED:
        status_str = tr("stopped");
        action_mode = ACTION_START;
        break;
    case ZDownloadTask::STAT_FAILED:
        status_str = tr("failed, ");
        status_str.append(QString::number(task->result));
        action_mode = ACTION_START;
        break;
    case ZDownloadTask::STAT_FINISHED:
        status_str = tr("finished");
        break;
    default:
        status_str = tr("unknown");
        break;
    }

    setData(index(row, COL_NAME), task->id, Qt::UserRole);

    setData(index(row, COL_STAT), status_str);
    setData(index(row, COL_STAT), status_int, Qt::UserRole+1);

    setData(index(row, COL_PROGRESS), task->progress/(float)task->size, Qt::UserRole+1);
    setData(index(row, COL_PROGRESS), task->progress, Qt::UserRole + 2);
    setData(index(row, COL_PROGRESS), task->size, Qt::UserRole + 3);

    if(app != NULL) {
        setData(index(row, COL_NAME), app->name);
        setData(index(row, COL_NAME), ZStringUtil::getPinyin(app->name), Qt::UserRole+1);

        setData(index(row, COL_SIZE), ZStringUtil::getSizeStr(app->size));
        setData(index(row, COL_SIZE), app->size, Qt::UserRole+1);

        if(app->icon.isNull()) {
            this->item(row, COL_NAME)->setIcon(QIcon(":/skins/skins/icon_downloading_app.png"));
        } else {
            this->item(row, COL_NAME)->setIcon(QIcon(QPixmap::fromImage(app->icon)));
        }
    } else {
        setData(index(row, COL_NAME), task->path);
        setData(index(row, COL_NAME), ZStringUtil::getPinyin(task->path), Qt::UserRole+1);

        setData(index(row, COL_SIZE), ZStringUtil::getSizeStr(task->size));
        setData(index(row, COL_SIZE), task->size, Qt::UserRole+1);

        this->item(row, COL_NAME)->setIcon(QIcon(":/skins/skins/icon_downloading_app.png"));
    }

    if(action_mode == ACTION_START) {
        setData(index(row, COL_ACTION), tr("start"));
        this->item(row, COL_ACTION)->setIcon(QIcon(":/skins/skins/icon_start.png"));
    } else if(action_mode == ACTION_PAUSE) {
        setData(index(row, COL_ACTION), tr("pause"));
        this->item(row, COL_ACTION)->setIcon(QIcon(":/skins/skins/icon_pause.png"));
    } else {
        setData(index(row, COL_ACTION), "");
        this->item(row, COL_ACTION)->setIcon(QIcon());
    }
    setData(index(row, COL_ACTION), action_mode, Qt::UserRole+1);

    mutex.unlock();
}

void DownloadModel::setDownloadSpeed(ZDownloadTask *task, int bytePerSecond) {
    mutex.lock();
    int row = -1;
    int i = 0;
    QStandardItem *item = NULL;
    while((item = this->item(i++, COL_NAME)) != NULL) {
        if(item->data(Qt::UserRole).toString() == task->id) {
            row = --i;
            break;
        }
    }

    if(row != -1) {
        setData(index(row, COL_SPEED), ZStringUtil::getSizeStr(bytePerSecond)+"/S");
        if(bytePerSecond==0)//下载速度为0时
        {
            if(task)
            {
                //testThread.addTask(task);
            }
        }
    }
    mutex.unlock();
}

void DownloadModel::slot_clicked(const QModelIndex &index) {
    QStandardItem *item = this->item(index.row(), 0);
    QString id = item->data(Qt::UserRole).toString();
    if(index.column() == COL_ACTION) {
        int action_mode = this->itemFromIndex(index)->data(Qt::UserRole+1).toInt();
        if(action_mode == ACTION_START) {
            dm->startTask(dm->getTask(id));
        } else if(action_mode == ACTION_PAUSE) {
            dm->stopTask(dm->getTask(id));
        }
        return;
    }

    if(item->checkState() == Qt::Unchecked) {
        item->setCheckState(Qt::Checked);
        checkList.append(id);
    } else {
        item->setCheckState(Qt::Unchecked);
        checkList.removeOne(id);
    }
}

void DownloadModel::slot_selAll() {
    int i = 0;
    QStandardItem *item = NULL;
    checkList.clear();

    while((item = this->item(i++, 0)) != NULL) {
        item->setCheckState(Qt::Checked);
        checkList.append(item->data(Qt::UserRole).toString());
    }
}

void DownloadModel::slot_selRev() {
    int i = 0;
    QStandardItem *item = NULL;
    checkList.clear();

    while((item = this->item(i++, 0)) != NULL) {
        if(item->checkState() == Qt::Unchecked) {
            item->setCheckState(Qt::Checked);
            checkList.append(item->data(Qt::UserRole).toString());
        } else {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

void TestThread::OnFinished(QNetworkReply * reply)
{
    QNetworkReply::NetworkError error = reply->error();
    QUrl url = reply->url();
    QString strurl = url.toString();
    QString strerro = reply->errorString();
    DBG("验证文件是否有效=%s=返回结果:%s\n",strurl.toAscii().data(),strerro.toAscii().data());
    delete reply;
}
void TestThread::addTask(ZDownloadTask* task)
{
    mutex.lock();
    tasks.insert(task);
    mutex.unlock();
}
void TestThread::run()
{
    AdbProcess p;
    QString strcmd = "ping ";
    QString strOutData;
    QByteArray cmdOutData;
    QByteArray errorData;
    QString cmd;
    QString sHost;
    QNetworkRequest urlrequest;
    QHostInfo hostInfo;
    QList<QHostAddress> addrList;

    connect(&am,SIGNAL(finished(QNetworkReply*)),this,SLOT(OnFinished(QNetworkReply *)));

    while(!bExit)
    {
        if(tasks.empty())
        {
            sleep(3);
            continue;
        }
        if(!mutex.tryLock())
            continue;
        QSet<ZDownloadTask*>::iterator i = tasks.begin();
        while (i != tasks.end())
        {
            if (((*i)->status == ZDownloadTask::STAT_FINISHED)||((*i)->status==ZDownloadTask::STAT_STOPPED))
            {
                i = tasks.erase(i);
            }
            else
            {
                ++i;
            }
        }
        mutex.unlock();
#if 0
        DBG("网络连接诊断开始/////////////////////////////////////////\n");
        foreach(ZDownloadTask* task,tasks)
        {
            sHost = QUrl(task->url).host();
            DBG("url=%s\n",task->url.toString().toAscii().data());
            cmd = strcmd + sHost;
            p.exec_cmd(cmd,cmdOutData,errorData);
            strOutData = cmdOutData;
            DBG("ping %s\n",sHost.toAscii().data());
            DBG("%s\n",strOutData.toAscii().data());
            int index1 = strOutData.indexOf("[");
            int index2 = strOutData.indexOf("]");
            if(index1!=-1&&index2!=-1)
            {
                QString strip = strOutData.mid(index1+1,index2-index1-1);
                DBG("当前服务器IP:%s\n",strip.toAscii().data());
            }

            //如果ping的结果是持续丢包，要重新匹配IP进行下载。
            int nTest = 0; // nTest 是用于测试专门设计的字段
            if ((strOutData.contains("100%"))||(5 == nTest)){
                //表示这时网络已经断开，需要重新匹配IP进行下载,通知下载过程重新开始,提前结束线程
                //通知下载过程结束，然后重启下载过程
                //exit(0);
                //terminate (); //退出线程
            }

            hostInfo = QHostInfo::fromName(sHost);
            addrList = hostInfo.addresses();
            for(int i = 0; i < addrList.size(); i++)
            {
                QString strurl = task->url.toString();
                QString sIP = addrList.at(i).toString();
                strurl.replace(sHost,sIP);
                urlrequest.setUrl(QUrl(strurl));
                am.get(urlrequest);
            }
            addrList.clear();
        }
        DBG("网络连接诊断结束/////////////////////////////////////////\n");
#endif
    }
}
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

FormDownload::FormDownload(ZDownloadManager *dm, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormDownload)
{
    ui->setupUi(this);
    this->dm = dm;
    controller = GlobalController::getInstance();

    model = new DownloadModel(controller->appconfig->globalBundle, dm, this);
    ui->treeView->setModel(model);
    ProgressDelegate *progressDel = new ProgressDelegate(ui->treeView, this);
    ui->treeView->setItemDelegateForColumn(DownloadModel::COL_PROGRESS, progressDel);

    connect(dm, SIGNAL(signal_status(ZDownloadTask*)), SLOT(slot_updateItemStatus(ZDownloadTask*)));
    connect(dm, SIGNAL(signal_status(ZDownloadTask*)), model, SLOT(setDownloadData(ZDownloadTask*)));
    connect(dm, SIGNAL(signal_progress(ZDownloadTask*)), model, SLOT(setDownloadData(ZDownloadTask*)));
    connect(dm, SIGNAL(signal_speed(ZDownloadTask*,int)), model, SLOT(setDownloadSpeed(ZDownloadTask*,int)));

    connect(ui->treeView, SIGNAL(clicked(QModelIndex)), model, SLOT(slot_clicked(QModelIndex)));
    connect(ui->btn_sel_all, SIGNAL(clicked()), model, SLOT(slot_selAll()));
    connect(ui->btn_sel_rev, SIGNAL(clicked()), model, SLOT(slot_selRev()));
    connect(ui->btn_start, SIGNAL(clicked()), SLOT(slot_start()));
    connect(ui->btn_stop, SIGNAL(clicked()), SLOT(slot_stop()));
    connect(ui->btn_cleanup, SIGNAL(clicked()), SLOT(slot_cleanup()));

    QTimer::singleShot(50, this, SLOT(slot_loadDownloadList()));
}

FormDownload::~FormDownload()
{
    delete ui;
}

void FormDownload::slot_loadDownloadList() {
    model->removeRows(0, model->rowCount());

    QList<ZDownloadTask *> tasks = dm->getTasks();
    foreach(ZDownloadTask *task, tasks) {
        model->setDownloadData(task);
    }

    for(int i=0; i<model->columnCount(); i++) {
        ui->treeView->resizeColumnToContents(i);
    }
}

void FormDownload::slot_updateItemStatus(ZDownloadTask *task) {
    BundleItem *item = controller->appconfig->globalBundle->getApp(task->id);
    if(item != NULL) {
        if(task->status == ZDownloadTask::STAT_FINISHED) {
            ItemParseThread *t = new ItemParseThread(item);
            connect(t, SIGNAL(signal_itemParseDone(BundleItem*)),
                    SLOT(slot_itemParseDone(BundleItem*)));
            t->start();
        } else if(task->status == ZDownloadTask::STAT_FAILED) {
            dm->startTask(task);
        } else {
            item->download_status = task->status;
            emit signal_itemStatusChanged(item);
            controller->appconfig->saveBundleItem(item);
        }
    }
}

void FormDownload::slot_itemParseDone(BundleItem *item) {
    QStringList list = item->id.split("_");
    item->id = list.at(0);
    emit signal_itemStatusChanged(item);
    ZDownloadTask *task = dm->getTask(item->id);
    if(item->download_status != ZDownloadTask::STAT_FINISHED) {
        task->status = ZDownloadTask::STAT_FAILED;
    }
    model->setDownloadData(task);
    controller->appconfig->saveBundleItem(item);
}

void FormDownload::slot_add() {
    DialogAddDownload *dlg = new DialogAddDownload(this);
    dlg->setWindowTitle(tr("add download task"));
    if(dlg->exec() == QDialog::Accepted) {
        QString url = dlg->getUrl();
        QString fileName = dlg->getFileName();
        if(!url.isEmpty()) {
            if(fileName.isEmpty()) {
                QByteArray html;
                QDomDocument dom;
                ZDMHttp::get(url.toLocal8Bit().data(), html);
                if(dom.setContent(html)) {
                    QDomNodeList nodes = dom.elementsByTagName("app");
                    for(int i=0; i<nodes.size(); i++) {
                        QDomNode app = nodes.at(i);
                        url = app.firstChildElement("down_url").text();
                        fileName = app.firstChildElement("appname").text() + ".apk";
                        controller->downloadmanager->addTask(url, fileName);
                    }
                }
            } else {
                controller->downloadmanager->addTask(url, fileName);
            }
        }
    }
    delete dlg;
}

void FormDownload::slot_start() {
    // if(model->checkList.isEmpty()) {
    //     return;
    // }

    //QList<ZDownloadTask *> tasks = dm->getTasks(model->checkList);
    QList<ZDownloadTask *> tasks = dm->getTasks();
    foreach(ZDownloadTask *task, tasks) {
        dm->startTask(task);
    }
}

void FormDownload::slot_stop() {
    //if(model->checkList.isEmpty()) {
    //       return;
    //}

    // QList<ZDownloadTask *> tasks = dm->getTasks(model->checkList);
    QList<ZDownloadTask *> tasks = dm->getTasks();
    foreach(ZDownloadTask *task, tasks) {
        dm->stopTask(task);
    }
}

void FormDownload::slot_cleanup() {
    int i = 0;
    QStandardItem *it = NULL;
    model->mutex.lock();
    while((it = model->item(i++, 0)) != NULL) {
        QString id = it->data(Qt::UserRole).toString();
        ZDownloadTask *task = dm->getTask(id);
        if(task != NULL && task->status == ZDownloadTask::STAT_FINISHED) {
            dm->removeTask(task);
            model->removeRow(--i);
        }
    }
    model->mutex.unlock();
}

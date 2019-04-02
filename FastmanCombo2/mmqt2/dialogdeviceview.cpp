#include "dialogdeviceview.h"
#include "ui_dialogdeviceview.h"

#include "fastman2.h"
#include "formdevicenode.h"
#include "adbdevicenode.h"
#include "zadbdevicenode.h"
#include "adbclient.h"
#include "zlog.h"
#include "zstringutil.h"
#include "dialogprogress.h"
#include "dialogwait.h"

#include <QList>
#include <QDebug>
#include <QStandardItemModel>
#include <QModelIndex>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QSize>
#include <QMouseEvent>
#include <QClipboard>
#include <QFileDialog>
#include <QTextCodec>
#include <QDateTime>

//View
static void setDfInfo(quint64 total, quint64 free, QProgressBar *pb, QLabel *lb) {
    if (total > 0) {
        int maxValue = total / 4096;
        int value = (total - free) / 4096;
        pb->setMaximum(maxValue);
        pb->setValue(value);
        lb->setText(ZStringUtil::getSizeStr(free));
    } else {
        pb->setMaximum(100);
        pb->setValue(0);
        lb->setText(QObject::tr("unknown"));
    }
}

DialogDeviceView::DialogDeviceView(AdbDeviceNode *node, QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogDeviceView)
  , m_node(node)
  , m_basicInfo(0)
  , m_appList(0)
  , m_model(0)
  , m_dragged(false)
  , threadcount(0)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_TranslucentBackground);

    getScreenshot();
    getBasicinfo();

    m_appList = new QList<AGENT_APP_INFO *>();
    m_model = new DeviceModel(this);
    ui->tv->setModel(m_model);
    getApps();

    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            this, SLOT(slot_updataCheckedState()));
    connect(ui->tv, SIGNAL(clicked(QModelIndex)),
            this, SLOT(slot_itemClicked(QModelIndex)));

    connect(ui->btn_max, SIGNAL(clicked()),
            this, SLOT(slot_maxBtnClicked()));
    connect(ui->btn_close, SIGNAL(clicked()),
            this, SLOT(close()));
    connect(ui->btn_refresh_ss, SIGNAL(clicked()),
            this, SLOT(slot_refreshScreenshot()));
    connect(ui->btn_copy_ss, SIGNAL(clicked()),
            this, SLOT(slot_copyScreenshot()));
    connect(ui->btn_save_ss, SIGNAL(clicked()),
            this, SLOT(slot_saveScreenshot()));
    connect(ui->btn_refreshList, SIGNAL(clicked()),
            this, SLOT(slot_refreshApps()));
    connect(ui->btn_checkAll, SIGNAL(clicked()),
            this, SLOT(slot_setCheckedAll()));
    connect(ui->btn_invertChk, SIGNAL(clicked()),
            this, SLOT(slot_setInvertChecked()));
    connect(ui->btn_backup, SIGNAL(clicked()),
            this, SLOT(slot_backup()));
    connect(ui->btn_uninstall, SIGNAL(clicked()),
            this, SLOT(slot_uninstall()));
    connect(ui->btn_enable,SIGNAL(clicked()),
            this,SLOT(slot_adbpmable()));
    connect(ui->btn_disable,SIGNAL(clicked()),
            this,SLOT(slot_adbpmdisable()));
}

DialogDeviceView::~DialogDeviceView() {
    qDeleteAll(m_appList->begin(), m_appList->end());
    m_appList->clear();
    delete ui;
}

bool DialogDeviceView::isInRange(QWidget *widget, int x, int y) {
    int x1 = widget->pos().x();
    int x2 = x1 + widget->width();
    int y1 = widget->pos().y();
    int y2 = y1 + widget->height();

    if(x >= x1 && x <= x2 && y >= y1 && y <= y2) {
        return true;
    }
    return false;
}

void DialogDeviceView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastPoint = event->globalPos() - this->pos();
        event->accept();
        m_dragged = true;
        mouseX = event->x();
        mouseY = event->y();
        isDraggingWindow = isInRange(ui->frm_top, mouseX, mouseY);
    }
}

void DialogDeviceView::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragged && isDraggingWindow
            && (event->buttons() & Qt::LeftButton)) {
        this->move(event->globalPos() - m_lastPoint);
        event->accept();
    }
}

void DialogDeviceView::mouseReleaseEvent(QMouseEvent *) {
    m_dragged = false;
    isDraggingWindow = false;
}

void DialogDeviceView::keyPressEvent(QKeyEvent *e) {
    e->ignore();
}

void DialogDeviceView::closeEvent(QCloseEvent *e) {
    if(threadcount != 0) {
        e->ignore();
    }else {
        e->accept();
    }
}

void DialogDeviceView::getBasicinfo() {
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)m_node;

    ui->lbl_index->setText(QString::number(znode->node_index));
    ui->lbl_brand->setText(znode->brand.isEmpty() ?
                               znode->adb_serial : znode->brand.toUpper());
    ui->lbl_model->setText(znode->model);

    setDfInfo(znode->zm_info.sys_total, znode->zm_info.sys_free,
              ui->pb_sysFree, ui->lbl_sysSpaceData);
    setDfInfo(znode->zm_info.tmp_total, znode->zm_info.tmp_free,
              ui->pb_dataFree, ui->lbl_phoneSpaceData);
    setDfInfo(znode->zm_info.store_total, znode->zm_info.store_free,
              ui->pb_sdFree, ui->lbl_SDSpaceData);
}

void DialogDeviceView::getScreenshot() {
    ui->lbl_screenshot->setText(tr("Please wait..."));
    AdbClient *adbClient = new AdbClient(m_node->adb_serial);
    GetScreenshotThread *thread = new GetScreenshotThread(m_image, adbClient);
    connect(thread, SIGNAL(finished()), this, SLOT(slot_getScrrenshotFinished()));
    connect(thread, SIGNAL(finished()), this, SLOT(slot_countReduce()));
    thread->start();
    threadcount++;

}

void DialogDeviceView::getApps() {
    if (m_appList == NULL) {
        m_appList = new QList<AGENT_APP_INFO *>();
    }

    // release the objects.
    qDeleteAll(m_appList->begin(), m_appList->end());
    m_appList->clear();
    m_appMap.clear();

    GetAppListThread *thread = new GetAppListThread(m_appList, m_node);
    connect(thread, SIGNAL(signal_getAppFinished(AGENT_APP_INFO *)),
            this, SLOT(slot_onGetAppFinished(AGENT_APP_INFO *)));
    connect(thread, SIGNAL(finished()), this, SLOT(slot_getAppsFinisehd()));
    connect(thread, SIGNAL(finished()), this, SLOT(slot_countReduce()));
    thread->start();
    threadcount++;
}

void DialogDeviceView::slot_itemClicked(const QModelIndex &index) {
    bool checked = m_model->data(m_model->index(index.row(), DeviceModel::COL_CHECKABLE), Qt::CheckStateRole).toBool();
    if (checked) {
        m_model->setData(m_model->index(index.row(), DeviceModel::COL_CHECKABLE), Qt::Unchecked, Qt::CheckStateRole);
    } else {
        m_model->setData(m_model->index(index.row(), DeviceModel::COL_CHECKABLE), Qt::Checked, Qt::CheckStateRole);
    }
}

void DialogDeviceView::slot_maxBtnClicked() {
    QWidget *btn = dynamic_cast<QWidget *>(sender());
    if (btn == ui->btn_max) {
        if (this->isMaximized()) {
            this->showNormal();
        } else {
            this->showMaximized();
        }
    }
}

void DialogDeviceView::slot_refreshScreenshot() {
    ui->btn_refresh_ss->setEnabled(false);
    getScreenshot();
}

void DialogDeviceView::slot_copyScreenshot() {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setImage(m_image);
}

void DialogDeviceView::slot_saveScreenshot() {
    QString path = qApp->applicationDirPath() + "/untitled.png";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save screenshot"),
                                                    path, tr("Images (*.png)"));
    if (!m_image.save(fileName)) {
        DBG("Save the screencap failed!");
    }
}

void DialogDeviceView::slot_refreshApps() {
    ui->btn_refreshList->setEnabled(false);
    m_model->removeRows(0, m_model->rowCount());
    getApps();
}

void DialogDeviceView::slot_setCheckedAll() {
    int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        m_model->item(row, DeviceModel::COL_CHECKABLE)->setCheckState(Qt::Checked);
    }
}

void DialogDeviceView::slot_setInvertChecked() {
    int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        bool checked = m_model->data(m_model->index(row, DeviceModel::COL_CHECKABLE), Qt::CheckStateRole).toBool();
        if (checked)
            m_model->item(row, DeviceModel::COL_CHECKABLE)->setCheckState(Qt::Unchecked);
        else
            m_model->item(row, DeviceModel::COL_CHECKABLE)->setCheckState(Qt::Checked);
    }
}

void DialogDeviceView::slot_backup() {
    QList<AGENT_APP_INFO *> apps;
    if (0 == getCheckedApps(apps)) {
        return;
    }

    QString caption = tr("Choose the dir for backup");
    QString strDes = QFileDialog::getExistingDirectory(this, caption);
    if (strDes.isEmpty()) {
        return;
    }

    DialogProgress *dlg = new DialogProgress(this);
    dlg->setTitle(tr("Backup"));
    BackupThread *thread = new BackupThread(m_node, apps, strDes, this);
    connect(thread, SIGNAL(signal_progressChanged(int,int)), dlg, SLOT(slot_setProgress(int,int)));
    connect(thread, SIGNAL(signal_stateChanged(QString)), dlg, SLOT(slot_setState(QString)));
    connect(thread, SIGNAL(finished()), dlg, SLOT(accept()));
    connect(dlg, SIGNAL(signal_stop()), thread, SLOT(slot_stop()));
    connect(thread, SIGNAL(finished()), this, SLOT(slot_countReduce()));
    thread->start();
    threadcount++;
    dlg->exec();
}

void DialogDeviceView::slot_adbpmable(){
    QList<AGENT_APP_INFO *> apps;
    if (0 == getCheckedApps(apps)) {
        return;
    }
    DialogProgress *dlg = new DialogProgress(this);
    dlg->setTitle("pm enable");
    PmAbleThread *thread = new PmAbleThread(m_node, apps, m_model,true,this);
    connect(thread,SIGNAL(signal_AbleChanged(QString,bool)),
            this,SLOT(slot_AbleChanged(QString,bool)));
    connect(thread,SIGNAL(signal_WarnRootFail()),
            this,SLOT(slot_WarnRootFail()));
    connect(thread,SIGNAL(signal_Progress(int,int)),
            dlg,SLOT(slot_setProgress(int,int)));
    connect(dlg,SIGNAL(signal_stop()),thread,
            SLOT(slot_stop()));
    connect(thread, SIGNAL(signal_state(QString)), dlg, SLOT(slot_setState(QString)));
    connect(thread,SIGNAL(finished()),dlg,SLOT(accept()));
    connect(thread, SIGNAL(finished()), this, SLOT(slot_countReduce()));
    thread->start();
    threadcount++;
    dlg->exec();
    delete dlg;
}

void DialogDeviceView::slot_adbpmdisable(){
    QList<AGENT_APP_INFO *> apps;
    if (0 == getCheckedApps(apps)) {
        return;
    }
    DialogProgress *dlg = new DialogProgress(this);
    dlg->setTitle("pm disable");
    PmAbleThread *thread = new PmAbleThread(m_node, apps, m_model,false,this);
    connect(thread,SIGNAL(signal_AbleChanged(QString,bool)),
            this,SLOT(slot_AbleChanged(QString,bool)));
    connect(thread,SIGNAL(signal_WarnRootFail()),
            this,SLOT(slot_WarnRootFail()));
    connect(thread,SIGNAL(signal_Progress(int,int)),
            dlg,SLOT(slot_setProgress(int,int)));
    connect(dlg,SIGNAL(signal_stop()),thread,
            SLOT(slot_stop()));
    connect(thread, SIGNAL(signal_state(QString)), dlg, SLOT(slot_setState(QString)));
    connect(thread,SIGNAL(finished()),dlg,SLOT(accept()));
    connect(thread, SIGNAL(finished()), this, SLOT(slot_countReduce()));
    thread->start();
    threadcount++;
    dlg->exec();
    delete dlg;
}

void DialogDeviceView::slot_uninstall() {
    QList<AGENT_APP_INFO *> apps;
    if (0 == getCheckedApps(apps)) {
        return;
    }

    DialogProgress *dlg = new DialogProgress(this);
    dlg->setTitle(tr("Unistall"));
    UninstallThread *thread = new UninstallThread(m_node, apps, m_model, this);
    connect(thread, SIGNAL(signal_progressChanged(int,int)), dlg, SLOT(slot_setProgress(int,int)));
    connect(thread, SIGNAL(signal_stateChanged(QString)), dlg, SLOT(slot_setState(QString)));
    connect(thread, SIGNAL(finished()), dlg, SLOT(accept()));
    connect(thread, SIGNAL(signal_removeRow(DeviceModel *, QString)), this, SLOT(slot_removeRow(DeviceModel*, QString)));
    connect(thread, SIGNAL(sigDeviceRefresh(AdbDeviceNode*)), this, SLOT(slot_refreshUI()));
    connect(dlg, SIGNAL(signal_stop()), thread, SLOT(slot_stop()));
    connect(thread, SIGNAL(finished()), this, SLOT(slot_countReduce()));
    thread->start();
    threadcount++;
    dlg->exec();
}

void DialogDeviceView::slot_getScrrenshotFinished() {
    ui->btn_refresh_ss->setEnabled(true);
    QImage scaledImage = m_image.scaledToWidth(ui->lbl_screenshot->width(), Qt::SmoothTransformation);
    ui->lbl_screenshot->setPixmap(QPixmap::fromImage(scaledImage));
}

void DialogDeviceView::slot_onGetAppFinished(AGENT_APP_INFO *app) {
    listMutex.lock();
    m_appMap.insert(app->packageName, app);
    m_model->setAppData(app);
    slot_updateTotalState();
    for (int i = 0; i < m_model->columnCount(); ++i) {
        ui->tv->resizeColumnToContents(i);
    }
    listMutex.unlock();
}

void DialogDeviceView::slot_getAppsFinisehd() {
    ui->btn_refreshList->setEnabled(true);
}

void DialogDeviceView::slot_refreshUI() {
    getBasicinfo();
}

void DialogDeviceView::slot_updateTotalState() {
    quint64 totalSize = 0;

    int rowCount = m_model->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        quint64 size = m_model->data(m_model->index(row, DeviceModel::COL_SIZE),
                                     Qt::UserRole + 1).toULongLong();
        totalSize += size;
    }

    QString strCount = QString::number(rowCount);
    QString strSize = ZStringUtil::getSizeStr(totalSize);
    ui->lbl_state->setText(tr("all %1 , total size %2").arg(strCount).arg(strSize));
}

void DialogDeviceView::slot_updataCheckedState() {
    unsigned int checkedCount = 0;
    quint64 checkedSize = 0;

    int rowCount = m_model->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        bool checked = m_model->data(m_model->index(row, DeviceModel::COL_NAME),
                                     Qt::CheckStateRole).toBool();
        quint64 size = m_model->data(m_model->index(row, DeviceModel::COL_SIZE),
                                     Qt::UserRole + 1).toULongLong();

        if (checked) {
            checkedCount++;
            checkedSize += size;
        }
    }

    QString strCount = QString::number(checkedCount);
    QString strSize = ZStringUtil::getSizeStr(checkedSize);

    ui->lbl_state_checked->setText(tr("Choosed %1, total size %2").arg(strCount).arg(strSize));
}

void DialogDeviceView::slot_WarnRootFail(){
    QMessageBox::warning(this,tr("Warning"),tr("Root failed!"),QMessageBox::Yes);
}

void DialogDeviceView::slot_AbleChanged(QString pkg, bool m){
    m_model->mutex.lock();
    int i = 0;

    for (i = 0;i < m_model->rowCount(); ++i) {
        QString pkgTmp = m_model->item(i, DeviceModel::COL_PKG)->data(Qt::UserRole).toString();
        if (pkgTmp == pkg ) {
            m_model->setData(m_model->item(i,DeviceModel::COL_ENABLED)->index(),m ? tr("enabled") : tr("disabled"),Qt::DisplayRole);
            break;
        }
    }

    for (int i = 0; i < m_appList->count(); ++i) {
        if (m_appList->at(i)->packageName == pkg) {
            m_appList->at(i)->enabled = m;
            break;
        }
    }
    m_model->mutex.unlock();
}

void DialogDeviceView::slot_removeRow(DeviceModel *model, const QString &pkg) {
    model->mutex.lock();
    int i = 0;
    QStandardItem *item = NULL;
    while ((item = model->item(i++)) != NULL) {
        if (item->data(Qt::UserRole).toString() == pkg) {
            model->removeRow(--i);
            break;
        }
    }

    for (int i = 0; i < m_appList->count(); ++i) {
        if (m_appList->at(i)->packageName == pkg) {
            m_appList->removeAt(i);
            break;
        }
    }

    m_appMap.remove(pkg);
    slot_updateTotalState();
    slot_updataCheckedState();
    model->mutex.unlock();
}

int DialogDeviceView::getCheckedApps(QList<AGENT_APP_INFO *> &apps) {
    int appCnt = 0;
    int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        bool checked = m_model->item(row, DeviceModel::COL_CHECKABLE)->data(Qt::CheckStateRole).toBool();
        if (checked) {
            appCnt++;
            QString pkg = m_model->item(row, DeviceModel::COL_PKG)->data(Qt::UserRole).toString();
            apps.append(m_appMap.value(pkg));
        }
    }
    return appCnt;
}

void DialogDeviceView::slot_countReduce() {
    threadcount--;
}

// Model
DeviceModel::DeviceModel(QObject *parent) :
    QStandardItemModel(parent) {
    setColumnCount(COL_END);
    QStringList labels;
    labels.append(tr("Name"));
    labels.append(tr("Version"));
    labels.append(tr("Enabled"));
    labels.append(tr("Type"));
    labels.append(tr("Size"));
    labels.append(tr("Time"));
    setHorizontalHeaderLabels(labels);

    setSortRole(Qt::UserRole + 1);
}

void DeviceModel::setAppsData(const QList<AGENT_APP_INFO *> &list) {
    for (int row = 0; row < list.count(); ++row) {
        setAppData(list.at(row));
    }
}

void DeviceModel::setAppData(AGENT_APP_INFO *app) {
    mutex.lock();
    int row = -1;
    for (int i = 0; i < rowCount(); ++i) {
        QString pkg = data(index(i, COL_PKG), Qt::UserRole).toString();
        if (pkg == app->packageName) {
            row = i;
            break;
        }
    }

    if (-1 == row) {
        row = rowCount();
        insertRow(row);
    }

    setData(index(row, COL_NAME), app->name, Qt::DisplayRole);
    item(row, COL_PKG)->setData(app->packageName, Qt::UserRole);
    item(row, COL_NAME)->setData(ZStringUtil::getPinyin(app->name), Qt::UserRole + 1);
    item(row, COL_CHECKABLE)->setData(Qt::Unchecked, Qt::CheckStateRole);
    QIcon icon = QIcon(QPixmap::fromImage(app->icon));
    if (icon.isNull()) {
        icon = QIcon(":/skins/skins/icon_downloading_app.png");
    }
    item(row, COL_ICON)->setIcon(icon);

    setData(index(row, COL_VERSION), app->versionName, Qt::DisplayRole);
    item(row, COL_VERSION)->setData(app->versionName, Qt::UserRole + 1);


    setData(index(row, COL_ENABLED), app->enabled ? tr("enabled") : tr("disabled"), Qt::DisplayRole);
    item(row, COL_ENABLED)->setData(app->enabled, Qt::UserRole + 1);

    setData(index(row, COL_TYPE), getFlagName(app->flags), Qt::DisplayRole);
    item(row, COL_TYPE)->setData(getType(app->flags), Qt::UserRole + 1);

    setData(index(row, COL_SIZE), ZStringUtil::getSizeStr(app->size), Qt::DisplayRole);
    item(row, COL_SIZE)->setData(app->size, Qt::UserRole + 1);

    setData(index(row, COL_TIME), QDateTime::fromMSecsSinceEpoch(app->mtime).toString("yyyy/MM/dd hh:mm:ss"), Qt::DisplayRole);
    item(row, COL_TIME)->setData(app->mtime, Qt::UserRole + 1);
    mutex.unlock();
}

QString DeviceModel::getFlagName(int f) {
    int type = getType(f);
    QString ret = QString::fromUtf8("未知");

    switch(type) {
    case TYPE_SYSTEM_APP:
        ret = QString::fromUtf8("系统内置");
        break;
    case TYPE_USER_APP:
        ret = QString::fromUtf8("用户安装");
        break;
    case TYPE_UPDATED_SYSTEM_APP:
        ret = QString::fromUtf8("系统更新");
        break;
    default:
        break;
    }

    if((f & 262144) != 0) {
        ret += QString::fromUtf8("[存储卡]");
    }
    return ret;
}

int DeviceModel::getType(int f) {
    if((f & 128) != 0) {
        return TYPE_UPDATED_SYSTEM_APP;
    } else if((f & 1) == 0) {
        return TYPE_USER_APP;
    } else {
        return TYPE_SYSTEM_APP;
    }
    return TYPE_UNKNOWN_APP;
}


// Thread: GetScreenshot
GetScreenshotThread::GetScreenshotThread(QImage &image, AdbClient *adbClient) : QThread()
  , m_adbClient(adbClient)
  , m_image(image)
{
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}

void GetScreenshotThread::run() {
    m_image = m_adbClient->adb_screencap();
}


// Thread: GetAppList
GetAppListThread::GetAppListThread(QList<AGENT_APP_INFO *> *list,
                                   AdbDeviceNode *node,
                                   QObject *parent) : QThread(parent)
  , m_list(list)
  , m_node(node)
{
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}

void GetAppListThread::run() {
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)m_node;
    FastManAgentClient *fmcli = new FastManAgentClient(znode->adb_serial, znode->zm_port);
    if (fmcli->getAppListHead()) {
        for(;;) {
            AGENT_APP_INFO *app = new AGENT_APP_INFO();
            if (fmcli->getNextApp(*app)) {
                emit signal_getAppFinished(app);
            } else {
                delete app;
                break;
            }
        }
    }
    delete fmcli;
}


// Thread: Backup
BackupThread::BackupThread(AdbDeviceNode *node,
                           const QList<AGENT_APP_INFO *> &appList,
                           const QString &strDes,
                           QObject *parent) : QThread(parent)
  , m_node(node)
  , m_list(appList)
  , m_strDes(strDes)
  , m_bStop(false)
{
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}

void BackupThread::run() {
    AdbClient *adbClient = new AdbClient(m_node->adb_serial);

    QString strState = tr("Doing `%1`[%2/%3]...");
    int count = m_list.count();
    for (int i = 0; i < count; ++i) {
        if (m_bStop) {
            break;
        }

        AGENT_APP_INFO *app = m_list.at(i);
        emit signal_stateChanged(strState.arg(app->name).arg(i + 1).arg(count));
        QString rPath = app->sourceDir;
        QString lPath = m_strDes + "/" + app->name + app->versionName + ".apk";
        adbClient->adb_pull(rPath, lPath);
        emit signal_progressChanged(i + 1, count);
    }

    delete adbClient;
}

void BackupThread::slot_stop() {
    m_bStop = true;
}

//Thread:pm disable or able
PmAbleThread ::PmAbleThread(AdbDeviceNode *node,
                            const QList<AGENT_APP_INFO *> &appList,
                            DeviceModel *model,
                            bool able,
                            QObject *parent) : QThread(parent)
  , m_node(node)
  , m_list(appList)
  , m_model(model)
  , status(able)
  , needstop(false)
{
    connect(this,SIGNAL(finished()),this,SLOT(deleteLater()));
}

void PmAbleThread::run() {
    AdbClient *adbClient = new AdbClient(m_node->adb_serial);
    ZAdbDeviceNode *znode = (ZAdbDeviceNode*)m_node;
    FastManAgentClient apkcli;

    FastManClient cli(znode->adb_serial, znode->zm_port);
    QByteArray out;

    if(!cli.switchRootCli()){
        emit signal_WarnRootFail();
        apkcli.addLog(tr("root failed"));
        return ;
    }
    int count = m_list.count();
    for (int i = 0; i < count; ++i) {
        if(needstop) {
            break;
        }
        AGENT_APP_INFO *app = m_list.at(i);
        if(status == app->enabled) {
            continue;
        }
        cli.exec(out,3,"/system/bin/pm", status ? "enable" : "disable", app->packageName.toUtf8().data());
        app->enabled = status;
        emit signal_state(tr("doing %1 %2/%3").arg(app->name).arg(i+1).arg(count));
        emit signal_Progress(i+1,count);
        emit signal_AbleChanged(app->packageName,status);
    }
    delete adbClient;
}

void PmAbleThread::slot_stop() {
    needstop = true;
}

// Thread: uninstall
UninstallThread::UninstallThread(AdbDeviceNode *node,
                                 const QList<AGENT_APP_INFO *> &appList,
                                 DeviceModel *model,
                                 QObject *parent) : QThread(parent)
  , m_node(node)
  , m_list(appList)
  , m_model(model)
  , m_bStop(false)
{
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}

void UninstallThread::run() {
    AdbClient *adbClient = new AdbClient(m_node->adb_serial);
    ZAdbDeviceNode *znode = (ZAdbDeviceNode*)m_node;
    FastManClient cli(znode->adb_serial, znode->zm_port);
    cli.switchRootCli();

    QString strState = tr("Doing `%1`[%2/%3]...");
    int count = m_list.count();
    for (int i = 0; i < count; ++i) {
        if (m_bStop) {
            break;
        }

        AGENT_APP_INFO *app = m_list.at(i);
        emit signal_stateChanged(strState.arg(app->name).arg(i + 1).arg(count));
        if(app->getType() == AGENT_APP_INFO::TYPE_SYSTEM_APP) {
            adbClient->adb_cmd("shell:am force-stop "+app->packageName);
            cli.remove(app->sourceDir);
        } else {
            adbClient->pm_cmd("uninstall " + app->packageName);
        }
        emit signal_progressChanged(i + 1, count);
        emit signal_removeRow(m_model, app->packageName);

        cli.getZMInfo(znode->zm_info);
        emit sigDeviceRefresh(m_node);
    }
    delete adbClient;
}

void UninstallThread::slot_stop() {
    m_bStop = true;
}

#include "dialogbundleview.h"
#include "ui_dialogbundleview.h"

#include "globalcontroller.h"
#include "appconfig.h"
#include "bundle.h"
#include "adbprocess.h"
#include "ziphelper.h"
#include "zstringutil.h"
#include "dialogprogress.h"
#include "formdownload.h"
#include "zdownloadmanager.h"

#include <QPoint>
#include <QMouseEvent>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QListView>
#include <QTreeView>
#include <QDateTime>
#include <QToolButton>
#include <QList>
#include <QTimer>
#include <QUuid>
#include <QDebug>
#include "loginmanager.h"

// View
static QString fmtSize(qint64 size) {
    char tmp[64];
    // MB
    if(size >= 1024*1024) {
        sprintf(tmp, "%.2f MB", (double)size/(1024*1024));
    } else if(size >= 1024) {
        sprintf(tmp, "%.2f KB", (double)size/1024);
    } else {
        sprintf(tmp, "%lld B", size);
    }
    return QString(tmp);
}

DialogBundleView::DialogBundleView(Bundle *bundle, bool bNewBundle, QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint)
  , ui(new Ui::DialogBundleView)
  , m_bNewBundle(bNewBundle)
  , m_isSaveBtnclicked(false)
  , m_dragged(false)
  , m_specialBundle(bundle)
  , m_globalModel(NULL)
  , m_specialModel(NULL)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_TranslucentBackground);

    m_config = GlobalController::getInstance()->appconfig;

    m_appList = m_specialBundle->getAppList();
    initGlobalView();
    initSpecialView();
    slot_onChkedStateChanged();
    initUI();

    GlobalController *controller = GlobalController::getInstance();
    if(controller->loginManager->disableBundleLocalFile) {
        ui->btn_addLocal->hide();
    }

    connect(ui->btn_max, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->btn_close, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->cbx_global, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->btn_addGlobal, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->cbx_bdl, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->btn_remove, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->btn_refresh, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->btn_save, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    connect(ui->btn_addLocal, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));

    connect(ui->ledt_filter, SIGNAL(textChanged(QString)), this, SLOT(slot_onFilterTextChanged(QString)));
    connect(ui->splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(slot_onSplitterMoved(int,int)));
}

DialogBundleView::~DialogBundleView()
{
    delete ui;
}

bool DialogBundleView::isInRange(QWidget *widget, int x, int y) {
    int x1 = widget->pos().x();
    int x2 = x1 + widget->width();
    int y1 = widget->pos().y();
    int y2 = y1 + widget->height();

    if(x >= x1 && x <= x2 && y >= y1 && y <= y2) {
        return true;
    }
    return false;
}

void DialogBundleView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastPoint = event->globalPos() - this->pos();
        m_dragged = true;
        mouseX = event->x();
        mouseY = event->y();
        isDraggingWindow = isInRange(ui->wdt_top, mouseX, mouseY);
        event->accept();
    }
}

void DialogBundleView::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragged && isDraggingWindow
            && (event->buttons() & Qt::LeftButton)) {
        this->move(event->globalPos() - m_lastPoint);
        event->accept();
    }
}

void DialogBundleView::mouseReleaseEvent(QMouseEvent *) {
    m_dragged = false;
    isDraggingWindow = false;
}

void DialogBundleView::keyPressEvent(QKeyEvent *e) {
    e->ignore();
}

void DialogBundleView::closeEvent(QCloseEvent *e) {
    if (maybeSave()) {
        if (m_isSaveBtnclicked) {
            slot_saveBundle();
            e->accept();
            return;
        }

        int ret = QMessageBox::information(this,
                                           tr("Save Bundle"),
                                           tr("The bundle is changed, do you save it?"),
                                           QMessageBox::Ok, QMessageBox::Cancel);
        if (ret == QMessageBox::Ok) {
            slot_saveBundle();
        } else {
            QStringList ids = m_globalBundle->getBundleIds();
            if (!ids.contains(m_specialBundle->id))
                m_globalBundle->removeBundle(m_specialBundle);
        }
    }
    e->accept();
}

void DialogBundleView::initGlobalView() {
    m_globalBundle = GlobalController::getInstance()->appconfig->globalBundle;
    if (m_globalModel == NULL)
        m_globalModel = new BundleModel(ui->tvw_global, this);

    m_globalModel->removeRows(0, m_globalModel->rowCount());
    m_globalModel->setBundleData(m_globalBundle);
    ui->tvw_global->setModel(m_globalModel);

    ui->tvw_global->setColumnHidden(BundleModel::COL_SIZE, true);
    ui->tvw_global->setColumnHidden(BundleModel::COL_STATUS, true);
    ui->tvw_global->setColumnHidden(BundleModel::COL_INSLOCATION, true);
    ui->tvw_global->setColumnHidden(BundleModel::COL_TIME, true);

    resizeColumn(ui->tvw_global);
}

void DialogBundleView::initSpecialView() {
    if (m_specialModel == NULL)
        m_specialModel = new BundleModel(ui->tvw_special, this);

    m_specialModel->removeRows(0, m_specialModel->rowCount());
    m_specialModel->setBundleData(m_specialBundle);
    ui->tvw_special->setModel(m_specialModel);

    resizeColumn(ui->tvw_special);
}

void DialogBundleView::initUI() {
    ui->lbl_title->setText(m_specialBundle->name);
    ui->ledt_name->setText(m_specialBundle->name);
    ui->cbx_enable->setChecked(m_specialBundle->enable);

    if (m_specialBundle->readonly) {
        ui->lbl_typeSpecial->setText(tr("System Bundle"));
        ui->ledt_name->setEnabled(false);
        ui->splitter->handle(1)->setEnabled(false);
        ui->wdt_filter->setVisible(false);
        ui->tvw_global->setVisible(false);
        ui->wdt_bottom_left->setVisible(false);
        ui->wdt_bottom_right->setVisible(false);
    } else {
        ui->lbl_typeSpecial->setText(tr("Custom Bundle"));
        ui->ledt_name->setEnabled(true);
        connect(ui->tvw_global, SIGNAL(clicked(QModelIndex)),
                this, SLOT(slot_itemClicked(QModelIndex)));
        connect(ui->tvw_special, SIGNAL(clicked(QModelIndex)),
                this, SLOT(slot_itemClicked(QModelIndex)));
    }

    QTimer::singleShot(1000, this, SLOT(slot_addClrBtn()));
}

void DialogBundleView::slot_addClrBtn() {
    clrFilterBtn = new QToolButton(ui->ledt_filter);
    clrFilterBtn->setObjectName("clrFilterText");
    int x = ui->ledt_filter->x() + ui->ledt_filter->width() - 16 - 4;
    int y = (ui->ledt_filter->y() + ui->ledt_filter->height() - 16) / 2;
    clrFilterBtn->setGeometry(x, y, 16, 16);
    clrFilterBtn->hide();
    connect(clrFilterBtn, SIGNAL(clicked()), this, SLOT(slot_btnClicked()));
    clrFilterBtn->setStyleSheet("#clrFilterText{border-image: url(:/skins/skins/btn_clr.png);} \n"
                                "#clrfilterText:pressed{border-image: url(:/skins/skins/btn_clr_hover.png);}");
}

void DialogBundleView::slot_onItemStatusChanged(BundleItem *item) {
    m_globalModel->setBundleItemData(item);
    for (int i = 0; i < m_specialModel->rowCount(); ++i) {
        QString specialId = m_specialModel->data(m_specialModel->index(i, BundleModel::COL_ID), Qt::UserRole).toString();
        if (item->id == specialId) {
            m_specialModel->setBundleItemData(item);
            break;
        }
    }
}

void DialogBundleView::slot_itemClicked(const QModelIndex &index) {
    QTreeView *tvw = dynamic_cast<QTreeView *>(sender());
    BundleModel *model = dynamic_cast<BundleModel *>(tvw->model());

    bool checked = model->data(tvw->model()->index(index.row(), BundleModel::COL_CHECKABLE),
                               Qt::CheckStateRole).toBool();
    if (checked) {
        model->item(index.row(), BundleModel::COL_CHECKABLE)->setCheckState(Qt::Unchecked);
    } else {
        model->item(index.row(), BundleModel::COL_CHECKABLE)->setCheckState(Qt::Checked);
    }

    if (tvw == ui->tvw_global)
        ui->cbx_global->setChecked(isCheckedAll(tvw));
    else if (tvw == ui->tvw_special)
        ui->cbx_bdl->setChecked(isCheckedAll(tvw));
}

void DialogBundleView::slot_btnClicked() {
    QWidget *wdt = dynamic_cast<QWidget *>(sender());
    if (wdt->objectName() == "btn_max") {
        if (this->isMaximized())
            this->showNormal();
        else
            this->showMaximized();
    } else if (wdt->objectName() == "btn_close") {
        this->close();
    } else if (wdt->objectName() == "cbx_global") {
        setAllChecked(ui->tvw_global, ui->cbx_global->isChecked());
    } else if (wdt->objectName() == "btn_addGlobal") {
        slot_addGlobal();
    } else if (wdt->objectName() == "cbx_bdl") {
        setAllChecked(ui->tvw_special, ui->cbx_bdl->isChecked());
    } else if (wdt->objectName() == "btn_remove") {
        removeApps();
    } else if (wdt->objectName() == "btn_refresh") {
        slot_refreshSpecialView();
    } else if (wdt->objectName() == "btn_save") {
        m_isSaveBtnclicked = true;
        this->close();
    } else if (wdt->objectName() == "btn_addLocal") {
        slot_addLocal();
    } else if (wdt->objectName() == "clrFilterText") {
        ui->ledt_filter->clear();
        slot_onFilterTextChanged(ui->ledt_filter->text());
    }
}

void DialogBundleView::slot_onFilterTextChanged(const QString &text) {
    for (int i = 0; i < m_globalModel->rowCount(); ++i) {
        if (m_globalModel->data(m_globalModel->index(i, BundleModel::COL_NAME),
                                Qt::DisplayRole).toString().contains(text, Qt::CaseInsensitive)) {
            ui->tvw_global->setRowHidden(i, ui->tvw_global->rootIndex(), false);
        } else {
            ui->tvw_global->setRowHidden(i, ui->tvw_global->rootIndex(), true);
        }
    }

    if (text.isEmpty()) {
        clrFilterBtn->hide();
    } else {
        clrFilterBtn->show();
    }

    ui->cbx_global->setChecked(isCheckedAll(ui->tvw_global));
}

void DialogBundleView::setAllChecked(QTreeView *tvw, bool checked) {
    BundleModel *model = dynamic_cast<BundleModel *>(tvw->model());
    for (int i = 0; i < model->rowCount(); ++i) {
        bool isHidden = tvw->isRowHidden(i, tvw->rootIndex());
        if (isHidden) {
            continue;
        }

        if (checked)
            model->item(i, BundleModel::COL_CHECKABLE)->setCheckState(Qt::Checked);
        else {
            model->item(i, BundleModel::COL_CHECKABLE)->setCheckState(Qt::Unchecked);
        }
    }
}

bool DialogBundleView::isCheckedAll(QTreeView *tvw) {
    if (tvw->model()->rowCount() == 0)
        return false;

    for (int i = 0; i < tvw->model()->rowCount(); ++i) {
        bool isHidden = tvw->isRowHidden(i, tvw->rootIndex());
        if (isHidden) {
            continue;
        }

        bool checked = tvw->model()->data(tvw->model()->index(i, BundleModel::COL_CHECKABLE),
                                         Qt::CheckStateRole).toBool();
        if (!checked)
            return false;
    }

    return true;
}

void DialogBundleView::slot_addGlobal() {
    for (int i = 0; i < m_globalModel->rowCount(); ++i) {
        bool checked = m_globalModel->data(m_globalModel->index(i , BundleModel::COL_CHECKABLE), Qt::CheckStateRole).toBool();
        QString id = m_globalModel->data(m_globalModel->index(i , BundleModel::COL_ID), Qt::UserRole).toString();
        if (checked) {
            BundleItem *item = m_globalBundle->getApp(id);
            if (item != NULL) {
                addApp(item);
            }
        }
    }
    ui->cbx_bdl->setChecked(false);
}

void DialogBundleView::slot_addLocal() {
    QStringList files = QFileDialog::getOpenFileNames(this,
                                                     tr("Open Local App"),
                                                     qApp->applicationDirPath(),
                                                     "Android app(*.apk)",
                                                     0,
                                                     0);
    if(0 != files.size()) {
        DialogProgress *dlg = new DialogProgress(this);
        dlg->setStopable(false);

        successedCnt = 0;
        upgradedCnt = 0;
        ignoredCnt = 0;

        LoadAppThread *thread = new LoadAppThread(files);
        connect(thread, SIGNAL(signal_parseAppFinished(BundleItem *)),
                this, SLOT(slot_onParseAppFinished(BundleItem *)));
        connect(thread, SIGNAL(signal_progressChanged(int, int)),
                dlg, SLOT(slot_setProgress(int, int)));
        connect(thread, SIGNAL(signal_stateChanged(QString)),
                dlg, SLOT(slot_setState(QString)));
        connect(thread, SIGNAL(finished()),
                dlg, SLOT(accept()));
        thread->start();

        if (dlg->exec() == QDialog::Accepted) {
            QString successedHint = tr("%1 be added").arg(successedCnt);
            QString upgradedHint = tr(", %1 be updated").arg(upgradedCnt);
            QString ignoredHint = tr(", %1 be ignored").arg(ignoredCnt);
            if (0 == upgradedCnt) {
                upgradedHint.clear();
            }
            if (0 == ignoredCnt) {
                ignoredHint.clear();
            }
            QString hint = successedHint + upgradedHint + ignoredHint + ".";
            QMessageBox::information(this, tr("Add Finished"), hint, QMessageBox::Ok);
        }
        delete dlg;
    }
}

void DialogBundleView::slot_onParseAppFinished(BundleItem *item) {
    // check in global.
    QList<BundleItem *> globalItems = m_globalBundle->getAppList();
    foreach (BundleItem *globalItem, globalItems) {
        if (item->pkg == globalItem->pkg) {
#if 0 //注释原因， 杭州这边套餐中的应用有时会发生，新上架的包版本比原来版本的包版本号低，从而指向一个不存在的路径
            if (item->version.compare(globalItem->version) <= 0) {
                delete item;
                item = globalItem;
            } else {
                item->id = globalItem->id;
            }
            break;
#endif
        }
    }

    addApp(item);
}

void DialogBundleView::slot_refreshGlobalView() {
    initGlobalView();
    slot_onFilterTextChanged(ui->ledt_filter->text());
}

void DialogBundleView::slot_refreshSpecialView() {
    ui->btn_refresh->setEnabled(false);

    m_specialModel->removeRows(0, m_specialModel->rowCount());
    for (int i = 0; i < m_appList.count(); ++i) {
        m_specialModel->setBundleItemData(m_appList.at(i));
    }
    resizeColumn(ui->tvw_special);

    ui->btn_refresh->setEnabled(true);

    slot_onChkedStateChanged();
    ui->cbx_bdl->setChecked(isCheckedAll(ui->tvw_special));
}

bool DialogBundleView::maybeSave() {
    // check if new bundle
    if (m_bNewBundle)
        return true;

    // check name
    if (ui->ledt_name->isModified())
        return true;

    // check enable
    if (m_specialBundle->enable != ui->cbx_enable->isChecked()) {
        return true;
    }

    // check item
    int newAppCnts = m_appList.count();
    int oldAppCnts = m_specialBundle->getAppIds().count();
    if (newAppCnts != oldAppCnts) {
        return true;
    } else {
        // --check version changed
        foreach (BundleItem *newItem, m_appList) {
            BundleItem *oldItem = m_specialBundle->getApp(newItem->id);
			if(!oldItem){
				return true;
				//continue;
			}
            if (newItem->version != oldItem->version) {
                return true;
            }
        }
    }

    return false;
}

void DialogBundleView::slot_saveBundle() {
    // save special
    m_specialBundle->name = ui->ledt_name->text();
    m_specialBundle->enable = ui->cbx_enable->isChecked();
    m_specialBundle->removeApps(m_specialBundle->getAppIds()); // --clear
    foreach (BundleItem *listApp, m_appList) { // --add

            m_specialBundle->addApp(listApp);
    }
    m_config->saveBundle(m_specialBundle);

    // save global
    QList<BundleItem *> specialItems = m_specialBundle->getAppList();
    foreach (BundleItem *specialItem, specialItems) {
        bool bInGlobal = false;
        bool bUpgraded = false;
        BundleItem *globalItem = m_globalBundle->getApp(specialItem->id);
        if (globalItem != NULL) {
            bInGlobal = true;
            if (globalItem->version.compare(specialItem->version) < 0) {
                bUpgraded = true;
                m_globalBundle->removeApp(globalItem->id);
                m_config->deleteBundleItems(QStringList() << globalItem->id);
            }
        }
        if (!bInGlobal || bUpgraded) {
            m_globalBundle->addApp(specialItem);
            m_config->saveBundleItem(specialItem);
        }
    }

    emit signal_bundleChanged();    
}

void DialogBundleView::slot_onChkedStateChanged() {
    QString hint = tr("The bundle have %1 items, total size is %2");
    quint64 size = 0;
    for (int i = 0; i < m_appList.count(); ++i) {
        size += m_appList.at(i)->size;
    }

    ui->lbl_state->setText(hint.arg(QString::number(m_appList.count())).arg(ZStringUtil::getSizeStr(size)));
}

void DialogBundleView::addApp(BundleItem *item) {
    if (isNeedAddToBundle(item)) {
        for (int i = 0; i < m_appList.count(); ++i) {
            if (m_appList.at(i)->id == item->id) {
                m_appList.removeAt(i);
                break;
            }
        }
        m_appList.append(item);

        m_specialModel->setBundleItemData(item);
        resizeColumn(ui->tvw_special);
        slot_onChkedStateChanged();
    }
}

void DialogBundleView::removeApps() {
    for (int i = m_specialModel->rowCount(); i >= 0; --i) {
        bool checked = m_specialModel->data(m_specialModel->index(i, BundleModel::COL_CHECKABLE), Qt::CheckStateRole).toBool();
        QString id = m_specialModel->data(m_specialModel->index(i, BundleModel::COL_ID), Qt::UserRole).toString();

        if (checked) {
            m_specialModel->mutex.lock();
            m_specialModel->removeRow(i);
            m_specialModel->mutex.unlock();
            for (int i = 0; i < m_appList.count(); ++i) {
                if (m_appList.at(i)->id == id) {
                    m_appList.removeAt(i);
                    break;
                }
            } // for
        } // if
    }

    slot_onChkedStateChanged();
    ui->cbx_bdl->setChecked(isCheckedAll(ui->tvw_special));
}

bool DialogBundleView::isNeedAddToBundle(BundleItem *item) {
    // check in special.
    for (int i = 0; i < m_appList.count(); ++i) {
        BundleItem *specialItem = m_appList.at(i);
        if (specialItem->pkg == item->pkg) {
            if (item->version.compare(specialItem->version) <= 0) {
                item = specialItem;
                ignoredCnt++;
                return false;
            } else {
                item->id = specialItem->id;
                upgradedCnt++;
                return true;
            }
        }
    }

    successedCnt++;
    return true;
}

void DialogBundleView::resizeColumn(QTreeView *tvw) {
    for (int i = 0; i < tvw->model()->columnCount(); ++i) {
        tvw->resizeColumnToContents(i);
    }
}

void DialogBundleView::slot_onSplitterMoved(int pos, int /*index*/) {
    int x = pos - 16 - 4;
    int y = (ui->ledt_filter->y() + ui->ledt_filter->height() - 16) / 2;
    clrFilterBtn->move(x, y);
}


// Model
static QString getStrStatus(int iStatus) {
    QString ret;
    switch (iStatus) {
    case 0:
        ret = QObject::tr("pending");
        break;
    case 1:
        ret = QObject::tr("downloading");
        break;
    case 2:
        ret = QObject::tr("stopped");
        break;
    case 3:
        ret = QObject::tr("failed");
        break;
    case 4:
        ret = QObject::tr("finished");
        break;
    default:
        ;
    }
    return ret;
}

BundleModel::BundleModel(QTreeView *tvw, QObject *parent) :
    QStandardItemModel(parent)
  , m_tvw(tvw)
{
    setColumnCount(COL_END);
    setHeaderData(COL_NAME, Qt::Horizontal, tr("Name"));
    setHeaderData(COL_SIZE, Qt::Horizontal, tr("Size"));
    setHeaderData(COL_VERSION, Qt::Horizontal, tr("Version"));
    setHeaderData(COL_STATUS, Qt::Horizontal, tr("Status"));
    setHeaderData(COL_INSLOCATION,Qt::Horizontal, tr("instLocation"));
    setHeaderData(COL_TIME, Qt::Horizontal, tr("Time"));

    setSortRole(Qt::UserRole + 1);
}

void BundleModel::setBundleData(Bundle *bundle) {
    removeRows(0, rowCount());

    QList<BundleItem *> list = bundle->getAppList();
    for (int i = 0; i < list.count(); ++i) {
        setBundleItemData(list.at(i));
    }
}

void BundleModel::setBundleItemData(BundleItem *bundleItem) {
    mutex.lock();

    int row = -1;
    // check whether the item in model.
    for (int i = 0; i < rowCount(); ++i) {
        QString id = data(index(i, COL_ID), Qt::UserRole).toString();
        if (id == bundleItem->id) {
            row = i;
            break;
        }
    }

    if (-1 == row) {
        row = rowCount();
        insertRow(row);
    }

    setData(index(row, COL_NAME), bundleItem->name, Qt::DisplayRole);
    item(row, COL_ID)->setData(bundleItem->id, Qt::UserRole);
    item(row, COL_PINYIN)->setData(ZStringUtil::getPinyin(bundleItem->name), Qt::UserRole + 1);
    item(row, COL_CHECKABLE)->setData(Qt::Unchecked, Qt::CheckStateRole);
    QIcon icon = QIcon(QPixmap::fromImage(bundleItem->icon));
    if (icon.isNull()) {
        icon = QIcon(":/skins/skins/icon_downloading_app.png");
    }
    item(row, COL_ICON)->setIcon(icon);

    setData(index(row, COL_SIZE), ZStringUtil::getSizeStr(bundleItem->size), Qt::DisplayRole);
    item(row, COL_SIZE)->setData(bundleItem->size, Qt::UserRole + 1);

    setData(index(row, COL_VERSION), bundleItem->version, Qt::DisplayRole);
    item(row, COL_VERSION)->setData(bundleItem->version, Qt::UserRole + 1);

    setData(index(row, COL_STATUS), getStrStatus(bundleItem->download_status), Qt::DisplayRole);
    item(row, COL_STATUS)->setData(bundleItem->download_status, Qt::UserRole + 1);

    setData(index(row,COL_INSLOCATION),
            bundleItem->instLocation == BundleItem::INST_LOCATION_SYSTEM
            ? tr("prefer system"): tr("normal"), Qt::DisplayRole);
    item(row,COL_INSLOCATION)->setData(bundleItem->instLocation, Qt::UserRole + 1);

    QDateTime tm = QDateTime::fromTime_t(bundleItem->mtime);
    setData(index(row, COL_TIME), bundleItem->mtime == 0 ? QString("") : tm.toString("yyyy/MM/dd hh:mm:ss"), Qt::DisplayRole);
    item(row, COL_TIME)->setData(bundleItem->mtime, Qt::UserRole + 1);

    //m_tv->scrollTo(index(row, 0));
    mutex.unlock();
}


// Thread
LoadAppThread::LoadAppThread(QStringList pathList, QObject *parent) :
    QThread(parent)
  , m_pathList(pathList)
{
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

void LoadAppThread::run() {
    QStringList::const_iterator constIter;
    int i = 0;
    for (constIter = m_pathList.constBegin();
         constIter != m_pathList.constEnd();
         ++constIter) {
        QString path = QString((*constIter).constData());

        BundleItem *item = new BundleItem;
        if (item->parseApk(path, true)) {
            item->id = QUuid::createUuid().toString();
            item->download_status = ZDownloadTask::STAT_FINISHED;
            emit signal_parseAppFinished(item);
            emit signal_stateChanged(item->name);
        } else {
            delete item;
        }

        emit signal_progressChanged(i, m_pathList.count());
        ++i;
    }
}

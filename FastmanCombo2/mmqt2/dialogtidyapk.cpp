#include "dialogtidyapk.h"
#include "ui_dialogtidyapk.h"

#include "globalcontroller.h"
#include "appconfig.h"
#include "bundle.h"
#include "zstringutil.h"
#include "zdownloadmanager.h"
#include <QMouseEvent>
#include <QPoint>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QIcon>
#include <QPixmap>
#include <QMutex>
#include <QMessageBox>
#include <QDateTime>

DialogTidyApk::DialogTidyApk(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogTidyApk)
  , m_isRefreshing(false)
  , m_dragged(false)
  , m_isDraggingWindow(false)
  , m_config(0)
  , m_globalBundle(0)
  , m_model(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TranslucentBackground);

    m_config = GlobalController::getInstance()->appconfig;
    m_globalBundle = GlobalController::getInstance()->appconfig->globalBundle;

    m_model = new QStandardItemModel;
    m_model->setColumnCount(COL_END);
    m_model->setSortRole(Qt::UserRole + 1);
    m_model->setHorizontalHeaderLabels(QStringList() << "Name" << "Size" << "Version" << "Time");

    ui->treeView->setModel(m_model);

    initialModel();

    connect(ui->treeView, SIGNAL(clicked(QModelIndex)), SLOT(slot_itemClicked(QModelIndex)));
    connect(ui->btn_max, SIGNAL(clicked()), SLOT(slot_btnClicked()));
    connect(ui->btn_close, SIGNAL(clicked()), SLOT(slot_btnClicked()));
    connect(ui->btn_checkAll, SIGNAL(clicked()), SLOT(slot_checkedAll()));
    connect(ui->btn_invertChk, SIGNAL(clicked()), SLOT(slot_invertChk()));
    connect(ui->btn_refresh, SIGNAL(clicked()), SLOT(slot_refresh()));
    connect(ui->btn_remove, SIGNAL(clicked()), SLOT(slot_remove()));
}

DialogTidyApk::~DialogTidyApk()
{
    delete ui;
}

bool DialogTidyApk::isInRange(QWidget *widget, int x, int y) {
    int x1 = widget->pos().x();
    int x2 = x1 + widget->width();
    int y1 = widget->pos().y();
    int y2 = y1 + widget->height();

    if(x >= x1 && x <= x2 && y >= y1 && y <= y2) {
        return true;
    }
    return false;
}

void DialogTidyApk::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastPoint = event->globalPos() - this->pos();
        event->accept();
        m_dragged = true;
        mouseX = event->x();
        mouseY = event->y();
        m_isDraggingWindow = isInRange(ui->wdt_title, mouseX, mouseY);
    }
}

void DialogTidyApk::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragged && m_isDraggingWindow
            && (event->buttons() & Qt::LeftButton)) {
        this->move(event->globalPos() - m_lastPoint);
        event->accept();
    }
}

void DialogTidyApk::mouseReleaseEvent(QMouseEvent *) {
    m_dragged = false;
    m_isDraggingWindow = false;
}

void DialogTidyApk::keyPressEvent(QKeyEvent *e) {
    e->ignore();
}

void DialogTidyApk::initialModel() {
    m_apps.clear();

    mutex.lock();
    m_model->removeRows(0, m_model->rowCount());    
    mutex.unlock();

    getUnusedApk();

    for (int i = 0; i < m_apps.count(); ++i) {
        addItem(m_apps[i], i);
    }
}

void DialogTidyApk::addItem(BundleItem *item, int row) {
    mutex.lock();
    m_model->insertRow(row);

    m_model->setData(m_model->index(row, COL_NAME), item->name, Qt::DisplayRole);
    m_model->item(row, COL_ID)->setData(item->id, Qt::UserRole);
    m_model->setData(m_model->index(row, COL_CHECKABLE), Qt::Unchecked, Qt::CheckStateRole);
    m_model->item(row, COL_PINYIN)->setData(ZStringUtil::getPinyin(item->name), Qt::UserRole + 1);
    if (!item->icon.isNull())
        m_model->item(row, COL_ICON)->setIcon(QIcon(QPixmap::fromImage(item->icon)));
    else
        m_model->item(row, COL_ICON)->setIcon(QIcon(":/skins/skins/icon_downloading_app.png"));

    m_model->setData(m_model->index(row, COL_SIZE), ZStringUtil::getSizeStr(item->size), Qt::DisplayRole);
    m_model->item(row, COL_SIZE)->setData(item->size, Qt::UserRole + 1);

    m_model->setData(m_model->index(row, COL_VERSION), item->version, Qt::DisplayRole);
    m_model->item(row, COL_VERSION)->setData(item->version, Qt::UserRole + 1);

    QDateTime tm = QDateTime::fromTime_t(item->mtime);
    m_model->setData(m_model->index(row, COL_TIME), item->mtime == 0 ? QString("") : tm.toString("yyyy/MM/dd hh:mm:ss"), Qt::DisplayRole);
    m_model->item(row, COL_TIME)->setData(item->mtime, Qt::UserRole + 1);

    resizeColumn();
    mutex.unlock();
}

void DialogTidyApk::removeItem(BundleItem *item) {
    mutex.lock();

    for (int i = 0; i < m_model->rowCount(); ++i) {
        QString id = m_model->data(m_model->index(i, COL_ID), Qt::UserRole).toString();
        if (id == item->id) {
            m_model->removeRow(i);
            break;
        }
    }

    int i = 0;
    foreach (BundleItem *itemTmp, m_apps) {
        if (itemTmp->id == item->id) {
            m_apps.removeAt(i);
            break;
        }
        i++;
    }

    m_config->deleteBundleItems(QStringList() << item->id);

    mutex.unlock();
}

void DialogTidyApk::removeItems(QList<BundleItem *> appList) {
    foreach (BundleItem *item, appList) {
        removeItem(item);
    }
}

void DialogTidyApk::getUnusedApk() {
    QList<BundleItem *> apps = m_globalBundle->getAppList();
    QList<Bundle*> bundles = m_globalBundle->getBundleList();

    for(int i = 0; i < apps.size(); ++i) {
        BundleItem *item = apps[i];
        bool found = false;
        foreach(Bundle *bundle, bundles) {
            if(bundle->getApp(item->id) != NULL) {
                found = true;
                break;
            }
        }

        if(!found && item->download_status == ZDownloadTask::STAT_FINISHED) {
            m_apps.append(item);
        }
    }
}

int DialogTidyApk::getCheckedApps(QList<BundleItem *> &apps) {
    int appCnt = 0;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (m_model->data(m_model->index(i, COL_CHECKABLE), Qt::CheckStateRole).toBool()) {
            QString id = m_model->data(m_model->index(i, COL_ID), Qt::UserRole).toString();
            foreach (BundleItem *item, m_apps) {
                if (item->id == id) {
                    appCnt++;
                    apps.append(item);
                    break;
                }
            } // foreach
        }
    } // for
    return appCnt;
}

QStringList DialogTidyApk::getIds(QList<BundleItem *> &apps) {
    QStringList ret;
    foreach (BundleItem *item, apps) {
        ret.append(item->id);
    }
    return ret;
}

void DialogTidyApk::slot_itemClicked(const QModelIndex &index) {
    bool checked = m_model->data(m_model->index(index.row(), COL_CHECKABLE), Qt::CheckStateRole).toBool();
    if (checked)
        m_model->item(index.row(), COL_CHECKABLE)->setData(Qt::Unchecked, Qt::CheckStateRole);
    else
        m_model->item(index.row(), COL_CHECKABLE)->setData(Qt::Checked, Qt::CheckStateRole);
}

void DialogTidyApk::slot_checkedAll() {
    for (int i = 0; i < m_model->rowCount(); ++i) {
        m_model->item(i, COL_CHECKABLE)->setData(Qt::Checked, Qt::CheckStateRole);
    }
}

void DialogTidyApk::slot_invertChk() {
    int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        bool checked = m_model->data(m_model->index(row, COL_CHECKABLE), Qt::CheckStateRole).toBool();
        if (checked)
            m_model->setData(m_model->index(row, COL_CHECKABLE), Qt::Unchecked, Qt::CheckStateRole);
        else
            m_model->setData(m_model->index(row, COL_CHECKABLE), Qt::Checked, Qt::CheckStateRole);
    }
}

bool DialogTidyApk::isAllChecked() {
    bool ret = true;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        bool checked = m_model->data(m_model->index(i, COL_CHECKABLE), Qt::CheckStateRole).toBool();
        if (!checked) {
            ret = false;
            break;
        }
    }
   return ret;
}

void DialogTidyApk::slot_refresh() {
    if (!m_isRefreshing) {
        m_isRefreshing = true;
        initialModel();
        m_isRefreshing = false;
    }
}

void DialogTidyApk::slot_remove() {
    QList<BundleItem *> apps;
    int appCnt = getCheckedApps(apps);
    if (0 == appCnt) {
        return;
    } else if (QMessageBox::question(this,
                                   tr("Remove apps"),
                                   tr("Are you sure remove?"),
                                   QMessageBox::Ok, QMessageBox::Cancel)
             == QMessageBox::Ok) {

        QStringList ids = getIds(apps);
        m_globalBundle->removeApps(ids);
        removeItems(apps);

        while(!apps.isEmpty()) {
            delete apps.takeFirst();
        }
    }
}

void DialogTidyApk::slot_btnClicked() {
    QWidget *btn = dynamic_cast<QWidget *>(sender());
    if (btn == ui->btn_max) {
        if (this->isMaximized())
            this->showNormal();
        else
            this->showMaximized();
    } else if (btn == ui->btn_close) {
        this->close();
    }
}

void DialogTidyApk::resizeColumn() {
    for (int i = 0; i < m_model->columnCount(); ++i) {
        ui->treeView->resizeColumnToContents(i);
    }
}



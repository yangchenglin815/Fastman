#include "formbundlelist.h"
#include "ui_formbundlelist.h"
#include "formwebapps.h"
#include "dialogbundleview.h"
#include "globalcontroller.h"
#include "appconfig.h"
#include "loginmanager.h"
#include "zstringutil.h"
#include "zlog.h"

#include <QMessageBox>
#include <QTimer>
#include <QUuid>

BundleListModel::BundleListModel(QObject *parent)
    : QStandardItemModel(parent) {
    controller = GlobalController::getInstance();

    setSortRole(Qt::UserRole + 1);
    setColumnCount(COL_EDIT + 1);

    QStringList labels;
    labels.append(tr("name"));
    labels.append(tr("description"));
    labels.append(tr("type"));
    labels.append(tr("enabled"));
    labels.append(tr("size"));
    labels.append(tr("progress"));
    labels.append(tr("actions"));
    setHorizontalHeaderLabels(labels);
}

void BundleListModel::slot_setRowData(Bundle *bundle) {
    mutex.lock();
    int row = -1;
    int i = 0;
    QStandardItem *item = NULL;
    while((item = this->item(i++, COL_NAME)) != NULL) {
        if(item->data(Qt::UserRole).toString() == bundle->id) {
            row = --i;
            break;
        }
    }

    if(row == -1) {
        row = rowCount();
        insertRow(row);
    }

    if(checkList.contains(bundle->id)) {
        setData(index(row, COL_NAME), Qt::Checked, Qt::CheckStateRole);
    } else {
        setData(index(row, COL_NAME), Qt::Unchecked, Qt::CheckStateRole);
    }

    setData(index(row, COL_NAME), bundle->id, Qt::UserRole);
    setData(index(row, COL_NAME), bundle->name);
    setData(index(row, COL_NAME), ZStringUtil::getPinyin(bundle->name), Qt::UserRole+1);

    setData(index(row, COL_DESC), bundle->getDescription());
    setData(index(row, COL_DESC), 0, Qt::UserRole+1);

    int typeInt = (bundle->type << 4) | (int)bundle->readonly;
    QString typeName = bundle->readonly ? tr("preload") : tr("custom");
    if(bundle->type == Bundle::TYPE_AGG) {
        typeName += tr(" AGG");
    } else if(bundle->type == Bundle::TYPE_FORCE) {
        typeName += tr(" Force");
    }

    setData(index(row, COL_TYPE), typeName);
    setData(index(row, COL_TYPE), typeInt, Qt::UserRole+1);

    setData(index(row, COL_ENABLED), bundle->enable ? tr("enabled") : tr("disabled"));
    setData(index(row, COL_ENABLED), bundle->enable, Qt::UserRole+1);

    setData(index(row, COL_SIZE), ZStringUtil::getSizeStr(bundle->size));
    setData(index(row, COL_SIZE), bundle->size, Qt::UserRole+1);

    setData(index(row, COL_EDIT), tr("Edit"));
    setData(index(row, COL_EDIT), 0, Qt::UserRole+1);

    this->item(row, COL_NAME)->setIcon(QIcon(Bundle::getIcon(row)));
    this->item(row, COL_EDIT)->setIcon(QIcon(":/skins/skins/icon_edit.png"));
    mutex.unlock();
}

void BundleListModel::slot_clicked(const QModelIndex &index) {    
    QStandardItem *item = this->item(index.row(), COL_NAME);
    QString id = item->data(Qt::UserRole).toString();

    if(index.column() == COL_EDIT) {
        emit signal_loadBundleView(id);
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

void BundleListModel::slot_selAll() {
    int i = 0;
    QStandardItem *item = NULL;
    checkList.clear();

    while((item = this->item(i++, COL_NAME)) != NULL) {
        item->setCheckState(Qt::Checked);
        checkList.append(item->data(Qt::UserRole).toString());
    }
}

void BundleListModel::slot_selRev() {
    int i = 0;
    QStandardItem *item = NULL;
    checkList.clear();

    while((item = this->item(i++, COL_NAME)) != NULL) {
        if(item->checkState() == Qt::Unchecked) {
            item->setCheckState(Qt::Checked);
            checkList.append(item->data(Qt::UserRole).toString());
        } else {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

FormBundleList::FormBundleList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormBundleList)
{
    ui->setupUi(this);
    controller = GlobalController::getInstance();

    model = new BundleListModel(this);
    ui->tv_bundlelist->setModel(model);
    connect(ui->tv_bundlelist, SIGNAL(clicked(QModelIndex)), model, SLOT(slot_clicked(QModelIndex)));
    connect(model, SIGNAL(signal_loadBundleView(QString)), this, SLOT(slot_loadBundleView(QString)));
    connect(ui->btn_sel_all, SIGNAL(clicked()), model, SLOT(slot_selAll()));
    connect(ui->btn_sel_rev, SIGNAL(clicked()), model, SLOT(slot_selRev()));

    connect(ui->btn_enable, SIGNAL(clicked()), SLOT(slot_enable()));
    connect(ui->btn_disable, SIGNAL(clicked()), SLOT(slot_disable()));
    connect(ui->btn_del, SIGNAL(clicked()), SLOT(slot_delete()));

    if(controller->loginManager->disableBundleDiy) {
        ui->btn_new_bundle->hide();
    }
    if(controller->loginManager->disableNavApps) {
        ui->btn_web_apps->hide();
    } else {
        FormWebApps *page_webapps = new FormWebApps(this);
        ui->stackedWidget->addWidget(page_webapps);
    }

    connect(ui->btn_bundle_list, SIGNAL(clicked()), SLOT(slot_pageToggled()));
    connect(ui->btn_web_apps, SIGNAL(clicked()), SLOT(slot_pageToggled()));
    connect(ui->btn_new_bundle, SIGNAL(clicked()), SLOT(slot_pageToggled()));

    QTimer::singleShot(20, this, SLOT(slot_refreshBundleList()));
}

FormBundleList::~FormBundleList()
{
    delete ui;
}

void FormBundleList::slot_pageToggled() {
    QToolButton *btn = (QToolButton *)sender();

    if(btn == ui->btn_bundle_list) {
        ui->stackedWidget->setCurrentIndex(0);
        ui->btn_bundle_list->setChecked(true);
        ui->btn_web_apps->setChecked(false);
    } else if(btn == ui->btn_web_apps) {
        ui->stackedWidget->setCurrentIndex(1);
        ui->btn_bundle_list->setChecked(false);
        ui->btn_web_apps->setChecked(true);
    } else if (btn == ui->btn_new_bundle) {
        Bundle *cBundle = new Bundle;
        cBundle->id = QUuid::createUuid().toString();
        cBundle->name = QObject::tr("New Bundle");
        controller->appconfig->globalBundle->addBundle(cBundle);
        slot_loadBundleView(cBundle->id, true);
    }
}

void FormBundleList::slot_enable() {
    if(model->checkList.isEmpty()) {
        return;
    }

    QList<Bundle*> checkedBundles = controller->appconfig->globalBundle->getBundleList(model->checkList);
    foreach(Bundle* bundle, checkedBundles) {
        bundle->enable = true;
        model->slot_setRowData(bundle);
    }
    emit signal_refreshBundleList();
}

void FormBundleList::slot_disable() {
    if(model->checkList.isEmpty()) {
        return;
    }

    QList<Bundle*> checkedBundles = controller->appconfig->globalBundle->getBundleList(model->checkList);
    foreach(Bundle* bundle, checkedBundles) {
        bundle->enable = false;
        model->slot_setRowData(bundle);
    }
    emit signal_refreshBundleList();
}

void FormBundleList::slot_delete() {
    if(model->checkList.isEmpty()) {
        return;
    }

    int r = QMessageBox::question(this, tr("delete bundle"),
                                  tr("Are you sure to delete selected bundles?\n"
                                     "Note that preloaded bundles wouldn't be deleted."),
                                  QMessageBox::Yes, QMessageBox::No);
    if(r != QMessageBox::Yes) {
        return;
    }

    QList<Bundle*> checkedBundles = controller->appconfig->globalBundle->getBundleList(model->checkList);
    QStringList chkBundleIds = model->checkList;
    for (int i = 0; i < checkedBundles.size(); ++i) {
        if (checkedBundles[i]->readonly == true) {
            chkBundleIds.removeOne(checkedBundles[i]->id);
        }
    }
    controller->appconfig->globalBundle->removeBundles(chkBundleIds);
    controller->appconfig->deleteBundles(chkBundleIds);
    emit signal_refreshBundleList();
    slot_refreshBundleList();
}

void FormBundleList::slot_refreshBundleList() {
    QList<Bundle *> bundles = controller->appconfig->globalBundle->getBundleList();
    model->removeRows(0, model->rowCount());

    foreach(Bundle *bundle, bundles) {
//#ifndef CMD_DEBUG
        if(bundle->type != Bundle::TYPE_APK) {
            continue;
        }
//#endif
        model->slot_setRowData(bundle);
    }

    for(int i=0; i<model->columnCount(); i++) {
        ui->tv_bundlelist->resizeColumnToContents(i);
    }
}

void FormBundleList::slot_loadBundleView(const QString &id, bool bNewBundle) {
    Bundle *bundle = controller->appconfig->globalBundle->getBundle(id);
    DialogBundleView *dlg = new DialogBundleView(bundle, bNewBundle, this);
    connect(dlg, SIGNAL(signal_bundleChanged()), this, SLOT(slot_refreshBundleList()));
    connect(dlg, SIGNAL(signal_bundleChanged()), this, SIGNAL(signal_refreshBundleList()));
    connect(this, SIGNAL(signal_itemStatusChanged(BundleItem*)), dlg, SLOT(slot_onItemStatusChanged(BundleItem*)));
    dlg->exec();
    delete dlg;
}

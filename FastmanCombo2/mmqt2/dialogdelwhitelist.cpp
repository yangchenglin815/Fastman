#include "dialogdelwhitelist.h"
#include "ui_dialogdelwhitelist.h"
#include "appconfig.h"
#include "globalcontroller.h"
#include <QLabel>
#include <QMessageBox>

dialogdelwhitelist::dialogdelwhitelist(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dialogdelwhitelist)
{
    ui->setupUi(this);

    config = GlobalController::getInstance()->appconfig;
    model = new WhitelistModel(this);
    ui->treeView->setModel(model);
    ui->treeView->header()->resizeSection(0, 320);
    ui->treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    model->infoinit(config->getSysWhiteLists());
    ui->treeView->show();


    connect(ui->btn_checkall,SIGNAL(clicked()),model,SLOT(slot_btn_checkall()));
    connect(ui->btn_invert,SIGNAL(clicked()),model,SLOT(slot_btn_invert()));
    connect(ui->btn_ok,SIGNAL(clicked()),this,SLOT(slot_btn_ok()));
    connect(ui->treeView,SIGNAL(clicked(QModelIndex)),model,SLOT(slot_clicked(QModelIndex)));
}

dialogdelwhitelist::~dialogdelwhitelist()
{
    delete ui;
}


void dialogdelwhitelist::slot_btn_ok() {
    if(model->checklist.isEmpty()) {
        return;
    }
    int ret = QMessageBox::question(this, tr("Delete List"), tr("Delete your choices ?"),
                          QMessageBox::Ok,QMessageBox::No);
    if(ret != QMessageBox::Ok) {
        return;
    }
    foreach (QString str, model->checklist) {
        config->deleteSysWhiteList(str);
        for(int i = 0; i < model->rowCount(); i++) {
            if(str == model->item(i)->data(Qt::DisplayRole).toString()) {
                model->removeRow(i);
            }
        }
    }
    model->checklist.clear();
}

WhitelistModel::WhitelistModel(QWidget *parent) :
    QStandardItemModel(parent)
{
    QStringList label;
    label.append(tr("fingerprint"));
    label.append(tr("whitelist"));
    this->setHorizontalHeaderLabels(label);
    this->setColumnCount(COL_WHITELIST + 1);
}

void WhitelistModel::slot_clicked(QModelIndex index) {
    bool check = this->item(index.row())->data(Qt::CheckStateRole).toBool();
    QString str = this->item(index.row(), COL_FINGERPRINT)->data(Qt::DisplayRole).toString();
    if(check) {
        this->item(index.row(),COL_CHECK)->setData(Qt::Unchecked, Qt::CheckStateRole);
        checklist.removeOne(str);
    } else {
        this->item(index.row(),COL_CHECK)->setData(Qt::Checked,Qt::CheckStateRole);
        checklist.append(str);
    }
}

void WhitelistModel::infoinit(QList<DoNotClean *> info) {
    mutex.lock();
    int i = 0;
    foreach (DoNotClean *tmp, info) {
        this->setItem(i, COL_FINGERPRINT, new QStandardItem(tmp->fingerprint));
        this->item(i, COL_CHECK)->setData(Qt::Unchecked, Qt::CheckStateRole);
        this->setItem(i, COL_WHITELIST, new QStandardItem(tmp->pkgList.join(",")));
        i++;
    }
    mutex.unlock();
}

void WhitelistModel::slot_btn_checkall() {
    checklist.clear();
    for(int i = 0; i<this->rowCount(); i++) {
        this->item(i, COL_CHECK)->setCheckState(Qt::Checked);
        checklist.append(this->item(i, COL_FINGERPRINT)->data(Qt::DisplayRole).toString());
    }
}

void WhitelistModel::slot_btn_invert() {
    checklist.clear();
    for(int i = 0; i<this->rowCount(); i++){
        if(this->data(this->index(i, COL_CHECK), Qt::CheckStateRole).toBool()) {
            this->item(i, COL_CHECK)->setCheckState(Qt::Unchecked);
        } else {
            this->item(i, COL_CHECK)->setCheckState(Qt::Checked);
            checklist.append(this->data(this->index(i, COL_FINGERPRINT), Qt::DisplayRole).toString());
        }
    }
}


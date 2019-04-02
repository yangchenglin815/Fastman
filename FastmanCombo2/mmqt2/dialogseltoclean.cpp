#include "dialogseltoclean.h"
#include "ui_dialogseltoclean.h"

#include "fastman2.h"
#include "zstringutil.h"
#include "zadbdevicenode.h"
#include "appconfig.h"
#include "globalcontroller.h"

DialogSelToClean::DialogSelToClean(void *data, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogSelToClean)
{
    ui->setupUi(this);
    controller = GlobalController::getInstance();
    param = (DlgSelCleanParam*)data;

    model = new AgentAppModel(this);
    ui->tv_applist->setModel(model);
    model->setAppListData(*param->list);
    for(int i=0; i<model->columnCount(); i++) {
        ui->tv_applist->resizeColumnToContents(i);
    }
    model->slot_selAll();

    ui->label_name->setText(param->node->getName());
    ui->label_signature->setText(param->node->ag_info.fingerprint);

    connect(ui->btn_selall, SIGNAL(clicked()), model, SLOT(slot_selAll()));
    connect(ui->btn_selrev, SIGNAL(clicked()), model, SLOT(slot_selRev()));
    connect(ui->tv_applist, SIGNAL(clicked(QModelIndex)), model, SLOT(slot_clicked(QModelIndex)));

    connect(ui->btn_ok, SIGNAL(clicked()), SLOT(slot_ok()));
}

DialogSelToClean::~DialogSelToClean()
{
    delete ui;
}

void DialogSelToClean::slot_ok() {
    param->result.append("com.dx.agent2");
    param->result.append(model->checkList);
    if(ui->checkBox->isChecked()) {
        controller->appconfig->saveSysWhiteList(param->node->ag_info.fingerprint, param->result);
    }
    done(QDialog::Accepted);
}

AgentAppModel::AgentAppModel(QObject *parent)
    :QStandardItemModel(parent) {
    setSortRole(Qt::UserRole+1);
    setColumnCount(COL_TIME+1);

    QStringList labels;
    labels.append(tr("name"));
    labels.append(tr("type"));
    labels.append(tr("version"));
    labels.append(tr("size"));
    labels.append(tr("time"));
    setHorizontalHeaderLabels(labels);
}

void AgentAppModel::setAppListData(QList<AGENT_APP_INFO *> appList) {
    mutex.lock();
    foreach(AGENT_APP_INFO *app, appList) {
        if(app->getType() == AGENT_APP_INFO::TYPE_USER_APP) {
            continue;
        }

        int row = -1;
        int i = 0;
        QStandardItem *item = NULL;
        while((item = this->item(i++, COL_NAME)) != NULL) {
            if(item->data(Qt::UserRole).toString() == app->packageName) {
                row = --i;
                break;
            }
        }
        if(row == -1) {
            row = rowCount();
            insertRow(row);
        }

        if(checkList.contains(app->packageName)) {
            setData(index(row, COL_NAME), Qt::Checked, Qt::CheckStateRole);
        } else {
            setData(index(row, COL_NAME), Qt::Unchecked, Qt::CheckStateRole);
        }

        if(app->icon.isNull()) {
            this->item(row, COL_NAME)->setIcon(QIcon(":/skins/skins/icon_downloading_app.png"));
        } else {
            this->item(row, COL_NAME)->setIcon(QIcon(QPixmap::fromImage(app->icon)));
        }
        setData(index(row, COL_NAME), app->packageName, Qt::UserRole);

        setData(index(row, COL_NAME), app->name);
        setData(index(row, COL_NAME), ZStringUtil::getPinyin(app->name), Qt::UserRole+1);

        setData(index(row, COL_TYPE), app->getTypeName());
        setData(index(row, COL_TYPE), app->getType(), Qt::UserRole+1);

        setData(index(row, COL_VER), app->versionName);
        setData(index(row, COL_VER), app->versionCode, Qt::UserRole+1);

        setData(index(row, COL_SIZE), ZStringUtil::getSizeStr(app->size));
        setData(index(row, COL_SIZE), app->size, Qt::UserRole+1);

        setData(index(row, COL_TIME), ZStringUtil::getTimeStr(app->mtime));
        setData(index(row, COL_TIME), app->mtime, Qt::UserRole+1);
    }
    mutex.unlock();
}

void AgentAppModel::slot_clicked(const QModelIndex &index) {
    QStandardItem *item = this->item(index.row(), COL_NAME);
    QString id = item->data(Qt::UserRole).toString();

    if(item->checkState() == Qt::Unchecked) {
        item->setCheckState(Qt::Checked);
        checkList.append(id);
    } else {
        item->setCheckState(Qt::Unchecked);
        checkList.removeOne(id);
    }
}

void AgentAppModel::slot_selAll() {
    int i = 0;
    QStandardItem *item = NULL;
    checkList.clear();

    while((item = this->item(i++, COL_NAME)) != NULL) {
        item->setCheckState(Qt::Checked);
        checkList.append(item->data(Qt::UserRole).toString());
    }
}

void AgentAppModel::slot_selRev() {
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

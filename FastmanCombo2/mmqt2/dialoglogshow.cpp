#include "dialoglogshow.h"
#include "ui_dialoglogshow.h"
#include "dialoglogview.h"
#include "dialoglogview.h"
#include "QJson/parser.h"
#include "globalcontroller.h"
#include "zstringutil.h"

#include <QStandardItemModel>
#include <QMessageBox>
#include <QTimer>
#include <QSqlDatabase>
#include <QDateTime>
#include <QMouseEvent>
#include <QTimer>
#include <QDate>
#include <QFileDialog>

LogModel::LogModel(QObject *parent)
    :QStandardItemModel(parent) {
    QStringList labels;
    labels.append(tr("time"));
    labels.append(tr("log"));
    this->setColumnCount(2);
    this->setHorizontalHeaderLabels(labels);
    this->setSortRole(Qt::UserRole+1);
}

QString LogModel::getTimeStr(uint time) {
    return QDateTime::fromTime_t(time).toString("yyyy/MM/dd hh:mm:ss");
}

void LogModel::setLogData(const QList<InstLog *> &logs) {
    int row = 0;
    foreach(InstLog *d, logs) {
        insertRow(row);
        setData(index(row, COL_TIME), row, Qt::UserRole);
        setData(index(row, COL_LOG), row, Qt::UserRole);

        setData(index(row, COL_TIME), getTimeStr(d->time));
        setData(index(row, COL_TIME), d->time, Qt::UserRole+1);

        setData(index(row, COL_LOG), d->result + " " + (d->isInstalled ? (d->installHint.isEmpty() ? "" : QString("[%1]").arg(d->installHint)) : ""));
        setData(index(row, COL_LOG), d->result + " " + d->json , Qt::UserRole+1);
        row++;
    }
}

DialogLogshow::DialogLogshow(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogLogshow)
{
    ui->setupUi(this);
    model = NULL;
    sort_model = NULL;
    config = GlobalController::getInstance()->appconfig;
    setAttribute(Qt::WA_TranslucentBackground);
    ui->lbl_title->setText(tr("Double click to show detail - ") + tr("Logshow"));
    ui->dateEdit2->setDate(QDate::currentDate());
    QDate d(2014, 01, 01);
    ui->dateEdit->setDate(d);
    startDate = ui->dateEdit->date();
    endDate = ui->dateEdit2->date();
    QTimer::singleShot(20, this, SLOT(slot_start()));
}

void DialogLogshow::slot_start() {
    logs = config->instLogDb->getAllData();
    logscache = config->instCacheDb->getAllData();
    modellogs.append(logs);
    modellogscache.append(logscache);

    model = new LogModel(this);
    sort_model = new QSortFilterProxyModel(this);
    sort_model->setSourceModel(model);
    sort_model->setFilterKeyColumn(LogModel::COL_LOG);
    sort_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sort_model->setDynamicSortFilter(true);
    ui->tv_log_1->setModel(sort_model);

    modelcache = new LogModel(this);
    sort_modelcache = new QSortFilterProxyModel(this);
    sort_modelcache->setSourceModel(modelcache);
    sort_modelcache->setFilterKeyColumn(LogModel::COL_LOG);
    sort_modelcache->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sort_modelcache->setDynamicSortFilter(true);
    ui->tv_log_2->setModel(sort_modelcache);

    model->setLogData(logs);
    ui->tv_log_1->resizeColumnToContents(0);

    modelcache->setLogData(logscache);
    ui->tv_log_2->resizeColumnToContents(0);

    ui->tabWidget->setTabText(0, tr("reported"));
    ui->tabWidget->setTabText(1, tr("unreport"));
    ui->tabWidget->setCurrentIndex(0);

    connect(ui->btn_close, SIGNAL(clicked()), this, SLOT(reject()));
    ui->label_tv1->setText(tr("number %1").arg(sort_model->rowCount()));
    ui->label_tv2->setText(tr("number %1").arg(sort_modelcache->rowCount()));
    connect(ui->tv_log_1, SIGNAL(doubleClicked(QModelIndex)), SLOT(slot_showDetail(QModelIndex)));
    connect(ui->tv_log_2, SIGNAL(doubleClicked(QModelIndex)), SLOT(slot_showDetail(QModelIndex)));
    connect(ui->filter_lineEdit, SIGNAL(textChanged(QString)), SLOT(slot_label(QString)));
    connect(ui->checkBox, SIGNAL(stateChanged(int)), SLOT(slot_filterData()));
    connect(ui->dateEdit, SIGNAL(dateChanged(QDate)), SLOT(slot_filterData()));
    connect(ui->dateEdit2, SIGNAL(dateChanged(QDate)), SLOT(slot_filterData()));
    connect(ui->btn_clear, SIGNAL(clicked()), SLOT(slot_clear()));
    connect(ui->btn_export, SIGNAL(clicked()), SLOT(slot_export()));
    connect(ui->checkBox_imeiFilter, SIGNAL(stateChanged(int)), SLOT(slot_filterData()));
}

void DialogLogshow::slot_label(QString str) {
    sort_model->setFilterFixedString(str);
    sort_modelcache->setFilterFixedString(str);
    ui->label_tv1->setText(tr("number %1").arg(sort_model->rowCount()));
    ui->label_tv2->setText(tr("number %1").arg(sort_modelcache->rowCount()));
}

void DialogLogshow::filterCheckBox() {
    modellogs.clear();
    modellogscache.clear();
    modellogs.append(logs);
    modellogscache.append(logscache);
    if(ui->checkBox->checkState() == Qt::Checked) {
        for(int i = 0; i < modellogs.size(); i++) {
            if(modellogs.at(i)->result != "OK") {
                modellogs.removeAt(i--);
            }
        }
    }
}

void DialogLogshow::dateTimeFilter(QDate &standardtime) {
    QDate logtime;
    QDateTime tmp;
    for(int i = 0; i < modellogs.size(); i++) {
        tmp.setTime_t(modellogs.at(i)->time);
        logtime = tmp.date();
        if(compareDate(standardtime, logtime)) {
            modellogs.removeAt(i--);
        }
    }
    for(int i = 0; i < modellogscache.size(); i++) {
        tmp.setTime_t(modellogscache.at(i)->time);
        logtime = tmp.date();
        if(compareDate(standardtime, logtime)) {
            modellogscache.removeAt(i--);
        }
    }
}

bool DialogLogshow::compareDate(QDate &starttime, QDate &modeldate) {
    QDate enddate = ui->dateEdit2->date();
    do{
        if(modeldate.year() < starttime.year()) {
            break;
        } else if(modeldate.year() == starttime.year()) {
            if(modeldate.month() < starttime.month()) {
                break;
            } else if(modeldate.month() == starttime.month()) {
                if(modeldate.day() < starttime.day()) {
                    break;
                }
            }
        }
        if(modeldate.year() > enddate.year()) {
            break;
        } else if(modeldate.year() == starttime.year()) {
            if(modeldate.month() > enddate.month()) {
                break;
            } else if(modeldate.month() == enddate.month()) {
                if(modeldate.day() > enddate.day()) {
                    break;
                }
            }
        }
        return false;
    }while(0);
    return true;
}

DialogLogshow::~DialogLogshow()
{
    while(!logs.isEmpty()) {
        delete logs.takeFirst();
    }
    while(!logscache.isEmpty()) {
        delete logscache.takeFirst();
    }
    delete ui;
}

void DialogLogshow::slot_showDetail(const QModelIndex &index) {
    QTreeView *sender = (QTreeView *)this->sender();
    InstLog* d;
    if(!sender->objectName().compare("tv_log_1")) {
        QModelIndex realIndex = sort_model->mapToSource(index);
        int row = realIndex.row();
        d = modellogs[row];
    } else {
        QModelIndex realIndex = sort_modelcache->mapToSource(index);
        int row = realIndex.row();
        d = modellogscache[row];
    }
    DialogLogView *dlg = new DialogLogView(d, this);
    dlg->exec();
    delete dlg;
}

void DialogLogshow::slot_clear() {
    ui->dateEdit2->setDate(QDate::currentDate());
    QDate d(2014, 01, 01);
    ui->dateEdit->setDate(d);
    ui->checkBox->setCheckState(Qt::Unchecked);
    ui->checkBox_imeiFilter->setCheckState(Qt::Unchecked);
}

void DialogLogshow::slot_filterData() {
    QDateTime startdate;
    QDateTime enddate;
    QDate dstart = ui->dateEdit->date();
    QDate dend = ui->dateEdit2->date();
    startdate.setDate(dstart);
    enddate.setDate(dend);
    if(startdate.toTime_t() > enddate.toTime_t()) {
        ui->dateEdit->setDate(startDate);
        ui->dateEdit2->setDate(endDate);
        return ;
    }
    filterCheckBox();
    dateTimeFilter(dstart);
    slot_imeiFilter();

    model->removeRows(0, model->rowCount());
    modelcache->removeRows(0, modelcache->rowCount());
    model->setLogData(modellogs);
    modelcache->setLogData(modellogscache);
    startDate = dstart;
    endDate = dend;
    ui->label_tv1->setText(tr("number %1").arg(sort_model->rowCount()));
    ui->label_tv2->setText(tr("number %1").arg(sort_modelcache->rowCount()));
}

void DialogLogshow::slot_export() {
    QString filepath = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                    "",
                                                    tr("Texts (*.txt)"));
    if(!filepath.isEmpty()) {
        QFile f(filepath);
        if(f.open(QIODevice::WriteOnly)) {
            int index = ui->tabWidget->currentIndex();
            InstLog *log;
            QList<InstLog *> list;
            if(0 == index) {
                list = modellogs;
            } else {
                list = modellogscache;
            }
            for(int i = list.size() - 1; i >= 0; i--) {
                log = list.at(i);
                QByteArray out;
                out.append(LogModel::getTimeStr(log->time) + "\r\n");
                out.append("adb_serial=" + log->adbSerial.toUtf8() + ";" + "result=" + log->result.toUtf8() + ";" + "json=" + log->json.toUtf8() + "\r\n");
                out.append("\r\n");
                f.write(out);
            }
            f.close();
        }
    }
}

void DialogLogshow::imeiFilter(bool isCache) {
    QList<InstLog *> list;
    if(isCache) {
        list = modellogscache;
    } else {
        list = modellogs;
    }
    QMap<uint, InstLog *> afterlist;
    QJson::Parser parser;
    QVariantMap map;
    uint imei;

    foreach (InstLog *var, list) {
        map = parser.parse(var->json.toUtf8()).toMap();
        imei = map.value("imei").toInt();
        if(afterlist.keys().contains(imei) && afterlist.value(imei)->time >= var->time) {
            continue;
        } else {
            afterlist.insert(imei, var);
        }
    }
    if(isCache) {
        modellogscache = afterlist.values();
    } else {
        modellogs = afterlist.values();
    }
}

void DialogLogshow::slot_imeiFilter() {
    bool filter = ui->checkBox_imeiFilter->checkState() == Qt::Checked;
    if(filter) {
        imeiFilter();
        imeiFilter(true);
    }
}

void DialogLogshow::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() = Qt::LeftButton) {
        int nx = event->pos().x();
        int ny = event->pos().y();

        int x = ui->wdt_title->pos().x();
        int y = ui->wdt_title->pos().y();

        int x1 = x + ui->wdt_title->width();
        int y1 = y + ui->wdt_title->height();

        if (nx > x && nx < x1 && ny > y && ny < y1) {
            isdraggwindow = true;
        }
    }
    lastmouse = event->pos();
}

void DialogLogshow::mouseMoveEvent(QMouseEvent *event) {
    if (isdraggwindow) {
        this->move(event->globalPos() - lastmouse);
    }
}

void DialogLogshow::mouseReleaseEvent(QMouseEvent *event) {
    isdraggwindow = false;
}

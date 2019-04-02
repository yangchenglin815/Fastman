#ifndef DIALOGLOGSHOW_H
#define DIALOGLOGSHOW_H

#include <QDialog>
#include <QDate>
#include <QSortFilterProxyModel>
#include <QStandardItem>

#include "appconfig.h"

namespace Ui {
class DialogLogshow;
}
namespace Ui {
class Widget;
}

class LogModel : public QStandardItemModel {
    Q_OBJECT
public:
    LogModel(QObject *parent = 0);
    enum {
        COL_TIME = 0,
        COL_LOG
    };

    void setLogData(const QList<InstLog *>& logs);
    static QString getTimeStr(uint);
};

class DialogLogshow : public QDialog
{
    Q_OBJECT
    QList<InstLog *>logs;
    QList<InstLog *>logscache;
    QList<InstLog *>modellogs;
    QList<InstLog *>modellogscache;
    QDate startDate;
    QDate endDate;
    LogModel *modelcache;
    LogModel *model;
    QSortFilterProxyModel *sort_model;
    QSortFilterProxyModel *sort_modelcache;
    QPoint lastmouse;
    bool isdraggwindow;
    AppConfig *config;
public:
    explicit DialogLogshow(QWidget *parent = 0);
    void filterCheckBox();
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void dateTimeFilter(QDate &);
    bool compareDate(QDate &, QDate &);
    ~DialogLogshow();
public slots:
    void slot_start();
    void slot_showDetail(const QModelIndex &index);
    void slot_label(QString );
    void slot_clear();
    void slot_filterData();
    void slot_export();
    void slot_imeiFilter();
private:
    void imeiFilter(bool isCache = false);
private:
    Ui::DialogLogshow *ui;
};

#endif // DIALOGLOGSHOW_H

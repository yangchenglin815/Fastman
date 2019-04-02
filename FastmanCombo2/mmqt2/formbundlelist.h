#ifndef FORMBUNDLELIST_H
#define FORMBUNDLELIST_H

#include <QWidget>
#include <QStandardItemModel>
#include "bundle.h"

class GlobalController;

class BundleListModel : public QStandardItemModel {
    Q_OBJECT
    GlobalController *controller;
    QMutex mutex;

signals:
    void signal_loadBundleView(const QString &id);

public:
    enum {
        COL_NAME,
        COL_DESC,
        COL_TYPE,
        COL_ENABLED,
        COL_SIZE,
        COL_PROGRESS,
        COL_EDIT
    };

    QStringList checkList;
    BundleListModel(QObject *parent);
public slots:
    void slot_setRowData(Bundle *bundle);
    void slot_clicked(const QModelIndex &index);
    void slot_selAll();
    void slot_selRev();
};

namespace Ui {
class FormBundleList;
}

class FormBundleList : public QWidget
{
    Q_OBJECT
    GlobalController *controller;
    BundleListModel *model;
public:
    explicit FormBundleList(QWidget *parent = 0);
    ~FormBundleList();

private slots:
    void slot_pageToggled();
    void slot_enable();
    void slot_disable();
    void slot_delete();

public slots:
    void slot_refreshBundleList();
    void slot_loadBundleView(const QString &id, bool bNewBundle = false);

signals:
    void signal_refreshBundleList();
    void signal_itemStatusChanged(BundleItem *item);
private:
    Ui::FormBundleList *ui;
};

#endif // FORMBUNDLELIST_H

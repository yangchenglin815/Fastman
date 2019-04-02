#ifndef DIALOGDELWHITELIST_H
#define DIALOGDELWHITELIST_H

#include "appconfig.h"
#include <QDialog>
#include <QStandardItemModel>
#include <QMutex>

namespace Ui {
class dialogdelwhitelist;
}

class WhitelistModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit WhitelistModel(QWidget *parent = 0);
    enum {
        COL_CHECK = 0,
        COL_FINGERPRINT = 0,
        COL_WHITELIST
    };
    void infoinit(QList<DoNotClean *>);
    QStringList checklist;
public slots:
    void slot_btn_checkall();
    void slot_btn_invert();
    void slot_clicked(QModelIndex);
private:
    QMutex mutex;
};

class dialogdelwhitelist : public QDialog
{
    Q_OBJECT

public:
    explicit dialogdelwhitelist(QWidget *parent = 0);
    ~dialogdelwhitelist();

public slots:
    void slot_btn_ok();

private:
    Ui::dialogdelwhitelist *ui;
    AppConfig *config;
    WhitelistModel *model;
};

#endif // DIALOGDELWHITELIST_H

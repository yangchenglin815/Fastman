#ifndef DIALOGLOGVIEW_H
#define DIALOGLOGVIEW_H

#include <QDialog>
#include <QStandardItemModel>

#include "appconfig.h"

namespace Ui {
class DialogLogView;
}

class Logdata;

class LogDataModel : public QStandardItemModel {
    Q_OBJECT
public:
    LogDataModel(QObject *parent = 0);
    enum {
        COL_KEY = 0,
        COL_VALUE
    };

    void setLogData(InstLog *d);
};

class DialogLogView : public QDialog
{
    Q_OBJECT
public:
    explicit DialogLogView(InstLog *d, QWidget *parent = 0);
    ~DialogLogView();
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
public slots:
    void doubleClicked(const QModelIndex& index);
private:
    Ui::DialogLogView *ui;
    QPoint lastmouse;
    bool isdraggwindow;
};

#endif // DIALOGLOGVIEW_H

#ifndef DialogSelToClean_H
#define DialogSelToClean_H

#ifndef NO_GUI
#include <QDialog>
#include <QStandardItemModel>
#include <QMutex>

namespace Ui {
class DialogSelToClean;
}

class ZAdbDeviceNode;
class AGENT_APP_INFO;
class GlobalController;

class AgentAppModel : public QStandardItemModel {
    Q_OBJECT
    QMutex mutex;
public:
    QStringList checkList;
    AgentAppModel(QObject * parent);

    enum {
        COL_NAME = 0,
        COL_TYPE,
        COL_VER,
        COL_SIZE,
        COL_TIME
    };

    void setAppListData(QList<AGENT_APP_INFO*> appList);
public slots:
    void slot_clicked(const QModelIndex& index);
    void slot_selAll();
    void slot_selRev();
};
#endif

class DlgSelCleanParam {
public:
    ZAdbDeviceNode *node;
    QList<AGENT_APP_INFO*> *list;
    QStringList result;
};

#ifndef NO_GUI
class DialogSelToClean : public QDialog
{
    Q_OBJECT
    GlobalController *controller;
    DlgSelCleanParam *param;
    AgentAppModel *model;
public:
    explicit DialogSelToClean(void *data, QWidget *parent = 0);
    ~DialogSelToClean();
private slots:
    void slot_ok();
private:
    Ui::DialogSelToClean *ui;
};
#endif

#endif // DialogSelToClean_H

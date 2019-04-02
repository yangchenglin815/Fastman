#ifndef DialogDeviceView_H
#define DialogDeviceView_H

#include <QObject>
#include <QDialog>
#include <QStandardItemModel>
#include <QMutex>
#include <QThread>
#include <qmessagebox.h>

namespace Ui {
class DialogDeviceView;
}

// View
class AdbDeviceNode;
class AGENT_BASIC_INFO;
class AGENT_APP_INFO;
class DeviceModel;
class QPoint;
class QImage;
class QProgressBar;
class DialogDeviceView : public QDialog
{
    Q_OBJECT

    AdbDeviceNode *m_node;
    AGENT_BASIC_INFO *m_basicInfo;
    QList<AGENT_APP_INFO *> *m_appList;
    DeviceModel *m_model;
    QMap<QString, AGENT_APP_INFO *> m_appMap;

    QMutex listMutex;
    int threadcount;
signals:
    void signal_checkedChanged(bool checked, quint64 size);

public:
    explicit DialogDeviceView(AdbDeviceNode *m_node, QWidget *parent = 0);
    ~DialogDeviceView();

    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void keyPressEvent(QKeyEvent *e);
    void closeEvent(QCloseEvent *);

    void getBasicinfo();
    void getScreenshot();
    void getApps();

    int getCheckedApps(QList<AGENT_APP_INFO *> &apps);

private slots:
    void slot_itemClicked(const QModelIndex &index);

    void slot_maxBtnClicked();
    void slot_refreshScreenshot();
    void slot_copyScreenshot();
    void slot_saveScreenshot();
    void slot_refreshApps();
    void slot_setCheckedAll();
    void slot_setInvertChecked();
    void slot_backup();
    void slot_uninstall();

    void slot_getScrrenshotFinished();
    void slot_onGetAppFinished(AGENT_APP_INFO *);
    void slot_getAppsFinisehd();

    void slot_refreshUI();
    void slot_updateTotalState();
    void slot_updataCheckedState();

    void slot_removeRow(DeviceModel *model, const QString &packageName);
    void slot_adbpmable();
    void slot_adbpmdisable();
    void slot_AbleChanged(QString, bool m);
    void slot_WarnRootFail();
    void slot_countReduce();
private:
    Ui::DialogDeviceView *ui;
    int mouseX;
    int mouseY;
    bool isDraggingWindow;
    bool isInRange(QWidget *widget, int x, int y);
    QPoint m_lastPoint;
    bool m_dragged;

    QImage m_image;    
};

// Model
class AdbDeviceNode;
class AGENT_APP_INFO;
class FastManClient;
class QModelIndex;
class DeviceModel : public QStandardItemModel
{
    Q_OBJECT

public:
    DeviceModel(QObject *parent);

    enum {
        TYPE_UNKNOWN_APP = 0,
        TYPE_SYSTEM_APP,
        TYPE_USER_APP,
        TYPE_UPDATED_SYSTEM_APP
    };

    enum {
        COL_NAME = 0,
        COL_PKG = 0,
        COL_CHECKABLE = 0,
        COL_ICON = 0,
        COL_VERSION,
        COL_ENABLED,
        COL_TYPE,
        COL_SIZE,
        COL_TIME,
        COL_END
    };

    void setAppsData(const QList<AGENT_APP_INFO *> &list);
    void setAppData(AGENT_APP_INFO *app);
    static int getType(int f);
    static QString getFlagName(int f);
    QMutex mutex;
};


// Thread: GetScreenshot
class AdbClient;
class QImage;
class GetScreenshotThread : public QThread {
    Q_OBJECT
public:
    explicit  GetScreenshotThread(QImage &image, AdbClient *adbClient);
    void run();

private:
    QImage &m_image;
    AdbClient *m_adbClient;
};


// Thread: GetAppList
class GetAppListThread : public QThread
{
    Q_OBJECT

signals:
    void signal_getAppFinished(AGENT_APP_INFO *app);
public:
    explicit GetAppListThread(QList<AGENT_APP_INFO *> *list,
                              AdbDeviceNode *node,
                              QObject *parent = 0);

    void run();

private:
    QList<AGENT_APP_INFO *> *m_list;
    AdbDeviceNode *m_node;
};


// Thread: Backup
class BackupThread : public QThread
{
    Q_OBJECT

public:
    BackupThread(AdbDeviceNode *node,
                 const QList<AGENT_APP_INFO *> &appList,
                 const QString &strDes,
                 QObject *parent = 0);

    void run();

public slots:
    void slot_stop();

private:
    AdbDeviceNode *m_node;
    QList<AGENT_APP_INFO *> m_list;
    QString m_strDes;
    bool m_bStop;

signals:
    void signal_progressChanged(int value, int max);
    void signal_stateChanged(const QString &state);
};

//Thread: uninstall
class PmAbleThread : public QThread{
    Q_OBJECT
public:
    PmAbleThread(AdbDeviceNode *node,
                 const QList<AGENT_APP_INFO *> &appList,
                 DeviceModel *model,
                 bool able,
                 QObject *parent = 0);
    void run();
public slots:
    void slot_stop();
private:
    bool needstop;
    AdbDeviceNode *m_node;
    QList<AGENT_APP_INFO *> m_list;
    DeviceModel *m_model;
    bool status;
signals:
    void signal_state(QString);
    void signal_Progress(int, int);
    void signal_WarnRootFail();
    void signal_AbleChanged(QString, bool );
};

// Thread: Uninstall
class UninstallThread : public QThread
{
    Q_OBJECT

public:
    UninstallThread(AdbDeviceNode *node,
                 const QList<AGENT_APP_INFO *> &appList,
                 DeviceModel *model,
                 QObject *parent = 0);

    void run();

public slots:
    void slot_stop();

private:
    AdbDeviceNode *m_node;
    QList<AGENT_APP_INFO *> m_list;
    DeviceModel *m_model;
    bool m_bStop;

signals:
    void sigDeviceRefresh(AdbDeviceNode *node);
    void signal_progressChanged(int value, int max);
    void signal_stateChanged(const QString &state);
    void signal_removeRow(DeviceModel *m_model, QString str);
};

#endif // DialogDeviceView_H

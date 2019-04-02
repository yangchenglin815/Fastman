#ifndef FORMMAIN_H
#define FORMMAIN_H

#include <QWidget>
#include <QMap>

#include <QCloseEvent>

namespace Ui {
class FormMain;
}

class GlobalController;
class ZDownloadManager;
class AdbTracker;
class QToolButton;
class DialogLogin;
class QLabel;
class ZLocalServer;
class FormMain : public QWidget
{
    Q_OBJECT
    GlobalController *controller;
    QMap<QToolButton*, QWidget*> nav_pages_map;

    bool isMaxWindow;
    bool isDraggingWindow;
    QPoint lastMousePos;

signals:
    void signal_closeLoginDlg();
    void singal_checkUpdateResult(bool);
    void signal_WiFiConfigChanged();
    void signal_LANConfigChanged();

public:
    explicit FormMain(QWidget *parent = 0);
    ~FormMain();

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void closeEvent(QCloseEvent *e);

    void switchNav(QToolButton *btn);
public slots:
    void slot_initManagers();
    void slot_initUI();
    void slot_loadInstallUI();

    void slot_winMaxRestore();
    void slot_feedback();
    void slot_setup();
    void slot_quitApp();
    void slot_dmIdle();

    void slot_autoCheckUpdates();
    void slot_checkTC();
    void slot_showBuildTime();
    void slot_checkUpdates();
    void slot_checkUpdatesResult(bool succeed);

    void slot_adbError(const QString& hint, int pid);
    void slot_critical(const QString& hint);

    void slot_switchBoard();
    void slot_switchInstallation();
    void slot_switchBundles();
    void slot_switchDownloads();
    void slot_switchMyAccount();
    void slot_switchTools();

    void slot_flipNavBoard(bool checked);
    void slot_flipNavInstallation(bool checked);
    void slot_flipNavBundles(bool checked);
    void slot_flipNavDownloads(bool checked);
    void slot_flipNavMyAccount(bool checked);
    void slot_flipNavTools(bool checked);

    void slot_updateCntStatus();
#ifdef USE_JUNBO
    void slot_mmpackage_added(const QString& packageName);
#endif

private:
    bool isAutoCheckUpdates;
    bool checkUpdatesFailed;
    int checkUpdates(void *p, void *q);

    int cleanupLogFiles(void *p, void *q);

public:
    static QString getMaintenceTool();
    static bool checkUpdate(QString &hint);


private:
    Ui::FormMain *ui;

    DialogLogin *m_loginDlg;
    QLabel *m_devIdx;
    QLabel *m_unfinishedIdx;

};

#endif // FORMMAIN_H

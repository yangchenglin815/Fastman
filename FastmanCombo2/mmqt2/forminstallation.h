#ifndef FORMINSTALLATION_H
#define FORMINSTALLATION_H

#include <QWidget>
#include <QMutex>
#include <QModelIndex>
#include <QThread>

namespace Ui {
class FormInstallation;
}

class AdbDeviceNode;
class RefreshAgentThread : public QThread {
    QList<AdbDeviceNode*> devices;
public:
    RefreshAgentThread(const QList<AdbDeviceNode*>& list);
    void run();
};

class GlobalController;
class Bundle;
class FormInstallation : public QWidget
{
    Q_OBJECT
    GlobalController *controller;
    QMutex bundleUIListMutex;
    QMutex devUIListMutex;
    QMutex cleanupMutex;
    bool initFinished;

public:
    explicit FormInstallation(QWidget *parent = 0);
    ~FormInstallation();
    QStringList getCheckedBundles();

private slots:
    void slot_bundleClicked(Bundle *b, bool checked);

    void slot_toggledAutoInstBundle(bool checked);
    void slot_toggledAsyncInst(bool checked);
    void slot_toggledInstByZAgent(bool checked);
    void slot_toggledTryRoot(bool checked);
    void slot_toggledUninstUsrApp(bool checked);
    void slot_toggledUninstSysApp(bool checked);
    void slot_toggledPauseInstall(bool checked);
    void slot_toggledUseLAN(bool checked);

    void slot_deviceClicked(const QModelIndex &index);
    void slot_bundleClicked(const QModelIndex &index);
#ifdef USE_JUNBO
    void slot_add_mm_bundle();
#endif
    void slot_bundleSelAll();
    void slot_bundleSelRev();
    void slot_install();
    void slot_cleanup();
    void slot_installFinished(bool result);

public slots:
    void slot_refreshBundleList();
    void slot_refreshDevList();

    void slot_hint(void *ret, int type, const QString& hint, const QString &tag);
    void slot_chooseToClean(void *ret, void *data);

    void slot_checkWiFiConfig();
    void slot_checkLANConfig();

signals:
    void sig_checkHint();
    void sig_checkChoice();
#ifdef USE_JUNBO
    void sig_mmpackage_added(const QString& packageName);
#endif

private:
    Ui::FormInstallation *ui;
};

#endif // FORMINSTALLATION_H

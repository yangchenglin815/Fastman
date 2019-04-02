#ifndef COREMAIN_H
#define COREMAIN_H

#include <QThread>
#include <QMutex>

class GlobalController;

class ZDownloadManager;
class ZDownloadTask;
class BundleItem;

class AdbTracker;
class AdbDeviceNode;

class ItemParseThread : public QThread {
    Q_OBJECT
    BundleItem *item;
public:
    ItemParseThread(BundleItem *item);
    void run();
signals:
    void signal_itemParseDone(BundleItem *item);
};

class CoreMain : public QObject
{
    Q_OBJECT
    GlobalController *controller;

    QMutex bundleUIListMutex;
    QMutex devUIListMutex;
public:
    CoreMain(QObject *parent = 0);
    ~CoreMain();

private slots:
    void slot_quitApp();
    void slot_dmIdle();

    void slot_critical(const QString& hint);

    void slot_refreshBundleList();
    void slot_refreshDevList();

    void slot_updateItemStatus(ZDownloadTask *task);
    void slot_itemParseDone(BundleItem *item);
    void slot_updateCount();

    void slot_refreshUI(AdbDeviceNode* node);
    void slot_setHint(AdbDeviceNode* node, const QString& hint);
    void slot_setProgress(AdbDeviceNode* node, int progress, int max);
    void slot_deviceLog(AdbDeviceNode* node, const QString& log);
};

#endif // COREMAIN_H

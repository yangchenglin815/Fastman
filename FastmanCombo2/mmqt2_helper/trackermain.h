#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QObject>

class AdbTracker;
class AdbDeviceNode;
class TrackerServer;
class TrackerClient;
class TrackerMain : public QObject
{
    Q_OBJECT

    AdbTracker *m_adbTracker;
    QList<AdbDeviceNode *> m_devices;

    void iniTracker();

public:
    TrackerMain(QObject *parent = 0);
    ~TrackerMain();

public slots:
    void slot_onDevListChanged();
    void slot_onAdbError(const QString &hint);
};
#endif // MAINWINDOW_H

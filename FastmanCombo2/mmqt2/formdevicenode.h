#ifndef FORMDEVICENODE_H
#define FORMDEVICENODE_H

#include <QWidget>
#include <QThread>

namespace Ui {
class FormDeviceNode;
}

class AdbDeviceNode;

class RebootThread : public QThread {
    QString adb_serial;
public:
    RebootThread(AdbDeviceNode *node, QObject *parent = 0);
    void run();
};

class FormDeviceNode : public QWidget
{
    Q_OBJECT

public:
    AdbDeviceNode *node;

    explicit FormDeviceNode(AdbDeviceNode *node, QWidget *parent = 0);
    ~FormDeviceNode();

private slots:
    void slot_cmd();
    void slot_reboot();
    void slot_detail();
    void slot_saveLog();
public slots:
    void slot_refreshUI(AdbDeviceNode *n);
    void slot_setHint(AdbDeviceNode *n, const QString& hint);
    void slot_setProgress(AdbDeviceNode *n, int progress, int max);
private:
    Ui::FormDeviceNode *ui;
};

#endif // FORMDEVICENODE_H

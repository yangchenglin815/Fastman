#ifndef FORMDEVICELOG_H
#define FORMDEVICELOG_H

#include <QWidget>

namespace Ui {
class FormDeviceLog;
}

class AdbDeviceNode;
class FormDeviceLog : public QWidget
{
    Q_OBJECT
public:
    enum LogType {
        LOG_NORMAL = 0,
        LOG_UNPID,
        LOG_STEP,
        LOG_WARNING,
        LOG_ERROR,
        LOG_FINISHED
    };

public:
    AdbDeviceNode *node;
    explicit FormDeviceLog(AdbDeviceNode *node, QWidget *parent = 0);
    ~FormDeviceLog();

public slots:
    void slotDeviceLog(AdbDeviceNode *n, const QString& log, int type = LOG_NORMAL, bool needShow = true);
    void slot_clearLog(AdbDeviceNode *n);

private slots:
    void slot_openLogFile();

private:
    Ui::FormDeviceLog *ui;

    QString logFilePath;
};

#endif // FORMDEVICELOG_H

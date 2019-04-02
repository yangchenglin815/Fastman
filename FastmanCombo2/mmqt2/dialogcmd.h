#ifndef DIALOGCMD_H
#define DIALOGCMD_H

#include <QDialog>

namespace Ui {
class DialogCmd;
}

class AdbDeviceNode;
class DialogCmd : public QDialog
{
    Q_OBJECT
    AdbDeviceNode *node;
public:
    explicit DialogCmd(AdbDeviceNode *node, QWidget *parent = 0);
    ~DialogCmd();
public slots:
    void slot_exec();
private:
    Ui::DialogCmd *ui;
};

#endif // DIALOGCMD_H

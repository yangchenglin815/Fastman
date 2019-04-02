#ifndef DIALOGCONFIGLAN_H
#define DIALOGCONFIGLAN_H

#include <QDialog>

namespace Ui {
class DialogConfigLAN;
}

class DialogConfigLAN : public QDialog
{
    Q_OBJECT

signals:
    void signal_LANConfigChanged();

public:
    explicit DialogConfigLAN(QWidget *parent = 0);
    ~DialogConfigLAN();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private slots:
    void slot_save();

private:
    Ui::DialogConfigLAN *ui;

    void init();

    bool isDraggingWindow;
    QPoint lastMousePos;
};

#endif // DIALOGCONFIGLAN_H

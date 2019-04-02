#ifndef DIALOGDESKTOPINI_H
#define DIALOGDESKTOPINI_H

#include <QDialog>

namespace Ui {
class Dialogdesktopini;
}

class Dialogdesktopini : public QDialog
{
    Q_OBJECT


signals:
    void signal_DesktopConfigChanged();

public:
    explicit Dialogdesktopini(QWidget *parent = 0);
    ~Dialogdesktopini();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private slots:
    void slot_save();
    void slot_notHintToggled(bool);

private:
    Ui::Dialogdesktopini *ui;

    void init();

    bool isDraggingWindow;
    QPoint lastMousePos;
};

#endif // DIALOGDESKTOPINI_H

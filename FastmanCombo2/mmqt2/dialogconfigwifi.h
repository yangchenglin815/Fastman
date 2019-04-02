#ifndef DIALOGCONFIGWIFI_H
#define DIALOGCONFIGWIFI_H

#include <QDialog>

namespace Ui {
class DialogConfigWiFi;
}

class DialogConfigWiFi : public QDialog
{
    Q_OBJECT

signals:
    void signal_WiFiConfigChanged();

public:
    explicit DialogConfigWiFi(QWidget *parent = 0);
    ~DialogConfigWiFi();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private slots:
    void slot_save();
    void slot_notHintToggled(bool);

private:
    Ui::DialogConfigWiFi *ui;

    void init();

    bool isDraggingWindow;
    QPoint lastMousePos;
};

#endif // DIALOGCONFIGWIFI_H

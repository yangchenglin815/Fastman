#ifndef DIALOGSETUP_H
#define DIALOGSETUP_H

#include <QDialog>

namespace Ui {
class DialogSetup;
}

class AppConfig;
class DialogSetup : public QDialog
{
    Q_OBJECT
    AppConfig *config;
    bool isDraggingWindow;
    QPoint lastMousePos;

    bool getAutoStart(const QString& key, const QString& file);
    void setAutoStart(const QString& key, const QString& file, bool start);

signals:
    void signal_WiFiConfigChanged();
    void signal_LANConfigChanged();

public:
    explicit DialogSetup(QWidget *parent = 0);
    ~DialogSetup();

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private slots:
    void slot_ok();
    void slot_tidyApk();
    void slot_delete_whitelist();
    void slot_delete_hintini();
    void slot_logview();
    void slot_delete_Cachelog();
    void slot_config_WiFi();
    void slot_config_LAN();
    void slot_desktopini();


private:
    Ui::DialogSetup *ui;
};

#endif // DIALOGSETUP_H

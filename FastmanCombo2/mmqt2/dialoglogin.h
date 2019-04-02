#ifndef DIALOGLOGIN_H
#define DIALOGLOGIN_H

#include <QThread>
#ifndef NO_GUI
#include <QDialog>
#include "dialogfeedback.h"

namespace Ui {
class DialogLogin;
}
#endif

class LoginThread : public QThread {
    Q_OBJECT
    QString username;
    QString password;
    bool remember;
    bool offline;
    bool CMCCFirst;
#ifdef NO_GUI
    bool _ret;
    QString _hint;
#endif
public:
    LoginThread(const QString& username, const QString& password,
                bool remember, bool offline, bool CMCCFirst);
    void run();
#ifdef NO_GUI
    static bool core_login(const QString& username, const QString& password,
                           bool remember, bool offline, QString &hint);
private slots:
    void slot_loginProgress(int value, int max, const QString& hint);
    void slot_loginDone(bool ret, const QString& hint);
#endif

signals:
    void signal_showHint(const QString &hint);
    void signal_showHintFinisehd();
    void signal_hasUpdate();
    void signal_loginProgress(int value, int max, const QString& hint);
    void signal_loginDone(bool ret, const QString& hint);
};

class AppConfig;
#ifndef NO_GUI
class DialogLogin : public QDialog
{
    Q_OBJECT
    AppConfig *config;
    QMovie *mov;
    bool loading;

    bool isDraggingWindow;
    QPoint lastMousePos;

signals:
    void signal_showHintFinished();
    void signal_loginSucceed();

public:
    explicit DialogLogin(QWidget *parent = 0);
    ~DialogLogin();

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void keyPressEvent(QKeyEvent *e);
    void closeEvent(QCloseEvent *e);
private slots:
    void slot_showHint(const QString &hint);
    void slot_hasUpdate();
    void slot_loginProgress(int value, int max, const QString& hint);
    void slot_loginDone(bool ret, const QString& hint);
    void slot_login();
    void slot_feedback();
    void slot_settings();
    void toggleOffline(bool checked);
private:
    Ui::DialogLogin *ui;
};
#endif
#endif // DIALOGLOGIN_H

#ifndef DIALOGJUNBOLOGIN_H
#define DIALOGJUNBOLOGIN_H

#include <QDialog>
#include <QThread>

namespace Ui {
class DialogJunboLogin;
}

class JunboLoginThread : public QThread {
    Q_OBJECT
    QString user;
    QString pass;
    bool remember;
public:
     JunboLoginThread(const QString& user, const QString& password, bool remember, QObject *parent);
     void run();
signals:
     void signalDone(bool ret);
};

class DialogJunboLogin : public QDialog
{
    Q_OBJECT
public:
    explicit DialogJunboLogin(QWidget *parent = 0);
    ~DialogJunboLogin();
private slots:
    void slot_login();
    void slot_loginDone(bool ret);
private:
    Ui::DialogJunboLogin *ui;
};

#endif // DIALOGJUNBOLOGIN_H

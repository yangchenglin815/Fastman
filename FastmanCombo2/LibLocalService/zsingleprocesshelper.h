#ifndef ZSINGLEPROCESSHELPER_H
#define ZSINGLEPROCESSHELPER_H

#include <QObject>

class ZLocalServer;
class ZLocalClient;
class QWidget;
class ZSingleProcessHelper : public QObject
{
    Q_OBJECT
signals:
    void signal_arguments(const QStringList &args);

public:
    explicit ZSingleProcessHelper(const QString &serverName, const QString &holder = "", QObject *parent = 0);

    virtual void handleQuitRequest();

    bool takeLock();
#ifndef NO_GUI
    void setActivationWindow(QWidget *aw);
#endif
    bool hasRemote();
    bool hasRemote(QString &holder);
    static bool hasRemote(const QString &name, QString &holder);
    bool activateRemote();
    static bool activateRemote(const QString &name);
    bool killRemote();
    static bool killRemote(const QString &name);

    void sendArguments(const QString &name);

public slots:
    void slot_newConnection();

private:
    ZLocalServer *m_server;
    QString m_serverName;
    QString m_holder;
    QWidget *m_actWin;

    void activateWinodw();
    void forceQuit();

    QString m_processId;
};

#endif // ZSINGLEPROCESSHELPER_H

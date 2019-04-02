#ifndef ZDOWNLOADMANAGER_H
#define ZDOWNLOADMANAGER_H

#include <QObject>
#include <QString>
#include <QUrl>

class ZDownloadTask : public QObject {
    Q_OBJECT
    friend class ZDMTask;
    ZDownloadTask(QObject *parent);
public:
    enum {
        STAT_PENDING = 0,
        STAT_DOWNLOADING,
        STAT_STOPPED, // stopped by user, doesn't need auto resume
        STAT_FAILED,
        STAT_FINISHED
    };

    enum {
        ERR_NOERROR = 0,
        ERR_CONNECT,
        ERR_CREATE_FILE,
        ERR_WRITE_FILE,
        ERR_INCOMPLETE
    };

    QString id;
    QUrl url;
    QString path;
    int status;
    int result;
    uint mtime;

    qint64 progress;
    qint64 size;
};

class ZDM;
class ZDownloadManager : public QObject {
    Q_OBJECT
    friend class ZDM;
    ZDownloadManager(QObject *parent);
public:
    // might be false
    static bool global_init();
    static ZDownloadManager *newDownloadManager(const QString &prefix, QObject *parent = 0);
    static void delDownloadManager(ZDownloadManager *obj);
    virtual void setMaxTasks(int max) = 0;
    virtual void setMaxParts(int max) = 0;
    virtual void setMaxRetry(int max) = 0;

    virtual void startWork() = 0;
    virtual void stopWork() = 0;
    virtual int getUnfinishedCount() = 0;
    virtual int getRunningCount() = 0;

    virtual ZDownloadTask *addTask(const QString& url, const QString &path) = 0;
    virtual ZDownloadTask *addTask(const QString& id, const QString& url, const QString &path) = 0;

    virtual ZDownloadTask *getTask(const QString& id) = 0;
    virtual QList<ZDownloadTask *> getTasks() = 0;
    virtual QList<ZDownloadTask *> getTasks(const QStringList& ids) = 0;

    virtual void startTask(ZDownloadTask *t) = 0;
    virtual void stopTask(ZDownloadTask *t) = 0;

    // note: task will be deleted after remove
    virtual void removeTask(ZDownloadTask *t) = 0;
    virtual void removeTask(const QString& id) = 0;
    virtual void removeTasks(const QStringList& ids) = 0;

signals:
    void signal_speed(ZDownloadTask *t, int bytesPerSecond);
    void signal_progress(ZDownloadTask *t);
    void signal_status(ZDownloadTask *t);
    void signal_idle();
};

#endif // ZDOWNLOADMANAGER_H

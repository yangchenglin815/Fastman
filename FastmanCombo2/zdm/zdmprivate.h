#ifndef ZDMPRIVATE_H
#define ZDMPRIVATE_H

#include "zdownloadmanager.h"
#include <QList>
#include <QFile>
#include <QMutex>
#include <QTime>
#include <QTimer>
#include <QThread>

#define DEF_MAX_TASKS 3
#define DEF_MAX_PARTS 3
#define DEF_MAX_RETRY 10

#define MIN_PART_BLKSIZE 500*1024 // 500 KB
#define MAX_BUFF_BLKSIZE 512*1024

class ZDM;
class ZDMThreadPool;

class ZDMPart {
public:
    qint64 begin;
    qint64 end;
    qint64 progress;
    QByteArray buffer;
};

class ZDMTask : public ZDownloadTask {
    Q_OBJECT
public:
    ZDM *dm;
    bool needsStop;
    int nextStatus;
    qint64 lastProgress;

    ZDMThreadPool *threadPool; // ZDMPartThreads
    QList<ZDMPart *> parts;

    QFile *file;
    QMutex file_mutex;

    QTimer speed_timer;
    QTime progress_timer;
    QMutex progress_mutex;

    bool initParts();
    bool initFile();
    qint64 writeFile(qint64 pos, const QByteArray &buffer);
    void addProgress(qint64 n);

    ZDMTask(ZDM *dm);
    ~ZDMTask();
    void run();
public slots:
    void slot_startThreads();
    void slot_calcSpeed();
    void slot_stopSpeedTimer(qint64 gap, int elapsed);
signals:
    void signal_startThreads();
    void signal_notStartedThreads();
    void signal_stopSpeedTimer(qint64 gap, int elapsed);
    void signal_speed(ZDownloadTask *task, int bytesPerSecond);
    void signal_progress(ZDownloadTask *task);
    void signal_status(ZDownloadTask *task);
    void signal_saveConfig();
};

class ZDMTaskThread : public QThread {
    ZDM *dm;
    ZDMTask *task;
public:
    ZDMTaskThread(ZDMTask *task, ZDM *dm);
    ~ZDMTaskThread();
    void run();
};

class ZDMPartThread : public QThread {
    ZDM *dm;
    ZDMTask *task;
    ZDMPart *part;
public:
    ZDMPartThread(QObject *parent, ZDM *dm, ZDMTask *task, ZDMPart *part);
    ~ZDMPartThread();
    void run();
};

class ZDM : public ZDownloadManager {
    Q_OBJECT

    QString dbPrefix;
    QTime dbTimer;
    QMutex dbMutex;
public:
    bool needsStop;
    ZDMThreadPool *threadPool; // ZDMTaskThreads

    QList<ZDMTask *> tasks;
    QMutex tasks_mutex;

    int maxTasks;
    int maxParts;
    int maxRetry;

    ZDM(const QString &prefix, QObject *parent);
    ~ZDM();

    void setMaxTasks(int max);
    void setMaxParts(int max);
    void setMaxRetry(int max);

    void startWork();
    void stopWork();
    int getUnfinishedCount();
    int getRunningCount();

    ZDownloadTask *addTask(const QString& url, const QString &path);
    ZDownloadTask *addTask(const QString& id, const QString& url, const QString &path);

    ZDownloadTask *getTask(const QString& id);
    QList<ZDownloadTask *> getTasks();
    QList<ZDownloadTask *> getTasks(const QStringList& ids);

    void startTask(ZDownloadTask *t);
    void stopTask(ZDownloadTask *t);

    // note: task won't be removed if it's downloading
    // note: task will be deleted after remove
    void removeTask(ZDownloadTask *t);
    void removeTask(const QString& id);
    void removeTasks(const QStringList& ids);

    void loadConfig();
    void saveConfig();
private slots:
    void slot_newTask(ZDMTask *t);
    void slot_saveConfig();
    void slot_checkAndStart();
signals:
    void signal_newTask(ZDMTask *t);
    void signal_checkAndStart();
};

class ZDMThreadPool : public QObject {
    Q_OBJECT
    QList<QThread *> list;
    QMutex mutex;
public:
    ZDMThreadPool(QObject *parent);
    ~ZDMThreadPool();

    void addThread(QThread *t);
    void removeThread(QThread *t);
    int threadCount();
signals:
    void signalEmpty();
};

#endif // ZDMPRIVATE_H

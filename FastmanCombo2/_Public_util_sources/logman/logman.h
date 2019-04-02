#ifndef _LOG_MAN_H_
#define _LOG_MAN_H_

#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>

// disable log-uploading flag
//#define NLOG_NOLOG

#ifdef NLOG_LOG
#define LOGMAN (LogMan::instance())
#define NLOG(t,a,s,...)           LOGMAN->Log2(__FILE__,__FUNCTION__,__LINE__,t,a,s,__VA_ARGS__)
#else
#define LOGMAN
#define NLOG(t,a,s,...)
#endif

// log type
enum {
    LOG_T_RUNTIME = 0,
    LOG_T_INSTALL,
    LOG_T_ROMDOWN,
    LOG_T_ROMPARSE,
    LOG_T_BRUSH,
    LOG_T_PHONE,
    LOG_T_UI
};

// log action
enum {
    LOG_A_RUNTIME = 0,
    LOG_A_START,
    LOG_A_FINISHED,
    LOG_A_SUCCESS,
    LOG_A_FAILED,
    LOG_A_PROGRESS,
    LOG_A_TIMEOUT,
    LOG_A_CONNECT,
    LOG_A_DISCONNECT,
    LOG_A_STOPPED
};

// basic log info
typedef struct {
    QString guid;
    QString client_version;
    QString rom_file;
    QString rom_id;
    QString platform;
    QString brand;
    QString model;
    QString salescode;
    QString vid;
    QString pid;
    QString driver_id;

    // windows is x64
    bool isWow64;

    // windows version
    int os_platform;
    int os_major;
    int os_minor;
    int os_build;

    QString channel;

    QString uid;
} BasicLogInfo;

// log item
typedef struct {
    int type;
    int action;
    QString logtext;
    QString logtext2;
    QString logtime;
} LogItem;

typedef QList<LogItem> LogList;
typedef BasicLogInfo(*GetBasicInfoFunc)();

#ifdef NLOG_LOG
class LogMan : public QThread
{
    Q_OBJECT
public:
    static LogMan *instance();

    void Log(const char *fn, const char *func, int line,
             int type, int action, const char *str);
    void Log2(const char *fn, const char *func, int line,
             int type, int action, const char *fmt, ...);

    void setBasicInfoGetFunc(GetBasicInfoFunc func);
    BasicLogInfo getBasicInfo();

signals:
    void signal_getBasicInfo();

private slots:
    void slot_getBasicInfo();

private:
    LogMan();

    void run();
    QByteArray buildJsonLog(const LogItem &item);

    LogList _logList;
    BasicLogInfo _basicInfo;
    GetBasicInfoFunc _getBasicInfoFunc;
    bool _runtime_log;
    QString _uuid;
    QMutex __log_lock;
};
#endif

#endif //_LOG_MAN_H_

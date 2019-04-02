#ifndef ZLOGGER_H
#define ZLOGGER_H

#ifdef ZLOG_ENABLE
#define ZLOG_TAG "default"
#define ZLOG_FILE "default.dxlog"

#include <QMutex>
#include <QFile>

class ZLoggerEntry {
public:
    ZLoggerEntry();

    qint64 time;
    int level;
    QString tag;
    QString log;

    bool fromByteArray(QByteArray &data);
    QByteArray toByteArray();
};

class ZLogger
{
    QMutex _mutex;
    QFile _file;

public:
    enum {
        LOG_VERBOSE = 0,
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR
    };

    ZLogger(const QString& file);
    ~ZLogger();
    void tempClose();

    static ZLogger* getInstance(const QString& file);
    static void destroy();

    void log(const char *tag, int level, const char *fmt, va_list ap);
    static ZLogger* log(const char *file, const char *tag, int level, const char *fmt, ...);
};

#if(defined(_MSC_VER))
#define ZLOGV(fmt, ...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_VERBOSE, "%s:%d "fmt, __FILE__, __LINE__, __VA_ARGS__)
#define ZLOGI(fmt, ...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_INFO, "%s:%d "fmt, __FILE__, __LINE__, __VA_ARGS__)
#define ZLOGW(fmt, ...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_WARN, "%s:%d "fmt, __FILE__, __LINE__, __VA_ARGS__)
#define ZLOGE(fmt, ...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_ERROR, "%s:%d "fmt, __FILE__, __LINE__, __VA_ARGS__)
#else
#define ZLOGV(fmt, args...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_VERBOSE, "%s:%d "fmt, __FILE__, __LINE__, ##args)
#define ZLOGI(fmt, args...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_INFO, "%s:%d "fmt, __FILE__, __LINE__, ##args)
#define ZLOGW(fmt, args...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_WARN, "%s:%d "fmt, __FILE__, __LINE__, ##args)
#define ZLOGE(fmt, args...) ZLogger::log(ZLOG_FILE, ZLOG_TAG, ZLogger::LOG_ERROR, "%s:%d "fmt, __FILE__, __LINE__, ##args)
#endif

#else

#define ZLOGV(...)
#define ZLOGI(...)
#define ZLOGW(...)
#define ZLOGE(...)

#endif
#endif // ZLOGGER_H

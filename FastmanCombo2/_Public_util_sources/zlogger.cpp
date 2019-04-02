#ifdef ZLOG_ENABLE

#define MAX_LOG_LEN 1024

#include "zlogger.h"
#include "zbytearray.h"

#include <QString>
#include <QMap>
#include <QDateTime>

ZLoggerEntry::ZLoggerEntry() {
    time = 0;
    level = ZLogger::LOG_VERBOSE;
}

bool ZLoggerEntry::fromByteArray(QByteArray &data) {
    ZByteArray d(false);
    d.append(QByteArray::fromBase64(data));
    int i = 0;
    if(d.size() < 12) {
        return false;
    }

    time = d.getNextInt64(i);
    level = d.getNextInt(i);
    tag = d.getNextUtf8(i);
    log = d.getNextUtf8(i);

    if(level < ZLogger::LOG_VERBOSE || level > ZLogger::LOG_ERROR) {
        return false;
    }
    return true;
}

QByteArray ZLoggerEntry::toByteArray() {
    ZByteArray d(false);
    d.putInt64(time);
    d.putInt(level);
    d.putUtf8(tag);
    d.putUtf8(log);
    return d.toBase64();
}

QMap<QString, ZLogger*> _instances;
QMutex _instances_mutex;

ZLogger::ZLogger(const QString &file)
    : _file(file) {

}

ZLogger::~ZLogger() {
    if(_file.isOpen()) {
        _file.close();
    }
}

void ZLogger::tempClose() {
    _mutex.lock();
    if(_file.isOpen()) {
        _file.close();
    }
    _mutex.unlock();
}

ZLogger* ZLogger::getInstance(const QString& file) {
    ZLogger *ret = NULL;
    _instances_mutex.lock();
    ret = _instances.value(file, NULL);
    if(ret == NULL) {
        ret = new ZLogger(file);
    }
    _instances_mutex.unlock();
    return ret;
}

void ZLogger::destroy() {
    QList<ZLogger*> list = _instances.values();
    while(!list.isEmpty()) {
        delete list.takeFirst();
    }
}

void ZLogger::log(const char *tag, int level, const char *fmt, va_list ap) {
    char clog[MAX_LOG_LEN];
    vsnprintf(clog, sizeof(clog)-1, fmt, ap);

    ZLoggerEntry entry;
    entry.time = QDateTime::currentDateTime().toMSecsSinceEpoch();
    entry.level = level;
#if(defined(_MSC_VER))
    entry.tag = QString::fromLocal8Bit(tag);
    entry.log = QString::fromLocal8Bit(clog);
#else
    entry.tag = QString::fromUtf8(tag);
    entry.log = QString::fromUtf8(clog);
#endif

#ifdef ZLOG_PRINTF
    printf("%s\n", qPrintable(entry.log));
#endif

    _mutex.lock();
    if(!_file.isOpen() && !_file.open(QIODevice::WriteOnly|QIODevice::Append)) {
        fprintf(stderr, "ERROR OPEN LOG FILE '%s'\n", qPrintable(_file.fileName()));
        _mutex.unlock();
        return;
    }

    _file.write(entry.toByteArray());
    _file.write("\r\n");
    _file.flush();
    _mutex.unlock();
}

ZLogger* ZLogger::log(const char *file, const char *tag, int level, const char *fmt, ...) {
    ZLogger* logger = getInstance(file);

    va_list ap;
    va_start(ap, fmt);
    logger->log(tag, level, fmt, ap);
    va_end(ap);

    return logger;
}

#endif

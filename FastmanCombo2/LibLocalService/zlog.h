#ifndef ZLOG_H
#define ZLOG_H

#define ZM_BASE_DIR "/data/local/tmp/work"

#ifdef CMD_DEBUG
#include <stdio.h>
#include <string.h>
static char *bname(char *str) {
#if defined(_WIN32)
    char *p = strrchr(str, '\\');
#else
    char *p = strrchr(str, '/');
#endif
    return p == NULL ? str : p+1;
}

#ifndef NO_QTLOG
#include <QDateTime>
#define LOGSTR_NOW QDateTime::currentDateTime().toString("[hh:mm:ss:zzz] ").toLocal8Bit().data()

#if defined(_WIN32) && (!defined(__MINGW32__))
#define DBG(fmt, ...) printf("%s%s %s %d: "fmt, LOGSTR_NOW, bname(__FILE__), __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define DBG(fmt, args...) printf("%s%s %s %d: "fmt, LOGSTR_NOW, bname(__FILE__), __FUNCTION__, __LINE__, ##args)
#endif
#else
#if defined(_WIN32) && (!defined(__MINGW32__))
#include <windows.h>
#include <time.h>
static void fmt_time(char *str, int max) {
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    _snprintf(str, max, "%04d-%02d-%02d %02d:%02d:%02d[%03d] ",
              wtm.wYear, wtm.wMonth, wtm.wDay,
              wtm.wHour, wtm.wMinute, wtm.wSecond, wtm.wMilliseconds
              );
}
#else
#include <sys/time.h>
#include <time.h>
static void fmt_time(char *str, int max) {
    time_t raw;
    struct timeval tv;
    struct tm *now;
    time(&raw);
    now = localtime(&raw);
    gettimeofday(&tv, NULL);
    strftime(str, max, "%Y-%m-%d %H:%M:%S", now);
    sprintf(str+strlen(str), "[%03d] ", tv.tv_usec/1000);
}
#endif

#ifdef __ANDROID__
#define DBG(fmt, args...) \
    do {\
    char now[128];\
    fmt_time(now, sizeof(now));\
    FILE *fp = fopen(ZM_BASE_DIR"/zm.log", "a");\
    if(fp!=NULL) {\
    fprintf(fp, "%s %s %d [%d]: "fmt, now, bname(__FILE__), __LINE__, getpid(), ##args);\
    fclose(fp);\
    }\
    } while(0)
#else
#if defined(_WIN32) && (!defined(__MINGW32__))
#define DBG(fmt, ...)\
    do {\
    char now[128];\
    fmt_time(now, sizeof(now));\
    printf("%s %s %s %d: "fmt, now, bname(__FILE__), __FUNCTION__, __LINE__, __VA_ARGS__);\
    }while(0)
#else
#define DBG(fmt, args...)\
    do {\
    char now[128];\
    fmt_time(now, sizeof(now));\
    printf("%s %s %s %d: "fmt, now, bname(__FILE__), __FUNCTION__, __LINE__, ##args);\
    }while(0)
#endif
#endif
#endif

#define DBG_HEX(x, y) __zprinthex(x, y)
void __zprinthex(void *data, int len);
#else
#define DBG(...)
#define DBG_HEX(x, y)
#endif

#ifdef _WIN32
#include <Windows.h>
#define xsleep(x) Sleep(x)
#define strdup _strdup
#else
#include <unistd.h>
#define xsleep(x) usleep((x)*1000)
#endif

#endif // ZLOG_H

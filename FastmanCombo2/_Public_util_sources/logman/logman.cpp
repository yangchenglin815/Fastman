#include "logman.h"
#include <QDateTime>
#include <QMutex>
#include <QUrl>
#include <QTextCodec>
#include <QApplication>
#include <QSettings>
#include <QUuid>
#include <QCryptographicHash>
#include "zdmhttp.h"
#include "zsettings.h"


static char *bname(char *str) {
#if defined(_WIN32)
    char *p = strrchr(str, '\\');
#else
    char *p = strrchr(str, '/');
#endif
    return p == NULL ? str : p+1;
}

#ifdef NLOG_LOG
LogMan *__log_man = NULL;


int GetCPUid(char *an)
{
    unsigned long s1 = 0, s2=0;
    __asm
    {
        mov eax,01h
        xor edx,edx
        cpuid
        mov s1,edx
        mov s2,eax
    }

    memcpy(an,(char *)&s1, 4);
    memcpy(an+4,(char *)&s2, 4);
    return 8;
}

LogMan::LogMan()
{
    char szbuf[8];
    GetCPUid(szbuf);

    QCryptographicHash hash(QCryptographicHash::Md5);
    QByteArray databa(szbuf, sizeof(szbuf));
    hash.addData(databa);
    QByteArray d = hash.result();

    char *p = d.data();
    if (d.size() < 16) {
        p = "1234567890123456";
    }
    uint l;
    memcpy(&l, p, sizeof(uint));
    p+=sizeof(l);
    ushort w1, w2;
    memcpy(&w1, p, sizeof(w1));
    p+=sizeof(w1);
    memcpy(&w2, p, sizeof(w2));
    p+=sizeof(w2);
    QUuid uuid(l, w1, w2, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    _uuid = uuid.toString();

    _getBasicInfoFunc = NULL;
    _runtime_log = false;

    QSettings settings(qApp->applicationDirPath() + "/diyflash.ini", QSettings::IniFormat);
    _runtime_log = settings.value("runtimelog", QVariant(false)).toBool();

    connect(this, SIGNAL(signal_getBasicInfo()), this, SLOT(slot_getBasicInfo()));
}

LogMan *LogMan::instance()
{
    if (__log_man == NULL) {
        __log_man = new LogMan();
//        __log_man->Log2(__FILE__,__FUNCTION__,__LINE__,
//                        LOG_T_RUNTIME, LOG_A_RUNTIME, "日志模块启动");
        __log_man->start();
    }

    return __log_man;
}

void LogMan::setBasicInfoGetFunc(GetBasicInfoFunc func)
{
    _getBasicInfoFunc = func;
}

BasicLogInfo LogMan::getBasicInfo()
{
    return _basicInfo;
}

void LogMan::slot_getBasicInfo()
{
    if (_getBasicInfoFunc) {
        _basicInfo = _getBasicInfoFunc();
    }
}

void LogMan::Log(const char *fn, const char *func, int line,
                 int type, int action, const char *str)
{
    emit signal_getBasicInfo();

    QDateTime dt = QDateTime::currentDateTime();

//    FILE *fp = fopen("log/NLOG.log", "a");
//    if (fp) {
//        fprintf(fp, "%s%s %s %d: %s",
//                dt.toString("[hh:mm:ss:zzz] ").toLocal8Bit().data(),
//                bname((char*)fn), func, line, str);
//        fclose(fp);
//    }

    char szbuf[1024];
    memset(szbuf, 0, sizeof(szbuf));
    _snprintf(szbuf, sizeof(szbuf) - 2, "%s", str);

    char szbuf2[1024];
    memset(szbuf2, 0, sizeof(szbuf));
    _snprintf(szbuf2, sizeof(szbuf) - 2, "%s %s %d",
              bname((char*)fn), func, line);

    // erase \r \n
    char *p = strchr(szbuf, '\r');
    if (p) {
        *p = '\0';
    }
    p = strchr(szbuf, '\n');
    if (p) {
        *p = '\0';
    }

    // add to log item list
    LogItem item;
    item.type = type;
    item.action = action;

    QTextCodec *utf8 = QTextCodec::codecForName("utf-8");
    QString s = QString::fromLocal8Bit(szbuf);
    QByteArray data = utf8->fromUnicode(s).toPercentEncoding();
    QString enc = QString::fromUtf8(data);
    //QByteArray data;
    //data.append(s);
    //data = data.toPercentEncoding();

    item.logtext  = enc;
    item.logtext2 = szbuf2;
    item.logtime  = dt.toString("[hh:mm:ss:zzz]");

    __log_lock.lock();
    _logList.push_back(item);
    __log_lock.unlock();
}

void LogMan::Log2(const char *fn, const char *func, int line,
                 int type, int action, const char *fmt, ...)
{
    // disable runtime log
    if (_runtime_log == false && type == LOG_T_RUNTIME) {
        return;
    }

    char szbuf[1024];

    memset(szbuf, 0, sizeof(szbuf));

    va_list va;
    va_start(va, fmt);
    vsnprintf(szbuf, sizeof(szbuf) - 2, fmt, va);
    va_end(va);

    int n = strlen(szbuf);
    if (n && szbuf[n - 1] != '\n') {
        strcat(szbuf, "\n");
    }

    Log(fn, func, line, type, action, szbuf);
}

QByteArray LogMan::buildJsonLog(const LogItem &item)
{    
    QString strVal = "";

    switch (item.type) {
    case LOG_T_RUNTIME:
        strVal = "runtime";
        break;
    case LOG_T_INSTALL:
        strVal = "install";
        break;
    case LOG_T_ROMDOWN:
        strVal = "romdown";
        break;
    case LOG_T_ROMPARSE:
        strVal = "romparse";
        break;
    case LOG_T_BRUSH:
        strVal = "brush";
        break;
    case LOG_T_PHONE:
        strVal = "adb";
        break;
    case LOG_T_UI:
        strVal = "ui";
        break;
    }

    QString strAct = "";
    switch (item.action) {
    case LOG_A_RUNTIME:
        strAct = "runtime";
        break;
    case LOG_A_START:
        strAct = "start";
        break;
    case LOG_A_FINISHED:
        strAct = "finished";
        break;
    case LOG_A_SUCCESS:
        strAct = "success";
        break;
    case LOG_A_FAILED:
        strAct = "failed";
        break;
    case LOG_A_PROGRESS:
        strAct = "progress";
        break;
    case LOG_A_TIMEOUT:
        strAct = "timeout";
        break;
    case LOG_A_CONNECT:
        strAct = "connect";
        break;
    case LOG_A_DISCONNECT:
        strAct = "disconnect";
        break;
    case LOG_A_STOPPED:
        strAct = "stopped";
        break;
    }

    // basic info
    QString json = QString("{");

    // type
    json += "\"type\": \"" + strVal + "\",";
    // action
    json += "\"action\": \"" + strAct + "\",";

    // log text
    json += "\"logText\": \"" + item.logtext + "\",";
    json += "\"logText2\": \"" + item.logtext2 + "\",";
    json += "\"logTime\": \"" + item.logtime + "\",";

    // client guid
    json += "\"clientGuid\": \"" + _basicInfo.guid + "\",";

    // client version
    json += "\"clientVersion\": \"" + _basicInfo.client_version + "\",";

    // channel code
    json += "\"channelCode\": \"" + _basicInfo.channel + "\",";

    // system info
    if (_basicInfo.isWow64) {
        json += "\"osIsWin64\": \"true\",";
    }
    else {
        json += "\"osIsWin64\": \"false\",";
    }
    json += "\"osPlatform\": \"" + QString::number(_basicInfo.os_platform) + "\",";
    json += "\"osMajorVer\": \"" + QString::number(_basicInfo.os_major) + "\",";
    json += "\"osMinorver\": \"" + QString::number(_basicInfo.os_minor) + "\",";
    json += "\"osBuildNum\": \"" + QString::number(_basicInfo.os_build) + "\",";

    // rom info
    if (   item.type == LOG_T_ROMPARSE
        || item.type == LOG_T_BRUSH) {
        QTextCodec *utf8 = QTextCodec::codecForName("utf-8");
        //QString s = QString::fromLocal8Bit(szbuf);
        QByteArray data = utf8->fromUnicode(_basicInfo.rom_file).toPercentEncoding();
        QString enc = QString::fromUtf8(data);

        json += "\"romName\": \"" + enc + "\",";
        if (_basicInfo.rom_id.isEmpty()) {
            json += "\"romId\": null,";
        }
        else {
            json += "\"romId\": " + _basicInfo.rom_id + ",";
        }
        json += "\"platform\": \"" + _basicInfo.platform + "\",";
        json += "\"brand\": \"" + _basicInfo.brand + "\",";
        json += "\"model\": \"" + _basicInfo.model + "\",";
        json += "\"salesCode\": \"" + _basicInfo.salescode + "\",";
    }

    // driver info
    if (   item.type == LOG_T_ROMDOWN
        || item.type == LOG_T_ROMPARSE
        || item.type == LOG_T_BRUSH
        || item.type == LOG_T_PHONE) {
        json += "\"vid\": \"" + _basicInfo.vid + "\",";
        json += "\"pid\": \"" + _basicInfo.pid + "\",";
        //json += "\"driverId\": " + _basicInfo.driver_id + ",";
    }

    // userid
    if (_basicInfo.uid.isEmpty()) {
        json += "\"userId\": null";
    }
    else {
        json += "\"userId\": " + _basicInfo.uid + "";
    }

    json += "}";

    QByteArray logdata(json.toUtf8().data());
    return logdata;
}

void LogMan::run()
{
    FILE *fp = fopen("log/debug_NLOG.log", "wb+");
    if (fp) {
        fprintf(fp, "LogMan started...\r\n");
        fclose(fp);
    }

    for (;;) {
        __log_lock.lock();
        int count = _logList.count();
        if (count == 0) {
            __log_lock.unlock();
            sleep(1);
            continue;
        }
        LogItem item = _logList[0];
        _logList.pop_front();
        __log_lock.unlock();

        // upload log
        QByteArray data = buildJsonLog(item);
        QUrl url("http://if.xianshuabao.com/commonlog.ashx");
        QByteArray out;
        int rc;
        try {
            rc = ZDMHttp::post(url.toString().toUtf8().data(), data.data(), out);
        }
        catch (...) {
            // network exception
            continue;
        }

        if (rc) {
            // set break point
            //rc = 0;
        }

        data = QByteArray::fromPercentEncoding(data);
        FILE *fp = fopen("log/debug_NLOG.log", "a");
        if (fp) {
            fprintf(fp, "%d, %s\r\n", rc, data.data());
            fclose(fp);
        }
    }
}
#endif

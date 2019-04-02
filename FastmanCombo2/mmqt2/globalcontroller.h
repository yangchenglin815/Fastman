#ifndef GLOBALCONTROLLER_H
#define GLOBALCONTROLLER_H

#define MMQT2_VER "v3.6.8"
#define MMQT2_DATE_TIME QString("%1 %2").arg(__DATE__).arg(__TIME__)
#define MMQT2_VER_DATE_TIME QString("%1 %2 %3").arg(MMQT2_VER).arg(__DATE__).arg(__TIME__)

#include <QString>

class ZThreadPool;
class AppConfig;
class ZDownloadManager;
class AdbTracker;
class LoginManager;

class GlobalController
{
    GlobalController();
    ~GlobalController();
public:
    bool needsQuit;
    ZThreadPool *threadpool;
    AppConfig *appconfig;
    ZDownloadManager *downloadmanager;
    AdbTracker *adbTracker;
    LoginManager *loginManager;

    static GlobalController *getInstance();
    static bool getFileMd5(const QString& path, QString& dest_md5, quint64& dest_size);
    static bool getApkRsaMd5(const QString& path, QString& dest_md5);
};

#endif // GLOBALCONTROLLER_H

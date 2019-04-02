#ifndef ZMASTERSERVER2_H
#define ZMASTERSERVER2_H
#if !defined(_WIN32)

#include "zctools.h"
#include "qmutex.h"
#include "zmaster2config.h"
#include "zmsg2.h"
#include <stdio.h>
#include <string.h>

using namespace std;

typedef struct ZMINFO {
    int euid;
    int sdk_int;
    int cpu_abi;
    unsigned long long sys_free;
    unsigned long long sys_total;
    unsigned long long tmp_free;
    unsigned long long tmp_total;
    unsigned long long store_free;
    unsigned long long store_total;
    long long mem_freeKB;
    long long mem_totalKB;
    char tmp_dir[256];
    char store_dir[256];
} ZMINFO;

class ZMasterServer2
{
    int server_fd;
    int server_port;
    char server_name[128];
public:
    ZMINFO server_info;
    bool needsQuit;
    bool postDeleteApk;

    bool needZAgentInstall;
    bool needCheckInstallFailed;
    bool needDelayInstall;

    QMutex taskMutex;
    QList<ZMsg2 *> taskList;
    pthread_t taskTid;
    bool taskHasError;

    // install result
    char installIDs[1024];
    char installHiddenIDs[1024];
    QMap<int, char*> uploadDataMap;
    int installTotal;
    int installSucceed;
    int installFailed;

    // for install failed
    char installFailedApkIDs[1024];
    char installFailedNewVersions[1024];
    char clientVersion[128];

    // for uiautomator
    bool needKillUiautomator;
    bool killUiautomatorNotTimeout;

    u16 alertType;

    static int getSdkInt();
    bool init();
    ZMasterServer2();
    ~ZMasterServer2();

    bool listen(int port);
    bool listen(char *name);
    void stop();
};

#endif
#endif // ZMASTERSERVER2_H

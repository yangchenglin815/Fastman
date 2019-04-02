#ifndef ZMSG2_H
#define ZMSG2_H

#include "zbytearray.h"

#define ZMSG2_MAGIC 0x47534d5a
#define ZMSG2_VERSION 0x0001 // 0.01

// .... ..xx last 2 bits for conntype
// xxxx xx device_index
#define ZMSG2_CONNTYPE_LOST      0
#define ZMSG2_CONNTYPE_USB       1
#define ZMSG2_CONNTYPE_WIFI      2

#define ZMSG2_ALERT_NONE                      0x3000
#define ZMSG2_ALERT_ASYNC_DONE                0x3001
#define ZMSG2_ALERT_ASYNC_FAIL                0x3002
#define ZMSG2_ALERT_INSTALL_DONE              0x3003
#define ZMSG2_ALERT_INSTALL_FAIL              0x3004
#define ZMSG2_ALERT_FINISH_UNPLUP             0x3005
#define ZMSG2_ALERT_FINISH_POWEROFF           0x3006
#define ZMSG2_ALERT_REPORT_FAILED             0x3007
#define ZMSG2_ALERT_ASYNC_INSTALL_FINISH      0x3008
#define ZMSG2_ALERT_ASYNC_REPORT_FAILED       0x3009
#define ZMSG2_ALERT_ASYNC_FINISH_POWEROFF     0x3010

#define ZMSG2_ALERT_SOUND        16
#define ZMSG2_ALERT_VIBRATE      32

// apk local socket cmds
#define ZMSG2_CMD_RESET_STATUS  0x1001
#define ZMSG2_CMD_SET_CONN_TYPE 0x1002
#define ZMSG2_CMD_ADD_LOG       0x1003
#define ZMSG2_CMD_SET_HINT      0x1004
#define ZMSG2_CMD_SET_PROGRESS  0x1005
#define ZMSG2_CMD_SET_ALERT     0x1006
#define ZMSG2_CMD_ADD_INSTLOG   0x1007
#define ZMSG2_CMD_INSTALL_APP         0x1023
#define ZMSG2_CMD_UNINSTALL_APP       0x1024
#define ZMSG2_CMD_ASYNC_CHECK_UPLOAD_RESULT   0x1016
#define ZMSG2_CMD_GET_LOG_DIR 0x1018
#define ZMSG2_CMD_UPLOAD_INSTALL_FAILED 0x1019

#define ZMSG2_CMD_GET_CURTIME         0x1009
#define ZMSG2_CMD_SET_TIMEGAP         0x1010
#define ZMSG2_CMD_GET_BASICINFO       0x1011
#define ZMSG2_CMD_GET_APPINFO         0x1012
#define ZMSG2_CMD_GET_APPINFO_NOICON  0x1013
#define ZMSG2_CMD_SET_SCREEN_OFF_TIMEOUT 0x1014

// zmaster2 shell/root (l)socket cmds
#define ZMSG2_SWITCH_ROOT       0x2000
#define ZMSG2_SWITCH_QUEUE      0x2001

#define ZMSG2_CMD_GET_ZMINFO    0x4000
#define ZMSG2_CMD_QUIT          0x4001
#define ZMSG2_CMD_EXEC_QUEUE    0x4002
#define ZMSG2_CMD_CANCEL_QUEUE  0x4003
#define ZMSG2_CMD_QUIT_QUEUE    0x4004
#define ZMSG2_CMD_SHELL_QUEUE   0x4005
#define ZMSG2_CMD_SAVE_UPLOAD_URL_WITH_PARAMS_QUEUE   0x4006

#define ZMSG2_CMD_PUSH          0x4010
#define ZMSG2_CMD_PULL          0x4011
#define ZMSG2_CMD_SYSCALL       0x4012
#define ZMSG2_CMD_EXEC          0x4013
#define ZMSG2_CMD_RM            0x4014

#define ZMSG2_CMD_INSTALL_APK   0x4020
#define ZMSG2_CMD_INST_APK_SYS  0x4021
#define ZMSG2_CMD_MOVE_APK      0x4022
#define ZMSG2_CMD_START_APPS    0x4023
#define ZMSG2_CMD_EXEC_SHELL_THREAD   0x4024
#define ZMSG2_CMD_OLDDRIVER     0x4025

#define ZMSG2_CMD_SET_ALERT_TYPE 0x4028

#define ZMSG2_CMD_GET_PROPS     0x4030
#define ZMSG2_CMD_GET_FREESPACE 0x4031
#define ZMSG2_CMD_GET_FILEMD5   0x4032
#define ZMSG2_CMD_GET_APKSAMPLE 0x4033
#define ZMSG2_CMD_SEARCH_APKSTR 0x4034
#define ZMSG2_CMD_GET_FILESIZE  0x4035
#define ZMSG2_CMD_GET_FILELIST  0x4036
#define ZMSG2_CMD_GET_SDCARDLIST 0x4037

#define ZMSG2_CMD_INVOKE_PROTECT 0x4040
#define ZMSG2_CMD_INVOKE_SU      0x4041

#define ZMSG2_CMD_SET_VERSION 0x5000

enum {
    LOCATION_AUTO = 0,
    LOCATION_PHONE,
    LOCATION_STORAGE
};

class ZMsg2
{
public:
    u16 ver;
    u16 cmd;
    ZByteArray data;

    ZMsg2();
    ZMsg2 *makeCopy();

    bool parse(ZByteArray &source);
    ZByteArray getPacket();

    bool writeTo(int fd);
};

#endif // ZMSG2_H

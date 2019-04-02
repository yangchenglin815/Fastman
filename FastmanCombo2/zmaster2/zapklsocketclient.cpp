#if !defined(_WIN32)
#include "zapklsocketclient.h"
#include "zmaster2config.h"
#include "msocket.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include "zlog.h"

ZApkLSocketClient::ZApkLSocketClient() {
    DBG("ZApkLSocketClient + %p\n", this);
    fd = -1;
}

ZApkLSocketClient::~ZApkLSocketClient() {
    DBG("ZApkLSocketClient ~ %p\n", this);
    if(fd != -1) {
        ::close(fd);
    }
}

bool ZApkLSocketClient::recv(ZMsg2 &msg) {
    msg.data.clear();
    if(fd == -1) {
        DBG("fd==-1, so reconnect local host\n");
        fd = socket_connect("127.0.0.1", ZMASTER2_APK_PORT, 200);
        DBG("ret=%d\n", fd);
        if(fd == -1) {
            DBG("connect local host failed!\n");
            return false;
        }
        socket_setblock(fd, 1);
    }

    u32 magic, left;
    if(read(fd, &magic, sizeof(magic)) != sizeof(magic)) {
        DBG("cannot read magic! errno=<%d>\n", errno);
        return false;
    }
    if(read(fd, &left, sizeof(left)) != sizeof(left)) {
        DBG("cannot read len! errno=<%d>\n", errno);
        return false;
    }
    if(magic != ZMSG2_MAGIC) {
        DBG("invalid magic!\n");
        return false;
    }

    char buf[4096];
    int n;
    ZByteArray packet(false);
    packet.putInt(magic);
    packet.putInt(left);
    left += 2; // for crc32
    while(left > 0) {
        n = read(fd, buf, left > sizeof(buf) ? sizeof(buf) : left);
        if(n <= 0) {
            break;
        }
        left -= n;
        packet.append(buf, n);
    }

    if(left != 0) {
        DBG("incomplete read, left = %d\n", left);
        return false;
    }

    return msg.parse(packet);
}

bool ZApkLSocketClient::sendAndRecv(ZMsg2 &msg) {
    if(fd == -1) {
        fd = socket_connect("127.0.0.1", ZMASTER2_APK_PORT, 200);
        if(fd == -1) {
            return false;
        }
        socket_setblock(fd, 1);
    }

    return msg.writeTo(fd) && recv(msg);
}

bool ZApkLSocketClient::resetStatus() {
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_RESET_STATUS;
    return sendAndRecv(msg);
}

bool ZApkLSocketClient::setConnType(u16 index, u16 type) {
    ZMsg2 msg;
    u16 t = type;
    t |= (index << 2);

    msg.cmd = ZMSG2_CMD_SET_CONN_TYPE;
    msg.data.putShort(t);
    return sendAndRecv(msg);
}

bool ZApkLSocketClient::getLogDir(char *log_dir)
{
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_GET_LOG_DIR;

    bool ret = sendAndRecv(msg);
    int i = 0;
    msg.data.getNextInt(i);
    log_dir = msg.data.getNextUtf8(i);

    return ret;
}

bool ZApkLSocketClient::addLog(char *str) {
    ZMsg2 msg;

    // sec
    time_t raw;
    struct tm *tm;
    time(&raw);
    tm = localtime(&raw);
    u64 sec = (u64)mktime(tm);

    // msec
    struct timeval tv;
    gettimeofday(&tv, NULL);
    u64 msec = tv.tv_usec / 1000;

    u64 now = sec * 1000 + msec;

    msg.cmd = ZMSG2_CMD_ADD_LOG;
    msg.data.putInt64(now);  // msec since Epoch
    msg.data.putUtf8(str);
    return sendAndRecv(msg);
}

bool ZApkLSocketClient::setHint(char *str) {
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_SET_HINT;
    msg.data.putUtf8(str);
    return sendAndRecv(msg);
}

bool ZApkLSocketClient::setProgress(u16 value, u16 subValue, u16 total) {
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_SET_PROGRESS;
    msg.data.putShort(value);
    msg.data.putShort(subValue);
    msg.data.putShort(total);
    return sendAndRecv(msg);
}

bool ZApkLSocketClient::setAlert(u16 type) {
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_SET_ALERT;
    msg.data.putShort(type);
    return sendAndRecv(msg);
}

bool ZApkLSocketClient::setScreenOffTimeout(u64 timeout)
{
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_SET_SCREEN_OFF_TIMEOUT;
    msg.data.putInt64(timeout);
    return sendAndRecv(msg);
}

bool ZApkLSocketClient::checkUploadInstallFailed(char *apkIDs, char *newVersions, char *clientVersion)
{
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_UPLOAD_INSTALL_FAILED;
    msg.data.putUtf8(apkIDs);
    msg.data.putUtf8(newVersions);
    msg.data.putUtf8(clientVersion);

    char hint[1024];
    snprintf(hint, sizeof(hint), "Tell ZAgent url for upload install failed, apkIDs=<%s>, newVersions=<%s>, clientVersion=<%s>",
             apkIDs, newVersions, clientVersion);
    DBG("%s\n", hint);

    bool ret = sendAndRecv(msg);
    if (!ret) {
        DBG("checkUpload install failed sendAndRecv failed!\n");
        return true;
    }

    int i = 0;
    msg.data.getNextInt(i);
    char *str = msg.data.getNextUtf8(i);

    snprintf(hint, sizeof(hint), "Get result from ZAgent=<%d><%s>", ret, str);
    DBG("%s\n", hint);

    return (ret && (strncmp(str, "OK", strlen("OK")) == 0));
}

bool ZApkLSocketClient::checkUpload(char *urlWithParams, char *IMEI, int installTotal, int installSucceed)
{
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_ASYNC_CHECK_UPLOAD_RESULT;
    msg.data.putUtf8(IMEI);
    msg.data.putUtf8(urlWithParams);
    msg.data.putInt(installTotal);
    msg.data.putInt(installSucceed);
    msg.data.putInt(installTotal - installSucceed);

    char hint[1024];
    snprintf(hint, sizeof(hint), "Tell ZAgent url for upload=<%s>", urlWithParams);
    DBG("%s\n", hint);

    bool ret = sendAndRecv(msg);
    if (!ret) {
        DBG("checkUpload sendAndRecv failed!\n");
        return true;
    }

    int i = 0;
    msg.data.getNextInt(i);
    char *str = msg.data.getNextUtf8(i);

    snprintf(hint, sizeof(hint), "Get result from ZAgent=<%d><%s>", ret, str);
    DBG("%s\n", hint);

    return (ret && (strncmp(str, "OK", strlen("OK")) == 0));
}

bool ZApkLSocketClient::installApp(char *path, char *hint)
{
    ZMsg2 msg;
    msg.cmd = ZMSG2_CMD_INSTALL_APP;
    msg.data.putUtf8(path);

    snprintf(hint, sizeof(hint), "installApp path=<%s>", path);
    DBG("%s\n", hint);

    bool ret = sendAndRecv(msg);
    if (!ret) {
        DBG("installApp sendAndRecv failed!\n");
    }

    int i = 0;
    msg.data.getNextInt(i);
    char *str = msg.data.getNextUtf8(i);

    snprintf(hint, sizeof(hint), "Get result from ZAgent=<%d><%s>", ret, str);
    DBG("%s\n", hint);

    return (ret && (strncmp(str, "OK", strlen("OK")) == 0));
}
#endif

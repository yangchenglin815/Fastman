#if !defined(_WIN32)
#include "zmasterserver2.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <errno.h>
#include <dirent.h>
#include <utime.h>

#include "msocket.h"
#include "zctools.h"
#include "cprocess.h"
#include "apkhelper.h"
#include "zapklsocketclient.h"
#include "protect_bin.h"
#include "zlog.h"
#include "md5.h"

extern char log_dir[256] = "/data/local/tmp/work";

ZMasterServer2::ZMasterServer2() {
    server_fd = -1;
    server_port = -1;
    server_name[0] = '\0';
    needsQuit = false;
    postDeleteApk = false;
    taskTid = -1;
    taskHasError = false;
    installTotal = 0;
    installSucceed = 0;
    installFailed = 0;
    alertType = ZMSG2_ALERT_SOUND | ZMSG2_ALERT_VIBRATE;
    memset(&server_info, 0, sizeof(ZMINFO));
}

ZMasterServer2::~ZMasterServer2() {
    while(!taskList.isEmpty()) {
        delete taskList.takeAt(0);
    }

    if(server_fd > -1) {
        close(server_fd);
        server_fd = -1;
    }
}

// declare fnc
void *task_exec_shell_thread(void *z);
void *task_exec_shell_thread_kill_uiautomator(void *z);
//void *task_olddriver_threak(void *z);
char *getVersionNameByPkg(const char *pkg);



void *forward_thread(void *z) {
    ZByteArray *arg = (ZByteArray*) z;
    int i = 0;
    int rfd = arg->getNextInt(i);
    int wfd = arg->getNextInt(i);
    bool *fwdQuit = (bool*) arg->getNextInt64(i);
    delete arg;

    char buf[4096];
    int n;

    while(*fwdQuit != true) {
        n = socket_read(rfd, buf, sizeof(buf), 500);
        if(n == -2) { // timeout
            continue;
        } else if(n <= 0) {
            *fwdQuit = true;
            break;
        }
        write(wfd, buf, n);
    }

    DBG("read on %d finished\n", rfd);
    pthread_exit(NULL);
}

static bool forward_connection(int fdr, int fdw, ZMsg2 &msg, ZByteArray &data) {
    DBG("forward %d->%d\n", fdr, fdw);
    if(fdr == -1 || fdw == -1) {
        return false;
    }

    ZByteArray *arg = new ZByteArray(false);
    bool fwdQuit = false;

    pthread_t tid;
    arg->putInt(fdw);
    arg->putInt(fdr);
    arg->putInt64((u64) &fwdQuit);
    pthread_create(&tid, NULL, forward_thread, arg);

    msg.writeTo(fdw);
    if(data.size() > 0) {
        write(fdw, data.data(), data.size());
    }

    char buf[4096];
    int n;

    while((n = read(fdr, buf, sizeof(buf))) > 0) {
        write(fdw, buf, n);
    }
    fwdQuit = true;

    DBG("read on %d finished\n", fdr);
    pthread_join(tid, NULL);
    DBG("forward finished!\n");
    close(fdw);
    return true;
}

bool sendAndRecvZMsg(ZMsg2& msg, int fd) {
    if(fd == -1) {
        return false;
    }

    socket_setblock(fd, 1);
    if(!msg.writeTo(fd)) {
        DBG("write ZMsg error!\n");
        return false;
    }
    msg.data.clear();

    u32 magic, left;
    if(read(fd, &magic, sizeof(magic)) != sizeof(magic)) {
        DBG("cannot read magic!\n");
        return false;
    }
    if(read(fd, &left, sizeof(left)) != sizeof(left)) {
        DBG("cannot read len!\n");
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

// xxxx:yyy:zzz
// 0 -> xxxx
// 1 -> yyy
static char *getArg(char *src, int len, int index, char key = ':') {
    int i = 0;
    int c = 0;
    int from = 0;
    int to = len;
    for (; i < len; i++) {
        char tmp = *(src + i);
        if (tmp == key) {
            ++c;
            if (c == index) {
                from = i + 1;
            } else if (c == index + 1) {
                to = i;
                break;
            }
        }
    }

    char *ret = (char *) malloc(to - from + 1);
    *(ret + to - from) = '\0';
    memcpy(ret, src + from, to - from);
    return ret;
}

void get_prop(const char *key, char *buf) {
    FILE *fp = fopen("/system/build.prop", "rb");
    buf[0] = '\0';
    if(fp == NULL) {
        return;
    }

    char line[512];
    while(fgets(line, sizeof(line), fp) != NULL) {
        if(strstr(line, key) == line) {
            char *p = line + strlen(key);
            char *q = buf;
            if(*p != '=') {
                continue;
            }

            p++;
            while(*p != '\n' && *p != '\0') {
                *q++ = *p++;
            }
            *q = '\0';
            break;
        }
    }
}

void handleSetClientInfo(ZMasterServer2 *server, ZMsg2& msg, int cli_fd) {
    DBG("handleSetClientInfo start\n");
    int i = 0;
    char *version = msg.data.getNextUtf8(i);
    int needZAgentInstall = msg.data.getNextByte(i);

    DBG("version=<%s>\n", version);
    snprintf(server->clientVersion, sizeof(server->clientVersion), "%s", version);

    DBG("needZAgentInstall=<%d>\n", needZAgentInstall);
    server->needZAgentInstall = (needZAgentInstall == 1);

    msg.writeTo(cli_fd);
    DBG("handleSetClientInfo end\n");
}

// req [head]
// resp [head][int euid][int sdk][u64 sys_free][u64 tmp_free][u64 stor_free]<tmp><storage>
void handleGetZMInfo(ZMasterServer2 *server, ZMsg2& msg, int cli_fd) {
    msg.data.clear();
    msg.ver = ZMSG2_VERSION;

    server->server_info.sys_free = getFreeSize("/system");
    server->server_info.sys_total = getMountSize("/system");
    server->server_info.tmp_free = getFreeSize(server->server_info.tmp_dir);
    server->server_info.tmp_total = getMountSize(server->server_info.tmp_dir);
    server->server_info.store_free = getFreeSize(server->server_info.store_dir);
    server->server_info.store_total = getMountSize(server->server_info.store_dir);
    getMemInfoKB(server->server_info.mem_freeKB, server->server_info.mem_totalKB);

    msg.data.putInt(server->server_info.euid);
    msg.data.putInt(server->server_info.sdk_int);
    msg.data.putInt(server->server_info.cpu_abi);
    msg.data.putInt64(server->server_info.sys_free);
    msg.data.putInt64(server->server_info.sys_total);
    msg.data.putInt64(server->server_info.tmp_free);
    msg.data.putInt64(server->server_info.tmp_total);
    msg.data.putInt64(server->server_info.store_free);
    msg.data.putInt64(server->server_info.store_total);
    msg.data.putUtf8(server->server_info.tmp_dir);
    msg.data.putUtf8(server->server_info.store_dir);
    msg.data.putInt64(server->server_info.mem_freeKB);
    msg.data.putInt64(server->server_info.mem_totalKB);
    msg.writeTo(cli_fd);
}

bool fast_crc32(const char* path, u64& size, uint& ret) {
    ret = crc32(0, NULL, 0);
    char buf[4096];
    int n = -1;

    FILE *fp = fopen(path, "rb");
    if(fp == NULL) {
        DBG("file open failed!\n");
        return false;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);
    if(size < sizeof(buf)) {
        n = fread(buf, 1, sizeof(buf), fp);
        ret = crc32(ret, (const u8*)buf, n);
        fclose(fp);
        return true;
    }

    u64 parts = size/sizeof(buf);
    if(parts > 20) {
        parts = 20;
    }

    u64 blkSize = size/parts;
    u64 rest = size % blkSize;
    u64 pos = 0;

    DBG("parts = %lld, blkSize = %lld, rest = %lld\n", parts, blkSize, rest);
    for(u64 i=0; i<parts; i++) {
        if(i != parts - 1) {
            n = fread(buf, 1, sizeof(buf), fp);
            ret = crc32(ret, (const u8*)buf, n);
            pos += blkSize;
        } else {
            pos += blkSize;
            pos += rest;
            pos -= sizeof(buf);
        }
        fseek(fp, pos, SEEK_SET);
    }

    n = fread(buf, 1, sizeof(buf), fp);
    ret = crc32(ret, (const u8*)buf, n);

    DBG("ret = %u\n", ret);
    fclose(fp);
    return true;
}

// req [head]"size:path:uid:gid:mode:mtime:crc32"
// resp [head]"code:message"
// req <file content>
void handlePush(ZMasterServer2 *server, ZMsg2& msg, int cli_fd) {
    int i = 0;
    u64 left = msg.data.getNextInt64(i);
    char *str_path = msg.data.getNextUtf8(i);
    int uid = msg.data.getNextInt(i);
    int gid = msg.data.getNextInt(i);
    int mode = msg.data.getNextInt(i);
    uint mtime = msg.data.getNextInt(i);
    uint remote_crc = msg.data.getNextInt(i);
    msg.data.clear();

    u64 local_size;
    uint local_crc;
    if(fast_crc32(str_path, local_size, local_crc)
            && local_size == left
            && remote_crc == local_crc) {
        msg.data.putInt(255);
        msg.data.putUtf8("文件已存在");
        msg.writeTo(cli_fd);
        free(str_path);
        return;
    }

    unlink(str_path);
    mkdirForFile(str_path);
    u64 freeSize = getFreeSizeForFile(str_path);
    char buf[8192];
    int n = -1;
    struct utimbuf ut;

    do {
        if (freeSize <= left) {
            msg.data.putInt(-1);
            msg.data.putUtf8("剩余空间不足");
            msg.writeTo(cli_fd);
            break;
        }

        FILE *fp = fopen(str_path, "wb");
        if (fp == NULL) {
            msg.data.putInt(-2);
            msg.data.putUtf8("创建文件失败");
            msg.writeTo(cli_fd);
            break;
        }

        msg.data.putInt(0);
        msg.data.putUtf8("OK");
        msg.writeTo(cli_fd);

        while (left > 0) {
            n = read(cli_fd, buf, left > 8192 ? 8192 : left);
            if (n <= 0) {
                break;
            }

            fwrite(buf, 1, n, fp);
            left -= n;
        }
        fflush(fp);
        fclose(fp);
        if(uid != -1) {
            lchown(str_path, uid, gid);
        }
        chmod(str_path, mode);
        if(mtime != 0) {
            ut.actime = time(NULL);
            ut.modtime = mtime;
            utime(str_path, &ut);
        }
        msg.writeTo(cli_fd);
    } while (0);

    free(str_path);
}

// req [head]"path"
// resp [head]"code:message"
// resp <file content>
void handlePull(ZMasterServer2 *server, ZMsg2& msg, int cli_fd) {
    int i = 0;
    char *str_path = msg.data.getNextUtf8(i);
    msg.data.clear();

    FILE *fp = fopen(str_path, "rb");
    free(str_path);

    if (fp == NULL) {
        msg.data.putInt(-1);
        msg.data.putUtf8("无法读取文件");
        msg.writeTo(cli_fd);
        return;
    }

    fseek(fp, 0, SEEK_END);
    u32 size = ftell(fp);
    rewind(fp);

    msg.data.putInt(size);
    msg.data.putUtf8("OK");
    msg.writeTo(cli_fd);

    char buf[8192];
    int n = -1;
    while ((n = fread(buf, 1, 8192, fp)) > 0) {
        if(cli_fd != -1) write(cli_fd, buf, n);
    }
    fclose(fp);
    msg.writeTo(cli_fd);
}

void handleSysCall(ZMasterServer2 *server, ZMsg2& msg, int cli_fd) {

}

// req [head][argc][argv]...
// resp [head][ret][data_len][var data]
void handleExec(ZMasterServer2 *server, ZMsg2& msg, int cli_fd) {
    int i = 0, j = 0;
    int argc = msg.data.getNextInt(i);
    char **argv = new char*[argc+1];

    for(j=0; j<argc; j++) {
        argv[j] = msg.data.getNextUtf8(i);
    }
    argv[argc] = NULL;

    msg.data.clear();

    ZByteArray out(false);
    int ret = CProcess::execvp(out, argv);

    for(j=0; j<argc; j++) {
        free(argv[j]);
    }
    delete[] argv;

    msg.data.putInt(ret);
    msg.data.putInt(out.length());
    msg.data.append(out);

    msg.writeTo(cli_fd);
}

// req [head]"path"[bool delself]
// resp [head]
void handleRemove(ZMasterServer2 *server, ZMsg2& msg, int cli_fd) {
    int i = 0;
    char *path = msg.data.getNextUtf8(i);
    bool delself = msg.data.getNextByte(i) == 1;
    DBG("delself = %d\n", delself);
    msg.data.clear();

    if(strBeginsWith(path, "/system")) {
        if(geteuid() == 0 && !dir_writable("/system")) {
            DBG("WTF, /system readonly??\n");
            mount(NULL, "/system", NULL, MS_REMOUNT, NULL);
        }
    }

    rm_files(path, delself);
    free(path);
    msg.writeTo(cli_fd);
}

bool hasPathInZip(const char *path, const char *prefix) {
    unzFile file = unzOpen(path);
    if(file == NULL) {
        DBG("unzOpen fail!\n");
        return false;
    }

    bool ret = false;
    unz_global_info global_info;
    unz_file_info file_info;
    char entry[256];
    int r = unzGetGlobalInfo(file, &global_info);
    for (uLong i = 0; i < global_info.number_entry; i++) {
        if ((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            DBG("unzGetCurrentFileInfo error\n");
            break;
        }

        if(strBeginsWith(entry, prefix)) {
            DBG("found lib '%s'\n", entry);
            ret = true;
            break;
        }

        if (i < global_info.number_entry - 1) {
            if ((r = unzGoToNextFile(file)) != UNZ_OK) {
                DBG("unzGoToNextFile error\n");
                break;
            }
        }
    }
    unzClose(file);
    return ret;
}

// req [head]"path"
// resp [head]"code:message"
void handleInstallApkSys(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    char *path = msg.data.getNextUtf8(i);
    char dest[512];
    msg.data.clear();

    char *lib_prefix = "lib/armeabi/lib";
    if(server->server_info.cpu_abi == 1) { // x86
        lib_prefix = "lib/x86/lib";
    } else if(server->server_info.cpu_abi == 2) { // mips
        lib_prefix = "lib/mips/lib";
    } else if(server->server_info.sdk_int >= 17) { // armeabi-v7a
        if(hasPathInZip(path, "lib/armeabi-v7a/lib")) {
            lib_prefix = "lib/armeabi-v7a/lib";
        }
    }

    do {
        if(!dir_writable("/system/app")) {
            mount(NULL, "/system", NULL, MS_REMOUNT, NULL);
        }

        if(!dir_writable("/system/app")) {
            msg.data.putInt(-1);
            msg.data.putUtf8("system read-only");
            break;
        }

        if(getFreeSize("/system/app") < getFileSize(path) + 1024*1024) {
            msg.data.putInt(-1);
            msg.data.putUtf8("system space not sufficient");
            break;
        }

        snprintf(dest, sizeof(dest), "/system/app/%s", strBasename(path));
        if(!strEndsWith(dest, ".apk")) {
            strcat(dest, ".apk");
        }

        if(copy_apklibs(lib_prefix, path) && copy_file(path, dest)) {
            DBG("install to system success\n");
            msg.data.putInt(0);
            msg.data.putUtf8("OKSYS");
        }
    } while(0);

    msg.writeTo(cli_fd);
}

bool invoke_pm_install(ZMasterServer2 *server, char *path, u8 location, ZByteArray& resp) {
    ZByteArray out(false);
    char cmd[512];
    char *p = NULL;
    char *q = NULL;
    char *args = "-r";

    if(server->server_info.sdk_int > 7) { // no location support until 2.2
        switch(location) {
        case LOCATION_PHONE:
            args = "-r -f";
            break;
        case LOCATION_STORAGE:
            args = "-r -s";
            break;
        default:
            if(server->server_info.tmp_free < 40*1024*1024
                    && server->server_info.store_free > server->server_info.tmp_free) {
                location = LOCATION_STORAGE;
                args = "-r -s";
            }
            break;
        }
    }

    DBG("install apk at %s, %s\n", path, args);
    snprintf(cmd, sizeof(cmd), "CLASSPATH=/system/framework/pm.jar "
                               "app_process /system/bin com.android.commands.pm.Pm "
                               "install %s '%s' 2>&1", args, path);
    CProcess::exec(cmd, out);
    DBG("out = [%s]\n", out.data());

    resp.clear();
    if((p = strstr(out.data(), "Failure [")) != NULL) {
        resp.putInt(1);
        p += strlen("Failure [");
        q = strchr(p, ']');
        if(q == NULL) {
            resp.putUtf8("unknown error");
        } else {
            *q = '\0';
            resp.putUtf8(p);
        }
    } else if ((p = strstr(out.data(), "pkg: /")) != NULL || (p = strstr(out.data(), "Success")) != NULL) {
        resp.putInt(0);
        resp.putUtf8("OK");
        return true;
    } else {
        resp.putInt(2);
        resp.putUtf8("unknown error");
    }
    return false;
}

//void handleOldDriver(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
//    int i = 0;
//    char cmd[512];
//    char *pkgs = msg.data.getNextUtf8(i);
//    msg.data.clear();
//    pthread_t ntid;
//    pthread_create(&ntid, NULL, task_olddriver_threak, pkgs);

//}

// req [head]rpath,pkg,name,loc,sys
// resp [head]code,msg
void handleInstallApk(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    char *path = msg.data.getNextUtf8(i);
    char *pkg = msg.data.getNextUtf8(i);
    char *name = msg.data.getNextUtf8(i);
    u8 location = msg.data.getNextByte(i);
    u8 sys = msg.data.getNextByte(i);
    char *id = msg.data.getNextUtf8(i);
    int isLocalApk = msg.data.getNextInt(i);
    int isHiddenApk = msg.data.getNextInt(i);
    msg.data.clear();

    ZByteArray out(false);
    char cmd[512];
    char hint[512];
    char *args = "-r";
    char *p = NULL;
    char *q = NULL;

    ZApkLSocketClient apkCli;
    DBG("path=%s pkg=%s name=%s loc=%d sys=%d\n", path, pkg, name, location, sys);

    if(!getFileExists(path)) {
        snprintf(cmd, sizeof(cmd), "file '%s' not found!\n", path);
        msg.data.putInt(-1);
        msg.data.putUtf8(cmd);
        goto handle_inst_apk_end;
    }

    if (!server->needZAgentInstall) {
        // system
        if(sys != 0) {
            ZMsg2 tmpmessage;
            tmpmessage.cmd = ZMSG2_CMD_INST_APK_SYS;
            tmpmessage.data.putUtf8(path);
            if(geteuid() == 0) {
                handleInstallApkSys(server, tmpmessage, -1);
            } else {
                if(!sendAndRecvZMsg(tmpmessage, socket_connect("127.0.0.1", ZMASTER2_ROOT_PORT, 1))) {
                    tmpmessage.data.clear();
                    tmpmessage.data.putInt(-1);
                    tmpmessage.data.putUtf8("error communicate with zmaster_root");
                }
            }

            i = 0;
            int tmpRet = tmpmessage.data.getNextInt(i);
            char *tmpHint = tmpmessage.data.getNextUtf8(i);

            if(tmpRet == 0) {
                apkCli.addLog("install to system success");
                msg.data.append(tmpmessage.data);
                goto handle_inst_apk_end;
            } else {
                //apkCli.addLog(tmpHint);
                //apkCli.addLog("install to system failed, try user space");
            }
            free(tmpHint);
        }
        if(strcmp(pkg, "com.iflytek.inputmethod") != 0){
            if(!invoke_pm_install(server, path, location, msg.data)
                    && location == LOCATION_STORAGE) {
                apkCli.addLog("install to storage failed, try phone space");
                invoke_pm_install(server, path, LOCATION_PHONE, msg.data);
            }
        }
handle_inst_apk_end:
        DBG_HEX(msg.data.data(), msg.data.size());
        i = 0;
        int retInt = msg.data.getNextInt(i);
        char *retStr = msg.data.getNextUtf8(i);
        DBG("install result, ret=<%d>, retStr=<%s>\n", retInt, retStr);

        // 安装成功，处理讯飞输入法
        if (strcmp(pkg, "com.iflytek.inputmethod") == 0) {
            DBG("install 讯飞输入法, so enable and set it\n");
            system("ime enable com.iflytek.inputmethod/.FlyIME;ime set com.iflytek.inputmethod/.FlyIME");
        }

        // 处理Failure code [-99], OPPLE手机有出现
        if (server->needCheckInstallFailed && retInt != 0) {
            DBG("sleep 10secs when need check install failed\n");
            xsleep(10000);
        }

        // 安装成功统计：只统计套餐内应用
        if (isLocalApk == 0) {
            if((strcmp(pkg, "com.tencent.mm") != 0) && (strcmp(pkg, "com.tencent.mobileqq") != 0) && (strcmp(pkg, "com.iflytek.inputmethod") != 0)){
                server->installTotal++;
            }
            if (retInt == 0) {
                if ((strlen(server->installIDs) + 3 + strlen(id)) < sizeof(server->installIDs)) {
                    if (strcmp(server->installIDs, "") != 0) {
                        strcat(server->installIDs, "%2c");
                    }
                    strcat(server->installIDs, id);
                }
                if((strcmp(pkg, "com.tencent.mm") != 0) && (strcmp(pkg, "com.tencent.mobileqq") != 0) && (strcmp(pkg, "com.iflytek.inputmethod") != 0)){
                    server->installSucceed++;
                }
            } else {
                if((strcmp(pkg, "com.tencent.mm") != 0) && (strcmp(pkg, "com.tencent.mobileqq") != 0) && (strcmp(pkg, "com.iflytek.inputmethod") != 0)){
                    server->installFailed++;
                }
                server->taskHasError = true;
            }

            if (retInt != 0 && (strstr(retStr, "INSTALL_FAILED_VERSION_DOWNGRADE") != NULL)) {
                if ((strlen(server->installFailedApkIDs) + 1 + strlen(id)) < sizeof(server->installFailedApkIDs)) {
                    if (strcmp(server->installFailedApkIDs, "") != 0) {
                        strcat(server->installFailedApkIDs, ",");
                    }
                    strcat(server->installFailedApkIDs, id);
                }

                char *versionName = getVersionNameByPkg(pkg);
                DBG("get version=<%s>\n", versionName);
                if ((strlen(server->installFailedNewVersions) + 1 + strlen(versionName)) < sizeof(server->installFailedNewVersions)) {
                    if (strcmp(server->installFailedNewVersions, "") != 0) {
                        strcat(server->installFailedNewVersions, ",");
                    }
                    strcat(server->installFailedNewVersions, versionName);
                }
            }
        }

        // 处理隐藏应用
        if (isHiddenApk == 1) {
            if (retInt == 0) {
                if ((strlen(server->installHiddenIDs) + 3 + strlen(id)) < sizeof(server->installHiddenIDs)) {
                    if (strcmp(server->installHiddenIDs, "") != 0) {
                        strcat(server->installHiddenIDs, "%2c");
                    }
                    strcat(server->installHiddenIDs, id);
                }
            }

            char *versionName = getVersionNameByPkg(pkg);
            DBG("get version=<%s>\n", versionName);
            if ((strlen(server->installFailedNewVersions) + 1 + strlen(versionName)) < sizeof(server->installFailedNewVersions)) {
                if (strcmp(server->installFailedNewVersions, "") != 0) {
                    strcat(server->installFailedNewVersions, ",");
                }
                strcat(server->installFailedNewVersions, versionName);
            }
        }

        if (retInt != 0) {
            snprintf(hint, sizeof(hint), "%s loc=%d sys=%d %s", name, location, sys, retStr);
            apkCli.addLog(hint);
        }

        free(retStr);

    } else {
        bool result = false;
        if(strcmp(pkg, "com.iflytek.inputmethod") != 0){
            // 安装
            do {
                // check invoke
                int times = 2;
                while (times--) {
                    char hint[1024];
                    result = apkCli.installApp(path, hint);
                    if (result) {
                        break;
                    }
                    if (strcmp(hint, "Failed2") == 0) {
                        system("uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#OPENACCESSBILITY");
                    }
                }
                if (!result) {
                    DBG("invoking ZAgent func failed\n");
                    break;
                }

                // check path
                times = 30;
                while (times--) {
                    xsleep(1000);

                    result = false;
                    char cmd[100];
                    cmd[0] = '\0';
                    strcat(cmd, "pm path ");
                    strcat(cmd, pkg);
                    FILE *file = popen(cmd, "r");
                    if (file != NULL) {
                        char tmp[1024];
                        while (fgets(tmp, sizeof(tmp), file) != NULL) {
                            if (tmp[strlen(tmp) - 1] == '\n') {
                                tmp[strlen(tmp) - 1] = '\0';
                            }

                            char startStr[10];
                            startStr[0] = '\0';
                            strncpy(startStr, tmp, 8);
                            DBG("pm path out=%s\n", tmp);
                            if (strncmp(startStr, "package:", 8) == 0) {
                                result = true;
                                break;
                            }
                        }
                        pclose(file);
                    } else {
                        DBG("popen failed\n");
                    }

                    if (result) {
                        break;
                    }
                }
            } while (0);
        }
        // 安装成功，处理讯飞输入法
        if (strcmp(pkg, "com.iflytek.inputmethod") == 0) {
            DBG("install 讯飞输入法, so enable and set it\n");
            system("ime enable com.iflytek.inputmethod/.FlyIME;ime set com.iflytek.inputmethod/.FlyIME");
        }

        // 安装成功统计：只统计套餐内应用
        if (isLocalApk == 0) {
            if((strcmp(pkg, "com.tencent.mm") != 0) && (strcmp(pkg, "com.tencent.mobileqq") != 0) && (strcmp(pkg, "com.iflytek.inputmethod") != 0)){
                server->installTotal++;
            }
            if (result) {
                if ((strlen(server->installIDs) + 3 + strlen(id)) < sizeof(server->installIDs)) {
                    if (strcmp(server->installIDs, "") != 0) {
                        strcat(server->installIDs, "%2c");
                    }
                    strcat(server->installIDs, id);
                }
                if((strcmp(pkg, "com.tencent.mm") != 0) && (strcmp(pkg, "com.tencent.mobileqq") != 0) && (strcmp(pkg, "com.iflytek.inputmethod") != 0)){
                    server->installSucceed++;
                }
            } else {
                if((strcmp(pkg, "com.tencent.mm") != 0) && (strcmp(pkg, "com.tencent.mobileqq") != 0) && (strcmp(pkg, "com.iflytek.inputmethod") != 0)){
                    server->installFailed++;
                }
                server->taskHasError = true;
            }
        }

        // 处理隐藏应用
        if (isHiddenApk == 1) {
            if (result) {
                if ((strlen(server->installHiddenIDs) + 3 + strlen(id)) < sizeof(server->installHiddenIDs)) {
                    if (strcmp(server->installHiddenIDs, "") != 0) {
                        strcat(server->installHiddenIDs, "%2c");
                    }
                    strcat(server->installHiddenIDs, id);
                }
            }

            char *versionName = getVersionNameByPkg(pkg);
            DBG("get version=<%s>\n", versionName);
            if ((strlen(server->installFailedNewVersions) + 1 + strlen(versionName)) < sizeof(server->installFailedNewVersions)) {
                if (strcmp(server->installFailedNewVersions, "") != 0) {
                    strcat(server->installFailedNewVersions, ",");
                }
                strcat(server->installFailedNewVersions, versionName);
            }
        }
    }

    if (server->postDeleteApk) {
        unlink(path);
    }

    free(path);
    free(pkg);
    free(name);

    msg.writeTo(cli_fd);
}

void handleMoveApk(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {

}

// req [head][type]
// resp [head]
void handleStartApps(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd){ //chensiyu. added.
    DBG("handleStartApps cli_fd = %d\n", cli_fd);
    int i = 0;
    char *key;
    ZByteArray tmp(false);
    key = msg.data.getNextUtf8(i);
    msg.data.clear();
    tmp.putUtf8(key);
    //msg.data.append(tmp);

    msg.writeTo(cli_fd);
    DBG("handleStartApps cli_fd = %d msg = %s \n", cli_fd,key);

}

// req [head][type]
// resp [head]
void handleExecShellInThread(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    char *cmd;
    cmd = msg.data.getNextUtf8(i);
    pthread_t ntid;
    bool ret = pthread_create(&ntid, NULL, task_exec_shell_thread, cmd);
    DBG("handleExecShellInThread ret=%d\n", ret);
    msg.writeTo(cli_fd);
}

// req [head][type]
// resp [head]
void handleSetAlertType(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    u16 type = msg.data.getNextShort(i);
    server->alertType = type;
    msg.data.clear();
    msg.writeTo(cli_fd);
}

// req [head][count][str][str]...
// resp [head][str][str]...
void handleGetProps(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    int count = msg.data.getNextInt(i);
    char *key;
    ZByteArray tmp(false);
    char line[512];

    while(count-- > 0) {
        key = msg.data.getNextUtf8(i);
        get_prop(key, line);
        tmp.putUtf8(line);
        free(key);
    }
    msg.data.clear();
    msg.data.append(tmp);
    msg.writeTo(cli_fd);
}

// req [head]"path"
// resp [head][u64]
void handleGetFreeSpace(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    char *path = msg.data.getNextUtf8(i);
    u64 ret = getFreeSize(path);
    free(path);

    msg.data.clear();
    msg.data.putInt64(ret);
    msg.writeTo(cli_fd);
}

// req [head]"path"
// resp [head][code]"msg/md5"
void handleGetFileMd5(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    char *path = msg.data.getNextUtf8(i);
    char md5[64];

    msg.data.clear();
    if(!getFileMd5(path, md5)) {
        msg.data.putInt(-1);
        msg.data.putUtf8("无法读取文件");
    } else {
        msg.data.putInt(0);
        msg.data.putUtf8(md5);
    }
    free(path);
    msg.writeTo(cli_fd);
}

// req [head]"path"
// resp [head]"code:message/size"
void handleGetFileSize(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    char *path = msg.data.getNextUtf8(i);
    long long size = getFileSize(path);

    msg.data.clear();
    if(size == -1) {
        msg.data.putInt(-1);
        msg.data.putUtf8("无法读取文件");
    } else {
        msg.data.putInt(0);
        msg.data.putInt64(size);
    }

    free(path);
    msg.writeTo(cli_fd);
}

void handleGetFileList(char *path, int depth, int maxdepth, ZMasterServer2 *server, ZMsg2 &msg, int cli_fd, QList<char *>& blackList);

void handleGetDirEntries(char *path, int depth, int maxdepth, ZMasterServer2 *server, ZMsg2 &msg, int cli_fd, QList<char *>& blackList) {
    if (depth > maxdepth) {
        return;
    }
    //DBG("depth=%d, path=[%s]\n", depth, path);

    char sub_path[PATH_MAX];
    DIR *dp = opendir(path);
    struct dirent* entry;

    if (dp == NULL) {
        DBG("error: %s\n", strerror(errno));
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if(depth == 1) {
            bool ignore = false;
            for(int i=0; i<blackList.size(); i++) {
                if(strcasecmp(blackList[i], entry->d_name) == 0) {
                    ignore = true;
                    break;
                }
            }

            if(ignore) {
                continue;
            }
        }

        snprintf(sub_path, sizeof(sub_path), "%s/%s", path, entry->d_name);
        handleGetFileList(sub_path, depth, maxdepth, server, msg, cli_fd, blackList);
    }
    closedir(dp);
}

void handleGetFileList(char *path, int depth, int maxdepth, ZMasterServer2 *server, ZMsg2 &msg, int cli_fd, QList<char *>& blackList) {
    if (depth > maxdepth) {
        return;
    }
    //DBG("depth=%d, path=[%s]\n", depth, path);

    struct stat st;
    char rpath[PATH_MAX];
    int n = -1;

    if (lstat(path, &st) < 0) {
        DBG("error: %s\n", strerror(errno));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        DBG("DIR [%s]\n", path);
        msg.data.clear();
        msg.data.putUtf8(path);
        msg.data.putInt(st.st_size);
        msg.data.putInt(st.st_mode);
        msg.data.putInt(st.st_mtime);
        msg.writeTo(cli_fd);
        handleGetDirEntries(path, depth + 1, maxdepth, server, msg, cli_fd, blackList);
    } else if (S_ISLNK(st.st_mode)) {
        char *p = realpath(path, rpath);
        if(p == NULL) {
            DBG("error: %s\n", strerror(errno));
            return;
        }

        DBG("LNK [%s] => [%s]\n", path, rpath);
        msg.data.clear();
        msg.data.putUtf8(path);
        msg.data.putInt(st.st_size);
        msg.data.putInt(st.st_mode);
        msg.data.putInt(st.st_mtime);
        msg.data.putUtf8(rpath);
        msg.writeTo(cli_fd);

        if (lstat(rpath, &st) < 0) {
            DBG("error: %s\n", strerror(errno));
            return;
        }
        if (S_ISDIR(st.st_mode)) {
            // not using rpath, to avoid path resolve problem for android
            handleGetDirEntries(path, depth + 1, maxdepth, server, msg, cli_fd, blackList);
        }
    } else {
        DBG("FLE [%s]\n", path);
        msg.data.clear();
        msg.data.putUtf8(path);
        msg.data.putInt(st.st_size);
        msg.data.putInt(st.st_mode);
        msg.data.putInt(st.st_mtime);
        msg.writeTo(cli_fd);
    }
}

// req [head]"path"[maxdepth]
// resp [head]"path"[size][mode][mtime]
// resp ...
// resp [head] // ends
void handleGetFileList(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    QList<char *> blackList;
    char *path = msg.data.getNextUtf8(i);
    int maxdepth = msg.data.getNextInt(i);
    int blackListSize = msg.data.getNextInt(i);
    while(blackListSize-- > 0) {
        char *name = msg.data.getNextUtf8(i);
        blackList.append(name);
    }

    // first response
    msg.data.clear();
    msg.data.putInt(0);
    msg.data.putUtf8("OK");
    msg.writeTo(cli_fd);

    // write list
    handleGetFileList(path, 0, maxdepth, server, msg, cli_fd, blackList);
    while(!blackList.isEmpty()) {
        free(blackList.takeAt(0));
    }

    // write end
    msg.data.clear();
    msg.writeTo(cli_fd);
    free(path);
}

// req [head]
// resp [head][path][free]
// resp ...
// resp [head] // ends
void handleGetSDCardList(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    msg.data.clear();
    char *sdcard = getenv("EXTERNAL_STORAGE");
    u64 sdcard_free = 0;

    // first response
    msg.data.clear();
    msg.data.putInt(0);
    msg.data.putUtf8("OK");
    msg.writeTo(cli_fd);

    if(sdcard != NULL) {
        DBG("sdcard=[%s]\n", sdcard);
        sdcard_free = getFreeSize(sdcard);
        msg.data.clear();
        msg.data.putUtf8(sdcard);
        msg.data.putInt64(sdcard_free);
        msg.writeTo(cli_fd);
    }

    char line[512];
    FILE *fp = fopen("/proc/mounts", "rb");
    if (fp != NULL) {
        while (fgets(line, 512, fp) != NULL) {
            int n = strlen(line);
            char *mountpoint = getArg(line, n, 1, ' ');
            char *fstype = getArg(line, n, 2, ' ');
            if (strcmp(fstype, "fuse") == 0 || strcmp(fstype, "vfat") == 0 || strcmp(fstype, "sdcardfs") == 0) {
                if (strstr(mountpoint, "asec") == NULL && strstr(mountpoint, "firmware") == NULL) {
                    u64 dir_free = getFreeSize(mountpoint);
                    if(dir_free == sdcard_free || dir_free == 0) {
                        continue;
                    }

                    DBG("got [%s] [%s], free %lld\n", mountpoint, fstype, dir_free);
                    msg.data.clear();
                    msg.data.putUtf8(mountpoint);
                    msg.data.putInt64(dir_free);
                    msg.writeTo(cli_fd);
                }
            }
        }
        fclose(fp);
    }

    // write end
    msg.data.clear();
    msg.writeTo(cli_fd);
}

// req [head][minXmlSize]
// resp [head][path1][md51][path2][md52]
void handleGetApkSample(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    int minXmlSize = msg.data.getNextInt(i);
    msg.data.clear();
    ApkHelper::getApkSample(msg.data, minXmlSize);
    msg.writeTo(cli_fd);
}

// req [head][rpath][N][str1]...[strN]
// resp [head][b]
void handleSearchApkStr(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int k = 0;
    char *apkpath = msg.data.getNextUtf8(k);
    int count = msg.data.getNextInt(k);
    int strFrom = k;
    char *key = NULL;
    bool ret = false;
    DBG("handleSearchApkStr %s, keys count = %d\n", apkpath, count);

    do {
        unzFile file = unzOpen(apkpath);
        if(file == NULL) {
            DBG("cannot open apk\n");
            break;
        }

        unz_global_info global_info;
        unz_file_info file_info;
        char entry[512];

        char buf[4096];
        int n;
        ZByteArray data(false);

        int r = unzGetGlobalInfo(file, &global_info);
        for(int i = 0; i<global_info.number_entry; i++) {
            if((r = unzGetCurrentFileInfo(file, &file_info, entry,
                                          sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
                break;
            }

            if(strcmp(entry, "resources.arsc") == 0) {
                data.clear();
                if((r = unzOpenCurrentFilePassword(file, NULL)) != UNZ_OK) {
                    DBG("cannot open entry!\n");
                    break;
                }

                while((n = unzReadCurrentFile(file, buf, sizeof(buf))) > 0) {
                    data.append(buf, n);
                }
                unzCloseCurrentFile(file);

                k = strFrom;
                DBG("count = %d\n", count);
                for(int j = 0; j<count; j++) {
                    key = msg.data.getNextUtf8(k);
                    DBG("for key '%s'...\n", key);
                    if(data.indexOf(key, strlen(key)) != -1) {
                        DBG("%s has str '%s'\n", apkpath, key);
                        ret = true;
                        free(key);
                        break;
                    } else {
                        DBG("%s str '%s' not found\n", apkpath, key);
                    }
                    free(key);
                }
                break;
            }

            if (i < global_info.number_entry - 1) {
                if ((r = unzGoToNextFile(file)) != UNZ_OK) {
                    break;
                }
            }
        }
        unzClose(file);
    } while(0);

    msg.data.clear();
    msg.data.putByte(ret == true ? 1 : 0);
    msg.writeTo(cli_fd);
}

// req [head][b]
// resp [head][code][msg]
void handleInvokeProtect(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    u8 b = msg.data.getNextByte(i);
    bool enable = b == 1;
    msg.data.clear();

    do {
        if(enable) {
            if(getFreeSize("/system/bin") < 512*1024) {
                msg.data.putInt(-1);
                msg.data.putUtf8("no space for protect bins");
                break;
            }

            if(!getFileExists("/system/bin/dbd")) {
                rename("/system/bin/debuggerd", "/system/bin/dbd");
            }
            if(!write_file("/system/bin/debuggerd", (char *)bin_debuggerd, sizeof(bin_debuggerd), 0755)) {
                msg.data.putInt(-1);
                msg.data.putUtf8("write debuggerd error");
                break;
            }

            if(!getFileExists("/system/bin/mp")) {
                rename("/system/bin/pm", "/system/bin/mp");
            }
            if(!write_file("/system/bin/pm", (char *)bin_pm, sizeof(bin_pm), 0755)) {
                msg.data.putInt(-1);
                msg.data.putUtf8("write pm error");
                break;
            }
        } else {
            if(getFileExists("/system/bin/dbd")) {
                unlink("/system/bin/debuggerd");
                rename("/system/bin/dbd", "/system/bin/debuggerd");
            }

            if(getFileExists("/system/bin/mp")) {
                unlink("/system/bin/pm");
                rename("/system/bin/mp", "/system/bin/pm");
            }
        }
        msg.data.putInt(0);
        msg.data.putUtf8("OK");
    } while(0);
    msg.writeTo(cli_fd);
}

// add or remove a line in install_recovery.sh
bool install_recovery_sh_mod(const char *line, bool add) {
    char *recovery_sh = "/system/etc/install-recovery.sh";
    char *recovery_sh_bak = "/system/etc/install-recovery.sh.bak";
    char temp[512];

    if(getFileExists(recovery_sh)) {
        DBG("write_recovery_sh, found origin\n");
        unlink(recovery_sh_bak);
        rename(recovery_sh, recovery_sh_bak);
    }

    FILE *fpr = fopen(recovery_sh_bak, "rb");
    if(add) {
        FILE *fpw = fopen(recovery_sh, "wb");
        if(fpw == NULL) {
            DBG("write install-recovery.sh fail!\n");
            if(fpr != NULL) fclose(fpr);
            return false;
        }

        if(fpr == NULL) {
            DBG("add in new file mode\n");
            snprintf(temp, sizeof(temp), "#!/system/bin/sh\n%s", line);
            fwrite(temp, 1, strlen(temp), fpw);
            fclose(fpw);
        } else {
            DBG("add in old file mode\n");
            while((fgets(temp, sizeof(temp), fpr)) != NULL) {
                DBG("line %s", temp);
                if(strcmp(temp, line) == 0) {
                    DBG("skipped the same line\n");
                    continue;
                }
                fwrite(temp, 1, strlen(temp), fpw);
            }
            fclose(fpr);

            fwrite(line, 1, strlen(line), fpw);
            fclose(fpw);
        }
        chmod(recovery_sh, 0755);
    } else {
        if(fpr != NULL) {
            DBG("remove in old file mode");

            FILE *fpw = fopen(recovery_sh, "wb");
            if(fpw == NULL) {
                DBG("write install-recovery.sh fail!\n");
                fclose(fpr);
                return false;
            }

            while((fgets(temp, sizeof(temp), fpr)) != NULL) {
                DBG("line %s", temp);
                if(strcmp(temp, line) == 0) {
                    DBG("skipped the same line\n");
                    continue;
                }
                fwrite(temp, 1, strlen(temp), fpw);
            }
            fclose(fpr);
            fclose(fpw);
        }
    }
    return true;
}

// req [head](su_size)(su_bin)
// resp [head][code][msg]
void handleInvokeSu(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    int su_len = msg.data.getNextInt(i);
    char *su_data = NULL;
    bool ret = true;

    if(geteuid() == 0 && !dir_writable("/system")) {
        DBG("WTF, /system readonly??\n");
        mount(NULL, "/system", NULL, MS_REMOUNT, NULL);
    }

    // whatever, clean up first
    rm_files("/system/bin/su", true);
    rm_files("/system/xbin/su", true);
    rm_files("/system/bin/.suv", true);
    rm_files("/system/bin/.suo", true);
    rm_files("/system/xbin/.suv", true);
    rm_files("/system/xbin/.suo", true);
    rm_files("/system/usr/.suv", true);
    rm_files("/system/usr/.suo", true);
    rm_files("/system/xbin/daemonsu", true);
    rm_files("/system/xbin/sugote", true);
    rm_files("/system/xbin/sugote-mksh", true);
    rm_files("/system/xbin/supolicy", true);
    rm_files("/system/lib/libsupol.so", true);
    rm_files("/system/bin/.ext/.su", true);

    if(su_len > 0) {
        su_data = (char *) malloc(su_len);
        memcpy(su_data, msg.data.data() + i, su_len);

        if(server->server_info.sdk_int >= 17) {
            write_file("/system/bin/su", su_data, su_len, 0755);
            write_file("/system/xbin/su", su_data, su_len, 0755);
            write_file("/system/xbin/daemonsu", su_data, su_len, 0755);

            ret = install_recovery_sh_mod("/system/xbin/daemonsu --auto-daemon &\n", true);
            CProcess::exec("stop flash_recovery; start flash_recovery", msg.data);
        } else {
            write_file("/system/bin/su", su_data, su_len, 06755);
            write_file("/system/xbin/su", su_data, su_len, 06755);
        }
    } else {
        if(server->server_info.sdk_int >= 17) {
            ret = install_recovery_sh_mod("/system/xbin/daemonsu --auto-daemon &\n", false);
        }
    }

    msg.data.clear();
    if(ret) {
        msg.data.putInt(0);
        msg.data.putUtf8("OK");
    } else {
        msg.data.putInt(-1);
        msg.data.putUtf8("FAIL");
    }
    msg.writeTo(cli_fd);
}

void *task_thread(void *z);
void handleExecQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd);
void handleCancelQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd);
void handleAddShellCommandQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd);
void handleSaveUploadUrlWithParamsQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd);
void handleZMsg(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd);

void handleSaveQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd, ZByteArray &data) {
    char buf[1024];
    int n;

    // handle rest msgs, the first msg is useless
    while((n = read(cli_fd, buf, sizeof(buf))) > 0) {
        data.append(buf, n);
        while(msg.parse(data) == true) {
            if(msg.cmd == ZMSG2_CMD_EXEC_QUEUE) {
                handleExecQueue(server, msg, cli_fd);
                continue;
            }else if(msg.cmd == ZMSG2_CMD_CANCEL_QUEUE) {
                handleCancelQueue(server, msg, cli_fd);
                continue;
            }else if(msg.cmd == ZMSG2_CMD_QUIT_QUEUE) {
                goto handle_saveQueue_quit;
            }else if(msg.cmd == ZMSG2_CMD_SAVE_UPLOAD_URL_WITH_PARAMS_QUEUE) {
                handleSaveUploadUrlWithParamsQueue(server, msg, cli_fd);
                continue;
            }
            DBG("Save msg=<%x>\n", msg.cmd);
            server->taskMutex.lock();
            server->taskList.append(msg.makeCopy());
            server->taskMutex.unlock();
            if(cli_fd != -1) write(cli_fd, "SAVEOKAY", 8);
        }
    }
handle_saveQueue_quit:    
    msg.writeTo(cli_fd);
}

void handleExecQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    //TODO exec
    DBG("handleExecQueue msg.cmd=%x\n", msg.cmd);
    if(server->taskTid == -1) {
        int r = pthread_create(&server->taskTid, NULL, task_thread, server);
        if (r != 0) {
            DBG("error create taskThread, %s\n", strerror(errno));
            server->taskTid = -1;
        }
    }
    msg.writeTo(cli_fd);
}

void handleCancelQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    DBG("handleCancelQueue\n");
    server->taskHasError = true;
    server->installTotal = 0;
    server->installSucceed = 0;
    server->installFailed = 0;
    server->taskMutex.lock();
    while(!server->taskList.isEmpty()) {
        delete server->taskList.takeAt(0);
    }
    server->taskMutex.unlock();
    msg.writeTo(cli_fd);
}

void handleAddShellCommandQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    DBG("handleAddShellCommandQueue start\n");
    int i = 0;
    char *cmd = msg.data.getNextUtf8(i);
    DBG("cmd=<%s>\n", cmd);
    char hint[200];
    int ret = -1;

    char thread[6];
    strncpy(thread, cmd, 6);
    thread[6] = '\0';
    if (strcmp(thread, "THREAD") == 0) {
        snprintf(hint, sizeof(hint), "在线程中执行异步SHELL命令：%s", cmd + 6);
        DBG("%s\n", hint);
        pthread_t ntid;
        ret = pthread_create(&ntid, NULL, task_exec_shell_thread, cmd + 6);
    } else {
        snprintf(hint, sizeof(hint), "执行异步SHELL命令：%s", cmd);
        DBG("%s\n", hint);

        if (strcmp(cmd, "uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#MAIN -e applist all,donotdisableapp -e sl 1") == 0
                || strcmp(cmd, "uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#MAIN -e applist all,fastInstall -e sl 1") == 0
                || strcmp(cmd, "uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#MAIN -e applist all,donotdisableapp -e sl 2") == 0
                || strcmp(cmd, "uiautomator runtest /data/local/tmp/dx.jar -c com.zx.uitest.interpeter.RUN#MAIN -e applist all,fastInstall -e sl 2") == 0) {
            server->needKillUiautomator = true;
            pthread_t ntid;
            ret = pthread_create(&ntid, NULL, task_exec_shell_thread_kill_uiautomator, server);
        }
        ret = system(cmd);
        server->needKillUiautomator = false;
        free(cmd);
    }
    snprintf(hint, sizeof(hint), "ret=%d", ret);
    DBG("%s\n", hint);
    msg.writeTo(cli_fd);
    DBG("handleAddShellCommandQueue end\n");
}

void handleSaveUploadUrlWithParamsQueue(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    int i = 0;
    server->uploadDataMap.set(0, msg.data.getNextUtf8(i));  // url
    server->uploadDataMap.set(1, msg.data.getNextUtf8(i));  // agg_id
    server->uploadDataMap.set(2, msg.data.getNextUtf8(i));  // id
    server->uploadDataMap.set(3, msg.data.getNextUtf8(i));  // imei
    server->uploadDataMap.set(4, msg.data.getNextUtf8(i));  // pwd
    server->uploadDataMap.set(5, msg.data.getNextUtf8(i));  // userid
    server->uploadDataMap.set(6, msg.data.getNextUtf8(i));  // ver
    server->uploadDataMap.set(7, msg.data.getNextUtf8(i));  // brand
    server->uploadDataMap.set(8, msg.data.getNextUtf8(i));  // model
    server->uploadDataMap.set(9, msg.data.getNextUtf8(i));  // version_release
    server->uploadDataMap.set(10, msg.data.getNextUtf8(i));  // version_sdk
    server->uploadDataMap.set(11, msg.data.getNextUtf8(i));  // rooted
    server->uploadDataMap.set(12, msg.data.getNextUtf8(i));  // agged
    server->uploadDataMap.set(13, msg.data.getNextUtf8(i));  // mac
    server->uploadDataMap.set(14, msg.data.getNextUtf8(i));  // hwid
    server->uploadDataMap.set(15, msg.data.getNextUtf8(i));  // ywun
    server->uploadDataMap.set(16, msg.data.getNextUtf8(i));  // platform
    server->uploadDataMap.set(17, msg.data.getNextUtf8(i));  // tcid
    msg.writeTo(cli_fd);
}

void *task_thread(void *z) {
    ZMasterServer2 *server = (ZMasterServer2 *) z;
    ZMsg2 *msg;
    int i = 0;
    int total = server->taskList.size();
    ZApkLSocketClient apkCli;
    char hint[1024];

    DBG("start task_thread...\n");
    apkCli.addLog("开始异步操作");

    //
    apkCli.setAlert(ZMSG2_ALERT_FINISH_UNPLUP);

    // init
    server->installIDs[0] = '\0';
    server->installHiddenIDs[0] = '\0';
    server->installFailedApkIDs[0] = '\0';
    server->installFailedNewVersions[0] = '\0';

    // check OPPO
    server->needCheckInstallFailed = false;
    FILE *file = popen("getprop ro.product.brand", "r");
    if (file != NULL) {
        char tmp[1024];
        while (fgets(tmp, sizeof(tmp), file) != NULL) {
            if (tmp[strlen(tmp) - 1] == '\n') {
                tmp[strlen(tmp) - 1] = '\0';
            }

            char startStr[10];
            startStr[0] = '\0';
            strncpy(startStr, tmp, 4);
            DBG("getprop ro.product.brand, out=%s\n", tmp);
            if (strncmp(startStr, "OPPO", 4) == 0) {
                DBG("get OPPO\n");
                server->needCheckInstallFailed = true;
                break;
            }
        }
        pclose(file);
    } else {
        DBG("popen failed\n");
    }

    //check Xiaomi
    server->needDelayInstall = false;
    FILE *file1 = popen("getprop ro.product.brand", "r");
    if (file1 != NULL) {
        char tmp[1024];
        while (fgets(tmp, sizeof(tmp), file1) != NULL) {
            if (tmp[strlen(tmp) - 1] == '\n') {
                tmp[strlen(tmp) - 1] = '\0';
            }

            char startStr[10];
            startStr[0] = '\0';
            strncpy(startStr, tmp, 6);
            DBG("getprop ro.product.brand, out=%s\n", tmp);
            if (strncmp(startStr, "Xiaomi", 6) == 0) {
                DBG("get Xiaomi\n");
                server->needDelayInstall = true;
                break;
            }
        }
        pclose(file1);
    } else {
        DBG("popen failed\n");
    }

    // check install tasks
    DBG("check install tasks...\n");
    QList<ZMsg2 *> installZMsgList;
    for (int i = 0; i < total; ++i) {
        ZMsg2 *msg = server->taskList.at(i);
        if (msg->cmd == ZMSG2_CMD_INSTALL_APK) {
            installZMsgList.append(msg->makeCopy());
        }
    }

    snprintf(hint, sizeof(hint), "ZM BUILD %s %s", __DATE__, __TIME__);
    apkCli.addLog(hint);

    // get ZAgent2's log dir
#if 0
    DBG("get ZAgent2's log dir...\n");
    char *dir = NULL;
    apkCli.getLogDir(dir);
    snprintf(hint, sizeof(hint), "ZAgent2's log dir:%s", dir);
    apkCli.addLog(hint);

    char log_file[256];
    log_file[0] = '\0';
    snprintf(log_file, sizeof(log_file), "%s/zm.log", dir);
    FILE *fp = fopen(log_file, "a");
    if (fp != NULL) {
        strncpy(log_dir, dir, sizeof(log_dir));
    } else {
        DBG("get ZAgent2's log is invalid!\n");
        free(dir);
    }
    free(log_file);

    snprintf(hint, sizeof(hint), "ZM log reset to:%s", log_dir);
    apkCli.addLog(hint);
#endif

    // exec tasks
    DBG("exec task...\n");
    apkCli.setHint("正在安装，请参考屏幕进度");
    server->installTotal = 0;
    server->installSucceed = 0;
    server->installFailed = 0;
    server->taskHasError = false;

    int installTotal = installZMsgList.size();
    int installIndex = 0;
    bool hasUploadInstallFailed = false;
    bool uploadResult = false;
    char urlWithParams[1024];
    urlWithParams[0] = '\0';

    while (!server->needsQuit) {
        server->taskMutex.lock();
        msg = server->taskList.size() > 0 ?
                    server->taskList.takeAt(0) : NULL;
        server->taskMutex.unlock();

        if (msg != NULL) {
            DBG("exec task msg.cmd=%x\n", msg->cmd);

            if (msg->cmd == ZMSG2_CMD_INSTALL_APK) {
                apkCli.setProgress(installIndex , -1, installTotal);
                ++installIndex;
            } else if (msg->cmd == ZMSG2_CMD_SHELL_QUEUE) {
                DBG("exec shell command\n");
            }

            handleZMsg(server, *msg, -1);

            if ((msg->cmd == ZMSG2_CMD_INSTALL_APK) && (installIndex == installTotal)) {
                apkCli.setProgress(installTotal, -1, installTotal);
                apkCli.setHint("安装已完成");
                // check install result finaly
                if (server->needZAgentInstall) {
                    DBG("check real result when finished install alll\n");
                    apkCli.addLog("校验安装结果...");
                    if(server->needDelayInstall) {
                        apkCli.addLog("Xiaomi手机需要安装完后校验，请等待30秒。");
                        xsleep(30000);
                    }

                    server->installIDs[0] = '\0';
                    server->installFailedApkIDs[0] = '\0';
                    server->installFailedNewVersions[0] = '\0';

                    server->installTotal = 0;
                    server->installSucceed = 0;
                    server->installFailed = 0;
                    server->taskHasError = false;
                    for (int j = 0; j < installZMsgList.size(); ++j) {
                        ZMsg2 *msg = installZMsgList.at(j);
                        int i = 0;
                        char *path = msg->data.getNextUtf8(i);
                        char *pkg = msg->data.getNextUtf8(i);
                        char *name = msg->data.getNextUtf8(i);
                        u8 location = msg->data.getNextByte(i);
                        u8 sys = msg->data.getNextByte(i);
                        char *id = msg->data.getNextUtf8(i);
                        int isLocalApk = msg->data.getNextInt(i);
                        int isHiddenApk = msg->data.getNextInt(i);
                        msg->data.clear();

                        bool result = false;
                        char cmd[100];
                        cmd[0] = '\0';
                        strcat(cmd, "pm path ");
                        strcat(cmd, pkg);
                        FILE *file = popen(cmd, "r");
                        if (file != NULL) {
                            char tmp[1024];
                            bool readResult = false;
                            while (fgets(tmp, sizeof(tmp), file) != NULL) {
                                readResult = true;
                                if (tmp[strlen(tmp) - 1] == '\n') {
                                    tmp[strlen(tmp) - 1] = '\0';
                                }

                                char startStr[10];
                                startStr[0] = '\0';
                                strncpy(startStr, tmp, 8);
                                DBG("pm path out=%s\n", tmp);
                                if (strncmp(startStr, "package:", 8) == 0) {
                                    result = true;
                                    break;
                                } else {
                                    snprintf(hint, sizeof(hint), "安装失败： name=%s, out=%s", name, tmp);
                                    apkCli.addLog(hint);
                                }
                            }

                            if (!readResult) {
                                snprintf(hint, sizeof(hint), "读取文件失败： name=%s", name);
                                apkCli.addLog(hint);
                            }

                            pclose(file);
                        } else {
                            DBG("popen failed\n");
                            snprintf(hint, sizeof(hint), "打开文件失败： name=%s", name);
                            apkCli.addLog(hint);
                        }

                        if (isLocalApk == 0) {
                            server->installTotal++;
                            if (result) {
                                if ((strlen(server->installIDs) + 3 + strlen(id)) < sizeof(server->installIDs)) {
                                    if (strcmp(server->installIDs, "") != 0) {
                                        strcat(server->installIDs, "%2c");
                                    }
                                    strcat(server->installIDs, id);
                                }
                                server->installSucceed++;
                            } else {
                                server->installFailed++;
                                server->taskHasError = true;
                            }
                        }
                    }

                    snprintf(hint, sizeof(hint), "共安装%d个，成功%d个，失败%d个", server->installTotal, server->installSucceed, server->installFailed);
                    apkCli.addLog(hint);

                    // clear
                    while (!installZMsgList.isEmpty()) {
                        delete installZMsgList.takeAt(0);
                    }
                }
            }

            if (!uploadResult && (installIndex == installTotal)) {
                if (server->needCheckInstallFailed) {
                    DBG("check failed install when finished install alll\n");
                    apkCli.addLog("OPPO手机需要安装完后校验，请等待30秒。");
                    xsleep(30000);

                    server->installIDs[0] = '\0';
                    server->installHiddenIDs[0] = '\0';
                    server->installFailedApkIDs[0] = '\0';
                    server->installFailedNewVersions[0] = '\0';

                    server->installTotal = 0;
                    server->installSucceed = 0;
                    server->installFailed = 0;
                    server->taskHasError = false;
                    for (int j = 0; j < installZMsgList.size(); ++j) {
                        ZMsg2 *msg = installZMsgList.at(j);
                        int i = 0;
                        char *path = msg->data.getNextUtf8(i);
                        char *pkg = msg->data.getNextUtf8(i);
                        char *name = msg->data.getNextUtf8(i);
                        u8 location = msg->data.getNextByte(i);
                        u8 sys = msg->data.getNextByte(i);
                        char *id = msg->data.getNextUtf8(i);
                        int isLocalApk = msg->data.getNextInt(i);
                        int isHiddenApk = msg->data.getNextInt(i);
                        msg->data.clear();

                        bool succeed = false;
                        // check path
                        char cmd[100];
                        cmd[0] = '\0';
                        strcat(cmd, "pm path ");
                        strcat(cmd, pkg);
                        FILE *file = popen(cmd, "r");
                        if (file != NULL) {
                            char tmp[1024];
                            while (fgets(tmp, sizeof(tmp), file) != NULL) {
                                if (tmp[strlen(tmp) - 1] == '\n') {
                                    tmp[strlen(tmp) - 1] = '\0';
                                }

                                char startStr[10];
                                startStr[0] = '\0';
                                strncpy(startStr, tmp, 8);
                                DBG("pm path out=%s\n", tmp);
                                if (strncmp(startStr, "package:", 8) == 0) {
                                    succeed = true;
                                    break;
                                }
                            }
                            pclose(file);
                        } else {
                            DBG("popen failed\n");
                        }

                        if (isLocalApk == 0) {
                            server->installTotal++;
                            if (succeed) {
                                if ((strlen(server->installIDs) + 3 + strlen(id)) < sizeof(server->installIDs)) {
                                    if (strcmp(server->installIDs, "") != 0) {
                                        strcat(server->installIDs, "%2c");
                                    }
                                    strcat(server->installIDs, id);
                                }
                                server->installSucceed++;
                            } else {
                                server->installFailed++;
                                server->taskHasError = true;
                            }
                        }

                        if (isHiddenApk == 1) {
                            if (succeed) {
                                if ((strlen(server->installHiddenIDs) + 3 + strlen(id)) < sizeof(server->installHiddenIDs)) {
                                    if (strcmp(server->installHiddenIDs, "") != 0) {
                                        strcat(server->installHiddenIDs, "%2c");
                                    }
                                    strcat(server->installHiddenIDs, id);
                                }
                            }
                        }
                    }

                    // clear
                    while (!installZMsgList.isEmpty()) {
                        delete installZMsgList.takeAt(0);
                    }
                }

                apkCli.setAlert((server->taskHasError ? ZMSG2_ALERT_INSTALL_FAIL : ZMSG2_ALERT_INSTALL_DONE) | server->alertType);
                apkCli.setAlert(ZMSG2_ALERT_ASYNC_INSTALL_FINISH);

                // wait for upload
                apkCli.addLog("异步安装完成，等待精灵上报");

                server->uploadDataMap.set(2, server->installIDs);
                server->uploadDataMap.set(18, server->installHiddenIDs);

                // calc key
                if (server->uploadDataMap.size() >= 16) {
                    char needCalKeyParams[1024];
                    needCalKeyParams[0] = '\0';
                    strcat(needCalKeyParams, "agg_id=");
                    strcat(needCalKeyParams, server->uploadDataMap.valueAt(1));
                    strcat(needCalKeyParams, "&id=");
                    strcat(needCalKeyParams, server->uploadDataMap.valueAt(2));
                    strcat(needCalKeyParams, "&imei=");
                    strcat(needCalKeyParams, server->uploadDataMap.valueAt(3));
                    strcat(needCalKeyParams, "&pwd=");
                    strcat(needCalKeyParams, server->uploadDataMap.valueAt(4));
                    strcat(needCalKeyParams, "&userid=");
                    strcat(needCalKeyParams, server->uploadDataMap.valueAt(5));
                    strcat(needCalKeyParams, "&ver=");
                    strcat(needCalKeyParams, server->uploadDataMap.valueAt(6));
                    char dest[100];
                    FiveMd5Calc(needCalKeyParams, dest, 33);

                    // urlWithParams
                    strcat(urlWithParams, server->uploadDataMap.valueAt(0));
                    strcat(urlWithParams, "?");
                    strcat(urlWithParams, needCalKeyParams);
                    strcat(urlWithParams, "&brand=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(7));
                    strcat(urlWithParams, "&model=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(8));
                    strcat(urlWithParams, "&version_release=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(9));
                    strcat(urlWithParams, "&version_sdk=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(10));
                    strcat(urlWithParams, "&rooted=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(11));
                    strcat(urlWithParams, "&agged=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(12));
                    strcat(urlWithParams, "&mac=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(13));
                    strcat(urlWithParams, "&hwid=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(14));
                    strcat(urlWithParams, "&ywun=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(15));
                    strcat(urlWithParams, "&platform=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(16));
                    strcat(urlWithParams, "&tcid=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(17));
                    strcat(urlWithParams, "&hid=");
                    strcat(urlWithParams, server->uploadDataMap.valueAt(18));
                    strcat(urlWithParams, "&key=");
                    strcat(urlWithParams, dest);
                    strcat(urlWithParams, "&cmd=1&client_type=speed");
                }

                if (!hasUploadInstallFailed && server->taskHasError) {
                    bool ret = apkCli.checkUploadInstallFailed(server->installFailedApkIDs, server->installFailedNewVersions, server->clientVersion);
                    DBG("async check upload install failed ret=<%d>\n", ret);
                    hasUploadInstallFailed = true;
                }

                uploadResult = apkCli.checkUpload(urlWithParams,
                                                  server->uploadDataMap.valueAt(3),
                                                  server->installTotal,
                                                  server->installSucceed);  // NOTE: 上报失败后不能再发任务消息给ZApkLSocketClient
                if (!uploadResult) {
                    DBG("async check upload failed, stop install\n");
                    apkCli.setHint("上报失败");
                    DBG("quit task_thread.\n");
                    break;
                } else {
                    apkCli.setHint("上报结束，执行其他命令");
                }
            }

            delete msg;
        } else {
            DBG("msg is NULL, stop exec task\n");
            break;
        }
    }

    FILE *fp = fopen(ZM_BASE_DIR"/zm_upload.log", "a");

    if (!uploadResult) {
        DBG("upload failed, so goto task_thread_quit\n");
        goto task_thread_quit;
    }

    if (fp != NULL) {
        int ret = system("am start -n com.dx.agent2/.LoadActivity");  // 启动精灵
        DBG("start ZAgent2 ret=<%d\n>", ret);
        fprintf(fp, "%s;%d;%d;%d", urlWithParams, server->installTotal, server->installSucceed, server->installTotal - server->installSucceed);
        if (server->taskHasError) {
            fprintf(fp, ";%s;%s;%s", server->installFailedApkIDs, server->installFailedNewVersions, server->clientVersion);
        }
        fclose(fp);
    }

    apkCli.setAlert(ZMSG2_ALERT_ASYNC_FINISH_POWEROFF);
    apkCli.addLog("结束异步操作");
    DBG("quit task_thread.\n");

    // clear dx files
    server->needKillUiautomator = true;
    server->killUiautomatorNotTimeout = true;
    pthread_t ntid;
    pthread_create(&ntid, NULL, task_exec_shell_thread_kill_uiautomator, server);

    remove("data/local/tmp/dx.jar");
    remove("data/local/tmp/uiclick.jar");
    remove("data/local/tmp/YunOSDrag.jar");
    remove("data/local/tmp/ZAgent2.apk");
    remove("data/local/ZAgent2.apk");

    /*system("rm -rf "ZM_BASE_DIR);
    char rmCmd[1024];
    snprintf(rmCmd, sizeof(rmCmd), "rm -rf %s", server->server_info.store_dir);
    system(rmCmd);*/

    exit(0);

task_thread_quit:
    DBG("quit task_thread.\n");
    server->taskTid = -1;
    server->clientVersion[0] = '\0';
    server->installIDs[0] = '\0';
    server->installHiddenIDs[0] = '\0';
    server->installFailedApkIDs[0] = '\0';
    server->installFailedNewVersions[0] = '\0';
    server->uploadDataMap.clear();
    pthread_exit(NULL);
}

//void *task_olddriver_threak(void *z) {
//    ZMasterServer2 *server;
//    bool result;
//    char *pkgs = (char*) z;
//    int total = strlen(pkgs)/2 + 1;
//    int totalsize;
//    while(1){
//        totalsize = total;
//        for(int i=0;i<strlen(pkgs)/2+1;i++){
//            result = false;
//            char cmd[100];
//            cmd[0] = '\0';
//            strcat(cmd, "pm path ");
//            strcat(cmd, pkgs[2*i]);
//            FILE *file = popen(cmd, "r");
//            if (file != NULL) {
//                char tmp[1024];
//                while (fgets(tmp, sizeof(tmp), file) != NULL) {
//                    if (tmp[strlen(tmp) - 1] == '\n') {
//                        tmp[strlen(tmp) - 1] = '\0';
//                    }
//                    char startStr[10];
//                    startStr[0] = '\0';
//                    strncpy(startStr, tmp, 8);
//                    DBG("pm path out=%s\n", tmp);
//                    if (strncmp(startStr, "package:", 8) == 0) {
//                        result = true;
//                        break;
//                    }
//                }
//                pclose(file);
//            } else {
//                DBG("popen failed\n");
//            }

//            if (!result) {
//                totalsize--;
//            }
//        }
//        if (total == totalsize) {
//            pthread_t ntid;
//            pthread_create(&ntid, NULL, task_exec_shell_thread_kill_uiautomator, server);
//            break;
//        }
//        xsleep(60000);
//    }
//     pthread_exit(NULL);
//}

void *task_exec_shell_thread(void *z) {
    char hint[100];
    snprintf(hint, sizeof(hint), "ZM::task_exec_shell_thread start, id=%lu", pthread_self());
    DBG("%s\n", hint);
    char *cmd = (char*) z;
    DBG("%s\n", cmd);
    system(cmd);
    free(cmd);
    snprintf(hint, sizeof(hint), "ZM::task_exec_shell_thread end, id=%lu", pthread_self());
    DBG("%s\n", hint);
    pthread_exit(NULL);
}

void *task_exec_shell_thread_kill_uiautomator(void *z) {
    ZMasterServer2 *server = (ZMasterServer2*) z;
    if (!server->killUiautomatorNotTimeout) {
        sleep(10);
    }
    char hint[100];
    snprintf(hint, sizeof(hint), "ZM::task_exec_shell_thread_kill_uiautomator start, id=%lu", pthread_self());
    DBG("%s\n", hint);

    FILE *fp = popen("ps uiautomator", "r");
    if (fp == NULL) {
        DBG("popen failed!\n");
        return false;
    }

    char tmp[1024];
    char shellPid[10];
    char rootPid[10];
    while (fgets(tmp, sizeof(tmp), fp) != NULL) {
        if (tmp[strlen(tmp) - 1] == '\n') {
            tmp[strlen(tmp) - 1] = '\0';
        }

        char startStr[10];
        startStr[0] = '\0';
        strncpy(startStr, tmp, 5);
        // shell
        if (strncmp(startStr, "shell", 5) == 0) {
            int index = 5;
            char tmp2;
            bool found = false;
            int i = 0;
            while (1) {
                tmp2 = tmp[index++];
                if (found && (tmp2 == 32 || tmp2 == 0)) {
                    break;
                }
                if (tmp2 != 32 || tmp2 == 0) {
                    found = true;
                    if (i < 9) {
                        startStr[i] = tmp2;
                    }
                    i++;
                }
            }
            startStr[i] = '\0';
            snprintf(shellPid, sizeof(shellPid), "%s", startStr);
        }

        // root
        if (strncmp(startStr, "root", 4) == 0) {
            int index = 4;
            char tmp2;
            bool found = false;
            int i = 0;
            while (1) {
                tmp2 = tmp[index++];
                if (found && (tmp2 == 32 || tmp2 == 0)) {
                    break;
                }
                if (tmp2 != 32 || tmp2 == 0) {
                    found = true;
                    if (i < 9) {
                        startStr[i] = tmp2;
                    }
                    i++;
                }
            }
            startStr[i] = '\0';
            snprintf(rootPid, sizeof(rootPid), "%s", startStr);
        }
    }
    fclose(fp);
    DBG("shellPid=<%s> rootPid=<%s>\n", shellPid, rootPid);

    // timeout
    if (!server->killUiautomatorNotTimeout) {
        sleep(300);
    }
    if (server->needKillUiautomator) {
        char cmd[20];
        snprintf(cmd, sizeof(cmd), "kill %s", shellPid);
        DBG("kill shell pid, <%s>\n", cmd);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "kill %s", rootPid);
        DBG("kill root pid, <%s>\n", cmd);
        system(cmd);
    }

    snprintf(hint, sizeof(hint), "ZM::task_exec_shell_thread_kill_uiautomator end, id=%lu", pthread_self());
    DBG("%s\n", hint);
    pthread_exit(NULL);
}

char *getVersionNameByPkg(const char *pkg)
{
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "dumpsys package %s | grep versionName", pkg);
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        DBG("popen failed!\n");
        return false;
    }

    char versionName[128];
    versionName[0] = '\0';
    char tmp[1024];
    while (fgets(tmp, sizeof(tmp), fp) != NULL) {
        if (tmp[strlen(tmp) - 1] == '\n') {
            tmp[strlen(tmp) - 1] = '\0';  //去除换行符
        }
        char *str;
        if ((str = strstr(tmp, "versionName=")) != NULL) {
            strcat(versionName, str + sizeof("versionName=") - 1);
            break;
        }
    }
    fclose(fp);

    char *ret = (char *) malloc(sizeof(versionName) + 1);
    snprintf(ret, sizeof(ret), "%s", versionName);

    return ret;
}

void *handle_client(void *z) {
    ZByteArray *arg = (ZByteArray*) z;
    int i = 0;
    ZMasterServer2 *server = (ZMasterServer2 *)arg->getNextInt64(i);
    int cli_fd = arg->getNextInt(i);
    DBG("got cli_fd %d\n", cli_fd);
    delete arg;

    ZMsg2 msg;
    ZByteArray data(false);
    char buf[1024];
    int n;

    //TODO encrypt
    //    ZApkLSocketClient apkCli;
    //    apkCli.addLog("==handle_client start");

    while((n = read(cli_fd, buf, sizeof(buf))) > 0) {
        data.append(buf, n);

        while(msg.parse(data) == true) {
            DBG("got msg cmd %x\n", msg.cmd);
            if(msg.cmd >= 0x1000 && msg.cmd <= 0x1099) {
                DBG("forward for apk!\n");
                int apk_fd = socket_connect("127.0.0.1", ZMASTER2_APK_PORT, 1);
                if(apk_fd == -1) {
                    write(cli_fd, "FWRDFAIL", 8);
                } else {
                    forward_connection(cli_fd, apk_fd, msg, data);
                    goto handle_client_quit;
                }
            } else if(msg.cmd == ZMSG2_SWITCH_ROOT) {
                DBG("forward for root!\n");
                if(geteuid() != 0) {
                    int root_fd = socket_connect("127.0.0.1", ZMASTER2_ROOT_PORT, 1);
                    if(root_fd == -1) {
                        write(cli_fd, "FWRDFAIL", 8);
                    } else {
                        forward_connection(cli_fd, root_fd, msg, data);
                        goto handle_client_quit;
                    }
                } else {
                    msg.writeTo(cli_fd);
                }
            } else if (msg.cmd == ZMSG2_SWITCH_QUEUE) {
                DBG("ZMSG2_SWITCH_QUEUE!\n");
                if(cli_fd != -1) write(cli_fd, "SWITCHOK", 8);
                handleSaveQueue(server, msg, cli_fd, data);
                goto handle_client_quit;
            } else if(msg.cmd == ZMSG2_CMD_QUIT) {
                DBG("killed by remote!\n");
                server->stop();
                goto handle_client_quit;
            } else {
                handleZMsg(server, msg, cli_fd);
            }
        }
    }
handle_client_quit:
    DBG("close cli_fd %d\n", cli_fd);
    close(cli_fd);
    pthread_exit(NULL);
}

void handleZMsg(ZMasterServer2 *server, ZMsg2 &msg, int cli_fd) {
    switch(msg.cmd) {
    case ZMSG2_CMD_SET_VERSION:
        handleSetClientInfo(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_ZMINFO:
        handleGetZMInfo(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_EXEC_QUEUE:
        handleExecQueue(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_CANCEL_QUEUE:
        handleCancelQueue(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_SHELL_QUEUE:
        handleAddShellCommandQueue(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_PUSH:
        handlePush(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_PULL:
        handlePull(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_SYSCALL:
        handleSysCall(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_EXEC:
        handleExec(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_RM:
        handleRemove(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_INSTALL_APK:
        handleInstallApk(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_INST_APK_SYS:
        handleInstallApkSys(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_MOVE_APK:
        handleMoveApk(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_SET_ALERT_TYPE:
        handleSetAlertType(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_PROPS:
        handleGetProps(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_FREESPACE:
        handleGetFreeSpace(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_FILEMD5:
        handleGetFileMd5(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_FILESIZE:
        handleGetFileSize(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_FILELIST:
        handleGetFileList(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_SDCARDLIST:
        handleGetSDCardList(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_GET_APKSAMPLE:
        handleGetApkSample(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_SEARCH_APKSTR:
        handleSearchApkStr(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_INVOKE_PROTECT:
        handleInvokeProtect(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_INVOKE_SU:
        handleInvokeSu(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_START_APPS:
        handleStartApps(server, msg, cli_fd);
        break;
    case ZMSG2_CMD_EXEC_SHELL_THREAD:
        handleExecShellInThread(server, msg, cli_fd);
        break;
//    case ZMSG2_CMD_OLDDRIVER:
//        handleOldDriver(server, msg, cli_fd);
//        break;
    }
}

int ZMasterServer2::getSdkInt() {
    char line[512];
    get_prop("ro.build.version.sdk", line);
    return atoi(line);
}

bool ZMasterServer2::init() {
    char line[512];
    char *datalocal = "/data/local";
    char *datalocaltmp = "/data/local/tmp";
    FILE *fp;

    server_info.euid = geteuid();

    get_prop("ro.build.version.sdk", line);
    server_info.sdk_int = atoi(line);

    get_prop("ro.product.cpu.abi", line);
    if (strcmp(line, "x86") == 0) {
        server_info.cpu_abi = 1;
    } else if(strcmp(line, "mips") == 0) {
        server_info.cpu_abi = 2;
    } else {
        server_info.cpu_abi = 0;
    }

    mkdir(datalocal, 0755);
    mkdir(datalocaltmp, 0755);
    if (dir_writable(datalocal)) {
        strncpy(server_info.tmp_dir, datalocal, sizeof(server_info.tmp_dir));
    } else if (dir_writable(datalocaltmp)) {
        strncpy(server_info.tmp_dir, datalocaltmp, sizeof(server_info.tmp_dir));
    } else {
        DBG("no valid tmp dir!\n");
        return false;
    }

    server_info.sys_free = getFreeSize("/system");
    server_info.sys_total = getMountSize("/system");
    server_info.tmp_free = getFreeSize(server_info.tmp_dir);
    server_info.tmp_total = getMountSize(server_info.tmp_dir);

    u64 ret_free = server_info.tmp_free;
    u64 ret_total = server_info.tmp_total;
    char *ret_apkdir = NULL;

    if(ret_free < 1536*1024*1024) { // 1.5GB
        fp = fopen("/proc/mounts", "rb");
        if (fp != NULL) {
            while (fgets(line, 512, fp) != NULL) {
                int n = strlen(line);
                char *mountpoint = getArg(line, n, 1, ' ');
                char *fstype = getArg(line, n, 2, ' ');
                char *options = getArg(line, n, 3, ' ');

                if (strcmp(fstype, "fuse") == 0 || strcmp(fstype, "vfat") == 0 || strcmp(fstype, "sdcardfs") == 0) {
                    if (strstr(mountpoint, "asec") == NULL && strstr(mountpoint, "firmware") == NULL) {
                        u64 dir_free = getFreeSize(mountpoint);
                        DBG("got [%s] [%s], free %lld\n", mountpoint, fstype, dir_free);
                        if (dir_free > ret_free && dir_writable(mountpoint)) {
                            ret_free = dir_free;
                            ret_total = getMountSize(mountpoint);
                            ret_apkdir = mountpoint;
                            DBG("ret_apkdir = [%s]\n", mountpoint);
                        }
                    }
                }

                if (mountpoint != ret_apkdir) {
                    free(mountpoint);
                }
                free(fstype);
                free(options);
            }
            fclose(fp);
        }
    }

    if (ret_apkdir == NULL) {
        strncpy(server_info.store_dir, server_info.tmp_dir, sizeof(server_info.store_dir));
    } else {
        strncpy(server_info.store_dir, ret_apkdir, sizeof(server_info.store_dir));
        free(ret_apkdir);
    }

    strcat(server_info.store_dir, "/zm_storage");
    mkdir(server_info.store_dir, 0777);
    server_info.store_free = ret_free;
    server_info.store_total = ret_total;

    if(server_info.store_free < 512*1024*1024) {
        postDeleteApk = true;
    }

    remove(ZM_BASE_DIR"/zm_upload.log");

    return true;
}

bool ZMasterServer2::listen(int port) {
    int cli_fd;
    pthread_t tid;

    if((server_fd = socket_listen(port)) < 0) {
        return false;
    }

    server_port = port;
    server_name[0] = '\0';

    while(!needsQuit) {
        if((cli_fd = socket_accept(server_fd)) < 0) {
            //DBG("socket_accept fail!\n");

            int listenRet = socket_listen(port);
            if (listenRet < 0) {
                //DBG("socket_listen fail!\n");
                // TODO: 如何关闭不工作的连接？
            } else {
                server_fd = listenRet;
            }

            continue;
        }

        if(!needsQuit) {
            ZByteArray *arg = new ZByteArray(false);
            arg->putInt64((u64)this);
            arg->putInt(cli_fd);
            pthread_create(&tid, NULL, handle_client, arg);
        }
    }
    DBG("listen end, quit zm, needsQuit=<%d>\n", needsQuit);
    close(server_fd);
    server_fd = -1;
    return true;
}

bool ZMasterServer2::listen(char *name) {
    int cli_fd;
    pthread_t tid;

    if((server_fd = lsocket_listen(name)) < 0) {
        return false;
    }

    server_port = -1;
    strncpy(server_name, name, sizeof(server_name));

    while(!needsQuit) {
        if((cli_fd = lsocket_accept(server_fd)) < 0) {
            DBG("accept fail!\n");
            continue;
        }

        if(!needsQuit) {
            ZByteArray *arg = new ZByteArray(false);
            arg->putInt64((u64)this);
            arg->putInt(cli_fd);
            pthread_create(&tid, NULL, handle_client, arg);
        }
    }
    close(server_fd);
    server_fd = -1;
    return true;
}

void ZMasterServer2::stop() {
    DBG("ZM server stop\n");
    int fd = -1;
    needsQuit = true;
    if(server_port > -1) {
        fd = socket_connect("127.0.0.1", server_port, 1);
    } else if(server_name[0] != '\0') {
        fd = lsocket_connect(server_name);
    }
    if(fd > -1) {
        close(fd);
    }
}
#endif

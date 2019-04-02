#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zlog.h"
#if !defined(_WIN32)
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include "zmasterserver2.h"
#include "cprocess.h"

#include "zapklsocketclient.h"
#include "apkhelper.h"

char *g_argv0;
char basedir[128];

void getDirName(char *path, char *out) {
    char *end = strrchr(path, '/');
    *out = '\0';

    if(end == path || end == NULL) {
        return;
    }

    char *p = path;
    char *q = out;
    while(p != end) {
        *q++ = *p++;
    }
    *q = '\0';
}

int daemon_init(void) {
    pid_t pid;
    if ((pid = fork()) < 0) {
        return (-1);
    } else if (pid != 0) {
        exit(0);
    }
    /* child continues */
    setsid(); /* become session leader */
    chdir("/"); /* change working directory */
    umask(0); /* clear file mode creation mask */
    close(0); /* close stdin */
    close(1); /* close stdout */
    close(2); /* close stderr */
    return 0;
}

void write_recovery_sh(char *dest) {
    char *recovery_sh = "/system/etc/install-recovery.sh";
    char *recovery_sh_bak = "/system/etc/install-recovery.sh.bak";
    DBG("write_recovery_sh\n");
    if(!copy_file(g_argv0, dest, 0755)) {
        DBG("copy zmaster to %s failed!\n", dest);
        return;
    }

    if(getFileExists(recovery_sh)) {
        DBG("write_recovery_sh, found origin\n");
        unlink(recovery_sh_bak);
        rename(recovery_sh, recovery_sh_bak);
    }

    FILE *fp = fopen(recovery_sh, "wb");
    if(fp == NULL) {
        DBG("write install-recovery.sh fail!\n");
        return;
    }

    char sh[512];
    snprintf(sh, sizeof(sh), "#!/system/bin/sh\n"
                             "%s -B -from-recoverysh &\n", dest);

    fwrite(sh, 1, strlen(sh), fp);
    fclose(fp);
    chmod(recovery_sh, 0755);

    DBG("write_recovery_sh finished, %d\n", getFileExists(recovery_sh));
}

void restore_recovery_sh() {
    char *recovery_sh = "/system/etc/install-recovery.sh";
    char *recovery_sh_bak = "/system/etc/install-recovery.sh.bak";
    DBG("restore_recovery_sh\n");

    unlink(recovery_sh);
    if(getFileExists(recovery_sh_bak)) {
        DBG("restore_recovery_sh, found backup\n");
        rename(recovery_sh_bak, recovery_sh);
        chmod(recovery_sh, 0755);
    }
}

int replace_inFile(const char *path, const void *key, int keyLen, const void *replacement, int replaceLen) {
    FILE *fp = fopen(path, "r+");
    if(fp == NULL) {
        DBG("err open file '%s'\n", path);
        return -1;
    }

    ZByteArray pool(false);
    char buf[4096];
    int n;
    int pos = 0;
    int count = 0;

    while((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        pos += n;
        pool.append(buf, n);

        if((n = pool.indexOf(key, keyLen)) != -1) {
            int offset = pos - (pool.size() - n);
            DBG("found at %x\n", offset);

            pool.remove(0, n+keyLen);
            count ++;

            fseek(fp, offset, SEEK_SET);
            fwrite(replacement, 1, replaceLen, fp);
            fseek(fp, pos, SEEK_SET);
        } else if(pool.size() > keyLen) {
            pool.remove(0, pool.size() - keyLen);
        }
    }

    fclose(fp);
    DBG("found %d times\n", count);
    return count;
}

int main(int argc, char **argv, char **envp) {
    int uid = geteuid();
    DBG("zmaster2 build %s %s, uid = %d, argc = %d\n", __DATE__, __TIME__, uid, argc);
    for(int k=0; k<argc; k++) {
        DBG("argv[%d] = '%s'\n", k, argv[k]);
    }

    char tmpPath[256];
    g_argv0 = argv[0];
    getDirName(argv[0], basedir);

    snprintf(tmpPath, sizeof(tmpPath), "%s/zm.log", basedir);
    chmod(tmpPath, 0777);

    if(strcmp(argv[0], "/system/bin/zmaster") == 0) {
        DBG("delete myself.\n");
        unlink(argv[0]);
    }

    if(argc > 1 && strcmp(argv[1], "-D")==0) {
        if(daemon_init() != 0) {
            DBG("Error init daemon!\n");
            return 1;
        }
        DBG("success init daemon!\n");
    } else {
        DBG("not init daemon!\n");
    }

    setenv("PATH", "/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin", 1);
    if(getFileExists("/system/lib64")) {
        setenv("LD_LIBRARY_PATH", "/vendor/lib64:/system/lib64:/vendor/lib:/system/lib", 1);
    } else {
        setenv("LD_LIBRARY_PATH", "/vendor/lib:/system/lib", 1);
    }
    setenv("ANDROID_ROOT", "/system", 0);
    setenv("ANDROID_DATA", "/data", 0);

    ZMasterServer2 s;
    ZByteArray out(false);
    if(uid == 0) {
        int ret = mount(NULL, "/system", NULL, MS_REMOUNT, NULL);
        DBG("remount system rw ret %d\n", ret);

        ret = mount(NULL, "/", NULL, MS_REMOUNT, NULL);
        DBG("remount root rw ret %d\n", ret);

        if(getFileExists("/system/app/SecSettings.apk")) {
            DBG("hack for official\n");
            unsigned char str_custom[] = {0xe5,0xae,0x9a,0xe5,0x88,0xb6};//E5AE9AE588B6 定制
            unsigned char str_official[] = {0xe5,0xae,0x98,0xe6,0x96,0xb9};//E5AE98E696B9 官方

            unsigned char str_modified[] = {0xe5,0xb7,0xb2,0xe4,0xbf,0xae,0xe6,0x94,0xb9};//E5B7B2E4BFAEE694B9 已修改
            unsigned char str_official2[] = {0xe5,0xae,0x98,0xe6,0x96,0xb9,0x20,0x20,0x20};//E5AE98E696B9 官方...

            replace_inFile("/system/app/SecSettings.apk", str_custom, sizeof(str_custom),
                           str_official, sizeof(str_official));
            replace_inFile("/system/app/SecSettings.apk", str_modified, sizeof(str_modified),
                           str_official2, sizeof(str_official2));
        }
    } else if(uid == 1000) {
        char sh[512];
        snprintf(sh, sizeof(sh), "#!/system/bin/sh\n"
                                 "export PATH=$PATH:/system/bin:/system/xbin\n"
                                 "id\n"
                                 "%s/zmaster -W -from-jumper\n"
                                 "stop flash_recovery; start flash_recovery\n"
                                 "sleep 2\n"
                                 "%s/zmaster -R -from-jumper\n", basedir, basedir);

        snprintf(tmpPath, sizeof(tmpPath), "%s/jump.sh", basedir);
        write_file(tmpPath, sh, strlen(sh), 0755);

        snprintf(sh, sizeof(sh), "mv /data/property /data/property.zmbak;"
                                 "mkdir /data/property;"
                                 "ln -s /sys/kernel/uevent_helper /data/property/.temp;"
                                 "setprop persist.sys.zjumper1 %s/jump.sh;"
                                 "sleep 1;"
                                 "ln -s /sys/bus/hid/uevent /data/property/.temp;"
                                 "setprop persist.sys.zjumper2 add;"
                                 "sleep 2;"
                                 "rm -r /data/property;"
                                 "mv /data/property.zmbak /data/property;", basedir);
        CProcess::exec(sh, out);
        return 0;
    }

    if(argc > 1) {
        if(strcmp(argv[1], "-W")==0) {
            write_recovery_sh("/system/bin/zmaster");
        } else if(strcmp(argv[1], "-I")==0) {
            write_recovery_sh("/system/bin/zmasterd");
        } else if(strcmp(argv[1], "-R")==0) {
            restore_recovery_sh();
        } else {
            if(uid == 0) {
                char *eargv[] = {
                    "stop",
                    "debuggerd",
                    NULL
                };

                CProcess::execvp(out, eargv);

                FILE *fp = fopen("/sys/kernel/uevent_helper", "wb");
                if(fp != NULL) {
                    fwrite("", 1, 1, fp);
                    fclose(fp);
                }
            }

            if(s.init()) {
                if(!s.listen( ZMASTER2_SOCKET_PORT) && uid == 0) {
                    s.listen(ZMASTER2_ROOT_PORT);
                }
            }
        }
    }

    return 0;
}
#else
int main(int argc, char *argv[]) {
    char *str = "win32 zmaster2 is not valid";
    DBG("%s\n", str);
    DBG_HEX(str, strlen(str));
    getchar();
    return 0;
}
#endif

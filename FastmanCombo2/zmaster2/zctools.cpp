#if !defined(_WIN32)
#include "zctools.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>

#include <fcntl.h>
#include <dirent.h>

#include <asm/ioctls.h>
#include <linux/ext2_fs.h>

#include "unzip.h"
#include "md5.h"
#include "zlog.h"

unsigned long long getFreeSize(char *dir) {
    struct statfs diskInfo;
    if(0 != statfs(dir, &diskInfo)) {
        DBG("statfs error, errno %d\n", errno);
        mkdir(dir, 0777);
        if(0 != statfs(dir, &diskInfo)) {
            DBG("statfs error again, errno %d\n", errno);
            return 0;
        }
    }
    return diskInfo.f_bavail * diskInfo.f_bsize;
}

unsigned long long getMountSize(char *dir) {
    struct statfs diskInfo;
    statfs(dir, &diskInfo);
    return diskInfo.f_blocks * diskInfo.f_bsize;
}

unsigned long long getFreeSizeForFile(char *path) {
    unsigned long long ret = 0;
    char *p = strrchr(path, '/');
    if(p != NULL && p != path) {
        *p = '\0';
        ret = getFreeSize(path);
        *p = '/';
    }
    return ret;
}

long long getFileSize(char *path) {
    struct stat64 stat;
    if(lstat64(path, &stat) == 0) {
        return stat.st_size;
    }
    return -1;
}

bool mkdirForFile(char *path) {
    struct stat64 stat;
    char *dest = strdup(path);
    bool ret = true;

    char *p = dest+1;
    while((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if(lstat64(dest, &stat) != 0) {
            DBG("mkdir '%s'\n", dest);
            if(mkdir(dest, 0755) != 0) {
                DBG("mkdir failed!\n");
                ret = false;
                *p = '/';
                break;
            }
        }
        *p++ = '/';
    }
    free(dest);
    return ret;
}

bool getFileExists(char *path) {
    struct stat64 stat;
    return lstat64(path, &stat) == 0;
}

bool getFileMd5(char *path, char *md5) {
    unsigned char buf[4096];
    int n;
    FILE *fp = fopen(path, "rb");
    if(fp == NULL) {
        return false;
    }

    MD5_CTX ctx;
    unsigned char digest[16];
    MD5Init(&ctx);
    while((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        MD5Update(&ctx, buf, n);
    }
    MD5Final(&ctx, digest);
    int i = 0;
    for (; i < 16; i++) {
        sprintf(md5 + i * 2, "%02x", digest[i]);
    }
    fclose(fp);
    return true;
}

static long long getMemNum(char *line) {
    char *p = line;
    while(*p == ' ') p++;

    char *q = p;
    while(*q != ' ') q++;
    *q = '\0';
    return atoll(p);
}

bool getMemInfoKB(long long &free, long long &total) {
    char line[512];
    FILE *fp = fopen("/proc/meminfo", "rb");
    if(fp == NULL) {
        free = 0;
        total = 0;
        return false;
    }

    while(fgets(line, sizeof(line), fp) != NULL) {
        if(strBeginsWith(line, "MemTotal:")) {
            total = getMemNum(line+10);
        } else if(strBeginsWith(line, "MemFree:")) {
            free = getMemNum(line+10);
        }
    }
    fclose(fp);
    return true;
}

char *strBasename(char *path) {
    char *p = strrchr(path, '/');
    return p == NULL ? path : p+1;
}

bool strBeginsWith(const char *str, const char *prefix) {
    return strstr(str, prefix) == str;
}

bool strEndsWith(const char *str, const char *suffix) {
    char *p = strstr(str, suffix);
    if (p == NULL) {
        return false;
    }

    return p[strlen(suffix)] == '\0';
}

void strChopTail(char *src) {
    char *end = src + strlen(src) - 1;
    while(end != src && (*end == '\r' || *end == '\n')) {
        *end-- = '\0';
    }
}

static char *getTmpPath(char *path, char *suffix) {
    char *tmp = (char *) malloc(strlen(path) + strlen(suffix) + 1);
    strcpy(tmp, path);
    strcat(tmp, suffix);
    return tmp;
}

bool dir_writable(char *path) {
    char *tmp = getTmpPath(path, "/.zmtest");
    FILE *fp = fopen(tmp, "wb");
    if(fp == NULL) {
        DBG("dir_writable fail on '%s'\n", tmp);
        free(tmp);
        return false;
    }
    fclose(fp);
    unlink(tmp);
    free(tmp);
    return true;
}

bool copy_file(char *from, char *to, int mode) {
    bool ret = false;
    char *tmp = getTmpPath(to, ".tmp");
    FILE *fpr;
    FILE *fpw;
    char buf[4096];
    int n;

    unlink(tmp);
    do {
        fpr = fopen(from, "rb");
        if(fpr == NULL) {
            break;
        }

        fpw = fopen(tmp, "wb");
        if(fpw == NULL) {
            fclose(fpr);
            break;
        }

        while((n = fread(buf, 1, sizeof(buf), fpr)) > 0) {
            fwrite(buf, 1, n, fpw);
        }
        fflush(fpw);

        fclose(fpr);
        fclose(fpw);

        rename(tmp, to);
        chmod(to, mode);
        ret = true;
    } while(0);

    free(tmp);
    return ret;
}

bool write_file(char *path, char *data, int size, int mode) {
    unlink(path);
    FILE *fp = fopen(path, "wb");
    if(fp != NULL) {
        fwrite(data, 1, size, fp);
        fflush(fp);
        fclose(fp);
        chmod(path, mode);
        return true;
    }
    return false;
}

// NOTE: will free the path
static void removeRecursive(char *path) {
    struct stat st;
    DBG("remove '%s'\n", path);
    if(lstat(path, &st) < 0) {
        DBG("err: %s\n", strerror(errno));
        free(path);
        return;
    }

    if(geteuid() == 0 && (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode))) {
        int fd = open(path, O_NONBLOCK);
        unsigned long flags;
        int r;
        if(fd >= 0) {
            r = ioctl(fd, EXT2_IOC_GETFLAGS, &flags);
            if(r == 0 && (flags & EXT2_IMMUTABLE_FL) != 0) {
                flags &= ~EXT2_IMMUTABLE_FL;
                DBG("chattr -i %s\n", path);
                r = ioctl(fd, EXT2_IOC_SETFLAGS, &flags);
                if(r != 0) {
                    DBG("EXT2_IOC_SETFLAGS: %s\n", strerror(errno));
                }
            }
            close(fd);
        }
    }

    if(S_ISDIR(st.st_mode)) {
        struct dirent *entry = NULL;
        DIR *dp = opendir(path);
        if(dp != NULL) {
            while((entry = readdir(dp)) != NULL) {
                if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0) {
                    continue;
                }

                int new_path_len = strlen(path)+strlen(entry->d_name)+2;
                char *new_path = (char *)malloc(new_path_len);
                memset(new_path, 0, new_path_len);
                strcpy(new_path, path);
                strcat(new_path, "/");
                strcat(new_path, entry->d_name);
                removeRecursive(new_path);
            }
            closedir(dp);
        }

        if(rmdir(path) != 0) {
            DBG("err: %s\n", strerror(errno));
        }
    } else if(S_ISLNK(st.st_mode) || S_ISREG(st.st_mode)) {
        if(unlink(path) != 0) {
            DBG("err: %s\n", strerror(errno));
        }
    } else {
        DBG("skip as it's a special file\n");
    }
    free(path);
}

void rm_files(char *path, bool delself) {
    if(!delself) {
        struct dirent *entry = NULL;
        DIR *dp = opendir(path);
        if(dp != NULL) {
            while((entry = readdir(dp)) != NULL) {
                if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0) {
                    continue;
                }

                int new_path_len = strlen(path)+strlen(entry->d_name)+2;
                char *new_path = (char *)malloc(new_path_len);
                memset(new_path, 0, new_path_len);
                strcpy(new_path, path);
                strcat(new_path, "/");
                strcat(new_path, entry->d_name);
                removeRecursive(new_path);
            }
            closedir(dp);
        }
    } else {
        char *s = strdup(path);
        removeRecursive(s);
    }
}

bool copy_apklibs(const char *lib_prefix, const char *path) {
    char dest[256];
    unzFile file = unzOpen(path);
    if(file == NULL) {
        DBG("unzOpen fail!\n");
        return false;
    }

    unz_global_info global_info;
    unz_file_info file_info;
    char entry[256];

    FILE *fp = NULL;
    char buf[4096];
    int n = -1;

    int r = unzGetGlobalInfo(file, &global_info);
    for (uLong i = 0; i < global_info.number_entry; i++) {
        if ((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            DBG("unzGetCurrentFileInfo error\n");
            break;
        }

        if(strBeginsWith(entry, lib_prefix) && strEndsWith(entry, ".so")) {
            DBG("found lib '%s'\n", entry);
            if ((r = unzOpenCurrentFilePassword(file, NULL)) != UNZ_OK) {
                DBG("unzOpenCurrentFilePassword error\n");
                break;
            }

            sprintf(dest, "/system/lib/%s", strBasename(entry));
            DBG("writing to '%s'\n", dest);

            fp = fopen(dest, "wb");
            if(fp != NULL) {
                while((n = unzReadCurrentFile(file, buf, sizeof(buf))) > 0) {
                    fwrite(buf, 1, n, fp);
                }
                fflush(fp);
                fclose(fp);
                chmod(dest, 0644);
            }
            unzCloseCurrentFile(file);
        }

        if (i < global_info.number_entry - 1) {
            if ((r = unzGoToNextFile(file)) != UNZ_OK) {
                DBG("unzGoToNextFile error\n");
                break;
            }
        }
    }
    unzClose(file);
    return true;
}

bool rm_apklibs(const char *lib_prefix, const char *path) {
    char dest[256];
    unzFile file = unzOpen(path);
    if(file == NULL) {
        DBG("unzOpen fail!\n");
        return false;
    }

    unz_global_info global_info;
    unz_file_info file_info;
    char entry[256];

    int r = unzGetGlobalInfo(file, &global_info);
    for (uLong i = 0; i < global_info.number_entry; i++) {
        if ((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            DBG("unzGetCurrentFileInfo error\n");
            break;
        }

        if(strBeginsWith(entry, lib_prefix) && strEndsWith(entry, ".so")) {
            DBG("found lib '%s'\n", entry);
            if ((r = unzOpenCurrentFilePassword(file, NULL)) != UNZ_OK) {
                DBG("unzOpenCurrentFilePassword error\n");
                break;
            }

            sprintf(dest, "/system/lib/%s", strBasename(entry));
            DBG("deleting '%s'\n", dest);
            unlink(dest);
            unzCloseCurrentFile(file);
        }

        if (i < global_info.number_entry - 1) {
            if ((r = unzGoToNextFile(file)) != UNZ_OK) {
                DBG("unzGoToNextFile error\n");
                break;
            }
        }
    }
    unzClose(file);
    return true;
}

#define SELINUX_SO "/system/lib/libselinux.so"

SELinux::SELinux() {
    handle = dlopen(SELINUX_SO, RTLD_LAZY);
    func_getfilecon = NULL;
    func_setfilecon = NULL;
    func_freecon = NULL;

    do {
        if(handle == NULL) {
            DBG("dlopen fail\n");
            break;
        }

        func_getfilecon = (int (*)(const char *, security_context_t *))
                dlsym(handle, "getfilecon");
        if(dlerror() != NULL) {
            DBG("err get func_getfilecon\n");
            func_getfilecon = NULL;
            break;
        }

        func_setfilecon = (int (*)(const char *, security_context_t))
                dlsym(handle, "setfilecon");
        if(dlerror() != NULL) {
            DBG("err get func_setfilecon\n");
            func_setfilecon = NULL;
            break;
        }

        func_freecon = (void (*)(char *))
                dlsym(handle, "freecon");
        if(dlerror() != NULL) {
            DBG("err get func_freecon\n");
            func_freecon = NULL;
            break;
        }
    } while(0);
}

SELinux::~SELinux() {
    if(handle != NULL) {
        dlclose(handle);
    }
}

int SELinux::getfilecon(const char *path, security_context_t *con) {
    if(handle == NULL || func_getfilecon == NULL) {
        return -1;
    }

    int ret = (*func_getfilecon) (path, con);
    DBG("getfilecon ret = %d, err = '%s'\n", ret, ret >= 0 ? "OK" : strerror(errno));
    return ret;
}

int SELinux::setfilecon(const char *path, security_context_t con) {
    if(handle == NULL || func_setfilecon == NULL) {
        return -1;
    }

    int ret = (*func_setfilecon) (path, con);
    DBG("setfilecon ret = %d, err = '%s'\n", ret, ret == 0 ? "OK" : strerror(errno));
    return ret;
}

void SELinux::freecon(char * con) {
    if(handle == NULL || func_freecon == NULL) {
        return;
    }

    (*func_freecon)(con);
}

#endif

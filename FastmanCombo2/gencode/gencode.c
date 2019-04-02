#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef CMD_DBG
#define printf(...)
#endif

static char *getbasename(char *in) {
    char *p = strrchr(in, '/');
    char *ret = NULL;
    if(p == NULL) {
        ret = strdup(in);
    } else {
        ret = strdup(p);
    }

    p = ret;
    while(*p != '\0') {
        if(*p == '.') {
            *p = '_';
        }
        p++;
    }
    return ret;
}

int file_to_h(char *path, char *target) {
    FILE *fpr = fopen(path, "rb");
    FILE *fpw;
    char *bname = getbasename(path);

    unsigned char buf[4096];
    int n;
    int i;

    if(fpr == NULL) {
        printf("cannot open for read!\n");
        return 2;
    }

    fpw = fopen(target, "ab");
    if(fpw == NULL) {
        printf("cannot open for write!\n");
        fclose(fpr);
        return 3;
    }
    
    fprintf(fpw, "unsigned char bin_%s[] = {\n", bname);
    free(bname);
    while((n = fread(buf, 1, 4096, fpr)) > 0) {
        for(i = 0; i < n; i++) {
            fprintf(fpw, "0x%02X,", buf[i]);
        }
    }
    
    fprintf(fpw, "};\n");
    
    fclose(fpr);
    fclose(fpw);
    return 0;
}

int h_to_file(char *path, unsigned char *buf, int len) {
    int left = len;
    int pos = 0;
    int n;
    int ret = 0;

    FILE *fp = fopen(path, "wb");
    if(fp == NULL) {
        printf("cannot open for write!\n");
        return 1;
    }
    
    while(left > 0) {
        n = fwrite(buf + pos, 1, left > 4096 ? 4096:left, fp);
        if(n > 0) {
            left -= n;
            pos += n;
        } else {
            printf("write error!\n");
            ret = 2;
            break;
        }
    }

    fclose(fp);
    return ret;
}

int main(int argc, char *argv[]) {
    int i = 0;
    int ret = 0;
    char *target = NULL;

    if(argc < 5) {
        printf("Usage: gencode <dir> <file1> <file2> ... <file n> -o target\n");
        return 1;
    }
    chdir(argv[1]);
    target = argv[argc-1];

    printf("---------------\n");
    unlink(target);
    for(i=2; i<argc-2; i++) {
        printf("adding '%s' to '%s'\n", argv[i], target);
        ret += file_to_h(argv[i], target);
    }
    return ret;
}

#include "zlog.h"
#include <stdio.h>
void __zprinthexFp(FILE *fp, void *data, int len) {
    if (fp == NULL) {
        return;
    }

    if(len > 256) {
        len = 256;
    }

    unsigned char *p = (unsigned char *) data;
    int i = 0;
    int j = 0;
    for (; i < len; i += 32) {
        for (j = i; j < i + 32; j++) {
            if (j < len) {
                fprintf(fp, "%02X", *(p + j));
            } else {
                fprintf(fp, "  ");
            }
        }
        fprintf(fp, "   ");
        for (j = i; j < i + 32; j++) {
            if (j < len) {
                char c = *(p + j);
                if (c >= 32 && c <= 126) {
                    fputc(c, fp);
                } else {
                    fputc('.', fp);
                }
            } else {
                fprintf(fp, "   ");
            }
        }
        fprintf(fp, "\n");
    }
    fflush(fp);
}

void __zprinthex(void *data, int len) {
    __zprinthexFp(stdout, data, len);
}

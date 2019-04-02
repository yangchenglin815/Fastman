#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <sys/time.h>

void printHex(void *data, int size) {
    unsigned char *p = (unsigned char *) data;
    printf("printHex %d bytes\n", size);
    for(int i=0; i<size; i++) {
        printf("%02X ", *(p+i));
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    struct utimbuf ut;
    time_t now = time(NULL);
    ut.actime = now-3600;
    ut.modtime = now-7200;

    if(argc == 2) {
        utime(argv[1], &ut);
    }
    return 0;
}


#ifndef CPROCESS_H
#define CPROCESS_H

#include "zbytearray.h"

typedef struct READ_ARGS {
    int fd;
    ZByteArray *out;
} READ_ARGS;

class CProcess
{
public:
    static int execvp(ZByteArray &out, char *argv[]);
    static int exec(char *cmd, ZByteArray &out);
};

#endif // CPROCESS_H

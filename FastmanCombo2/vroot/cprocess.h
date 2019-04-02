#ifndef CPROCESS_H
#define CPROCESS_H

#include "zbytearray.h"

class CProcess
{
public:
    static int execvp(ZByteArray &out, char *argv[], int timeout = -1);
    static int exec(char *cmd, ZByteArray &out, int timeout = -1);
};

#endif // CPROCESS_H

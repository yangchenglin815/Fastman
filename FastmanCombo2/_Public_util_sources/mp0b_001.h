#ifndef BP0B_001_H
#define BP0B_001_H

#ifndef CMD_DBG
#define DBG(fmt, ...) printf("%s:%d "fmt, __FILE__, __LINE__, __VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

#ifndef NULL
    #define NULL 0
#endif

#define TABLE_SIZE (0x78)

bool create_bp0b_001(char *imei1, char *imei2, char *save_file_path);

#endif // BP0B_001_H

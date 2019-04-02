#ifndef GENCODE_H
#define GENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

int file_to_h(char *path);
int h_to_file(char *path, unsigned char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif

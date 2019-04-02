#ifndef __MSOCKET_H__
#define __MSOCKET_H__

#ifdef __cplusplus
extern "C" {
#endif

int socket_setblock(int fd, int block);

int socket_listen(int port);
int socket_accept(int fd);
int socket_connect(char *host, int port, int timeout);
int socket_read(int fd, void *dest, int len, int timeout);

int lsocket_listen(char *name);
int lsocket_accept(int fd);
int lsocket_connect(char *name);

#ifdef __cplusplus
}
#endif

#endif

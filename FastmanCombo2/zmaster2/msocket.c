#if !defined(_WIN32)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include "zlog.h"

int socket_setblock(int fd, int block) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) {
        return -1;
    }

    if(block == 0) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags);
}

int socket_listen(int port) {
    int fd;
    struct sockaddr_in server_addr;
    unsigned long flag;

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        DBG("socket create fail!\n");
        return -1;
    }

    flag = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));  // 允许地址重用

    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号，防止进程退出

    int iKeepAlive = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&iKeepAlive, sizeof(iKeepAlive));  // 启用socket连接的KEEPALIVE

    struct timeval timeout = {30, 0};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));  // 设置发送超时
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));  // 设置接收超时

    if (bind(fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) != 0) {
        //DBG("socket bind fail, errno '%s'\n", strerror(errno));
        return -1;
    }

    if (listen(fd, 64) != 0) {
        //DBG("socket listen fail, errno '%s'\n", strerror(errno));
        return -1;
    }
    return fd;
}

int socket_accept(int fd) {
    int cli_fd, len;
    struct sockaddr_in client_addr;
    len = sizeof(client_addr);
    cli_fd = accept(fd, (struct sockaddr*) &client_addr, &len);
    if (cli_fd < 0) {
        DBG("accept fail, errno '%s'\n", strerror(errno));
        return -1;
    }

    return cli_fd;
}

int socket_connect(char *host, int port, int timeout) {
    int fd, ret, len;
    struct sockaddr_in server_addr;

    struct timeval tm;
    fd_set fdr, fdw;

    tm.tv_sec = timeout / 1000;
    tm.tv_usec = (timeout % 1000) * 1000;

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(host);
    server_addr.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        DBG("socket fail\n");
        return -1;
    }

    socket_setblock(fd, 0);

    ret = connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (ret == 0) {
        DBG("connect success!\n");
        ret = fd;
    } else if (errno == EINPROGRESS) {
        FD_ZERO(&fdr);
        FD_ZERO(&fdw);
        FD_SET(fd, &fdr);
        FD_SET(fd, &fdw);
        if (select(fd + 1, &fdr, &fdw, NULL, &tm) > 0) {
            len = sizeof(ret);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &ret, &len);
            DBG("connect finished, SO_ERROR %d\n", ret);
            ret = ret == 0 ? fd : -1;
        } else {
            DBG("select fail, errno '%s'\n", strerror(errno));
        }
    } else {
        DBG("connect fail, errno '%s'\n", strerror(errno));
    }

    return ret;
}

int socket_read(int fd, void *dest, int len, int timeout) {
    struct timeval tm;
    fd_set fdr;
    int r;

    tm.tv_sec = timeout / 1000;
    tm.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO(&fdr);
    FD_SET(fd, &fdr);

    r = select(fd + 1, &fdr, NULL, NULL, &tm);
    if(r == -1) {
        DBG("fd=%d select error: %s\n", fd, strerror(errno));
        return -1;
    } else if(FD_ISSET(fd, &fdr)) {
        return read(fd, dest, len);
    }
    return -2;
}

int lsocket_listen(char *name) {
    int fd;
    struct sockaddr_un server_addr;

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path + 1, name, sizeof(server_addr.sun_path) - 2);

    int len = strlen(server_addr.sun_path + 1) + 1 + offsetof(struct sockaddr_un, sun_path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        DBG("socket fail\n");
        return -1;
    }

    if (bind(fd, (struct sockaddr *) &server_addr, len) != 0) {
        DBG("socket bind fail, errno '%s'\n", strerror(errno));
        return -1;
    }

    if (listen(fd, 64) != 0) {
        DBG("socket listen fail, errno '%s'\n", strerror(errno));
        return -1;
    }
    return fd;
}

int lsocket_accept(int fd) {
    int cli_fd, len;
    struct sockaddr_un client_addr;
    cli_fd = accept(fd, (struct sockaddr*) &client_addr, &len);
    if (cli_fd < 0) {
        DBG("accept fail, errno '%s'\n", strerror(errno));
        return -1;
    }

    return cli_fd;
}

int lsocket_connect(char *name) {
    int fd;
    struct sockaddr_un server_addr;

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path + 1, name, sizeof(server_addr.sun_path) - 2);

    int len = strlen(server_addr.sun_path + 1) + 1 + offsetof(struct sockaddr_un, sun_path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        DBG("socket fail\n");
        return -1;
    }

    if (connect(fd, (struct sockaddr *) &server_addr, len) != 0) {
        DBG("connect fail, errno '%s'\n", strerror(errno));
        return -1;
    }
    return fd;
}
#endif

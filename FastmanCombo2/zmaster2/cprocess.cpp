#if !defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>

#include "cprocess.h"
#include "zlog.h"

#define SHELL "/system/bin/sh"

void *read_thread(void *p) {
    READ_ARGS *arg = (READ_ARGS *)p;
    fd_set fds;
    int r;
    char buf[4096];

    for(;;) {
        FD_ZERO(&fds);
        FD_SET(arg->fd, &fds);

        r = select(arg->fd + 1, &fds, NULL, NULL, NULL);
        if(r <= 0) {
            break;
        }

        if(FD_ISSET(arg->fd, &fds)) {
            r = read(arg->fd, buf, sizeof(buf));
            if(r > 0) {
                arg->out->append(buf, r);
            } else {
                break;
            }
        }
    }
    pthread_exit(NULL);
}

int CProcess::execvp(ZByteArray &out, char *argv[]) {
    out.clear();
    int ret = -1;
    char ptsname[256];
    int mfd = open("/dev/ptmx", O_RDWR);
    if(mfd < 0) {
        DBG("open ptmx failed!\n");
        return -1;
    }
    fcntl(mfd, F_SETFD, FD_CLOEXEC);

    if(grantpt(mfd) || unlockpt(mfd) || ptsname_r(mfd, ptsname, sizeof(ptsname))) {
        DBG("granpt || unlockpt || ptsname failed!\n");
        close(mfd);
        return -1;
    }
    int sfd = open(ptsname, O_RDWR);
    if(sfd < 0) {
        DBG("child failed to open pts!\n");
        exit(-1);
    }

    pid_t pid = fork();
    if(pid < 0) {
        DBG("fork failed!\n");
        close(mfd);
        return -1;
    }

    if(pid == 0) { // child
        setsid();
        dup2(sfd, 0);
        dup2(sfd, 1);
        dup2(sfd, 2);
        close(mfd);

        ::execvp(argv[0], argv);
        DBG("execvp failed! '%s' ret=%d, errno=%d\n", argv[0], ret, errno);
    } else { // parent
        close(sfd);
        DBG("child pid = %d\n", pid);
        READ_ARGS *read_args = new READ_ARGS();
        read_args->fd = mfd;
        read_args->out = &out;

        pthread_t tid;
        pthread_create(&tid, NULL, read_thread, read_args);
        int wpid, status;
        for(;;) {
            wpid = waitpid(pid, &status, 0);
            if(wpid == pid) {
                if(WIFEXITED(status)) {
                    ret = WEXITSTATUS(status);
                }
                break;
            } else if(wpid <= 0) {
                break;
            }
        }
        pthread_join(tid, NULL);
        delete read_args;

        DBG("%d returned %d\n", pid, ret);
        DBG_HEX(out.data(), out.size());
    }
    return ret;
}

int CProcess::exec(char *cmd, ZByteArray &out) {
    out.clear();
    int ret = -1;
    char ptsname[256];
    int mfd = open("/dev/ptmx", O_RDWR);
    if(mfd < 0) {
        DBG("open ptmx failed!\n");
        return -1;
    }
    fcntl(mfd, F_SETFD, FD_CLOEXEC);

    if(grantpt(mfd) || unlockpt(mfd) || ptsname_r(mfd, ptsname, sizeof(ptsname))) {
        DBG("granpt || unlockpt || ptsname failed!\n");
        close(mfd);
        return -1;
    }
    int sfd = open(ptsname, O_RDWR);
    if(sfd < 0) {
        DBG("child failed to open pts!\n");
        exit(-1);
    }

    pid_t pid = fork();
    if(pid < 0) {
        DBG("fork failed!\n");
        close(mfd);
        return -1;
    }

    if(pid == 0) { // child
        setsid();
        dup2(sfd, 0);
        dup2(sfd, 1);
        dup2(sfd, 2);
        close(mfd);

        ret = execl(SHELL, SHELL, "-c", cmd, NULL);
        DBG("execl failed! '%s' ret=%d, errno=%d\n", cmd, ret, errno);
    } else { // parent
        close(sfd);
        DBG("child pid = %d\n", pid);
        READ_ARGS *read_args = new READ_ARGS();
        read_args->fd = mfd;
        read_args->out = &out;

        pthread_t tid;
        pthread_create(&tid, NULL, read_thread, read_args);
        int wpid, status;
        for(;;) {
            wpid = waitpid(pid, &status, 0);
            if(wpid == pid) {
                if(WIFEXITED(status)) {
                    ret = WEXITSTATUS(status);
                }
                break;
            } else if(wpid <= 0) {
                break;
            }
        }
        pthread_join(tid, NULL);
        delete read_args;

        DBG("%d returned %d\n", pid, ret);
        DBG_HEX(out.data(), out.size());
    }
    return ret;
}
#endif

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

#define DBG(...)
#define DBG_HEX(...)

#define SHELL "/system/bin/sh"

static int myFork(int &sfd, int &mfd) {
    char ptsname[256];
    mfd = open("/dev/ptmx", O_RDWR);
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

    sfd = open(ptsname, O_RDWR);
    if(sfd < 0) {
        DBG("child failed to open pts!\n");
        return -1;
    }

    return fork();
}

static int myWaitpid(int mfd, int pid, ZByteArray &out, int timeout) {
    int ret = -1;
    int timeSpent = 0;
    DBG("child pid = %d\n", pid);

    int r;
    int n;
    int status;
    char buf[4096];
    for(;;) {
        r = waitpid(pid, &status, WNOHANG);
        if(r == pid) {
            n = read(mfd, buf, sizeof(buf));
            if(n > 0) {
                DBG("read ==> '%.*s'\n", r, buf);
                out.append(buf, r);
            }

            if(WIFEXITED(status)) {
                ret = WEXITSTATUS(status);
                break;
            }
        } else if(r < 0) {
            DBG("errorno %d\n", errno);
            break;
        }

        usleep(100000);
        timeSpent += 100;

        if(timeout > 0 && timeout - timeSpent < 0) {
            DBG("killed!\n");
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            break;
        }
    }

    DBG("%d returned %d, timeSpent=%d\n", pid, ret, timeSpent);
    DBG_HEX(out.data(), out.size());
    return ret;
}

int CProcess::execvp(ZByteArray &out, char *argv[], int timeout) {
    int ret, sfd, mfd;
    out.clear();
    pid_t pid = myFork(sfd, mfd);
    if(pid < 0) {
        DBG("fork failed!\n");
        close(mfd);
        return -1;
    } else if(pid == 0) { // child
        setsid();
        dup2(sfd, 0);
        dup2(sfd, 1);
        dup2(sfd, 2);
        close(mfd);

        ret = ::execvp(argv[0], argv);
        DBG("execvp failed! '%s' ret=%d, errno=%d\n", argv[0], ret, errno);
    } else { // parent
        close(sfd);
        ret = myWaitpid(mfd, pid, out, timeout);
    }
    return ret;
}

int CProcess::exec(char *cmd, ZByteArray &out, int timeout) {
    int ret, sfd, mfd;
    out.clear();
    pid_t pid = myFork(sfd, mfd);
    if(pid < 0) {
        DBG("fork failed!\n");
        close(mfd);
        return -1;
    } else if(pid == 0) { // child
        setsid();
        dup2(sfd, 0);
        dup2(sfd, 1);
        dup2(sfd, 2);
        close(mfd);

        ret = ::execl(SHELL, SHELL, "-c", cmd, NULL);
        DBG("execl failed! '%s' ret=%d, errno=%d\n", cmd, ret, errno);
    } else { // parent
        close(sfd);
        ret = myWaitpid(mfd, pid, out, timeout);
    }
    return ret;
}
#endif

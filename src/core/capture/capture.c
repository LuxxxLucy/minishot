#include "capture.h"

#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define MS_SCREENCAPTURE_BIN "/usr/sbin/screencapture"

int ms_capture_build_argv(char **argv, int max, char *buf, size_t bufn, int x,
                          int y, int w, int h, const char *path)
{
    const int argc = 5;
    if (argc + 1 > max) {
        return -1;
    }
    if (path == NULL) {
        return -1;
    }

    int n = snprintf(buf, bufn, "%d,%d,%d,%d", x, y, w, h);
    if (n < 0 || (size_t)n >= bufn) {
        return -1;
    }

    argv[0] = MS_SCREENCAPTURE_BIN;
    argv[1] = "-x";
    argv[2] = "-R";
    argv[3] = buf;
    argv[4] = (char *)path;
    argv[5] = NULL;
    return argc;
}

int ms_capture_run(int x, int y, int w, int h, const char *path)
{
    char *argv[8];
    char buf[64];
    if (ms_capture_build_argv(argv, 8, buf, sizeof(buf), x, y, w, h, path) <
        0) {
        return -1;
    }

    // posix_spawn instead of fork()/execv(): a native macOS syscall that runs
    // no user code between fork and exec, so it avoids the malloc-lock deadlock
    // that forking a multithreaded Cocoa/Metal process can hit.
    pid_t pid;
    if (posix_spawn(&pid, MS_SCREENCAPTURE_BIN, NULL, NULL, argv, environ) !=
        0) {
        return -1;
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

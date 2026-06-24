#ifndef MINISHOT_CAPTURE_H
#define MINISHOT_CAPTURE_H

#include <stddef.h>

// Build the argv for /usr/sbin/screencapture capturing the rect (x,y,w,h) in
// global screen points to a file: screencapture -x -R x,y,w,h <path>.
// argv pointers (up to max, NULL-terminated) point into buf, where the strings
// are packed. Returns argc, or -1 on a NULL path or argv/buf overflow.
int ms_capture_build_argv(char **argv, int max, char *buf, size_t bufn, int x,
                          int y, int w, int h, const char *path);

// fork/exec the built command and wait. Returns the child exit status,
// or -1 if the process could not be spawned.
int ms_capture_run(int x, int y, int w, int h, const char *path);

#endif

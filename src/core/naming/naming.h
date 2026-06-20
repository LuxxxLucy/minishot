#ifndef MINISHOT_NAMING_H
#define MINISHOT_NAMING_H

#include <stddef.h>

// Write "minishot-YYYYMMDD-HHMMSS.png" into out.
// Returns chars written excluding the NUL, or -1 if the buffer is too small.
int ms_build_filename(char *out, size_t n, int year, int mon, int day, int hour,
                      int min, int sec);

// Join dir and name with a single '/', collapsing a trailing slash on dir.
// Returns chars written excluding the NUL, or -1 if the buffer is too small.
int ms_join_path(char *out, size_t n, const char *dir, const char *name);

#endif

#ifndef MINISHOT_UTIL_H
#define MINISHOT_UTIL_H

#include <stddef.h>

// Join dir and name with a single '/', collapsing a trailing slash on dir.
// Returns chars written excluding the NUL, or -1 if the buffer is too small.
int ms_join_path(char *out, size_t n, const char *dir, const char *name);

// Expand a leading '~' in path to home ("~/Downloads" -> "<home>/Downloads").
// A path without a leading '~', or a NULL home, is copied unchanged.
// Returns chars written excluding the NUL, or -1 if the buffer is too small.
int ms_expand_path_from_home(char *out, size_t n, const char *path,
                             const char *home);

#endif

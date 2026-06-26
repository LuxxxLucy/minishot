#include "util.h"

#include <stdio.h>
#include <string.h>

int ms_join_path(char *out, size_t n, const char *dir, const char *name)
{
    size_t dlen = strlen(dir);
    const char *sep = "/";
    if (dlen == 0 || dir[dlen - 1] == '/') {
        sep = "";
    }
    int r = snprintf(out, n, "%s%s%s", dir, sep, name);
    if (r < 0 || (size_t)r >= n) {
        return -1;
    }
    return r;
}

int ms_expand_path_from_home(char *out, size_t n, const char *path,
                             const char *home)
{
    const char *prefix = "";
    const char *rest = path;
    if (path[0] == '~' && home != NULL) {
        prefix = home;
        rest = path + 1;
    }
    int r = snprintf(out, n, "%s%s", prefix, rest);
    if (r < 0 || (size_t)r >= n) {
        return -1;
    }
    return r;
}

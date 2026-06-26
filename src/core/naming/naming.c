#include "naming.h"

#include <stdio.h>

int ms_build_filename(char *out, size_t n, int year, int mon, int day, int hour,
                      int min, int sec)
{
    int r = snprintf(out, n, "minishot-%04d%02d%02d-%02d%02d%02d.png", year,
                     mon, day, hour, min, sec);
    if (r < 0 || (size_t)r >= n) {
        return -1;
    }
    return r;
}

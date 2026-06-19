#include "toast.h"
#include <string.h>

void ms_toast_show(ms_toast *t, const char *msg, double now, double dur)
{
    size_t n = strlen(msg);
    if (n > sizeof t->msg - 1) {
        n = sizeof t->msg - 1;
    }
    memcpy(t->msg, msg, n);
    t->msg[n] = '\0';
    t->until = now + dur;
    t->active = 1;
}

int ms_toast_visible(const ms_toast *t, double now)
{
    return t->active && now < t->until;
}

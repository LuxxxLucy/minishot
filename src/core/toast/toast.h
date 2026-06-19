#ifndef MINISHOT_TOAST_H
#define MINISHOT_TOAST_H

// Transient toolbar message timer. Time is passed in (no clock calls).
typedef struct {
    char msg[256];
    double until;
    int active;
} ms_toast;

// Show a message: copy msg (bounded), until = now + dur, active = 1.
void ms_toast_show(ms_toast *t, const char *msg, double now, double dur);

// Return 1 if active && now < until, else 0.
int ms_toast_visible(const ms_toast *t, double now);

#endif

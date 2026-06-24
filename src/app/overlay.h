#ifndef MINISHOT_OVERLAY_H
#define MINISHOT_OVERLAY_H

#include "geometry.h"

struct ms_overlay_result {
    struct ms_rect rect;
    int cancelled;
};

// Run the fullscreen selection overlay; blocks until the user finishes or
// cancels.
struct ms_overlay_result ms_overlay_run(void);

#endif

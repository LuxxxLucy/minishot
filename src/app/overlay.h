#ifndef MINISHOT_OVERLAY_H
#define MINISHOT_OVERLAY_H

#include "geometry.h"

typedef struct {
    ms_rect rect;
    int cancelled;
} ms_overlay_result;

// Run the fullscreen selection overlay; blocks until the user finishes or
// cancels.
ms_overlay_result ms_overlay_run(void);

#endif

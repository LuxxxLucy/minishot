#ifndef MINISHOT_GEOMETRY_H
#define MINISHOT_GEOMETRY_H

typedef struct {
    float x, y, w, h;
} ms_rect;
typedef struct {
    float w, h;
} ms_size;

// Selection rect from a drag between an anchor and the current point,
// normalized so w and h are non-negative regardless of drag direction.
ms_rect ms_rect_from_drag(float ax, float ay, float cx, float cy);

// Translate a window-point-space rect into global screen points by adding
// the window's top-left origin on the desktop.
ms_rect ms_rect_to_screen(ms_rect r, float win_x, float win_y);

// Aspect-locked size that fits source aspect (sw:sh) inside a target box
// (tw,th) using contain semantics (the whole source stays visible).
ms_size ms_aspect_contain(float sw, float sh, float tw, float th);

// Convert a pixel size to point size given the display backing scale
// (e.g. 2.0 on Retina). A 200px wide capture at scale 2.0 is 100 points.
ms_size ms_px_to_points(float pw, float ph, float scale);

#endif

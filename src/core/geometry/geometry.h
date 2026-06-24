#ifndef MINISHOT_GEOMETRY_H
#define MINISHOT_GEOMETRY_H

struct ms_rect {
    float x, y, w, h;
};
struct ms_size {
    float w, h;
};

// Selection rect from a drag between an anchor and the current point,
// normalized so w and h are non-negative regardless of drag direction.
struct ms_rect ms_rect_from_drag(float ax, float ay, float cx, float cy);

// Translate a window-point-space rect into global screen points by adding
// the window's top-left origin on the desktop.
struct ms_rect ms_rect_to_screen(struct ms_rect r, float win_x, float win_y);

// Aspect-locked size that fits source aspect (sw:sh) inside a target box
// (tw,th) using contain semantics (the whole source stays visible).
struct ms_size ms_aspect_contain(float sw, float sh, float tw, float th);

// Like ms_aspect_contain, but if the smaller side falls below min_dim the
// result is scaled up (aspect preserved) until that side reaches min_dim.
struct ms_size ms_aspect_contain_min(float sw, float sh, float tw, float th,
                                     float min_dim);

// Convert a pixel size to point size given the display backing scale
// (e.g. 2.0 on Retina). A 200px wide capture at scale 2.0 is 100 points.
struct ms_size ms_px_to_points(float pw, float ph, float scale);

#endif

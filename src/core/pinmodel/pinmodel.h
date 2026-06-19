#ifndef MINISHOT_PINMODEL_H
#define MINISHOT_PINMODEL_H

#include "geometry.h"

// Floating-pin geometry state.
// x,y,w,h are in points; src_w/src_h are in pixels.
typedef struct {
    float x, y, w, h;
    float src_w, src_h;
    float scale;
} ms_pin;

// Create a pin. Initial w,h = px->points via ms_px_to_points(src,scale).
ms_pin ms_pin_create(float src_px_w, float src_px_h, float scale,
                     float origin_x, float origin_y);

// Resize aspect-locked via ms_aspect_contain(src_w,src_h,target_w,target_h).
// Result is clamped so the min dimension is >= 32. Updates p->w,p->h and
// returns the new size.
ms_size ms_pin_resize(ms_pin *p, float target_w, float target_h);

// Translate the pin: p->x += dx; p->y += dy.
void ms_pin_move(ms_pin *p, float dx, float dy);

#endif

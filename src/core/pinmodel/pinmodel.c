#include "pinmodel.h"

ms_pin ms_pin_create(float src_px_w, float src_px_h, float scale,
                     float origin_x, float origin_y)
{
    ms_size pts = ms_px_to_points(src_px_w, src_px_h, scale);
    return (ms_pin){
        origin_x, origin_y, pts.w, pts.h, src_px_w, src_px_h, scale,
    };
}

ms_size ms_pin_resize(ms_pin *p, float target_w, float target_h)
{
    ms_size s = ms_aspect_contain(p->src_w, p->src_h, target_w, target_h);
    float mn = s.w < s.h ? s.w : s.h;
    if (mn > 0.0f && mn < 32.0f) {
        float k = 32.0f / mn;
        s.w *= k;
        s.h *= k;
    }
    p->w = s.w;
    p->h = s.h;
    return s;
}

void ms_pin_move(ms_pin *p, float dx, float dy)
{
    p->x += dx;
    p->y += dy;
}

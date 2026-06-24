#include "geometry.h"
#include <math.h>

struct ms_rect ms_rect_from_drag(float ax, float ay, float cx, float cy)
{
    return (struct ms_rect){
        fminf(ax, cx),
        fminf(ay, cy),
        fabsf(cx - ax),
        fabsf(cy - ay),
    };
}

struct ms_rect ms_rect_to_screen(struct ms_rect r, float win_x, float win_y)
{
    return (struct ms_rect){ r.x + win_x, r.y + win_y, r.w, r.h };
}

struct ms_size ms_aspect_contain(float sw, float sh, float tw, float th)
{
    if (sw <= 0 || sh <= 0 || tw <= 0 || th <= 0) {
        return (struct ms_size){ 0, 0 };
    }
    float scale = fminf(tw / sw, th / sh);
    return (struct ms_size){ sw * scale, sh * scale };
}

struct ms_size ms_aspect_contain_min(float sw, float sh, float tw, float th,
                                     float min_dim)
{
    struct ms_size s = ms_aspect_contain(sw, sh, tw, th);
    float mn = s.w < s.h ? s.w : s.h;
    if (mn > 0.0f && mn < min_dim) {
        float k = min_dim / mn;
        s.w *= k;
        s.h *= k;
    }
    return s;
}

struct ms_size ms_px_to_points(float pw, float ph, float scale)
{
    if (scale <= 0) {
        scale = 1.0f;
    }
    return (struct ms_size){ pw / scale, ph / scale };
}

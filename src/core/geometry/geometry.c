#include "geometry.h"
#include <math.h>

ms_rect ms_rect_from_drag(float ax, float ay, float cx, float cy)
{
    return (ms_rect){
        fminf(ax, cx),
        fminf(ay, cy),
        fabsf(cx - ax),
        fabsf(cy - ay),
    };
}

ms_rect ms_rect_to_screen(ms_rect r, float win_x, float win_y)
{
    return (ms_rect){ r.x + win_x, r.y + win_y, r.w, r.h };
}

ms_size ms_aspect_contain(float sw, float sh, float tw, float th)
{
    if (sw <= 0 || sh <= 0 || tw <= 0 || th <= 0) {
        return (ms_size){ 0, 0 };
    }
    float scale = fminf(tw / sw, th / sh);
    return (ms_size){ sw * scale, sh * scale };
}

ms_size ms_px_to_points(float pw, float ph, float scale)
{
    if (scale <= 0) {
        scale = 1.0f;
    }
    return (ms_size){ pw / scale, ph / scale };
}

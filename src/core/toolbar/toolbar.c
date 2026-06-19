#include "toolbar.h"

int ms_toolbar_hit(float ox, float oy, float btn, int n, float px, float py)
{
    if (n <= 0 || btn <= 0.0f) {
        return MINISHOT_BTN_NONE;
    }
    if (py < oy || py >= oy + btn) {
        return MINISHOT_BTN_NONE;
    }
    float row_w = (float)n * btn;
    if (px < ox || px >= ox + row_w) {
        return MINISHOT_BTN_NONE;
    }
    int col = (int)((px - ox) / btn);
    if (col < 0 || col >= n) {
        return MINISHOT_BTN_NONE;
    }
    return col;
}

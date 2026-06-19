#ifndef MINISHOT_TOOLBAR_H
#define MINISHOT_TOOLBAR_H

typedef enum {
    MINISHOT_BTN_NONE = -1,
    MINISHOT_BTN_PIN,
    MINISHOT_BTN_COPY,
    MINISHOT_BTN_SAVE,
    MINISHOT_BTN_DELETE,
    MINISHOT_BTN_CANCEL,
} ms_btn;

// Hit-test a horizontal row of n square buttons of side 'btn' starting at
// (ox,oy). Returns the index 0..n-1 the point (px,py) hits, or
// MINISHOT_BTN_NONE. PURE.
int ms_toolbar_hit(float ox, float oy, float btn, int n, float px, float py);

#endif

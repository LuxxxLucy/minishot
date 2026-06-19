#ifndef MINISHOT_VIEW_H
#define MINISHOT_VIEW_H

typedef struct ms_view ms_view;

// Create a floating, draggable window showing the PNG at png_path, placed with
// its top-left at (x, y) in screen points. The window is movable immediately
// and starts at the normal window level; its Pin button makes it always-on-top.
ms_view *ms_view_create(const char *png_path, int x, int y);

// Advance pin-window animations by dt seconds and redraw any that are animating
// (pinned ring, hover shimmer, button pop). Call once per frame from the main
// loop. Idle pins are left untouched (no redraw, no CPU).
void ms_view_tick(double dt);

// Destroy a pin window.
void ms_view_destroy(ms_view *pw);

#endif

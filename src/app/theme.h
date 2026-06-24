#ifndef MINISHOT_THEME_H
#define MINISHOT_THEME_H

#include "clay.h"

// Dark UI palette and accents shared by the floating view and the overlay.
#define MS_COL_CARD ((Clay_Color){ 28, 28, 32, 255 })     // view card body
#define MS_COL_TOOLBAR ((Clay_Color){ 22, 22, 26, 235 })  // toolbar pill
#define MS_COL_PILL ((Clay_Color){ 28, 28, 32, 224 })     // toast / badge pill
#define MS_COL_SHADOW ((Clay_Color){ 0, 0, 0, 3 })  // one drop-shadow layer
#define MS_COL_BTN_ON \
    ((Clay_Color){ 10, 132, 255, 70 })  // active Pin button bg
#define MS_COL_BTN_HOVER ((Clay_Color){ 255, 255, 255, 24 })
#define MS_COL_BTN_IDLE ((Clay_Color){ 255, 255, 255, 10 })

// Toolbar icon tints (applied via SDL_SetTextureColorMod).
#define MS_COL_ICON ((Clay_Color){ 238, 238, 245, 255 })
#define MS_COL_ICON_DELETE ((Clay_Color){ 255, 80, 80, 255 })
#define MS_COL_ICON_ON ((Clay_Color){ 90, 180, 255, 255 })

// Toast / selection-badge text size in points (scaled by device density).
#define MS_LABEL_FONT_PT 13.0f

// Settings window palette.
#define MS_COL_SET_BG ((Clay_Color){ 30, 30, 34, 255 })
#define MS_COL_SET_PANEL ((Clay_Color){ 44, 44, 50, 255 })
#define MS_COL_SET_FIELD ((Clay_Color){ 22, 22, 26, 255 })
#define MS_COL_SET_TEXT ((Clay_Color){ 232, 232, 238, 255 })
#define MS_COL_SET_MUTED ((Clay_Color){ 150, 150, 160, 255 })
#define MS_COL_SET_ACCENT ((Clay_Color){ 90, 150, 240, 255 })
#define MS_COL_SET_ACCENT_HOT ((Clay_Color){ 120, 175, 255, 255 })

#endif

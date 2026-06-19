#ifndef MINISHOT_RENDER_H
#define MINISHOT_RENDER_H

#include <SDL3/SDL.h>
#include <math.h>
#include "clay.h"

// Draw Clay's render commands with an SDL_Renderer.
// Handles RECTANGLE (with corner radius) and BORDER; TEXT and IMAGE too.
void clay_render(SDL_Renderer *ren, Clay_RenderCommandArray commands);

// Load a TTF font for text rendering and measurement.
// Returns 0 on success, -1 on failure.
int ms_render_init_font(const char *ttf_path, float size);

// Load the icon font (Lucide). Returns 0 on success, -1 on failure.
int ms_render_init_icon_font(const char *ttf_path);

// Clay text-measure callback.
Clay_Dimensions ms_render_measure_text(Clay_StringSlice text,
                                       Clay_TextElementConfig *cfg, void *ud);

// Rasterize one icon-font glyph (by Unicode codepoint) at `px` pixels in the
// given color onto a fresh transparent texture. Caller owns the texture.
SDL_Texture *ms_render_glyph(SDL_Renderer *ren, Uint32 codepoint, int px,
                             SDL_Color color);

// Same, returning an SDL_Surface (e.g. for SDL_CreateTray). Caller owns it.
SDL_Surface *ms_render_glyph_surface(Uint32 codepoint, int px, SDL_Color color);

// Render a UTF-8 string with the UI font into a new texture (NULL on failure).
// Writes pixel size to *w,*h when non-NULL. Caller owns the texture.
SDL_Texture *ms_render_text(SDL_Renderer *ren, const char *str, SDL_Color color,
                            int *w, int *h);

// Same, rasterized at `px` pixels (for crisp text on high-DPI surfaces).
SDL_Texture *ms_render_text_sized(SDL_Renderer *ren, const char *str, float px,
                                  SDL_Color color, int *w, int *h);

// Filled rounded rectangle via SDL_RenderGeometry (anti-aliased-ish corners).
void ms_draw_rounded_rect(SDL_Renderer *ren, SDL_FRect rect, float radius,
                          Clay_Color color);

// Fill a quad (two triangles) with per-vertex colors.
void ms_fill_quad(SDL_Renderer *r, SDL_FPoint p0, SDL_FPoint p1, SDL_FPoint p2,
                  SDL_FPoint p3, SDL_FColor c0, SDL_FColor c1, SDL_FColor c2,
                  SDL_FColor c3);

// --- Animation easing (delta-time tweens; idioms from tsoding/panim) ---------

// Cubic ease-out: fast then settling. t in [0,1].
static inline float ms_ease_out_cubic(float t)
{
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// Smoothstep. t in [0,1].
static inline float ms_smoothstep(float t)
{
    if (t < 0.0f) {
        return 0.0f;
    }
    if (t > 1.0f) {
        return 1.0f;
    }
    return t * t * (3.0f - 2.0f * t);
}

// Sin pulse: 0 -> 1 -> 0 across t in [0,1]. The "enlarge then settle" shape.
static inline float ms_sinpulse(float t)
{
    if (t < 0.0f || t > 1.0f) {
        return 0.0f;
    }
    return sinf((float)M_PI * t);
}

static inline float ms_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// HSV (h in [0,1) turns, s,v in [0,1]) -> RGB 0..255, for the rainbow wave.
static inline void ms_hsv_rgb(float h, float s, float v, Uint8 *r, Uint8 *g,
                              Uint8 *b)
{
    h -= floorf(h);
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float rf, gf, bf;
    switch ((int)i % 6) {
        case 0:
            rf = v;
            gf = t;
            bf = p;
            break;
        case 1:
            rf = q;
            gf = v;
            bf = p;
            break;
        case 2:
            rf = p;
            gf = v;
            bf = t;
            break;
        case 3:
            rf = p;
            gf = q;
            bf = v;
            break;
        case 4:
            rf = t;
            gf = p;
            bf = v;
            break;
        default:
            rf = v;
            gf = p;
            bf = q;
            break;
    }
    *r = (Uint8)(rf * 255.0f + 0.5f);
    *g = (Uint8)(gf * 255.0f + 0.5f);
    *b = (Uint8)(bf * 255.0f + 0.5f);
}

// --- Icons (Lucide glyphs) ---------------------------------------------------

typedef enum {
    MS_ICON_PIN,
    MS_ICON_COPY,
    MS_ICON_SAVE,
    MS_ICON_DELETE,
    MS_ICON_CANCEL,
    MS_ICON_TRAY,
} ms_icon_kind;

// White Lucide glyph for `kind` at `size` points (rasterized at 2x for retina).
// Tint at draw time via texture color mod. Caller owns the texture.
SDL_Texture *ms_icon_make(SDL_Renderer *ren, ms_icon_kind kind, int size);

// Tray glyph (Lucide camera) at `px` pixels, white on transparent, as a new
// SDL_Surface for SDL_CreateTray. Caller owns it.
SDL_Surface *ms_icon_tray_surface(int px);

// --- Images ------------------------------------------------------------------

// Load a PNG into an SDL texture via stb_image. Returns 0 on success, fills
// *out, *w, *h.
int ms_image_load_texture(SDL_Renderer *r, const char *png_path,
                          SDL_Texture **out, int *w, int *h);

// Put a PNG file's contents onto the system clipboard. 0 on success.
int ms_clipboard_put_png(const char *png_path);

#endif

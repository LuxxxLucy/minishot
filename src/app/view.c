#include "view.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "config.h"
#include "geometry.h"
#include "naming.h"
#include "render.h"
#include "settings.h"
#include "theme.h"
#include "util.h"

// Layout (window points). A rounded card (image at top inside an animated
// border, toolbar below) floats on a soft drop shadow. The window carries
// a transparent margin around the card to hold the shadow halo.
#define MS_CARD_BORDER 3.0f
#define MS_CARD_GAP 5.0f
#define MS_CARD_MARGIN 22.0f
#define MS_CARD_RADIUS 3.0f
#define MS_CARD_RESIZE_EDGE 10.0f
#define MS_CARD_MIN_PX 32.0f  // smallest allowed image dimension (points)
#define MS_TOAST_SECS 1.6

#define MS_TB_N 4
#define MS_TB_BTN 22.0f  // square, icon-only
#define MS_TB_ICON 14.0f
#define MS_TB_GAP 3.0f
#define MS_TB_PAD 4.0f
#define MS_TB_RADIUS 10.0f  // toolbar pill corner radius
#define MS_BTN_RADIUS 8.0f  // toolbar button corner radius

// Drop shadow: one translucent rounded rect grown and offset down behind card.
#define MS_SHADOW_SPREAD 1.6f
#define MS_SHADOW_OFFSET_Y 4.0f

// Toast pill metrics (points).
#define MS_TOAST_RADIUS 6.0f
#define MS_TOAST_PAD_X 10.0f
#define MS_TOAST_PAD_Y 4.0f
#define MS_TOAST_TOP 10.0f  // gap from the image top edge

// Icon-pop animation (panim sinpulse: 0 -> 1 -> 0 over the duration).
#define MS_POP_DUR 0.28f
#define MS_POP_AMP 0.32f
#define MS_PIN_BUMP 1.10f  // persistent icon scale while pinned

#define MS_ZOOM_KEY 1.1f     // cmd +/- zoom step
#define MS_ZOOM_WHEEL 1.08f  // mouse-wheel zoom step
#define MS_TOAST_GRACE 0.1   // extra seconds to redraw once after a toast ends

enum { MS_PIN = 0, MS_COPY = 1, MS_SAVE = 2, MS_DELETE = 3 };
static const enum ms_icon_kind MS_TB_ICONK[MS_TB_N] = {
    MS_ICON_PIN, MS_ICON_COPY, MS_ICON_SAVE, MS_ICON_DELETE
};

// Transient toolbar message with an expiry; the clock is passed in.
struct ms_toast {
    char msg[256];
    double until;
    bool active;
};

static void ms_toast_show(struct ms_toast *t, const char *msg, double now,
                          double dur)
{
    SDL_strlcpy(t->msg, msg, sizeof(t->msg));
    t->until = now + dur;
    t->active = true;
}

static bool ms_toast_visible(const struct ms_toast *t, double now)
{
    return t->active && now < t->until;
}

struct ms_view {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;             // captured image
    SDL_Texture *icons[MS_TB_N];  // white glyphs, tinted at draw
    struct ms_size src_px;        // captured image size (pixels)
    struct ms_size disp;          // image display size (points)
    float scale;                  // display pixel density, for crisp text
    bool pinned;  // Pin button -> always-on-top + glowing border
    char png_path[1024];
    struct ms_toast toast;              // transient toolbar message timer
    struct ms_label_cache toast_label;  // cached render of the toast text
    // Animation: per-button pop clock (<0 = idle), hovered button index.
    float pop_t[MS_TB_N];
    int hover_btn;
    // Deferred-work flags set from the event watch (may be off-thread);
    // consumed on the main thread by ms_view_process.
    bool needs_redraw;
    bool pending_resize;
    bool pending_destroy;
    float resize_w, resize_h;
    struct ms_view *next;
};

static struct ms_view *g_views = NULL;
static bool g_watch_installed = false;
static Uint32 g_process_event = 0;

static double ms_now(void) { return (double)SDL_GetTicks() / 1000.0; }

static float ms_tb_w(void)
{
    return MS_TB_N * MS_TB_BTN + (MS_TB_N - 1) * MS_TB_GAP + 2 * MS_TB_PAD;
}
static float ms_tb_h(void) { return MS_TB_BTN + 2 * MS_TB_PAD; }

static float ms_content_w(const struct ms_view *v)
{
    float iw = v->disp.w + 2 * MS_CARD_BORDER;
    float tw = ms_tb_w();
    return iw > tw ? iw : tw;
}
static float ms_content_h(const struct ms_view *v)
{
    return MS_CARD_BORDER + v->disp.h + MS_CARD_GAP + ms_tb_h();
}
static float ms_win_w(const struct ms_view *v)
{
    return ms_content_w(v) + 2 * MS_CARD_MARGIN;
}
static float ms_win_h(const struct ms_view *v)
{
    return ms_content_h(v) + 2 * MS_CARD_MARGIN;
}

static SDL_FRect ms_card_rect(const struct ms_view *v)
{
    return (SDL_FRect){ MS_CARD_MARGIN, MS_CARD_MARGIN, ms_content_w(v),
                        ms_content_h(v) };
}
static SDL_FRect ms_image_rect(const struct ms_view *v)
{
    SDL_FRect c = ms_card_rect(v);
    return (SDL_FRect){ c.x + (c.w - v->disp.w) * 0.5f, c.y + MS_CARD_BORDER,
                        v->disp.w, v->disp.h };
}
static SDL_FRect ms_toolbar_rect(const struct ms_view *v)
{
    SDL_FRect img = ms_image_rect(v);
    float tw = ms_tb_w();
    // Right-aligned under the image.
    return (SDL_FRect){ img.x + img.w - tw, img.y + img.h + MS_CARD_GAP, tw,
                        ms_tb_h() };
}
static SDL_FRect ms_button_rect(const struct ms_view *v, int i)
{
    SDL_FRect tb = ms_toolbar_rect(v);
    return (SDL_FRect){ tb.x + MS_TB_PAD + (float)i * (MS_TB_BTN + MS_TB_GAP),
                        tb.y + MS_TB_PAD, MS_TB_BTN, MS_TB_BTN };
}

static void ms_view_show_toast(struct ms_view *v, const char *msg)
{
    ms_toast_show(&v->toast, msg, ms_now(), MS_TOAST_SECS);
}

static int ms_button_at(const struct ms_view *v, float px, float py)
{
    for (int i = 0; i < MS_TB_N; i++) {
        SDL_FRect b = ms_button_rect(v, i);
        if (px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h) {
            return i;
        }
    }
    return -1;
}

// Image body drags the window; toolbar buttons click; outer edge band resizes.
static SDL_HitTestResult SDLCALL ms_view_hit_test(SDL_Window *win,
                                                  const SDL_Point *area,
                                                  void *data)
{
    (void)win;
    struct ms_view *v = (struct ms_view *)data;
    float px = (float)area->x, py = (float)area->y;
    SDL_FRect c = ms_card_rect(v);
    float e = MS_CARD_RESIZE_EDGE;

    // Resize bands sit on the card edges, not the transparent margin.
    bool left = SDL_fabsf(px - c.x) <= e;
    bool right = SDL_fabsf(px - (c.x + c.w)) <= e;
    bool top = SDL_fabsf(py - c.y) <= e;
    bool bottom = SDL_fabsf(py - (c.y + c.h)) <= e;
    bool inx = px >= c.x - e && px <= c.x + c.w + e;
    bool iny = py >= c.y - e && py <= c.y + c.h + e;
    if (top && left) {
        return SDL_HITTEST_RESIZE_TOPLEFT;
    }
    if (top && right) {
        return SDL_HITTEST_RESIZE_TOPRIGHT;
    }
    if (bottom && left) {
        return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    }
    if (bottom && right) {
        return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    }
    if (left && iny) {
        return SDL_HITTEST_RESIZE_LEFT;
    }
    if (right && iny) {
        return SDL_HITTEST_RESIZE_RIGHT;
    }
    if (top && inx) {
        return SDL_HITTEST_RESIZE_TOP;
    }
    if (bottom && inx) {
        return SDL_HITTEST_RESIZE_BOTTOM;
    }

    if (ms_button_at(v, px, py) >= 0) {
        return SDL_HITTEST_NORMAL;
    }
    return SDL_HITTEST_DRAGGABLE;
}

// Animated border around the image: flowing rainbow when pinned, a
// mono shimmer on hover, a faint static hairline otherwise.
static void ms_draw_border(struct ms_view *v, SDL_FRect img, float tsec)
{
    if (v->pinned) {
        ms_draw_rainbow_border(v->ren, img, MS_CARD_BORDER, tsec);
        return;
    }
    SDL_FPoint O[4], I[4];
    ms_border_corners(img, MS_CARD_BORDER, O, I);
    bool hover = v->hover_btn >= 0;
    for (int e = 0; e < 4; e++) {
        int a = e, b = (e + 1) % 4;
        SDL_FColor ca, cb;
        if (hover) {
            float va =
                0.55f +
                0.45f * 0.5f * (1.0f + sinf((float)a * 1.57f - tsec * 3.0f));
            float vb =
                0.55f +
                0.45f * 0.5f * (1.0f + sinf((float)b * 1.57f - tsec * 3.0f));
            ca = (SDL_FColor){ 0.35f, 0.65f * va + 0.2f, va, 1.0f };
            cb = (SDL_FColor){ 0.35f, 0.65f * vb + 0.2f, vb, 1.0f };
        } else {
            ca = cb = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 0.18f };
        }
        ms_fill_quad(v->ren, O[a], O[b], I[b], I[a], ca, cb, cb, ca);
    }
}

static void ms_draw_button(struct ms_view *v, int i)
{
    SDL_FRect b = ms_button_rect(v, i);
    bool hover = v->hover_btn == i;
    bool on = (i == MS_PIN && v->pinned);

    // Background tint: bare when idle, subtle overlay on hover, accent for the
    // active Pin state.
    Clay_Color bg;
    if (on) {
        bg = MS_COL_BTN_ON;
    } else if (hover) {
        bg = MS_COL_BTN_HOVER;
    } else {
        bg = MS_COL_BTN_IDLE;
    }
    ms_draw_rounded_rect(v->ren, b, MS_BTN_RADIUS, bg);

    // Icon, scaled by the pop (and a small persistent bump when Pin is on).
    float pop = v->pop_t[i] >= 0.0f
                    ? 1.0f + MS_POP_AMP * ms_sinpulse(v->pop_t[i] / MS_POP_DUR)
                    : 1.0f;
    float base = on ? MS_PIN_BUMP : 1.0f;
    float s = MS_TB_ICON * base * pop;

    Clay_Color tint = MS_COL_ICON;
    if (i == MS_DELETE) {
        tint = MS_COL_ICON_DELETE;
    }
    if (on) {
        tint = MS_COL_ICON_ON;
    }

    if (v->icons[i]) {
        SDL_SetTextureColorMod(v->icons[i], (Uint8)tint.r, (Uint8)tint.g,
                               (Uint8)tint.b);
        SDL_FRect dst = { b.x + (b.w - s) * 0.5f, b.y + (b.h - s) * 0.5f, s,
                          s };
        SDL_RenderTexture(v->ren, v->icons[i], NULL, &dst);
    }
}

static void ms_view_draw(struct ms_view *v)
{
    double now = ms_now();
    float tsec = (float)now;
    SDL_SetRenderDrawBlendMode(v->ren, SDL_BLENDMODE_BLEND);

    // Transparent window; the card floats on a soft shadow.
    SDL_SetRenderDrawColor(v->ren, 0, 0, 0, 0);
    SDL_RenderClear(v->ren);

    SDL_FRect card = ms_card_rect(v);
    // A translucent rounded rect, grown and offset down behind the card, forms
    // a soft drop shadow.
    SDL_FRect sr = { card.x - MS_SHADOW_SPREAD,
                     card.y - MS_SHADOW_SPREAD + MS_SHADOW_OFFSET_Y,
                     card.w + 2 * MS_SHADOW_SPREAD,
                     card.h + 2 * MS_SHADOW_SPREAD };
    ms_draw_rounded_rect(v->ren, sr, MS_CARD_RADIUS + MS_SHADOW_SPREAD,
                         MS_COL_SHADOW);
    ms_draw_rounded_rect(v->ren, card, MS_CARD_RADIUS, MS_COL_CARD);

    SDL_FRect img = ms_image_rect(v);
    ms_draw_border(v, img, tsec);
    if (v->tex) {
        SDL_RenderTexture(v->ren, v->tex, NULL, &img);
    }

    // Toolbar strip background (rounded pill) below the image.
    SDL_FRect tb = ms_toolbar_rect(v);
    ms_draw_rounded_rect(v->ren, tb, MS_TB_RADIUS, MS_COL_TOOLBAR);
    for (int i = 0; i < MS_TB_N; i++) {
        ms_draw_button(v, i);
    }

    // Toast: centered pill near the top of the image. The text is rendered once
    // per message and cached, not rebuilt every frame.
    if (ms_toast_visible(&v->toast, now)) {
        // Rasterize at device resolution so the text stays crisp under the
        // window's 2x logical presentation.
        SDL_Texture *lt = ms_label_cache_get(
            v->ren, &v->toast_label, v->toast.msg, MS_LABEL_FONT_PT * v->scale);
        if (lt) {
            float tw = v->toast_label.w / v->scale,
                  th = v->toast_label.h / v->scale;
            SDL_FRect pill = { img.x + (img.w - tw) * 0.5f - MS_TOAST_PAD_X,
                               img.y + MS_TOAST_TOP, tw + 2 * MS_TOAST_PAD_X,
                               th + 2 * MS_TOAST_PAD_Y };
            ms_draw_rounded_rect(v->ren, pill, MS_TOAST_RADIUS, MS_COL_PILL);
            SDL_FRect dst = { pill.x + MS_TOAST_PAD_X, pill.y + MS_TOAST_PAD_Y,
                              tw, th };
            SDL_RenderTexture(v->ren, lt, NULL, &dst);
        }
    } else if (v->toast.active) {
        v->toast.active = false;
        ms_label_cache_free(&v->toast_label);
    }

    SDL_RenderPresent(v->ren);
}

static void ms_view_apply_size(struct ms_view *v, float target_w,
                               float target_h)
{
    float img_w = target_w - 2 * MS_CARD_MARGIN - 2 * MS_CARD_BORDER;
    float img_h = target_h - 2 * MS_CARD_MARGIN - MS_CARD_BORDER - MS_CARD_GAP -
                  ms_tb_h();
    v->disp = ms_aspect_contain_min(v->src_px.w, v->src_px.h, img_w, img_h,
                                    MS_CARD_MIN_PX);
    int ww = (int)ms_win_w(v), wh = (int)ms_win_h(v);
    SDL_SetWindowSize(v->win, ww, wh);
    SDL_SetRenderLogicalPresentation(v->ren, ww, wh,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

// Scale the image by `factor` via the deferred-resize path (thread-safe).
static void ms_view_request_zoom(struct ms_view *v, float factor)
{
    v->resize_w = ms_win_w(v) * factor;
    v->resize_h = ms_win_h(v) * factor;
    v->pending_resize = true;
    v->needs_redraw = true;
}

static void ms_view_do_save(struct ms_view *v)
{
    struct ms_config cfg;
    ms_config_load(&cfg);

    char dir[1024];
    ms_expand_path_from_home(dir, sizeof(dir), cfg.save_dir, SDL_getenv("HOME"));

    time_t t = time(NULL);
    struct tm lt;
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char name[128];
    if (ms_build_filename(name, sizeof(name), lt.tm_year + 1900, lt.tm_mon + 1,
                          lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec) < 0) {
        ms_view_show_toast(v, "Save failed");
        return;
    }
    char dest[1280];
    if (ms_join_path(dest, sizeof(dest), dir, name) < 0) {
        ms_view_show_toast(v, "Save failed");
        return;
    }
    SDL_CreateDirectory(dir);
    size_t sz = 0;
    void *bytes = SDL_LoadFile(v->png_path, &sz);
    bool ok = bytes && SDL_SaveFile(dest, bytes, sz);
    if (bytes) {
        SDL_free(bytes);
    }
    ms_view_show_toast(v, ok ? "Saved" : "Save failed");
}

static struct ms_view *ms_view_from_id(SDL_WindowID id)
{
    for (struct ms_view *p = g_views; p; p = p->next) {
        if (p->win && SDL_GetWindowID(p->win) == id) {
            return p;
        }
    }
    return NULL;
}

static void ms_view_handle_click(struct ms_view *v, float px, float py)
{
    int btn = ms_button_at(v, px, py);
    switch (btn) {
        case MS_PIN:
            v->pinned = !v->pinned;
            SDL_SetWindowAlwaysOnTop(v->win, v->pinned);
            v->pop_t[MS_PIN] = 0.0f;
            ms_view_show_toast(v, v->pinned ? "Pinned on top" : "Unpinned");
            break;
        case MS_COPY:
            v->pop_t[MS_COPY] = 0.0f;
            ms_view_show_toast(v, ms_clipboard_put_png(v->png_path) == 0
                                      ? "Copied"
                                      : "Copy failed");
            break;
        case MS_SAVE:
            v->pop_t[MS_SAVE] = 0.0f;
            ms_view_do_save(v);
            break;
        case MS_DELETE:
            v->pending_destroy = true;
            break;
        default:
            break;
    }
}

static void ms_view_process(SDL_WindowID id)
{
    struct ms_view *v = ms_view_from_id(id);
    if (!v) {
        return;
    }
    if (v->pending_destroy) {
        ms_view_destroy(v);
        return;
    }
    if (v->pending_resize) {
        v->pending_resize = false;
        ms_view_apply_size(v, v->resize_w, v->resize_h);
    }
    if (v->needs_redraw) {
        v->needs_redraw = false;
        ms_view_draw(v);
    }
}

static void ms_view_schedule(SDL_WindowID id)
{
    if (g_process_event == 0) {
        ms_view_process(id);
        return;
    }
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = g_process_event;
    ev.user.windowID = id;
    SDL_PushEvent(&ev);
}

static bool SDLCALL ms_view_event_watch(void *userdata, SDL_Event *ev)
{
    (void)userdata;
    if (g_process_event != 0 && ev->type == g_process_event) {
        ms_view_process(ev->user.windowID);
        return true;
    }
    switch (ev->type) {
        case SDL_EVENT_WINDOW_RESIZED: {
            struct ms_view *v = ms_view_from_id(ev->window.windowID);
            if (v) {
                v->resize_w = (float)ev->window.data1;
                v->resize_h = (float)ev->window.data2;
                v->pending_resize = true;
                v->needs_redraw = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_WINDOW_EXPOSED: {
            struct ms_view *v = ms_view_from_id(ev->window.windowID);
            if (v) {
                v->needs_redraw = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            struct ms_view *v = ms_view_from_id(ev->motion.windowID);
            if (v) {
                int hb = ms_button_at(v, ev->motion.x, ev->motion.y);
                if (hb != v->hover_btn) {
                    v->hover_btn = hb;
                    v->needs_redraw = true;
                    ms_view_schedule(ev->motion.windowID);
                }
            }
            break;
        }
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            struct ms_view *v = ms_view_from_id(ev->window.windowID);
            if (v && v->hover_btn != -1) {
                v->hover_btn = -1;
                v->needs_redraw = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (ev->button.button != SDL_BUTTON_LEFT) {
                break;
            }
            struct ms_view *v = ms_view_from_id(ev->button.windowID);
            if (v) {
                ms_view_handle_click(v, ev->button.x, ev->button.y);
                if (!v->pending_destroy) {
                    v->needs_redraw = true;
                }
                ms_view_schedule(ev->button.windowID);
            }
            break;
        }
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            struct ms_view *v = ms_view_from_id(ev->window.windowID);
            if (v) {
                v->pending_destroy = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            struct ms_view *v = ms_view_from_id(ev->key.windowID);
            if (!v) {
                break;
            }
            if (ev->key.key == SDLK_ESCAPE) {
                v->pending_destroy = true;
                ms_view_schedule(ev->key.windowID);
            } else if (ev->key.mod & SDL_KMOD_GUI) {  // cmd +/- zoom
                if (ev->key.key == SDLK_EQUALS || ev->key.key == SDLK_KP_PLUS) {
                    ms_view_request_zoom(v, MS_ZOOM_KEY);
                } else if (ev->key.key == SDLK_MINUS ||
                           ev->key.key == SDLK_KP_MINUS) {
                    ms_view_request_zoom(v, 1.0f / MS_ZOOM_KEY);
                } else {
                    break;
                }
                ms_view_schedule(ev->key.windowID);
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            struct ms_view *v = ms_view_from_id(ev->wheel.windowID);
            if (v && ev->wheel.y != 0.0f) {
                ms_view_request_zoom(
                    v, ev->wheel.y > 0 ? MS_ZOOM_WHEEL : 1.0f / MS_ZOOM_WHEEL);
                ms_view_schedule(ev->wheel.windowID);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

struct ms_view *ms_view_create(const char *png_path, int x, int y)
{
    if (!png_path) {
        return NULL;
    }

    struct ms_view *v = (struct ms_view *)SDL_calloc(1, sizeof(*v));
    if (!v) {
        return NULL;
    }
    SDL_strlcpy(v->png_path, png_path, sizeof(v->png_path));
    v->hover_btn = -1;
    for (int i = 0; i < MS_TB_N; i++) {
        v->pop_t[i] = -1.0f;
    }

    // Borderless, resizable, draggable from creation; NOT always-on-top until
    // the Pin button raises it. UTILITY keeps it out of the Dock/app switcher.
    SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE |
                            SDL_WINDOW_UTILITY | SDL_WINDOW_TRANSPARENT |
                            SDL_WINDOW_HIGH_PIXEL_DENSITY;
    v->win = SDL_CreateWindow("MiniShot Pin", 200, 200, flags);
    // Place so the image (inset by margin + border) overlays the captured
    // region.
    if (v->win) {
        int off = (int)(MS_CARD_MARGIN + MS_CARD_BORDER);
        SDL_SetWindowPosition(v->win, x - off, y - off);
    }
    v->ren = v->win ? SDL_CreateRenderer(v->win, NULL) : NULL;
    if (!v->win || !v->ren) {
        SDL_Log("view: window/renderer failed: %s", SDL_GetError());
        ms_view_destroy(v);
        return NULL;
    }

    int iw = 0, ih = 0;
    if (ms_image_load_texture(v->ren, png_path, &v->tex, &iw, &ih) != 0) {
        SDL_Log("view: image load failed: %s", png_path);
        ms_view_destroy(v);
        return NULL;
    }
    SDL_SetTextureScaleMode(v->tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(v->tex, SDL_BLENDMODE_NONE);

    for (int i = 0; i < MS_TB_N; i++) {
        v->icons[i] = ms_icon_make(v->ren, MS_TB_ICONK[i], (int)MS_TB_ICON);
    }

    float scale = SDL_GetWindowDisplayScale(v->win);
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    v->scale = scale;
    v->src_px = (struct ms_size){ (float)iw, (float)ih };
    v->disp = ms_px_to_points((float)iw, (float)ih, scale);
    SDL_Log("view: img %dx%d px, scale=%.2f, image=%.0fx%.0f pts, tex=%p", iw,
            ih, scale, v->disp.w, v->disp.h, (void *)v->tex);

    int ww = (int)ms_win_w(v), wh = (int)ms_win_h(v);
    SDL_SetWindowSize(v->win, ww, wh);
    SDL_SetRenderLogicalPresentation(v->ren, ww, wh,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetWindowHitTest(v->win, ms_view_hit_test, v);

    v->next = g_views;
    g_views = v;
    if (!g_watch_installed) {
        Uint32 evt = SDL_RegisterEvents(1);
        if (evt != (Uint32)-1) {
            g_process_event = evt;
        }
        SDL_AddEventWatch(ms_view_event_watch, NULL);
        g_watch_installed = true;
    }

    // Pop above the foreground app on creation
    SDL_ShowWindow(v->win);
    SDL_SetWindowAlwaysOnTop(v->win, true);
    SDL_RaiseWindow(v->win);
    SDL_SetWindowAlwaysOnTop(v->win, false);
    ms_view_draw(v);
    return v;
}

void ms_view_tick(double dt)
{
    double now = ms_now();
    for (struct ms_view *v = g_views; v; v = v->next) {
        bool animating = false;
        for (int i = 0; i < MS_TB_N; i++) {
            if (v->pop_t[i] >= 0.0f) {
                v->pop_t[i] += (float)dt;
                if (v->pop_t[i] >= MS_POP_DUR) {
                    v->pop_t[i] = -1.0f;
                } else {
                    animating = true;
                }
            }
        }
        // Pinned border and hover shimmer animate continuously; toast needs a
        // final redraw to clear. Otherwise stay idle (0% CPU).
        bool toast = v->toast.active && now < v->toast.until + MS_TOAST_GRACE;
        if (animating || v->pinned || v->hover_btn >= 0 || toast) {
            ms_view_draw(v);
        }
    }
}

void ms_view_destroy(struct ms_view *v)
{
    if (!v) {
        return;
    }
    for (struct ms_view **pp = &g_views; *pp; pp = &(*pp)->next) {
        if (*pp == v) {
            *pp = v->next;
            break;
        }
    }
    if (v->win) {
        SDL_SetWindowHitTest(v->win, NULL, NULL);
    }
    for (int i = 0; i < MS_TB_N; i++) {
        if (v->icons[i]) {
            SDL_DestroyTexture(v->icons[i]);
        }
    }
    ms_label_cache_free(&v->toast_label);
    if (v->tex) {
        SDL_DestroyTexture(v->tex);
    }
    if (v->ren) {
        SDL_DestroyRenderer(v->ren);
    }
    if (v->win) {
        SDL_DestroyWindow(v->win);
    }
    SDL_free(v);
}

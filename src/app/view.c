#include "view.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "config.h"
#include "naming.h"
#include "pinmodel.h"
#include "render.h"
#include "settings.h"
#include "toast.h"

// Layout (window points). A rounded card (image at top inside an animated
// border ring, toolbar below) floats on a soft drop shadow. The window carries
// a transparent margin around the card to hold the shadow halo.
#define MS_PIN_BORDER 3.0f
#define MS_PIN_GAP 5.0f
#define MS_PIN_MARGIN 22.0f
#define MS_CARD_RADIUS 12.0f
#define MS_PIN_RESIZE_EDGE 10.0f
#define MS_PIN_TOAST_SECS 1.6

#define MS_TB_N 4
#define MS_TB_BTN 22.0f  // square, icon-only
#define MS_TB_ICON 14.0f
#define MS_TB_GAP 3.0f
#define MS_TB_PAD 4.0f

// Icon-pop animation (panim sinpulse: 0 -> 1 -> 0 over the duration).
#define MS_HIT_DUR 0.28f
#define MS_HIT_AMP 0.32f

#define MS_ZOOM_KEY 1.1f     // cmd +/- zoom step
#define MS_ZOOM_WHEEL 1.08f  // mouse-wheel zoom step
#define MS_TOAST_GRACE 0.1   // extra seconds to redraw once after a toast ends

enum { MS_PIN = 0, MS_COPY = 1, MS_SAVE = 2, MS_DELETE = 3 };
static const ms_icon_kind MS_TB_ICONK[MS_TB_N] = { MS_ICON_PIN, MS_ICON_COPY,
                                                   MS_ICON_SAVE,
                                                   MS_ICON_DELETE };

struct ms_view {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;             // captured image
    SDL_Texture *icons[MS_TB_N];  // white glyphs, tinted at draw
    ms_pin pin;                   // image display size (points)
    float scale;                  // display pixel density, for crisp text
    bool pinned;                  // Pin button -> always-on-top + glowing ring
    char png_path[1024];
    ms_toast toast;          // transient toolbar message timer
    SDL_Texture *toast_tex;  // cached render of toast_tex_msg
    char toast_tex_msg[256];
    int toast_tw, toast_th;
    // Animation: per-button pop clock (<0 = idle), hovered button index.
    float hit_t[MS_TB_N];
    int hover_btn;
    // Deferred-work flags set from the event watch (may be off-thread);
    // consumed on the main thread by ms_view_process.
    bool needs_redraw;
    bool pending_resize;
    bool pending_destroy;
    float resize_w, resize_h;
    struct ms_view *next;
};

static ms_view *g_pins = NULL;
static bool g_watch_installed = false;
static Uint32 g_process_event = 0;

static double ms_now(void) { return (double)SDL_GetTicks() / 1000.0; }

static float ms_tb_w(void)
{
    return MS_TB_N * MS_TB_BTN + (MS_TB_N - 1) * MS_TB_GAP + 2 * MS_TB_PAD;
}
static float ms_tb_h(void) { return MS_TB_BTN + 2 * MS_TB_PAD; }

static float ms_content_w(const ms_view *pw)
{
    float iw = pw->pin.w + 2 * MS_PIN_BORDER;
    float tw = ms_tb_w();
    return iw > tw ? iw : tw;
}
static float ms_content_h(const ms_view *pw)
{
    return MS_PIN_BORDER + pw->pin.h + MS_PIN_GAP + ms_tb_h();
}
static float ms_win_w(const ms_view *pw)
{
    return ms_content_w(pw) + 2 * MS_PIN_MARGIN;
}
static float ms_win_h(const ms_view *pw)
{
    return ms_content_h(pw) + 2 * MS_PIN_MARGIN;
}

static SDL_FRect ms_card_rect(const ms_view *pw)
{
    return (SDL_FRect){ MS_PIN_MARGIN, MS_PIN_MARGIN, ms_content_w(pw),
                        ms_content_h(pw) };
}
static SDL_FRect ms_image_rect(const ms_view *pw)
{
    SDL_FRect c = ms_card_rect(pw);
    return (SDL_FRect){ c.x + (c.w - pw->pin.w) * 0.5f, c.y + MS_PIN_BORDER,
                        pw->pin.w, pw->pin.h };
}
static SDL_FRect ms_toolbar_rect(const ms_view *pw)
{
    SDL_FRect img = ms_image_rect(pw);
    float tw = ms_tb_w();
    // Right-aligned under the image.
    return (SDL_FRect){ img.x + img.w - tw, img.y + img.h + MS_PIN_GAP, tw,
                        ms_tb_h() };
}
static SDL_FRect ms_button_rect(const ms_view *pw, int i)
{
    SDL_FRect tb = ms_toolbar_rect(pw);
    return (SDL_FRect){ tb.x + MS_TB_PAD + (float)i * (MS_TB_BTN + MS_TB_GAP),
                        tb.y + MS_TB_PAD, MS_TB_BTN, MS_TB_BTN };
}

static void ms_view_show_toast(ms_view *pw, const char *msg)
{
    ms_toast_show(&pw->toast, msg, ms_now(), MS_PIN_TOAST_SECS);
}

static int ms_button_at(const ms_view *pw, float px, float py)
{
    for (int i = 0; i < MS_TB_N; i++) {
        SDL_FRect b = ms_button_rect(pw, i);
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
    ms_view *pw = (ms_view *)data;
    float px = (float)area->x, py = (float)area->y;
    SDL_FRect c = ms_card_rect(pw);
    float e = MS_PIN_RESIZE_EDGE;

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

    if (ms_button_at(pw, px, py) >= 0) {
        return SDL_HITTEST_NORMAL;
    }
    return SDL_HITTEST_DRAGGABLE;
}

// Animated illuminated ring around the image: flowing rainbow when pinned, a
// mono shimmer on hover, a faint static hairline otherwise.
static void ms_draw_border(ms_view *pw, SDL_FRect img, float tsec)
{
    float B = MS_PIN_BORDER;
    SDL_FPoint O[4] = { { img.x - B, img.y - B },
                        { img.x + img.w + B, img.y - B },
                        { img.x + img.w + B, img.y + img.h + B },
                        { img.x - B, img.y + img.h + B } };
    SDL_FPoint I[4] = { { img.x, img.y },
                        { img.x + img.w, img.y },
                        { img.x + img.w, img.y + img.h },
                        { img.x, img.y + img.h } };
    bool hover = pw->hover_btn >= 0;
    for (int e = 0; e < 4; e++) {
        int a = e, b = (e + 1) % 4;
        SDL_FColor ca, cb;
        if (pw->pinned) {
            Uint8 r1, g1, b1, r2, g2, b2;
            ms_hsv_rgb((float)a / 4.0f + tsec * 0.15f, 0.85f, 1.0f, &r1, &g1,
                       &b1);
            ms_hsv_rgb((float)b / 4.0f + tsec * 0.15f, 0.85f, 1.0f, &r2, &g2,
                       &b2);
            ca = (SDL_FColor){ r1 / 255.0f, g1 / 255.0f, b1 / 255.0f, 1.0f };
            cb = (SDL_FColor){ r2 / 255.0f, g2 / 255.0f, b2 / 255.0f, 1.0f };
        } else if (hover) {
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
        ms_fill_quad(pw->ren, O[a], O[b], I[b], I[a], ca, cb, cb, ca);
    }
}

static void ms_draw_button(ms_view *pw, int i)
{
    SDL_FRect b = ms_button_rect(pw, i);
    bool hover = pw->hover_btn == i;
    bool on = (i == MS_PIN && pw->pinned);

    // Background tint: bare when idle, subtle overlay on hover, accent for the
    // active Pin state.
    Clay_Color bg;
    if (on) {
        bg = (Clay_Color){ 10, 132, 255, 70 };
    } else if (hover) {
        bg = (Clay_Color){ 255, 255, 255, 24 };
    } else {
        bg = (Clay_Color){ 255, 255, 255, 10 };
    }
    ms_draw_rounded_rect(pw->ren, b, 8.0f, bg);

    // Icon, scaled by the pop (and a small persistent bump when Pin is on).
    float pop = pw->hit_t[i] >= 0.0f
                    ? 1.0f + MS_HIT_AMP * ms_sinpulse(pw->hit_t[i] / MS_HIT_DUR)
                    : 1.0f;
    float base = on ? 1.10f : 1.0f;
    float s = MS_TB_ICON * base * pop;

    Uint8 tr = 238, tg = 238, tb = 245;  // fg.icon
    if (i == MS_DELETE) {
        tr = 255;
        tg = 80;
        tb = 80;
    }
    if (on) {
        tr = 90;
        tg = 180;
        tb = 255;
    }

    if (pw->icons[i]) {
        SDL_SetTextureColorMod(pw->icons[i], tr, tg, tb);
        SDL_FRect dst = { b.x + (b.w - s) * 0.5f, b.y + (b.h - s) * 0.5f, s,
                          s };
        SDL_RenderTexture(pw->ren, pw->icons[i], NULL, &dst);
    }
}

static void ms_view_draw(ms_view *pw)
{
    double now = ms_now();
    float tsec = (float)now;
    SDL_SetRenderDrawBlendMode(pw->ren, SDL_BLENDMODE_BLEND);

    // Transparent window; the card floats on a soft shadow.
    SDL_SetRenderDrawColor(pw->ren, 0, 0, 0, 0);
    SDL_RenderClear(pw->ren);

    SDL_FRect card = ms_card_rect(pw);
    // Layered translucent rounded rects, offset down, form a soft drop shadow.
    for (int k = 9; k >= 1; k--) {
        float sp = (float)k * 1.6f;
        SDL_FRect sr = { card.x - sp, card.y - sp + 5.0f, card.w + 2 * sp,
                         card.h + 2 * sp };
        ms_draw_rounded_rect(pw->ren, sr, MS_CARD_RADIUS + sp,
                             (Clay_Color){ 0, 0, 0, 7 });
    }
    ms_draw_rounded_rect(pw->ren, card, MS_CARD_RADIUS,
                         (Clay_Color){ 28, 28, 32, 255 });

    SDL_FRect img = ms_image_rect(pw);
    ms_draw_border(pw, img, tsec);
    if (pw->tex) {
        SDL_RenderTexture(pw->ren, pw->tex, NULL, &img);
    }

    // Toolbar strip background (rounded pill) below the image.
    SDL_FRect tb = ms_toolbar_rect(pw);
    ms_draw_rounded_rect(pw->ren, tb, 10.0f, (Clay_Color){ 22, 22, 26, 235 });
    for (int i = 0; i < MS_TB_N; i++) {
        ms_draw_button(pw, i);
    }

    // Toast: centered pill near the top of the image. The text is rendered once
    // per message and cached, not rebuilt every frame.
    if (ms_toast_visible(&pw->toast, now)) {
        if (!pw->toast_tex ||
            SDL_strcmp(pw->toast_tex_msg, pw->toast.msg) != 0) {
            if (pw->toast_tex) {
                SDL_DestroyTexture(pw->toast_tex);
            }
            SDL_Color fg = { 255, 255, 255, 255 };
            // Rasterize at device resolution so the text stays crisp under the
            // window's 2x logical presentation.
            pw->toast_tex =
                ms_render_text_sized(pw->ren, pw->toast.msg, 13.0f * pw->scale,
                                     fg, &pw->toast_tw, &pw->toast_th);
            SDL_strlcpy(pw->toast_tex_msg, pw->toast.msg,
                        sizeof(pw->toast_tex_msg));
        }
        if (pw->toast_tex) {
            float tw = pw->toast_tw / pw->scale, th = pw->toast_th / pw->scale;
            SDL_FRect pill = { img.x + (img.w - tw) * 0.5f - 10.0f,
                               img.y + 10.0f, tw + 20.0f, th + 8.0f };
            ms_draw_rounded_rect(pw->ren, pill, 6.0f,
                                 (Clay_Color){ 28, 28, 32, 224 });
            SDL_FRect dst = { pill.x + 10.0f, pill.y + 4.0f, tw, th };
            SDL_RenderTexture(pw->ren, pw->toast_tex, NULL, &dst);
        }
    } else if (pw->toast.active) {
        pw->toast.active = 0;
        if (pw->toast_tex) {
            SDL_DestroyTexture(pw->toast_tex);
            pw->toast_tex = NULL;
            pw->toast_tex_msg[0] = '\0';
        }
    }

    SDL_RenderPresent(pw->ren);
}

static void ms_view_apply_size(ms_view *pw, float target_w, float target_h)
{
    float img_w = target_w - 2 * MS_PIN_MARGIN - 2 * MS_PIN_BORDER;
    float img_h =
        target_h - 2 * MS_PIN_MARGIN - MS_PIN_BORDER - MS_PIN_GAP - ms_tb_h();
    ms_pin_resize(&pw->pin, img_w, img_h);
    int ww = (int)ms_win_w(pw), wh = (int)ms_win_h(pw);
    SDL_SetWindowSize(pw->win, ww, wh);
    SDL_SetRenderLogicalPresentation(pw->ren, ww, wh,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

// Scale the image by `factor` via the deferred-resize path (thread-safe).
static void ms_view_request_zoom(ms_view *pw, float factor)
{
    pw->resize_w = ms_win_w(pw) * factor;
    pw->resize_h = ms_win_h(pw) * factor;
    pw->pending_resize = true;
    pw->needs_redraw = true;
}

static void ms_view_do_save(ms_view *pw)
{
    ms_config cfg;
    ms_config_load(&cfg);

    const char *home = SDL_getenv("HOME");
    char dir[1024];
    if (cfg.save_dir[0] == '~' && home) {
        SDL_snprintf(dir, sizeof(dir), "%s%s", home, cfg.save_dir + 1);
    } else {
        SDL_strlcpy(dir, cfg.save_dir, sizeof(dir));
    }

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
        ms_view_show_toast(pw, "Save failed");
        return;
    }
    char dest[1280];
    if (ms_join_path(dest, sizeof(dest), dir, name) < 0) {
        ms_view_show_toast(pw, "Save failed");
        return;
    }
    SDL_CreateDirectory(dir);
    size_t sz = 0;
    void *bytes = SDL_LoadFile(pw->png_path, &sz);
    bool ok = bytes && SDL_SaveFile(dest, bytes, sz);
    if (bytes) {
        SDL_free(bytes);
    }
    ms_view_show_toast(pw, ok ? "Saved" : "Save failed");
}

static ms_view *ms_view_from_id(SDL_WindowID id)
{
    for (ms_view *p = g_pins; p; p = p->next) {
        if (p->win && SDL_GetWindowID(p->win) == id) {
            return p;
        }
    }
    return NULL;
}

static void ms_view_handle_click(ms_view *pw, float px, float py)
{
    int btn = ms_button_at(pw, px, py);
    switch (btn) {
        case MS_PIN:
            pw->pinned = !pw->pinned;
            SDL_SetWindowAlwaysOnTop(pw->win, pw->pinned);
            pw->hit_t[MS_PIN] = 0.0f;
            ms_view_show_toast(pw, pw->pinned ? "Pinned on top" : "Unpinned");
            break;
        case MS_COPY:
            pw->hit_t[MS_COPY] = 0.0f;
            ms_view_show_toast(pw, ms_clipboard_put_png(pw->png_path) == 0
                                       ? "Copied"
                                       : "Copy failed");
            break;
        case MS_SAVE:
            pw->hit_t[MS_SAVE] = 0.0f;
            ms_view_do_save(pw);
            break;
        case MS_DELETE:
            pw->pending_destroy = true;
            break;
        default:
            break;
    }
}

static void ms_view_process(SDL_WindowID id)
{
    ms_view *pw = ms_view_from_id(id);
    if (!pw) {
        return;
    }
    if (pw->pending_destroy) {
        ms_view_destroy(pw);
        return;
    }
    if (pw->pending_resize) {
        pw->pending_resize = false;
        ms_view_apply_size(pw, pw->resize_w, pw->resize_h);
    }
    if (pw->needs_redraw) {
        pw->needs_redraw = false;
        ms_view_draw(pw);
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
            ms_view *pw = ms_view_from_id(ev->window.windowID);
            if (pw) {
                pw->resize_w = (float)ev->window.data1;
                pw->resize_h = (float)ev->window.data2;
                pw->pending_resize = true;
                pw->needs_redraw = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_WINDOW_EXPOSED: {
            ms_view *pw = ms_view_from_id(ev->window.windowID);
            if (pw) {
                pw->needs_redraw = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            ms_view *pw = ms_view_from_id(ev->motion.windowID);
            if (pw) {
                int hb = ms_button_at(pw, ev->motion.x, ev->motion.y);
                if (hb != pw->hover_btn) {
                    pw->hover_btn = hb;
                    pw->needs_redraw = true;
                    ms_view_schedule(ev->motion.windowID);
                }
            }
            break;
        }
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            ms_view *pw = ms_view_from_id(ev->window.windowID);
            if (pw && pw->hover_btn != -1) {
                pw->hover_btn = -1;
                pw->needs_redraw = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (ev->button.button != SDL_BUTTON_LEFT) {
                break;
            }
            ms_view *pw = ms_view_from_id(ev->button.windowID);
            if (pw) {
                ms_view_handle_click(pw, ev->button.x, ev->button.y);
                if (!pw->pending_destroy) {
                    pw->needs_redraw = true;
                }
                ms_view_schedule(ev->button.windowID);
            }
            break;
        }
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            ms_view *pw = ms_view_from_id(ev->window.windowID);
            if (pw) {
                pw->pending_destroy = true;
                ms_view_schedule(ev->window.windowID);
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            ms_view *pw = ms_view_from_id(ev->key.windowID);
            if (!pw) {
                break;
            }
            if (ev->key.key == SDLK_ESCAPE) {
                pw->pending_destroy = true;
                ms_view_schedule(ev->key.windowID);
            } else if (ev->key.mod & SDL_KMOD_GUI) {  // cmd +/- zoom
                if (ev->key.key == SDLK_EQUALS || ev->key.key == SDLK_KP_PLUS) {
                    ms_view_request_zoom(pw, MS_ZOOM_KEY);
                } else if (ev->key.key == SDLK_MINUS ||
                           ev->key.key == SDLK_KP_MINUS) {
                    ms_view_request_zoom(pw, 1.0f / MS_ZOOM_KEY);
                } else {
                    break;
                }
                ms_view_schedule(ev->key.windowID);
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            ms_view *pw = ms_view_from_id(ev->wheel.windowID);
            if (pw && ev->wheel.y != 0.0f) {
                ms_view_request_zoom(
                    pw, ev->wheel.y > 0 ? MS_ZOOM_WHEEL : 1.0f / MS_ZOOM_WHEEL);
                ms_view_schedule(ev->wheel.windowID);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

ms_view *ms_view_create(const char *png_path, int x, int y)
{
    if (!png_path) {
        return NULL;
    }

    ms_view *pw = (ms_view *)SDL_calloc(1, sizeof(*pw));
    if (!pw) {
        return NULL;
    }
    SDL_strlcpy(pw->png_path, png_path, sizeof(pw->png_path));
    pw->hover_btn = -1;
    for (int i = 0; i < MS_TB_N; i++) {
        pw->hit_t[i] = -1.0f;
    }

    // Borderless, resizable, draggable from creation; NOT always-on-top until
    // the Pin button raises it. UTILITY keeps it out of the Dock/app switcher.
    SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE |
                            SDL_WINDOW_UTILITY | SDL_WINDOW_TRANSPARENT |
                            SDL_WINDOW_HIGH_PIXEL_DENSITY;
    pw->win = SDL_CreateWindow("MiniShot Pin", 200, 200, flags);
    // Place so the image (inset by margin + border) overlays the captured
    // region.
    if (pw->win) {
        int off = (int)(MS_PIN_MARGIN + MS_PIN_BORDER);
        SDL_SetWindowPosition(pw->win, x - off, y - off);
    }
    pw->ren = pw->win ? SDL_CreateRenderer(pw->win, NULL) : NULL;
    if (!pw->win || !pw->ren) {
        SDL_Log("view: window/renderer failed: %s", SDL_GetError());
        ms_view_destroy(pw);
        return NULL;
    }

    int iw = 0, ih = 0;
    if (ms_image_load_texture(pw->ren, png_path, &pw->tex, &iw, &ih) != 0) {
        SDL_Log("view: image load failed: %s", png_path);
        ms_view_destroy(pw);
        return NULL;
    }
    SDL_SetTextureScaleMode(pw->tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(pw->tex, SDL_BLENDMODE_NONE);

    for (int i = 0; i < MS_TB_N; i++) {
        pw->icons[i] = ms_icon_make(pw->ren, MS_TB_ICONK[i], (int)MS_TB_ICON);
    }

    float scale = SDL_GetWindowDisplayScale(pw->win);
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    pw->scale = scale;
    pw->pin = ms_pin_create((float)iw, (float)ih, scale, 0.0f, 0.0f);
    SDL_Log("view: img %dx%d px, scale=%.2f, image=%.0fx%.0f pts, tex=%p", iw,
            ih, scale, pw->pin.w, pw->pin.h, (void *)pw->tex);

    int ww = (int)ms_win_w(pw), wh = (int)ms_win_h(pw);
    SDL_SetWindowSize(pw->win, ww, wh);
    SDL_SetRenderLogicalPresentation(pw->ren, ww, wh,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetWindowHitTest(pw->win, ms_view_hit_test, pw);

    pw->next = g_pins;
    g_pins = pw;
    if (!g_watch_installed) {
        Uint32 evt = SDL_RegisterEvents(1);
        if (evt != (Uint32)-1) {
            g_process_event = evt;
        }
        SDL_AddEventWatch(ms_view_event_watch, NULL);
        g_watch_installed = true;
    }

    SDL_ShowWindow(pw->win);
    SDL_RaiseWindow(pw->win);
    ms_view_draw(pw);
    return pw;
}

void ms_view_tick(double dt)
{
    for (ms_view *pw = g_pins; pw; pw = pw->next) {
        bool animating = false;
        for (int i = 0; i < MS_TB_N; i++) {
            if (pw->hit_t[i] >= 0.0f) {
                pw->hit_t[i] += (float)dt;
                if (pw->hit_t[i] >= MS_HIT_DUR) {
                    pw->hit_t[i] = -1.0f;
                } else {
                    animating = true;
                }
            }
        }
        // Pinned ring and hover shimmer animate continuously; toast needs a
        // final redraw to clear. Otherwise stay idle (0% CPU).
        bool toast =
            pw->toast.active && ms_now() < pw->toast.until + MS_TOAST_GRACE;
        if (animating || pw->pinned || pw->hover_btn >= 0 || toast) {
            ms_view_draw(pw);
        }
    }
}

void ms_view_destroy(ms_view *pw)
{
    if (!pw) {
        return;
    }
    for (ms_view **pp = &g_pins; *pp; pp = &(*pp)->next) {
        if (*pp == pw) {
            *pp = pw->next;
            break;
        }
    }
    if (pw->win) {
        SDL_SetWindowHitTest(pw->win, NULL, NULL);
    }
    for (int i = 0; i < MS_TB_N; i++) {
        if (pw->icons[i]) {
            SDL_DestroyTexture(pw->icons[i]);
        }
    }
    if (pw->toast_tex) {
        SDL_DestroyTexture(pw->toast_tex);
    }
    if (pw->tex) {
        SDL_DestroyTexture(pw->tex);
    }
    if (pw->ren) {
        SDL_DestroyRenderer(pw->ren);
    }
    if (pw->win) {
        SDL_DestroyWindow(pw->win);
    }
    SDL_free(pw);
}

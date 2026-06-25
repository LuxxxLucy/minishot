#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "clay.h"
#include "render.h"

#include "assets.h"
#include "capture.h"
#include "config.h"
#include "geometry.h"
#include "hotkey.h"
#include "overlay.h"
#include "view.h"
#include "settings.h"

#define MS_FRAME_DELAY_MS 16  // ~60 fps main-loop pacing

// Log to a file as well as stderr, so a GUI launch leaves a trail to inspect.
static FILE *g_log_file = NULL;

static void ms_log_output(void *ud, int category, SDL_LogPriority pri,
                          const char *msg)
{
    (void)ud;
    (void)category;
    if (g_log_file) {
        fprintf(g_log_file, "[%d] %s\n", (int)pri, msg);
        fflush(g_log_file);
    }
    fprintf(stderr, "%s\n", msg);
}

static void ms_log_init(void)
{
    const char *path = MS_LOG_PATH;
    g_log_file = fopen(path, "w");
    SDL_SetLogOutputFunction(ms_log_output, NULL);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    SDL_Log("MiniShot starting; log file: %s", path);
}

// Custom SDL event: a capture was requested (hotkey or tray "Capture").
static Uint32 g_capture_event = 0;

// ----------------------------------------------------------------------------
// Tray.
// ----------------------------------------------------------------------------

static void post_capture_event(void)
{
    if (g_capture_event == 0) {
        return;
    }
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = g_capture_event;
    SDL_PushEvent(&ev);
}

static void hotkey_cb(void *ud)
{
    (void)ud;
    post_capture_event();
}

static void capture_cb(void *userdata, SDL_TrayEntry *entry)
{
    (void)userdata;
    (void)entry;
    post_capture_event();
}

static void settings_cb(void *userdata, SDL_TrayEntry *entry)
{
    (void)userdata;
    (void)entry;
    ms_settings_open();
}

static void quit_cb(void *userdata, SDL_TrayEntry *entry)
{
    (void)userdata;
    (void)entry;
    SDL_Event quit = { .type = SDL_EVENT_QUIT };
    SDL_PushEvent(&quit);
}

#define MS_TRAY_ICON_PX 36
#define MS_FALLBACK_ICON_PX 22
#define MS_FALLBACK_ICON_RGBA 240, 240, 245, 255

static SDL_Surface *make_icon(void)
{
    // Lucide camera glyph; macOS renders it as a monochrome menu-bar template.
    SDL_Surface *s = ms_icon_tray_surface(MS_TRAY_ICON_PX);
    if (s) {
        return s;
    }
    // Fallback: a plain dot if the icon font failed to load.
    s = SDL_CreateSurface(MS_FALLBACK_ICON_PX, MS_FALLBACK_ICON_PX,
                          SDL_PIXELFORMAT_RGBA32);
    if (s) {
        SDL_FillSurfaceRect(s, NULL,
                            SDL_MapSurfaceRGBA(s, MS_FALLBACK_ICON_RGBA));
    }
    return s;
}

static void add_tray_button(SDL_TrayMenu *menu, const char *label,
                            SDL_TrayCallback cb)
{
    SDL_TrayEntry *e =
        SDL_InsertTrayEntryAt(menu, -1, label, SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(e, cb, NULL);
}

static SDL_Tray *make_tray(void)
{
    SDL_Surface *icon = make_icon();
    SDL_Tray *tray = SDL_CreateTray(icon, "MiniShot");
    if (icon) {
        SDL_DestroySurface(icon);
    }
    if (!tray) {
        return NULL;
    }

    SDL_TrayMenu *menu = SDL_CreateTrayMenu(tray);
    add_tray_button(menu, "Capture", capture_cb);
    SDL_InsertTrayEntryAt(menu, -1, NULL, SDL_TRAYENTRY_BUTTON);  // separator
    add_tray_button(menu, "Settings", settings_cb);
    add_tray_button(menu, "Quit", quit_cb);
    return tray;
}

// ----------------------------------------------------------------------------
// Capture flow: overlay -> screencapture -> floating view window.
// Runs synchronously start-to-finish; the overlay blocks until the user
// finishes or cancels.
// ----------------------------------------------------------------------------

static void ms_run_capture_flow(void)
{
    SDL_Log("capture flow: begin");
    struct ms_overlay_result sel = ms_overlay_run();
    SDL_Log("overlay: cancelled=%d rect=(%.0f,%.0f %.0fx%.0f)", sel.cancelled,
            sel.rect.x, sel.rect.y, sel.rect.w, sel.rect.h);
    if (sel.cancelled) {
        return;
    }

    int x = (int)(sel.rect.x + 0.5f);
    int y = (int)(sel.rect.y + 0.5f);
    int w = (int)(sel.rect.w + 0.5f);
    int h = (int)(sel.rect.h + 0.5f);
    if (w < 1 || h < 1) {
        SDL_Log("selection too small (%dx%d); aborting", w, h);
        return;
    }

    char tmp_png[1024];
    SDL_snprintf(tmp_png, sizeof(tmp_png), MS_TMP_CAPTURE_FMT,
                 (unsigned long long)SDL_GetTicksNS());

    int rc = ms_capture_run(x, y, w, h, tmp_png);
    SDL_Log("capture: rc=%d path=%s", rc, tmp_png);
    if (rc != 0) {
        SDL_Log("capture failed (rc=%d)", rc);
        SDL_RemovePath(tmp_png);  // may have left a partial file
        return;
    }

    // The captured region appears immediately as a floating, draggable window
    // placed over the selection. It owns tmp_png for its copy/save buttons; on
    // failure we delete the temp file to avoid leaking one per capture.
    struct ms_view *v = ms_view_create(tmp_png, x, y);
    SDL_Log("view create: %s", v ? "ok" : "FAILED");
    if (!v) {
        SDL_RemovePath(tmp_png);
    }
}

// ----------------------------------------------------------------------------
// main.
// ----------------------------------------------------------------------------

static void handle_clay_errors(Clay_ErrorData err)
{
    SDL_Log("clay error: %s", err.errorText.chars);
}

// Load the Lucide icon font from next to the binary or the app's Resources.
// Downloaded by build.sh; never committed.
static int ms_init_icon_font(void)
{
    const char *base = SDL_GetBasePath();
    if (!base) {
        return -1;
    }
    char p[1200];
    SDL_snprintf(p, sizeof(p), "%s%s", base, MS_ICON_FONT_FILE);
    if (ms_render_init_icon_font(p) == 0) {
        return 0;
    }
    SDL_snprintf(p, sizeof(p), "%s%s%s", base, MS_RESOURCES_DIR,
                 MS_ICON_FONT_FILE);
    return ms_render_init_icon_font(p);
}

int main(void)
{
    // hint SDL to not show the app in the Dock, but rather as a background app
    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    ms_log_init();

    // Borderless overlay/menu windows must take the focusing click as a real
    // event and become key on show, so the first press-drag draws a selection
    // immediately instead of only activating the window.
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, "1");

    g_capture_event = SDL_RegisterEvents(1);
    if (g_capture_event == (Uint32)-1) {
        g_capture_event = 0;
    }

    struct ms_config cfg;
    ms_config_load(&cfg);

    // Shared Clay arena + context for the app (tray/action menus create their
    // own contexts; this one anchors text measurement / a default context).
    uint64_t mem = Clay_MinMemorySize();
    Clay_Arena arena =
        (Clay_Arena){ .memory = SDL_malloc(mem), .capacity = mem };
    Clay_Initialize(arena, (Clay_Dimensions){ 1.0f, 1.0f },
                    (Clay_ErrorHandler){ handle_clay_errors, NULL });

    // UI font from config, falling back to the bundled default if a configured
    // font fails to load.
    if (cfg.font_path[0] &&
        ms_render_init_font(cfg.font_path, MS_UI_FONT_SIZE) != 0) {
        SDL_Log("font '%s' failed; falling back to %s", cfg.font_path,
                MS_UI_FONT_PATH);
        cfg.font_path[0] = '\0';
    }
    if (!cfg.font_path[0] &&
        ms_render_init_font(MS_UI_FONT_PATH, MS_UI_FONT_SIZE) != 0) {
        SDL_Log("font init failed (%s); text will not render", MS_UI_FONT_PATH);
    }
    Clay_SetMeasureTextFunction(ms_render_measure_text, NULL);

    if (ms_init_icon_font() != 0) {
        SDL_Log("icon font init failed; icons will not render");
    }

    SDL_Tray *tray = make_tray();
    if (!tray) {
        SDL_Log("tray failed: %s", SDL_GetError());
    }

    // Register the global hotkey via Carbon. If it does not register, the tray
    // "Capture" item posts the same event.
    struct ms_hotkey hk = ms_hotkey_parse(cfg.hotkey);
    if (hk.ok) {
        if (ms_hotkey_register(hk, hotkey_cb, NULL) != 0) {
            SDL_Log("hotkey '%s' not registered; use tray Capture", cfg.hotkey);
        }
    } else {
        SDL_Log("hotkey '%s' did not parse", cfg.hotkey);
    }

    bool running = true;
    Uint64 last = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (g_capture_event != 0 && ev.type == g_capture_event) {
                ms_run_capture_flow();
            }
            // View events are routed by view's own SDL_AddEventWatch.
        }
        Uint64 now = SDL_GetTicks();
        ms_view_tick((double)(now - last) / 1000.0);  // drive view animations
        last = now;
        SDL_Delay(MS_FRAME_DELAY_MS);
    }

    if (tray) {
        SDL_DestroyTray(tray);
    }
    SDL_free(arena.memory);
    SDL_Quit();
    return 0;
}

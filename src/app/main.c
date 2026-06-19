#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "clay.h"
#include "render.h"

#include "appstate.h"
#include "assets.h"
#include "capture.h"
#include "config.h"
#include "geometry.h"
#include "hotkey.h"
#include "overlay.h"
#include "view.h"
#include "settings.h"

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
    const char *path = "/tmp/minishot.log";
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

static SDL_Surface *make_icon(void)
{
    // Lucide camera glyph; macOS renders it as a monochrome menu-bar template.
    SDL_Surface *s = ms_icon_tray_surface(36);
    if (s) {
        return s;
    }
    // Fallback: a plain dot if the icon font failed to load.
    s = SDL_CreateSurface(22, 22, SDL_PIXELFORMAT_RGBA32);
    if (s) {
        SDL_FillSurfaceRect(s, NULL, SDL_MapSurfaceRGBA(s, 240, 240, 245, 255));
    }
    return s;
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
    SDL_TrayEntry *cap =
        SDL_InsertTrayEntryAt(menu, -1, "Capture", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(cap, capture_cb, NULL);
    SDL_InsertTrayEntryAt(menu, -1, NULL, SDL_TRAYENTRY_BUTTON);  // separator
    SDL_TrayEntry *set =
        SDL_InsertTrayEntryAt(menu, -1, "Settings", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(set, settings_cb, NULL);
    SDL_TrayEntry *quit =
        SDL_InsertTrayEntryAt(menu, -1, "Quit", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(quit, quit_cb, NULL);
    return tray;
}

// ----------------------------------------------------------------------------
// Capture flow: overlay -> screencapture -> floating pin window.
// Driven by the IDLE -> SELECTING -> ACTION_MENU -> IDLE state machine.
// ----------------------------------------------------------------------------

static void ms_run_capture_flow(ms_app_state *state)
{
    SDL_Log("capture flow: begin");
    *state = ms_app_next(*state, MINISHOT_EV_HOTKEY);  // IDLE -> SELECTING
    if (*state != MINISHOT_SELECTING) {
        return;
    }

    ms_overlay_result sel = ms_overlay_run();
    SDL_Log("overlay: cancelled=%d rect=(%.0f,%.0f %.0fx%.0f)", sel.cancelled,
            sel.rect.x, sel.rect.y, sel.rect.w, sel.rect.h);
    if (sel.cancelled) {
        *state = ms_app_next(*state, MINISHOT_EV_SELECT_CANCEL);  // -> IDLE
        return;
    }
    *state = ms_app_next(*state, MINISHOT_EV_SELECT_DONE);  // -> ACTION_MENU

    int x = (int)(sel.rect.x + 0.5f);
    int y = (int)(sel.rect.y + 0.5f);
    int w = (int)(sel.rect.w + 0.5f);
    int h = (int)(sel.rect.h + 0.5f);
    if (w < 1 || h < 1) {
        SDL_Log("selection too small (%dx%d); aborting", w, h);
        *state = ms_app_next(*state, MINISHOT_EV_SELECT_CANCEL);
        return;
    }

    char tmp_png[1024];
    SDL_snprintf(tmp_png, sizeof(tmp_png), "/tmp/minishot-%llu.png",
                 (unsigned long long)SDL_GetTicksNS());

    int rc = ms_capture_run(MINISHOT_CAP_FILE, x, y, w, h, tmp_png);
    SDL_Log("capture: rc=%d path=%s", rc, tmp_png);
    if (rc != 0) {
        SDL_Log("capture failed (rc=%d)", rc);
        SDL_RemovePath(tmp_png);  // may have left a partial file
        *state = ms_app_next(*state, MINISHOT_EV_SELECT_CANCEL);
        return;
    }

    // The captured region appears immediately as a floating, draggable window
    // placed over the selection. It owns tmp_png for its copy/save buttons; on
    // failure we delete the temp file to avoid leaking one per capture.
    ms_view *pw = ms_view_create(tmp_png, x, y);
    SDL_Log("pin window create: %s", pw ? "ok" : "FAILED");
    if (!pw) {
        SDL_RemovePath(tmp_png);
    }

    *state = ms_app_next(*state, MINISHOT_EV_ACTION_CHOSEN);  // -> IDLE
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

    ms_config cfg;
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

    // Register the global hotkey. The Carbon-backed registration is a stub for
    // now (returns -1); the tray "Capture" item posts the same event so the
    // flow is fully exercisable meanwhile.
    ms_hotkey hk = ms_hotkey_parse(cfg.hotkey);
    if (hk.ok) {
        if (ms_hotkey_register(hk, hotkey_cb, NULL) != 0) {
            SDL_Log(
                "hotkey '%s' not registered (Carbon stub); use tray Capture",
                cfg.hotkey);
        }
    } else {
        SDL_Log("hotkey '%s' did not parse", cfg.hotkey);
    }

    ms_app_state state = MINISHOT_IDLE;

    bool running = true;
    Uint64 last = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (g_capture_event != 0 && ev.type == g_capture_event) {
                if (state == MINISHOT_IDLE) {
                    ms_run_capture_flow(&state);
                }
            }
            // Pin-window events are routed by view's own SDL_AddEventWatch.
        }
        Uint64 now = SDL_GetTicks();
        ms_view_tick((double)(now - last) / 1000.0);  // drive pin animations
        last = now;
        SDL_Delay(16);
    }

    if (tray) {
        SDL_DestroyTray(tray);
    }
    SDL_free(arena.memory);
    SDL_Quit();
    return 0;
}

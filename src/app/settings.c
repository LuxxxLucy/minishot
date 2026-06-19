#include "settings.h"

#include <SDL3/SDL.h>

#include "clay.h"
#include "render.h"
#include "assets.h"
#include "config.h"

// Config persistence lives here: config.h declares parse/serialize but no file
// IO, so the load/save against $HOME/MS_CONFIG_FILE live in the app layer.
#define MS_SETTINGS_FONT_ID 0
#define MS_SETTINGS_WIN_W 460
#define MS_SETTINGS_WIN_H 240

// Resolve the config file path ("$HOME/MS_CONFIG_FILE") into buf.
// Returns 0 on success, -1 if no home directory is known.
static int ms_config_path(char *buf, size_t n)
{
    const char *home = SDL_getenv("HOME");
    if (!home || !home[0]) {
        return -1;
    }
    int written = SDL_snprintf(buf, n, "%s/%s", home, MS_CONFIG_FILE);
    if (written < 0 || (size_t)written >= n) {
        return -1;
    }
    return 0;
}

void ms_config_load(ms_config *out)
{
    *out = ms_config_default();

    char path[1200];
    if (ms_config_path(path, sizeof(path)) != 0) {
        return;
    }
    size_t size = 0;
    char *text = (char *)SDL_LoadFile(path, &size);
    if (!text) {
        return;
    }
    ms_config parsed = ms_config_default();
    if (ms_config_parse(&parsed, text) == 0) {
        *out = parsed;
    }
    SDL_free(text);
}

int ms_config_save(const ms_config *cfg)
{
    char path[1200];
    if (ms_config_path(path, sizeof(path)) != 0) {
        return -1;
    }
    char text[2048];
    int len = ms_config_serialize(cfg, text, sizeof(text));
    if (len < 0) {
        return -1;
    }
    return SDL_SaveFile(path, text, (size_t)len) ? 0 : -1;
}

// Build a non-static Clay_String over an existing char buffer. The buffer must
// outlive the layout pass that consumes the returned string.
static Clay_String ms_settings_dyn_string(const char *s)
{
    return (Clay_String){
        .isStaticallyAllocated = false,
        .length = (int32_t)SDL_strlen(s),
        .chars = s,
    };
}

// State shared with the folder-dialog callback.
typedef struct {
    ms_config *cfg;
    bool dirty;
    bool dialog_open;
} ms_settings_state;

static void SDLCALL ms_settings_folder_cb(void *userdata,
                                          const char *const *filelist,
                                          int filter)
{
    (void)filter;
    ms_settings_state *st = (ms_settings_state *)userdata;
    st->dialog_open = false;
    if (!filelist || !filelist[0]) {
        return;  // cancelled or error
    }
    SDL_strlcpy(st->cfg->save_dir, filelist[0], sizeof(st->cfg->save_dir));
    st->dirty = true;
}

static void ms_settings_handle_errors(Clay_ErrorData err)
{
    SDL_Log("settings clay error: %s", err.errorText.chars);
}

// Lay out the settings panel for the current frame. The Clay_Strings built over
// cfg->save_dir / cfg->hotkey only need to outlive Clay_EndLayout() below, and
// cfg outlives this call, so no intermediate copy buffers are needed.
static Clay_RenderCommandArray ms_settings_build(const ms_config *cfg)
{
    Clay_Color bg = { 30, 30, 34, 255 };
    Clay_Color panel = { 44, 44, 50, 255 };
    Clay_Color field = { 22, 22, 26, 255 };
    Clay_Color text_color = { 232, 232, 238, 255 };
    Clay_Color muted = { 150, 150, 160, 255 };
    Clay_Color accent = { 90, 150, 240, 255 };
    Clay_Color accent_hot = { 120, 175, 255, 255 };

    Clay_BeginLayout();
    CLAY(CLAY_ID("SettingsRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .padding = CLAY_PADDING_ALL(16),
            .childGap = 14,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = bg,
    })
    {
        CLAY_TEXT(CLAY_STRING("MiniShot Settings"),
                  CLAY_TEXT_CONFIG({ .textColor = text_color,
                                     .fontId = MS_SETTINGS_FONT_ID,
                                     .fontSize = 20 }));

        // Save folder row.
        CLAY(CLAY_ID("FolderSection"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .padding = CLAY_PADDING_ALL(12),
                .childGap = 8,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = panel,
            .cornerRadius = CLAY_CORNER_RADIUS(8),
        })
        {
            CLAY_TEXT(CLAY_STRING("Save folder"),
                      CLAY_TEXT_CONFIG({ .textColor = muted,
                                         .fontId = MS_SETTINGS_FONT_ID,
                                         .fontSize = 14 }));
            CLAY(CLAY_ID("FolderRow"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 10,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
            })
            {
                CLAY(CLAY_ID("FolderField"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                        .padding = CLAY_PADDING_ALL(10),
                    },
                    .backgroundColor = field,
                    .cornerRadius = CLAY_CORNER_RADIUS(6),
                })
                {
                    CLAY_TEXT(
                        ms_settings_dyn_string(cfg->save_dir),
                        CLAY_TEXT_CONFIG({ .textColor = text_color,
                                           .fontId = MS_SETTINGS_FONT_ID,
                                           .fontSize = 15,
                                           .wrapMode = CLAY_TEXT_WRAP_NONE }));
                }
                bool hot = Clay_PointerOver(
                    Clay_GetElementId(CLAY_STRING("ChangeFolderBtn")));
                CLAY(CLAY_ID("ChangeFolderBtn"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_FIXED(96), CLAY_SIZING_FIT(0) },
                        .padding = { 12, 12, 10, 10 },
                        .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = hot ? accent_hot : accent,
                    .cornerRadius = CLAY_CORNER_RADIUS(6),
                })
                {
                    CLAY_TEXT(CLAY_STRING("Change..."),
                              CLAY_TEXT_CONFIG({ .textColor = text_color,
                                                 .fontId = MS_SETTINGS_FONT_ID,
                                                 .fontSize = 15 }));
                }
            }
        }

        // Hotkey row.
        CLAY(CLAY_ID("HotkeySection"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .padding = CLAY_PADDING_ALL(12),
                .childGap = 8,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = panel,
            .cornerRadius = CLAY_CORNER_RADIUS(8),
        })
        {
            CLAY_TEXT(CLAY_STRING("Capture hotkey"),
                      CLAY_TEXT_CONFIG({ .textColor = muted,
                                         .fontId = MS_SETTINGS_FONT_ID,
                                         .fontSize = 14 }));
            CLAY(CLAY_ID("HotkeyField"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .padding = CLAY_PADDING_ALL(10),
                },
                .backgroundColor = field,
                .cornerRadius = CLAY_CORNER_RADIUS(6),
            })
            {
                CLAY_TEXT(
                    ms_settings_dyn_string(cfg->hotkey),
                    CLAY_TEXT_CONFIG({ .textColor = text_color,
                                       .fontId = MS_SETTINGS_FONT_ID,
                                       .fontSize = 15,
                                       .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
        }
    }
    return Clay_EndLayout(0.0f);
}

void ms_settings_open(void)
{
    ms_config cfg;
    ms_config_load(&cfg);

    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    void *mem = NULL;

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            SDL_Log("settings: SDL_InitSubSystem failed: %s", SDL_GetError());
            return;
        }
    }

    win = SDL_CreateWindow("MiniShot Settings", MS_SETTINGS_WIN_W,
                           MS_SETTINGS_WIN_H, 0);
    ren = win ? SDL_CreateRenderer(win, NULL) : NULL;
    if (!win || !ren) {
        SDL_Log("settings: window/renderer failed: %s", SDL_GetError());
        goto out;
    }

    uint64_t mem_size = Clay_MinMemorySize();
    mem = SDL_malloc(mem_size);
    if (!mem) {
        goto out;
    }
    Clay_Arena arena = (Clay_Arena){ .memory = mem, .capacity = mem_size };

    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    Clay_Initialize(arena, (Clay_Dimensions){ (float)w, (float)h },
                    (Clay_ErrorHandler){ ms_settings_handle_errors, NULL });
    Clay_SetMeasureTextFunction(ms_render_measure_text, NULL);

    ms_settings_state st = { .cfg = &cfg,
                             .dirty = false,
                             .dialog_open = false };

    bool running = true;
    while (running) {
        SDL_Event ev;
        bool pointer_down = false;
        bool click = false;
        float mx = 0.0f, my = 0.0f;

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    // Re-post so the outer main loop also observes the quit; we
                    // share the single global SDL event queue with main.c.
                    SDL_PushEvent(&ev);
                    if (!st.dialog_open) {
                        running = false;
                    }
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (ev.window.windowID == SDL_GetWindowID(win) &&
                        !st.dialog_open) {
                        running = false;
                    }
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (ev.key.key == SDLK_ESCAPE && !st.dialog_open) {
                        running = false;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        pointer_down = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        click = true;
                    }
                    break;
                default:
                    break;
            }
        }

        SDL_GetMouseState(&mx, &my);
        SDL_GetWindowSize(win, &w, &h);
        Clay_SetLayoutDimensions((Clay_Dimensions){ (float)w, (float)h });
        Clay_SetPointerState((Clay_Vector2){ mx, my }, pointer_down);

        if (click && !st.dialog_open &&
            Clay_PointerOver(
                Clay_GetElementId(CLAY_STRING("ChangeFolderBtn")))) {
            st.dialog_open = true;
            const char *start = cfg.save_dir[0] ? cfg.save_dir : NULL;
            SDL_ShowOpenFolderDialog(ms_settings_folder_cb, &st, win, start,
                                     false);
        }

        if (st.dirty) {
            if (ms_config_save(&cfg) != 0) {
                SDL_Log("settings: failed to save config");
            }
            st.dirty = false;
        }

        Clay_RenderCommandArray cmds = ms_settings_build(&cfg);

        SDL_SetRenderDrawColor(ren, 30, 30, 34, 255);
        SDL_RenderClear(ren);
        clay_render(ren, cmds);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    // The folder dialog is async: its callback writes through &cfg/&st, which
    // are stack locals here. Do not tear down (and return) until it has
    // resolved, or those writes land in freed stack memory.
    while (st.dialog_open) {
        SDL_PumpEvents();
        SDL_Delay(16);
    }

out:
    if (mem) {
        SDL_free(mem);
    }
    if (ren) {
        SDL_DestroyRenderer(ren);
    }
    if (win) {
        SDL_DestroyWindow(win);
    }
}

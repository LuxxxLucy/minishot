#include "settings.h"

#include <SDL3/SDL.h>

#include "clay.h"
#include "render.h"
#include "assets.h"
#include "config.h"
#include "theme.h"

// Config persistence lives here: config.h declares parse/serialize but no file
// IO, so the load/save against $HOME/MS_CONFIG_FILE live in the app layer.
#define MS_SETTINGS_FONT_ID 0
#define MS_SETTINGS_WIN_W 460
#define MS_SETTINGS_WIN_H 290
#define MS_SETTINGS_FONT_TITLE 20
#define MS_SETTINGS_FONT_LABEL 14
#define MS_SETTINGS_FONT_BODY 15
#define MS_SETTINGS_PANEL_RADIUS 8
#define MS_SETTINGS_FIELD_RADIUS 6
#define MS_SETTINGS_BTN_W 96

#define MS_SETTINGS_PAD_WINDOW 16   // outer window padding
#define MS_SETTINGS_GAP_SECTION 14  // between panels
#define MS_SETTINGS_PAD_PANEL 12    // inside a panel
#define MS_SETTINGS_GAP_LABEL 8     // label to its field
#define MS_SETTINGS_PAD_FIELD 10    // inside a field; button vertical padding
#define MS_SETTINGS_GAP_ROW 10      // folder field to button

#define MS_FRAME_DELAY_MS 16  // ~60 fps idle pacing

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

void ms_config_load(struct ms_config *out)
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
    struct ms_config parsed = ms_config_default();
    if (ms_config_parse(&parsed, text) == 0) {
        *out = parsed;
    }
    SDL_free(text);
}

int ms_config_save(const struct ms_config *cfg)
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
struct ms_settings_state {
    struct ms_config *cfg;
    bool dialog_open;
};

static void SDLCALL ms_settings_folder_cb(void *userdata,
                                          const char *const *filelist,
                                          int filter)
{
    (void)filter;
    struct ms_settings_state *st = (struct ms_settings_state *)userdata;
    st->dialog_open = false;
    if (!filelist || !filelist[0]) {
        return;  // cancelled or error
    }
    SDL_strlcpy(st->cfg->save_dir, filelist[0], sizeof(st->cfg->save_dir));
}

static void ms_settings_handle_errors(Clay_ErrorData err)
{
    SDL_Log("settings clay error: %s", err.errorText.chars);
}

// Fixed-width accent button with a hover tint. The id drives both the layout
// here and the click hit-test (ms_settings_clicked) in the event loop.
static void ms_settings_button(Clay_String id, const char *label,
                               Clay_Color text_color, Clay_Color accent,
                               Clay_Color accent_hot)
{
    bool hot = Clay_PointerOver(Clay_GetElementId(id));
    CLAY(CLAY_SID(id), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(MS_SETTINGS_BTN_W), CLAY_SIZING_FIT(0) },
            .padding = { MS_SETTINGS_PAD_PANEL, MS_SETTINGS_PAD_PANEL,
                         MS_SETTINGS_PAD_FIELD, MS_SETTINGS_PAD_FIELD },
            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = hot ? accent_hot : accent,
        .cornerRadius = CLAY_CORNER_RADIUS(MS_SETTINGS_FIELD_RADIUS),
    })
    {
        CLAY_TEXT(ms_settings_dyn_string(label),
                  CLAY_TEXT_CONFIG({ .textColor = text_color,
                                     .fontId = MS_SETTINGS_FONT_ID,
                                     .fontSize = MS_SETTINGS_FONT_BODY }));
    }
}

// True when a left-release lands on the button with the given id and no modal
// folder dialog is open.
static bool ms_settings_clicked(bool click, bool dialog_open, Clay_String id)
{
    return click && !dialog_open && Clay_PointerOver(Clay_GetElementId(id));
}

// Lay out the settings panel for the current frame. The Clay_Strings built over
// cfg->save_dir / cfg->hotkey only need to outlive Clay_EndLayout() below, and
// cfg outlives this call, so no intermediate copy buffers are needed.
static Clay_RenderCommandArray ms_settings_build(const struct ms_config *cfg)
{
    Clay_Color bg = MS_COL_SET_BG;
    Clay_Color panel = MS_COL_SET_PANEL;
    Clay_Color field = MS_COL_SET_FIELD;
    Clay_Color text_color = MS_COL_SET_TEXT;
    Clay_Color muted = MS_COL_SET_MUTED;
    Clay_Color accent = MS_COL_SET_ACCENT;
    Clay_Color accent_hot = MS_COL_SET_ACCENT_HOT;

    Clay_BeginLayout();
    CLAY(CLAY_ID("SettingsRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .padding = CLAY_PADDING_ALL(MS_SETTINGS_PAD_WINDOW),
            .childGap = MS_SETTINGS_GAP_SECTION,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = bg,
    })
    {
        CLAY_TEXT(CLAY_STRING("MiniShot Settings"),
                  CLAY_TEXT_CONFIG({ .textColor = text_color,
                                     .fontId = MS_SETTINGS_FONT_ID,
                                     .fontSize = MS_SETTINGS_FONT_TITLE }));

        // Save folder row.
        CLAY(CLAY_ID("FolderSection"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .padding = CLAY_PADDING_ALL(MS_SETTINGS_PAD_PANEL),
                .childGap = MS_SETTINGS_GAP_LABEL,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = panel,
            .cornerRadius = CLAY_CORNER_RADIUS(MS_SETTINGS_PANEL_RADIUS),
        })
        {
            CLAY_TEXT(CLAY_STRING("Save folder"),
                      CLAY_TEXT_CONFIG({ .textColor = muted,
                                         .fontId = MS_SETTINGS_FONT_ID,
                                         .fontSize = MS_SETTINGS_FONT_LABEL }));
            CLAY(CLAY_ID("FolderRow"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = MS_SETTINGS_GAP_ROW,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
            })
            {
                CLAY(CLAY_ID("FolderField"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                        .padding = CLAY_PADDING_ALL(MS_SETTINGS_PAD_FIELD),
                    },
                    .backgroundColor = field,
                    .cornerRadius = CLAY_CORNER_RADIUS(MS_SETTINGS_FIELD_RADIUS),
                })
                {
                    CLAY_TEXT(
                        ms_settings_dyn_string(cfg->save_dir),
                        CLAY_TEXT_CONFIG({ .textColor = text_color,
                                           .fontId = MS_SETTINGS_FONT_ID,
                                           .fontSize = MS_SETTINGS_FONT_BODY,
                                           .wrapMode = CLAY_TEXT_WRAP_NONE }));
                }
                ms_settings_button(CLAY_STRING("ChangeFolderBtn"), "Change...",
                                   text_color, accent, accent_hot);
            }
        }

        // Hotkey row.
        CLAY(CLAY_ID("HotkeySection"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .padding = CLAY_PADDING_ALL(MS_SETTINGS_PAD_PANEL),
                .childGap = MS_SETTINGS_GAP_LABEL,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = panel,
            .cornerRadius = CLAY_CORNER_RADIUS(MS_SETTINGS_PANEL_RADIUS),
        })
        {
            CLAY_TEXT(CLAY_STRING("Capture hotkey"),
                      CLAY_TEXT_CONFIG({ .textColor = muted,
                                         .fontId = MS_SETTINGS_FONT_ID,
                                         .fontSize = MS_SETTINGS_FONT_LABEL }));
            CLAY(CLAY_ID("HotkeyField"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .padding = CLAY_PADDING_ALL(MS_SETTINGS_PAD_FIELD),
                },
                .backgroundColor = field,
                .cornerRadius = CLAY_CORNER_RADIUS(MS_SETTINGS_FIELD_RADIUS),
            })
            {
                CLAY_TEXT(
                    ms_settings_dyn_string(cfg->hotkey),
                    CLAY_TEXT_CONFIG({ .textColor = text_color,
                                       .fontId = MS_SETTINGS_FONT_ID,
                                       .fontSize = MS_SETTINGS_FONT_BODY,
                                       .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
        }

        // Apply row: right-aligned button that saves and closes.
        CLAY(CLAY_ID("ApplyRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childAlignment = { .x = CLAY_ALIGN_X_RIGHT },
            },
        })
        {
            ms_settings_button(CLAY_STRING("ApplyBtn"), "Apply", text_color,
                               accent, accent_hot);
        }
    }
    return Clay_EndLayout(0.0f);
}

void ms_settings_open(void)
{
    struct ms_config cfg;
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
                           MS_SETTINGS_WIN_H, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    ren = win ? SDL_CreateRenderer(win, NULL) : NULL;
    if (!win || !ren) {
        SDL_Log("settings: window/renderer failed: %s", SDL_GetError());
        goto out;
    }

    // Draw the point-based layout into a high-DPI backbuffer: logical
    // presentation maps points to device pixels, and the render layer
    // oversamples text by the same factor so it stays crisp.
    float scale = SDL_GetWindowPixelDensity(win);
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    SDL_SetRenderLogicalPresentation(ren, MS_SETTINGS_WIN_W, MS_SETTINGS_WIN_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    ms_render_set_ui_scale(scale);

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

    struct ms_settings_state st = { .cfg = &cfg, .dialog_open = false };

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

        if (ms_settings_clicked(click, st.dialog_open,
                                CLAY_STRING("ChangeFolderBtn"))) {
            st.dialog_open = true;
            const char *start = cfg.save_dir[0] ? cfg.save_dir : NULL;
            SDL_ShowOpenFolderDialog(ms_settings_folder_cb, &st, win, start,
                                     false);
        }

        // Apply persists the edited config and closes the window.
        if (ms_settings_clicked(click, st.dialog_open,
                                CLAY_STRING("ApplyBtn"))) {
            if (ms_config_save(&cfg) != 0) {
                SDL_Log("settings: failed to save config");
            }
            running = false;
        }

        Clay_RenderCommandArray cmds = ms_settings_build(&cfg);

        Clay_Color clear = MS_COL_SET_BG;
        SDL_SetRenderDrawColor(ren, (Uint8)clear.r, (Uint8)clear.g,
                               (Uint8)clear.b, (Uint8)clear.a);
        SDL_RenderClear(ren);
        clay_render(ren, cmds);
        SDL_RenderPresent(ren);
        SDL_Delay(MS_FRAME_DELAY_MS);
    }

    // The folder dialog is async: its callback writes through &cfg/&st, which
    // are stack locals here. Do not tear down (and return) until it has
    // resolved, or those writes land in freed stack memory.
    while (st.dialog_open) {
        SDL_PumpEvents();
        SDL_Delay(MS_FRAME_DELAY_MS);
    }

out:
    ms_render_set_ui_scale(1.0f);
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

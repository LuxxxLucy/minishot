#include "overlay.h"

#include <SDL3/SDL.h>

#include "render.h"

static SDL_Window *ms_overlay_create_window(SDL_Rect bounds)
{
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        return NULL;
    }

    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                          "MiniShot Selection");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, bounds.x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, bounds.y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, bounds.w);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                          bounds.h);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN,
                           true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN,
                           true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN,
                           true);
    SDL_SetBooleanProperty(
        props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    // Not a UTILITY/non-activating panel: the overlay must be able to become
    // key so it takes keyboard (Esc) and the first mouse press.

    SDL_Window *win = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    return win;
}

ms_overlay_result ms_overlay_run(void)
{
    ms_overlay_result res = { { 0, 0, 0, 0 }, 1 };

    SDL_Rect bounds = { 0, 0, 0, 0 };
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0 || !SDL_GetDisplayBounds(display, &bounds)) {
        SDL_Log("overlay: display bounds failed: %s", SDL_GetError());
        return res;
    }

    SDL_Window *win = ms_overlay_create_window(bounds);
    if (!win) {
        SDL_Log("overlay: window create failed: %s", SDL_GetError());
        return res;
    }
    SDL_SetWindowAlwaysOnTop(win, true);
    SDL_RaiseWindow(win);

    SDL_Renderer *ren = SDL_CreateRenderer(win, NULL);
    if (!ren) {
        SDL_Log("overlay: renderer create failed: %s", SDL_GetError());
        SDL_DestroyWindow(win);
        return res;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // Drawing is in device pixels (no logical presentation); mouse coords are
    // in points, so scale them by the display's pixel density.
    float s = SDL_GetWindowPixelDensity(win);
    if (s <= 0.0f) {
        s = 1.0f;
    }

    SDL_Cursor *crosshair = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    if (crosshair) {
        SDL_SetCursor(crosshair);
    }

    bool dragging = false, done = false, cancelled = true;
    float anchor_x = 0, anchor_y = 0, cur_x = 0, cur_y = 0;
    ms_rect sel = { 0, 0, 0, 0 };

    // The W x H badge is re-rasterized only when the dimension string changes.
    SDL_Texture *badge = NULL;
    char badge_label[64] = "";
    int badge_w = 0, badge_h = 0;

    while (!done) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    done = true;
                    cancelled = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (ev.key.key == SDLK_ESCAPE) {
                        done = true;
                        cancelled = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (ev.button.button == SDL_BUTTON_RIGHT) {
                        done = true;
                        cancelled = true;
                    } else if (ev.button.button == SDL_BUTTON_LEFT) {
                        dragging = true;
                        anchor_x = ev.button.x;
                        anchor_y = ev.button.y;
                        cur_x = ev.button.x;
                        cur_y = ev.button.y;
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    cur_x = ev.motion.x;
                    cur_y = ev.motion.y;
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (ev.button.button == SDL_BUTTON_LEFT && dragging) {
                        cur_x = ev.button.x;
                        cur_y = ev.button.y;
                        sel =
                            ms_rect_from_drag(anchor_x, anchor_y, cur_x, cur_y);
                        dragging = false;
                        if (sel.w >= 1.0f && sel.h >= 1.0f) {
                            done = true;
                            cancelled = false;
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        ms_rect live = dragging
                           ? ms_rect_from_drag(anchor_x, anchor_y, cur_x, cur_y)
                           : sel;

        // Neutral dim backdrop.
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
        SDL_RenderClear(ren);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 102);
        SDL_RenderFillRect(ren, NULL);

        if (dragging && live.w >= 1.0f && live.h >= 1.0f) {
            SDL_FRect r = { live.x * s, live.y * s, live.w * s, live.h * s };

            // Punch the selection back to fully transparent.
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
            SDL_RenderFillRect(ren, &r);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

            // White rubber-band (2px) with a faint dark inner edge.
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 230);
            SDL_RenderRect(ren, &r);
            SDL_FRect r2 = { r.x + 1, r.y + 1, r.w - 2, r.h - 2 };
            SDL_RenderRect(ren, &r2);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 51);
            SDL_FRect r3 = { r.x + 2, r.y + 2, r.w - 4, r.h - 4 };
            SDL_RenderRect(ren, &r3);

            // Crisp "W x H" badge near the cursor.
            char label[64];
            SDL_snprintf(label, sizeof(label), "%d \xc3\x97 %d",
                         (int)(live.w + 0.5f), (int)(live.h + 0.5f));
            if (!badge || SDL_strcmp(label, badge_label) != 0) {
                if (badge) {
                    SDL_DestroyTexture(badge);
                }
                SDL_Color fg = { 255, 255, 255, 255 };
                badge = ms_render_text_sized(ren, label, 13.0f * s, fg,
                                             &badge_w, &badge_h);
                SDL_strlcpy(badge_label, label, sizeof(badge_label));
            }
            if (badge) {
                float padx = 8 * s, pady = 4 * s;
                float bw = badge_w + 2 * padx, bh = badge_h + 2 * pady;
                float bx = (cur_x + 14) * s, by = (cur_y + 14) * s;
                if (bx + bw > bounds.w * s) {
                    bx = (cur_x - 14) * s - bw;
                }
                if (by + bh > bounds.h * s) {
                    by = (cur_y - 14) * s - bh;
                }
                ms_draw_rounded_rect(ren, (SDL_FRect){ bx, by, bw, bh }, 6 * s,
                                     (Clay_Color){ 28, 28, 32, 224 });
                SDL_FRect dst = { bx + padx, by + pady, (float)badge_w,
                                  (float)badge_h };
                SDL_RenderTexture(ren, badge, NULL, &dst);
            }
        }

        SDL_RenderPresent(ren);
        SDL_Delay(8);
    }

    if (!cancelled) {
        int wx = bounds.x, wy = bounds.y;
        SDL_GetWindowPosition(win, &wx, &wy);
        res.rect = ms_rect_to_screen(sel, (float)wx, (float)wy);
        res.cancelled = 0;
    } else {
        res.cancelled = 1;
    }

    if (badge) {
        SDL_DestroyTexture(badge);
    }
    if (crosshair) {
        SDL_SetCursor(SDL_GetDefaultCursor());
        SDL_DestroyCursor(crosshair);
    }
    SDL_HideWindow(win);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_PumpEvents();

    return res;
}

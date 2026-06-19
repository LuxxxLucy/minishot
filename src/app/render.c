#include <SDL3_ttf/SDL_ttf.h>
#include "clay.h"
#include "render.h"
#include "stb_image.h"

#define MS_CORNER_SEG 6  // arc segments per rounded corner

// Single font shared by all text rendering and measurement.
static TTF_Font *g_font = NULL;
static float g_font_size = 15.0f;
// Icon font (Lucide), sized per-glyph at render time.
static TTF_Font *g_icon_font = NULL;

static SDL_FRect to_frect(Clay_BoundingBox b)
{
    return (SDL_FRect){ b.x, b.y, b.width, b.height };
}

static void set_color(SDL_Renderer *ren, Clay_Color c)
{
    SDL_SetRenderDrawColor(ren, (Uint8)c.r, (Uint8)c.g, (Uint8)c.b, (Uint8)c.a);
}

void ms_fill_quad(SDL_Renderer *r, SDL_FPoint p0, SDL_FPoint p1, SDL_FPoint p2,
                  SDL_FPoint p3, SDL_FColor c0, SDL_FColor c1, SDL_FColor c2,
                  SDL_FColor c3)
{
    SDL_Vertex v[6] = {
        { p0, c0, { 0, 0 } }, { p1, c1, { 0, 0 } }, { p2, c2, { 0, 0 } },
        { p0, c0, { 0, 0 } }, { p2, c2, { 0, 0 } }, { p3, c3, { 0, 0 } },
    };
    SDL_RenderGeometry(r, NULL, v, 6, NULL, 0);
}

// Convert a freshly rendered surface to a texture, write its pixel size to
// *w/*h (when non-NULL), and free the surface. Returns NULL on failure.
static SDL_Texture *ms_texture_from_surface(SDL_Renderer *ren,
                                            SDL_Surface *surf, int *w, int *h)
{
    if (!surf) {
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    if (tex) {
        if (w) {
            *w = surf->w;
        }
        if (h) {
            *h = surf->h;
        }
    }
    SDL_DestroySurface(surf);
    return tex;
}

// Unit corner-arc offsets (cos,sin) per corner, computed once and reused so the
// rounded-rect hot path does no per-call trigonometry.
static float g_arc_cos[4][MS_CORNER_SEG + 1];
static float g_arc_sin[4][MS_CORNER_SEG + 1];
static bool g_arc_ready = false;

static void ms_arc_init(void)
{
    const float a0[4] = { (float)M_PI, -(float)M_PI / 2.0f, 0.0f,
                          (float)M_PI / 2.0f };
    for (int c = 0; c < 4; c++) {
        for (int s = 0; s <= MS_CORNER_SEG; s++) {
            float a =
                a0[c] + (float)M_PI / 2.0f * (float)s / (float)MS_CORNER_SEG;
            g_arc_cos[c][s] = cosf(a);
            g_arc_sin[c][s] = sinf(a);
        }
    }
    g_arc_ready = true;
}

void ms_draw_rounded_rect(SDL_Renderer *ren, SDL_FRect rect, float radius,
                          Clay_Color color)
{
    float max_r = (rect.w < rect.h ? rect.w : rect.h) * 0.5f;
    if (radius > max_r) {
        radius = max_r;
    }
    if (radius < 0.5f) {
        set_color(ren, color);
        SDL_RenderFillRect(ren, &rect);
        return;
    }

    if (!g_arc_ready) {
        ms_arc_init();
    }
    const int seg = MS_CORNER_SEG;
    SDL_FColor fc = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f,
                      color.a / 255.0f };

    // Build the rounded outline (4 corner arcs), then fan-triangulate from the
    // center. Corner centers, clockwise from top-left.
    float cx[4] = { rect.x + radius, rect.x + rect.w - radius,
                    rect.x + rect.w - radius, rect.x + radius };
    float cy[4] = { rect.y + radius, rect.y + radius, rect.y + rect.h - radius,
                    rect.y + rect.h - radius };

    int npts = 4 * (seg + 1);
    SDL_FPoint pts[4 * (MS_CORNER_SEG + 1)];
    int n = 0;
    for (int c = 0; c < 4; c++) {
        for (int s = 0; s <= seg; s++) {
            pts[n].x = cx[c] + g_arc_cos[c][s] * radius;
            pts[n].y = cy[c] + g_arc_sin[c][s] * radius;
            n++;
        }
    }

    SDL_FPoint center = { rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f };
    SDL_Vertex verts[3 * 4 * (MS_CORNER_SEG + 1)];
    int vn = 0;
    for (int i = 0; i < npts; i++) {
        int j = (i + 1) % npts;
        verts[vn++] = (SDL_Vertex){ center, fc, { 0, 0 } };
        verts[vn++] = (SDL_Vertex){ pts[i], fc, { 0, 0 } };
        verts[vn++] = (SDL_Vertex){ pts[j], fc, { 0, 0 } };
    }
    SDL_RenderGeometry(ren, NULL, verts, vn, NULL, 0);
}

// Draw a Clay text command by rasterizing the slice with TTF and blitting it.
static void render_text(SDL_Renderer *ren, Clay_TextRenderData t,
                        SDL_FRect rect)
{
    if (!g_font || t.stringContents.length <= 0) {
        return;
    }
    SDL_Color fg = { (Uint8)t.textColor.r, (Uint8)t.textColor.g,
                     (Uint8)t.textColor.b, (Uint8)t.textColor.a };
    SDL_Surface *surf = TTF_RenderText_Blended(
        g_font, t.stringContents.chars, (size_t)t.stringContents.length, fg);
    int tw = 0, th = 0;
    SDL_Texture *tex = ms_texture_from_surface(ren, surf, &tw, &th);
    if (!tex) {
        return;
    }
    SDL_FRect dst = { rect.x, rect.y, (float)tw, (float)th };
    SDL_RenderTexture(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

// Blit an SDL_Texture passed through Clay's imageData pointer.
static void render_image(SDL_Renderer *ren, Clay_ImageRenderData img,
                         SDL_FRect rect)
{
    SDL_Texture *tex = (SDL_Texture *)img.imageData;
    if (!tex) {
        return;
    }
    Clay_Color tint = img.backgroundColor;
    bool tinted = (tint.r || tint.g || tint.b || tint.a);
    if (tinted) {
        SDL_SetTextureColorMod(tex, (Uint8)tint.r, (Uint8)tint.g,
                               (Uint8)tint.b);
        SDL_SetTextureAlphaMod(tex, (Uint8)tint.a);
    }
    SDL_RenderTexture(ren, tex, NULL, &rect);
    if (tinted) {
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
    }
}

void clay_render(SDL_Renderer *ren, Clay_RenderCommandArray commands)
{
    for (int32_t i = 0; i < commands.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        SDL_FRect rect = to_frect(cmd->boundingBox);
        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData r = cmd->renderData.rectangle;
                if (r.cornerRadius.topLeft > 0.5f) {
                    ms_draw_rounded_rect(ren, rect, r.cornerRadius.topLeft,
                                         r.backgroundColor);
                } else {
                    set_color(ren, r.backgroundColor);
                    SDL_RenderFillRect(ren, &rect);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                render_text(ren, cmd->renderData.text, rect);
                break;
            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                render_image(ren, cmd->renderData.image, rect);
                break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                Clay_BorderRenderData b = cmd->renderData.border;
                set_color(ren, b.color);
                SDL_FRect top = { rect.x, rect.y, rect.w, b.width.top };
                SDL_FRect bottom = { rect.x, rect.y + rect.h - b.width.bottom,
                                     rect.w, b.width.bottom };
                SDL_FRect left = { rect.x, rect.y, b.width.left, rect.h };
                SDL_FRect right = { rect.x + rect.w - b.width.right, rect.y,
                                    b.width.right, rect.h };
                if (b.width.top) {
                    SDL_RenderFillRect(ren, &top);
                }
                if (b.width.bottom) {
                    SDL_RenderFillRect(ren, &bottom);
                }
                if (b.width.left) {
                    SDL_RenderFillRect(ren, &left);
                }
                if (b.width.right) {
                    SDL_RenderFillRect(ren, &right);
                }
                break;
            }
            default:
                break;
        }
    }
}

int ms_render_init_font(const char *ttf_path, float size)
{
    if (!TTF_WasInit() && !TTF_Init()) {
        return -1;
    }
    if (g_font) {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }
    g_font = TTF_OpenFont(ttf_path, size);
    g_font_size = size;
    return g_font ? 0 : -1;
}

int ms_render_init_icon_font(const char *ttf_path)
{
    if (!TTF_WasInit() && !TTF_Init()) {
        return -1;
    }
    if (g_icon_font) {
        TTF_CloseFont(g_icon_font);
        g_icon_font = NULL;
    }
    g_icon_font = TTF_OpenFont(ttf_path, 64.0f);  // size set per-glyph below
    return g_icon_font ? 0 : -1;
}

SDL_Surface *ms_render_glyph_surface(Uint32 codepoint, int px, SDL_Color color)
{
    if (!g_icon_font || px <= 0) {
        return NULL;
    }
    if (!TTF_SetFontSize(g_icon_font, (float)px)) {
        return NULL;
    }
    return TTF_RenderGlyph_Blended(g_icon_font, codepoint, color);
}

SDL_Texture *ms_render_glyph(SDL_Renderer *ren, Uint32 codepoint, int px,
                             SDL_Color color)
{
    SDL_Surface *surf = ms_render_glyph_surface(codepoint, px, color);
    return ms_texture_from_surface(ren, surf, NULL, NULL);
}

SDL_Texture *ms_render_text(SDL_Renderer *ren, const char *str, SDL_Color color,
                            int *w, int *h)
{
    if (!g_font || !str) {
        return NULL;
    }
    SDL_Surface *surf = TTF_RenderText_Blended(g_font, str, 0, color);
    return ms_texture_from_surface(ren, surf, w, h);
}

SDL_Texture *ms_render_text_sized(SDL_Renderer *ren, const char *str, float px,
                                  SDL_Color color, int *w, int *h)
{
    if (!g_font) {
        return NULL;
    }
    TTF_SetFontSize(g_font, px);
    SDL_Texture *tex = ms_render_text(ren, str, color, w, h);
    TTF_SetFontSize(g_font, g_font_size);
    return tex;
}

Clay_Dimensions ms_render_measure_text(Clay_StringSlice text,
                                       Clay_TextElementConfig *cfg, void *ud)
{
    (void)cfg;
    (void)ud;
    if (!g_font || text.length <= 0) {
        return (Clay_Dimensions){ 0, 0 };
    }
    int w = 0, h = 0;
    if (!TTF_GetStringSize(g_font, text.chars, (size_t)text.length, &w, &h)) {
        return (Clay_Dimensions){ 0, 0 };
    }
    return (Clay_Dimensions){ (float)w, (float)h };
}

// --- Icons (Lucide glyphs) ---------------------------------------------------

// Lucide codepoints, verified against the bundled font's info.json.
static Uint32 ms_icon_codepoint(ms_icon_kind kind)
{
    switch (kind) {
        case MS_ICON_PIN:
            return 0xE259;  // pin
        case MS_ICON_COPY:
            return 0xE09E;  // copy
        case MS_ICON_SAVE:
            return 0xE14D;  // save
        case MS_ICON_DELETE:
            return 0xE18E;  // trash-2
        case MS_ICON_CANCEL:
            return 0xE1B2;  // x
        case MS_ICON_TRAY:
            return 0xE064;  // camera
    }
    return 0xE1B2;
}

SDL_Texture *ms_icon_make(SDL_Renderer *ren, ms_icon_kind kind, int size)
{
    if (!ren || size <= 0) {
        return NULL;
    }
    SDL_Color white = { 238, 238, 245, 255 };
    SDL_Texture *tex =
        ms_render_glyph(ren, ms_icon_codepoint(kind), size * 2, white);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    }
    return tex;
}

SDL_Surface *ms_icon_tray_surface(int px)
{
    if (px <= 0) {
        return NULL;
    }
    SDL_Color white = { 244, 244, 246, 255 };
    return ms_render_glyph_surface(ms_icon_codepoint(MS_ICON_TRAY), px, white);
}

// --- Images ------------------------------------------------------------------

int ms_image_load_texture(SDL_Renderer *r, const char *png_path,
                          SDL_Texture **out, int *w, int *h)
{
    if (!r || !png_path || !out) {
        return -1;
    }

    int rc = -1;
    stbi_uc *pixels = NULL;
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;

    int iw = 0, ih = 0, channels = 0;
    pixels = stbi_load(png_path, &iw, &ih, &channels, 4);
    if (!pixels) {
        goto out;
    }
    surface =
        SDL_CreateSurfaceFrom(iw, ih, SDL_PIXELFORMAT_RGBA32, pixels, iw * 4);
    if (!surface) {
        goto out;
    }
    texture = SDL_CreateTextureFromSurface(r, surface);
    if (!texture) {
        goto out;
    }

    *out = texture;
    texture = NULL;  // ownership transferred to caller
    if (w) {
        *w = iw;
    }
    if (h) {
        *h = ih;
    }
    rc = 0;
out:
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    if (surface) {
        SDL_DestroySurface(surface);
    }
    if (pixels) {
        stbi_image_free(pixels);
    }
    return rc;
}

struct ms_clipboard_png {
    void *bytes;
    size_t size;
};

static const void *ms_clipboard_png_callback(void *userdata,
                                             const char *mime_type,
                                             size_t *size)
{
    (void)mime_type;
    struct ms_clipboard_png *data = (struct ms_clipboard_png *)userdata;
    if (size) {
        *size = data->size;
    }
    return data->bytes;
}

static void ms_clipboard_png_cleanup(void *userdata)
{
    struct ms_clipboard_png *data = (struct ms_clipboard_png *)userdata;
    SDL_free(data->bytes);
    SDL_free(data);
}

int ms_clipboard_put_png(const char *png_path)
{
    if (!png_path) {
        return -1;
    }

    size_t size = 0;
    void *bytes = SDL_LoadFile(png_path, &size);
    if (!bytes) {
        return -1;
    }

    struct ms_clipboard_png *data =
        (struct ms_clipboard_png *)SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_free(bytes);
        return -1;
    }
    data->bytes = bytes;
    data->size = size;

    const char *mime_types[] = { "image/png" };
    if (!SDL_SetClipboardData(ms_clipboard_png_callback,
                              ms_clipboard_png_cleanup, data, mime_types, 1)) {
        // SDL took ownership of the cleanup before the driver failed; clear it
        // so cleanup runs exactly once. Never SDL_free here (would
        // double-free).
        SDL_ClearClipboardData();
        return -1;
    }
    return 0;
}

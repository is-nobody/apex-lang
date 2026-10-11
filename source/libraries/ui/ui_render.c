// source/libraries/ui/ui_render.c
// Implementation of UI painter and renderer for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_text.h"
#include "ui_window.h"
#include <string.h>
#include <math.h>

// simd dispatch
#if defined(__SSE2__) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#  include <emmintrin.h>
#  define UI_HAVE_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#  include <arm_neon.h>
#  define UI_HAVE_NEON 1
#endif

// active framebuffer and its dimensions
static uint32_t* g_pixels; static int g_w, g_h;

// nested scissor rectangles
static int clip_stack[16][4]; static int clip_depth;

// top-of-stack clip cached as (x0, y0, x1, y1). refreshed on every push/pop
// so the hot pixel loops read two globals instead of chasing the stack.
static int g_clip_x0, g_clip_y0, g_clip_x1, g_clip_y1;

// refresh the cached clip from the current top of the stack
static inline void clip_refresh(void) {
    if (clip_depth > 0) {
        g_clip_x0 = clip_stack[clip_depth-1][0];
        g_clip_y0 = clip_stack[clip_depth-1][1];
        g_clip_x1 = g_clip_x0 + clip_stack[clip_depth-1][2];
        g_clip_y1 = g_clip_y0 + clip_stack[clip_depth-1][3];
    } else {
        g_clip_x0 = 0; g_clip_y0 = 0;
        g_clip_x1 = g_w; g_clip_y1 = g_h;
    }
}

// binds the painter to a framebuffer and clears the clip stack
void ui_painter_init(uint32_t* p, int w, int h) {
    g_pixels = p; g_w = w; g_h = h; clip_depth = 0;
    clip_refresh();
    ui_text_set_clip(0, 0, w, h);                        // whole surface: nothing clipped
}

// pushes a scissor rect, intersected with the current top of the stack
void ui_painter_push_clip(int x, int y, int w, int h) {
    if (clip_depth < 16) {                              // hard cap to bound recursion
        int cx0 = 0, cy0 = 0, cx1 = g_w, cy1 = g_h;     // start from the whole surface
        if (clip_depth > 0) {                           // intersect with current clip
            cx0 = clip_stack[clip_depth-1][0];
            cy0 = clip_stack[clip_depth-1][1];
            cx1 = cx0 + clip_stack[clip_depth-1][2];
            cy1 = cy0 + clip_stack[clip_depth-1][3];
        }
        int nx0 = x > cx0 ? x : cx0, ny0 = y > cy0 ? y : cy0;  // max of the left/top edges
        int nx1 = x+w < cx1 ? x+w : cx1, ny1 = y+h < cy1 ? y+h : cy1;  // min of the right/bottom edges
        if (nx1 < nx0) nx1 = nx0;                       // guard against inverted rects
        if (ny1 < ny0) ny1 = ny0;
        clip_stack[clip_depth][0] = nx0; clip_stack[clip_depth][1] = ny0;
        clip_stack[clip_depth][2] = nx1 - nx0; clip_stack[clip_depth][3] = ny1 - ny0;
        clip_depth++;
        clip_refresh();
        ui_text_set_clip(nx0, ny0, nx1, ny1);
    }
}

// pops the most recent scissor rect
void ui_painter_pop_clip(void) {
    if (clip_depth > 0) clip_depth--;
    clip_refresh();
    ui_text_set_clip(g_clip_x0, g_clip_y0, g_clip_x1, g_clip_y1);
}

// clears the whole clip stack
void ui_painter_reset_clip(void) {
    clip_depth = 0;
    clip_refresh();
    ui_text_set_clip(0, 0, g_w, g_h);
}

// returns true when (x, y) lies inside the active scissor rect
static inline bool in_clip(int x, int y) {
    return x >= g_clip_x0 && x < g_clip_x1 &&
           y >= g_clip_y0 && y < g_clip_y1;
}

// alpha-blends a single pixel into the framebuffer, honoring the active clip
static inline void blend_px(int x, int y, uint32_t c) {
    if ((unsigned)x >= (unsigned)g_clip_x1 || x < g_clip_x0) return;
    if ((unsigned)y >= (unsigned)g_clip_y1 || y < g_clip_y0) return;
    uint32_t sa = (c >> 24) & 0xFF;                     // source alpha
    if (!sa) return;                                    // fully transparent: nothing to do
    uint32_t idx = (uint32_t)y * g_w + x;               // linear pixel index
    if (sa == 255) { g_pixels[idx] = 0xFF000000u | (c & 0x00FFFFFFu); return; }
    uint32_t inv = 255 - sa;                            // destination weight
    uint32_t d  = g_pixels[idx];
    uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
    uint32_t sr = (c >> 16) & 0xFF, sg = (c >> 8) & 0xFF, sb = c & 0xFF;
    uint32_t r = (sr*sa + dr*inv + 128) >> 8;
    uint32_t g = (sg*sa + dg*inv + 128) >> 8;
    uint32_t b = (sb*sa + db*inv + 128) >> 8;
    g_pixels[idx] = 0xFF000000u | (r<<16) | (g<<8) | b;
}

// alpha-blends a pixel using an extra coverage factor for antialiased edges
static inline void blend_px_cov(int x, int y, uint32_t c, int cov) {
    if (cov <= 0) return;                               // no coverage: skip
    if (cov >= 255) { blend_px(x, y, c); return; }      // full coverage: delegate
    if ((unsigned)x >= (unsigned)g_clip_x1 || x < g_clip_x0) return;
    if ((unsigned)y >= (unsigned)g_clip_y1 || y < g_clip_y0) return;
    uint32_t idx = (uint32_t)y * g_w + x;               // linear pixel index
    uint32_t ca = (c >> 24) & 0xFF;                     // source alpha
    uint32_t sa = (ca * (uint32_t)cov + 128) >> 8;      // combined alpha, branchless
    if (!sa) return;                                    // fully transparent after coverage
    uint32_t inv = 255 - sa;                            // destination weight
    uint32_t d  = g_pixels[idx];
    uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
    uint32_t sr = (c >> 16) & 0xFF, sg = (c >> 8) & 0xFF, sb = c & 0xFF;
    uint32_t r = (sr*sa + dr*inv + 128) >> 8;
    uint32_t g = (sg*sa + dg*inv + 128) >> 8;
    uint32_t b = (sb*sa + db*inv + 128) >> 8;
    g_pixels[idx] = 0xFF000000u | (r<<16) | (g<<8) | b;
}

// simd row blenders
#ifdef UI_HAVE_SSE2
// processes 4 pixels at a time. uses epi16 multiplies on epi32 lanes
// holding one channel each, with a +128>>8 rounding approximation.
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

    __m128i k_sa = _mm_set1_epi32((int)sa);
    __m128i k_sr = _mm_set1_epi32((int)(sr * inv + 128));
    __m128i k_sg = _mm_set1_epi32((int)(sg * inv + 128));
    __m128i k_sb = _mm_set1_epi32((int)(sb * inv + 128));
    __m128i kFF  = _mm_set1_epi32(0xFF);
    __m128i kAA  = _mm_set1_epi32((int)0xFF000000u);

    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128i d  = _mm_loadu_si128((const __m128i*)(row + i));
        __m128i dr = _mm_and_si128(_mm_srli_epi32(d, 16), kFF);
        __m128i dg = _mm_and_si128(_mm_srli_epi32(d, 8),  kFF);
        __m128i db = _mm_and_si128(d,                     kFF);

        __m128i r = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi16(dr, k_sa), k_sr), 8);
        __m128i g = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi16(dg, k_sa), k_sg), 8);
        __m128i b = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi16(db, k_sa), k_sb), 8);

        __m128i out = _mm_or_si128(
            kAA,
            _mm_or_si128(
                _mm_slli_epi32(r, 16),
                _mm_or_si128(_mm_slli_epi32(g, 8), b)));
        _mm_storeu_si128((__m128i*)(row + i), out);
    }
    for (; i < n; i++) {                                    // scalar tail
        uint32_t d  = row[i];
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (dr*sa + sr*inv + 128) >> 8;
        uint32_t g = (dg*sa + sg*inv + 128) >> 8;
        uint32_t b = (db*sa + sb*inv + 128) >> 8;
        row[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}
#elif defined(UI_HAVE_NEON)
// processes 8 pixels at a time via vld4/vst4 deinterleave.
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

    uint16x8_t k_sa = vdupq_n_u16((uint16_t)sa);
    uint16x8_t k_sr = vdupq_n_u16((uint16_t)(sr * inv + 128));
    uint16x8_t k_sg = vdupq_n_u16((uint16_t)(sg * inv + 128));
    uint16x8_t k_sb = vdupq_n_u16((uint16_t)(sb * inv + 128));
    uint8x8_t  kA8  = vdup_n_u8(0xFF);

    int i = 0;
    for (; i + 8 <= n; i += 8) {
        uint8x8x4_t d = vld4_u8((const uint8_t*)(row + i));   // [B][G][R][A]
        uint16x8_t b16 = vmlaq_u16(k_sb, vmovl_u8(d.val[0]), k_sa);
        uint16x8_t g16 = vmlaq_u16(k_sg, vmovl_u8(d.val[1]), k_sa);
        uint16x8_t r16 = vmlaq_u16(k_sr, vmovl_u8(d.val[2]), k_sa);
        uint8x8x4_t out;
        out.val[0] = vshrn_n_u16(b16, 8);
        out.val[1] = vshrn_n_u16(g16, 8);
        out.val[2] = vshrn_n_u16(r16, 8);
        out.val[3] = kA8;
        vst4_u8((uint8_t*)(row + i), out);
    }
    for (; i < n; i++) {
        uint32_t d  = row[i];
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (dr*sa + sr*inv + 128) >> 8;
        uint32_t g = (dg*sa + sg*inv + 128) >> 8;
        uint32_t b = (db*sa + sb*inv + 128) >> 8;
        row[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}
#else
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;
    for (int i = 0; i < n; i++) {
        uint32_t d  = row[i];
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (dr*sa + sr*inv + 128) >> 8;
        uint32_t g = (dg*sa + sg*inv + 128) >> 8;
        uint32_t b = (db*sa + sb*inv + 128) >> 8;
        row[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}
#endif

// fills an axis-aligned rectangle with the given color
void ui_painter_fill_rect(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;                       // degenerate rectangle

    // clamp to the cached clip; the clip already lives inside the surface
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < g_clip_x0) x0 = g_clip_x0;
    if (y0 < g_clip_y0) y0 = g_clip_y0;
    if (x1 > g_clip_x1) x1 = g_clip_x1;
    if (y1 > g_clip_y1) y1 = g_clip_y1;
    if (x1 <= x0 || y1 <= y0) return;                   // fully clipped

    uint32_t sa = (c >> 24) & 0xFF;
    if (!sa) return;                                    // fully transparent

    int rw = x1 - x0;

    if (sa == 255) {                                    // opaque fast path: row-wise store
        uint32_t color = 0xFF000000u | (c & 0x00FFFFFFu);
        for (int yy = y0; yy < y1; yy++) {
            uint32_t* row = g_pixels + (size_t)yy * g_w + x0;
            for (int i = 0; i < rw; i++) row[i] = color;
        }
        return;
    }

    // alpha path: one SIMD (or scalar) sweep per row
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = g_pixels + (size_t)yy * g_w + x0;
        blend_row_simd(row, rw, c);
    }
}

// corner coverage atlas
typedef struct {
    int      r;              // radius this entry was built for, 0 = empty
    uint8_t* fill_cov;       // r*r coverage, top-left corner, filled arc
    uint8_t* stroke_cov;     // r*r coverage, top-left corner, 1px stroke band
} CornerEntry;

#define CORNER_CACHE_MAX 16
static CornerEntry g_corners[CORNER_CACHE_MAX];
static int         g_corners_count;

// looks up the cache entry for r, building it on first use
static const CornerEntry* corner_get(int r) {
    for (int i = 0; i < g_corners_count; i++)
        if (g_corners[i].r == r) return &g_corners[i];
    if (g_corners_count >= CORNER_CACHE_MAX) return NULL;

    CornerEntry* e = &g_corners[g_corners_count++];
    e->r = r;
    e->fill_cov   = (uint8_t*)malloc((size_t)r * (size_t)r);
    e->stroke_cov = (uint8_t*)malloc((size_t)r * (size_t)r);

    const int SS = 4;
    float rr  = (float)r;
    float rr2 = rr * rr;
    float ri  = rr - 1.0f;
    float ri2 = ri > 0 ? ri * ri : 0.0f;

    for (int yy = 0; yy < r; yy++) {
        for (int xx = 0; xx < r; xx++) {
            int inside_fill = 0, inside_stroke = 0;
            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    float fx = (float)xx + (sx + 0.5f) / SS;
                    float fy = (float)yy + (sy + 0.5f) / SS;
                    float ex = rr - fx;                 // distance from corner center (r, r)
                    float ey = rr - fy;
                    float d2 = ex*ex + ey*ey;
                    if (d2 <= rr2) inside_fill++;
                    if (d2 <= rr2 && d2 >= ri2) inside_stroke++;
                }
            }
            e->fill_cov  [(size_t)yy*r + xx] = (uint8_t)(inside_fill   * 255 / (SS * SS));
            e->stroke_cov[(size_t)yy*r + xx] = (uint8_t)(inside_stroke * 255 / (SS * SS));
        }
    }
    return e;
}

// releases the corner cache; called from ui_painter_shutdown
void ui_painter_shutdown(void) {
    for (int i = 0; i < g_corners_count; i++) {
        free(g_corners[i].fill_cov);
        free(g_corners[i].stroke_cov);
    }
    g_corners_count = 0;
}

// blends an r*r coverage bitmap into the framebuffer at (ox, oy)
static void blit_corner_cov(const uint8_t* cov, int r, int ox, int oy,
                            int flip_x, int flip_y, uint32_t c) {
    for (int yy = 0; yy < r; yy++) {
        int sy = flip_y ? (r - 1 - yy) : yy;
        const uint8_t* row = cov + (size_t)sy * r;
        for (int xx = 0; xx < r; xx++) {
            int sx = flip_x ? (r - 1 - xx) : xx;
            uint8_t cv = row[sx];
            if (cv) blend_px_cov(ox + xx, oy + yy, c, cv);
        }
    }
}

// draws the four rounded corners of a rectangle from the fill bitmap
static void fill_corners_atlas(int x, int y, int w, int h, int r, uint32_t c) {
    if (r <= 0) return;
    const CornerEntry* e = corner_get(r);
    if (!e) return;
    blit_corner_cov(e->fill_cov, r, x,       y,       0, 0, c);   // top-left
    blit_corner_cov(e->fill_cov, r, x + w-r, y,       1, 0, c);   // top-right
    blit_corner_cov(e->fill_cov, r, x,       y + h-r, 0, 1, c);   // bottom-left
    blit_corner_cov(e->fill_cov, r, x + w-r, y + h-r, 1, 1, c);   // bottom-right
}

// draws the four rounded corners of a rectangle from the stroke bitmap
static void stroke_corners_atlas(int x, int y, int w, int h, int r, uint32_t c) {
    if (r <= 0) return;
    const CornerEntry* e = corner_get(r);
    if (!e) return;
    blit_corner_cov(e->stroke_cov, r, x,       y,       0, 0, c); // top-left
    blit_corner_cov(e->stroke_cov, r, x + w-r, y,       1, 0, c); // top-right
    blit_corner_cov(e->stroke_cov, r, x,       y + h-r, 0, 1, c); // bottom-left
    blit_corner_cov(e->stroke_cov, r, x + w-r, y + h-r, 1, 1, c); // bottom-right
}

// fills a rounded rectangle with antialiased corners
void ui_painter_fill_round(int x, int y, int w, int h, int r, uint32_t c) {
    if (w <= 0 || h <= 0) return;                       // degenerate rectangle
    if (r > w/2) r = w/2;                               // clamp radius to half the width
    if (r > h/2) r = h/2;                               // clamp radius to half the height
    if (r <= 0) { ui_painter_fill_rect(x, y, w, h, c); return; }  // square corners: delegate
    ui_painter_fill_rect(x+r,   y,     w-2*r, h,     c);          // horizontal middle band
    ui_painter_fill_rect(x,     y+r,   r,     h-2*r, c);          // left middle
    ui_painter_fill_rect(x+w-r, y+r,   r,     h-2*r, c);          // right middle
    fill_corners_atlas(x, y, w, h, r, c);                         // all four rounded corners
}

// strokes a 1px axis-aligned rectangle outline
void ui_painter_stroke_rect(int x, int y, int w, int h, uint32_t c) {
    ui_painter_fill_rect(x, y, w, 1, c);                // top edge
    ui_painter_fill_rect(x, y+h-1, w, 1, c);            // bottom edge
    ui_painter_fill_rect(x, y, 1, h, c);                // left edge
    ui_painter_fill_rect(x+w-1, y, 1, h, c);            // right edge
}

// strokes a 1px rounded rectangle outline with antialiased corners
void ui_painter_stroke_round(int x, int y, int w, int h, int r, uint32_t c) {
    if (w <= 0 || h <= 0) return;                       // degenerate rectangle
    if (r > w/2) r = w/2;                               // clamp radius to half the width
    if (r > h/2) r = h/2;                               // clamp radius to half the height
    if (r <= 0) { ui_painter_stroke_rect(x, y, w, h, c); return; }  // square corners: delegate
    ui_painter_fill_rect(x+r,   y,       w-2*r, 1,     c);          // top edge
    ui_painter_fill_rect(x+r,   y+h-1,   w-2*r, 1,     c);          // bottom edge
    ui_painter_fill_rect(x,     y+r,     1,     h-2*r, c);          // left edge
    ui_painter_fill_rect(x+w-1, y+r,     1,     h-2*r, c);          // right edge
    stroke_corners_atlas(x, y, w, h, r, c);                         // all four rounded corners
}

// damage-aware
static void draw_widget(UiContext* ctx, Widget* w, const DamageRect* d);

// draws a single line of text at the given baseline position
static void draw_text_at(UiContext* ctx, float x, float baseline,
                         const char* s, int len, float size, uint32_t color) {
    if (!ctx->font || !s || len <= 0) return;           // nothing to draw
    ui_text_draw(ctx->font, g_pixels, g_w, g_h, x, baseline, s, len, size, color);
}

// returns the font size used for a text-bearing widget kind
static float text_fs(WidgetKind k) { return k == WK_HEADING ? UI_FS_H1 : UI_FS_BODY; }

// returns the line height used for a text-bearing widget kind
static float text_lh(WidgetKind k) { return k == WK_HEADING ? UI_LINE_H + 8 : UI_LINE_H; }

// draws wrapped multi-line text using the line breakdown cached by wrap_measure during layout
static void draw_wrapped(UiContext* ctx, Widget* w, int x, int y, int width, uint32_t color) {
    (void)width;                                        // lines were wrapped for this width at layout
    if (!ctx->font) return;                             // no font: nothing to draw
    const char* s = w->text.ptr;
    if (!s || w->line_count == 0) return;               // no cached breakdown: nothing to draw

    float size = text_fs(w->kind);                      // font size for this widget kind
    float lh   = text_lh(w->kind);                      // line height for this widget kind
    float ly   = (float)y + lh - 4;                     // first baseline
    for (int i = 0; i < w->line_count; i++) {
        int a = w->line_starts[i];
        int b = w->line_ends[i];
        if (b > a)                                      // empty ranges (leading indents) draw nothing
            draw_text_at(ctx, (float)x, ly, s + a, b - a, size, color);
        ly += lh;                                       // advance to the next baseline
    }
}

// recursively draws a widget and all of its visible children
static void draw_widget(UiContext* ctx, Widget* w, const DamageRect* d) {
    if (!w || !w->visible) return;      // nothing to draw

    int x  = (int)floorf(w->x + 0.5f);  // rounded pixel coordinates
    int y  = (int)floorf(w->y + 0.5f);
    int ww = (int)floorf(w->w + 0.5f);
    int hh = (int)floorf(w->h + 0.5f);

    bool draw_self = true;
    if (d) {
        int wx1 = x + ww;
        int wy1 = y + hh;
        if (wx1 <= d->x || x >= d->x + d->w ||
            wy1 <= d->y || y >= d->y + d->h) {
            draw_self = false;
        }
    }

    // a scroll always clips its children to its viewport
    bool clip_pushed = false;
    if (w->kind == WK_SCROLL && ww > 0 && hh > 0) {
        int has_sb = w->content_h > (float)hh;
        int content_w = has_sb ? ww - UI_SCROLL_W : ww;
        if (content_w < 1) content_w = 1;
        ui_painter_push_clip(x, y, content_w, hh);
        clip_pushed = true;
    }

    if (draw_self && ww > 0 && hh > 0) {
        if (w->focused && w->kind == WK_INPUT) {  // focus ring around inputs
            ui_painter_stroke_round(x-2, y-2, ww+4, hh+4, UI_RADIUS+2, UI_C_FOCUS);
        }

        switch (w->kind) {
            case WK_TEXT:
                draw_wrapped(ctx, w, x, y, ww, UI_C_FG);  // plain body text
                break;
            case WK_HEADING:
                draw_wrapped(ctx, w, x, y, ww, UI_C_FG);  // heading text uses a larger size
                break;
            case WK_BUTTON: {
                uint32_t bg = UI_C_ACCENT;                // default button background
                if (w->disabled) bg = UI_C_DIS_BG;        // greyed when disabled
                else if (w->pressed) bg = UI_C_ACCENT_A;  // brighter while pressed
                else if (w->hovered) bg = UI_C_ACCENT_H;  // slightly brighter on hover
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, bg);
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER_S);
                float fs = UI_FS_BODY;
                float tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr, w->text.len, fs) : w->text.len*8.0f;
                float tx = x + (ww - tw) * 0.5f;           // center horizontally
                float base = y + (hh + fs * 0.7f) * 0.5f;  // vertically center the cap height
                uint32_t tc = w->disabled ? UI_C_DIS_FG : UI_C_ACCENT_T;
                draw_text_at(ctx, tx, base, w->text.ptr, w->text.len, fs, tc);
                break;
            }
            case WK_BADGE: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS_SM, UI_C_ACCENT);  // pill background
                float fs = UI_FS_SMALL;
                float tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr, w->text.len, fs) : 0;
                float tx = x + (ww - tw) * 0.5f;            // center horizontally
                float base = y + hh - 5;                    // baseline near the bottom
                draw_text_at(ctx, tx, base, w->text.ptr, w->text.len, fs, UI_C_ACCENT_T);
                break;
            }
            case WK_CHECKBOX: {
                int sz = UI_CB_SIZE;                        // box side length
                int bx = x, by = y + (hh - sz)/2;           // vertically centered box

                if (w->checked) {                           // checked state: accent fill plus checkmark
                    uint32_t fill = w->hovered ? UI_C_ACCENT_H : UI_C_ACCENT;
                    ui_painter_fill_round(bx, by, sz, sz, UI_RADIUS_SM, fill);
                    ui_painter_stroke_round(bx, by, sz, sz, UI_RADIUS_SM, UI_C_BORDER_S);

                    int cx = bx + sz/2 - 1, cy = by + sz/2 - 2;  // checkmark origin

                    for (int i = 0; i <= 4; i++) {          // downstroke of the checkmark
                        int px = cx - 4 + i;
                        int py = cy + i;
                        blend_px(px, py,     UI_C_ACCENT_T);
                        blend_px(px, py + 1, UI_C_ACCENT_T);  // 2px thick for legibility
                    }
                    for (int i = 0; i <= 4; i++) {          // upstroke of the checkmark
                        int px = cx + i;
                        int py = cy + 4 - i;
                        blend_px(px, py,     UI_C_ACCENT_T);
                        blend_px(px, py + 1, UI_C_ACCENT_T);
                    }
                } else {                                    // unchecked: empty box
                    uint32_t fill = w->hovered ? UI_C_PANEL_ALT : UI_C_INPUT_BG;
                    ui_painter_fill_round(bx, by, sz, sz, UI_RADIUS_SM, fill);
                    ui_painter_stroke_round(bx, by, sz, sz, UI_RADIUS_SM, UI_C_BORDER);
                }

                float fs = UI_FS_BODY;
                // center the label on the widget instead of pinning it to the bottom
                float base = y + (hh + fs * 0.7f) * 0.5f;
                draw_text_at(ctx, (float)(bx + sz + 8), base,
                             w->text.ptr, w->text.len, fs, UI_C_FG);  // label to the right of the box
                break;
            }
            case WK_RADIO: {
                int sz = UI_CB_SIZE;                        // radio circle side
                int bx = x, by = y + (hh - sz)/2;           // vertically centered circle

                uint32_t fill = w->hovered ? UI_C_PANEL_ALT : UI_C_INPUT_BG;
                ui_painter_fill_round(bx, by, sz, sz, sz/2, fill);              // outer circle
                ui_painter_stroke_round(bx, by, sz, sz, sz/2, UI_C_BORDER);     // outline
                if (w->checked)                                                 // filled dot when selected
                    ui_painter_fill_round(bx+4, by+4, sz-8, sz-8, (sz-8)/2, UI_C_ACCENT);

                float fs = UI_FS_BODY;
                float base = y + (hh + fs * 0.7f) * 0.5f;  // center on the widget
                draw_text_at(ctx, (float)(bx + sz + 8), base, w->text.ptr, w->text.len, fs, UI_C_FG);
                break;
            }
            case WK_PROGRESS: {
                ui_painter_fill_round(x, y, ww, hh, hh/2, UI_C_DIS_BG);         // empty track
                double v = w->num_value;                    // current value, expected 0..1
                if (v < 0) v = 0;
                if (v > 1) v = 1;
                int fw = (int)(ww * v);                     // filled width in pixels
                if (fw > 0) ui_painter_fill_round(x, y, fw, hh, hh/2, UI_C_ACCENT);
                break;
            }
            case WK_SLIDER: {
                int track_y = y + hh/2 - 2;                 // center the track vertically
                ui_painter_fill_round(x, track_y, ww, 4, 2, UI_C_DIS_BG);       // track
                double range = w->num_max - w->num_min;
                double t = range > 0 ? (w->num_value - w->num_min) / range : 0;  // normalized value
                if (t < 0) t = 0;
                if (t > 1) t = 1;
                int thumb_x = x + (int)(t * (ww - 16));     // thumb position along the track
                uint32_t tc = w->pressed ? UI_C_ACCENT_A
                            : w->hovered ? UI_C_ACCENT_H
                            : UI_C_ACCENT;                  // thumb color state
                ui_painter_fill_round(thumb_x, y + hh/2 - 8, 16, 16, 8, tc);    // round thumb
                ui_painter_stroke_round(thumb_x, y + hh/2 - 8, 16, 16, 8, UI_C_BORDER_S);
                break;
            }
            case WK_INPUT: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_INPUT_BG);  // input background
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);  // outline
                float fs = UI_FS_BODY;
                float base = y + (hh + fs * 0.7f) * 0.5f;   // vertically centered text baseline

                WidgetState* s = ui_state_get(ctx, w->id);

                // the persistent cursor and anchor can point past the descriptor's text for one frame
                int lim = w->input_value.len;
                int cur_off = s->cursor < 0 ? lim : s->cursor;
                if (cur_off > lim) cur_off = lim;
                int anc_off = s->sel_anchor < 0 ? lim : s->sel_anchor;
                if (anc_off > lim) anc_off = lim;

                // selection highlight goes under the text so glyphs sit on top
                if (w->focused && anc_off != cur_off) {
                    int a = anc_off < cur_off ? anc_off : cur_off;
                    int b = anc_off < cur_off ? cur_off : anc_off;
                    float x0 = x + 8.0f;
                    float x1 = x + 8.0f;
                    if (ctx->font) {
                        x0 += ui_text_measure_cached(ctx, w->input_value.ptr, a, fs);
                        x1 += ui_text_measure_cached(ctx, w->input_value.ptr, b, fs);
                    }
                    ui_painter_fill_rect((int)x0, y + 5, (int)(x1 - x0), hh - 10, UI_C_SEL_BG);
                }

                draw_text_at(ctx, (float)(x + 8), base, w->input_value.ptr,
                             w->input_value.len, fs, UI_C_FG);

                if (w->focused) {                           // draw the caret when focused
                    if (s->blink_phase < 0.5) {             // visible half of the blink cycle
                        float cx = x + 8;
                        if (ctx->font) cx += ui_text_measure_cached(ctx, w->input_value.ptr, cur_off, fs);
                        ui_painter_fill_rect((int)cx, y + 6, 1, hh - 12, UI_C_FG);
                    }
                }
                break;
            }
            case WK_SEPARATOR:
                ui_painter_fill_rect(x, y, ww, 1, UI_C_BORDER);  // 1px horizontal rule
                break;
            case WK_PANEL: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_PANEL);      // panel background
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);   // panel outline
                break;
            }
            case WK_SPACER:
                break;                                      // spacers draw nothing
            case WK_SCROLL:
                break;                                      // clip already pushed above
            case WK_HSCROLL:
            case WK_VBOX:
            case WK_HBOX:
                break;                                      // pure containers: no own chrome
        }
    }

    for (int i = 0; i < w->child_count; i++)                // first pass: normal children
        if (!w->children[i]->on_top)
            draw_widget(ctx, w->children[i], d);

    for (int i = 0; i < w->child_count; i++)                // second pass: on-top overlay children
        if (w->children[i]->on_top)
            draw_widget(ctx, w->children[i], d);

    if (clip_pushed) ui_painter_pop_clip();                 // restore the clip after scroll children

    if (draw_self && w->kind == WK_SCROLL) {
        if (w->content_h > (float)hh) {                     // scrollbar drawn last, above children
            int sb_x = x + ww - UI_SCROLL_W;
            ui_painter_fill_rect(sb_x, y, UI_SCROLL_W, hh, UI_C_SCROLL_T);   // track
            double maxs = w->content_h - hh;                // scrollable extent
            double t = maxs > 0 ? w->scroll_y / maxs : 0;
            int th = (int)(hh * (hh / w->content_h));       // thumb height proportional to viewport
            if (th < 24) th = 24;                           // enforce a minimum thumb size
            int ty = y + (int)(t * (hh - th));              // thumb position
            ui_painter_fill_round(sb_x+2, ty+2, UI_SCROLL_W-4, th-4,
                                  (UI_SCROLL_W-4)/2, UI_C_SCROLL_H);
        }
    }
}

// paints the custom titlebar across the top of the framebuffer
void ui_render_titlebar(UiContext* ctx) {
    if (!ctx->custom_chrome) return;                     // feature disabled

    int w = ctx->win_w;
    int h = UI_TITLEBAR_H;
    if (w <= 0 || h <= 0) return;                        // degenerate surface

    ui_painter_fill_rect(0, 0, w, h - 1, UI_C_TB_BG);    // titlebar background
    ui_painter_fill_rect(0, h - 1, w, 1, UI_C_BORDER);   // hairline underneath

    // three floating 28x28 buttons pinned to the right with a small gap,
    // matching the apex-engine chrome: minimize, maximize, close
    const int sz  = UI_TB_BTN_SZ;
    const int gap = UI_TB_BTN_GAP;
    const int pr  = UI_TB_BTN_PAD;
    int by  = (h - sz) / 2;
    if (by < UI_TB_RESIZE) by = UI_TB_RESIZE;            // keep buttons clear of the resize strip
    int bx3 = w - pr - sz;
    int bx2 = bx3 - gap - sz;
    int bx1 = bx2 - gap - sz;

    int xs[3] = { bx1, bx2, bx3 };
    for (int i = 0; i < 3; i++) {
        int id = i + 1;                                  // 1 min, 2 max, 3 close
        int bx = xs[i];

        uint32_t fg = UI_C_TB_BTN;                       // default icon color
        if (ctx->tb_active == id) {
            ui_painter_fill_round(bx, by, sz, sz, 6, UI_C_TB_BTN_ACT);
            fg = 0xFFFFFFFFu;
        } else if (ctx->tb_hot == id) {
            ui_painter_fill_round(bx, by, sz, sz, 6, UI_C_TB_BTN_HOV);
            fg = 0xFFFFFFFFu;
        }

        int cx = bx + sz / 2;
        int cy = by + sz / 2;

        // erase color for the restore notch; tracks the button background
        uint32_t bg = UI_C_TB_BG;
        if (ctx->tb_active == id)      bg = UI_C_TB_BTN_ACT;
        else if (ctx->tb_hot == id)    bg = UI_C_TB_BTN_HOV;

        if (id == 1) {                                   // minimize: 10x1 pixel line
            ui_painter_fill_rect(cx - 5, cy, 10, 1, fg);
        } else if (id == 2) {                            // maximize / restore
            bool maxed = ctx->window && ui_window_is_maximized(ctx->window);
            if (!maxed) {
                // 9x9 pixel outline, 1px stroke, no aa
                ui_painter_stroke_rect(cx - 4, cy - 4, 9, 9, fg);
            } else {
                // two overlapping 8x8 outlines; the rear one is punched out
                // where the front one overlaps so the stroke weight stays 1px
                ui_painter_stroke_rect(cx - 3, cy - 4, 8, 8, fg);   // rear
                ui_painter_fill_rect(cx - 5, cy - 2, 8, 8, bg);     // erase front footprint
                ui_painter_stroke_rect(cx - 5, cy - 2, 8, 8, fg);   // front
            }
        } else {                                         // close: 9-pixel diagonals, no aa
            for (int k = 0; k < 9; k++) {
                ui_painter_fill_rect(cx - 4 + k, cy - 4 + k, 1, 1, fg);
                ui_painter_fill_rect(cx - 4 + k, cy + 4 - k, 1, 1, fg);
            }
        }
    }

    // window title at the left edge, truncated with "..." if it would collide with the buttons
    int tlen = (int)strlen(ctx->title);
    if (tlen > 0 && ctx->font) {
        float fs   = UI_FS_BODY;
        float base = (h + fs * 0.7f) * 0.5f;

        const int left_pad  = 14;                     // gap from the window edge
        const int right_pad = 10;                     // gap before the minimize button
        int avail = bx1 - left_pad - right_pad;       // pixels the title may occupy
        if (avail < 0) avail = 0;

        float full_w = ui_text_measure_cached(ctx, ctx->title, tlen, fs);
        if (full_w <= (float)avail) {                 // whole title fits
            ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                         (float)left_pad, base, ctx->title, tlen, fs, UI_C_TB_FG);
        } else {
            static const char ell[] = "...";
            float ell_w = ui_text_measure_cached(ctx, ell, 3, fs);
            if ((float)avail > ell_w) {
                float budget = (float)avail - ell_w;  // width left for the prefix
                int cut = 0;                          // byte length of the kept prefix
                float cut_w = 0.0f;
                int i = 0;
                while (i < tlen) {                    // grow the prefix codepoint by codepoint
                    int next = ui_utf8_next(ctx->title, i, tlen);
                    float w  = ui_text_measure_cached(ctx, ctx->title, next, fs);
                    if (w > budget) break;            // this codepoint would overflow
                    i = next;
                    cut = i;
                    cut_w = w;
                }
                if (cut > 0) {                        // draw prefix then the ellipsis after it
                    ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                                 (float)left_pad, base, ctx->title, cut, fs, UI_C_TB_FG);
                    ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                                 (float)left_pad + cut_w, base, ell, 3, fs, UI_C_TB_FG);
                } else {                              // even a single glyph doesn't fit: just "..."
                    ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                                 (float)left_pad, base, ell, 3, fs, UI_C_TB_FG);
                }
            }
        }
    }
}

// clears the surface and paints the titlebar plus the whole widget tree
void ui_render_frame(UiContext* ctx) {
    ui_painter_init(ctx->pixels, ctx->win_w, ctx->win_h);       // bind the painter to the surface
    ui_painter_fill_rect(0, 0, ctx->win_w, ctx->win_h, UI_C_BG); // clear with the background color
    ui_render_titlebar(ctx);                                    // custom chrome, if enabled
    if (ctx->root) {
        if (ctx->custom_chrome) {                               // keep the tree out of the titlebar
            ui_painter_push_clip(0, UI_TITLEBAR_H,
                                 ctx->win_w, ctx->win_h - UI_TITLEBAR_H);
            draw_widget(ctx, ctx->root, NULL);
            ui_painter_pop_clip();
        } else {
            draw_widget(ctx, ctx->root, NULL);                  // full repaint, no damage mask
        }
    }
}

// repaints only the given damage rectangles, plus the titlebar when it is affected
void ui_render_frame_damage(UiContext* ctx, const DamageRect* rects, int n) {
    if (n <= 0) return;                                         // nothing damaged, no work
    ui_painter_init(ctx->pixels, ctx->win_w, ctx->win_h);       // bind painter to the framebuffer

    bool tb_damaged = false;                                    // did any rect touch the titlebar?

    for (int i = 0; i < n; i++) {                               // one pass per damage rect
        const DamageRect* d = &rects[i];                        // current damage rect
        if (ctx->custom_chrome && d->y < UI_TITLEBAR_H) tb_damaged = true;
        ui_painter_push_clip(d->x, d->y, d->w, d->h);           // restrict writes to this rect
        ui_painter_fill_rect(d->x, d->y, d->w, d->h, UI_C_BG);  // clear rect to background
        if (ctx->root) {
            if (ctx->custom_chrome) {                           // keep the tree out of the titlebar
                ui_painter_push_clip(0, UI_TITLEBAR_H,
                                     ctx->win_w, ctx->win_h - UI_TITLEBAR_H);
                draw_widget(ctx, ctx->root, d);
                ui_painter_pop_clip();
            } else {
                draw_widget(ctx, ctx->root, d);                 // redraw widgets touching rect
            }
        }
        ui_painter_pop_clip();                                  // restore previous clip
    }

    if (tb_damaged) {                                           // repaint the titlebar on top
        ui_painter_push_clip(0, 0, ctx->win_w, UI_TITLEBAR_H);
        ui_painter_fill_rect(0, 0, ctx->win_w, UI_TITLEBAR_H, UI_C_BG);
        ui_render_titlebar(ctx);
        ui_painter_pop_clip();
    }
}
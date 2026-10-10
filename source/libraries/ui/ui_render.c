// source/libraries/ui/ui_render.c
// Implementation of UI painter and renderer for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_text.h"
#include <string.h>
#include <math.h>

// active framebuffer and its dimensions
static uint32_t* g_pixels; static int g_w, g_h;

// nested scissor rectangles
static int clip_stack[16][4]; static int clip_depth;

// binds the painter to a framebuffer and clears the clip stack
void ui_painter_init(uint32_t* p, int w, int h) {
    g_pixels = p; g_w = w; g_h = h; clip_depth = 0;
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
    }
}

// pops the most recent scissor rect
void ui_painter_pop_clip(void) { if (clip_depth > 0) clip_depth--; }

// clears the whole clip stack
void ui_painter_reset_clip(void) { clip_depth = 0; }

// returns true when (x, y) lies inside the active scissor rect
static inline bool in_clip(int x, int y) {
    if (clip_depth <= 0) return true;                   // no clip: everything is inside
    int cx = clip_stack[clip_depth-1][0], cy = clip_stack[clip_depth-1][1];
    int cw = clip_stack[clip_depth-1][2], ch = clip_stack[clip_depth-1][3];
    return x >= cx && x < cx+cw && y >= cy && y < cy+ch;
}

// alpha-blends a single pixel into the framebuffer, honoring the active clip
static inline void blend_px(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= g_w || y >= g_h) return; // outside the surface
    if (!in_clip(x, y)) return;                         // outside the scissor
    uint32_t sa = (c >> 24) & 0xFF;                     // source alpha
    if (!sa) return;                                    // fully transparent: nothing to do
    uint32_t idx = (uint32_t)y * g_w + x;               // linear pixel index
    if (sa == 255) { g_pixels[idx] = 0xFF000000u | (c & 0x00FFFFFFu); return; }  // opaque fast path
    uint32_t inv = 255 - sa;                            // destination weight
    uint32_t dr = (g_pixels[idx] >> 16) & 0xFF, dg = (g_pixels[idx] >> 8) & 0xFF, db = g_pixels[idx] & 0xFF;
    uint32_t sr = (c >> 16) & 0xFF, sg = (c >> 8) & 0xFF, sb = c & 0xFF;
    uint32_t r = (sr*sa + dr*inv)/255, g = (sg*sa + dg*inv)/255, b = (sb*sa + db*inv)/255;
    g_pixels[idx] = 0xFF000000u | (r<<16) | (g<<8) | b;
}

// alpha-blends a pixel using an extra coverage factor for antialiased edges
static inline void blend_px_cov(int x, int y, uint32_t c, int cov) {
    if (cov <= 0) return;                               // no coverage: skip
    if (cov >= 255) { blend_px(x, y, c); return; }      // full coverage: delegate
    if (x < 0 || y < 0 || x >= g_w || y >= g_h) return; // outside the surface
    if (!in_clip(x, y)) return;                         // outside the scissor
    uint32_t idx = (uint32_t)y * g_w + x;               // linear pixel index
    uint32_t ca = (c >> 24) & 0xFF;                     // source alpha
    uint32_t sa = ca * (uint32_t)cov / 255u;            // combined alpha
    if (!sa) return;                                    // fully transparent after coverage
    uint32_t inv = 255 - sa;                            // destination weight
    uint32_t dr = (g_pixels[idx] >> 16) & 0xFF, dg = (g_pixels[idx] >> 8) & 0xFF, db = g_pixels[idx] & 0xFF;
    uint32_t sr = (c >> 16) & 0xFF, sg = (c >> 8) & 0xFF, sb = c & 0xFF;
    uint32_t r = (sr*sa + dr*inv)/255, g = (sg*sa + dg*inv)/255, b = (sb*sa + db*inv)/255;
    g_pixels[idx] = 0xFF000000u | (r<<16) | (g<<8) | b;
}

// fills an axis-aligned rectangle with the given color
void ui_painter_fill_rect(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;                       // degenerate rectangle
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            blend_px(x+xx, y+yy, c);
}

// supersamples a rounded corner and blends the result into the destination
static void fill_corner_aa(int ox, int oy, int r, int cx_off, int cy_off, uint32_t c) {
    if (r <= 0) return;                                 // no corner to draw
    const int SS = 4;                                   // 4x4 supersampling grid
    float rr2 = (float)r * (float)r;                    // outer radius squared
    for (int yy = 0; yy < r; yy++) {
        for (int xx = 0; xx < r; xx++) {
            int inside = 0;                             // samples inside the arc
            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    float fx = (float)xx + (sx + 0.5f) / SS;  // sample position in corner space
                    float fy = (float)yy + (sy + 0.5f) / SS;
                    float ex = (float)cx_off - fx;      // distance from corner center
                    float ey = (float)cy_off - fy;
                    if (ex*ex + ey*ey <= rr2) inside++; // count samples inside the circle
                }
            }
            if (!inside) continue;                      // fully outside
            int cov = inside * 255 / (SS * SS);         // convert sample count to coverage
            if (cov >= 255) blend_px(ox + xx, oy + yy, c);
            else            blend_px_cov(ox + xx, oy + yy, c, cov);
        }
    }
}

// supersamples a rounded corner ring for stroked rounded rectangles
static void stroke_corner_aa(int ox, int oy, int r, int cx_off, int cy_off, uint32_t c) {
    if (r <= 0) return;                                 // no corner to draw
    const int SS = 4;                                   // 4x4 supersampling grid
    float rr  = (float)r;                               // outer radius
    float rr2 = rr * rr;
    float ri  = rr - 1.0f;                              // inner radius for a 1px stroke
    float ri2 = ri > 0 ? ri * ri : 0.0f;
    for (int yy = 0; yy < r; yy++) {
        for (int xx = 0; xx < r; xx++) {
            int inside = 0;                             // samples inside the ring
            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    float fx = (float)xx + (sx + 0.5f) / SS;
                    float fy = (float)yy + (sy + 0.5f) / SS;
                    float ex = (float)cx_off - fx;
                    float ey = (float)cy_off - fy;
                    float d2 = ex*ex + ey*ey;           // sample distance squared
                    if (d2 <= rr2 && d2 >= ri2) inside++;  // inside the stroke band
                }
            }
            if (!inside) continue;                      // fully outside
            int cov = inside * 255 / (SS * SS);         // convert sample count to coverage
            if (cov >= 255) blend_px(ox + xx, oy + yy, c);
            else            blend_px_cov(ox + xx, oy + yy, c, cov);
        }
    }
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
    fill_corner_aa(x,     y,     r, r, r, c);                     // top-left corner
    fill_corner_aa(x+w-r, y,     r, 0, r, c);                     // top-right corner
    fill_corner_aa(x,     y+h-r, r, r, 0, c);                     // bottom-left corner
    fill_corner_aa(x+w-r, y+h-r, r, 0, 0, c);                     // bottom-right corner
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
    stroke_corner_aa(x,     y,     r, r, r, c);                     // top-left corner
    stroke_corner_aa(x+w-r, y,     r, 0, r, c);                     // top-right corner
    stroke_corner_aa(x,     y+h-r, r, r, 0, c);                     // bottom-left corner
    stroke_corner_aa(x+w-r, y+h-r, r, 0, 0, c);                     // bottom-right corner
}

static void draw_widget(UiContext* ctx, Widget* w);     // forward declaration for recursion

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

// draws wrapped multi-line text into the given rectangle width
static void draw_wrapped(UiContext* ctx, Widget* w, int x, int y, int width, uint32_t color) {
    float size = text_fs(w->kind);                      // font size for this widget kind
    float lh   = text_lh(w->kind);                      // line height for this widget kind
    if (!ctx->font) return;                             // no font: nothing to draw
    int len = w->text.len;
    const char* s = w->text.ptr;
    int i = 0; float ly = (float)y + lh - 4;            // first baseline
    while (i < len) {
        while (i < len && s[i] == ' ') i++;             // skip leading spaces
        int line_start = i; float cur = 0;              // start of the current line, its width
        int last_break = -1;                            // position where the line wrapped
        int j = i;
        while (j < len) {
            int nj = j;
            while (nj < len && s[nj] != ' ') nj++;      // end of the next word
            float word_w = ui_text_measure(ctx->font, s + j, nj - j, size);  // word width
            float sp = (cur > 0) ? ui_text_measure(ctx->font, " ", 1, size) : 0;  // space before it
            if (cur + sp + word_w > (float)width && cur > 0) {
                last_break = j;                         // wrap here
                break;
            }
            cur += sp + word_w;                         // append the word
            j = nj;
            if (j < len && s[j] == ' ') j++;            // consume the trailing space
        }
        int end;
        if (last_break > 0) end = last_break;           // stop at the wrap point
        else end = j;                                   // otherwise take the rest of the block
        if (end <= line_start) end = len;               // safety: never loop forever
        draw_text_at(ctx, (float)x, ly, s + line_start, end - line_start, size, color);
        ly += lh;                                       // advance to the next baseline
        i = end;
    }
}

// recursively draws a widget and all of its visible children
static void draw_widget(UiContext* ctx, Widget* w) {
    if (!w || !w->visible) return;                      // nothing to draw
    int x = (int)floorf(w->x + 0.5f);                   // rounded pixel coordinates
    int y = (int)floorf(w->y + 0.5f);
    int ww = (int)floorf(w->w + 0.5f);
    int hh = (int)floorf(w->h + 0.5f);
    if (ww <= 0 || hh <= 0) return;                     // degenerate rect: skip

    if (w->focused && w->kind == WK_INPUT) {            // focus ring around inputs
        ui_painter_stroke_round(x-2, y-2, ww+4, hh+4, UI_RADIUS+2, UI_C_FOCUS);
    }

    switch (w->kind) {
        case WK_TEXT:
            draw_wrapped(ctx, w, x, y, ww, UI_C_FG);    // plain body text
            break;
        case WK_HEADING:
            draw_wrapped(ctx, w, x, y, ww, UI_C_FG);    // heading text uses a larger size
            break;
        case WK_BUTTON: {
            uint32_t bg = UI_C_ACCENT;                  // default button background
            if (w->disabled) bg = UI_C_DIS_BG;          // greyed when disabled
            else if (w->pressed) bg = UI_C_ACCENT_A;    // brighter while pressed
            else if (w->hovered) bg = UI_C_ACCENT_H;    // slightly brighter on hover
            ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, bg);
            ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER_S);
            float fs = UI_FS_BODY;
            float tw = ctx->font ? ui_text_measure(ctx->font, w->text.ptr, w->text.len, fs) : w->text.len*8.0f;
            float tx = x + (ww - tw) * 0.5f;            // center horizontally
            float base = y + (hh + fs * 0.7f) * 0.5f;   // vertically center the cap height
            uint32_t tc = w->disabled ? UI_C_DIS_FG : UI_C_ACCENT_T;
            draw_text_at(ctx, tx, base, w->text.ptr, w->text.len, fs, tc);
            break;
        }
        case WK_LINK: {
            float fs = UI_FS_BODY;
            float base = y + hh - 4;                    // baseline at the bottom of the box
            uint32_t tc = w->hovered ? UI_C_FG : UI_C_FG_DIM;
            draw_text_at(ctx, (float)x, base, w->text.ptr, w->text.len, fs, tc);
            float tw = ctx->font ? ui_text_measure(ctx->font, w->text.ptr, w->text.len, fs) : 0;
            ui_painter_fill_rect(x, (int)base + 2, (int)tw, 1, tc);  // underline
            break;
        }
        case WK_BADGE: {
            ui_painter_fill_round(x, y, ww, hh, UI_RADIUS_SM, UI_C_ACCENT);  // pill background
            float fs = UI_FS_SMALL;
            float tw = ctx->font ? ui_text_measure(ctx->font, w->text.ptr, w->text.len, fs) : 0;
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
            float base = y + hh - 4;                    // label baseline at the bottom
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
            float base = y + hh - 4;                    // label baseline
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
            draw_text_at(ctx, (float)(x + 8), base, w->input_value.ptr,
                         w->input_value.len, fs, UI_C_FG);
            if (w->focused) {                           // draw the caret when focused
                WidgetState* s = ui_state_get(ctx, w->id);
                if (s->blink_phase < 0.5) {             // visible half of the blink cycle
                    int cur = s->cursor < 0 ? w->input_value.len : s->cursor;
                    float cx = x + 8;
                    if (ctx->font) cx += ui_text_measure(ctx->font, w->input_value.ptr, cur, fs);
                    ui_painter_fill_rect((int)cx, y + 6, 1, hh - 12, UI_C_FG);
                }
            }
            break;
        }
        case WK_SEPARATOR:
            ui_painter_fill_rect(x, y, ww, 1, UI_C_BORDER);  // 1px horizontal rule
            break;
        case WK_IMAGE: {
            ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_DIS_BG);     // placeholder fill
            ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);   // outline
            break;
        }
        case WK_PANEL: {
            ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_PANEL);      // panel background
            ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);   // panel outline
            break;
        }
        case WK_SPACER:
            break;                                      // spacers draw nothing
        case WK_SCROLL: {
            // children are clipped to a narrower rect so they never overlap the
            // scrollbar; the scrollbar itself is drawn after children so it
            // always sits on top of any content that bleeds through the clip
            int has_sb = w->content_h > (float)hh;      // scrollbar needed?
            int content_w = has_sb ? ww - UI_SCROLL_W : ww;
            if (content_w < 1) content_w = 1;
            ui_painter_push_clip(x, y, content_w, hh);
            break;
        }
        case WK_FRAME:
        case WK_HSCROLL:
        case WK_GRID:
        case WK_VBOX:
        case WK_HBOX:
            break;                                      // pure containers: no own chrome
    }

    for (int i = 0; i < w->child_count; i++) draw_widget(ctx, w->children[i]);  // draw children in order

    if (w->kind == WK_SCROLL) {
        ui_painter_pop_clip();                          // restore the clip after scroll children

        if (w->content_h > (float)hh) {                 // scrollbar drawn last, above children
            int sb_x = x + ww - UI_SCROLL_W;
            ui_painter_fill_rect(sb_x, y, UI_SCROLL_W, hh, UI_C_SCROLL_T);   // track
            double maxs = w->content_h - hh;            // scrollable extent
            double t = maxs > 0 ? w->scroll_y / maxs : 0;
            int th = (int)(hh * (hh / w->content_h));   // thumb height proportional to viewport
            if (th < 24) th = 24;                       // enforce a minimum thumb size
            int ty = y + (int)(t * (hh - th));          // thumb position
            ui_painter_fill_round(sb_x+2, ty+2, UI_SCROLL_W-4, th-4,
                                  (UI_SCROLL_W-4)/2, UI_C_SCROLL_H);
        }
    }
}

// clears the surface and paints the whole widget tree
void ui_render_frame(UiContext* ctx) {
    ui_painter_init(ctx->pixels, ctx->win_w, ctx->win_h);       // bind the painter to the surface
    ui_painter_fill_rect(0, 0, ctx->win_w, ctx->win_h, UI_C_BG); // clear with the background color
    if (ctx->root) draw_widget(ctx, ctx->root);                 // paint the tree
}
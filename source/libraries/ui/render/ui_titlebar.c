// source/libraries/ui/render/ui_titlebar.c
// Custom titlebar renderer for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_render.h"
#include "ui_text.h"
#include "ui_window.h"
#include <string.h>

// paints the custom titlebar across the top of the framebuffer
void ui_render_titlebar(UiContext* ctx) {
    if (!ctx->custom_chrome) return;                     // feature disabled

    int w = ctx->win_w;
    int h = UI_TITLEBAR_H;
    if (w <= 0 || h <= 0) return;                        // degenerate surface

    ui_painter_fill_rect(0, 0, w, h - 1, UI_C_TB_BG);    // titlebar background
    ui_painter_fill_rect(0, h - 1, w, 1, UI_C_BORDER);   // hairline underneath

    // three floating 28x28 buttons pinned to the right with a small gap
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
            // blit a precomputed 9x9 coverage blob in one pass per row
            blit_close_x_shared(cx - 4, cy - 4, fg);
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
// source/libraries/ui/render/ui_render.c
// Widget renderer for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_render.h"
#include "ui_text.h"
#include "ui_window.h"
#include <string.h>
#include <math.h>

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
            ui_draw_text_at(ctx, (float)x, ly, s + a, b - a, size, color);
        ly += lh;                                       // advance to the next baseline
    }
}

// recursively draws a widget and all of its visible children
static void draw_widget(UiContext* ctx, Widget* w, const DamageRect* d) {
    if (!w || !w->visible) return;      // nothing to draw

    // fast reject
    if (d) {
        if (w->bbox_x1 <= d->x || w->bbox_x0 >= d->x + d->w ||
            w->bbox_y1 <= d->y || w->bbox_y0 >= d->y + d->h)
            return;
    }

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
        if (w->focused && (w->kind == WK_INPUT || w->kind == WK_COMBOBOX)) {
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
                ui_draw_text_at(ctx, tx, base, w->text.ptr, w->text.len, fs, tc);
                break;
            }
            case WK_BADGE: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS_SM, UI_C_ACCENT);  // pill background
                float fs = UI_FS_SMALL;
                float tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr, w->text.len, fs) : 0;
                float tx = x + (ww - tw) * 0.5f;            // center horizontally
                float base = y + hh - 5;                    // baseline near the bottom
                ui_draw_text_at(ctx, tx, base, w->text.ptr, w->text.len, fs, UI_C_ACCENT_T);
                break;
            }
            case WK_CHECKBOX: {
                int sz = UI_CB_SIZE;                        // box side length
                int bx = x, by = y + (hh - sz)/2;           // vertically centered box

                if (w->checked) {                           // checked state: accent fill plus checkmark
                    uint32_t fill = w->hovered ? UI_C_ACCENT_H : UI_C_ACCENT;
                    ui_painter_fill_round(bx, by, sz, sz, UI_RADIUS_SM, fill);
                    ui_painter_stroke_round(bx, by, sz, sz, UI_RADIUS_SM, UI_C_BORDER_S);

                    // blit a precomputed 9x6 coverage blob
                    int cx = bx + sz/2 - 1, cy = by + sz/2 - 2;
                    blit_checkmark_shared(cx - 4, cy, UI_C_ACCENT_T);
                } else {                                    // unchecked: empty box
                    uint32_t fill = w->hovered ? UI_C_PANEL_ALT : UI_C_INPUT_BG;
                    ui_painter_fill_round(bx, by, sz, sz, UI_RADIUS_SM, fill);
                    ui_painter_stroke_round(bx, by, sz, sz, UI_RADIUS_SM, UI_C_BORDER);
                }

                float fs = UI_FS_BODY;
                // center the label on the widget instead of pinning it to the bottom
                float base = y + (hh + fs * 0.7f) * 0.5f;
                ui_draw_text_at(ctx, (float)(bx + sz + 8), base,
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
                ui_draw_text_at(ctx, (float)(bx + sz + 8), base, w->text.ptr, w->text.len, fs, UI_C_FG);
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

                ui_draw_text_at(ctx, (float)(x + 8), base, w->input_value.ptr,
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
            case WK_COMBOBOX: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_INPUT_BG);
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);
                float fs = UI_FS_BODY;
                float base = y + (hh + fs * 0.7f) * 0.5f;
                ui_draw_text_at(ctx, (float)(x + 8), base,
                             w->combo_value.ptr, w->combo_value.len, fs, UI_C_FG);
                int cx = x + ww - 14;
                int cy = y + hh / 2 - 2;
                for (int i = 0; i < 4; i++) {
                    ui_painter_fill_rect(cx + i,     cy + i, 1, 1, UI_C_FG_DIM);
                    ui_painter_fill_rect(cx + 6 - i, cy + i, 1, 1, UI_C_FG_DIM);
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

// paints open combobox dropdowns on top of the tree, in pool order
static void draw_combobox_popups(UiContext* ctx, const DamageRect* d) {
    for (int i = 0; i < ctx->pool_used; i++) {
        Widget* w = &ctx->pool[i];
        if (w->kind != WK_COMBOBOX || !w->combo_open || !w->visible) continue;
        if (w->combo_option_count == 0) continue;

        int x  = (int)floorf(w->x + 0.5f);
        int y  = (int)floorf(w->y + 0.5f);
        int ww = (int)floorf(w->w + 0.5f);
        int hh = (int)floorf(w->h + 0.5f);
        int dy = y + hh;
        int dh = w->combo_option_count * UI_INPUT_H;

        if (d) {
            if (x + ww <= d->x || x >= d->x + d->w ||
                dy + dh <= d->y || dy >= d->y + d->h) continue;
        }

        ui_painter_fill_round(x, dy, ww, dh, UI_RADIUS_SM, UI_C_PANEL);
        ui_painter_stroke_round(x, dy, ww, dh, UI_RADIUS_SM, UI_C_BORDER);

        for (int j = 0; j < w->combo_option_count; j++) {
            int oy = dy + j * UI_INPUT_H;
            bool hot = (ctx->input.x >= x && ctx->input.x < x + ww &&
                        ctx->input.y >= oy && ctx->input.y < oy + UI_INPUT_H);
            if (j == w->combo_selected || hot)
                ui_painter_fill_rect(x + 1, oy + 1, ww - 2, UI_INPUT_H - 2, UI_C_PANEL_ALT);
            float fs = UI_FS_BODY;
            float base = oy + (UI_INPUT_H + fs * 0.7f) * 0.5f;
            ui_draw_text_at(ctx, (float)(x + 8), base,
                         w->combo_options[j].ptr, w->combo_options[j].len,
                         fs, UI_C_FG);
        }
    }
}

// clears the surface and paints the titlebar plus the whole widget tree
void ui_render_frame(UiContext* ctx) {
    ui_painter_init(ctx->pixels, ctx->win_w, ctx->win_h);       // bind the painter to the surface
    ui_painter_clear(UI_C_BG);                                  // streaming full-surface clear
    ui_render_titlebar(ctx);                                    // custom chrome, if enabled
    if (ctx->root) {
        if (ctx->custom_chrome) {                               // keep the tree out of the titlebar
            ui_painter_push_clip(0, UI_TITLEBAR_H,
                                 ctx->win_w, ctx->win_h - UI_TITLEBAR_H);
            draw_widget(ctx, ctx->root, NULL);
            draw_combobox_popups(ctx, NULL);
            ui_painter_pop_clip();
        } else {
            draw_widget(ctx, ctx->root, NULL);                  // full repaint, no damage mask
            draw_combobox_popups(ctx, NULL);
        }
    }
    ui_render_dialog(ctx);                                      // modal dialog on top of everything
}

// repaints only the given damage rectangles, plus the titlebar when it is affected
void ui_render_frame_damage(UiContext* ctx, const DamageRect* rects, int n) {
    if (ctx->dialog.active) {                                   // modal dialog: repaint whole window
        ui_render_frame(ctx);
        return;
    }
    if (n <= 0) return;                                         // nothing damaged, no work
    ui_painter_init(ctx->pixels, ctx->win_w, ctx->win_h);       // bind painter to the framebuffer

    bool tb_damaged = false;                                    // did any rect touch the titlebar?

    for (int i = 0; i < n; i++) {                               // one pass per damage rect
        const DamageRect* d = &rects[i];                        // current damage rect
        if (ctx->custom_chrome && d->y < UI_TITLEBAR_H) tb_damaged = true;
        ui_painter_push_clip(d->x, d->y, d->w, d->h);           // restrict writes to this rect
        if (d->x == 0 && d->y == 0 &&
            d->w >= ctx->win_w && d->h >= ctx->win_h) {
            ui_painter_clear(UI_C_BG);                          // full surface: streaming path
        } else {
            ui_painter_fill_rect(d->x, d->y, d->w, d->h, UI_C_BG);  // partial: regular blend
        }
        if (ctx->root) {
            if (ctx->custom_chrome) {                           // keep the tree out of the titlebar
                ui_painter_push_clip(0, UI_TITLEBAR_H,
                                     ctx->win_w, ctx->win_h - UI_TITLEBAR_H);
                draw_widget(ctx, ctx->root, d);
                draw_combobox_popups(ctx, d);
                ui_painter_pop_clip();
            } else {
                draw_widget(ctx, ctx->root, d);                 // redraw widgets touching rect
                draw_combobox_popups(ctx, d);
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
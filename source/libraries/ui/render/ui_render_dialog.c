// source/libraries/ui/render/ui_render_dialog.c
// Modal dialog painter for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_render.h"
#include "ui_text.h"
#include "ui_widgets.h"
#include <string.h>

// fades any 0xAARRGGBB color by a 0..1 factor (dialog variant)
uint32_t dlg_fade(uint32_t c, double t) {
    uint32_t a = (c >> 24) & 0xFF;
    a = (uint32_t)(a * t + 0.5);
    return (c & 0x00FFFFFFu) | (a << 24);
}

// draws the currently open modal dialog on top of everything else
void ui_render_dialog(UiContext* ctx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;
    double t = d->anim_t;
    if (t <= 0.0) return;

    // backdrop
    ui_painter_fill_rect(0, 0, ctx->win_w, ctx->win_h, dlg_fade(0x8C000000u, t));

    UiDialogGeom g;
    ui_dialog_geom(ctx, &g);

    uint32_t panel_c  = dlg_fade(UI_C_PANEL,     t);
    uint32_t border_c = dlg_fade(UI_C_BORDER_S,  t);
    uint32_t fg       = dlg_fade(UI_C_FG,        t);
    uint32_t fg_dim   = dlg_fade(UI_C_FG_DIM,    t);
    uint32_t fg_muted = dlg_fade(UI_C_FG_MUTED,  t);
    uint32_t input_bg = dlg_fade(UI_C_INPUT_BG,  t);
    uint32_t input_bd = dlg_fade(UI_C_BORDER,    t);

    ui_painter_fill_round(g.bx, g.by, g.bw, g.bh, DLG_RADIUS, panel_c);
    ui_painter_stroke_round(g.bx, g.by, g.bw, g.bh, DLG_RADIUS, border_c);

    if (d->title && *d->title)
        ui_draw_text_at(ctx, (float)g.tx, (float)g.ty,
                     d->title, (int)strlen(d->title), DLG_TITLE_FS, fg);

    if (d->message && *d->message) {
        const char* s = d->message;
        int len = (int)strlen(s);
        float max_w = (float)(g.bw - 2 * DLG_PAD);
        float py = (float)g.my;
        int i = 0;
        while (i < len) {
            // a hard break (\n, \r, \r\n) closes the current segment
            int seg_end = i;
            while (seg_end < len && s[seg_end] != '\n' && s[seg_end] != '\r') seg_end++;

            // greedy word wrap inside [i, seg_end), flushing one line at a time
            float cur = 0;
            int line_start = i;
            int j = i;
            while (j < seg_end) {
                while (j < seg_end && (s[j] == ' ' || s[j] == '\t')) j++;
                if (j >= seg_end) break;
                int ws = j;
                while (j < seg_end && s[j] != ' ' && s[j] != '\t') j++;
                float ww = ui_text_measure_cached(ctx, s + ws, j - ws, DLG_BODY_FS);
                float sp = cur > 0 ? ui_text_measure_cached(ctx, " ", 1, DLG_BODY_FS) : 0;
                if (cur + sp + ww > max_w && cur > 0) {
                    int le = ws;
                    while (le > line_start && s[le - 1] == ' ') le--;
                    if (le > line_start)
                        ui_draw_text_at(ctx, (float)g.mx, py,
                                     s + line_start, le - line_start,
                                     DLG_BODY_FS, fg);
                    py += DLG_LINE_H;
                    line_start = ws;
                    cur = ww;
                } else {
                    cur += sp + ww;
                }
            }
            // flush the final line of this segment
            int le = seg_end;
            while (le > line_start && s[le - 1] == ' ') le--;
            if (le > line_start)
                ui_draw_text_at(ctx, (float)g.mx, py,
                             s + line_start, le - line_start,
                             DLG_BODY_FS, fg);
            py += DLG_LINE_H;

            i = seg_end;
            if (i < len && s[i] == '\r' && i + 1 < len && s[i + 1] == '\n') i += 2;
            else if (i < len) i++;
        }
    }

    // fields
    int label_h = (int)(DLG_LABEL_FS * 1.4f) + 6;
    for (int i = 0; i < d->field_count; i++) {
        int fy = g.field_y + i * (g.field_row_h + 6);
        UiDialogField* f = &d->fields[i];
        if (f->label && *f->label)
            ui_draw_text_at(ctx, (float)(g.bx + DLG_PAD),
                         (float)(fy + (int)(DLG_LABEL_FS * 1.0f)),
                         f->label, (int)strlen(f->label),
                         DLG_LABEL_FS, fg_dim);
        int iy = fy + label_h;
        ui_painter_fill_round(g.bx + DLG_PAD, iy, g.input_w, DLG_INPUT_H, 6, input_bg);
        ui_painter_stroke_round(g.bx + DLG_PAD, iy, g.input_w, DLG_INPUT_H, 6, input_bd);

        float base = iy + (DLG_INPUT_H + DLG_BODY_FS * 0.7f) * 0.5f;
        if (f->input && f->input_len > 0) {
            ui_draw_text_at(ctx, (float)(g.bx + DLG_PAD + 10), base,
                         f->input, f->input_len, DLG_BODY_FS, fg);
        } else if (f->placeholder && *f->placeholder) {
            ui_draw_text_at(ctx, (float)(g.bx + DLG_PAD + 10), base,
                         f->placeholder, (int)strlen(f->placeholder),
                         DLG_BODY_FS, fg_muted);
        }
        if (d->focused_field == i && !d->closing && d->blink_phase < 0.5) {
            float cx = (float)(g.bx + DLG_PAD + 10);
            if (ctx->font && f->input && f->cursor > 0)
                cx += ui_text_measure_cached(ctx, f->input, f->cursor, DLG_BODY_FS);
            ui_painter_fill_rect((int)cx, iy + 6, 1, DLG_INPUT_H - 12, fg);
        }
    }

    // buttons
    for (int i = 0; i < g.btn_count; i++) {
        UiDialogButton* b = &d->buttons[i];
        uint32_t bg;
        if (b->primary)
            bg = (d->active_btn == i) ? UI_C_ACCENT_A
               : (d->hover_btn  == i) ? UI_C_ACCENT_H
                                      : UI_C_ACCENT;
        else
            bg = (d->active_btn == i) ? UI_C_ACCENT_A
               : (d->hover_btn  == i) ? UI_C_ACCENT_H
                                      : UI_C_PANEL;
        uint32_t bd = dlg_fade(b->primary ? UI_C_BORDER_S : UI_C_BORDER, t);
        uint32_t tx_c = dlg_fade(b->primary ? UI_C_ACCENT_T : UI_C_FG,   t);

        ui_painter_fill_round(g.btn_x[i], g.button_y, g.btn_w[i], g.button_h, 6,
                              dlg_fade(bg, t));
        ui_painter_stroke_round(g.btn_x[i], g.button_y, g.btn_w[i], g.button_h, 6, bd);

        const char* lbl = b->label ? b->label : "";
        float tw = ui_text_measure_cached(ctx, lbl, (int)strlen(lbl), DLG_BODY_FS);
        float x0 = g.btn_x[i] + (g.btn_w[i] - tw) * 0.5f;
        float base = g.button_y + (g.button_h + DLG_BODY_FS * 0.7f) * 0.5f;
        ui_draw_text_at(ctx, x0, base, lbl, (int)strlen(lbl), DLG_BODY_FS, tx_c);
    }
}
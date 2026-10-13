// source/libraries/ui/render/ui_render_notif.c
// Notification popup painter for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_render.h"
#include "ui_text.h"
#include "ui_window.h"
#include <string.h>

// fades any 0xAARRGGBB color by a 0..1 factor
static uint32_t dlg_fade_static(uint32_t c, double t) {
    uint32_t a = (c >> 24) & 0xFF;                       // extract the source alpha
    a = (uint32_t)(a * t + 0.5);                         // scale by the phase, rounded
    return (c & 0x00FFFFFFu) | (a << 24);                // reassemble rgb with the new alpha
}

// paints one notification into its own framebuffer and blits to its window
void ui_render_notification(UiNotification* n) {
    if (!n || !n->window) return;                        // null guard
    UiContext* ctx = n->ctx;                             // owning context for the font and helpers

    int w = n->win_w;                                    // popup client width
    int h = n->win_h;                                    // popup client height
    uint32_t* px = ui_window_pixels(n->window);          // popup framebuffer
    if (!px) return;                                     // no surface: nothing to paint

    ui_painter_init(px, w, h);                           // bind the painter to this surface
    ui_painter_clear(0x00000000u);                       // transparent: no window background

    // fade factor
    double t = n->anim_t;                                // 0 = hidden, 1 = fully visible
    if (t < 0) t = 0;                                    // clamp low
    if (t > 1) t = 1;                                    // clamp high

    uint32_t bg     = dlg_fade_static(UI_C_NOTIF_BG,     t);  // panel background, faded
    uint32_t border = dlg_fade_static(UI_C_NOTIF_BORDER, t);  // panel border, faded
    uint32_t title  = dlg_fade_static(UI_C_NOTIF_TITLE,  t);  // title text, faded
    uint32_t body   = dlg_fade_static(UI_C_NOTIF_BODY,   t);  // body text, faded
    uint32_t fgcl   = dlg_fade_static(UI_C_NOTIF_CLOSE,  t);  // close glyph, faded

    // panel with rounded corners
    ui_painter_fill_round(0, 0, w, h, UI_NOTIF_RADIUS, bg);        // filled rounded body
    ui_painter_stroke_round(0, 0, w, h, UI_NOTIF_RADIUS, border);  // 1px rounded outline

    // close button glyph at top-right, hover/press feedback
    if (n->show_close) {                                 // only when the close button is enabled
        int cx = w - UI_NOTIF_PAD - UI_NOTIF_CLOSE_SZ / 2;        // close button center x
        int cy = UI_NOTIF_PAD + UI_NOTIF_CLOSE_SZ / 2;            // close button center y
        if (n->press_close)                              // pressed: strongest highlight
            ui_painter_fill_round(cx - UI_NOTIF_CLOSE_SZ/2, cy - UI_NOTIF_CLOSE_SZ/2,
                                  UI_NOTIF_CLOSE_SZ, UI_NOTIF_CLOSE_SZ,
                                  UI_NOTIF_CLOSE_SZ/2,
                                  dlg_fade_static(UI_C_NOTIF_CLOSE_HOV, t));  // round hover chip
        else if (n->hover_close)                         // hover: same chip, softer state
            ui_painter_fill_round(cx - UI_NOTIF_CLOSE_SZ/2, cy - UI_NOTIF_CLOSE_SZ/2,
                                  UI_NOTIF_CLOSE_SZ, UI_NOTIF_CLOSE_SZ,
                                  UI_NOTIF_CLOSE_SZ/2,
                                  dlg_fade_static(UI_C_NOTIF_CLOSE_HOV, t));  // round hover chip
        blit_close_x_shared(cx - 4, cy - 4, fgcl);       // 9x9 close glyph, centered on the chip
    }

    int inner = w - 2*UI_NOTIF_PAD;                      // usable width inside the padding
    if (n->show_close) inner -= UI_NOTIF_CLOSE_SZ + 6;   // reserve room for the close button

    int ty = UI_NOTIF_PAD;                               // running y cursor inside the panel
    if (n->title && *n->title) {                         // draw the title when present
        float base = (float)(ty + UI_NOTIF_TITLE_FS);    // baseline of the title line
        ui_text_draw(ctx->font, px, w, h,
                     (float)UI_NOTIF_PAD, base,
                     n->title, (int)strlen(n->title),
                     UI_NOTIF_TITLE_FS, title);          // draw the title text
        ty += UI_NOTIF_TITLE_FS + 6;                     // advance past the title line box
    }

    if (n->message && *n->message) {                     // draw the wrapped body when present
        const char* s = n->message;                      // body string
        int len = (int)strlen(s);                        // body length
        float max_w = (float)inner;                      // wrap budget for each line
        float py = (float)(ty + UI_NOTIF_BODY_FS);       // first baseline inside the panel
        int i = 0;                                       // byte cursor
        int line_start = i;                              // first byte of the current line
        float cur = 0;                                   // width of the current line
        while (i < len) {                                // walk the string word by word
            while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;  // skip leading whitespace
            if (i >= len) break;                         // nothing left to scan
            int ws = i;                                  // first byte of this word
            while (i < len && s[i] != ' ' && s[i] != '\t') i++;  // advance past the word
            float ww = ui_text_measure_cached(ctx, s + ws, i - ws, UI_NOTIF_BODY_FS);  // word width
            float sp = cur > 0 ? ui_text_measure_cached(ctx, " ", 1, UI_NOTIF_BODY_FS) : 0;  // space before it
            if (cur + sp + ww > max_w && cur > 0) {      // would overflow: wrap
                int le = ws;                             // end of the line to flush
                while (le > line_start && s[le - 1] == ' ') le--;  // trim trailing spaces
                if (le > line_start)                     // non-empty line: draw it
                    ui_text_draw(ctx->font, px, w, h,
                                 (float)UI_NOTIF_PAD, py,
                                 s + line_start, le - line_start,
                                 UI_NOTIF_BODY_FS, body);
                py += UI_NOTIF_LINE_H;                   // advance to the next baseline
                line_start = ws;                         // next line starts at this word
                cur = ww;                                // reset the running width
            } else {
                cur += sp + ww;                          // append the word to the current line
            }
        }
        int le = len;                                    // final line ends at the end of the string
        while (le > line_start && s[le - 1] == ' ') le--;  // trim trailing spaces
        if (le > line_start)                             // non-empty: draw the last line
            ui_text_draw(ctx->font, px, w, h,
                         (float)UI_NOTIF_PAD, py,
                         s + line_start, le - line_start,
                         UI_NOTIF_BODY_FS, body);
    }

    ui_window_present(n->window);                        // blit the finished frame to the popup
}
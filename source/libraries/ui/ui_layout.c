// source/libraries/ui/ui_layout.c
// Implementation of UI layout engine for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_text.h"
#include <math.h>
#include <string.h>

// line height for a text-bearing widget kind
static float text_line_h(WidgetKind k) {
    if (k == WK_HEADING) return 30;              // headings get extra vertical room
    return UI_LINE_H;                            // everything else uses body line height
}

// font size for a text-bearing widget kind
static float text_fs(WidgetKind k) {
    if (k == WK_HEADING) return UI_FS_H1;        // headings render at the h1 size
    return UI_FS_BODY;                           // everything else uses body size
}

// measures a string, wrapping on spaces to fit max_w, and reports line count
static float wrap_measure(UiContext* ctx, const char* s, int len, float size, float max_w,
                          int* out_lines) {
    if (!ctx->font) { if (out_lines) *out_lines = 1; return (float)len * size * 0.5f; }  // no font: rough estimate
    if (max_w <= 0) {                            // no wrap constraint: single line
        float w = ui_text_measure(ctx->font, s, len, size);
        if (out_lines) *out_lines = 1;
        return w;
    }
    float maxw = 0, cur = 0;                     // widest line so far, current line width
    int lines = 1;                               // line count, at least one
    int i = 0;
    while (i < len) {
        while (i < len && s[i] == ' ') i++;      // skip leading spaces
        int j = i;
        while (j < len && s[j] != ' ') j++;      // find end of the next word
        if (i == j) break;                       // no more words
        float w = ui_text_measure(ctx->font, s + i, j - i, size);  // width of the word
        float sp = cur > 0 ? ui_text_measure(ctx->font, " ", 1, size) : 0;  // space before it
        if (cur + sp + w > max_w && cur > 0) {   // would overflow: wrap
            if (cur > maxw) maxw = cur;          // record the finished line
            cur = w;                             // start a new line with the word
            lines++;
        } else {
            cur += sp + w;                       // append to the current line
        }
        i = j;                                   // advance past the word
    }
    if (cur > maxw) maxw = cur;                  // record the final line
    if (out_lines) *out_lines = lines;
    return maxw;                                 // widest line in the wrapped block
}

// computes the intrinsic size of a widget given an available width
static void measure(UiContext* ctx, Widget* w, float avail_w) {
    if (!w->visible) { w->meas_w = w->meas_h = 0; return; }  // invisible widgets take no space

    switch (w->kind) {
        case WK_TEXT: case WK_HEADING: {                     // single-block wrapped text
            int lines = 1;
            float maxw = avail_w > 0 ? avail_w : 1e6f;       // no limit if unconstrained
            float ww = wrap_measure(ctx, w->text.ptr, w->text.len,
                                    text_fs(w->kind), maxw, &lines);
            w->meas_w = ww;                                  // widest wrapped line
            w->meas_h = lines * text_line_h(w->kind);        // lines times line height
            break;
        }
        case WK_BUTTON: case WK_LINK: case WK_BADGE: {       // single-line labelled widgets
            float fs = (w->kind == WK_BADGE) ? UI_FS_SMALL : UI_FS_BODY;  // badges render smaller
            float tw = ctx->font ? ui_text_measure(ctx->font, w->text.ptr, w->text.len, fs) : w->text.len * 8.0f;
            if (w->kind == WK_BUTTON) {
                w->meas_w = tw + 24; w->meas_h = UI_BTN_H;   // buttons have fixed height and side padding
            } else if (w->kind == WK_BADGE) {
                w->meas_w = tw + 12; w->meas_h = fs + 8;     // badges are compact pills
            } else {
                w->meas_w = tw; w->meas_h = text_line_h(WK_TEXT);  // links hug their text
            }
            break;
        }
        case WK_CHECKBOX: case WK_RADIO: {                   // box plus label
            float fs = UI_FS_BODY;
            float tw = ctx->font ? ui_text_measure(ctx->font, w->text.ptr, w->text.len, fs) : w->text.len*8.0f;
            w->meas_w = UI_CB_SIZE + 8 + tw;                 // box, gap, then label
            w->meas_h = UI_CB_SIZE;                          // height driven by the box
            break;
        }
        case WK_SLIDER: {                                    // fixed-size track
            w->meas_w = 200; w->meas_h = 24;
            break;
        }
        case WK_PROGRESS: {                                  // fixed-size bar
            w->meas_w = 200; w->meas_h = 10;
            break;
        }
        case WK_INPUT: {                                     // editable single line
            float fs = UI_FS_BODY;
            float tw = ctx->font ? ui_text_measure(ctx->font, w->input_value.ptr,
                                                   w->input_value.len, fs) : w->input_value.len*8.0f;
            w->meas_w = (tw > 200 ? tw : 200) + 16;          // grow with content, min 200px
            w->meas_h = UI_INPUT_H;
            break;
        }
        case WK_SEPARATOR:                                   // full-width rule
            w->meas_w = avail_w > 0 ? avail_w : 100;
            w->meas_h = 1;
            break;
        case WK_SPACER:                                      // explicit empty gap
            w->meas_w = 0; w->meas_h = 0;
            break;
        case WK_IMAGE:                                       // placeholder until real sizing
            w->meas_w = 100; w->meas_h = 100;
            break;
        case WK_FRAME:                                       // fixed-size clipping frame
            w->meas_w = w->req_w > 0 ? w->req_w : 100;
            w->meas_h = w->req_h > 0 ? w->req_h : 100;
            break;
        case WK_VBOX: case WK_HBOX: case WK_PANEL:
        case WK_SCROLL: case WK_HSCROLL: case WK_GRID: {     // container kinds
            bool vert = (w->kind != WK_HBOX && w->kind != WK_HSCROLL);  // orientation
            float pad = w->padding;
            float inner_w = avail_w - pad*2;                 // available width inside padding
            if (inner_w < 0) inner_w = 0;
            float main_sum = 0, cross_max = 0;               // total main size, widest cross size
            int n = w->child_count;
            for (int i = 0; i < n; i++) {
                Widget* c = w->children[i];
                if (!c->visible) continue;                   // skip invisible children
                float ch_avail = vert ? inner_w : (avail_w > 0 ? 1e6f : inner_w);  // children of hbox measure unconstrained
                measure(ctx, c, ch_avail);                   // recurse
                if (c->req_w >= 0) c->meas_w = c->req_w;     // explicit size overrides measurement
                if (c->req_h >= 0) c->meas_h = c->req_h;
                if (vert) { main_sum += c->meas_h; if (c->meas_w > cross_max) cross_max = c->meas_w; }
                else      { main_sum += c->meas_w; if (c->meas_h > cross_max) cross_max = c->meas_h; }
            }
            if (n > 1) main_sum += w->gap * (n - 1);         // add inter-child gaps
            if (vert) {
                w->meas_w = cross_max + pad*2;               // cross size is the widest child
                w->meas_h = main_sum + pad*2;                // main size is the summed children
            } else {
                w->meas_w = main_sum + pad*2;
                w->meas_h = cross_max + pad*2;
            }
            if (w->kind == WK_PANEL) { w->meas_w += 8; w->meas_h += 8; }  // panels add a bit of frame
            break;
        }
    }
    if (w->req_w >= 0) w->meas_w = w->req_w;             // explicit requests always win
    if (w->req_h >= 0) w->meas_h = w->req_h;
    if (w->max_w > 0 && w->meas_w > w->max_w) w->meas_w = w->max_w;  // clamp to max
    if (w->min_w > 0 && w->meas_w < w->min_w) w->meas_w = w->min_w;  // clamp to min
}

// assigns final geometry to a widget and lays out its children
static void arrange(UiContext* ctx, Widget* w, float x, float y, float width, float height) {
    w->x = x; w->y = y; w->w = width; w->h = height;     // commit this widget's box
    if (!w->visible) return;                             // nothing more to do

    bool vert = (w->kind == WK_VBOX || w->kind == WK_PANEL ||
                 w->kind == WK_SCROLL || w->kind == WK_GRID);  // vertical container kinds
    bool horiz = (w->kind == WK_HBOX || w->kind == WK_HSCROLL);  // horizontal container kinds
    if (!vert && !horiz) return;                         // leaf widgets have no children to place

    float pad = w->padding;
    float ix = x + pad, iy = y + pad;                    // inner origin
    float iw = width - pad*2, ih = height - pad*2;       // inner size
    if (iw < 0) iw = 0;
    if (ih < 0) ih = 0;
    int n = w->child_count;

    int nvis = 0; float fixed = 0, tw = 0;               // visible count, fixed size, total weight
    for (int i = 0; i < n; i++) {
        Widget* c = w->children[i];
        if (!c->visible) continue;                       // skip invisible children
        nvis++;
        if (vert) fixed += c->meas_h; else fixed += c->meas_w;  // accumulate along main axis
        tw += c->weight;                                 // accumulate flex weight
    }
    float gaps = nvis > 1 ? w->gap * (nvis - 1) : 0;     // total gap space
    float main_size = vert ? ih : iw;                    // main axis size
    float free_main = main_size - fixed - gaps;          // space left to distribute

    if (w->kind == WK_SCROLL) {
        w->content_h = fixed + gaps + pad*2;             // scroll extent = content plus padding
    }

    if (free_main > 0 && tw > 0) {                       // distribute leftover to weighted children
        for (int i = 0; i < n; i++) {
            Widget* c = w->children[i];
            if (!c->visible) continue;
            float add = free_main * (c->weight / tw);    // proportional to weight
            if (vert) c->meas_h += add; else c->meas_w += add;
        }
        free_main = 0;                                   // consumed all free space
    } else if (free_main < 0 && tw > 0) {
        float deficit = -free_main;
        for (int i = 0; i < n; i++) {
            Widget* c = w->children[i];
            if (!c->visible || c->weight <= 0) continue;
            float shrink = deficit * (c->weight / tw);
            if (vert) {
                c->meas_h -= shrink;
                if (c->meas_h < 0.f) c->meas_h = 0.f;
            } else {
                c->meas_w -= shrink;
                if (c->meas_w < 0.f) c->meas_w = 0.f;
            }
        }
    }

    float lead = 0, between = w->gap;                    // leading offset and inter-child gap
    if (free_main > 0 && tw == 0 && nvis > 0) {          // no weights: honor justify
        switch (w->justify) {
            case JU_CENTER: lead = free_main * 0.5f; break;               // center the block
            case JU_END:    lead = free_main; break;                      // pack to the end
            case JU_SPACE_BETWEEN:
                if (nvis > 1) between += free_main / (nvis - 1);          // spread between children
                break;
            case JU_SPACE_AROUND: {                                       // even space around each
                float each = free_main / nvis;
                lead = each * 0.5f;
                between += each;
                break;
            }
            case JU_SPACE_EVENLY: {                                       // even space including ends
                float each = free_main / (nvis + 1);
                lead = each; between += each;
                break;
            }
            default: break;                                              // JU_START: no adjustment
        }
    }

    float pen = (vert ? iy : ix) + lead;                 // running main-axis position
    for (int i = 0; i < n; i++) {
        Widget* c = w->children[i];
        if (!c->visible) continue;
        float cw = c->meas_w, ch = c->meas_h;            // child desired size
        float cx, cy;
        if (vert) {                                      // vertical main axis
            cy = pen;
            if (c->align == AL_STRETCH) cx = ix, cw = iw;              // fill cross axis
            else if (c->align == AL_CENTER) cx = ix + (iw - cw) * 0.5f;  // centered cross
            else if (c->align == AL_END)   cx = ix + (iw - cw);          // end-aligned cross
            else                            cx = ix;                     // start-aligned cross
            if (w->kind == WK_SCROLL) cy -= (float)w->scroll_y;          // apply scroll offset
            pen += ch + between;
        } else {                                         // horizontal main axis
            cx = pen;
            if (c->align == AL_STRETCH) cy = iy, ch = ih;              // fill cross axis
            else if (c->align == AL_CENTER) cy = iy + (ih - ch) * 0.5f;
            else if (c->align == AL_END)   cy = iy + (ih - ch);
            else                            cy = iy;
            pen += cw + between;
        }
        if (c->req_w >= 0) cw = c->req_w;                // explicit size requests win
        if (c->req_h >= 0) ch = c->req_h;
        float sy = cy;
        if (w->kind == WK_SCROLL) sy = cy;               // scroll offset already applied above
        arrange(ctx, c, cx, sy, cw, ch);                 // recurse into the child
    }
}

// top-level layout: measure the root then arrange the whole tree within the window
void ui_layout(UiContext* ctx) {
    if (!ctx->root) return;                              // nothing to lay out
    measure(ctx, ctx->root, (float)ctx->win_w);          // bottom-up intrinsic sizing
    if (ctx->root->kind == WK_VBOX || ctx->root->kind == WK_HBOX ||
        ctx->root->kind == WK_PANEL) {                   // container roots fill the window
        ctx->root->meas_w = (float)ctx->win_w;
        ctx->root->meas_h = (float)ctx->win_h;
    }
    arrange(ctx, ctx->root, 0, 0, (float)ctx->win_w, (float)ctx->win_h);  // top-down placement
}
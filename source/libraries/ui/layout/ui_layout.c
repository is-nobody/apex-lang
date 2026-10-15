// source/libraries/ui/layout/ui_layout.c
// Implementation of UI layout engine for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_text.h"
#include "ui_widgets.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

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

// appends one [start, end) line range to the widget's cached breakdown
static void push_line(UiContext* ctx, Widget* w, int start, int end) {
    if (w->line_count >= w->line_cap) {
        int new_cap = w->line_cap ? w->line_cap * 2 : 8;
        int* ns = (int*)ui_arena_alloc(ctx, sizeof(int) * (size_t)new_cap);
        int* ne = (int*)ui_arena_alloc(ctx, sizeof(int) * (size_t)new_cap);
        if (w->line_count > 0) {                          // copy the live prefix forward
            memcpy(ns, w->line_starts, sizeof(int) * (size_t)w->line_count);
            memcpy(ne, w->line_ends,   sizeof(int) * (size_t)w->line_count);
        }
        w->line_starts = ns;                              // old arena block stays alive until frame end
        w->line_ends   = ne;
        w->line_cap    = new_cap;
    }
    w->line_starts[w->line_count] = start;
    w->line_ends[w->line_count]   = end;
    w->line_count++;
}

// fnv-1a over a byte range
static uint64_t wrap_hash64(uint64_t h, const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint64_t)b[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

// key for a wrapped-text cache entry
static uint64_t wrap_cache_key(const char* s, int len, float size, float max_w) {
    uint64_t h = 0xCBF29CE484222325ULL;
    h = wrap_hash64(h, &size,  sizeof(size));
    h = wrap_hash64(h, &max_w, sizeof(max_w));
    h = wrap_hash64(h, &len,   sizeof(len));
    if (len > 0) h = wrap_hash64(h, s, (size_t)len);
    return h ? h : 1;
}

// linear scan; the cache holds on the order of the number of distinct triples in the tree
static WrapCacheEntry* wrap_cache_lookup(UiContext* ctx, uint64_t key) {
    for (int i = 0; i < ctx->wrap_cache_count; i++)
        if (ctx->wrap_cache[i].key == key)
            return &ctx->wrap_cache[i];
    return NULL;
}

// copies starts/ends into malloc'd storage and appends to the cache
static WrapCacheEntry* wrap_cache_insert(UiContext* ctx, uint64_t key,
                                         const int* starts, const int* ends,
                                         int lines, float max_width) {
    if (lines <= 0) return NULL;
    if (ctx->wrap_cache_count >= ctx->wrap_cache_cap) {
        int ncap = ctx->wrap_cache_cap ? ctx->wrap_cache_cap * 2 : 64;
        WrapCacheEntry* nc = (WrapCacheEntry*)realloc(ctx->wrap_cache,
                                sizeof(WrapCacheEntry) * (size_t)ncap);
        if (!nc) return NULL;
        ctx->wrap_cache     = nc;
        ctx->wrap_cache_cap = ncap;
    }
    int* ns = (int*)malloc(sizeof(int) * (size_t)lines);
    int* ne = (int*)malloc(sizeof(int) * (size_t)lines);
    if (!ns || !ne) { free(ns); free(ne); return NULL; }
    memcpy(ns, starts, sizeof(int) * (size_t)lines);
    memcpy(ne, ends,   sizeof(int) * (size_t)lines);

    WrapCacheEntry* e = &ctx->wrap_cache[ctx->wrap_cache_count++];
    e->key        = key;
    e->last_frame = ctx->text_cache_frame;
    e->lines      = lines;
    e->max_width  = max_width;
    e->starts     = ns;
    e->ends       = ne;
    return e;
}

// drops entries whose last_frame is older than the cutoff
void ui_wrap_cache_sweep(UiContext* ctx, uint32_t cutoff) {
    int out = 0;
    for (int i = 0; i < ctx->wrap_cache_count; i++) {
        WrapCacheEntry* e = &ctx->wrap_cache[i];
        if (e->last_frame >= cutoff) {
            ctx->wrap_cache[out++] = *e;
        } else {
            free(e->starts);
            free(e->ends);
        }
    }
    ctx->wrap_cache_count = out;
}

// frees every wrap cache entry
void ui_wrap_cache_free(UiContext* ctx) {
    for (int i = 0; i < ctx->wrap_cache_count; i++) {
        free(ctx->wrap_cache[i].starts);
        free(ctx->wrap_cache[i].ends);
    }
    free(ctx->wrap_cache);
    ctx->wrap_cache       = NULL;
    ctx->wrap_cache_count = 0;
    ctx->wrap_cache_cap   = 0;
}

// measures a string, wrapping on spaces to fit max_w, and stores the resulting line ranges on the widget
static float wrap_measure(UiContext* ctx, Widget* w, const char* s, int len,
                          float size, float max_w, int* out_lines) {
    w->line_count = 0;

    if (!ctx->font) {                            // no font: rough estimate, single line
        if (out_lines) *out_lines = 1;
        push_line(ctx, w, 0, len);
        return (float)len * size * 0.5f;
    }
    if (max_w <= 0) {                            // no wrap constraint: single line
        float mw = ui_text_measure_cached(ctx, s, len, size);
        push_line(ctx, w, 0, len);
        if (out_lines) *out_lines = 1;
        return mw;
    }

    // fast path: same text at the same size and the same wrap width as a previous layout
    uint64_t key;
    if (w->wrap_key != 0 &&
        w->wrap_avail_w == max_w &&
        w->wrap_fs == size) {
        key = w->wrap_key;                    // text/size/width unchanged: reuse the digest
    } else {
        key = wrap_cache_key(s, len, size, max_w);
        w->wrap_key     = key;                // publish so the next layout can skip the hash
        w->wrap_avail_w = max_w;
        w->wrap_fs      = size;
    }
    WrapCacheEntry* ce = wrap_cache_lookup(ctx, key);
    if (ce) {
        ce->last_frame = ctx->text_cache_frame;  // mark as live for the age sweep
        w->line_starts = ce->starts;             // widgets only read these
        w->line_ends   = ce->ends;
        w->line_count  = ce->lines;
        w->line_cap    = ce->lines;
        if (out_lines) *out_lines = ce->lines;
        return ce->max_width;
    }

    // miss: walk the text, measure each word, push line ranges into the arena
    float maxw = 0, cur = 0;                     // widest line so far, current line width
    int lines = 1;                               // line count, at least one
    int i = 0;
    int line_start = -1;                         // -1 = no content byte on this line yet
    while (i < len) {
        while (i < len && s[i] == ' ') i++;      // skip leading spaces
        if (line_start < 0) line_start = i;      // first content byte of the current line
        int j = i;
        while (j < len && s[j] != ' ') j++;      // find end of the next word
        if (i == j) break;                       // no more words
        float wd = ui_text_measure_cached(ctx, s + i, j - i, size);  // width of the word
        float sp = cur > 0 ? ui_text_measure_cached(ctx, " ", 1, size) : 0;  // space before it
        if (cur + sp + wd > max_w && cur > 0) {  // would overflow: wrap
            push_line(ctx, w, line_start, i);    // [line_start, i) is this line
            if (cur > maxw) maxw = cur;          // record the widest so far
            cur = wd;                            // start a new line with the word
            line_start = i;                      // next line starts at this word
            lines++;
        } else {
            cur += sp + wd;                      // append to the current line
        }
        i = j;                                   // advance past the word
    }
    if (line_start < 0) line_start = len;        // empty text: one empty line
    push_line(ctx, w, line_start, len);          // final line runs to the end
    if (cur > maxw) maxw = cur;                  // record the final line width
    if (out_lines) *out_lines = lines;

    // publish the result so future layouts and other widgets with the same
    // text at the same size and width hit the cache instead of re-wrapping
    wrap_cache_insert(ctx, key, w->line_starts, w->line_ends,
                      w->line_count, maxw);

    return maxw;                                 // widest line in the wrapped block
}

// computes the intrinsic size of a widget given an available width
static void measure(UiContext* ctx, Widget* w, float avail_w) {
    if (!w->visible) { w->meas_w = w->meas_h = 0; return; }  // invisible widgets take no space

    switch (w->kind) {
        case WK_TEXT: case WK_HEADING: {                     // single-block wrapped text
            int lines = 1;
            float maxw = avail_w > 0 ? avail_w : 1e6f;       // no limit if unconstrained
            float ww = wrap_measure(ctx, w, w->text.ptr, w->text.len,
                                    text_fs(w->kind), maxw, &lines);
            w->meas_w = ww;                                  // widest wrapped line
            w->meas_h = lines * text_line_h(w->kind);        // lines times line height
            break;
        }
        case WK_BUTTON: case WK_BADGE: {                     // single-line labelled widgets
            float fs = (w->kind == WK_BADGE) ? UI_FS_SMALL : UI_FS_BODY;  // badges render smaller
            float tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr, w->text.len, fs) : w->text.len * 8.0f;
            if (w->kind == WK_BUTTON) {
                w->meas_w = tw + 24; w->meas_h = UI_BTN_H;   // buttons have fixed height and side padding
            } else {
                w->meas_w = tw + 12; w->meas_h = fs + 8;     // badges are compact pills
            }
            break;
        }
        case WK_CHECKBOX: case WK_RADIO: {                   // box plus label
            float fs = UI_FS_BODY;
            float tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr, w->text.len, fs) : w->text.len*8.0f;
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
            float tw = ctx->font ? ui_text_measure_cached(ctx, w->input_value.ptr,
                                                          w->input_value.len, fs) : w->input_value.len*8.0f;
            w->meas_w = (tw > 200 ? tw : 200) + 16;          // grow with content, min 200px
            w->meas_h = UI_INPUT_H;
            break;
        }
        case WK_COMBOBOX: {                                  // button plus hidden dropdown
            float fs = UI_FS_BODY;
            float maxw = 0;
            for (int i = 0; i < w->combo_option_count; i++) {
                float ww = ctx->font ? ui_text_measure_cached(ctx, w->combo_options[i].ptr,
                                                              w->combo_options[i].len, fs) : 0;
                if (ww > maxw) maxw = ww;
            }
            float vw = ctx->font ? ui_text_measure_cached(ctx, w->combo_value.ptr,
                                                          w->combo_value.len, fs) : 0;
            if (vw > maxw) maxw = vw;
            w->meas_w = maxw + 32;                           // text + side padding + chevron
            if (w->meas_w < 120) w->meas_w = 120;
            w->meas_h = UI_INPUT_H;
            break;
        }
        case WK_TABLE: {                                     // columns of text rows
            int nc = w->table_col_count;
            if (nc == 0) { w->meas_w = 0; w->meas_h = 0; break; }
            if (nc > 64) nc = 64;                            // match the render cap
            float cw[64];
            table_natural_widths(ctx, w, cw);                // content-hugging widths
            float total = 2;                                 // outer border
            for (int c = 0; c < nc; c++) total += cw[c];
            w->meas_w = total;
            int rows_drawn = (w->table_header ? 1 : 0) + w->table_row_count;
            w->meas_h = (float)rows_drawn * UI_TABLE_ROW_H + 2;
            break;
        }
        case WK_LIST_ITEM: {                                 // single-line row
            float fs = UI_FS_BODY;
            float lw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr,
                                                          w->text.len, fs)
                                 : w->text.len * 8.0f;
            w->meas_w = lw + UI_LIST_ITEM_PAD * 2;
            w->meas_h = UI_LIST_ITEM_H;
            break;
        }
        case WK_TREE: {                                      // flat row list, one row per visible node
            w->meas_w = 240;
            w->meas_h = (float)w->tree_node_count * UI_TREE_ROW_H + 2;
            break;
        }
        case WK_SWITCH: {                                    // pill track plus optional label
            float fs = UI_FS_BODY;
            float tw = 0;
            float gap = 0;
            if (w->text.len > 0) {
                tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr,
                                                        w->text.len, fs)
                               : w->text.len * 8.0f;
                gap = 8.0f;                                  // gap between track and label
            }
            w->meas_w = UI_SWITCH_W + gap + tw;              // track first, then label
            w->meas_h = UI_SWITCH_H;
            if (w->meas_h < fs + 4) w->meas_h = fs + 4;      // at least tall enough for the label
            break;
        }
        case WK_SEPARATOR:                                   // full-width rule
            w->meas_w = avail_w > 0 ? avail_w : 100;
            w->meas_h = 1;
            break;
        case WK_SPACER:                                      // explicit empty gap
            w->meas_w = 0; w->meas_h = 0;
            break;
        case WK_VBOX: case WK_HBOX: case WK_PANEL:
        case WK_SCROLL: case WK_HSCROLL: {                   // container kinds
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
            break;
        }
    }
    if (w->req_w >= 0) w->meas_w = w->req_w;             // explicit requests always win
    if (w->req_h >= 0) w->meas_h = w->req_h;
    if (w->max_w > 0 && w->meas_w > w->max_w) w->meas_w = w->max_w;  // clamp to max
    if (w->min_w > 0 && w->meas_w < w->min_w) w->meas_w = w->min_w;  // clamp to min

    // snap up to the next integer pixel
    w->meas_w = ceilf(w->meas_w);
    w->meas_h = ceilf(w->meas_h);
}

// assigns final geometry to a widget and lays out its children
static void arrange(UiContext* ctx, Widget* w, float x, float y, float width, float height) {
    // snap to whole pixels
    x      = floorf(x      + 0.5f);
    y      = floorf(y      + 0.5f);
    width  = floorf(width  + 0.5f);
    height = floorf(height + 0.5f);
    if (width  < 1.0f) width  = 1.0f;
    if (height < 1.0f) height = 1.0f;

    // a widget that keeps its exact box from the previous layout does not invalidate the hit-test grid
    if (w->x != x || w->y != y || w->w != width || w->h != height)
        ctx->geom_gen++;
    w->x = x; w->y = y; w->w = width; w->h = height;     // commit this widget's box
    if (!w->visible) return;                             // nothing more to do

    bool vert = (w->kind == WK_VBOX || w->kind == WK_PANEL ||
                 w->kind == WK_SCROLL);                      // vertical container kinds
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
        // integer accumulation (Bresenham-style)
        int free_int = (int)floorf(free_main + 0.5f);
        int given = 0;
        float accum = 0.0f;
        for (int i = 0; i < n; i++) {
            Widget* c = w->children[i];
            if (!c->visible) continue;
            accum += free_int * (c->weight / tw);
            int upto = (int)floorf(accum + 0.5f);
            int share = upto - given;
            if (vert) c->meas_h += share; else c->meas_w += share;
            given = upto;
        }
        free_main = 0;                                   // consumed all free space
    } else if (free_main < 0 && tw > 0) {
        int deficit_int = (int)floorf(-free_main + 0.5f);
        int taken = 0;
        float accum = 0.0f;
        for (int i = 0; i < n; i++) {
            Widget* c = w->children[i];
            if (!c->visible || c->weight <= 0) continue;
            accum += deficit_int * (c->weight / tw);
            int upto = (int)floorf(accum + 0.5f);
            int share = upto - taken;
            if (vert) {
                c->meas_h -= share;
                if (c->meas_h < 1.f) c->meas_h = 1.f;
            } else {
                c->meas_w -= share;
                if (c->meas_w < 1.f) c->meas_w = 1.f;
            }
            taken = upto;
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

// computes the aggregated screen-space bbox of a subtree
static void compute_subtree_bbox(Widget* w) {
    if (!w) return;
    if (!w->visible) {                                   // invisible subtrees draw nothing
        w->bbox_x0 = w->bbox_y0 = w->bbox_x1 = w->bbox_y1 = 0;
        return;
    }
    int x0 = (int)floorf(w->x + 0.5f);
    int y0 = (int)floorf(w->y + 0.5f);
    int x1 = x0 + (int)floorf(w->w + 0.5f);
    int y1 = y0 + (int)floorf(w->h + 0.5f);

    bool clips = (w->kind == WK_SCROLL || w->kind == WK_HSCROLL);
    int cx1 = x1;
    if (w->kind == WK_SCROLL) {
        int has_sb = w->content_h > (float)w->h;         // scrollbar steals width
        cx1 = x0 + (has_sb ? (int)w->w - UI_SCROLL_W : (int)w->w);
    }

    for (int i = 0; i < w->child_count; i++) {
        Widget* c = w->children[i];
        compute_subtree_bbox(c);
        if (!c->visible) continue;                       // invisible children contribute nothing
        int bx0 = c->bbox_x0, by0 = c->bbox_y0, bx1 = c->bbox_x1, by1 = c->bbox_y1;
        if (clips) {                                     // clamp to the visible viewport
            if (bx0 < x0) bx0 = x0;
            if (by0 < y0) by0 = y0;
            if (bx1 > cx1) bx1 = cx1;
            if (by1 > y1) by1 = y1;
            if (bx0 >= bx1 || by0 >= by1) continue;      // fully clipped out
        }
        if (bx0 < x0) x0 = bx0;
        if (by0 < y0) y0 = by0;
        if (bx1 > x1) x1 = bx1;
        if (by1 > y1) y1 = by1;
    }

    // an open combobox dropdown extends the drawn area below the button
    if (w->kind == WK_COMBOBOX && w->combo_open && w->combo_option_count > 0) {
        int dropdown_bottom = y1 + w->combo_option_count * UI_INPUT_H;
        if (dropdown_bottom > y1) y1 = dropdown_bottom;
    }

    // 2px margin covers the focus ring (drawn at x-2) and antialiased edges
    w->bbox_x0 = x0 - 2;
    w->bbox_y0 = y0 - 2;
    w->bbox_x1 = x1 + 2;
    w->bbox_y1 = y1 + 2;
}

// top-level layout: measure the root then arrange the whole tree within the window
void ui_layout(UiContext* ctx) {
    if (!ctx->root) return;                              // nothing to lay out
    float rx = ctx->root->x;                             // preserve caller-set root rect
    float ry = ctx->root->y;
    float rw = ctx->root->w;
    float rh = ctx->root->h;
    if (rw <= 0) rw = (float)ctx->win_w;                 // fall back to window size
    if (rh <= 0) rh = (float)ctx->win_h;
    measure(ctx, ctx->root, rw);                         // bottom-up intrinsic sizing
    if (ctx->root->kind == WK_VBOX || ctx->root->kind == WK_HBOX ||
        ctx->root->kind == WK_PANEL) {                   // container roots fill the given rect
        ctx->root->meas_w = rw;
        ctx->root->meas_h = rh;
    }
    uint32_t gen0 = ctx->geom_gen;                       // snapshot the geometry generation
    arrange(ctx, ctx->root, rx, ry, rw, rh);             // top-down placement
    compute_subtree_bbox(ctx->root);                     // aggregate bboxes for the damage reject
    // only rebuild the grid when the tree was reset
    if (!ctx->hit_grid_valid || ctx->geom_gen != gen0)
        hit_grid_build(ctx, ctx->root);                  // refresh the uniform hit-test grid
}
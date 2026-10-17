// source/libraries/ui/core/ui_widget.c
// Widget pool, reset, kind names, focus helpers for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_core.h"
#include "ui_hash.h"
#include "ui_widgets.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>

// grows the widget pool, doubling capacity each time
static void pool_grow(UiContext* ctx) {
    ctx->pool_cap = ctx->pool_cap ? ctx->pool_cap * 2 : 256;  // start at 256, then double
    ctx->pool = (Widget*)realloc(ctx->pool, sizeof(Widget) * ctx->pool_cap);
}

// allocates and zero-initialises the next widget slot from the pool
Widget* ui_widget_alloc(UiContext* ctx) {
    if (ctx->pool_used >= ctx->pool_cap) pool_grow(ctx);  // grow pool if exhausted
    Widget* w = &ctx->pool[ctx->pool_used++];             // take next slot
    memset(w, 0, sizeof(*w));                             // clear every field
    w->req_w = w->req_h = -1;                             // no explicit size requested
    w->align = AL_STRETCH;                                // default align: fill parent
    w->justify = JU_START;                                // default justify: pack to start
    w->visible = true;                                    // visible by default
    w->weight = 0;                                        // no flex weight
    w->gap = UI_GAP;                                      // default child gap
    w->padding = 0;                                       // no padding
    w->child_cap = 0; w->children = NULL;                 // no child storage yet
    w->key_str = NULL;                                    // no user key
    w->on_top = false;                                    // draw in tree order by default
    return w;                                             // return freshly initialised slot
}

// releases every heap allocation owned by pooled widgets and rewinds the pool
void ui_widgets_reset(UiContext* ctx) {
    // the widget pool no longer owns any heap buffers directly
    ctx->pool_used = 0;                                   // reset pool cursor
    ctx->root = NULL;                                     // drop tree root
    ctx->geometry_dirty = true;                           // fresh tree needs a layout pass
    ctx->hit_grid_valid = false;                          // grid holds pointers into the old tree

    // bulk-free text/group/key_str by releasing every arena block
    UiArenaBlock* ab = ctx->arena;
    while (ab) {
        UiArenaBlock* next = ab->next;
        free(ab);
        ab = next;
    }
    ctx->arena = NULL;

    // the text-measurement cache is keyed by hash(content, size), not by pointer into the arena
    ctx->text_cache_frame++;

    // drop wrap cache entries that no widget has asked for in the last 120 rebuilds
    {
        uint32_t age_cutoff = ctx->text_cache_frame > 120
                            ? ctx->text_cache_frame - 120
                            : 1;
        ui_wrap_cache_sweep(ctx, age_cutoff);
    }

    if (ctx->text_cache_cap > 0 && ctx->text_cache_count > 0 &&
        ctx->text_cache_count * 4 > ctx->text_cache_cap * 3 &&
        ctx->text_cache_frame - ctx->text_cache_swept >= 30) {
        ctx->text_cache_swept = ctx->text_cache_frame;

        uint32_t age_cutoff = ctx->text_cache_frame > 120
                            ? ctx->text_cache_frame - 120
                            : 1;                      // keep everything on the first sweeps

        TextMeasureEntry* nc =
            (TextMeasureEntry*)calloc((size_t)ctx->text_cache_cap, sizeof(TextMeasureEntry));
        if (nc) {
            uint32_t mask = (uint32_t)(ctx->text_cache_cap - 1);
            int kept = 0;
            for (int i = 0; i < ctx->text_cache_cap; i++) {
                TextMeasureEntry* e = &ctx->text_cache[i];
                if (e->hash == 0) continue;               // empty slot
                if (e->last_frame < age_cutoff) continue; // stale: drop
                uint32_t s = (uint32_t)e->hash & mask;
                while (nc[s].hash != 0) s = (s + 1) & mask;
                nc[s] = *e;                               // carries hash, width, last_frame
                kept++;
            }
            free(ctx->text_cache);
            ctx->text_cache = nc;
            ctx->text_cache_count = kept;
        }
    }
}

// appends a child to a widget, growing the child array from the arena as needed
void widget_add_child(UiContext* ctx, Widget* p, Widget* c) {
    if (p->child_count >= p->child_cap) {                 // child array is full
        int new_cap = p->child_cap ? p->child_cap * 2 : 4;   // start at 4, then double
        Widget** new_arr = (Widget**)ui_arena_alloc(ctx, sizeof(Widget*) * (size_t)new_cap);
        if (p->children && p->child_count > 0)            // copy the live prefix forward
            memcpy(new_arr, p->children, sizeof(Widget*) * (size_t)p->child_count);
        p->children = new_arr;                            // old arena block stays alive until frame end
        p->child_cap = new_cap;
    }
    p->children[p->child_count++] = c;                    // append at end
    c->parent = p;                                        // back-link to parent
}

// maps a widget kind enum to its lowercase string name
const char* widget_kind_name(WidgetKind k) {
    switch (k) {
        case WK_TEXT: return "text";
        case WK_HEADING: return "heading";
        case WK_BUTTON: return "button";
        case WK_CHECKBOX: return "checkbox";
        case WK_RADIO: return "radio";
        case WK_SLIDER: return "slider";
        case WK_PROGRESS: return "progress";
        case WK_INPUT: return "input";
        case WK_BADGE: return "badge";
        case WK_COMBOBOX: return "combobox";
        case WK_TABLE: return "table";
        case WK_LIST_ITEM: return "list_item";
        case WK_SWITCH: return "switch";
        case WK_TREE:   return "tree";
        case WK_TABS:   return "tabs";
        case WK_SEPARATOR: return "separator";
        case WK_SPACER: return "spacer";
        case WK_VBOX: return "vbox";
        case WK_HBOX: return "hbox";
        case WK_PANEL: return "panel";
        case WK_SCROLL: return "scroll";
        case WK_HSCROLL: return "hscroll";
        case WK_NUMBER_INPUT: return "number_input";
    }
    return "widget";                                      // fallback for unknown kinds
}

// returns true if the widget kind participates in keyboard focus
bool widget_focusable(WidgetKind k) {
    return k == WK_BUTTON || k == WK_CHECKBOX || k == WK_RADIO ||
           k == WK_SLIDER || k == WK_INPUT || k == WK_NUMBER_INPUT || k == WK_COMBOBOX ||
           k == WK_SWITCH;
}

// collects focusable widgets in draw order into a caller-supplied array
void collect_focusable(Widget* w, Widget** arr, int* n, int cap) {
    if (!w || !w->visible || w->disabled) return;                 // skip invisible/disabled
    if (widget_focusable(w->kind) && *n < cap) arr[(*n)++] = w;   // append if focusable and room
    for (int i = 0; i < w->child_count; i++)
        collect_focusable(w->children[i], arr, n, cap);           // recurse into children
}

// memoized ui_text_measure: same (content, size) is measured at most once per frame
float ui_text_measure_cached(UiContext* ctx, const char* s, int len, float size) {
    if (!ctx->font || !s || len <= 0) return 0.0f;    // nothing to measure

    uint64_t key = text_measure_hash(s, len, size);   // key for this measurement

    if (ctx->text_cache_cap > 0) {                    // probe the open-addressed table
        uint32_t mask = (uint32_t)(ctx->text_cache_cap - 1);
        uint32_t slot = (uint32_t)key & mask;
        for (;;) {
            TextMeasureEntry* e = &ctx->text_cache[slot];
            if (e->hash == 0) break;                  // empty: miss
            if (e->hash == key) {                     // hit
                e->last_frame = ctx->text_cache_frame;  // mark as live for the age sweep
                return e->width;
            }
            slot = (slot + 1) & mask;
        }
    }

    float w = ui_text_measure(ctx->font, s, len, size);  // miss: measure for real

    if (ctx->text_cache_cap == 0 ||
        (ctx->text_cache_count + 1) * 4 > ctx->text_cache_cap * 3) {  // grow at 75% load
        int new_cap = ctx->text_cache_cap ? ctx->text_cache_cap * 2 : 256;
        TextMeasureEntry* nc = (TextMeasureEntry*)calloc(new_cap, sizeof(TextMeasureEntry));
        if (nc) {                                     // rehash existing entries into the new table
            uint32_t new_mask = (uint32_t)(new_cap - 1);
            for (int i = 0; i < ctx->text_cache_cap; i++) {
                if (ctx->text_cache[i].hash == 0) continue;
                uint32_t s2 = (uint32_t)ctx->text_cache[i].hash & new_mask;
                while (nc[s2].hash != 0) s2 = (s2 + 1) & new_mask;
                nc[s2] = ctx->text_cache[i];
            }
            free(ctx->text_cache);
            ctx->text_cache = nc;
            ctx->text_cache_cap = new_cap;
        }                                             // oom: fall through with old table
    }

    if (ctx->text_cache_cap > 0) {                    // insert the fresh measurement
        uint32_t mask = (uint32_t)(ctx->text_cache_cap - 1);
        uint32_t slot = (uint32_t)key & mask;
        while (ctx->text_cache[slot].hash != 0) slot = (slot + 1) & mask;
        ctx->text_cache[slot].hash       = key;
        ctx->text_cache[slot].width      = w;
        ctx->text_cache[slot].last_frame = ctx->text_cache_frame;  // fresh entry is live
        ctx->text_cache_count++;
    }
    return w;
}
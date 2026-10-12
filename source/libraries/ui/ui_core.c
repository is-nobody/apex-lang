// source/libraries/ui/ui_core.c
// Implementation of UI library core for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_window.h"
#include "ui_text.h"
#include "ui_module.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

// full widget tree rebuild then repaint
static void rebuild_and_draw(UiContext* ctx);

// repaint without rebuilding the tree
static void redraw_only(UiContext* ctx);

// host window paint hook
static void ui_window_paint_cb(void* user, int w, int h);

// hit test for titlebar buttons: 0 none, 1 min, 2 max, 3 close
static int  titlebar_button_at(UiContext* ctx, int x, int y);

// true when (x, y) is inside the draggable portion of the titlebar
static bool titlebar_is_drag(UiContext* ctx, int x, int y);

// returns the resize edge name at (x, y), or null
static const char* titlebar_resize_edge(UiContext* ctx, int x, int y);

// forward declarations for the input-edit helpers
static void push_input_change(UiContext* ctx, Widget* w, const char* str_val);

// forward: dialog helpers defined further down
void dialog_clear(UiDialog* d);

// fnv-1a hash of parent id + key (or index) to produce a stable widget id
uint64_t ui_hash_id(uint64_t parent, const char* key, int idx) {
    uint64_t h = 0xCBF29CE484222325ULL ^ parent;     // fold parent id into the seed
    if (key) {
        while (*key) { h ^= (unsigned char)*key++; h *= 0x100000001B3ULL; }  // hash each key byte
    } else {
        h ^= (uint64_t)(idx + 1);                    // no key: mix in 1-based positional index
        h *= 0x100000001B3ULL;
    }
    return h ? h : 1;                                // never return 0, id 0 is reserved for "none"
}

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

// forward: defined near arena_dup further down; used here and by push_line
static void* arena_alloc(UiContext* ctx, size_t n);

// appends a child to a widget, growing the child array from the arena as needed
static void widget_add_child(UiContext* ctx, Widget* p, Widget* c) {
    if (p->child_count >= p->child_cap) {                 // child array is full
        int new_cap = p->child_cap ? p->child_cap * 2 : 4;   // start at 4, then double
        Widget** new_arr = (Widget**)arena_alloc(ctx, sizeof(Widget*) * (size_t)new_cap);
        if (p->children && p->child_count > 0)            // copy the live prefix forward
            memcpy(new_arr, p->children, sizeof(Widget*) * (size_t)p->child_count);
        p->children = new_arr;                            // old arena block stays alive until frame end
        p->child_cap = new_cap;
    }
    p->children[p->child_count++] = c;                    // append at end
    c->parent = p;                                        // back-link to parent
}

// mixes a 64-bit widget id into a 32-bit slot hash
static inline uint32_t state_hash(uint64_t id) {
    uint64_t h = id * 0x9E3779B97F4A7C15ULL;   // 64-bit golden-ratio step
    return (uint32_t)(h ^ (h >> 32));          // fold high half down
}

// allocates or grows the index and reinserts every existing entry
static void state_index_grow(UiContext* ctx) {
    int need = (ctx->state.count + 1) * 2;             // load factor <= 0.5
    int cap = 8;
    while (cap < need) cap <<= 1;                       // next power of two
    free(ctx->state.index);
    ctx->state.index = (int*)malloc(sizeof(int) * cap);
    ctx->state.index_cap = cap;
    for (int i = 0; i < cap; i++) ctx->state.index[i] = -1;
    uint32_t mask = (uint32_t)(cap - 1);
    for (int i = 0; i < ctx->state.count; i++) {        // reinsert live entries
        uint32_t slot = state_hash(ctx->state.entries[i].id) & mask;
        while (ctx->state.index[slot] >= 0) slot = (slot + 1) & mask;
        ctx->state.index[slot] = i;
    }
}

// o(1) average lookup: id -> state entry, creating on miss
WidgetState* ui_state_get(UiContext* ctx, uint64_t id) {
    if (ctx->state.index == NULL) state_index_grow(ctx);        // lazy init on first call

    uint32_t mask = (uint32_t)(ctx->state.index_cap - 1);       // power-of-two mask for slot folding
    uint32_t slot = state_hash(id) & mask;                      // starting slot from id hash
    for (;;) {                                                  // linear probe until an empty slot
        int idx = ctx->state.index[slot];                       // entry index stored in this slot
        if (idx < 0) break;                                     // empty slot: id is not present
        if (ctx->state.entries[idx].id == id)                   // id matches this entry
            return &ctx->state.entries[idx];                    // hit: hand back the live state
        slot = (slot + 1) & mask;                               // collide: advance to next slot
    }

    // miss: append a fresh entry, then insert it into the index
    if (ctx->state.count >= ctx->state.cap) {                   // entries array is full
        ctx->state.cap = ctx->state.cap ? ctx->state.cap * 2 : 64;  // double, or start at 64
        ctx->state.entries = (WidgetState*)realloc(ctx->state.entries,
                                sizeof(WidgetState) * ctx->state.cap);  // grow the array
    }
    if ((ctx->state.count + 1) * 2 > ctx->state.index_cap) {    // load factor would exceed 0.5
        state_index_grow(ctx);                                  // rehash: keeps probes short
        mask = (uint32_t)(ctx->state.index_cap - 1);            // recompute mask after growth
        slot = state_hash(id) & mask;                           // recompute starting slot
        while (ctx->state.index[slot] >= 0) slot = (slot + 1) & mask;  // find an empty slot
    }

    WidgetState* s = &ctx->state.entries[ctx->state.count];     // next free entry slot
    memset(s, 0, sizeof(*s));                                   // clear every field
    s->id = id;                                                 // bind to the widget id
    s->cursor = -1;                                             // no cursor override yet
    s->sel_anchor = -1;                                         // no active selection
    s->seen = false;                                            // marked when the gc touches it
    ctx->state.index[slot] = ctx->state.count;                  // publish index -> entry mapping
    ctx->state.count++;                                         // advance the entry cursor
    return s;                                                   // return the fresh entry
}

// marks the state entry backing each live widget, recursively
static void state_gc_mark(UiContext* ctx, Widget* w) {
    if (!w) return;
    ui_state_get(ctx, w->id)->seen = true;                // touch creates or reuses the entry
    for (int i = 0; i < w->child_count; i++)
        state_gc_mark(ctx, w->children[i]);
}

// drops state entries no widget referenced this frame
static void state_gc_sweep(UiContext* ctx) {
    int out = 0;
    for (int i = 0; i < ctx->state.count; i++) {
        WidgetState* s = &ctx->state.entries[i];
        if (s->seen) {
            s->seen = false;                              // clear for the next frame
            ctx->state.entries[out++] = *s;               // compact live entry forward
        } else {
            for (int j = 0; j < s->undo_count; j++) free(s->undo_stack[j].text);
            for (int j = 0; j < s->redo_count; j++) free(s->redo_stack[j].text);
            free(s->undo_stack);                          // release the undo history
            free(s->redo_stack);                          // release the redo history
            free(s->input_buf);                           // release the owned text buffer
        }
    }
    ctx->state.count = out;                               // truncate the tail

    // rebuild the index after compaction so lookups stay consistent
    if (ctx->state.index) {
        for (int i = 0; i < ctx->state.index_cap; i++)
            ctx->state.index[i] = -1;                     // clear all slots
        uint32_t mask = (uint32_t)(ctx->state.index_cap - 1);
        for (int i = 0; i < ctx->state.count; i++) {
            uint32_t slot = state_hash(ctx->state.entries[i].id) & mask;
            while (ctx->state.index[slot] >= 0) slot = (slot + 1) & mask;
            ctx->state.index[slot] = i;
        }
    }
}

// appends an event to the pending event queue
void ui_event_push(UiContext* ctx, const UiEvent* ev) {
    if (ev->kind == EV_CHANGE && ev->widget_id != 0) {
        for (int i = 0; i < ctx->queue.count; i++) {
            UiEvent* q = &ctx->queue.events[i];
            if (q->kind == EV_CHANGE && q->widget_id == ev->widget_id) {
                free(q->str_value);            // release the superseded payload
                *q = *ev;                      // take ownership of the new one
                return;
            }
        }
    }

    if (ctx->queue.count >= ctx->queue.cap) {             // event queue is full
        ctx->queue.cap = ctx->queue.cap ? ctx->queue.cap * 2 : 32;  // start at 32, then double
        ctx->queue.events = (UiEvent*)realloc(ctx->queue.events,
                                sizeof(UiEvent) * ctx->queue.cap);
    }
    ctx->queue.events[ctx->queue.count++] = *ev;          // copy event into the queue
}

// fnv-1a over a byte range;
static uint64_t hash_bytes64(uint64_t h, const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;                              // byte cursor over the range
    for (size_t i = 0; i < n; i++) {                                   // one step per byte
        h ^= (uint64_t)b[i];                                           // xor next byte into state
        h *= 0x100000001B3ULL;                                         // multiply by fnv-1a prime
    }
    return h;                                                          // running digest
}

// how many leading/trailing bytes of a string field are folded into the visual hash
#define VIS_HASH_EDGE 16

// hashes a string field by (length, first vis_hash_edge bytes, last vis_hash_edge bytes)
static uint64_t hash_str_sampled(uint64_t h, const char* s, int len) {
    h = hash_bytes64(h, &len, sizeof(len));               // length: catches inserts/deletes outright
    if (!s || len <= 0) return h;                         // empty: nothing else to fold in

    int edge = VIS_HASH_EDGE;
    if (len <= edge * 2) {                                // short string: hash it whole
        return hash_bytes64(h, s, (size_t)len);
    }
    h = hash_bytes64(h, s, (size_t)edge);                 // leading window
    h = hash_bytes64(h, s + len - edge, (size_t)edge);    // trailing window
    return h;
}

// hash of (size, length, content) used as the text measurement cache key
static uint64_t text_measure_hash(const char* s, int len, float size) {
    uint64_t h = 0xCBF29CE484222325ULL;               // fnv-1a offset basis
    h = hash_bytes64(h, &size, sizeof(size));         // fold font size first
    h = hash_bytes64(h, &len,  sizeof(len));          // then the length
    if (len > 0) h = hash_bytes64(h, s, (size_t)len); // then the payload bytes
    return h ? h : 1;                                 // 0 is reserved for empty slots
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

// digest of every field that affects how a widget renders
uint64_t ui_visual_hash(Widget* w) {
    uint64_t nv_bits, sy_bits, ch_bits;
    memcpy(&nv_bits, &w->num_value, sizeof(nv_bits));   // bit-exact, no aliasing UB
    memcpy(&sy_bits, &w->scroll_y,  sizeof(sy_bits));
    memcpy(&ch_bits, &w->content_h, sizeof(ch_bits));

    // fingerprint covers every field below that can change between rebuilds
    uint64_t fp = 0xCBF29CE484222325ULL;
    fp ^= (uint64_t)(uint32_t)w->hovered    <<  0;
    fp ^= (uint64_t)(uint32_t)w->pressed    <<  1;
    fp ^= (uint64_t)(uint32_t)w->focused    <<  2;
    fp ^= (uint64_t)(uint32_t)w->cursor     <<  3;
    fp ^= (uint64_t)(uint32_t)w->sel_anchor << 23;
    fp ^= (uint64_t)(uint32_t)w->combo_open     <<  5;
    fp ^= (uint64_t)(uint32_t)w->combo_selected <<  6;
    fp ^= nv_bits;
    fp ^= sy_bits;
    fp ^= ch_bits;
    fp ^= (uint64_t)(uintptr_t)w->input_value.ptr;
    fp ^= (uint64_t)(uint32_t)w->input_value.len << 43;

    if (fp == w->visual_hash_fp && w->visual_hash_cache != 0)
        return w->visual_hash_cache;                                   // state unchanged

    uint64_t h = 0xCBF29CE484222325ULL;                                // fnv-1a offset basis
    h = hash_bytes64(h, &w->kind,      sizeof(w->kind));               // widget kind
    h = hash_bytes64(h, &w->visible,   sizeof(w->visible));            // visibility flag
    h = hash_bytes64(h, &w->disabled,  sizeof(w->disabled));           // disabled flag
    h = hash_bytes64(h, &w->hovered,   sizeof(w->hovered));            // hover flag
    h = hash_bytes64(h, &w->pressed,   sizeof(w->pressed));            // pressed flag
    h = hash_bytes64(h, &w->focused,   sizeof(w->focused));            // focus flag
    h = hash_bytes64(h, &w->checked,   sizeof(w->checked));            // checkbox/radio state
    h = hash_bytes64(h, &w->cursor,    sizeof(w->cursor));             // input caret position
    h = hash_bytes64(h, &w->sel_anchor, sizeof(w->sel_anchor));        // input selection anchor
    h = hash_bytes64(h, &w->num_value, sizeof(w->num_value));          // slider/progress value
    h = hash_bytes64(h, &w->scroll_y,  sizeof(w->scroll_y));           // scroll offset
    h = hash_bytes64(h, &w->content_h, sizeof(w->content_h));          // scroll content height
    h = hash_bytes64(h, &w->combo_open,     sizeof(w->combo_open));    // dropdown expanded
    h = hash_bytes64(h, &w->combo_selected, sizeof(w->combo_selected));// selected option index

    // str fields carry a fresh heap copy each rebuild
    h = hash_str_sampled(h, w->text.ptr,        w->text.len);          // label/content
    h = hash_str_sampled(h, w->input_value.ptr, w->input_value.len);   // input text
    h = hash_str_sampled(h, w->group.ptr,       w->group.len);         // radio group name
    h = hash_str_sampled(h, w->combo_value.ptr, w->combo_value.len);

    w->visual_hash_fp    = fp;                                         // publish the memo
    w->visual_hash_cache = h;
    return h;                                                          // final widget digest
}

// widgets can draw effects a couple of pixels outside their own box
#define SNAP_MARGIN 2

// recursively capture geometry and visual hash into the given table
static void snapshot_walk(Widget* w, SnapshotTable* t) {
    if (!w) return;                                                    // null guard
    if (t->count >= t->cap) {                                          // need room for another entry
        t->cap = t->cap ? t->cap * 2 : 64;                             // double capacity (or start at 64)
        t->entries = (WidgetSnapshot*)realloc(t->entries,              // grow the snapshot array
                                              sizeof(WidgetSnapshot) * t->cap);
    }
    WidgetSnapshot* e = &t->entries[t->count++];                       // take the next free slot
    e->id   = w->id;                                                   // stable widget identity
    e->x    = (int)floorf(w->x + 0.5f) - SNAP_MARGIN;                  // rounded screen x, padded
    e->y    = (int)floorf(w->y + 0.5f) - SNAP_MARGIN;                  // rounded screen y, padded
    e->w    = (int)floorf(w->w + 0.5f) + 2*SNAP_MARGIN;                // rounded screen width, padded
    int snap_h = (int)floorf(w->h + 0.5f) + 2*SNAP_MARGIN;             // rounded screen height, padded
    if (w->kind == WK_COMBOBOX && w->combo_open && w->combo_option_count > 0)
        snap_h += w->combo_option_count * UI_INPUT_H;                  // include the open dropdown
    e->h = snap_h;
    e->hash = ui_visual_hash(w);                                       // visual-state digest
    for (int i = 0; i < w->child_count; i++) snapshot_walk(w->children[i], t);  // recurse into children
}

void ui_snapshot_capture(UiContext* ctx, Widget* root) {
    ctx->snap_cur.count = 0;                                           // rewind the current snapshot
    snapshot_walk(root, &ctx->snap_cur);                               // walk the whole tree
}

void ui_damage_begin(UiContext* ctx) {
    ctx->damage_count = 0;                                             // forget the previous frame's damage
}

void ui_damage_add(UiContext* ctx, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;                                      // degenerate: skip
    if (x < 0) { w += x; x = 0; }                                      // clip left edge
    if (y < 0) { h += y; y = 0; }                                      // clip top edge
    if (x + w > ctx->win_w) w = ctx->win_w - x;                        // clip right edge
    if (y + h > ctx->win_h) h = ctx->win_h - y;                        // clip bottom edge
    if (w <= 0 || h <= 0) return;                                      // fully clipped: skip

    if (ctx->damage_count >= ctx->damage_cap) {                        // need room for another rect
        ctx->damage_cap = ctx->damage_cap ? ctx->damage_cap * 2 : 16;  // double capacity (or start at 16)
        ctx->damage = (DamageRect*)realloc(ctx->damage,                // grow the damage array
                                           sizeof(DamageRect) * ctx->damage_cap);
    }
    ctx->damage[ctx->damage_count++] = (DamageRect){x, y, w, h};       // append the clamped rect
}

// diff the previous snapshot against the current tree, filling ctx->damage
void ui_damage_diff(UiContext* ctx) {
    SnapshotTable* prev = &ctx->snap_prev;                             // previous frame's snapshot
    SnapshotTable* cur  = &ctx->snap_cur;                              // current frame's snapshot

    // no prior frame: every entry in cur is new
    if (prev->count == 0) {
        for (int j = 0; j < cur->count; j++) {
            WidgetSnapshot* e = &cur->entries[j];
            ui_damage_add(ctx, e->x, e->y, e->w, e->h);
        }
        return;
    }
    // nothing drawn this frame: every prev entry was removed
    if (cur->count == 0) {
        for (int i = 0; i < prev->count; i++) {
            WidgetSnapshot* e = &prev->entries[i];
            ui_damage_add(ctx, e->x, e->y, e->w, e->h);
        }
        return;
    }

    // ensure the id->index table and seen-flag array can hold prev
    if (ctx->snap_prev_index_cap < prev->count * 2) {
        int cap = 8;
        while (cap < prev->count * 2) cap <<= 1;                       // next power of two, load <= 0.5
        free(ctx->snap_prev_index);
        ctx->snap_prev_index = (int*)malloc(sizeof(int) * (size_t)cap);
        ctx->snap_prev_index_cap = cap;
    }
    if (ctx->snap_prev_seen_cap < prev->count) {
        ctx->snap_prev_seen_cap = prev->count;
        ctx->snap_prev_seen = (uint8_t*)realloc(ctx->snap_prev_seen,
                                                (size_t)prev->count);
    }

    uint32_t mask = (uint32_t)(ctx->snap_prev_index_cap - 1);
    for (int i = 0; i < ctx->snap_prev_index_cap; i++) ctx->snap_prev_index[i] = -1;
    memset(ctx->snap_prev_seen, 0, (size_t)prev->count);

    // insert every prev entry into the id -> index table
    for (int i = 0; i < prev->count; i++) {
        uint32_t slot = state_hash(prev->entries[i].id) & mask;        // same mix used by the state map
        while (ctx->snap_prev_index[slot] >= 0) slot = (slot + 1) & mask;
        ctx->snap_prev_index[slot] = i;
    }

    // walk cur once; each lookup is O(1) average, each match marks the prev entry seen
    for (int j = 0; j < cur->count; j++) {
        WidgetSnapshot* n = &cur->entries[j];
        uint32_t slot = state_hash(n->id) & mask;
        int found = -1;
        for (;;) {
            int idx = ctx->snap_prev_index[slot];
            if (idx < 0) break;                                        // empty: id was not in prev
            if (prev->entries[idx].id == n->id) { found = idx; break; }
            slot = (slot + 1) & mask;
        }

        if (found < 0) {                                               // widget appeared this frame
            ui_damage_add(ctx, n->x, n->y, n->w, n->h);
        } else {
            ctx->snap_prev_seen[found] = 1;                            // mark as consumed
            WidgetSnapshot* o = &prev->entries[found];
            if (o->x != n->x || o->y != n->y ||
                o->w != n->w || o->h != n->h || o->hash != n->hash) {
                ui_damage_add(ctx, o->x, o->y, o->w, o->h);            // erase old position
                ui_damage_add(ctx, n->x, n->y, n->w, n->h);            // draw new position
            }
        }
    }

    // any prev entry not consumed this frame is a widget that went away
    for (int i = 0; i < prev->count; i++) {
        if (ctx->snap_prev_seen[i]) continue;
        WidgetSnapshot* e = &prev->entries[i];
        ui_damage_add(ctx, e->x, e->y, e->w, e->h);
    }
}

static int rects_overlap(const DamageRect* a, const DamageRect* b) {
    return !(a->x + a->w <= b->x || b->x + b->w <= a->x ||             // separated on x-axis
             a->y + a->h <= b->y || b->y + b->h <= a->y);              // or on y-axis
}

static void rect_union(DamageRect* out, const DamageRect* a, const DamageRect* b) {
    int x0 = a->x < b->x ? a->x : b->x;                                // min left edge
    int y0 = a->y < b->y ? a->y : b->y;                                // min top edge
    int x1 = (a->x + a->w) > (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);  // max right edge
    int y1 = (a->y + a->h) > (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);  // max bottom edge
    out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0;      // write bounding union
}

// sort key for the damage sweep: left edge, then top edge for determinism
static int cmp_damage_x(const void* a, const void* b) {
    const DamageRect* ra = (const DamageRect*)a;
    const DamageRect* rb = (const DamageRect*)b;
    if (ra->x != rb->x) return (ra->x > rb->x) - (ra->x < rb->x);
    return (ra->y > rb->y) - (ra->y < rb->y);
}

// collapse overlapping damage rects
void ui_damage_finalize(UiContext* ctx) {
    int n = ctx->damage_count;
    if (n < 2) return;                                     // nothing to merge

    qsort(ctx->damage, (size_t)n, sizeof(DamageRect), cmp_damage_x);

    int out = 0;                                           // number of live entries in [0, out)
    for (int i = 0; i < n; i++) {
        DamageRect r = ctx->damage[i];
        if (r.w <= 0 || r.h <= 0) continue;                // skip degenerate input

        // repeatedly absorb any live output rect that overlaps r
        bool merged_any = false;
        bool progress   = true;
        while (progress) {
            progress = false;
            for (int j = 0; j < out; j++) {
                DamageRect* o = &ctx->damage[j];
                if (o->w <= 0) continue;                   // tombstone from an earlier merge
                if (!rects_overlap(o, &r)) continue;

                DamageRect u;
                rect_union(&u, o, &r);
                long a_old = (long)o->w * o->h + (long)r.w * r.h;
                long a_new = (long)u.w * u.h;
                if (a_new > a_old * 4) continue;           // union balloons: leave both alone

                *o = u;
                r  = u;
                o->w = 0;                                  // tombstone: compacted below
                progress   = true;
                merged_any = true;
                break;                                     // rescan from the top
            }
        }

        // compact tombstones before appending
        if (merged_any) {
            int w = 0;
            for (int j = 0; j < out; j++)
                if (ctx->damage[j].w > 0)
                    ctx->damage[w++] = ctx->damage[j];
            out = w;
        }

        ctx->damage[out++] = r;                            // publish the merged rect
    }
    ctx->damage_count = out;
}

// swap cur/prev snapshots so the next frame diffs against this one
void ui_damage_swap(UiContext* ctx) {
    SnapshotTable t = ctx->snap_prev;                                  // stash previous
    ctx->snap_prev = ctx->snap_cur;                                    // previous becomes current
    ctx->snap_cur  = t;                                                // reused buffer as scratch
}

void ui_damage_free(UiContext* ctx) {
    free(ctx->damage);                                                 // release damage rect array
    free(ctx->snap_cur.entries);                                       // release current snapshot
    free(ctx->snap_prev.entries);                                      // release previous snapshot
    free(ctx->snap_prev_index);                                        // release the id->index map
    free(ctx->snap_prev_seen);                                         // release the seen-flag array
    ctx->damage            = NULL;                                     // clear dangling pointer
    ctx->snap_cur.entries  = NULL;                                     // clear dangling pointer
    ctx->snap_prev.entries = NULL;                                     // clear dangling pointer
    ctx->snap_prev_index   = NULL;                                     // clear dangling pointer
    ctx->snap_prev_seen    = NULL;                                     // clear dangling pointer
    ctx->snap_prev_index_cap = 0;                                      // reset index capacity
    ctx->snap_prev_seen_cap  = 0;                                      // reset seen capacity
    ctx->damage_count = ctx->damage_cap = 0;                           // reset damage list
    ctx->snap_cur.count = ctx->snap_cur.cap = 0;                       // reset current snapshot
    ctx->snap_prev.count = ctx->snap_prev.cap = 0;                     // reset previous snapshot
}

// maps a widget kind enum to its lowercase string name
static const char* widget_kind_name(WidgetKind k) {
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
        case WK_SEPARATOR: return "separator";
        case WK_SPACER: return "spacer";
        case WK_VBOX: return "vbox";
        case WK_HBOX: return "hbox";
        case WK_PANEL: return "panel";
        case WK_SCROLL: return "scroll";
        case WK_HSCROLL: return "hscroll";
    }
    return "widget";                                      // fallback for unknown kinds
}

// interns a c string into the vm's intern table, returning a tagged string value
static Value intern(VM* vm, const char* s) {
    return MAKE_STRING(string_intern(&vm->intern_table, s, (int)strlen(s)));
}

// converts a native ui event into an apex table value for the script layer
Value ui_event_to_table(VM* vm, const UiEvent* ev) {
    Table* t = table_create(14);                          // event table with room for ~10 fields
    Value k, v;
    const char* ty = "none";                              // default type string
    switch (ev->kind) {
        case EV_CLOSE:  ty = "close";  break;
        case EV_CLICK:  ty = "click";  break;
        case EV_CHANGE: ty = "change"; break;
        case EV_KEY:    ty = "key";    break;
        case EV_SCROLL: ty = "scroll"; break;
        case EV_HOVER:  ty = "hover";  break;
        case EV_RESIZE: ty = "resize"; break;
        case EV_DIALOG: ty = "dialog"; break;
        default: break;
    }

    k = intern(vm, "type");                               // "type" = event kind string
    v = intern(vm, ty);
    table_set(t, k, v); value_decref(k); value_decref(v);

    k = intern(vm, "key");                                // "key" = user key string
    if (ev->key_name[0]) {
        v = MAKE_STRING(string_create(ev->key_name, (int)strlen(ev->key_name)));
        table_set(t, k, v); value_decref(k); value_decref(v);
    } else {
        v = intern(vm, "");                               // empty string when no user key
        table_set(t, k, v); value_decref(k); value_decref(v);
    }

    k = intern(vm, "widget");                             // "widget" = widget type name
    v = intern(vm, ev->widget_type);
    table_set(t, k, v); value_decref(k); value_decref(v);

    k = intern(vm, "value");                              // "value" = event payload, type varies
    if (ev->str_value) {
        v = MAKE_STRING(string_create(ev->str_value, (int)strlen(ev->str_value)));
    } else if (strcmp(ev->widget_type, "slider") == 0 ||
               strcmp(ev->widget_type, "progress") == 0 ||
               ev->kind == EV_SCROLL) {
        v = MAKE_NUMBER(ev->num_value);                   // numeric payload
    } else {
        v = MAKE_BOOL(ev->bool_value);                    // default to boolean payload
    }
    table_set(t, k, v); value_decref(k);
    if ((v & QNAN) == QNAN) value_decref(v);              // release heap-tagged payload

    if (ev->kind == EV_KEY) {                             // extra fields for key events
        k = intern(vm, "key_code"); v = MAKE_NUMBER((double)ev->key_code);
        table_set(t, k, v); value_decref(k);
        k = intern(vm, "mods"); v = MAKE_NUMBER((double)ev->mods);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_SCROLL) {                          // extra field for scroll events
        k = intern(vm, "scroll_dy"); v = MAKE_NUMBER(ev->scroll_dy);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_CLICK || ev->kind == EV_HOVER) {   // extra fields for pointer events
        k = intern(vm, "x"); v = MAKE_NUMBER((double)ev->mouse_x);
        table_set(t, k, v); value_decref(k);
        k = intern(vm, "y"); v = MAKE_NUMBER((double)ev->mouse_y);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_DIALOG) {                          // "action" = ok / cancel / close
        k = intern(vm, "action");
        const char* act = (ev->num_value == 1.0) ? "ok" : "cancel";
        v = intern(vm, act);
        table_set(t, k, v); value_decref(k); value_decref(v);
    }
    return MAKE_TABLE(t);                                 // hand the event table to the caller
}

// reads a string field from a table, returns a heap copy or null on miss
char* table_get_str_dup(VM* vm, Table* t, const char* key) {
    Value k = intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return NULL; }  // key absent
    value_decref(k);
    if (!IS_STRING(v)) { value_decref(v); return NULL; }         // wrong type
    StringObject* s = AS_STRING(v);
    char* out = (char*)malloc(s->length + 1);                    // allocate copy buffer
    memcpy(out, s->chars, s->length);                            // copy bytes
    out[s->length] = 0;                                          // null terminate
    value_decref(v);                                             // release the table's value
    return out;                                                  // caller owns the copy
}

// reads a number field from a table, returns default on miss or wrong type
static double table_get_num(VM* vm, Table* t, const char* key, double def) {
    Value k = intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return def; }   // key absent
    value_decref(k);
    double r = IS_NUMBER(v) ? AS_NUMBER(v) : def;                // use value only if numeric
    value_decref(v);
    return r;
}

// reads a boolean field from a table, returns default on miss or wrong type
static bool table_get_bool(VM* vm, Table* t, const char* key, bool def) {
    Value k = intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return def; }   // key absent
    value_decref(k);
    bool r = IS_BOOL(v) ? AS_BOOL(v) : def;                      // use value only if boolean
    value_decref(v);
    return r;
}

// reads a string field via an already-interned key
static bool table_get_str_view(Table* t, Value key, StrView* out) {
    Value v;
    if (!table_get(t, key, &v)) return false;
    if (!IS_STRING(v)) { value_decref(v); return false; }
    StringObject* s = AS_STRING(v);
    out->ptr = s->chars;
    out->len = s->length;
    value_decref(v);                                 // table keeps its own ref: buffer stays alive
    return true;
}

// reads a number field via a cached key; returns default on miss or wrong type. no interning.
static double table_get_num_key(Table* t, Value key, double def) {
    Value v;
    if (!table_get(t, key, &v)) return def;
    double r = IS_NUMBER(v) ? AS_NUMBER(v) : def;
    value_decref(v);
    return r;
}

// reads a boolean field via a cached key; returns default on miss or wrong type. no interning.
static bool table_get_bool_key(Table* t, Value key, bool def) {
    Value v;
    if (!table_get(t, key, &v)) return def;
    bool r = IS_BOOL(v) ? AS_BOOL(v) : def;
    value_decref(v);
    return r;
}

// length-exact comparison against a string literal, no allocation
static inline bool str_eq_n(const char* s, int n, const char* lit, int lit_n) {
    return n == lit_n && memcmp(s, lit, (size_t)n) == 0;
}
#define STR_EQ(s, n, lit) str_eq_n((s), (n), (lit), (int)(sizeof(lit) - 1))

// length-aware variant of kind_from_str
static WidgetKind kind_from_str_n(const char* s, int n) {
    if (STR_EQ(s,n,"text"))      return WK_TEXT;
    if (STR_EQ(s,n,"heading"))   return WK_HEADING;
    if (STR_EQ(s,n,"button"))    return WK_BUTTON;
    if (STR_EQ(s,n,"checkbox"))  return WK_CHECKBOX;
    if (STR_EQ(s,n,"radio"))     return WK_RADIO;
    if (STR_EQ(s,n,"slider"))    return WK_SLIDER;
    if (STR_EQ(s,n,"progress"))  return WK_PROGRESS;
    if (STR_EQ(s,n,"input"))     return WK_INPUT;
    if (STR_EQ(s,n,"badge"))     return WK_BADGE;
    if (STR_EQ(s,n,"combobox"))  return WK_COMBOBOX;
    if (STR_EQ(s,n,"separator")) return WK_SEPARATOR;
    if (STR_EQ(s,n,"spacer"))    return WK_SPACER;
    if (STR_EQ(s,n,"vbox"))      return WK_VBOX;
    if (STR_EQ(s,n,"hbox"))      return WK_HBOX;
    if (STR_EQ(s,n,"panel"))     return WK_PANEL;
    if (STR_EQ(s,n,"scroll"))    return WK_SCROLL;
    if (STR_EQ(s,n,"hscroll"))   return WK_HSCROLL;
    return WK_TEXT;
}

// length-aware variant of parse_align
static Align parse_align_n(const char* s, int n) {
    if (STR_EQ(s,n,"start"))   return AL_START;
    if (STR_EQ(s,n,"center"))  return AL_CENTER;
    if (STR_EQ(s,n,"end"))     return AL_END;
    if (STR_EQ(s,n,"stretch")) return AL_STRETCH;
    return AL_STRETCH;
}

// length-aware variant of parse_justify
static Justify parse_justify_n(const char* s, int n) {
    if (STR_EQ(s,n,"center"))        return JU_CENTER;
    if (STR_EQ(s,n,"end"))           return JU_END;
    if (STR_EQ(s,n,"space-between")) return JU_SPACE_BETWEEN;
    if (STR_EQ(s,n,"space-around"))  return JU_SPACE_AROUND;
    if (STR_EQ(s,n,"space-evenly"))  return JU_SPACE_EVENLY;
    return JU_START;
}

// same hash as ui_hash_id but reads from a length-tagged view
static uint64_t ui_hash_id_view(uint64_t parent, const StrView* kv, int idx) {
    if (!kv) return ui_hash_id(parent, NULL, idx);
    uint64_t h = 0xCBF29CE484222325ULL ^ parent;
    for (int i = 0; i < kv->len; i++) {
        h ^= (unsigned char)kv->ptr[i];
        h *= 0x100000001B3ULL;
    }
    return h ? h : 1;
}

// default payload capacity of one arena block
#define UI_ARENA_BLOCK_SIZE (64 * 1024)

// allocates n bytes (8-aligned) from the per-frame arena
static void* arena_alloc(UiContext* ctx, size_t n) {
    n = (n + 7u) & ~(size_t)7u;                          // round up to 8 bytes
    UiArenaBlock* b = ctx->arena;                        // current (newest) block
    if (!b || b->used + (int)n > b->cap) {               // no block or no room: allocate one
        int cap = UI_ARENA_BLOCK_SIZE;
        if ((int)n > cap) cap = (int)n;                  // oversized request: one-off block
        UiArenaBlock* nb = (UiArenaBlock*)malloc(sizeof(UiArenaBlock) + (size_t)cap);
        nb->next = ctx->arena;                           // link older blocks behind
        nb->used = 0;
        nb->cap  = cap;
        ctx->arena = nb;                                 // publish the new head
        b = nb;
    }
    void* p = b->data + b->used;                         // bump the cursor inside this block
    b->used += (int)n;                                   // advance the block cursor
    return p;
}

// copies n bytes of a c string into the context's per-frame arena
static Str arena_dup(UiContext* ctx, const char* s, int n) {
    char* p = (char*)arena_alloc(ctx, (size_t)n + 1);    // payload plus null terminator
    memcpy(p, s, n);                                     // copy the payload
    p[n] = 0;                                            // null terminate
    Str r; r.ptr = p; r.len = n;                         // return by value
    return r;
}

// public wrapper so other translation units (ui_layout.c) can allocate from the arena
void* ui_arena_alloc(UiContext* ctx, size_t n) { return arena_alloc(ctx, n); }

// syncs a widget's input view against the descriptor's value_text
static void sync_input_buffer(Widget* w, WidgetState* s, const char* sval, int slen) {
    uint64_t h = hash_bytes64(0xCBF29CE484222325ULL, sval, (size_t)slen);
    if (!s->input_buf || s->input_hash != h) {           // new widget, or script changed text
        free(s->input_buf);                              // release the previous buffer
        s->input_buf = (char*)malloc((size_t)slen + 1);
        memcpy(s->input_buf, sval, (size_t)slen);
        s->input_buf[slen] = 0;
        s->input_len  = slen;
        s->input_hash = h;
    }
    w->input_value.ptr = s->input_buf;                   // publish the view on the widget
    w->input_value.len = s->input_len;
}

// recursively builds a Widget subtree from an apex table descriptor
Widget* ui_build_tree(UiContext* ctx, Value descr, Widget* parent, int idx) {
    if (!IS_TABLE(descr)) return NULL;                   // descriptors must be tables
    Table* t = AS_TABLE(descr);

    StrView tv;
    if (!table_get_str_view(t, ctx->key_cache[UK_T], &tv)) return NULL;  // missing kind: skip

    Widget* w = ui_widget_alloc(ctx);                    // allocate the widget slot
    w->kind = kind_from_str_n(tv.ptr, tv.len);           // resolve the kind enum

    StrView kv;
    uint64_t parent_id = parent ? parent->id : 0;        // root widgets hash from 0
    if (table_get_str_view(t, ctx->key_cache[UK_KEY], &kv)) {
        w->id = ui_hash_id_view(parent_id, &kv, idx);    // derive stable id
        w->key_str = arena_dup(ctx, kv.ptr, kv.len).ptr; // arena copy of the key
    } else {
        w->id = ui_hash_id(parent_id, NULL, idx);        // no key: positional fallback
    }

    StrView sv;
    if (table_get_str_view(t, ctx->key_cache[UK_ALIGN], &sv))
        w->align = parse_align_n(sv.ptr, sv.len);
    if (table_get_str_view(t, ctx->key_cache[UK_JUSTIFY], &sv))
        w->justify = parse_justify_n(sv.ptr, sv.len);

    w->gap      = (float)table_get_num_key(t, ctx->key_cache[UK_GAP], 8);           // child spacing
    w->padding  = (float)table_get_num_key(t, ctx->key_cache[UK_PADDING], 0);       // inner padding
    w->weight   = (float)table_get_num_key(t, ctx->key_cache[UK_WEIGHT], 0);        // flex weight
    w->min_w    = (float)table_get_num_key(t, ctx->key_cache[UK_MIN_WIDTH], 0);     // minimum width
    w->max_w    = (float)table_get_num_key(t, ctx->key_cache[UK_MAX_WIDTH], 0);     // maximum width
    w->disabled = table_get_bool_key(t, ctx->key_cache[UK_DISABLED], false);        // disabled flag
    w->visible  = table_get_bool_key(t, ctx->key_cache[UK_VISIBLE], true);          // visibility flag
    w->on_top   = table_get_bool_key(t, ctx->key_cache[UK_ON_TOP], false);          // draw after siblings

    Value wv;
    if (table_get(t, ctx->key_cache[UK_WIDTH], &wv)) {   // explicit width override
        if (IS_NUMBER(wv)) w->req_w = (float)AS_NUMBER(wv);
        value_decref(wv);
    }
    if (table_get(t, ctx->key_cache[UK_HEIGHT], &wv)) {  // explicit height override
        if (IS_NUMBER(wv)) w->req_h = (float)AS_NUMBER(wv);
        value_decref(wv);
    }

    StrView txt;
    bool has_txt = table_get_str_view(t, ctx->key_cache[UK_CONTENT], &txt);  // "content" preferred
    if (!has_txt) has_txt = table_get_str_view(t, ctx->key_cache[UK_LABEL], &txt);  // "label" fallback
    if (has_txt) w->text = arena_dup(ctx, txt.ptr, txt.len);

    w->checked = table_get_bool_key(t, ctx->key_cache[UK_CHECKED], false);  // checkbox/radio state

    StrView grp;
    if (table_get_str_view(t, ctx->key_cache[UK_GROUP], &grp))              // radio group name
        w->group = arena_dup(ctx, grp.ptr, grp.len);

    w->num_value = table_get_num_key(t, ctx->key_cache[UK_VALUE], 0);       // slider/progress value
    w->num_min   = table_get_num_key(t, ctx->key_cache[UK_MIN], 0);         // slider/progress min
    w->num_max   = table_get_num_key(t, ctx->key_cache[UK_MAX], 100);       // slider/progress max

    WidgetState* st = ui_state_get(ctx, w->id);             // pull persistent state

    if (w->kind == WK_COMBOBOX) {                           // combobox reads a string value + option list
        StrView vval;
        bool has_vval = table_get_str_view(t, ctx->key_cache[UK_VALUE], &vval);
        if (has_vval) w->combo_value = arena_dup(ctx, vval.ptr, vval.len);

        Value optv;
        if (table_get(t, ctx->key_cache[UK_OPTIONS], &optv)) {
            if (IS_TABLE(optv)) {
                Table* ot = AS_TABLE(optv);

                int n_arr = ot->array_part ? ot->array_count : 0;
                int n_opts = 0;
                for (int i = 0; i < n_arr; i++) {           // array part first
                    if (IS_NONE(ot->array_part[i])) break;
                    n_opts++;
                }
                for (int i = n_arr + 1; ; i++) {            // then numeric hash entries
                    Value ov;
                    if (!table_get(ot, MAKE_NUMBER((double)i), &ov)) break;
                    value_decref(ov);
                    n_opts++;
                }

                w->combo_options = (Str*)arena_alloc(ctx, sizeof(Str) * (size_t)(n_opts ? n_opts : 1));
                w->combo_option_count = 0;

                for (int i = 0; i < n_arr; i++) {           // copy each option into the arena
                    Value ov = ot->array_part[i];
                    if (IS_NONE(ov)) break;
                    if (IS_STRING(ov)) {
                        StringObject* s = AS_STRING(ov);
                        w->combo_options[w->combo_option_count++] = arena_dup(ctx, s->chars, s->length);
                    }
                }
                for (int i = n_arr + 1; ; i++) {
                    Value ov;
                    if (!table_get(ot, MAKE_NUMBER((double)i), &ov)) break;
                    if (IS_STRING(ov)) {
                        StringObject* s = AS_STRING(ov);
                        w->combo_options[w->combo_option_count++] = arena_dup(ctx, s->chars, s->length);
                    }
                    value_decref(ov);
                }
            }
            value_decref(optv);
        }

        w->combo_selected = -1;                             // resolve which option matches value
        if (has_vval) {
            for (int i = 0; i < w->combo_option_count; i++) {
                if (w->combo_options[i].len == vval.len &&
                    memcmp(w->combo_options[i].ptr, vval.ptr, (size_t)vval.len) == 0) {
                    w->combo_selected = i;
                    break;
                }
            }
        }
        w->combo_open = st->combo_open;                     // restore persisted open state
    }

    if (w->kind == WK_INPUT) {                              // input widgets own text in the state
        StrView ival;
        bool has_ival = table_get_str_view(t, ctx->key_cache[UK_VALUE_TEXT], &ival);
        if (!has_ival)
            has_ival = table_get_str_view(t, ctx->key_cache[UK_CONTENT], &ival);
        const char* init_s = has_ival ? ival.ptr : "";
        int         init_n = has_ival ? ival.len : 0;
        sync_input_buffer(w, st, init_s, init_n);
    }

    w->hovered = st->hovered;                               // restore hover flag
    w->pressed = st->pressed;                               // restore press flag
    w->focused = (ctx->focused_id == w->id);                // reflect current focus
    w->cursor  = st->cursor < 0 ? (int)w->input_value.len : st->cursor;
    w->sel_anchor = st->sel_anchor;                         // restore the selection anchor
    w->scroll_y = st->scroll_y;                             // restore scroll offset

    Value cv;
    if (table_get(t, ctx->key_cache[UK_CHILDREN], &cv)) {   // child descriptor present?
        if (IS_TABLE(cv)) {
            Table* ct = AS_TABLE(cv);

            // fast path: walk the array part directly
            int i = 1;
            int n_arr = ct->array_part ? ct->array_count : 0;
            for (; i <= n_arr; i++) {
                Value vv = ct->array_part[i - 1];
                if (IS_NONE(vv)) break;      // hole: fall through to the hash walk below
                Widget* ch = ui_build_tree(ctx, vv, w, i - 1);
                if (ch) widget_add_child(ctx, w, ch);
            }

            // slow path: indices past array_count, or holes inside it that
            // might still resolve to a numeric hash entry
            for (;; i++) {
                Value vv;
                if (!table_get(ct, MAKE_NUMBER((double)i), &vv)) break;
                Widget* ch = ui_build_tree(ctx, vv, w, i - 1);
                if (ch) widget_add_child(ctx, w, ch);
                value_decref(vv);
            }
        } else if (!IS_NONE(cv)) {                          // single child descriptor
            Widget* ch = ui_build_tree(ctx, cv, w, 0);
            if (ch) widget_add_child(ctx, w, ch);
        }
        value_decref(cv);
    }

    return w;                                               // return the built subtree
}

// lazily creates the ui context on first use, attached to the vm
UiContext* ui_ensure(VM* vm) {
    if (vm->ui) return vm->ui;                                    // already created
    UiContext* ctx = (UiContext*)calloc(1, sizeof(UiContext));    // fresh zeroed context
    ctx->vm = vm;                                                 // back-pointer to the vm
    ctx->focused_id = 0;                                          // nothing focused
    ctx->last_tree = MAKE_NONE();                                 // no tree rendered yet

    ctx->font = ui_font_load_system();                            // try to load a default font
    if (!ctx->font) {
        fprintf(stderr, "[ui] no system font available; text will not render\n");
    }

    // pre-intern every descriptor field name once; saves a hash + lookup per field per widget
    static const char* const kKeyNames[UK_COUNT] = {
        "__t", "key", "align", "justify", "gap", "padding", "weight",
        "min_width", "max_width", "disabled", "visible", "on_top",
        "width", "height", "content", "label", "checked", "group",
        "value", "min", "max", "value_text", "children", "options"
    };
    for (int i = 0; i < UK_COUNT; i++)
        ctx->key_cache[i] = intern(vm, kKeyNames[i]);

    vm->ui = ctx;                                                 // publish on the vm
    return ctx;
}

// tears down the ui context and clears the vm's back-pointer
void ui_context_destroy(UiContext* ctx) {
    if (!ctx) return;                                             // null guard
    VM* saved_vm = ctx->vm;                                       // remember vm for back-pointer clear
    ui_widgets_reset(ctx);                                        // free all per-widget allocations

    for (int i = 0; i < ctx->state.count; i++) {                  // release per-state history
        WidgetState* s = &ctx->state.entries[i];
        for (int j = 0; j < s->undo_count; j++) free(s->undo_stack[j].text);
        for (int j = 0; j < s->redo_count; j++) free(s->redo_stack[j].text);
        free(s->undo_stack);
        free(s->redo_stack);
        free(s->input_buf);                                       // release the owned text buffer
    }

    free(ctx->pool);                                              // free the widget pool array
    free(ctx->state.entries);                                     // free the persistent state table
    free(ctx->text_cache);                                        // free the text measurement cache
    free(ctx->state.index);                                       // free the state lookup index
    dialog_clear(&ctx->dialog);                                   // release any open dialog strings
    ui_wrap_cache_free(ctx);                                      // free the wrap cache and its line arrays
    for (int i = 0; i < ctx->queue.count; i++) free(ctx->queue.events[i].str_value);  // release queued event strings
    free(ctx->queue.events);                                      // free the event queue array
    if (ctx->font)   ui_font_free(ctx->font);                     // release the font
    if (ctx->window) ui_window_destroy(ctx->window);              // release the host window
    ctx->last_tree = MAKE_NONE();                                 // clear the cached tree
    ctx->has_tree = false;
    hit_grid_free(ctx);                                           // release hit-test grid
    ui_painter_shutdown();                                        // release the corner atlas
    ui_damage_free(ctx);                                          // free ui damage
    free(ctx);                                                    // free the context itself
    if (saved_vm) saved_vm->ui = NULL;                            // drop the vm's back-pointer
}

// grid cell size in pixels. 64 keeps typical widget bboxes confined to 1-2 cells.
#define HIT_CELL_SIZE 64

// releases every allocation owned by ctx->hit_grid
void hit_grid_free(UiContext* ctx) {
    free(ctx->hit_grid.cell_start);
    free(ctx->hit_grid.cell_items);
    memset(&ctx->hit_grid, 0, sizeof(ctx->hit_grid));
}

// first pass: count how many widgets land in each cell, and count cells-per-widget along the way
static void hit_grid_count(Widget* w, int cx0, int cy0, int cx1, int cy1,
                           int* counts, int cols) {
    if (!w || !w->visible) return;

    int wx0 = (int)floorf(w->x + 0.5f);
    int wy0 = (int)floorf(w->y + 0.5f);
    int wx1 = (int)floorf(w->x + w->w + 0.5f);
    int wy1 = (int)floorf(w->y + w->h + 0.5f);
    if (wx0 < cx0) wx0 = cx0;
    if (wy0 < cy0) wy0 = cy0;
    if (wx1 > cx1) wx1 = cx1;
    if (wy1 > cy1) wy1 = cy1;

    int ncx0 = cx0, ncy0 = cy0, ncx1 = cx1, ncy1 = cy1;
    if (wx1 > wx0 && wy1 > wy0) {                         // visible portion exists
        int c0 = wx0 / HIT_CELL_SIZE;
        int r0 = wy0 / HIT_CELL_SIZE;
        int c1 = (wx1 - 1) / HIT_CELL_SIZE;
        int r1 = (wy1 - 1) / HIT_CELL_SIZE;
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++)
                counts[r * cols + c]++;

        if (w->kind == WK_SCROLL) {                       // tighten clip for the subtree
            if (wx0 > ncx0) ncx0 = wx0;
            if (wy0 > ncy0) ncy0 = wy0;
            if (wx1 < ncx1) ncx1 = wx1;
            if (wy1 < ncy1) ncy1 = wy1;
        }
    }

    for (int i = 0; i < w->child_count; i++)              // draw order: normal children first
        if (!w->children[i]->on_top)
            hit_grid_count(w->children[i], ncx0, ncy0, ncx1, ncy1, counts, cols);
    for (int i = 0; i < w->child_count; i++)              // ...then overlay children
        if (w->children[i]->on_top)
            hit_grid_count(w->children[i], ncx0, ncy0, ncx1, ncy1, counts, cols);
}

// second pass: fill cell_items in the same order the first pass counted
static void hit_grid_fill(Widget* w, int cx0, int cy0, int cx1, int cy1,
                          int* cell_cursor, Widget** items, int cols) {
    if (!w || !w->visible) return;

    int wx0 = (int)floorf(w->x + 0.5f);
    int wy0 = (int)floorf(w->y + 0.5f);
    int wx1 = (int)floorf(w->x + w->w + 0.5f);
    int wy1 = (int)floorf(w->y + w->h + 0.5f);
    if (wx0 < cx0) wx0 = cx0;
    if (wy0 < cy0) wy0 = cy0;
    if (wx1 > cx1) wx1 = cx1;
    if (wy1 > cy1) wy1 = cy1;

    int ncx0 = cx0, ncy0 = cy0, ncx1 = cx1, ncy1 = cy1;
    if (wx1 > wx0 && wy1 > wy0) {
        int c0 = wx0 / HIT_CELL_SIZE;
        int r0 = wy0 / HIT_CELL_SIZE;
        int c1 = (wx1 - 1) / HIT_CELL_SIZE;
        int r1 = (wy1 - 1) / HIT_CELL_SIZE;
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++) {
                int cell = r * cols + c;
                items[cell_cursor[cell]++] = w;
            }

        if (w->kind == WK_SCROLL) {
            if (wx0 > ncx0) ncx0 = wx0;
            if (wy0 > ncy0) ncy0 = wy0;
            if (wx1 < ncx1) ncx1 = wx1;
            if (wy1 < ncy1) ncy1 = wy1;
        }
    }

    for (int i = 0; i < w->child_count; i++)
        if (!w->children[i]->on_top)
            hit_grid_fill(w->children[i], ncx0, ncy0, ncx1, ncy1, cell_cursor, items, cols);
    for (int i = 0; i < w->child_count; i++)
        if (w->children[i]->on_top)
            hit_grid_fill(w->children[i], ncx0, ncy0, ncx1, ncy1, cell_cursor, items, cols);
}

// builds ctx->hit_grid from the laid-out tree. called by ui_layout after arranging.
void hit_grid_build(UiContext* ctx, Widget* root) {
    HitGrid* g = &ctx->hit_grid;
    int ww = ctx->win_w > 0 ? ctx->win_w : 1;
    int wh = ctx->win_h > 0 ? ctx->win_h : 1;

    // early out if grid shape is unchanged: reuse the existing cells
    int cols = (ww + HIT_CELL_SIZE - 1) / HIT_CELL_SIZE;
    int rows = (wh + HIT_CELL_SIZE - 1) / HIT_CELL_SIZE;
    int ncells = cols * rows;

    if (g->cols != cols || g->rows != rows || !g->cell_start) {
        free(g->cell_start);
        free(g->cell_items);
        g->cell_start = (int*)calloc((size_t)ncells + 1, sizeof(int));
        g->cell_items = NULL;
        g->cols = cols;
        g->rows = rows;
        g->win_w = ww;
        g->win_h = wh;
    } else {
        memset(g->cell_start, 0, sizeof(int) * ((size_t)ncells + 1));
        g->win_w = ww;
        g->win_h = wh;
    }
    if (!g->cell_start) return;                           // allocation failed: leave the flag clear
    ctx->hit_grid_valid = true;                           // grid memory is live and shaped for this window
    if (!root) return;                                    // empty tree: nothing to fill in

    // pass 1: per-cell counts
    hit_grid_count(root, 0, 0, ww, wh, g->cell_start, cols);

    // prefix sum: cell_start[i] = start of cell i in cell_items
    int total = 0;
    for (int i = 0; i < ncells; i++) {
        int n = g->cell_start[i];
        g->cell_start[i] = total;
        total += n;
    }
    g->cell_start[ncells] = total;

    // allocate the flat item array (reuse the old buffer if big enough)
    g->cell_items = (Widget**)realloc(g->cell_items, sizeof(Widget*) * (size_t)(total ? total : 1));

    // pass 2: fill in draw order using a per-cell write cursor
    int* cursor = (int*)malloc(sizeof(int) * (size_t)ncells);
    memcpy(cursor, g->cell_start, sizeof(int) * (size_t)ncells);
    hit_grid_fill(root, 0, 0, ww, wh, cursor, g->cell_items, cols);
    free(cursor);
}

// finds the topmost visible widget containing (x, y) via the uniform grid
static Widget* hit_test_tree(Widget* w, int x, int y) {
    if (!w || !w->visible) return NULL;
    if (x < w->x || x >= w->x + w->w || y < w->y || y >= w->y + w->h) return NULL;

    for (int i = w->child_count - 1; i >= 0; i--) {
        if (w->children[i]->on_top) continue;                // overlay children are considered last
        Widget* r = hit_test_tree(w->children[i], x, y);
        if (r) return r;
    }
    for (int i = w->child_count - 1; i >= 0; i--) {
        if (!w->children[i]->on_top) continue;
        Widget* r = hit_test_tree(w->children[i], x, y);
        if (r) return r;
    }
    return w;
}

static Widget* hit_test(UiContext* ctx, int x, int y) {
    HitGrid* g = &ctx->hit_grid;
    if (!g->cell_start || g->cols <= 0 || g->rows <= 0)       // grid not ready
        return hit_test_tree(ctx->root, x, y);

    if (x < 0 || y < 0 || x >= g->win_w || y >= g->win_h) return NULL;

    int c = x / HIT_CELL_SIZE;
    int r = y / HIT_CELL_SIZE;
    if (c < 0 || c >= g->cols || r < 0 || r >= g->rows) return NULL;

    int cell  = r * g->cols + c;
    int start = g->cell_start[cell];
    int end   = g->cell_start[cell + 1];

    // reverse iteration: the last-written widget in the cell is the topmost drawn
    for (int i = end - 1; i >= start; i--) {
        Widget* w = g->cell_items[i];
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            return w;
    }
    return NULL;
}

// returns true if the widget kind participates in keyboard focus
static bool widget_focusable(WidgetKind k) {
    return k == WK_BUTTON || k == WK_CHECKBOX || k == WK_RADIO ||
           k == WK_SLIDER || k == WK_INPUT || k == WK_COMBOBOX;
}

// collects focusable widgets in draw order into a caller-supplied array
static void collect_focusable(Widget* w, Widget** arr, int* n, int cap) {
    if (!w || !w->visible || w->disabled) return;                 // skip invisible/disabled
    if (widget_focusable(w->kind) && *n < cap) arr[(*n)++] = w;   // append if focusable and room
    for (int i = 0; i < w->child_count; i++)
        collect_focusable(w->children[i], arr, n, cap);           // recurse into children
}

// true when the state has a live non-empty selection
static bool input_has_selection(WidgetState* s) {
    return s->sel_anchor >= 0 && s->sel_anchor != s->cursor;
}

// returns the [min, max) byte range covered by the selection
static void input_sel_range(WidgetState* s, int* out_min, int* out_max) {
    int a = s->sel_anchor;
    int b = s->cursor;
    if (a > b) { int t = a; a = b; b = t; }
    *out_min = a;
    *out_max = b;
}

// maps a local x offset inside an input widget to a byte offset in its text
static int input_hit_offset(UiContext* ctx, Widget* w, float local_x) {
    const char* s = w->input_value.ptr;
    int n = w->input_value.len;
    if (!ctx->font || n == 0) return 0;
    if (local_x <= 0) return 0;

    // walk the text one codepoint at a time, measuring each codepoint's
    float prev = 0;
    int i = 0;
    while (i < n) {
        int next = ui_utf8_next(s, i, n);
        float adv = ui_text_measure_cached(ctx, s + i, next - i, UI_FS_BODY);
        float w2 = prev + adv;
        float mid = (prev + w2) * 0.5f;
        if (local_x < mid) return i;
        i = next;
        prev = w2;
    }
    return n;
}

// finds the [start, end) byte range of the word containing pos
static void input_word_range(Widget* w, int pos, int* out_start, int* out_end) {
    const char* s = w->input_value.ptr;
    int n = w->input_value.len;
    int a = pos;
    while (a > 0) {
        int prev = ui_utf8_prev(s, a);
        unsigned char c = (unsigned char)s[prev];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        a = prev;
    }
    int b = pos;
    while (b < n) {
        unsigned char c = (unsigned char)s[b];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        b = ui_utf8_next(s, b, n);
    }
    *out_start = a;
    *out_end = b;
}

// replaces the input buffer with a new heap copy and frees the old one
static void input_set_buffer(UiContext* ctx, Widget* w, char* buf, int len) {
    WidgetState* s = ui_state_get(ctx, w->id);
    free(s->input_buf);                                     // release the previous buffer
    s->input_buf = buf;
    s->input_len = len;
    w->input_value.ptr = buf;                               // refresh the widget view
    w->input_value.len = len;
}

// pushes a snapshot of the current buffer onto the undo stack
#define INPUT_UNDO_MAX 128
static void input_push_undo(WidgetState* s, const char* text, int len, int cursor) {
    if (s->undo_count >= INPUT_UNDO_MAX) {                 // drop the oldest snapshot
        free(s->undo_stack[0].text);
        memmove(&s->undo_stack[0], &s->undo_stack[1],
                sizeof(UndoEntry) * (s->undo_count - 1));
        s->undo_count--;
    }
    if (s->undo_count >= s->undo_cap) {                    // grow the undo stack
        s->undo_cap = s->undo_cap ? s->undo_cap * 2 : 16;
        s->undo_stack = (UndoEntry*)realloc(s->undo_stack,
                                            sizeof(UndoEntry) * s->undo_cap);
    }
    char* copy = (char*)malloc((size_t)len + 1);           // snapshot the text
    memcpy(copy, text, len);
    copy[len] = 0;
    s->undo_stack[s->undo_count].text   = copy;
    s->undo_stack[s->undo_count].len    = len;
    s->undo_stack[s->undo_count].cursor = cursor;
    s->undo_count++;

    for (int i = 0; i < s->redo_count; i++) free(s->redo_stack[i].text);  // invalidate redo
    s->redo_count = 0;
}

// steps the buffer back one snapshot, moving the current state onto redo
static void input_undo(UiContext* ctx, Widget* w, WidgetState* s) {
    if (s->undo_count == 0) return;                        // nothing to undo
    if (s->redo_count >= s->redo_cap) {                    // grow redo
        s->redo_cap = s->redo_cap ? s->redo_cap * 2 : 16;
        s->redo_stack = (UndoEntry*)realloc(s->redo_stack,
                                            sizeof(UndoEntry) * s->redo_cap);
    }
    char* cur = (char*)malloc((size_t)w->input_value.len + 1);  // snapshot current
    memcpy(cur, w->input_value.ptr, w->input_value.len);
    cur[w->input_value.len] = 0;
    s->redo_stack[s->redo_count].text   = cur;
    s->redo_stack[s->redo_count].len    = w->input_value.len;
    s->redo_stack[s->redo_count].cursor = s->cursor;
    s->redo_count++;

    UndoEntry e = s->undo_stack[--s->undo_count];          // pop the previous state
    free(s->input_buf);                                    // state owns the buffer
    s->input_buf = e.text;
    s->input_len = e.len;
    w->input_value.ptr = e.text;                           // refresh the widget view
    w->input_value.len = e.len;
    s->cursor = e.cursor;
    s->sel_anchor = e.cursor;
    push_input_change(ctx, w, e.text);
    ctx->redraw_needed = true;
}

// steps the buffer forward one snapshot, moving the current state back to undo
static void input_redo(UiContext* ctx, Widget* w, WidgetState* s) {
    if (s->redo_count == 0) return;                        // nothing to redo
    if (s->undo_count >= s->undo_cap) {                    // grow undo
        s->undo_cap = s->undo_cap ? s->undo_cap * 2 : 16;
        s->undo_stack = (UndoEntry*)realloc(s->undo_stack,
                                            sizeof(UndoEntry) * s->undo_cap);
    }
    char* cur = (char*)malloc((size_t)w->input_value.len + 1);  // snapshot current
    memcpy(cur, w->input_value.ptr, w->input_value.len);
    cur[w->input_value.len] = 0;
    s->undo_stack[s->undo_count].text   = cur;
    s->undo_stack[s->undo_count].len    = w->input_value.len;
    s->undo_stack[s->undo_count].cursor = s->cursor;
    s->undo_count++;

    UndoEntry e = s->redo_stack[--s->redo_count];          // pop the forward state
    free(s->input_buf);                                    // state owns the buffer
    s->input_buf = e.text;
    s->input_len = e.len;
    w->input_value.ptr = e.text;                           // refresh the widget view
    w->input_value.len = e.len;
    s->cursor = e.cursor;
    s->sel_anchor = e.cursor;
    push_input_change(ctx, w, e.text);
    ctx->redraw_needed = true;
}

// deletes [a, b) from the input buffer, moving the caret to a
static void input_delete_range(UiContext* ctx, Widget* w, WidgetState* s, int a, int b) {
    int len = (int)w->input_value.len;
    if (a < 0) a = 0;
    if (b > len) b = len;
    if (b <= a) return;                                   // nothing to remove
    int new_len = len - (b - a);
    char* nb = (char*)malloc((size_t)new_len + 1);
    memcpy(nb, w->input_value.ptr, a);                    // prefix
    memcpy(nb + a, w->input_value.ptr + b, len - b);      // suffix
    nb[new_len] = 0;
    input_set_buffer(ctx, w, nb, new_len);
    s->cursor = a;
    s->sel_anchor = a;
    push_input_change(ctx, w, nb);
    ctx->redraw_needed = true;
}

// emits a click event for a widget, plus a change event for checkboxes
static void fire_click(UiContext* ctx, Widget* w) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));                       // zero the event
    ev.kind = EV_CLICK;                                           // click kind
    ev.widget_id = w->id;                                         // source widget id
    strncpy(ev.widget_type, widget_kind_name(w->kind), 15);       // widget type name
    if (w->key_str) strncpy(ev.key_name, w->key_str, 63);         // user key if present
    ev.mouse_x = ctx->input.x;                                    // record pointer position
    ev.mouse_y = ctx->input.y;
    ui_event_push(ctx, &ev);                                      // enqueue the click

    if (w->kind == WK_CHECKBOX) {                                 // checkboxes also fire change
        UiEvent cev; memset(&cev, 0, sizeof(cev));
        cev.kind = EV_CHANGE;
        cev.widget_id = w->id;
        strncpy(cev.widget_type, "checkbox", 15);
        if (w->key_str) strncpy(cev.key_name, w->key_str, 63);
        cev.bool_value = !w->checked;                             // new value is the inverse
        ui_event_push(ctx, &cev);
    }
    ctx->redraw_needed = true;                                    // force a repaint
}

// emits a change event for an input widget, copying the new string
static void push_input_change(UiContext* ctx, Widget* w, const char* str_val) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));                       // zero the event
    ev.kind = EV_CHANGE;                                          // change kind
    ev.widget_id = w->id;                                         // source widget id
    strncpy(ev.widget_type, "input", 15);                         // widget type name
    if (w->key_str) strncpy(ev.key_name, w->key_str, 63);         // user key if present
    ev.str_value = (char*)malloc(strlen(str_val) + 1);            // own a copy of the new text
    memcpy(ev.str_value, str_val, strlen(str_val) + 1);
    ui_event_push(ctx, &ev);                                      // enqueue the change
    ctx->geometry_dirty = true;                                   // input width may have changed
}

// releases every heap string owned by one dialog field and zeroes it
static void dialog_field_free(UiDialogField* f) {
    free(f->key);
    free(f->label);
    free(f->placeholder);
    free(f->input);
    memset(f, 0, sizeof(*f));
}

// releases every heap allocation owned by the dialog and resets it
void dialog_clear(UiDialog* d) {
    free(d->key);
    free(d->title);
    free(d->message);
    for (int i = 0; i < d->field_count; i++) dialog_field_free(&d->fields[i]);  // release each field's strings
    free(d->fields);                                                            // release the field array
    for (int i = 0; i < d->button_count; i++) {
        free(d->buttons[i].id);                                                 // release the button id
        free(d->buttons[i].label);                                              // release the button label
    }
    free(d->buttons);                                                           // release the button array
    memset(d, 0, sizeof(*d));                                                   // wipe every field
    d->hover_btn     = -1;                                                      // no hovered button
    d->active_btn    = -1;                                                      // no pressed button
    d->focused_field = -1;                                                      // no focused field
    d->pending_btn   = -1;                                                      // no pending action
}

// duplicates a c string into a fresh heap buffer, mapping null to ""
static char* dlg_dup(const char* s) {
    if (!s) s = "";                                                             // null guard: treat as empty
    size_t n = strlen(s);                                                       // payload length
    char* out = (char*)malloc(n + 1);                                           // payload plus terminator
    if (!out) return NULL;                                                      // oom: hand back null
    memcpy(out, s, n + 1);                                                      // copy bytes and null
    return out;                                                                 // caller owns the copy
}

// begins a dialog: clears any previous one and stages a fresh empty one
void dialog_begin(UiContext* ctx, const char* key, const char* title, const char* message) {
    dialog_clear(&ctx->dialog);                                                 // drop any previous dialog
    UiDialog* d = &ctx->dialog;                                                 // working pointer
    d->key     = dlg_dup(key);                                                  // copy the script key
    d->title   = dlg_dup(title);                                                // copy the heading
    d->message = dlg_dup(message);                                              // copy the body text
    d->active  = true;                                                          // mark the dialog live
    d->anim_t  = 0.0;                                                           // start fully transparent
    d->closing = false;                                                         // not fading out yet
    d->pending_btn   = -1;                                                      // no pending action
    d->hover_btn     = -1;                                                      // no hovered button
    d->active_btn    = -1;                                                      // no pressed button
    d->focused_field = -1;                                                      // no focused field
    ctx->redraw_needed = true;                                                  // schedule a repaint
    ctx->full_repaint  = true;                                                  // full repaint next frame
}

// appends one editable text field to the dialog
void dialog_add_field(UiContext* ctx, const char* key, const char* label,
                      const char* placeholder, const char* def) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op
    if (d->field_count >= d->field_cap) {                                       // field array is full
        d->field_cap = d->field_cap ? d->field_cap * 2 : 4;                     // double capacity
        d->fields = (UiDialogField*)realloc(d->fields,
                        sizeof(UiDialogField) * (size_t)d->field_cap);          // grow the array
    }
    UiDialogField* f = &d->fields[d->field_count++];                            // take the next slot
    memset(f, 0, sizeof(*f));                                                   // clear every field
    f->key         = dlg_dup(key);                                              // copy the field key
    f->label       = dlg_dup(label);                                            // copy the caption
    f->placeholder = dlg_dup(placeholder);                                      // copy the placeholder
    const char* init = def ? def : "";                                          // default text
    size_t n = strlen(init);                                                    // default length
    int cap = (int)n + 32;                                                      // room for a few edits
    f->input = (char*)malloc((size_t)cap);                                      // allocate the buffer
    if (f->input) {                                                             // allocation succeeded
        memcpy(f->input, init, n + 1);                                          // seed with the default
        f->input_len = (int)n;                                                  // record its length
        f->input_cap = cap;                                                     // record its capacity
    }
    f->cursor = f->input_len;                                                   // caret after the text
    if (d->focused_field < 0) d->focused_field = d->field_count - 1;            // focus the first field
}

// appends one button to the dialog
void dialog_add_button(UiContext* ctx, const char* id, const char* label, bool primary) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op
    if (d->button_count >= d->button_cap) {                                     // button array is full
        d->button_cap = d->button_cap ? d->button_cap * 2 : 4;                  // double capacity
        d->buttons = (UiDialogButton*)realloc(d->buttons,
                        sizeof(UiDialogButton) * (size_t)d->button_cap);        // grow the array
    }
    UiDialogButton* b = &d->buttons[d->button_count++];                         // take the next slot
    memset(b, 0, sizeof(*b));                                                   // clear every field
    b->id      = dlg_dup(id);                                                   // copy the action id
    b->label   = dlg_dup(label);                                                // copy the caption
    b->primary = primary;                                                       // mark accent fill
}

// finalises the dialog; drops it if it has no buttons
void dialog_commit(UiContext* ctx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op
    if (d->button_count == 0) dialog_clear(d);                                  // no action: drop entirely
    else ctx->redraw_needed = true;                                             // otherwise schedule a paint
}

// computes the dialog box and the per-button rectangles
void ui_dialog_geom(UiContext* ctx, UiDialogGeom* g) {
    memset(g, 0, sizeof(*g));                                                   // clear the output
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: nothing to do

    int win_w = ctx->win_w > 0 ? ctx->win_w : 640;                              // fall back to a sane width
    int win_h = ctx->win_h > 0 ? ctx->win_h : 480;                              // fall back to a sane height

    int bw = DLG_WIDTH;                                                         // desired dialog width
    if (bw > win_w - 40) bw = win_w - 40;                                       // keep it on screen
    if (bw < 280) bw = 280;                                                     // enforce a minimum

    int inner_w = bw - 2 * DLG_PAD;                                             // usable inner width

    // pass 1: count wrapped message lines
    int msg_lines = 0;
    if (d->message && *d->message) {
        const char* s = d->message;
        int len = (int)strlen(s);
        float max_w = (float)inner_w;                                           // wrap budget
        int lines = 0;                                                          // running total
        int i = 0;
        while (i < len) {
            int seg_end = i;                                                    // end of this hard-break segment
            while (seg_end < len && s[seg_end] != '\n' && s[seg_end] != '\r') seg_end++;  // scan to a break

            float cur = 0;                                                      // width of the current line
            int seg_lines = 0;                                                  // lines in this segment
            int j = i;
            while (j < seg_end) {
                while (j < seg_end && (s[j] == ' ' || s[j] == '\t')) j++;       // skip leading spaces
                if (j >= seg_end) break;                                        // no more words
                int ws = j;                                                     // first byte of this word
                while (j < seg_end && s[j] != ' ' && s[j] != '\t') j++;         // advance past the word
                float ww = ui_text_measure_cached(ctx, s + ws, j - ws, DLG_BODY_FS);  // word width
                float sp = cur > 0 ? ui_text_measure_cached(ctx, " ", 1, DLG_BODY_FS) : 0;  // space before it
                if (cur + sp + ww > max_w && cur > 0) { cur = ww; seg_lines++; }  // would overflow: wrap
                else                                  { cur += sp + ww; }       // append to the line
            }
            if (cur > 0 || seg_lines == 0) seg_lines++;                         // account for the last line
            lines += seg_lines;                                                 // accumulate

            i = seg_end;                                                        // advance past the segment
            if (i < len && s[i] == '\r' && i + 1 < len && s[i + 1] == '\n') i += 2;  // crlf: two bytes
            else if (i < len) i++;                                              // lone cr or lf: one byte
        }
        if (lines == 0) lines = 1;                                              // always at least one line
        msg_lines = lines;
    }

    // pass 2: vertical layout, y counted from the top edge of the box
    int y = 0;                                                                  // running vertical offset
    int title_off  = 0;                                                         // title baseline offset
    int msg_off    = 0;                                                         // first message baseline
    int field_off  = 0;                                                         // top of the field block
    int button_off = 0;                                                         // top of the button row

    // title baseline + descent
    if (d->title && *d->title) {
        y += DLG_PAD;                                                           // padding above the title
        title_off = y + (int)(DLG_TITLE_FS * 0.85f);                            // baseline inside its line box
        y = title_off + (int)(DLG_TITLE_FS * 0.35f);                            // leave room for the descent
    }

    // message
    if (msg_lines > 0) {
        y += (title_off > 0) ? 14 : DLG_PAD;                                    // gap above the message
        msg_off = y + DLG_LINE_H - 5;                                           // first baseline
        y = msg_off + (msg_lines - 1) * DLG_LINE_H + 5;                         // advance to the last line
    }

    // fields
    int field_row_h = (int)(DLG_LABEL_FS * 1.4f) + 6 + DLG_INPUT_H;             // caption + input height
    if (d->field_count > 0) {
        y += (title_off > 0 || msg_lines > 0) ? 14 : DLG_PAD;                   // gap above the fields
        field_off = y;                                                          // remember the block top
        y += d->field_count * field_row_h;                                      // one row per field
        if (d->field_count > 1) y += (d->field_count - 1) * 6;                  // gaps between rows
    }

    // button row
    if (title_off > 0 || msg_lines > 0 || d->field_count > 0) y += 22;          // gap above the buttons
    else                                                       y += DLG_PAD;     // no content: just padding
    button_off = y;                                                             // remember the row top
    y += DLG_BTN_H;                                                             // button height
    y += DLG_PAD;                                                               // bottom padding

    int bh = y;                                                                 // total box height
    int bx = (win_w - bw) / 2;                                                  // horizontally centered
    int by = (win_h - bh) / 2;                                                  // vertically centered
    if (by < 20) by = 20;                                                       // keep clear of the top edge

    g->bx = bx; g->by = by; g->bw = bw; g->bh = bh;                             // publish the box
    g->tx = bx + DLG_PAD;                                                       // title left edge
    g->ty = by + title_off;                                                     // title baseline
    g->mx = bx + DLG_PAD;                                                       // message left edge
    g->my = by + msg_off;                                                       // first message baseline
    g->field_y     = by + field_off;                                            // top of the field block
    g->field_row_h = field_row_h;                                               // height of one field row
    g->input_w     = inner_w;                                                   // width of the inputs
    g->button_y    = by + button_off;                                           // top of the button row
    g->button_h    = DLG_BTN_H;                                                 // button height

    // buttons: measure and right-align 
    int n = d->button_count;
    if (n > 16) n = 16;                                                         // hard cap on button count
    int widths[16];                                                             // per-button widths
    int total_w = 0;                                                            // total row width
    for (int i = 0; i < n; i++) {
        const char* lbl = d->buttons[i].label ? d->buttons[i].label : "";       // caption, or empty
        int tw = (int)ui_text_measure_cached(ctx, lbl, (int)strlen(lbl), DLG_BODY_FS);  // caption width
        int w = tw + 2 * DLG_BTN_PAD;                                           // plus side padding
        if (w < 72) w = 72;                                                     // enforce a minimum
        widths[i] = w;
        total_w += w;
    }
    if (n > 1) total_w += (n - 1) * DLG_BTN_GAP;                                // gaps between buttons

    int right = bx + bw - DLG_PAD;                                              // right edge of the row
    int cur_x = right - total_w;                                                // start of the row
    for (int i = 0; i < n; i++) {
        g->btn_x[i] = cur_x;                                                    // this button's left edge
        g->btn_w[i] = widths[i];                                                // this button's width
        cur_x += widths[i] + DLG_BTN_GAP;                                       // advance past the button
    }
    g->btn_count = n;                                                           // publish the count
}

// returns the button index under (x, y), or -1
static int dialog_button_at(UiContext* ctx, int x, int y) {
    UiDialogGeom g;
    ui_dialog_geom(ctx, &g);                                                    // resolve the row layout
    for (int i = 0; i < g.btn_count; i++) {
        if (x >= g.btn_x[i] && x < g.btn_x[i] + g.btn_w[i] &&                   // inside on x
            y >= g.button_y && y < g.button_y + g.button_h)                     // and on y
            return i;                                                           // hit: hand back the index
    }
    return -1;                                                                  // miss
}

// returns the field index under (x, y), or -1
static int dialog_field_at(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    UiDialogGeom g;
    ui_dialog_geom(ctx, &g);                                                    // resolve the field layout
    int label_h = (int)(DLG_LABEL_FS * 1.4f) + 6;                               // caption line height + gap
    for (int i = 0; i < d->field_count; i++) {
        int fy = g.field_y + i * (g.field_row_h + 6);                           // top of this row
        int iy = fy + label_h;                                                  // top of its input box
        if (x >= g.bx + DLG_PAD && x < g.bx + DLG_PAD + g.input_w &&            // inside on x
            y >= iy && y < iy + DLG_INPUT_H)                                    // and on y
            return i;                                                           // hit: hand back the index
    }
    return -1;                                                                  // miss
}

// builds the JSON payload string and pushes an EV_DIALOG event
static void dialog_emit_json(UiContext* ctx, int btn_idx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op

    size_t cap = 256;                                                           // generous starting size
    for (int i = 0; i < d->field_count; i++) {                                  // reserve room for each field
        cap += (d->fields[i].key    ? strlen(d->fields[i].key)    : 0) + 8;     // key + quoting overhead
        cap += (d->fields[i].input  ? strlen(d->fields[i].input)  : 0) * 2 + 4; // worst-case escaped value
    }
    char* buf = (char*)malloc(cap);                                             // allocate the output buffer
    if (!buf) return;                                                           // oom: nothing to emit
    size_t p = 0;                                                               // write cursor
    buf[p++] = '{';                                                             // open the outer object

    // "action":"<id>"
    static const char kAction[] = "\"action\":\"";                              // field prefix
    memcpy(buf + p, kAction, sizeof(kAction) - 1);                              // copy the prefix
    p += sizeof(kAction) - 1;
    const char* act = "close";                                                  // fallback action id
    if (btn_idx >= 0 && btn_idx < d->button_count && d->buttons[btn_idx].id)
        act = d->buttons[btn_idx].id;                                           // use the chosen button's id
    for (const char* c = act; *c && p + 2 < cap; c++) {
        if (*c == '"' || *c == '\\') buf[p++] = '\\';                           // escape quotes and slashes
        buf[p++] = *c;                                                          // copy the character
    }
    buf[p++] = '"';                                                             // close the action string

    // "fields":{...}
    if (d->field_count > 0) {                                                   // only when there are fields
        buf[p++] = ',';                                                         // separate from action
        static const char kFields[] = "\"fields\":{";                           // fields prefix
        memcpy(buf + p, kFields, sizeof(kFields) - 1);
        p += sizeof(kFields) - 1;

        for (int i = 0; i < d->field_count; i++) {
            if (i) buf[p++] = ',';                                              // separate entries
            buf[p++] = '"';                                                     // open the key string
            const char* k = d->fields[i].key ? d->fields[i].key : "";           // field key, or empty
            for (const char* c = k; *c && p + 2 < cap; c++) {
                if (*c == '"' || *c == '\\') buf[p++] = '\\';                   // escape quotes and slashes
                buf[p++] = *c;
            }
            buf[p++] = '"'; buf[p++] = ':'; buf[p++] = '"';                     // "key":"
            const char* v = d->fields[i].input ? d->fields[i].input : "";       // field value, or empty
            for (const char* c = v; *c && p + 2 < cap; c++) {
                if (*c == '"' || *c == '\\') buf[p++] = '\\';                   // escape quotes and slashes
                else if (*c == '\n') { buf[p++] = '\\'; buf[p++] = 'n'; continue; }  // escape newline
                else if (*c == '\r') { buf[p++] = '\\'; buf[p++] = 'r'; continue; }  // escape carriage return
                else if (*c == '\t') { buf[p++] = '\\'; buf[p++] = 't'; continue; }  // escape tab
                else if ((unsigned char)*c < 0x20) continue;                    // drop other control bytes
                buf[p++] = *c;                                                  // copy the character
            }
            buf[p++] = '"';                                                     // close the value string
        }
        buf[p++] = '}';                                                         // close the fields object
    }
    buf[p++] = '}';                                                             // close the outer object
    buf[p] = 0;                                                                 // null terminate

    UiEvent ev; memset(&ev, 0, sizeof(ev));                                     // zero the event
    ev.kind = EV_DIALOG;                                                        // dialog event kind
    strncpy(ev.widget_type, "dialog", 15);                                      // widget type name
    if (d->key) strncpy(ev.key_name, d->key, 63);                               // script key, if any
    ev.str_value = buf;                                                         // hand off the payload
    ui_event_push(ctx, &ev);                                                    // enqueue the event
}

// asks for a graceful close; the actual emit happens when the fade finishes
static void dialog_request_close(UiContext* ctx, int btn_idx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // already closing or gone
    d->closing     = true;                                                      // start the fade out
    d->pending_btn = btn_idx;                                                   // remember the choice
    ctx->redraw_needed = true;                                                  // schedule a repaint
}

// called from ui_tick once anim_t hits zero
static void dialog_finish_close(UiContext* ctx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // nothing to close
    dialog_emit_json(ctx, d->pending_btn);                                      // emit the result event
    dialog_clear(d);                                                            // release every allocation
    ctx->redraw_needed = true;                                                  // schedule a repaint
    ctx->full_repaint  = true;                                                  // the dialog dimmed the screen
}

// updates the hovered button on pointer move
static void dialog_mouse_move(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing
    int h = dialog_button_at(ctx, x, y);                                        // hit test the row
    if (h != d->hover_btn) { d->hover_btn = h; ctx->redraw_needed = true; }     // repaint on change
}

// handles a pointer press inside the dialog
static void dialog_mouse_down(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing

    int b = dialog_button_at(ctx, x, y);                                        // button hit test first
    if (b >= 0) { d->active_btn = b; ctx->redraw_needed = true; return; }       // press the button

    int f = dialog_field_at(ctx, x, y);                                         // field hit test next
    if (f >= 0) {
        d->focused_field = f;                                                   // focus this field
        UiDialogGeom g;
        ui_dialog_geom(ctx, &g);                                                // resolve the geometry
        UiDialogField* fld = &d->fields[f];
        int label_h = (int)(DLG_LABEL_FS * 1.4f) + 6;                           // caption height
        int iy = g.field_y + f * (g.field_row_h + 6) + label_h;                 // top of this input box
        float local = (float)(x - (g.bx + DLG_PAD) - 10);                       // into text coordinates
        if (local < 0) local = 0;                                               // clamp at the left edge
        int off = fld->input_len;                                               // default: past the end
        if (ctx->font && fld->input && fld->input_len > 0) {
            float prev = 0;
            int i = 0;
            off = fld->input_len;
            while (i < fld->input_len) {
                int next = ui_utf8_next(fld->input, i, fld->input_len);         // advance one codepoint
                float adv = ui_text_measure_cached(ctx, fld->input + i, next - i, DLG_BODY_FS);  // its width
                float w2 = prev + adv;                                          // width up to the next glyph
                if (local < (prev + w2) * 0.5f) { off = i; break; }             // landed on this glyph
                i = next;
                prev = w2;
            }
        }
        fld->cursor = off;                                                      // place the caret
        ctx->redraw_needed = true;                                              // schedule a repaint
        (void)iy;
    }
}

// handles a pointer release inside the dialog
static void dialog_mouse_up(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing
    int was = d->active_btn;                                                    // remember the pressed button
    d->active_btn = -1;                                                         // clear the press state
    if (was < 0) return;                                                        // nothing was pressed
    int hit = dialog_button_at(ctx, x, y);                                      // hit test on release
    if (hit == was) dialog_request_close(ctx, was);                             // same button: accept
    else ctx->redraw_needed = true;                                             // moved off: just repaint
}

// routes a key press to the dialog
static void dialog_key_down(UiContext* ctx, const UiWinEvent* e) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing

    if (e->key == UI_KEY_ESCAPE) {                                              // esc: pick the first non-primary
        int cancel_idx = -1;
        for (int i = 0; i < d->button_count; i++) {
            if (!d->buttons[i].primary) { cancel_idx = i; break; }              // first ghost button
        }
        dialog_request_close(ctx, cancel_idx);                                  // close with that id
        return;
    }
    if (e->key == UI_KEY_ENTER) {                                               // enter: pick the primary button
        int ok_idx = -1;
        for (int i = 0; i < d->button_count; i++)
            if (d->buttons[i].primary) { ok_idx = i; break; }                   // first accent button
        if (ok_idx < 0 && d->button_count > 0) ok_idx = d->button_count - 1;    // fallback: last button
        dialog_request_close(ctx, ok_idx);                                      // close with that id
        return;
    }
    if (e->key == UI_KEY_TAB) {                                                 // tab: cycle focused field
        if (d->field_count > 0) {
            int dir = (e->mods & 1) ? -1 : 1;                                   // shift-tab goes back
            int cur = d->focused_field;
            if (cur < 0) cur = (dir > 0) ? 0 : d->field_count - 1;              // no focus: pick an edge
            else         cur = (cur + dir + d->field_count) % d->field_count;   // step and wrap
            d->focused_field = cur;
            ctx->redraw_needed = true;
        }
        return;
    }

    int fi = d->focused_field;                                                  // active field
    if (fi < 0 || fi >= d->field_count) return;                                 // none focused: nothing to edit
    UiDialogField* f = &d->fields[fi];

    if (e->utf8_len > 0) {                                                      // printable text
        if (f->input_len + e->utf8_len + 1 > f->input_cap) {                    // buffer too small
            int nc = f->input_cap ? f->input_cap : 32;
            while (nc < f->input_len + e->utf8_len + 1) nc *= 2;                // grow to fit
            char* nb = (char*)realloc(f->input, (size_t)nc);
            if (!nb) return;                                                    // oom: drop the key
            f->input = nb;
            f->input_cap = nc;
        }
        memmove(f->input + f->cursor + e->utf8_len, f->input + f->cursor,
                (size_t)(f->input_len - f->cursor + 1));                        // make room
        memcpy(f->input + f->cursor, e->utf8, (size_t)e->utf8_len);             // insert the bytes
        f->input_len += e->utf8_len;                                            // grow the length
        f->cursor    += e->utf8_len;                                            // advance the caret
        ctx->redraw_needed = true;
        return;
    }
    if (e->key == UI_KEY_BACKSPACE) {                                           // backspace
        if (f->cursor > 0) {
            int prev = ui_utf8_prev(f->input, f->cursor);                       // start of the previous glyph
            memmove(f->input + prev, f->input + f->cursor,
                    (size_t)(f->input_len - f->cursor + 1));                    // splice it out
            f->input_len -= f->cursor - prev;                                   // shrink the length
            f->cursor    = prev;                                                // move the caret
            ctx->redraw_needed = true;
        }
        return;
    }
    if (e->key == UI_KEY_DELETE) {                                              // delete (forward)
        if (f->cursor < f->input_len) {
            int next = ui_utf8_next(f->input, f->cursor, f->input_len);         // end of the next glyph
            memmove(f->input + f->cursor, f->input + next,
                    (size_t)(f->input_len - next + 1));                         // splice it out
            f->input_len -= next - f->cursor;                                   // shrink the length
            ctx->redraw_needed = true;
        }
        return;
    }
    if (e->key == UI_KEY_LEFT)  { f->cursor = ui_utf8_prev(f->input, f->cursor); ctx->redraw_needed = true; return; }  // move caret left
    if (e->key == UI_KEY_RIGHT) { f->cursor = ui_utf8_next(f->input, f->cursor, f->input_len); ctx->redraw_needed = true; return; }  // move caret right
    if (e->key == UI_KEY_HOME)  { f->cursor = 0;            ctx->redraw_needed = true; return; }  // jump to start
    if (e->key == UI_KEY_END)   { f->cursor = f->input_len; ctx->redraw_needed = true; return; }  // jump to end
}

// hit test for titlebar buttons: 0 none, 1 min, 2 max, 3 close
static int titlebar_button_at(UiContext* ctx, int x, int y) {
    if (!ctx->custom_chrome) return 0;
    if (y < 0 || y >= UI_TITLEBAR_H) return 0;

    const int sz  = UI_TB_BTN_SZ;
    const int gap = UI_TB_BTN_GAP;
    const int pr  = UI_TB_BTN_PAD;
    int by = (UI_TITLEBAR_H - sz) / 2;
    if (by < UI_TB_RESIZE) by = UI_TB_RESIZE;            // mirror the render layout
    if (y < by || y >= by + sz) return 0;                // outside the button row

    int bx3 = ctx->win_w - pr - sz;                      // close is rightmost
    int bx2 = bx3 - gap - sz;                            // maximize in the middle
    int bx1 = bx2 - gap - sz;                            // minimize on the left

    if (x >= bx1 && x < bx1 + sz) return 1;
    if (x >= bx2 && x < bx2 + sz) return 2;
    if (x >= bx3 && x < bx3 + sz) return 3;
    return 0;
}

// true when (x, y) is inside the draggable portion of the titlebar
static bool titlebar_is_drag(UiContext* ctx, int x, int y) {
    if (!ctx->custom_chrome) return false;
    if (y < 0 || y >= UI_TITLEBAR_H) return false;
    if (titlebar_button_at(ctx, x, y) != 0) return false;
    if (y < UI_TB_RESIZE && (x < UI_TB_RESIZE || x >= ctx->win_w - UI_TB_RESIZE))
        return false;                                          // corners belong to resize
    return true;
}

// returns the resize edge name at (x, y), or null
static const char* titlebar_resize_edge(UiContext* ctx, int x, int y) {
    if (!ctx->custom_chrome) return NULL;
    int w = ctx->win_w, h = ctx->win_h;
    bool left   = x < UI_TB_RESIZE;
    bool right  = x >= w - UI_TB_RESIZE;
    bool top    = y < UI_TB_RESIZE;
    bool bottom = y >= h - UI_TB_RESIZE;
    if (top && left)     return "top-left";
    if (top && right)    return "top-right";
    if (bottom && left)  return "bottom-left";
    if (bottom && right) return "bottom-right";
    if (left)   return "left";
    if (right)  return "right";
    if (top)    return "top";
    if (bottom) return "bottom";
    return NULL;
}

// translates a raw window event into ui-layer events and state changes
static void handle_win_event(UiContext* ctx, const UiWinEvent* e) {
    switch (e->kind) {
        case UW_EV_CLOSE: {                                       // window close requested
            UiEvent ev; memset(&ev, 0, sizeof(ev));
            ev.kind = EV_CLOSE;
            ui_event_push(ctx, &ev);
            ctx->quit_requested = true;                           // mark shutdown
            break;
        }
        case UW_EV_EXPOSE: {                                      // window exposed, needs paint
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;                            // server may have cleared us
            break;
        }
        case UW_EV_RESIZE: {                                      // window resized
            ctx->pixels = ui_window_pixels(ctx->window);          // refresh pixel buffer
            ctx->win_w = e->w;                                    // new width from the event
            ctx->win_h = e->h;                                    // new height from the event
            ctx->geometry_dirty = true;                           // root rect changed: relayout
            ctx->full_repaint   = true;                           // fresh pixels are undefined
            UiEvent ev; memset(&ev, 0, sizeof(ev));
            ev.kind = EV_RESIZE;
            ev.num_value = e->w;                                  // pack new width in num_value
            ev.scroll_dy = e->h;                                  // pack new height in scroll_dy
            ui_event_push(ctx, &ev);
            ctx->redraw_needed = true;
            break;
        }
        case UW_EV_MOUSEMOVE: {                                   // pointer moved
            ctx->input.x = e->x;                                  // track current pointer
            ctx->input.y = e->y;

            if (ctx->dialog.active) {                             // modal dialog captures the pointer
                dialog_mouse_move(ctx, e->x, e->y);
                break;
            }

            if (ctx->custom_chrome) {                         // update titlebar hover feedback
                const char* edge = titlebar_resize_edge(ctx, e->x, e->y);
                ui_window_set_resize_cursor(ctx->window, edge);
                int hot = titlebar_button_at(ctx, e->x, e->y);
                if (hot != ctx->tb_hot) {
                    ctx->tb_hot = hot;
                    ctx->redraw_needed = true;
                }
            }

            if (ctx->input.lmb && ctx->drag_widget_id) {          // dragging something
                Widget* dw = NULL;
                for (int i = 0; i < ctx->pool_used; i++)
                    if (ctx->pool[i].id == ctx->drag_widget_id) {
                        dw = &ctx->pool[i];
                        break;
                    }

                if (dw && dw->kind == WK_SLIDER) {                // slider: track pointer value
                    double range = dw->num_max - dw->num_min;
                    int thumb_w = 16;
                    int track_w = (int)dw->w - thumb_w;
                    if (track_w < 1) track_w = 1;
                    int local = e->x - (int)dw->x - thumb_w / 2;
                    if (local < 0) local = 0;
                    if (local > track_w) local = track_w;
                    double t = (double)local / (double)track_w;
                    double v = dw->num_min + t * range;

                    dw->num_value = v;

                    UiEvent cev; memset(&cev, 0, sizeof(cev));
                    cev.kind = EV_CHANGE;
                    cev.widget_id = dw->id;
                    strncpy(cev.widget_type, "slider", 15);
                    if (dw->key_str) strncpy(cev.key_name, dw->key_str, 63);
                    cev.num_value = v;
                    ui_event_push(ctx, &cev);

                    ctx->redraw_needed = true;
                } else if (dw && dw->kind == WK_INPUT) {          // input: extend selection
                    WidgetState* s = ui_state_get(ctx, dw->id);
                    if (s->sel_anchor < 0) s->sel_anchor = s->cursor;  // first drag: seed anchor
                    float local_x = (float)e->x - dw->x - 8.0f;        // into text coordinates
                    int off = input_hit_offset(ctx, dw, local_x);
                    s->cursor = off;                                   // head follows the pointer
                    ctx->redraw_needed = true;
                }
            }

            // redraw open combobox dropdowns when the cursor moves over their list
            for (int i = 0; i < ctx->pool_used; i++) {
                Widget* w = &ctx->pool[i];
                if (w->kind != WK_COMBOBOX || !w->combo_open || !w->visible) continue;
                int bx = (int)floorf(w->x + 0.5f);
                int by = (int)floorf(w->y + 0.5f);
                int bw = (int)floorf(w->w + 0.5f);
                int bh = (int)floorf(w->h + 0.5f);
                int dh = w->combo_option_count * UI_INPUT_H;
                if (e->x >= bx && e->x < bx + bw &&
                    e->y >= by + bh && e->y < by + bh + dh) {
                    ctx->redraw_needed = true;
                    break;
                }
            }

            if (ctx->root) {                                      // update hover state
                Widget* h = hit_test(ctx, e->x, e->y);
                uint64_t new_hover = h ? h->id : 0;
                if (new_hover != ctx->hovered_id) {               // hover target changed
                    if (ctx->hovered_id) {                        // clear the outgoing widget
                        WidgetState* old = ui_state_get(ctx, ctx->hovered_id);
                        old->hovered = false;
                        for (int i = 0; i < ctx->pool_used; i++)
                            if (ctx->pool[i].id == ctx->hovered_id) {
                                ctx->pool[i].hovered = false;
                                break;
                            }
                    }
                    if (new_hover) {                              // mark the incoming widget
                        WidgetState* s = ui_state_get(ctx, new_hover);
                        s->hovered = true;
                        for (int i = 0; i < ctx->pool_used; i++)
                            if (ctx->pool[i].id == new_hover) {
                                ctx->pool[i].hovered = true;
                                break;
                            }
                    }
                    ctx->hovered_id = new_hover;
                    ctx->redraw_needed = true;
                }
            }
            break;
        }
        case UW_EV_MOUSEDOWN:
            if (e->button == 1) {                                 // left button only
                ctx->input.x = e->x;
                ctx->input.y = e->y;

                if (ctx->dialog.active) {                         // modal dialog is on top
                    dialog_mouse_down(ctx, e->x, e->y);
                    break;
                }

                // wm-owned gestures win over anything we draw
                const char* redge = titlebar_resize_edge(ctx, e->x, e->y);
                if (redge) {
                    ui_window_begin_resize(ctx->window, redge);
                    break;
                }
                if (titlebar_is_drag(ctx, e->x, e->y)) {
                    ui_window_begin_drag(ctx->window);
                    break;
                }

                ctx->input.lmb = true;                        // record button as held

                // an open combobox dropdown captures the click before anything else
                Widget* open_combo = NULL;
                for (int i = 0; i < ctx->pool_used; i++) {
                    Widget* w = &ctx->pool[i];
                    if (w->kind != WK_COMBOBOX || !w->combo_open || !w->visible) continue;
                    int bx = (int)floorf(w->x + 0.5f);
                    int by = (int)floorf(w->y + 0.5f);
                    int bw = (int)floorf(w->w + 0.5f);
                    int bh = (int)floorf(w->h + 0.5f);
                    int dh = w->combo_option_count * UI_INPUT_H;
                    if (e->x >= bx && e->x < bx + bw && e->y >= by + bh && e->y < by + bh + dh) {
                        open_combo = w;
                        break;
                    }
                }
                if (open_combo) {
                    int by = (int)floorf(open_combo->y + 0.5f);
                    int bh = (int)floorf(open_combo->h + 0.5f);
                    int idx = (e->y - by - bh) / UI_INPUT_H;
                    if (idx >= 0 && idx < open_combo->combo_option_count) {
                        Str* opt = &open_combo->combo_options[idx];
                        UiEvent cev; memset(&cev, 0, sizeof(cev));
                        cev.kind = EV_CHANGE;
                        cev.widget_id = open_combo->id;
                        strncpy(cev.widget_type, "combobox", 15);
                        if (open_combo->key_str) strncpy(cev.key_name, open_combo->key_str, 63);
                        cev.str_value = (char*)malloc((size_t)opt->len + 1);
                        memcpy(cev.str_value, opt->ptr, (size_t)opt->len);
                        cev.str_value[opt->len] = 0;
                        ui_event_push(ctx, &cev);
                    }
                    WidgetState* s = ui_state_get(ctx, open_combo->id);
                    s->combo_open = false;
                    open_combo->combo_open = false;
                    ctx->redraw_needed = true;
                    ctx->geometry_dirty = true;
                    break;
                }

                int tbtn = titlebar_button_at(ctx, e->x, e->y);
                if (tbtn) {
                    ctx->tb_active = tbtn;
                    ctx->redraw_needed = true;
                    break;
                }

                if (ctx->root) {
                    Widget* h = hit_test(ctx, e->x, e->y);
                    // a click outside any open combobox closes it
                    for (int i = 0; i < ctx->pool_used; i++) {
                        Widget* w = &ctx->pool[i];
                        if (w->kind != WK_COMBOBOX || !w->combo_open) continue;
                        if (h == w) continue;               // about to toggle this one: leave it
                        w->combo_open = false;
                        WidgetState* ws = ui_state_get(ctx, w->id);
                        ws->combo_open = false;
                        ctx->geometry_dirty = true;
                        ctx->redraw_needed = true;
                    }
                    if (h) {
                        WidgetState* s = ui_state_get(ctx, h->id);
                        if (h->kind == WK_COMBOBOX) {            // toggle the dropdown
                            s->combo_open = !s->combo_open;
                            h->combo_open = s->combo_open;
                            ctx->focused_id = h->id;
                            for (int i = 0; i < ctx->pool_used; i++)
                                ctx->pool[i].focused = (ctx->pool[i].id == ctx->focused_id);
                            ctx->redraw_needed = true;
                            ctx->geometry_dirty = true;
                            break;
                        }
                        s->pressed = true;                        // mark pressed
                        h->pressed = true;                        // mirror for this frame
                        // focus follows the click
                        if (widget_focusable(h->kind)) ctx->focused_id = h->id;
                        else                           ctx->focused_id = 0;
                        for (int i = 0; i < ctx->pool_used; i++)  // mirror focus to live widgets
                            ctx->pool[i].focused = (ctx->pool[i].id == ctx->focused_id);

                        // clicking inside a text input places the caret
                        if (h->kind == WK_INPUT) {
                            float local_x = (float)e->x - h->x - 8.0f;
                            int off = input_hit_offset(ctx, h, local_x);

                            double now = apex_now_seconds();
                            bool is_double = (h->id == ctx->last_click_id) &&
                                             (now - ctx->last_click_time) < 0.4;
                            ctx->last_click_id   = h->id;         // remember for next click
                            ctx->last_click_time = now;

                            if (is_double) {                      // select the word under the caret
                                int a, b;
                                input_word_range(h, off, &a, &b);
                                s->sel_anchor = a;
                                s->cursor = b;
                            } else {                              // single click: place caret
                                s->cursor = off;
                                s->sel_anchor = off;
                            }
                            ctx->drag_widget_id = h->id;          // begin drag-selection
                        }

                        if (h->kind == WK_SLIDER && !h->disabled) {  // begin slider drag
                            ctx->drag_widget_id = h->id;
                            double range = h->num_max - h->num_min;
                            int thumb_w = 16;
                            int track_w = (int)h->w - thumb_w;
                            if (track_w < 1) track_w = 1;
                            int local = e->x - (int)h->x - thumb_w / 2;
                            if (local < 0) local = 0;
                            if (local > track_w) local = track_w;
                            double t = (double)local / (double)track_w;
                            double v = h->num_min + t * range;
                            h->num_value = v;                     // jump to pointer position

                            UiEvent cev; memset(&cev, 0, sizeof(cev));  // emit change on down
                            cev.kind = EV_CHANGE;
                            cev.widget_id = h->id;
                            strncpy(cev.widget_type, "slider", 15);
                            if (h->key_str) strncpy(cev.key_name, h->key_str, 63);
                            cev.num_value = v;
                            ui_event_push(ctx, &cev);
                        }
                        ctx->redraw_needed = true;
                    } else {
                        ctx->focused_id = 0;                      // clicked outside any widget
                    }
                }
            }
            break;
        case UW_EV_MOUSEUP:
            if (e->button == 1) {                                 // left button released
                ctx->input.lmb = false;

                if (ctx->dialog.active) {                         // modal dialog is on top
                    dialog_mouse_up(ctx, e->x, e->y);
                    break;
                }

                // finish a titlebar button gesture
                if (ctx->tb_active) {
                    int was = ctx->tb_active;
                    int hot = titlebar_button_at(ctx, e->x, e->y);
                    ctx->tb_active = 0;
                    if (hot == was) {
                        if (was == 1)      ui_window_minimize(ctx->window);
                        else if (was == 2) ui_window_toggle_maximize(ctx->window);
                        else if (was == 3) {
                            UiEvent cev; memset(&cev, 0, sizeof(cev));
                            cev.kind = EV_CLOSE;
                            ui_event_push(ctx, &cev);
                            ctx->quit_requested = true;
                        }
                    }
                    ctx->redraw_needed = true;
                    break;
                }

                ctx->drag_widget_id = 0;                          // end any drag
                if (ctx->root) {
                    Widget* h = hit_test(ctx, e->x, e->y);
                    for (int i = 0; i < ctx->state.count; i++) {  // release all pressed flags
                        if (ctx->state.entries[i].pressed) {
                            ctx->state.entries[i].pressed = false;
                            if (h && h->id == ctx->state.entries[i].id) {
                                fire_click(ctx, h);               // click only if released on same widget
                            }
                        }
                    }
                    for (int i = 0; i < ctx->pool_used; i++)      // clear pool press flags
                        ctx->pool[i].pressed = false;
                    ctx->redraw_needed = true;
                }
            }
            break;
        case UW_EV_SCROLL: {                                      // wheel scrolled
            if (ctx->root) {
                Widget* h = hit_test(ctx, e->x, e->y);
                Widget* sc = h;
                while (sc && sc->kind != WK_SCROLL && sc->kind != WK_HSCROLL)
                    sc = sc->parent;                              // find enclosing scroll container
                if (sc && sc->kind == WK_SCROLL) {                // vertical scroll container
                    WidgetState* s = ui_state_get(ctx, sc->id);
                    s->scroll_target += e->scroll_dy * 40.0;      // push the target, not scroll_y
                    double max = sc->content_h - sc->h;           // max scrollable offset
                    if (max < 0) max = 0;
                    if (s->scroll_target < 0)   s->scroll_target = 0;    // clamp target to top
                    if (s->scroll_target > max) s->scroll_target = max;  // clamp target to bottom
                    ctx->geometry_dirty = true;                   // children positions changed
                    ctx->redraw_needed = true;
                } else {                                          // no container: emit raw scroll event
                    UiEvent ev; memset(&ev, 0, sizeof(ev));
                    ev.kind = EV_SCROLL;
                    ev.scroll_dy = e->scroll_dy;
                    ev.mouse_x = e->x;
                    ev.mouse_y = e->y;
                    ui_event_push(ctx, &ev);
                }
            }
            break;
        }
        case UW_EV_KEYDOWN: {                                     // key pressed
            if (ctx->dialog.active) {                             // modal dialog captures the keyboard
                dialog_key_down(ctx, e);
                break;
            }
            if (ctx->focused_id && ctx->root) {                   // deliver to focused widget
                for (int i = 0; i < ctx->pool_used; i++) {
                    Widget* w = &ctx->pool[i];
                    if (w->id != ctx->focused_id) continue;       // skip non-focused

                    if (w->kind == WK_INPUT) {                    // input field editing
                        WidgetState* s = ui_state_get(ctx, w->id);
                        int len = (int)w->input_value.len;
                        // the cursor can be one frame ahead of the buffer
                        int cur = s->cursor < 0 ? len : s->cursor;
                        if (cur > len) cur = len;
                        if (s->sel_anchor < 0 || s->sel_anchor > len) s->sel_anchor = cur;

                        // ctrl shortcuts: select all, copy, cut, paste, undo, redo, and word-delete
                        if (e->mods & 2) {
                            int k = e->key;
                            if (k >= 'A' && k <= 'Z') k = k - 'A' + 'a';
                            if (k == 'a') {                   // select all
                                s->sel_anchor = 0;
                                s->cursor = len;
                                ctx->redraw_needed = true;
                                break;
                            }
                            if (k == 'c') {                   // copy selection
                                if (input_has_selection(s)) {
                                    int a, b;
                                    input_sel_range(s, &a, &b);
                                    ui_window_clipboard_set(ctx->window,
                                        w->input_value.ptr + a, b - a);
                                }
                                break;
                            }
                            if (k == 'x') {                   // cut selection
                                if (input_has_selection(s)) {
                                    int a, b;
                                    input_sel_range(s, &a, &b);
                                    ui_window_clipboard_set(ctx->window,
                                        w->input_value.ptr + a, b - a);
                                    input_push_undo(s, w->input_value.ptr, len, cur);
                                    input_delete_range(ctx, w, s, a, b);
                                }
                                break;
                            }
                            if (k == 'v') {                   // paste
                                char* text = NULL;
                                int tlen = 0;
                                if (ui_window_clipboard_get(ctx->window, &text, &tlen)
                                    && text && tlen > 0)
                                {
                                    input_push_undo(s, w->input_value.ptr, len, cur);

                                    if (input_has_selection(s)) {  // drop the selection first
                                        int a, b;
                                        input_sel_range(s, &a, &b);
                                        input_delete_range(ctx, w, s, a, b);
                                        cur = s->cursor;
                                        len = (int)w->input_value.len;
                                    }
                                    int need = len + tlen;
                                    char* nb = (char*)malloc((size_t)need + 1);
                                    memcpy(nb, w->input_value.ptr, cur);
                                    memcpy(nb + cur, text, tlen);
                                    memcpy(nb + cur + tlen,
                                           w->input_value.ptr + cur, len - cur);
                                    nb[need] = 0;
                                    input_set_buffer(ctx, w, nb, need);
                                    s->cursor = cur + tlen;
                                    s->sel_anchor = s->cursor;
                                    push_input_change(ctx, w, nb);
                                    ctx->redraw_needed = true;
                                }
                                free(text);
                                break;
                            }
                            if (k == 'z') {                   // undo (shift+z = redo)
                                if (e->mods & 1) input_redo(ctx, w, s);
                                else             input_undo(ctx, w, s);
                                break;
                            }
                            if (k == 'y') {                   // redo
                                input_redo(ctx, w, s);
                                break;
                            }
                            if (e->key == UI_KEY_BACKSPACE) { // ctrl+backspace: delete word back
                                if (input_has_selection(s)) {
                                    int a, b;
                                    input_sel_range(s, &a, &b);
                                    input_push_undo(s, w->input_value.ptr, len, cur);
                                    input_delete_range(ctx, w, s, a, b);
                                } else if (cur > 0) {
                                    int word_start = cur;
                                    while (word_start > 0) {
                                        int prev = ui_utf8_prev(w->input_value.ptr, word_start);
                                        unsigned char c = (unsigned char)w->input_value.ptr[prev];
                                        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
                                        word_start = prev;
                                    }
                                    if (word_start < cur) {
                                        input_push_undo(s, w->input_value.ptr, len, cur);
                                        input_delete_range(ctx, w, s, word_start, cur);
                                    }
                                }
                                break;
                            }
                            break;                            // other ctrl+key: consume and drop
                        }

                        // printable text: replaces any active selection first
                        if (e->utf8_len > 0) {
                            input_push_undo(s, w->input_value.ptr, len, cur);
                            if (input_has_selection(s)) {
                                int a, b;
                                input_sel_range(s, &a, &b);
                                input_delete_range(ctx, w, s, a, b);
                                cur = s->cursor;
                                len = (int)w->input_value.len;
                            }
                            int need = len + e->utf8_len;
                            char* nb = (char*)malloc((size_t)need + 1);
                            memcpy(nb, w->input_value.ptr, cur);
                            memcpy(nb + cur, e->utf8, e->utf8_len);
                            memcpy(nb + cur + e->utf8_len,
                                   w->input_value.ptr + cur, len - cur);
                            nb[need] = 0;
                            input_set_buffer(ctx, w, nb, need);
                            s->cursor = cur + e->utf8_len;
                            s->sel_anchor = s->cursor;
                            push_input_change(ctx, w, nb);
                            ctx->redraw_needed = true;
                            break;
                        }

                        if (e->key == UI_KEY_BACKSPACE) {     // backspace
                            if (input_has_selection(s)) {
                                int a, b;
                                input_sel_range(s, &a, &b);
                                input_push_undo(s, w->input_value.ptr, len, cur);
                                input_delete_range(ctx, w, s, a, b);
                            } else if (cur > 0) {
                                int prev = ui_utf8_prev(w->input_value.ptr, cur);
                                input_push_undo(s, w->input_value.ptr, len, cur);
                                input_delete_range(ctx, w, s, prev, cur);
                            }
                            break;
                        }
                        if (e->key == UI_KEY_DELETE) {        // delete (forward)
                            if (input_has_selection(s)) {
                                int a, b;
                                input_sel_range(s, &a, &b);
                                input_push_undo(s, w->input_value.ptr, len, cur);
                                input_delete_range(ctx, w, s, a, b);
                            } else if (cur < len) {
                                int next = ui_utf8_next(w->input_value.ptr, cur, len);
                                input_push_undo(s, w->input_value.ptr, len, cur);
                                input_delete_range(ctx, w, s, cur, next);
                            }
                            break;
                        }

                        // arrow keys move the caret; shift extends the selection
                        if (e->key == UI_KEY_LEFT) {          // left
                            int np = ui_utf8_prev(w->input_value.ptr, cur);
                            s->cursor = np;
                            if (!(e->mods & 1)) s->sel_anchor = np;   // clear on plain move
                            ctx->redraw_needed = true;
                            break;
                        }
                        if (e->key == UI_KEY_RIGHT) {         // right
                            int np = ui_utf8_next(w->input_value.ptr, cur, len);
                            s->cursor = np;
                            if (!(e->mods & 1)) s->sel_anchor = np;
                            ctx->redraw_needed = true;
                            break;
                        }
                        if (e->key == UI_KEY_HOME) {          // home
                            s->cursor = 0;
                            if (!(e->mods & 1)) s->sel_anchor = 0;
                            ctx->redraw_needed = true;
                            break;
                        }
                        if (e->key == UI_KEY_END) {           // end
                            s->cursor = len;
                            if (!(e->mods & 1)) s->sel_anchor = len;
                            ctx->redraw_needed = true;
                            break;
                        }
                    }

                    if (w->kind == WK_BUTTON &&
                        (e->key == UI_KEY_SPACE || e->key == UI_KEY_ENTER)) {   // space or enter activates
                        fire_click(ctx, w);
                        break;
                    }
                    if (w->kind == WK_CHECKBOX &&
                        (e->key == UI_KEY_SPACE || e->key == UI_KEY_ENTER)) {   // space or enter toggles
                        fire_click(ctx, w);
                        break;
                    }
                }
            }

            if (e->key == UI_KEY_TAB && ctx->root) {              // tab cycles focus
                Widget* focs[512];
                int n = 0;
                collect_focusable(ctx->root, focs, &n, 512);      // gather in draw order
                if (n > 0) {
                    int cur_idx = -1;
                    for (int i = 0; i < n; i++)
                        if (focs[i]->id == ctx->focused_id) { cur_idx = i; break; }
                    if (e->mods & 1) cur_idx = (cur_idx <= 0) ? n - 1 : cur_idx - 1;          // shift-tab goes back
                    else             cur_idx = (cur_idx < 0 || cur_idx == n - 1) ? 0 : cur_idx + 1;  // tab goes forward
                    ctx->focused_id = focs[cur_idx]->id;          // apply new focus
                    for (int i = 0; i < ctx->pool_used; i++)      // reflect on live widgets
                        ctx->pool[i].focused = (ctx->pool[i].id == ctx->focused_id);
                    ctx->redraw_needed = true;
                }
            }

            if (e->key == UI_KEY_ESCAPE) {                        // esc closes any open dropdown
                bool closed = false;
                for (int i = 0; i < ctx->pool_used; i++) {
                    Widget* w = &ctx->pool[i];
                    if (w->kind == WK_COMBOBOX && w->combo_open) {
                        w->combo_open = false;
                        WidgetState* s = ui_state_get(ctx, w->id);
                        s->combo_open = false;
                        closed = true;
                    }
                }
                if (closed) {
                    ctx->redraw_needed = true;
                    ctx->geometry_dirty = true;
                    break;
                }
            }

            UiEvent ev; memset(&ev, 0, sizeof(ev));               // always also emit a raw key event
            ev.kind = EV_KEY;
            ev.widget_id = ctx->focused_id;
            ev.key_code = e->key;
            ev.mods = e->mods;
            if (e->utf8_len > 0) {                                // copy utf8 payload if any
                ev.str_value = (char*)malloc(e->utf8_len + 1);
                memcpy(ev.str_value, e->utf8, e->utf8_len);
                ev.str_value[e->utf8_len] = 0;
            }
            ui_event_push(ctx, &ev);
            break;
        }
        default: break;                                           // ignore other events
    }
}

// true while any state entry still has a scroll offset to ease toward its target
static bool ui_animating(UiContext* ctx) {
    for (int i = 0; i < ctx->state.count; i++) {
        WidgetState* s = &ctx->state.entries[i];
        if (s->scroll_y != s->scroll_target) return true;  // still moving
    }
    return false;                                          // all settled
}

// advances time-based widget state
void ui_tick(UiContext* ctx, double dt) {
    if (dt <= 0.0) return;                               // no time passed
    if (dt > 0.25) dt = 0.25;                            // clamp after a long pause

    // ease every scroll offset toward its target in fixed 1/60 s substeps
    const double STEP = 1.0 / 60.0;
    const double RATE = 28.0;                            // ~36 ms time constant
    double remaining = dt;
    while (remaining > 1e-6) {
        double h = remaining > STEP ? STEP : remaining;
        remaining -= h;
        double k = 1.0 - exp(-h * RATE);                 // fraction of distance this substep
        for (int i = 0; i < ctx->state.count; i++) {
            WidgetState* s = &ctx->state.entries[i];
            if (s->scroll_y == s->scroll_target) continue;  // already settled
            double diff = s->scroll_target - s->scroll_y;
            if (fabs(diff) < 0.5) {
                s->scroll_y = s->scroll_target;          // snap: kill the asymptotic tail
            } else {
                s->scroll_y += diff * k;                 // ease toward the target
            }
        }
    }

    // publish animated offsets onto the live widgets and flag one relayout
    bool moved = false;
    for (int i = 0; i < ctx->state.count; i++) {
        WidgetState* s = &ctx->state.entries[i];
        for (int j = 0; j < ctx->pool_used; j++) {
            if (ctx->pool[j].id == s->id && ctx->pool[j].scroll_y != s->scroll_y) {
                ctx->pool[j].scroll_y = s->scroll_y;     // mirror for this frame's layout
                moved = true;
                break;
            }
        }
    }
    if (moved) {
        ctx->geometry_dirty = true;                      // scroll containers moved
        ctx->redraw_needed  = true;                      // schedule a repaint
    }

    // caret blink: only the focused widget has a phase
    if (ctx->focused_id) {
        WidgetState* s = ui_state_get(ctx, ctx->focused_id);
        double old_phase = s->blink_phase;               // remember for boundary detection
        s->blink_phase += dt;                            // advance the blink clock
        if (s->blink_phase >= 1.0) s->blink_phase -= 1.0;// wrap at one second
        if ((old_phase < 0.5) != (s->blink_phase < 0.5)) // crossed the visibility edge
            ctx->redraw_needed = true;                   // schedule a repaint
    }

    // dialog open/close animation + caret blink
    if (ctx->dialog.active) {
        UiDialog* d = &ctx->dialog;
        if (d->closing) {
            if (d->anim_t > 0.0) {
                d->anim_t -= dt / DLG_CLOSE_SEC;                 // fade out toward 0
                if (d->anim_t < 0.0) d->anim_t = 0.0;            // clamp: no negative phase
                ctx->redraw_needed = true;                       // schedule a repaint
                if (d->anim_t == 0.0) dialog_finish_close(ctx);  // fade done: emit + drop
            }
        } else if (d->anim_t < 1.0) {
            d->anim_t += dt / DLG_OPEN_SEC;                      // fade in toward 1
            if (d->anim_t > 1.0) d->anim_t = 1.0;                // clamp: fully opaque
            ctx->redraw_needed = true;                           // schedule a repaint
        }
        if (!d->closing && d->focused_field >= 0 && d->focused_field < d->field_count) {
            double old_phase = d->blink_phase;                   // remember for boundary detection
            d->blink_phase += dt;                                // advance the blink clock
            if (d->blink_phase >= 1.0) d->blink_phase -= 1.0;    // wrap at one second
            if ((old_phase < 0.5) != (d->blink_phase < 0.5))     // crossed the visibility edge
                ctx->redraw_needed = true;                       // schedule a repaint
        }
    }
}

// pumps the window event loop once, returning true if events were queued
static bool pump(UiContext* ctx) {
    if (!ctx->window) return false;                               // no window: nothing to pump

    // advance animated state first, before any early return
    static double s_last_tick = 0.0;                              // previous wall-clock reading
    double now = apex_now_seconds();
    double dt = now - s_last_tick;
    s_last_tick = now;
    if (dt < 0.0) dt = 0.0;                                       // clock went backwards
    if (dt > 0.25) dt = 0.25;                                     // clamp after a long pause
    ui_tick(ctx, dt);

    UiWinEvent we;
    while (ui_window_poll(ctx->window, &we)) {                    // drain pending events
        handle_win_event(ctx, &we);
        if (we.kind == UW_EV_CLOSE) break;                        // stop on close
    }
    if (ctx->queue.count > 0) return true;                        // events already queued
    if (!ui_window_is_open(ctx->window)) return false;            // window closed

    int timeout;
    if (ctx->redraw_needed)      timeout = 8;                     // ~120 fps: repaint pending, poll fast
    else if (ui_animating(ctx))  timeout = 8;                     // ~120 fps: scroll ease in flight
    else if (ctx->focused_id)    timeout = 62;                    // ~16 hz: only needed for caret blink
    else if (ctx->dialog.active) timeout = 62;                    // dialog animation / caret blink
    else                         timeout = 1000;                  // fully idle: sleep until real input
    if (ui_window_wait(ctx->window, timeout)) {                   // block briefly for input
        while (ui_window_poll(ctx->window, &we)) {                // drain newly arrived events
            handle_win_event(ctx, &we);
            if (we.kind == UW_EV_CLOSE) break;
        }
    }

    return ctx->queue.count > 0;                                  // true if anything was queued
}

// calls an apex function value synchronously by running it as a one-shot coroutine
static Value apex_call_function(VM* vm, Value func, int argc, Value* argv) {
    if (!IS_FUNCTION(func)) return MAKE_NONE();       // not callable
    int func_idx = AS_FUNCTION(func);                 // unwrap packed function index

    FutureObject* fut = (FutureObject*)calloc(1, sizeof(FutureObject));
    if (!fut) return MAKE_NONE();                     // oom: no result
    fut->header.ref_count = 1;                        // fresh object
    fut->header.type      = VAL_FUTURE;               // mark as future
    fut->result           = MAKE_NONE();
    fut->state            = 0;                        // pending
    fut->func_idx         = func_idx;
    fut->arg_count        = argc;
    fut->args             = argc > 0 ? (Value*)malloc(sizeof(Value) * argc) : NULL;
    fut->saved_dest_reg   = -1;
    fut->awaiting         = MAKE_NONE();
    fut->saved_iter_depth = -1;
    fut->saved_table_iter_depth = -1;
    fut->saved_call_depth = 0;
    for (int i = 0; i < argc; i++) {                  // capture args, keeping refs alive
        if ((argv[i] & QNAN) == QNAN) value_incref(argv[i]);
        fut->args[i] = argv[i];
    }

    future_start(vm, fut);                            // enqueue the coroutine
    Value target = MAKE_FUTURE(fut);
    Value result;
    vm_drive_until(vm, target, &result);              // run it to completion
    value_decref(target);                             // drop our local reference
    return result;                                    // caller owns one ref
}

// builtin: creates the native window if not already open
Value ui_impl_init(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->window) return MAKE_BOOL(true);                      // already initialised
    int w = 640, h = 480;                                         // default window size
    int min_w = UI_MIN_W, min_h = UI_MIN_H;                       // default minimum size
    char* title = NULL;
    bool chrome = true;                                           // custom titlebar on by default
    if (argc >= 1 && IS_TABLE(argv[0])) {                         // optional options table
        Table* t = AS_TABLE(argv[0]);
        w = (int)table_get_num(vm, t, "width", 640);              // width override
        h = (int)table_get_num(vm, t, "height", 480);             // height override
        title  = table_get_str_dup(vm, t, "title");               // title override
        chrome = table_get_bool(vm, t, "titlebar", true);         // default: on
        min_w = (int)table_get_num(vm, t, "min_width",  UI_MIN_W);  // minimum width
        min_h = (int)table_get_num(vm, t, "min_height", UI_MIN_H);  // minimum height
    }
    const char* use_title = title ? title : "Apex UI";            // fall back to default title
    ctx->custom_chrome = chrome;                                  // remember for the render pass
    ctx->tb_hot = ctx->tb_active = 0;                             // clear button state
    strncpy(ctx->title, use_title, sizeof(ctx->title) - 1);       // mirror the title
    ctx->title[sizeof(ctx->title) - 1] = 0;
    ctx->window = ui_window_create(w, h, use_title, !chrome);     // create the host window
    free(title);                                                  // release the temp title
    if (!ctx->window) return MAKE_BOOL(false);                    // creation failed
    ctx->win_w = w;                                               // cache window size
    ctx->win_h = h;
    ctx->pixels = ui_window_pixels(ctx->window);                  // cache pixel buffer

    ctx->redraw_needed = true;                                    // force first paint
    ctx->full_repaint  = true;                                    // first frame is undefined
    ui_window_set_min_size(ctx->window, min_w, min_h);            // enforce a floor on resize
    ui_window_set_paint_callback(ctx->window, ui_window_paint_cb, ctx);  // install paint hook

    return MAKE_BOOL(true);                                       // success
}

// builtin: records the current widget tree descriptor for the next repaint
Value ui_impl_render(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->has_tree && (ctx->last_tree & QNAN) == QNAN)
        value_decref(ctx->last_tree);                             // release previous tree reference
    if (argc >= 1 && (argv[0] & QNAN) == QNAN) {                  // new tree is heap-tagged
        value_incref(argv[0]);                                    // take a reference
        ctx->last_tree = argv[0];
        ctx->has_tree = true;
    } else {
        ctx->last_tree = MAKE_NONE();                             // no tree or non-heap value
        ctx->has_tree = false;
    }
    ctx->redraw_needed = true;                                    // schedule a repaint
    return MAKE_NONE();
}

// mixes a descriptor Value into the running hash
static uint64_t hash_descr_value(UiContext* ctx, uint64_t h, Value v);

// recursive hash of a descriptor tree
static uint64_t hash_descr_table(UiContext* ctx, uint64_t h, Table* t) {
    // hash array-style numeric keys 1..n first
    int j = 1;
    int n_arr = t->array_part ? t->array_count : 0;
    for (; j <= n_arr; j++) {
        Value v = t->array_part[j - 1];
        if (IS_NONE(v)) break;
        uint64_t idx = (uint64_t)j;
        h = hash_bytes64(h, &idx, sizeof(idx));
        h = hash_descr_value(ctx, h, v);
    }
    for (;; j++) {
        Value v;
        if (!table_get(t, MAKE_NUMBER((double)j), &v)) break;
        uint64_t idx = (uint64_t)j;
        h = hash_bytes64(h, &idx, sizeof(idx));
        h = hash_descr_value(ctx, h, v);
        value_decref(v);
    }

    for (int i = 0; i < UK_COUNT; i++) {
        Value v;
        if (!table_get(t, ctx->key_cache[i], &v)) continue;
        h = hash_bytes64(h, &i, sizeof(i));
        if (i == UK_CHILDREN && IS_TABLE(v)) {
            Table* ct = AS_TABLE(v);
            int count = 0;

            // fast path: direct array walk, same trick as the other two callers.
            int j = 1;
            int n_arr = ct->array_part ? ct->array_count : 0;
            for (; j <= n_arr; j++) {
                Value cv = ct->array_part[j - 1];
                if (IS_NONE(cv)) break;
                h = hash_descr_value(ctx, h, cv);
                count++;
            }
            for (;; j++) {
                Value cv;
                if (!table_get(ct, MAKE_NUMBER((double)j), &cv)) break;
                h = hash_descr_value(ctx, h, cv);
                value_decref(cv);
                count++;
            }

            if (count == 0) {
                // no iterable children
                h = hash_descr_value(ctx, h, v);
                count = 1;
            }
            h = hash_bytes64(h, &count, sizeof(count));
        } else {
            h = hash_descr_value(ctx, h, v);
        }
        value_decref(v);
    }
    return h;
}

static uint64_t hash_descr_value(UiContext* ctx, uint64_t h, Value v) {
    if (IS_TABLE(v))       return hash_descr_table(ctx, h, AS_TABLE(v));
    if (IS_STRING(v)) {
        StringObject* s = AS_STRING(v);
        return hash_str_sampled(h, s->chars, s->length);
    }
    if (IS_NUMBER(v)) { double d = AS_NUMBER(v); return hash_bytes64(h, &d, sizeof(d)); }
    if (IS_BOOL(v))   { bool b = AS_BOOL(v);    return hash_bytes64(h, &b, sizeof(b)); }
    return hash_bytes64(h, &v, sizeof(v));                  // none, function, etc.
}

// public wrapper: hash a whole descriptor Value
static uint64_t hash_descr_recursive(UiContext* ctx, Value descr) {
    return hash_descr_value(ctx, 0xCBF29CE484222325ULL, descr);
}

// rebuilds the widget tree from the last descriptor and paints it
static void rebuild_and_draw(UiContext* ctx) {
    // decide whether we can reuse the tree from the previous frame
    bool can_reuse = false;
    uint64_t new_hash = 0;

    if (ctx->has_tree && ctx->descr_hash_valid && ctx->root)
    {
        if (ctx->last_tree == ctx->hashed_tree) {
            new_hash = ctx->last_descr_hash;                      // same Value, cached hash
        } else {
            new_hash = hash_descr_recursive(ctx, ctx->last_tree);
        }
        if (new_hash == ctx->last_descr_hash) can_reuse = true;
    }

    if (!can_reuse) {
        ui_widgets_reset(ctx);                                    // drop the previous tree
        if (ctx->has_tree) {
            if (IS_TABLE(ctx->last_tree)) {
                Table* t = AS_TABLE(ctx->last_tree);
                Value first;
                if (table_get(t, MAKE_NUMBER(1.0), &first)) {     // array-style: multiple root children
                    value_decref(first);
                    Widget* root = ui_widget_alloc(ctx);          // synthesize a vbox root
                    root->kind = WK_VBOX;
                    root->id = ui_hash_id(0, "__root__", 0);
                    root->padding = UI_PAD_SCREEN;
                    root->gap = UI_GAP;
                    for (int i = 1; ; i++) {                      // walk 1-based array children
                        Value vv;
                        if (!table_get(t, MAKE_NUMBER((double)i), &vv)) break;
                        Widget* ch = ui_build_tree(ctx, vv, root, i - 1);
                        if (ch) widget_add_child(ctx, root, ch);
                        value_decref(vv);
                    }
                    ctx->root = root;
                } else {                                          // single descriptor root
                    ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);
                }
            } else {
                ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);  // non-table descriptor
            }
        }
        if (!ctx->root) {                                         // no tree: synthesize an empty root
            Widget* root = ui_widget_alloc(ctx);
            root->kind = WK_VBOX;
            root->id = ui_hash_id(0, "__root__", 0);
            root->padding = UI_PAD_SCREEN;
            ctx->root = root;
        }
        // remember what we just built so the next frame can compare
        ctx->last_descr_hash = new_hash;
        ctx->hashed_tree     = ctx->last_tree;
        ctx->descr_hash_valid = true;
    }

    int top = ctx->custom_chrome ? UI_TITLEBAR_H : 0;             // reserve room for the titlebar
    ctx->root->x = 0;
    ctx->root->y = (float)top;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)(ctx->win_h - top);
    if (ctx->geometry_dirty) {                                    // skip when nothing moved
        ui_layout(ctx);                                           // compute widget geometry
        ctx->geometry_dirty = false;
    }

    // snapshot the freshly laid-out tree, diff against the previous frame to compute damage
    ui_snapshot_capture(ctx, ctx->root);
    ui_damage_begin(ctx);
    if (ctx->full_repaint) {
        // the host told us our pixels are undefined (first frame, expose, resize):
        // repaint the whole window, ignore the damage diff
        ui_damage_add(ctx, 0, 0, ctx->win_w, ctx->win_h);
        ctx->full_repaint = false;
    } else {
        ui_damage_diff(ctx);
        if (ctx->custom_chrome)                                   // always repaint the titlebar
            ui_damage_add(ctx, 0, 0, ctx->win_w, UI_TITLEBAR_H);
    }
    ui_damage_finalize(ctx);

    if (ctx->damage_count > 0) {
        long total = 0, area = (long)ctx->win_w * (long)ctx->win_h;
        for (int i = 0; i < ctx->damage_count; i++)
            total += (long)ctx->damage[i].w * (long)ctx->damage[i].h;

        ui_render_frame_damage(ctx, ctx->damage, ctx->damage_count);
        if (ctx->dialog.active || total * 2 >= area) {
            // a modal dialog dims the whole window and paints on top of everything
            ui_window_present(ctx->window);                       // whole-window blit
        } else {
            ui_window_present_rects(ctx->window,                  // small damage: per-rect blits
                                    (const int*)ctx->damage,
                                    ctx->damage_count);
        }
    }
    ui_damage_swap(ctx);
    ctx->redraw_needed = false;                                   // repaint satisfied

    // state gc is only needed when the tree was actually rebuilt
    if (!can_reuse) {
        state_gc_mark(ctx, ctx->root);                            // mark reachable state
        state_gc_sweep(ctx);                                      // drop the rest

        // clear interaction ids that no longer match a live widget after the rebuild
        if (ctx->focused_id) {
            bool found = false;
            for (int i = 0; i < ctx->pool_used && !found; i++)
                if (ctx->pool[i].id == ctx->focused_id) found = true;
            if (!found) ctx->focused_id = 0;
        }
        if (ctx->hovered_id) {
            bool found = false;
            for (int i = 0; i < ctx->pool_used && !found; i++)
                if (ctx->pool[i].id == ctx->hovered_id) found = true;
            if (!found) ctx->hovered_id = 0;
        }
        if (ctx->drag_widget_id) {
            bool found = false;
            for (int i = 0; i < ctx->pool_used && !found; i++)
                if (ctx->pool[i].id == ctx->drag_widget_id) found = true;
            if (!found) ctx->drag_widget_id = 0;
        }
    }
}

// repaints the existing widget tree without rebuilding it
static void redraw_only(UiContext* ctx) {
    if (!ctx->root) return;                                       // nothing to draw

    int top = ctx->custom_chrome ? UI_TITLEBAR_H : 0;             // reserve room for the titlebar
    ctx->root->x = 0;
    ctx->root->y = (float)top;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)(ctx->win_h - top);

    if (ctx->geometry_dirty) {                                    // skip when nothing moved
        ui_layout(ctx);                                           // recompute geometry
        ctx->geometry_dirty = false;
    }
    ui_render_frame(ctx);                                         // redraw into the pixel buffer
    ui_window_present(ctx->window);                               // blit to the screen
    ctx->redraw_needed = false;                                   // repaint satisfied
    ctx->full_repaint  = false;                                   // full repaint satisfied
}

// host window paint hook: refreshes buffers and repaints the existing tree
static void ui_window_paint_cb(void* user, int w, int h) {
    UiContext* ctx = (UiContext*)user;
    if (!ctx || !ctx->window) return;                             // nothing to paint

    ctx->pixels = ui_window_pixels(ctx->window);                  // refresh pixel buffer
    if (w != ctx->win_w || h != ctx->win_h) {                     // client rect changed under us
        ctx->win_w = w;
        ctx->win_h = h;
        ctx->geometry_dirty = true;
    }

    redraw_only(ctx);                                             // repaint the current tree
}

// builtin: blocks until an event is available, returns it as a table
Value ui_impl_next_event(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (!ctx->window) return MAKE_NONE();                         // no window: nothing to wait on

    int safety = 0;                                               // guard against a runaway loop
    while (ctx->queue.count == 0) {                               // wait for an event
        if (ctx->quit_requested || !ui_window_is_open(ctx->window)) {
            UiEvent ev; memset(&ev, 0, sizeof(ev));               // synthesize a close event
            ev.kind = EV_CLOSE;
            ui_event_push(ctx, &ev);
            break;
        }
        if (ctx->redraw_needed) rebuild_and_draw(ctx);            // repaint if requested
        pump(ctx);                                                // drain native events
        if (ctx->queue.count > 0) break;                          // event arrived
        if (++safety > 5000) break;                               // give up after too many empty spins
    }
    if (ctx->queue.count == 0) return MAKE_NONE();                // still nothing: return none

    UiEvent ev = ctx->queue.events[0];                            // peek the front event
    memmove(&ctx->queue.events[0], &ctx->queue.events[1],
            sizeof(UiEvent) * (ctx->queue.count - 1));            // shift the queue down
    ctx->queue.count--;                                           // one fewer queued event

    Value t = ui_event_to_table(vm, &ev);                         // convert to an apex table
    free(ev.str_value);                                           // release the event's owned string
    return t;                                                     // return the event table
}

// drive the message loop with batched repaints
Value ui_impl_run(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                   // ensure the context exists
    if (!ctx->window) return MAKE_NONE();             // no window: nothing to run
    if (argc < 1 || !IS_FUNCTION(argv[0])) return MAKE_NONE();  // callback required

    Value callback = argv[0];
    value_incref(callback);                           // keep alive across the loop

    if (ctx->redraw_needed) rebuild_and_draw(ctx);    // honour a pre-run ui.render

    while (!ctx->quit_requested && ui_window_is_open(ctx->window)) {
        pump(ctx);                                    // block, drain native events

        bool rebuilt = false;                         // did any callback return a tree?

        while (ctx->queue.count > 0) {                // process the whole batch
            UiEvent ev = ctx->queue.events[0];        // peek front
            memmove(&ctx->queue.events[0], &ctx->queue.events[1],
                    sizeof(UiEvent) * (ctx->queue.count - 1));  // shift down
            ctx->queue.count--;                       // one fewer queued event

            Value ev_tbl = ui_event_to_table(vm, &ev);  // convert to Apex table
            free(ev.str_value);                       // release the event's owned string

            Value result = apex_call_function(vm, callback, 1, &ev_tbl);
            value_decref(ev_tbl);                     // release our local reference

            if (IS_TABLE(result)) {                   // callback staged a new tree
                if (ctx->has_tree && (ctx->last_tree & QNAN) == QNAN)
                    value_decref(ctx->last_tree);     // release previous tree
                ctx->last_tree = result;              // take ownership of the new one
                ctx->has_tree  = true;
                ctx->redraw_needed = true;
                rebuilt = true;
            } else {
                value_decref(result);                 // none or non-table: drop it
            }
        }

        if (rebuilt || ctx->redraw_needed)            // one rebuild per batch
            rebuild_and_draw(ctx);
    }

    value_decref(callback);                           // release the callback
    return MAKE_NONE();
}

// builtin: destroys the native window and marks the context as shut down
Value ui_impl_shutdown(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->window) {                                            // tear down the window if present
        ui_window_destroy(ctx->window);
        ctx->window = NULL;
    }
    ctx->quit_requested = true;                                   // mark the context as quit
    return MAKE_NONE();
}

// builtin: sets the native window title
Value ui_impl_set_title(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (!ctx->window) return MAKE_NONE();                         // no window: no-op
    if (argc >= 1 && IS_STRING(argv[0])) {                        // only string titles are applied
        const char* s = AS_STRING(argv[0])->chars;
        ui_window_set_title(ctx->window, s);
        strncpy(ctx->title, s, sizeof(ctx->title) - 1);           // mirror for the custom titlebar
        ctx->title[sizeof(ctx->title) - 1] = 0;
        ctx->redraw_needed = true;                                // schedule a repaint
    }
    return MAKE_NONE();
}

// builtin: returns the current window size as a {w, h} table
Value ui_impl_window_size(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    Table* t = table_create(4);                                   // small result table
    Value k, v;
    k = intern(vm, "w"); v = MAKE_NUMBER(ctx->win_w);             // width field
    table_set(t, k, v); value_decref(k);
    k = intern(vm, "h"); v = MAKE_NUMBER(ctx->win_h);             // height field
    table_set(t, k, v); value_decref(k);
    return MAKE_TABLE(t);                                         // hand the table to the caller
}
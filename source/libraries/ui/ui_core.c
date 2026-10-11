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
    for (int i = 0; i < ctx->pool_used; i++) {            // walk each live widget
        free(ctx->pool[i].children);                      // free child pointer array
        free(ctx->pool[i].input_value.ptr);               // free input buffer (heap, mutable)
        free(ctx->pool[i].line_starts);                   // free cached wrapped-line offsets
        free(ctx->pool[i].line_ends);                     // parallel end-offset array
    }
    ctx->pool_used = 0;                                   // reset pool cursor
    ctx->root = NULL;                                     // drop tree root
    ctx->arena_used = 0;                                  // bulk-free text/group/key_str

    // the text-measurement cache is keyed by hash(content, size), not by pointer into the arena
    ctx->text_cache_frame++;
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

// appends a child to a widget, growing the child array as needed
static void widget_add_child(Widget* p, Widget* c) {
    if (p->child_count >= p->child_cap) {                 // child array is full
        p->child_cap = p->child_cap ? p->child_cap * 2 : 4;  // start at 4, then double
        p->children = (Widget**)realloc(p->children, sizeof(Widget*) * p->child_cap);
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
uint64_t ui_visual_hash(const Widget* w) {
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

    // str fields carry a fresh heap copy each rebuild
    h = hash_str_sampled(h, w->text.ptr,        w->text.len);          // label/content
    h = hash_str_sampled(h, w->input_value.ptr, w->input_value.len);   // input text
    h = hash_str_sampled(h, w->group.ptr,       w->group.len);         // radio group name

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
    e->h    = (int)floorf(w->h + 0.5f) + 2*SNAP_MARGIN;                // rounded screen height, padded
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

// collapse overlapping damage rects; n^2 is fine because n stays tiny
void ui_damage_finalize(UiContext* ctx) {
    bool merged = true;                                                // keep merging until fixpoint
    while (merged) {                                                   // repeat while any merge happened
        merged = false;                                                // reset for this sweep
        for (int i = 0; i < ctx->damage_count; i++) {                  // scan each rect
            for (int j = i + 1; j < ctx->damage_count; ) {             // against later rects
                if (rects_overlap(&ctx->damage[i], &ctx->damage[j])) { // overlapping pair
                    rect_union(&ctx->damage[i], &ctx->damage[i], &ctx->damage[j]);  // fold into i
                    ctx->damage[j] = ctx->damage[--ctx->damage_count];  // swap-remove j
                    merged = true;                                     // remember progress
                } else {
                    j++;                                               // no overlap: advance j
                }
            }
        }
    }
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
    return MAKE_TABLE(t);                                 // hand the event table to the caller
}

// reads a string field from a table, returns a heap copy or null on miss
static char* table_get_str_dup(VM* vm, Table* t, const char* key) {
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

// duplicates n bytes of a c string into a heap-owned Str
static Str str_dup(const char* s, int n) {
    Str r;
    r.ptr = (char*)malloc(n + 1);                        // allocate room for payload + null
    memcpy(r.ptr, s, n);                                 // copy bytes
    r.ptr[n] = 0;                                        // null terminate
    r.len = n;                                           // record length
    return r;                                            // return by value
}

// copies n bytes of a c string into the context's per-frame arena
static Str arena_dup(UiContext* ctx, const char* s, int n) {
    if (ctx->arena_used + n + 1 > ctx->arena_cap) {      // arena full: grow
        int cap = ctx->arena_cap ? ctx->arena_cap : 64 * 1024;
        while (ctx->arena_used + n + 1 > cap) cap *= 2;
        ctx->arena = (char*)realloc(ctx->arena, (size_t)cap);
        ctx->arena_cap = cap;
    }
    Str r;
    r.ptr = ctx->arena + ctx->arena_used;                // bump the cursor
    memcpy(r.ptr, s, n);                                 // copy the payload
    r.ptr[n] = 0;                                        // null terminate
    r.len = n;                                           // record the length
    ctx->arena_used += n + 1;                            // advance the cursor
    return r;                                            // return by value
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

    StrView ival;
    bool has_ival = table_get_str_view(t, ctx->key_cache[UK_VALUE_TEXT], &ival);
    if (!has_ival && w->kind == WK_INPUT)
        has_ival = table_get_str_view(t, ctx->key_cache[UK_CONTENT], &ival);
    if (has_ival) {
        w->input_value = str_dup(ival.ptr, ival.len);       // copy into owned buffer
    } else if (w->kind == WK_INPUT) {
        w->input_value = str_dup("", 0);                    // input widgets always own a buffer
    }

    WidgetState* st = ui_state_get(ctx, w->id);             // pull persistent state
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
            Value probe;
            if (table_get(ct, MAKE_NUMBER(1.0), &probe)) {  // array-style children list
                value_decref(probe);
                for (int i = 1; ; i++) {                    // walk 1-based array part
                    Value vv;
                    if (!table_get(ct, MAKE_NUMBER((double)i), &vv)) break;
                    Widget* ch = ui_build_tree(ctx, vv, w, i - 1);
                    if (ch) widget_add_child(w, ch);
                    value_decref(vv);
                }
            }
        } else if (!IS_NONE(cv)) {                          // single child descriptor
            Widget* ch = ui_build_tree(ctx, cv, w, 0);
            if (ch) widget_add_child(w, ch);
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
        "value", "min", "max", "value_text", "children"
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
    }

    free(ctx->pool);                                              // free the widget pool array
    free(ctx->arena);                                             // free the per-frame string arena
    free(ctx->state.entries);                                     // free the persistent state table
    free(ctx->text_cache);                                        // free the text measurement cache
    free(ctx->state.index);                                       // free the state lookup index
    for (int i = 0; i < ctx->queue.count; i++) free(ctx->queue.events[i].str_value);  // release queued event strings
    free(ctx->queue.events);                                      // free the event queue array
    if (ctx->font)   ui_font_free(ctx->font);                     // release the font
    if (ctx->window) ui_window_destroy(ctx->window);              // release the host window
    ctx->last_tree = MAKE_NONE();                                 // clear the cached tree
    ctx->has_tree = false;
    ui_damage_free(ctx);                                          // free ui damage
    free(ctx);                                                    // free the context itself
    if (saved_vm) saved_vm->ui = NULL;                            // drop the vm's back-pointer
}

// finds the deepest visible widget containing (x, y), accounting for scroll offset
static Widget* hit_test(Widget* w, int x, int y) {
    if (!w || !w->visible) return NULL;                           // invisible widgets are never hit
    if (x < w->x || x >= w->x + w->w || y < w->y || y >= w->y + w->h) return NULL;  // outside bounds

    if (w->kind == WK_SCROLL) {                                   // scroll containers clip and offset
        for (int i = w->child_count - 1; i >= 0; i--) {           // topmost child first
            Widget* r = hit_test(w->children[i], x, y);
            if (r) return r;
        }
        return w;                                                 // fall back to the container itself
    }
    for (int i = w->child_count - 1; i >= 0; i--) {               // topmost child first
        Widget* r = hit_test(w->children[i], x, y);
        if (r) return r;
    }
    return w;                                                     // no child hit: return this widget
}

// returns true if the widget kind participates in keyboard focus
static bool widget_focusable(WidgetKind k) {
    return k == WK_BUTTON || k == WK_CHECKBOX || k == WK_RADIO ||
           k == WK_SLIDER || k == WK_INPUT;
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
    float full = ui_text_measure_cached(ctx, s, n, UI_FS_BODY);
    if (local_x >= full) return n;                        // past the end: caret at end

    int best = 0;
    float prev = 0;
    int i = 0;
    while (i < n) {
        int next = ui_utf8_next(s, i, n);
        float w2 = ui_text_measure_cached(ctx, s, next, UI_FS_BODY);
        float mid = (prev + w2) * 0.5f;                   // midpoint of this codepoint
        if (local_x < mid) return i;                      // clicked left of the midpoint
        i = next;
        prev = w2;
        best = i;
    }
    return best;
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
static void input_set_buffer(Widget* w, char* buf, int len) {
    free(w->input_value.ptr);
    w->input_value.ptr = buf;
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
    free(w->input_value.ptr);
    w->input_value.ptr = e.text;
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
    free(w->input_value.ptr);
    w->input_value.ptr = e.text;
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
    input_set_buffer(w, nb, new_len);
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
            break;
        }
        case UW_EV_RESIZE: {                                      // window resized
            ctx->pixels = ui_window_pixels(ctx->window);          // refresh pixel buffer
            ctx->win_w = e->w;                                    // new width from the event
            ctx->win_h = e->h;                                    // new height from the event
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

            if (ctx->root) {                                      // update hover state
                Widget* h = hit_test(ctx->root, e->x, e->y);
                uint64_t new_hover = h ? h->id : 0;
                for (int i = 0; i < ctx->pool_used; i++) {        // update live widget flags
                    if (ctx->pool[i].id == new_hover && !ctx->pool[i].hovered) {
                        ctx->pool[i].hovered = true;
                        ctx->redraw_needed = true;
                    } else if (ctx->pool[i].id != new_hover && ctx->pool[i].hovered) {
                        ctx->pool[i].hovered = false;
                        ctx->redraw_needed = true;
                    }
                }
                for (int i = 0; i < ctx->state.count; i++)        // clear persistent hover flags
                    ctx->state.entries[i].hovered = false;
                if (h) {                                          // set persistent hover on new target
                    WidgetState* s = ui_state_get(ctx, h->id);
                    s->hovered = true;
                }
            }
            break;
        }
        case UW_EV_MOUSEDOWN:
            if (e->button == 1) {                                 // left button only
                ctx->input.x = e->x;
                ctx->input.y = e->y;

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

                int tbtn = titlebar_button_at(ctx, e->x, e->y);
                if (tbtn) {
                    ctx->tb_active = tbtn;
                    ctx->redraw_needed = true;
                    break;
                }

                if (ctx->root) {
                    Widget* h = hit_test(ctx->root, e->x, e->y);
                    if (h) {
                        WidgetState* s = ui_state_get(ctx, h->id);
                        s->pressed = true;                        // mark pressed
                        // focus follows the click
                        if (widget_focusable(h->kind)) ctx->focused_id = h->id;
                        else                           ctx->focused_id = 0;

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
                    Widget* h = hit_test(ctx->root, e->x, e->y);
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
                Widget* h = hit_test(ctx->root, e->x, e->y);
                Widget* sc = h;
                while (sc && sc->kind != WK_SCROLL && sc->kind != WK_HSCROLL)
                    sc = sc->parent;                              // find enclosing scroll container
                if (sc && sc->kind == WK_SCROLL) {                // vertical scroll container
                    WidgetState* s = ui_state_get(ctx, sc->id);
                    s->scroll_y += e->scroll_dy * 40.0;           // apply wheel delta
                    if (s->scroll_y < 0) s->scroll_y = 0;         // clamp to top
                    double max = sc->content_h - sc->h;           // max scrollable offset
                    if (max < 0) max = 0;
                    if (s->scroll_y > max) s->scroll_y = max;     // clamp to bottom
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
                                    input_set_buffer(w, nb, need);
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
                            input_set_buffer(w, nb, need);
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

// advances time-based widget state
//
// the only time-based state right now is the caret blink of the focused
// widget. dt is the wall-clock delta since the previous call; a repaint is
// requested only when the caret crosses the visible/hidden boundary so
// idle frames stay cheap.
void ui_tick(UiContext* ctx, double dt) {
    if (!ctx->focused_id) return;                        // nothing focused: nothing to animate
    if (dt <= 0.0) return;                               // no time passed

    WidgetState* s = ui_state_get(ctx, ctx->focused_id);
    double old_phase = s->blink_phase;                   // remember for boundary detection
    s->blink_phase += dt;                                // advance the blink clock
    if (s->blink_phase >= 1.0) s->blink_phase -= 1.0;    // wrap at one second

    if ((old_phase < 0.5) != (s->blink_phase < 0.5))     // crossed the visibility edge
        ctx->redraw_needed = true;                       // schedule a repaint
}

// pumps the window event loop once, returning true if events were queued
static bool pump(UiContext* ctx) {
    if (!ctx->window) return false;                               // no window: nothing to pump
    UiWinEvent we;
    while (ui_window_poll(ctx->window, &we)) {                    // drain pending events
        handle_win_event(ctx, &we);
        if (we.kind == UW_EV_CLOSE) break;                        // stop on close
    }
    if (ctx->queue.count > 0) return true;                        // events already queued
    if (!ui_window_is_open(ctx->window)) return false;            // window closed

    int timeout;
    if (ctx->redraw_needed)      timeout = 8;                     // ~120 fps: repaint pending, poll fast
    else if (ctx->focused_id)    timeout = 62;                    // ~16 hz: only needed for caret blink
    else                         timeout = 1000;                  // fully idle: sleep until real input
    if (ui_window_wait(ctx->window, timeout)) {                   // block briefly for input
        while (ui_window_poll(ctx->window, &we)) {                // drain newly arrived events
            handle_win_event(ctx, &we);
            if (we.kind == UW_EV_CLOSE) break;
        }
    }

    static double s_last_tick = 0.0;                              // previous wall-clock reading
    double now = apex_now_seconds();
    double dt = now - s_last_tick;
    s_last_tick = now;
    if (dt < 0.0) dt = 0.0;                                       // clock went backwards
    if (dt > 0.25) dt = 0.25;                                     // clamp after a long pause
    ui_tick(ctx, dt);

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

// rebuilds the widget tree from the last descriptor and paints it
static void rebuild_and_draw(UiContext* ctx) {
    ui_widgets_reset(ctx);                                        // drop the previous tree
    if (ctx->has_tree) {
        if (IS_TABLE(ctx->last_tree)) {
            Table* t = AS_TABLE(ctx->last_tree);
            Value first;
            if (table_get(t, MAKE_NUMBER(1.0), &first)) {         // array-style: multiple root children
                value_decref(first);
                Widget* root = ui_widget_alloc(ctx);              // synthesize a vbox root
                root->kind = WK_VBOX;
                root->id = ui_hash_id(0, "__root__", 0);
                root->padding = UI_PAD_SCREEN;
                root->gap = UI_GAP;
                for (int i = 1; ; i++) {                          // walk 1-based array children
                    Value vv;
                    if (!table_get(t, MAKE_NUMBER((double)i), &vv)) break;
                    Widget* ch = ui_build_tree(ctx, vv, root, i - 1);
                    if (ch) widget_add_child(root, ch);
                    value_decref(vv);
                }
                ctx->root = root;
            } else {                                              // single descriptor root
                ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);
            }
        } else {
            ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);  // non-table descriptor
        }
    }
    if (!ctx->root) {                                             // no tree: synthesize an empty root
        Widget* root = ui_widget_alloc(ctx);
        root->kind = WK_VBOX;
        root->id = ui_hash_id(0, "__root__", 0);
        root->padding = UI_PAD_SCREEN;
        ctx->root = root;
    }
    int top = ctx->custom_chrome ? UI_TITLEBAR_H : 0;             // reserve room for the titlebar
    ctx->root->x = 0;
    ctx->root->y = (float)top;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)(ctx->win_h - top);
    ui_layout(ctx);                                               // compute widget geometry

    // snapshot the freshly laid-out tree, diff against the previous frame to compute damage
    ui_snapshot_capture(ctx, ctx->root);
    ui_damage_begin(ctx);
    ui_damage_diff(ctx);
    if (ctx->custom_chrome)                                       // always repaint the titlebar
        ui_damage_add(ctx, 0, 0, ctx->win_w, UI_TITLEBAR_H);
    ui_damage_finalize(ctx);

    if (ctx->damage_count > 0) {
        ui_render_frame_damage(ctx, ctx->damage, ctx->damage_count);
    }
    ui_window_present(ctx->window);
    ui_damage_swap(ctx);
    ctx->redraw_needed = false;                                   // repaint satisfied

    state_gc_mark(ctx, ctx->root);                                // mark reachable state
    state_gc_sweep(ctx);                                          // drop the rest
}

// repaints the existing widget tree without rebuilding it
static void redraw_only(UiContext* ctx) {
    if (!ctx->root) return;                                       // nothing to draw

    int top = ctx->custom_chrome ? UI_TITLEBAR_H : 0;             // reserve room for the titlebar
    ctx->root->x = 0;
    ctx->root->y = (float)top;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)(ctx->win_h - top);

    ui_layout(ctx);                                               // recompute geometry
    ui_render_frame(ctx);                                         // redraw into the pixel buffer
    ui_window_present(ctx->window);                               // blit to the screen
    ctx->redraw_needed = false;                                   // repaint satisfied
}

// host window paint hook: refreshes buffers and repaints the existing tree
static void ui_window_paint_cb(void* user, int w, int h) {
    (void)w; (void)h;                                             // sizes read from the window itself
    UiContext* ctx = (UiContext*)user;
    if (!ctx || !ctx->window) return;                             // nothing to paint

    ctx->pixels = ui_window_pixels(ctx->window);                  // refresh pixel buffer
    ctx->win_w = ui_window_width(ctx->window);                    // refresh window size
    ctx->win_h = ui_window_height(ctx->window);

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
// source/libraries/ui/interaction/ui_damage.c
// Snapshot capture and damage diffing for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_interaction.h"
#include "ui_hash.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    // input, number_input and text_area widgets keep their caret and selection in WidgetState
    for (int i = 0; i < ctx->pool_used; i++) {
        Widget* w = &ctx->pool[i];
        if (w->kind != WK_INPUT && w->kind != WK_NUMBER_INPUT && w->kind != WK_TEXT_AREA)
            continue;
        WidgetState* s = ui_state_get(ctx, w->id);
        w->cursor     = s->cursor < 0 ? (int)w->input_value.len : s->cursor;
        w->sel_anchor = s->sel_anchor;
        // caret blink: mirror the phase into the widget
        w->blink_on   = (w->focused && s->blink_phase < 0.5);
    }

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
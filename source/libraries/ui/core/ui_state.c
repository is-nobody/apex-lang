// source/libraries/ui/core/ui_state.c
// Per-widget persistent state and animation clock for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_core.h"
#include "ui_hash.h"
#include "ui_widgets.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

// marks reachable state from the current tree root
void state_gc_mark_root(UiContext* ctx, Widget* root) {
    state_gc_mark(ctx, root);
}

// drops state entries no widget referenced this frame
void state_gc_sweep(UiContext* ctx) {
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

// true while any state entry still has a scroll offset to ease toward its target
bool ui_animating(UiContext* ctx) {
    for (int i = 0; i < ctx->state.count; i++) {
        WidgetState* s = &ctx->state.entries[i];
        if (s->scroll_y != s->scroll_target) return true;  // still moving
        if (s->switch_init && s->switch_knob > 0.0f && s->switch_knob < 1.0f)
            return true;                                   // knob is mid-flight
    }
    for (int i = 0; i < ctx->notif_count; i++) {
        UiNotification* n = ctx->notifs[i];
        if (n->closing && n->anim_t > 0.0) return true;    // sliding out
        if (!n->closing && n->anim_t < 1.0) return true;   // sliding in
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

    // switch knobs ease toward the current checked state
    {
        float k = (float)(dt / UI_SWITCH_ANIM_SEC);      // fraction of the track per tick
        if (k > 1.0f) k = 1.0f;
        bool switch_moved = false;
        for (int i = 0; i < ctx->pool_used; i++) {
            Widget* w = &ctx->pool[i];
            if (w->kind != WK_SWITCH) continue;
            WidgetState* s = ui_state_get(ctx, w->id);
            float target = w->checked ? 1.0f : 0.0f;
            if (s->switch_knob != target) {              // still easing
                float diff = target - s->switch_knob;
                s->switch_knob += diff * k;              // exponential ease toward the target
                if (fabsf(s->switch_knob - target) < 0.005f)
                    s->switch_knob = target;             // snap: kill the asymptotic tail
                switch_moved = true;
            }
            w->switch_knob = s->switch_knob;             // publish for hash, damage, render
        }
        if (switch_moved) ctx->redraw_needed = true;
    }

    // notification fade and auto-close timers
    ui_notification_tick(ctx, dt);

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
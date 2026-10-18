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
    s->hover_row = -1;                                          // wk_tree: no row hovered
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
            free(s->tab_anim_x);                          // release the tab animation
            free(s->tab_anim_w);
            free(s->line_starts);                         // release the line-start cache
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
        if (s->switch_init && s->switch_anim_t < UI_SWITCH_ANIM_SEC)
            return true;                                   // knob is mid-flight
        if (s->tab_anim_init) return true;                 // tab strip easing
        if (!ctx->tooltip_shown_id &&
            s->id == ctx->hovered_id && s->tooltip_hover_start > 0)
            return true;                                   // tooltip timer running
    }
    for (int i = 0; i < ctx->notif_count; i++) {
        UiNotification* n = ctx->notifs[i];
        if (n->closing && n->anim_t > 0.0) return true;    // sliding out
        if (!n->closing && n->anim_t < 1.0) return true;   // sliding in
    }
    if (ctx->dialog.active) {                              // dialog fade in or out
        if (ctx->dialog.closing && ctx->dialog.anim_t > 0.0) return true;
        if (!ctx->dialog.closing && ctx->dialog.anim_t < 1.0) return true;
    }
    return false;                                          // all settled
}

// advances time-based widget state
void ui_tick(UiContext* ctx, double dt) {
    if (dt <= 0.0) return;                               // no time passed
    if (dt > 0.25) dt = 0.25;                            // clamp after a long pause

    // tooltip: fire the pending timer, or drop a stale one
    {
        double now = apex_now_seconds();
        if (ctx->tooltip_shown_id) {
            // a shown tooltip is only valid while its widget is alive and visible
            bool found = false;
            for (int i = 0; i < ctx->pool_used; i++)
                if (ctx->pool[i].id == ctx->tooltip_shown_id &&
                    ctx->pool[i].visible &&
                    ctx->pool[i].tooltip.len > 0) {
                    found = true;
                    break;
                }
            if (!found) {
                ctx->tooltip_shown_id = 0;
                ctx->full_repaint = true;
                ctx->redraw_needed = true;
            }
        } else if (ctx->hovered_id &&
                   !ctx->context_menu.active &&
                   !ctx->dialog.active) {
            // pending timer for the hovered widget?
            WidgetState* s = NULL;
            for (int i = 0; i < ctx->state.count; i++)
                if (ctx->state.entries[i].id == ctx->hovered_id) {
                    s = &ctx->state.entries[i];
                    break;
                }
            if (s && s->tooltip_hover_start > 0) {
                Widget* hw = NULL;
                for (int i = 0; i < ctx->pool_used; i++)
                    if (ctx->pool[i].id == ctx->hovered_id) {
                        hw = &ctx->pool[i];
                        break;
                    }
                if (hw && hw->visible && hw->tooltip.len > 0) {
                    float delay = hw->tooltip_delay > 0
                                ? hw->tooltip_delay
                                : UI_TOOLTIP_DELAY_DEFAULT;
                    if (now - s->tooltip_hover_start >= delay) {
                        ctx->tooltip_shown_id = ctx->hovered_id;
                        ctx->tooltip_x = ctx->input.x;
                        ctx->tooltip_y = ctx->input.y;
                        ctx->full_repaint = true;         // tooltip draws over the tree
                        ctx->redraw_needed = true;
                    }
                }
            }
        }
    }

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

    // switch knobs run a fixed-duration ease-out toward the checked state
    {
        bool switch_moved = false;
        for (int i = 0; i < ctx->pool_used; i++) {
            Widget* w = &ctx->pool[i];
            if (w->kind != WK_SWITCH) continue;
            WidgetState* s = ui_state_get(ctx, w->id);
            float target = w->checked ? 1.0f : 0.0f;

            if (s->switch_to != target) {                // checked flipped: restart the ease
                s->switch_from   = s->switch_knob;       // start from wherever we are
                s->switch_to     = target;
                s->switch_anim_t = 0.0f;
            }

            if (s->switch_anim_t < UI_SWITCH_ANIM_SEC) {
                s->switch_anim_t += (float)dt;           // advance the animation clock
                float t = s->switch_anim_t / UI_SWITCH_ANIM_SEC;
                if (t > 1.0f) t = 1.0f;
                float u = 1.0f - t;
                float eased = 1.0f - u * u * u;          // ease-out cubic
                s->switch_knob = s->switch_from
                               + (s->switch_to - s->switch_from) * eased;
                switch_moved = true;
            } else {
                s->switch_knob = target;                 // settled: no per-frame churn
            }
            w->switch_knob = s->switch_knob;             // publish for hash, damage, render
        }
        if (switch_moved) ctx->redraw_needed = true;
    }

    // wk_tabs drag animation
    {
        bool tabs_moved = false;

        for (int i = 0; i < ctx->pool_used; i++) {
            Widget* w = &ctx->pool[i];
            if (w->kind != WK_TABS) continue;

            WidgetState* s = ui_state_get(ctx, w->id);
            int n = w->tab_count;
            if (n > 256) n = 256;
            if (n <= 0) { s->tab_anim_init = false; continue; }

            bool drag_active = (w->tab_drag_idx >= 0);

            // while a drag is active the tab under the cursor is drawn at the pointer position
            if (drag_active) tabs_moved = true;

            // start the animation on the first tick of a drag
            if (drag_active && !s->tab_anim_init) {
                free(s->tab_anim_x);
                free(s->tab_anim_w);
                s->tab_anim_x = (float*)calloc((size_t)n, sizeof(float));
                s->tab_anim_w = (float*)calloc((size_t)n, sizeof(float));
                s->tab_anim_n = n;
                s->tab_anim_init = true;

                int saved_idx = w->tab_drag_idx;
                int saved_tgt = w->tab_drag_target;
                w->tab_drag_idx    = -1;
                w->tab_drag_target = -1;

                float sx[256], sw[256];
                tabs_target_positions(ctx, w, sx, sw);
                for (int j = 0; j < n; j++) {
                    s->tab_anim_x[j] = sx[j];
                    s->tab_anim_w[j] = sw[j];
                }

                w->tab_drag_idx    = saved_idx;
                w->tab_drag_target = saved_tgt;

                tabs_moved = true;
            }

            if (!s->tab_anim_init) continue;

            // resize on tab count change (add / close) — just snap to layout
            if (s->tab_anim_n != n) {
                free(s->tab_anim_x);
                free(s->tab_anim_w);
                s->tab_anim_x = (float*)calloc((size_t)n, sizeof(float));
                s->tab_anim_w = (float*)calloc((size_t)n, sizeof(float));
                s->tab_anim_n = n;

                float sx[256], sw[256];
                tabs_target_positions(ctx, w, sx, sw);
                for (int j = 0; j < n; j++) {
                    s->tab_anim_x[j] = sx[j];
                    s->tab_anim_w[j] = sw[j];
                }
                tabs_moved = true;
            }

            // ease toward the current targets
            float tx[256], tw[256];
            tabs_target_positions(ctx, w, tx, tw);

            const float RATE = 32.0f;   // ~31 ms time constant
            float k = 1.0f - expf((float)-dt * RATE);

            bool settled = true;
            for (int j = 0; j < n; j++) {
                float dx = tx[j] - s->tab_anim_x[j];
                float dw = tw[j] - s->tab_anim_w[j];
                if (fabsf(dx) < 0.5f) s->tab_anim_x[j] = tx[j];
                else { s->tab_anim_x[j] += dx * k; settled = false; }
                if (fabsf(dw) < 0.5f) s->tab_anim_w[j] = tw[j];
                else { s->tab_anim_w[j] += dw * k; settled = false; }
            }

            if (!settled) tabs_moved = true;

            // mirror onto the live widget for the renderer
            w->tab_anim_x = s->tab_anim_x;
            w->tab_anim_w = s->tab_anim_w;
            w->tab_anim_n = s->tab_anim_n;

            // once the drag ends and the animation settles
            if (!drag_active && settled) {
                s->tab_anim_init = false;
                w->tab_anim_x = NULL;
                w->tab_anim_w = NULL;
                w->tab_anim_n = 0;
            }
        }

        if (tabs_moved) {
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;   // transient: only during a drag
        }
    }

    // text_area: keep the focused caret visible when the caret actually moves
    for (int i = 0; i < ctx->pool_used; i++) {
        Widget* w = &ctx->pool[i];
        if (w->kind != WK_TEXT_AREA || !w->focused) continue;
        WidgetState* s = ui_state_get(ctx, w->id);
        if (s->cursor != s->last_ensure_cursor) {
            s->last_ensure_cursor = s->cursor;
            text_area_ensure_visible(ctx, w);
        }
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
            double now = apex_now_seconds();
            if (d->open_started_at == 0.0) {
                d->open_started_at = now;                        // anchor: don't advance yet
                ctx->redraw_needed = true;                       // make the next tick soon
            } else {
                double t = (now - d->open_started_at) / DLG_OPEN_SEC;
                if (t > 1.0) t = 1.0;                            // clamp: fully opaque
                if (t != d->anim_t) {
                    d->anim_t = t;
                    ctx->redraw_needed = true;                   // schedule a repaint
                }
            }
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
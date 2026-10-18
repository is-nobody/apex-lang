// source/libraries/ui/widgets/ui_tabs.c
// Tab strip: descriptor flattening, hit testing, click/drag handling, rendering
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_table.h"
#include "ui_text.h"
#include "ui_render.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// hard cap on tabs per widget; matches the local slot arrays used everywhere
#define TABS_MAX 256

// copies n bytes into the per-frame arena, null-terminated
static Str tabs_arena_dup(UiContext* ctx, const char* s, int n) {
    char* p = (char*)ui_arena_alloc(ctx, (size_t)n + 1);     // payload plus null terminator
    if (n > 0) memcpy(p, s, (size_t)n);                      // copy the payload
    p[n] = 0;                                                // null terminate
    Str r; r.ptr = p; r.len = n;                             // return by value
    return r;                                                // caller gets the arena view
}

// preferred width of a single tab label
static int tabs_label_width(UiContext* ctx, TabItem* tab) {
    if (!tab->label.ptr || tab->label.len <= 0) return 0;    // empty label: zero width
    float w = ui_text_measure_cached(ctx, tab->label.ptr, tab->label.len, UI_FS_BODY);  // measure at body size
    return (int)ceilf(w);                                    // round up so the pref width is never under-counted
}

// intrinsic width of tab idx before fitting/clamping
static int tabs_pref_width(UiContext* ctx, Widget* w, int idx) {
    TabItem* t = &w->tabs[idx];                              // the tab in question
    int lw = tabs_label_width(ctx, t);                       // measured label width
    int pref = lw + UI_TAB_PAD_X + UI_TAB_PAD_R;             // label plus side paddings
    if (t->closeable) pref += UI_TAB_LABEL_GAP + UI_TAB_CLOSE_SZ;  // reserve the close slot
    if (pref < UI_TAB_MIN_W) pref = UI_TAB_MIN_W;            // clamp to the minimum tab width
    if (pref > UI_TAB_MAX_W) pref = UI_TAB_MAX_W;            // clamp to the maximum tab width
    return pref;                                             // final intrinsic width
}

// content-driven preferred width of the whole strip, including the "+" button
float tabs_natural_width(UiContext* ctx, Widget* w) {
    if (!w) return 0.0f;                                     // null guard
    int n = w->tab_count;                                    // live tab count
    if (n > TABS_MAX) n = TABS_MAX;                          // respect the hard cap
    int total = UI_TAB_STRIP_PAD_X * 2;                      // start with left and right padding
    for (int i = 0; i < n; i++) total += tabs_pref_width(ctx, w, i);  // sum every tab's pref
    if (n > 1) total += (n - 1) * UI_TAB_GAP;                // add the gaps between tabs
    if (w->tab_show_add) total += UI_TAB_GAP + UI_TAB_ADD_SZ;  // reserve the "+" button slot
    return (float)total;                                     // hand back the natural width
}

// fills out[i] with the fitted pixel width of tab i in original order
static int tabs_fit(UiContext* ctx, Widget* w, int wx, int ww, int* out) {
    int n = w->tab_count;                                    // live tab count
    if (n > TABS_MAX) n = TABS_MAX;                          // respect the hard cap
    int sum = 0;                                             // total of every pref width
    for (int i = 0; i < n; i++) {
        out[i] = tabs_pref_width(ctx, w, i);                 // seed each slot with its pref
        sum += out[i];                                       // accumulate the total
    }
    int tab_gaps    = (n > 1) ? (n - 1) * UI_TAB_GAP : 0;    // space eaten by inter-tab gaps
    int add_reserve = w->tab_show_add ? (UI_TAB_GAP + UI_TAB_ADD_SZ) : 0;  // space for the "+" button
    int avail = ww - tab_gaps - add_reserve - (UI_TAB_STRIP_PAD_X * 2); // usable width for the tabs
    if (avail < 1) avail = 1;                                // guard against a degenerate strip

    if (sum > avail) {                                       // strip overflows: shrink the tabs
        int min_total = n * UI_TAB_MIN_W;                    // every tab at its minimum width
        if (min_total <= avail) {                            // shrink fits: scale proportionally
            float scale = (float)avail / (float)sum;         // uniform scale factor
            for (int i = 0; i < n; i++) {
                int s = (int)(out[i] * scale);               // scaled width
                if (s < UI_TAB_MIN_W) s = UI_TAB_MIN_W;      // never go below the minimum
                out[i] = s;                                  // publish the fitted width
            }
        } else {
            for (int i = 0; i < n; i++) out[i] = UI_TAB_MIN_W;  // even shrinking doesn't fit: clamp all
        }
    }

    int cx = wx;                                             // running x cursor
    for (int i = 0; i < n; i++) cx += out[i] + UI_TAB_GAP;   // advance past each tab and its gap
    return cx;                                               // x after the last slot
}

// fills out_slot_tab[i] with the tab drawn in slot i (drag-aware permute)
static int tabs_layout(UiContext* ctx, Widget* w, int wx, int ww,
                       int* out_slot_tab, int* out_slot_x, int* out_slot_w) {
    int n = w->tab_count;                                    // live tab count
    if (n > TABS_MAX) n = TABS_MAX;                          // respect the hard cap

    // slot -> tab permutation for the active drag
    int perm[TABS_MAX];                                      // slot index -> source tab index
    for (int i = 0; i < n; i++) perm[i] = i;                 // identity when no drag is active
    if (w->tab_drag_idx >= 0 && w->tab_drag_idx < n) {       // drag is live
        int from = w->tab_drag_idx;                          // source tab
        int to   = w->tab_drag_target;                       // destination slot
        if (to < 0) to = 0;                                  // clamp target low
        if (to > n - 1) to = n - 1;                          // clamp target high
        if (from != to) {                                    // only permute when the slot changed
            if (from < to) {                                 // dragging right: shift the range left
                for (int i = from; i < to; i++) perm[i] = perm[i + 1];
                perm[to] = from;                             // dropped tab lands at the target
            } else {                                         // dragging left: shift the range right
                for (int i = from; i > to; i--) perm[i] = perm[i - 1];
                perm[to] = from;                             // dropped tab lands at the target
            }
        }
    }

    // fitted widths per tab, in original order
    int prefs[TABS_MAX];                                     // fitted pixel width per tab
    tabs_fit(ctx, w, wx, ww, prefs);                         // resolve fitted widths

    // assign slots
    int cx = wx + UI_TAB_STRIP_PAD_X;                        // running x cursor
    for (int i = 0; i < n; i++) {                            // one slot per position
        int ti = perm[i];                                    // the tab drawn in this slot
        out_slot_tab[i] = ti;                                // publish the tab index
        out_slot_x[i]   = cx;                                // slot left edge
        out_slot_w[i]   = prefs[ti];                         // slot width follows the tab's fitted width
        cx += prefs[ti] + UI_TAB_GAP;                        // advance past the slot and its gap
    }
    return cx;                                               // x after the last slot
}

// compute the current layout target for each tab, indexed by tab order
void tabs_target_positions(UiContext* ctx, Widget* w,
                           float* out_x, float* out_w) {
    int n = w->tab_count;                                    // live tab count
    if (n > TABS_MAX) n = TABS_MAX;                          // respect the hard cap
    if (n <= 0) return;                                      // nothing to lay out

    int wx = (int)floorf(w->x + 0.5f);                       // widget x, rounded
    int ww = (int)floorf(w->w + 0.5f);                       // widget width, rounded

    int slot_tab[TABS_MAX], slot_x[TABS_MAX], slot_w[TABS_MAX];  // slot layout scratch
    tabs_layout(ctx, w, wx, ww, slot_tab, slot_x, slot_w);   // resolve the slot layout

    // invert the slot permutation: slot -> tab index -> position
    for (int i = 0; i < n; i++) {                            // one entry per slot
        int ti = slot_tab[i];                                // the tab in this slot
        if (ti >= 0 && ti < n) {                             // sanity check
            out_x[ti] = (float)(slot_x[i] - wx);             // widget-local x for this tab
            out_w[ti] = (float)slot_w[i];                    // widget-local width for this tab
        }
    }
}

// slot index containing (mx, my), or -1
static int tabs_slot_at(UiContext* ctx, Widget* w, int wx, int wy, int ww,
                        int mx, int my) {
    if (mx < wx || mx >= wx + ww) return -1;                 // outside on x
    if (my < wy || my >= wy + UI_TABS_H) return -1;          // outside on y
    int n = w->tab_count;                                    // live tab count
    if (n > TABS_MAX) n = TABS_MAX;                          // respect the hard cap
    int slot_tab[TABS_MAX], slot_x[TABS_MAX], slot_w[TABS_MAX];  // slot layout scratch
    tabs_layout(ctx, w, wx, ww, slot_tab, slot_x, slot_w);   // resolve the slot layout
    for (int i = 0; i < n; i++) {                            // one check per slot
        if (mx >= slot_x[i] && mx < slot_x[i] + slot_w[i]) return i;  // inside this slot
    }
    return -1;                                               // miss
}

// tab index under (mx, my), or -1
static int tabs_hit_idx(UiContext* ctx, Widget* w, int wx, int wy, int ww,
                        int mx, int my) {
    int slot = tabs_slot_at(ctx, w, wx, wy, ww, mx, my);     // resolve the slot first
    if (slot < 0) return -1;                                 // outside the strip
    int slot_tab[TABS_MAX], slot_x[TABS_MAX], slot_w[TABS_MAX];  // slot layout scratch
    tabs_layout(ctx, w, wx, ww, slot_tab, slot_x, slot_w);   // resolve the slot layout
    return slot_tab[slot];                                   // map slot -> tab index
}

// true when the given tab is the selected one
static bool tabs_is_active(Widget* w, int ti) {
    TabItem* t = &w->tabs[ti];                               // the tab in question
    return w->tab_selected.len > 0 &&                        // a selection exists
           t->key.len == w->tab_selected.len &&              // and its key matches this tab
           memcmp(t->key.ptr, w->tab_selected.ptr, (size_t)t->key.len) == 0;
}

// true when the close button of the slot under (mx, my) is visible and hit
static bool tabs_close_hit(UiContext* ctx, Widget* w, int wx, int wy, int ww,
                           int mx, int my) {
    int slot = tabs_slot_at(ctx, w, wx, wy, ww, mx, my);     // resolve the slot first
    if (slot < 0) return false;                              // outside the strip
    int slot_tab[TABS_MAX], slot_x[TABS_MAX], slot_w[TABS_MAX];  // slot layout scratch
    tabs_layout(ctx, w, wx, ww, slot_tab, slot_x, slot_w);   // resolve the slot layout
    int ti = slot_tab[slot];                                 // the tab in this slot
    TabItem* t = &w->tabs[ti];                               // the tab in question
    if (!t->closeable) return false;                         // nothing to hit

    // close button is visible only on the active tab or the hovered one
    bool active  = tabs_is_active(w, ti);                    // is this the selected tab?
    bool hovered = (ti == w->tab_hover_idx);                 // is this the hovered tab?
    if (!(active || hovered)) return false;                  // hidden: no hit

    int tx = slot_x[slot];                                   // slot left edge
    int tw = slot_w[slot];                                   // slot width
    int cb_x = tx + tw - UI_TAB_CLOSE_SZ - UI_TAB_PAD_R;     // close button left edge
    int tab_h = UI_TABS_H - UI_TAB_TOP_PAD - UI_TAB_BOTTOM_PAD;
    int cb_y = wy + UI_TAB_TOP_PAD + (tab_h - UI_TAB_CLOSE_SZ) / 2;
    return mx >= cb_x && mx < cb_x + UI_TAB_CLOSE_SZ &&      // inside on x
           my >= cb_y && my < cb_y + UI_TAB_CLOSE_SZ;        // and on y
}

// true when (mx, my) is on the "+" button
static bool tabs_add_hit(UiContext* ctx, Widget* w, int wx, int wy, int ww,
                         int mx, int my) {
    if (!w->tab_show_add) return false;                          // feature disabled: no hit
    int n = w->tab_count;                                        // live tab count
    if (n > TABS_MAX) n = TABS_MAX;                              // respect the hard cap
    int slot_tab[TABS_MAX], slot_x[TABS_MAX], slot_w[TABS_MAX];  // slot layout scratch
    int end_x = tabs_layout(ctx, w, wx, ww, slot_tab, slot_x, slot_w);  // x after the last tab
    int ay = wy + (UI_TABS_H - UI_TAB_ADD_SZ) / 2;               // screen-space top of the button
    return mx >= end_x && mx < end_x + UI_TAB_ADD_SZ &&          // inside on x
           my >= ay && my < ay + UI_TAB_ADD_SZ;                  // and on y
}

// target slot for the tab being dragged, given a pointer x
static int tabs_drag_target(UiContext* ctx, Widget* w, int wx, int ww, int mx) {
    int n = w->tab_count;                                        // live tab count
    int from = w->tab_drag_idx;                                  // source tab index
    if (n <= 1 || from < 0) return 0;                            // trivial: single tab or no drag
    if (n > TABS_MAX) n = TABS_MAX;                              // respect the hard cap

    // fitted widths as if no drag were active
    int prefs[TABS_MAX];                                         // fitted pixel width per tab
    tabs_fit(ctx, w, wx, ww, prefs);                             // resolve fitted widths

    int cx = wx + UI_TAB_STRIP_PAD_X;                            // running x cursor
    int count = 0;                                               // slots the pointer has passed
    for (int i = 0; i < n; i++) {                                // one check per tab
        if (i != from) {                                         // skip the dragged tab itself
            int mid = cx + prefs[i] / 2;                         // midpoint of this tab
            if (mx > mid) count++;                               // pointer past the midpoint: advance
        }
        cx += prefs[i] + UI_TAB_GAP;                             // advance the cursor
    }
    if (count > n - 1) count = n - 1;                            // clamp high
    if (count < 0) count = 0;                                    // clamp low
    return count;                                                // target slot index
}

// flattens a wk_tabs descriptor's "tabs" array into w->tabs
void tabs_build_items(UiContext* ctx, Widget* w, Table* arr) {
    int n = arr->array_part ? arr->array_count : 0;              // number of array entries
    if (n > TABS_MAX) n = TABS_MAX;                              // respect the hard cap
    if (n <= 0) {                                                // empty array: nothing to build
        w->tabs = NULL;                                          // no tab storage
        w->tab_count = 0;                                        // zero live tabs
        return;
    }

    w->tabs = (TabItem*)ui_arena_alloc(ctx, sizeof(TabItem) * (size_t)n);  // arena storage for the tabs
    memset(w->tabs, 0, sizeof(TabItem) * (size_t)n);             // clear every slot
    w->tab_count = 0;                                            // reset the live count

    for (int i = 0; i < n; i++) {                                // one pass per entry
        Value item = arr->array_part[i];                         // the tab descriptor
        if (IS_NONE(item)) break;                                // hole: stop the walk
        if (!IS_TABLE(item)) continue;                           // non-table: skip
        Table* it = AS_TABLE(item);                              // unwrap the descriptor

        TabItem* t = &w->tabs[w->tab_count];                     // next free slot

        StrView lv, kv;                                          // string views for label and key
        if (table_get_str_view(it, ctx->key_cache[UK_LABEL], &lv))
            t->label = tabs_arena_dup(ctx, lv.ptr, lv.len);      // "label" wins
        else if (table_get_str_view(it, ctx->key_cache[UK_CONTENT], &lv))
            t->label = tabs_arena_dup(ctx, lv.ptr, lv.len);      // "content" fallback
        if (table_get_str_view(it, ctx->key_cache[UK_KEY], &kv))
            t->key = tabs_arena_dup(ctx, kv.ptr, kv.len);        // script key for this tab

        t->closeable = table_get_bool_key(it, ctx->key_cache[UK_CLOSEABLE],
                                          w->tab_default_closeable);  // per-tab close flag, or default

        w->tab_count++;                                          // commit this slot
    }
}

// records the pressed tab / "+" button and the pointer x at press time
void tabs_handle_mousedown(UiContext* ctx, Widget* w, int x, int y) {
    int wx = (int)floorf(w->x + 0.5f);                           // widget x, rounded
    int wy = (int)floorf(w->y + 0.5f);                           // widget y, rounded
    int ww = (int)floorf(w->w + 0.5f);                           // widget width, rounded

    // a press on the "+" button takes priority
    if (tabs_add_hit(ctx, w, wx, wy, ww, x, y)) {                // hit test the "+" first
        w->tab_add_press    = true;                              // latch the press
        w->tab_press_idx    = -1;                                // no tab pressed
        w->tab_press_close  = false;                             // not on a close button
        w->tab_drag_idx     = -1;                                // no drag started
        w->tab_drag_start_x = x;                                 // remember the press x
        return;
    }

    // otherwise: press inside a tab
    int idx = tabs_hit_idx(ctx, w, wx, wy, ww, x, y);            // resolve the tab under the pointer
    w->tab_add_press    = false;                                 // clear any add-button press
    w->tab_press_idx    = idx;                                   // remember the pressed tab
    w->tab_press_close  = (idx >= 0) &&                          // and whether the press landed
                          tabs_close_hit(ctx, w, wx, wy, ww, x, y);  // on its close button
    w->tab_drag_idx     = -1;                                    // no drag yet
    w->tab_drag_target  = -1;                                    // no target yet
    w->tab_drag_start_x = x;                                     // remember the press x
    w->tab_drag_cur_x   = x;                                     // seed the current x

    // record how far into the tab the pointer landed
    w->tab_drag_grab_dx = 0;                                     // default: no offset
    if (idx >= 0 && !w->tab_press_close) {                       // a real tab press, not on close
        int slot_tab[TABS_MAX], slot_x[TABS_MAX], slot_w[TABS_MAX];  // slot layout scratch
        tabs_layout(ctx, w, wx, ww, slot_tab, slot_x, slot_w);   // resolve the slot layout
        for (int i = 0; i < w->tab_count; i++) {                 // find the slot holding this tab
            if (slot_tab[i] == idx) {                            // this slot holds the pressed tab
                w->tab_drag_grab_dx = x - slot_x[i];             // pointer offset from the tab's left edge
                break;
            }
        }
    }
}

// refreshes hover and drag state; returns true if anything changed
bool tabs_handle_mousemove(UiContext* ctx, Widget* w, int x, int y) {
    int wx = (int)floorf(w->x + 0.5f);                       // widget x, rounded
    int wy = (int)floorf(w->y + 0.5f);                       // widget y, rounded
    int ww = (int)floorf(w->w + 0.5f);                       // widget width, rounded

    // drag in progress: update the target, no hover changes
    if (w->tab_drag_idx >= 0) {                              // a tab is being dragged
        w->tab_drag_cur_x = x;                               // track the latest pointer x
        int nt = tabs_drag_target(ctx, w, wx, ww, x);        // resolve the new target slot
        w->tab_drag_target = nt;                             // publish it
        return true;                                         // always repaint while dragging
    }

    // a press is held on a tab: promote to a drag once the pointer moves far enough
    if (ctx->input.lmb && w->tab_press_idx >= 0 && !w->tab_press_close &&
        w->tab_reorder) {                                    // reorder enabled and a real tab press
        int dx = x - w->tab_drag_start_x;                    // horizontal displacement
        if (dx < 0) dx = -dx;                                // absolute value
        if (dx > UI_TAB_DRAG_THRESH) {                       // past the drag threshold
            w->tab_drag_idx    = w->tab_press_idx;           // promote the press to a drag
            w->tab_drag_target = w->tab_press_idx;           // start at the current slot
            w->tab_drag_cur_x  = x;                          // seed the current pointer x
            w->tab_hover_idx   = -1;                         // clear hover while dragging
            w->tab_hover_close = false;                      // clear close hover
            w->tab_add_hover   = false;                      // clear add-button hover
            return true;                                     // repaint
        }
    }

    // normal hover tracking
    int  new_idx       = tabs_hit_idx(ctx, w, wx, wy, ww, x, y);     // tab under the pointer
    bool new_close     = (new_idx >= 0) &&                           // is the pointer on its close button?
                         tabs_close_hit(ctx, w, wx, wy, ww, x, y);
    bool new_add_hover = tabs_add_hit(ctx, w, wx, wy, ww, x, y);     // is the pointer on the "+" button?

    bool changed = (new_idx       != w->tab_hover_idx) ||            // hovered tab changed
                   (new_close     != w->tab_hover_close) ||          // close hover changed
                   (new_add_hover != w->tab_add_hover);              // add hover changed

    w->tab_hover_idx   = new_idx;                                    // publish the hovered tab
    w->tab_hover_close = new_close;                                  // publish the close hover
    w->tab_add_hover   = new_add_hover;                              // publish the add hover
    return changed;                                                  // report whether anything moved
}

// finishes a drag and emits a tab_move event. returns true when it consumed the mouseup.
bool tabs_handle_mouseup(UiContext* ctx, Widget* w, int x, int y) {
    (void)x; (void)y;
    bool consumed = false;

    int from = w->tab_drag_idx;                              // source tab index
    int to   = w->tab_drag_target;                           // destination slot

    if (from >= 0) {
        if (to < 0) to = 0;                                  // clamp target low
        if (to >= w->tab_count) to = w->tab_count - 1;       // clamp target high

        if (from != to && from < w->tab_count) {             // only emit on a real move
            UiEvent ev; memset(&ev, 0, sizeof(ev));          // zero the event
            ev.kind = EV_CHANGE;                             // change kind
            ev.widget_id = w->id;                            // source widget id
            strncpy(ev.widget_type, "tab_move", 15);         // widget type name

            // key_name carries the moved tab's key
            TabItem* t = &w->tabs[from];                     // the tab being moved
            if (t->key.ptr && t->key.len > 0) {
                int copy = t->key.len < 63 ? t->key.len : 63;
                memcpy(ev.key_name, t->key.ptr, (size_t)copy);
                ev.key_name[copy] = 0;                       // null terminate
            }
            // num_value = destination index, scroll_dy = source index
            ev.num_value = (double)to;                       // destination slot
            ev.scroll_dy = (double)from;                     // source slot
            ev.mouse_x   = ctx->input.x;                     // pointer position
            ev.mouse_y   = ctx->input.y;
            ui_event_push(ctx, &ev);                         // enqueue the move
        }
        consumed = true;                                     // a drag always consumes the mouseup

        // permute the animation arrays to match the reorder the script is about to apply
        WidgetState* st = ui_state_get(ctx, w->id);          // persistent tab state
        if (st->tab_anim_init &&                             // only when the anim is live
            st->tab_anim_n == w->tab_count &&                // and the count still matches
            st->tab_anim_n > 0 &&
            st->tab_anim_x && st->tab_anim_w)
        {
            int n = w->tab_count;                            // live tab count
            int perm[TABS_MAX];                              // slot -> source permute
            for (int i = 0; i < n; i++) perm[i] = i;         // identity by default
            if (from != to) {                                // mirror tabs_layout's permute
                if (from < to) {
                    for (int i = from; i < to; i++) perm[i] = perm[i + 1];
                    perm[to] = from;
                } else {
                    for (int i = from; i > to; i--) perm[i] = perm[i - 1];
                    perm[to] = from;
                }
            }

            float* nx = (float*)malloc(sizeof(float) * (size_t)n);  // new x array
            float* nw = (float*)malloc(sizeof(float) * (size_t)n);  // new w array
            if (nx && nw) {
                for (int i = 0; i < n; i++) {                // apply the permute
                    nx[i] = st->tab_anim_x[perm[i]];
                    nw[i] = st->tab_anim_w[perm[i]];
                }

                // the dragged tab now lives at index `to`
                nx[to] = (float)(w->tab_drag_cur_x - w->tab_drag_grab_dx) - w->x;  // pointer local x

                float* old_x = st->tab_anim_x;               // stash old x buffer
                float* old_w = st->tab_anim_w;               // stash old w buffer
                st->tab_anim_x = nx;                         // publish the new x array
                st->tab_anim_w = nw;                         // publish the new w array
                if (w->tab_anim_x == old_x) w->tab_anim_x = nx;  // keep the widget view in sync
                if (w->tab_anim_w == old_w) w->tab_anim_w = nw;  // keep the widget view in sync
                free(old_x);                                 // release the old x buffer
                free(old_w);                                 // release the old w buffer
            } else {
                free(nx); free(nw);                          // oom: bail without touching state
            }
        }
    }

    w->tab_drag_idx    = -1;                                 // end the drag
    w->tab_drag_target = -1;                                 // clear the target
    w->tab_add_press   = false;                              // clear any add-button press
    ctx->redraw_needed = true;                               // schedule a repaint
    return consumed;                                         // report consumption
}

// emits the select / close / add event matching the recorded press
void tabs_handle_click(UiContext* ctx, Widget* w) {
    // "+" button
    if (w->tab_add_press && w->tab_add_hover) {                  // press landed on the add button
        UiEvent ev; memset(&ev, 0, sizeof(ev));                  // zero the event
        ev.kind = EV_CLICK;                                      // click kind
        ev.widget_id = w->id;                                    // source widget id
        strncpy(ev.widget_type, "tab_add", 15);                  // widget type name
        if (w->key_str) strncpy(ev.key_name, w->key_str, 63);    // tabs widget key, if any
        ev.mouse_x = ctx->input.x;                               // pointer x
        ev.mouse_y = ctx->input.y;                               // pointer y
        ui_event_push(ctx, &ev);                                 // enqueue the add event
        w->tab_add_press = false;                                // clear the press latch
        ctx->redraw_needed = true;                               // schedule a repaint
        return;                                                  // done with this press
    }
    w->tab_add_press = false;                                    // clear any stale add press

    int idx = w->tab_press_idx;                                  // the tab that was pressed
    if (idx < 0 || idx >= w->tab_count) return;                  // no valid tab: nothing to emit
    TabItem* t = &w->tabs[idx];                                  // the pressed tab

    UiEvent ev; memset(&ev, 0, sizeof(ev));                      // zero the event
    ev.widget_id = w->id;                                        // source widget id
    ev.mouse_x = ctx->input.x;                                   // pointer x
    ev.mouse_y = ctx->input.y;                                   // pointer y
    if (t->key.ptr && t->key.len > 0) {                          // per-tab key present?
        int copy = t->key.len < 63 ? t->key.len : 63;            // clamp to the key_name buffer
        memcpy(ev.key_name, t->key.ptr, (size_t)copy);           // copy the key bytes
        ev.key_name[copy] = 0;                                   // null terminate
    }

    if (w->tab_press_close) {                                    // press was on the close button
        ev.kind = EV_CHANGE;                                     // close emits a change event
        strncpy(ev.widget_type, "tab_close", 15);                // widget type name
        ev.bool_value = true;                                    // always true for close

        // after the close, the tab immediately to the left should take over selection
        int next = (idx > 0) ? idx - 1 : 0;
        if (next == idx && idx + 1 < w->tab_count) next = idx + 1;
        if (next != idx) {
            TabItem* nt = &w->tabs[next];
            if (nt->key.ptr && nt->key.len > 0) {
                ev.str_value = (char*)malloc((size_t)nt->key.len + 1);
                if (ev.str_value) {
                    memcpy(ev.str_value, nt->key.ptr, (size_t)nt->key.len);
                    ev.str_value[nt->key.len] = 0;
                }
            }
        }
    } else {                                                     // plain tab press
        ev.kind = EV_CLICK;                                      // select emits a click event
        strncpy(ev.widget_type, "tab", 15);                      // widget type name
    }

    ui_event_push(ctx, &ev);                                     // enqueue the tab event
    ctx->redraw_needed = true;                                   // schedule a repaint
}

// draws a single tab into (tx, ty, tw)
static void tabs_draw_one(UiContext* ctx, Widget* w, int idx,
                          int tx, int ty, int tw,
                          bool hovered, bool dragging) {
    TabItem* t = &w->tabs[idx];                                  // the tab in question
    bool active = tabs_is_active(w, idx);                        // is this the selected tab?

    // close button is visible on the active tab or the hovered one
    bool close_visible = t->closeable && (active || hovered);    // show only when it matters
    bool close_hov     = hovered && w->tab_hover_close && close_visible;  // pointer over the close glyph

    // background priority: dragging > active > hovered > idle
    uint32_t bg;                                                 // resolved background color
    if (dragging)     bg = UI_C_TAB_DRAG_BG;                     // dragged tab: strongest fill
    else if (active)  bg = UI_C_TAB_BG_ACT;                      // active tab: filled
    else if (hovered) bg = UI_C_TAB_BG_HOV;                      // hovered tab: lighter fill
    else              bg = UI_C_TAB_BG;                          // idle tab: default fill
    int tab_h = UI_TABS_H - UI_TAB_TOP_PAD - UI_TAB_BOTTOM_PAD;
    ui_painter_fill_round(tx, ty + UI_TAB_TOP_PAD, tw, tab_h, UI_TAB_RADIUS, bg);  // rounded tab body

    // foreground priority mirrors the background
    uint32_t fg;                                                 // resolved foreground color
    if (dragging || active) fg = UI_C_TAB_FG_ACT;                // active or dragged: bright label
    else if (hovered)       fg = UI_C_TAB_FG_HOV;                // hovered: slightly brighter
    else                    fg = UI_C_TAB_FG;                    // idle: dim label

    // label rect: reserve the close slot only when the close button is drawn
    int label_max_w;                                             // usable width for the label
    if (close_visible)
        label_max_w = tw - UI_TAB_PAD_X - UI_TAB_PAD_R
                         - UI_TAB_LABEL_GAP - UI_TAB_CLOSE_SZ;   // close slot is reserved
    else
        label_max_w = tw - UI_TAB_PAD_X - UI_TAB_PAD_R;          // idle: label gets the extra room
    if (label_max_w < 4) label_max_w = 4;                        // never shrink below 4px

    int label_x = tx + UI_TAB_PAD_X;                             // label left edge

    if (t->label.ptr && t->label.len > 0 && ctx->font) {         // draw only when there is text
        float full_w = ui_text_measure_cached(ctx, t->label.ptr, t->label.len, UI_FS_BODY);  // full label width
        float base = ty + UI_TAB_TOP_PAD + (tab_h + UI_FS_BODY * 0.7f) * 0.5f;  // vertically centered baseline

        if (full_w <= (float)label_max_w + 0.5f) {               // whole label fits
            ui_draw_text_at(ctx, (float)label_x, base,
                            t->label.ptr, t->label.len, UI_FS_BODY, fg);
        } else {                                                 // need to truncate with "..."
            const char ell[] = "...";                            // ellipsis marker
            float ell_w = ui_text_measure_cached(ctx, ell, 3, UI_FS_BODY);  // ellipsis width
            if ((float)label_max_w > ell_w) {                    // room for at least "..."
                float budget = (float)label_max_w - ell_w;       // width left for the prefix
                int cut = 0;                                     // byte length of the kept prefix
                float cut_w = 0.0f;                              // measured width of that prefix
                int i = 0;                                       // byte cursor
                while (i < t->label.len) {                       // grow the prefix one codepoint at a time
                    int next = ui_utf8_next(t->label.ptr, i, t->label.len);  // advance one codepoint
                    float w2 = ui_text_measure_cached(ctx, t->label.ptr, next, UI_FS_BODY);  // prefix width
                    if (w2 > budget) break;                      // this codepoint would overflow
                    i = next;                                    // keep it
                    cut = i;                                     // remember the byte length
                    cut_w = w2;                                  // remember the width
                }
                if (cut > 0) {                                   // some prefix survived
                    ui_draw_text_at(ctx, (float)label_x, base,
                                    t->label.ptr, cut, UI_FS_BODY, fg);  // draw the prefix
                    ui_draw_text_at(ctx, (float)label_x + cut_w, base,
                                    ell, 3, UI_FS_BODY, fg);     // then the ellipsis
                }
            }
        }
    }

    if (!close_visible) return;                                  // nothing else to draw

    // close chip: optional hover background, then the shared 9x9 glyph
    int cx = tx + tw - UI_TAB_CLOSE_SZ - UI_TAB_PAD_R;           // close button left edge
    int cy = ty + UI_TAB_TOP_PAD + (tab_h - UI_TAB_CLOSE_SZ) / 2 + 1; // close button top edge

    if (close_hov)                                               // pointer over the glyph
        ui_painter_fill_round(cx, cy, UI_TAB_CLOSE_SZ, UI_TAB_CLOSE_SZ,
                              UI_TAB_CLOSE_SZ / 2, UI_C_TAB_CLOSE_BG);  // round hover chip

    uint32_t close_fg = close_hov ? UI_C_TAB_CLOSE_HOV : UI_C_TAB_CLOSE;  // glyph color by state
    blit_close_x_shared(cx + (UI_TAB_CLOSE_SZ - 9) / 2,          // centered 9x9 close glyph
                        cy + (UI_TAB_CLOSE_SZ - 9 + 1) / 2,
                        close_fg);
}

// draws the "+" button at (ax, ay)
static void tabs_draw_add(Widget* w, int ax, int ay) {
    if (w->tab_add_hover)                                        // pointer over the button
        ui_painter_fill_round(ax, ay, UI_TAB_ADD_SZ, UI_TAB_ADD_SZ,
                              UI_TAB_RADIUS, UI_C_TAB_BG_HOV);   // soft hover chip

    int gx = ax + UI_TAB_ADD_SZ / 2;                             // glyph center x
    int gy = ay + UI_TAB_ADD_SZ / 2;                             // glyph center y
    uint32_t fg = w->tab_add_hover ? UI_C_TAB_FG_HOV : UI_C_TAB_FG;  // glyph color by state
    ui_painter_fill_rect(gx - 5, gy,     10, 1, fg);             // horizontal bar
    ui_painter_fill_rect(gx,     gy - 5,  1, 10, fg);            // vertical bar
}

// draws the wk_tabs strip into its own rectangle
void tabs_render(UiContext* ctx, Widget* w, int x, int y, int ww, int hh) {
    (void)hh;                                                    // height not used: strip height is fixed
    if (ww <= 0) return;                                         // degenerate strip: nothing to draw

    // strip background with a subtle 1px outline
    ui_painter_fill_rect(x, y, ww, UI_TABS_H, UI_C_TABS_BG);
    ui_painter_stroke_rect(x, y, ww, UI_TABS_H, UI_C_BORDER);

    int n = w->tab_count;                                        // live tab count
    if (n > TABS_MAX) n = TABS_MAX;                              // respect the hard cap

    int slot_tab[TABS_MAX], slot_x[TABS_MAX], slot_w[TABS_MAX];  // slot layout scratch
    int end_x = tabs_layout(ctx, w, x, ww, slot_tab, slot_x, slot_w);  // resolve the slot layout

    // per-slot draw rect
    int draw_x[TABS_MAX], draw_w[TABS_MAX];                      // resolved draw rect per slot
    bool anim = (w->tab_anim_x && w->tab_anim_w && w->tab_anim_n == n);  // eased positions available
    for (int i = 0; i < n; i++) {                                // one slot per position
        int ti = slot_tab[i];                                    // tab drawn in this slot
        if (anim && ti >= 0 && ti < n) {                         // use the eased position
            draw_x[i] = x + (int)floorf(w->tab_anim_x[ti] + 0.5f);
            draw_w[i] = (int)floorf(w->tab_anim_w[ti] + 0.5f);
        } else {                                                 // fall back to the flat layout
            draw_x[i] = slot_x[i];
            draw_w[i] = slot_w[i];
        }
    }

    // the tab being dragged follows the pointer instead of any slot
    if (w->tab_drag_idx >= 0) {                                  // drag is live
        for (int i = 0; i < n; i++) {                            // find the slot holding the dragged tab
            if (slot_tab[i] == w->tab_drag_idx) {                // this slot holds the dragged tab
                draw_x[i] = ctx->input.x - w->tab_drag_grab_dx;  // stick it to the pointer
                break;
            }
        }
    }

    // three passes so the active and dragging tabs paint above idle ones
    for (int pass = 0; pass < 3; pass++) {                       // z-order pass
        for (int i = 0; i < n; i++) {                            // one slot per iteration
            int ti = slot_tab[i];                                // tab drawn in this slot
            if (ti < 0 || ti >= w->tab_count) continue;          // skip invalid entries
            bool active   = tabs_is_active(w, ti);               // is this the selected tab?
            bool hovered  = (ti == w->tab_hover_idx);            // is this the hovered tab?
            bool dragging = (ti == w->tab_drag_idx);             // is this the dragged tab?

            bool idle_pass   = !active && !hovered && !dragging; // bottom layer
            bool hover_pass  = !active && !dragging && hovered;  // middle layer
            bool active_pass = active || dragging;               // top layer

            if (pass == 0 && idle_pass)                          // draw idle tabs first
                tabs_draw_one(ctx, w, ti, draw_x[i], y, draw_w[i], hovered, dragging);
            if (pass == 1 && hover_pass)                         // then hovered tabs
                tabs_draw_one(ctx, w, ti, draw_x[i], y, draw_w[i], hovered, dragging);
            if (pass == 2 && active_pass)                        // then active and dragged tabs
                tabs_draw_one(ctx, w, ti, draw_x[i], y, draw_w[i], hovered, dragging);
        }
    }

    // "+" button after the last tab
    if (w->tab_show_add) {                                       // feature enabled
        int ay = y + (UI_TABS_H - UI_TAB_ADD_SZ) / 2;            // vertically centered
        tabs_draw_add(w, end_x, ay);                             // draw it at the end of the strip
    }

    // the stroke_rect above already draws the bottom hairline, so we skip the manual fill
}
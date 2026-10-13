// source/libraries/ui/interaction/ui_handle.c
// Native event translation and titlebar gesture handling for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_interaction.h"
#include "ui_core.h"
#include "ui_widgets.h"
#include "ui_render.h"
#include "ui_text.h"
#include "ui_window.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

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
void handle_win_event(UiContext* ctx, const UiWinEvent* e) {
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

            // resize edges and window buttons live on the window frame
            if (ctx->custom_chrome) {
                const char* edge = titlebar_resize_edge(ctx, e->x, e->y);
                ui_window_set_resize_cursor(ctx->window, edge);
                int hot = titlebar_button_at(ctx, e->x, e->y);
                if (hot != ctx->tb_hot) {
                    ctx->tb_hot = hot;
                    ctx->redraw_needed = true;
                }
            }

            // the menubar runs after the frame handlers
            if (ui_menubar_handle_mousemove(ctx, e->x, e->y))
                break;

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

                if (ui_menubar_handle_mousedown(ctx, e->x, e->y)) // menu bar consumes the click
                    break;

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
            if (e->key == UI_KEY_ESCAPE && ui_menubar_handle_escape(ctx))
                break;                                            // menu bar ate escape
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
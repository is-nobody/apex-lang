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
    if (ctx->window && ui_window_is_fullscreen(ctx->window))
        return NULL;                                          // no resize edges in fullscreen
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
#ifdef _WIN32
            // win32: the paint callback fires synchronously
            bool already_done = (ctx->win_w == e->w &&
                                 ctx->win_h == e->h &&
                                 !ctx->geometry_dirty);
#else
            bool already_done = false;
#endif

            ctx->pixels = ui_window_pixels(ctx->window);          // refresh pixel buffer
            ctx->win_w = e->w;                                    // new width from the event
            ctx->win_h = e->h;                                    // new height from the event

            if (!already_done) {
                ctx->geometry_dirty = true;                       // root rect changed: relayout
                ctx->full_repaint   = true;                       // fresh pixels are undefined
            }

            UiEvent ev; memset(&ev, 0, sizeof(ev));
            ev.kind = EV_RESIZE;
            ev.num_value = e->w;                                  // pack new width in num_value
            ev.scroll_dy = e->h;                                  // pack new height in scroll_dy
            ui_event_push(ctx, &ev);

            if (!already_done)
                ctx->redraw_needed = true;
            break;
        }
        case UW_EV_MOUSEMOVE: {                                   // pointer moved
            ctx->input.x = e->x;                                  // track current pointer
            ctx->input.y = e->y;

            // any pointer motion hides a shown tooltip and restarts the
            // hover timer for the widget still under the cursor
            if (ctx->tooltip_shown_id) {
                ctx->tooltip_shown_id = 0;
                ctx->full_repaint     = true;                     // erase the stale box
                ctx->redraw_needed    = true;
            }
            if (ctx->hovered_id) {
                for (int i = 0; i < ctx->state.count; i++)
                    if (ctx->state.entries[i].id == ctx->hovered_id) {
                        ctx->state.entries[i].tooltip_hover_start = apex_now_seconds();
                        break;
                    }
            }

            // an open context menu swallows hover before anything else sees it
            if (ctx->context_menu.active) {
                ui_context_menu_handle_mousemove(ctx, e->x, e->y);
                break;
            }

            // a fullscreen titlebar press is waiting for real movement before it restores the window
            if (ctx->pending_fullscreen_drag) {
                if (!ctx->input.lmb) {
                    ctx->pending_fullscreen_drag = false;         // button was released
                } else {
                    int dx = e->x - ctx->pending_drag_x;
                    int dy = e->y - ctx->pending_drag_y;
                    if (dx < 0) dx = -dx;                         // absolute displacement
                    if (dy < 0) dy = -dy;
                    if (dx > UI_TAB_DRAG_THRESH || dy > UI_TAB_DRAG_THRESH) {
                        ctx->pending_fullscreen_drag = false;     // consume the gesture
                        ui_window_toggle_fullscreen(ctx->window);
                        ctx->geometry_dirty = true;
                        ctx->full_repaint   = true;
                        ctx->redraw_needed  = true;
                        ui_window_begin_drag(ctx->window);        // hand off to the WM
                    }
                }
                break;                                            // never fall through to hover
            }

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

            if (ctx->input.lmb && ctx->scrollbar_drag_id) {       // dragging a scrollbar thumb
                Widget* sc = NULL;
                for (int i = 0; i < ctx->pool_used; i++) {
                    if (ctx->pool[i].id == ctx->scrollbar_drag_id) {
                        sc = &ctx->pool[i];
                        break;
                    }
                }

                if (sc) {
                    if (sc->kind == WK_SCROLL) {
                        int hh = (int)floorf(sc->h + 0.5f);
                        double maxs = sc->content_h - hh;
                        if (maxs > 0) {
                            int th = (int)(hh * (hh / sc->content_h));
                            if (th < 24) th = 24;
                            int track_h = hh - th;
                            int dy = e->y - ctx->scrollbar_drag_start_y;

                            if (track_h > 0) {
                                double scroll_delta = ((double)dy / track_h) * maxs;
                                double new_scroll = ctx->scrollbar_drag_start_scroll + scroll_delta;
                                
                                if (new_scroll < 0) new_scroll = 0;
                                if (new_scroll > maxs) new_scroll = maxs;

                                WidgetState* s = ui_state_get(ctx, sc->id);
                                s->scroll_target = new_scroll;
                                s->scroll_y      = new_scroll;
                                sc->scroll_y     = new_scroll;
                                
                                ctx->geometry_dirty = true;
                                ctx->redraw_needed  = true;
                            }
                        }
                    } else if (sc->kind == WK_TEXT_AREA) {
                        int hh = (int)floorf(sc->h + 0.5f);
                        WidgetState* ws = ui_state_get(ctx, sc->id);
                        ta_line_cache_ensure(ws, sc->input_value.ptr, sc->input_value.len);
                        int line_count = ws->line_count;
                        if (line_count < 1) line_count = 1;
                        int content_h = line_count * UI_LINE_H;
                        int inner_h = hh - 12;
                        if (inner_h < UI_LINE_H) inner_h = UI_LINE_H;
                        
                        double maxs = content_h - inner_h;
                        if (maxs > 0) {
                            int th = (int)((hh - 4) * (inner_h / (double)content_h));
                            if (th < 24) th = 24;
                            int track_h = (hh - 4) - th;
                            int dy = e->y - ctx->scrollbar_drag_start_y;

                            if (track_h > 0) {
                                double scroll_delta = ((double)dy / track_h) * maxs;
                                double new_scroll = ctx->scrollbar_drag_start_scroll + scroll_delta;
                                
                                if (new_scroll < 0) new_scroll = 0;
                                if (new_scroll > maxs) new_scroll = maxs;

                                WidgetState* s = ui_state_get(ctx, sc->id);
                                s->scroll_target = new_scroll;
                                s->scroll_y      = new_scroll;
                                sc->scroll_y     = new_scroll;
                                
                                ctx->geometry_dirty = true;
                                ctx->redraw_needed  = true;
                            }
                        }
                    } else {
                        ctx->scrollbar_drag_id = 0;
                    }
                } else {
                    ctx->scrollbar_drag_id = 0;
                }
                break;
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
                } else if (dw && (dw->kind == WK_INPUT || dw->kind == WK_NUMBER_INPUT ||
                                  dw->kind == WK_TEXT_AREA)) {         // input: extend selection
                    WidgetState* s = ui_state_get(ctx, dw->id);
                    if (s->sel_anchor < 0) s->sel_anchor = s->cursor;  // first drag: seed anchor
                    int offset_x = (dw->kind == WK_NUMBER_INPUT) ? 28 : 0;
                    int offset_y = (dw->kind == WK_TEXT_AREA) ? 6 : 0;
                    float local_x = (float)e->x - dw->x - offset_x - 8.0f;  // into text coordinates
                    float local_y = (float)e->y - dw->y - offset_y;
                    int off;
                    if (dw->kind == WK_TEXT_AREA)
                        off = text_area_hit_offset(ctx, dw, local_x, local_y);
                    else
                        off = input_hit_offset(ctx, dw, local_x);
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

                // wk_tree: track which row is under the pointer
                if (h && h->kind == WK_TREE) {
                    int local_y = e->y - (int)h->y - 1;       // skip the 1px top border
                    int row = -1;
                    if (local_y >= 0 && h->tree_node_count > 0) {
                        row = local_y / UI_TREE_ROW_H;
                        if (row >= h->tree_node_count) row = -1;
                    }
                    WidgetState* ts = ui_state_get(ctx, h->id);
                    if (ts->hover_row != row) {
                        ts->hover_row = row;
                        h->hover_row  = row;
                        ctx->redraw_needed = true;
                    }
                }

                // wk_tabs: feed the hovered widget, plus any tabs widget with an active drag
                if (h && h->kind == WK_TABS) {
                    if (tabs_handle_mousemove(ctx, h, e->x, e->y))
                        ctx->redraw_needed = true;
                }
                for (int i = 0; i < ctx->pool_used; i++) {
                    Widget* tw = &ctx->pool[i];
                    if (tw->kind != WK_TABS) continue;
                    if (tw == h) continue;              // already fed above
                    if (tw->tab_drag_idx >= 0) {
                        if (tabs_handle_mousemove(ctx, tw, e->x, e->y))
                            ctx->redraw_needed = true;
                    } else if (tw->tab_hover_idx >= 0) {
                        tw->tab_hover_idx   = -1;
                        tw->tab_hover_close = false;
                        ctx->redraw_needed  = true;
                    }
                }
            }
            break;
        }
        case UW_EV_MOUSEDOWN:
            // any click dismisses a shown tooltip
            if (ctx->tooltip_shown_id) {
                ctx->tooltip_shown_id = 0;
                ctx->full_repaint     = true;                     // erase the stale box
                ctx->redraw_needed    = true;
            }
            if (ctx->hovered_id) {
                for (int i = 0; i < ctx->state.count; i++)
                    if (ctx->state.entries[i].id == ctx->hovered_id) {
                        ctx->state.entries[i].tooltip_hover_start = apex_now_seconds();
                        break;
                    }
            }

            if (e->button == 3) {                                 // right button
                ctx->input.x = e->x;
                ctx->input.y = e->y;

                if (ui_context_menu_handle_mousedown(ctx, e->x, e->y))
                    break;                                        // menu dismissed or handled

                if (ctx->dialog.active) break;                    // dialog eats everything else

                Widget* h = ctx->root ? hit_test(ctx, e->x, e->y) : NULL;
                if (h) fire_click_b(ctx, h, 3);                   // emit right_click
                break;
            }
            if (e->button == 1) {                                 // left button only
                ctx->input.x = e->x;
                ctx->input.y = e->y;

                if (ui_context_menu_handle_mousedown(ctx, e->x, e->y))
                    break;                                        // left-click also dismisses a menu

                if (ctx->dialog.active) {                         // modal dialog is on top
                    dialog_mouse_down(ctx, e->x, e->y);
                    break;
                }

                if (ui_menubar_handle_mousedown(ctx, e->x, e->y)) // menu bar consumes the click
                    break;

                // scrollbar thumb: record the drag start if the press landed on one
                bool hit_scrollbar = false;
                for (int i = ctx->pool_used - 1; i >= 0; i--) {
                    Widget* w = &ctx->pool[i];
                    if (!w->visible) continue;
                    
                    int wx = (int)floorf(w->x + 0.5f);
                    int wy = (int)floorf(w->y + 0.5f);
                    int ww = (int)floorf(w->w + 0.5f);
                    int hh = (int)floorf(w->h + 0.5f);

                    if (w->kind == WK_SCROLL && w->content_h > (float)hh) {
                        int sb_x = wx + ww - UI_SCROLL_W;
                        double maxs = w->content_h - hh;
                        double t = maxs > 0 ? w->scroll_y / maxs : 0;
                        int th = (int)(hh * (hh / w->content_h));
                        if (th < 24) th = 24;
                        int ty = wy + (int)(t * (hh - th));
                        
                        if (e->x >= sb_x && e->x < sb_x + UI_SCROLL_W &&
                            e->y >= ty && e->y < ty + th) {
                            ctx->scrollbar_drag_id = w->id;
                            ctx->scrollbar_drag_start_y = e->y;
                            ctx->scrollbar_drag_start_scroll = w->scroll_y;
                            ctx->input.lmb = true;
                            hit_scrollbar = true;
                            break;
                        }
                    } else if (w->kind == WK_TEXT_AREA) {
                        WidgetState* ws = ui_state_get(ctx, w->id);
                        ta_line_cache_ensure(ws, w->input_value.ptr, w->input_value.len);
                        int line_count = ws->line_count;
                        if (line_count < 1) line_count = 1;
                        int content_h = line_count * UI_LINE_H;
                        int inner_h = hh - 12;
                        if (inner_h < UI_LINE_H) inner_h = UI_LINE_H;
                        
                        if (content_h > inner_h) {
                            int sb_x = wx + ww - UI_SCROLL_W - 2;
                            double maxs = content_h - inner_h;
                            double t = maxs > 0 ? w->scroll_y / maxs : 0;
                            if (t < 0) t = 0;
                            if (t > 1) t = 1;
                            int th = (int)((hh - 4) * (inner_h / (double)content_h));
                            if (th < 24) th = 24;
                            int ty = wy + 2 + (int)(t * ((hh - 4) - th));
                            
                            if (e->x >= sb_x && e->x < sb_x + UI_SCROLL_W &&
                                e->y >= ty && e->y < ty + th) {
                                ctx->scrollbar_drag_id = w->id;
                                ctx->scrollbar_drag_start_y = e->y;
                                ctx->scrollbar_drag_start_scroll = w->scroll_y;
                                ctx->input.lmb = true;
                                hit_scrollbar = true;
                                break;
                            }
                        }
                    }
                }
                if (hit_scrollbar) break;

                // wm-owned gestures win over anything we draw
                const char* redge = titlebar_resize_edge(ctx, e->x, e->y);
                if (redge) {
                    ui_window_begin_resize(ctx->window, redge);
                    break;
                }
                if (titlebar_is_drag(ctx, e->x, e->y)) {
                    // in fullscreen a titlebar press must not restore the window by itself
                    if (ctx->window && ui_window_is_fullscreen(ctx->window)) {
                        ctx->pending_fullscreen_drag = true;
                        ctx->pending_drag_x = e->x;
                        ctx->pending_drag_y = e->y;
                        ctx->input.lmb = true;            // track that the button is held
                    } else {
                        ui_window_begin_drag(ctx->window);
                    }
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
                    
                    // dock panel header click toggles collapse
                    if (h && h->kind == WK_PANEL && h->text.len > 0) {
                        int local_y = e->y - (int)h->y;
                        if (local_y >= 0 && local_y < UI_DOCK_HEADER_H) {
                            h->collapsed = !h->collapsed;
                            WidgetState* s = ui_state_get(ctx, h->id);
                            s->collapsed = h->collapsed;
                            
                            UiEvent cev; memset(&cev, 0, sizeof(cev));
                            cev.kind = EV_CHANGE;
                            cev.widget_id = h->id;
                            strncpy(cev.widget_type, "panel_collapse", 15);
                            if (h->key_str) strncpy(cev.key_name, h->key_str, 63);
                            cev.bool_value = h->collapsed;
                            ui_event_push(ctx, &cev);
                            
                            ctx->geometry_dirty = true;
                            ctx->redraw_needed = true;
                            break; // consume the click
                        }
                    }

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
                        if (h->kind == WK_NUMBER_INPUT) {
                            int bw = 24;
                            int left_btn_end = (int)h->x + bw;
                            int right_btn_start = (int)h->x + (int)h->w - bw;
                            
                            if (e->x >= left_btn_end && e->x < right_btn_start) {
                                // clicked the text field, fall through to normal input handling
                            } else {
                                // clicked the left/right step buttons
                                double step = (e->x >= right_btn_start) ? 1.0 : -1.0;
                                ui_number_step(ctx, h, step);
                                ctx->focused_id = h->id;
                                for (int i = 0; i < ctx->pool_used; i++)
                                    ctx->pool[i].focused = (ctx->pool[i].id == ctx->focused_id);
                                ctx->redraw_needed = true;
                                break;
                            }
                        }
                        if (h->kind == WK_INPUT || h->kind == WK_NUMBER_INPUT ||
                            h->kind == WK_TEXT_AREA) {
                            int offset_x = (h->kind == WK_NUMBER_INPUT) ? 28 : 0;
                            int offset_y = (h->kind == WK_TEXT_AREA) ? 6 : 0;
                            float local_x = (float)e->x - h->x - offset_x - 8.0f;
                            float local_y = (float)e->y - h->y - offset_y;
                            int off;
                            if (h->kind == WK_TEXT_AREA)
                                off = text_area_hit_offset(ctx, h, local_x, local_y);
                            else
                                off = input_hit_offset(ctx, h, local_x);

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

                        if (h->kind == WK_TABS) {                 // record which tab was pressed
                            tabs_handle_mousedown(ctx, h, e->x, e->y);
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

                if (ctx->scrollbar_drag_id) {                     // finish a scrollbar drag
                    ctx->scrollbar_drag_id = 0;
                    break;
                }

                // a fullscreen titlebar press that never moved is a plain click
                if (ctx->pending_fullscreen_drag) {
                    ctx->pending_fullscreen_drag = false;
                    break;
                }

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
                        else if (was == 2) {
                            // while fullscreen the middle button exits fullscreen
                            if (ui_window_is_fullscreen(ctx->window))
                                ui_window_toggle_fullscreen(ctx->window);
                            else
                                ui_window_toggle_maximize(ctx->window);
                        }
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

                // finish any active tab drag first
                bool tab_drag_consumed = false;
                if (ctx->root) {
                    for (int i = 0; i < ctx->pool_used; i++) {
                        Widget* tw = &ctx->pool[i];
                        if (tw->kind != WK_TABS) continue;
                        if (tw->tab_drag_idx >= 0) {
                            if (tabs_handle_mouseup(ctx, tw, e->x, e->y))
                                tab_drag_consumed = true;
                        }
                    }
                }

                ctx->drag_widget_id = 0;                          // end any drag
                if (ctx->root) {
                    Widget* h = hit_test(ctx, e->x, e->y);
                    for (int i = 0; i < ctx->state.count; i++) {  // release all pressed flags
                        if (ctx->state.entries[i].pressed) {
                            ctx->state.entries[i].pressed = false;
                            if (!tab_drag_consumed &&
                                h && h->id == ctx->state.entries[i].id) {
                                fire_click(ctx, h);               // click only if released on same widget
                            }
                        }
                    }
                    for (int i = 0; i < ctx->pool_used; i++) {    // clear pool press flags
                        ctx->pool[i].pressed = false;
                        if (ctx->pool[i].kind == WK_TABS) {
                            ctx->pool[i].tab_press_idx   = -1;
                            ctx->pool[i].tab_press_close = false;
                            ctx->pool[i].tab_add_press   = false;
                        }
                    }
                    ctx->redraw_needed = true;
                }
            }
            break;
        case UW_EV_SCROLL: {                                      // wheel scrolled
            // any scroll dismisses a shown tooltip
            if (ctx->tooltip_shown_id) {
                ctx->tooltip_shown_id = 0;
                ctx->full_repaint     = true;                     // erase the stale box
                ctx->redraw_needed    = true;
            }
            if (ctx->hovered_id) {
                for (int i = 0; i < ctx->state.count; i++)
                    if (ctx->state.entries[i].id == ctx->hovered_id) {
                        ctx->state.entries[i].tooltip_hover_start = apex_now_seconds();
                        break;
                    }
            }

            if (ctx->root) {
                Widget* h = hit_test(ctx, e->x, e->y);
                if (h && h->kind == WK_TEXT_AREA) {
                    WidgetState* s = ui_state_get(ctx, h->id);
                    ta_line_cache_ensure(s, h->input_value.ptr, h->input_value.len);
                    int line_count = s->line_count;
                    if (line_count < 1) line_count = 1;
                    int content_h = line_count * UI_LINE_H;
                    int inner_h   = (int)h->h - 12;
                    int max_scroll = content_h - inner_h;
                    if (max_scroll > 0) {                     // only eat the wheel if it scrolls
                        s->scroll_target += e->scroll_dy * UI_LINE_H;
                        if (s->scroll_target < 0)          s->scroll_target = 0;
                        if (s->scroll_target > max_scroll) s->scroll_target = max_scroll;
                        ctx->redraw_needed = true;
                        break;
                    }
                }
                if (h && h->kind == WK_NUMBER_INPUT) {
                    ui_number_step(ctx, h, -e->scroll_dy);   // wheel up increments, wheel down decrements
                    ctx->focused_id = h->id;
                    for (int i = 0; i < ctx->pool_used; i++)
                        ctx->pool[i].focused = (ctx->pool[i].id == ctx->focused_id);
                    ctx->redraw_needed = true;
                    break;
                }
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
            if (e->key == UI_KEY_ESCAPE && ui_context_menu_handle_escape(ctx))
                break;                                            // context menu ate escape
            if (e->key == UI_KEY_ESCAPE && ui_menubar_handle_escape(ctx))
                break;                                            // menu bar ate escape
            if (ctx->focused_id && ctx->root) {                   // deliver to focused widget
                for (int i = 0; i < ctx->pool_used; i++) {
                    Widget* w = &ctx->pool[i];
                    if (w->id != ctx->focused_id) continue;       // skip non-focused

                    if (w->kind == WK_NUMBER_INPUT) {
                        if (e->key == UI_KEY_UP) { ui_number_step(ctx, w, 1.0); break; }
                        if (e->key == UI_KEY_DOWN) { ui_number_step(ctx, w, -1.0); break; }
                    }
                    if (w->kind == WK_INPUT || w->kind == WK_NUMBER_INPUT ||
                        w->kind == WK_TEXT_AREA) {                  // input field editing
                        WidgetState* s = ui_state_get(ctx, w->id);
                        int len = (int)w->input_value.len;
                        // the cursor can be one frame ahead of the buffer
                        int cur = s->cursor < 0 ? len : s->cursor;
                        if (cur > len) cur = len;
                        if (s->sel_anchor < 0 || s->sel_anchor > len) s->sel_anchor = cur;

                        // text_area: Enter inserts a newline
                        if (w->kind == WK_TEXT_AREA &&
                            e->key == UI_KEY_ENTER && !(e->mods & 2)) {
                            input_push_undo(s, w->input_value.ptr, len, cur);
                            int need = len + 1;
                            char* nb = (char*)malloc((size_t)need + 1);
                            memcpy(nb, w->input_value.ptr, cur);
                            nb[cur] = '\n';
                            memcpy(nb + cur + 1, w->input_value.ptr + cur, len - cur);
                            nb[need] = 0;
                            input_set_buffer(ctx, w, nb, need);
                            s = ui_state_get(ctx, w->id);
                            s->cursor = cur + 1;
                            s->sel_anchor = s->cursor;
                            push_input_change(ctx, w, nb);
                            ctx->redraw_needed = true;
                            break;
                        }

                        // text_area: Up/Down move by line
                        if (w->kind == WK_TEXT_AREA &&
                            (e->key == UI_KEY_UP || e->key == UI_KEY_DOWN) &&
                            !(e->mods & 2)) {
                            text_area_move_vertical(ctx, w, s,
                                e->key == UI_KEY_DOWN ? 1 : -1,
                                (e->mods & 1) != 0);
                            ctx->redraw_needed = true;
                            break;
                        }

                        // text_area: Page Up/Down move the caret by a full viewport
                        if (w->kind == WK_TEXT_AREA &&
                            (e->key == UI_KEY_PAGE_UP || e->key == UI_KEY_PAGE_DOWN)) {
                            int inner_h = (int)w->h - 12;
                            if (inner_h < UI_LINE_H) inner_h = UI_LINE_H;
                            int page_lines = inner_h / UI_LINE_H;
                            if (page_lines < 1) page_lines = 1;
                            int dir = (e->key == UI_KEY_PAGE_DOWN) ? 1 : -1;
                            for (int k = 0; k < page_lines; k++)
                                text_area_move_vertical(ctx, w, s, dir,
                                                        (e->mods & 1) != 0);
                            text_area_ensure_visible(ctx, w);      // scroll to follow
                            ctx->redraw_needed = true;
                            break;
                        }

                        // text_area: Home/End operate on the current line (Ctrl = buffer)
                        if (w->kind == WK_TEXT_AREA &&
                            (e->key == UI_KEY_HOME || e->key == UI_KEY_END)) {
                            int line_start = 0;
                            int line_end = len;
                            if (!(e->mods & 2)) {
                                for (int i = 0; i < cur; i++)
                                    if (w->input_value.ptr[i] == '\n') line_start = i + 1;
                                line_end = line_start;
                                while (line_end < len && w->input_value.ptr[line_end] != '\n')
                                    line_end++;
                            }
                            if (e->key == UI_KEY_HOME) {
                                s->cursor = line_start;
                                if (!(e->mods & 1)) s->sel_anchor = line_start;
                            } else {
                                s->cursor = line_end;
                                if (!(e->mods & 1)) s->sel_anchor = line_end;
                            }
                            ctx->redraw_needed = true;
                            break;
                        }

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
                                    if (w->kind == WK_NUMBER_INPUT) {
                                        int a = cur, b = cur;
                                        if (input_has_selection(s)) input_sel_range(s, &a, &b);
                                        int new_len = len - (b - a) + tlen;
                                        char* tmp = (char*)malloc((size_t)new_len + 1);
                                        if (tmp && new_len >= 0) {
                                            memcpy(tmp, w->input_value.ptr, a);
                                            memcpy(tmp + a, text, tlen);
                                            memcpy(tmp + a + tlen, w->input_value.ptr + b, len - b);
                                            tmp[new_len] = 0;
                                            if (!ui_number_is_valid_text(tmp, new_len, w->num_min, w->num_max)) {
                                                free(tmp);
                                                free(text);
                                                break; // reject the paste entirely
                                            }
                                            free(tmp);
                                        }
                                    }
                                    input_push_undo(s, w->input_value.ptr, len, cur);

                                    if (input_has_selection(s)) {  // drop the selection first
                                        int a, b;
                                        input_sel_range(s, &a, &b);
                                        input_delete_range(ctx, w, s, a, b);
                                        s = ui_state_get(ctx, w->id); // re-fetch
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
                                    s = ui_state_get(ctx, w->id); // re-fetch
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
                            bool has_printable = false;
                            for (int ci = 0; ci < e->utf8_len; ci++) {
                                unsigned char c = (unsigned char)e->utf8[ci];
                                if (c >= 0x20 && c != 0x7F) { has_printable = true; break; }
                            }
                            if (!has_printable) break;            // control-only: drop it

                            if (w->kind == WK_NUMBER_INPUT) {
                                int a = cur, b = cur;
                                if (input_has_selection(s)) input_sel_range(s, &a, &b);
                                int check_len = len - (b - a) + e->utf8_len;
                                char tmp[256];
                                if (check_len >= 0 && check_len < 256) {
                                    memcpy(tmp, w->input_value.ptr, a);
                                    memcpy(tmp + a, e->utf8, e->utf8_len);
                                    memcpy(tmp + a + e->utf8_len, w->input_value.ptr + b, len - b);
                                    if (!ui_number_is_valid_text(tmp, check_len, w->num_min, w->num_max)) {
                                        break; // reject the keypress completely
                                    }
                                }
                            }
                            input_push_undo(s, w->input_value.ptr, len, cur);
                            if (input_has_selection(s)) {
                                int a, b;
                                input_sel_range(s, &a, &b);
                                input_delete_range(ctx, w, s, a, b);
                                s = ui_state_get(ctx, w->id); // re-fetch
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
                            s = ui_state_get(ctx, w->id); // re-fetch
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
                    if ((w->kind == WK_CHECKBOX || w->kind == WK_SWITCH) &&
                        (e->key == UI_KEY_SPACE || e->key == UI_KEY_ENTER)) {   // space or enter toggles
                        fire_click(ctx, w);
                        break;
                    }
                }
            }

            // keep the focused text_area's caret visible after any edit
            if (ctx->focused_id && ctx->root) {
                for (int i = 0; i < ctx->pool_used; i++) {
                    if (ctx->pool[i].id == ctx->focused_id &&
                        ctx->pool[i].kind == WK_TEXT_AREA) {
                        text_area_ensure_visible(ctx, &ctx->pool[i]);
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
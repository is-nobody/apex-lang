// source/libraries/ui/widgets/ui_context_menu.c
// Context menu lifecycle, rendering and input for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_menu.h"
#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_render.h"
#include "ui_window.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>

// resolves the visible top-left of the current menu, clamped to the window
static void cm_panel_rect(UiContext* ctx, UiContextMenu* cm,
                          int* out_x, int* out_y, int* out_w, int* out_h) {
    int w;
    int h = menu_measure_panel(ctx, cm->items, cm->item_count, &w);
    int x = cm->px;
    int y = cm->py;
    if (x + w > ctx->win_w - 4) x = ctx->win_w - 4 - w;
    if (y + h > ctx->win_h - 4) y = ctx->win_h - 4 - h;
    if (x < 4) x = 4;
    if (y < 4) y = 4;
    *out_x = x; *out_y = y; *out_w = w; *out_h = h;
}

// opens a context menu from a descriptor at (px, py)
void ui_context_menu_set(UiContext* ctx, Value items_descr, int px, int py) {
    ui_context_menu_close(ctx);                              // drop any existing menu
    if (!IS_TABLE(items_descr)) return;
    if (ctx->dialog.active) return;                          // never over a modal

    Table* t = AS_TABLE(items_descr);
    int n = t->array_part ? t->array_count : 0;
    if (n <= 0) return;

    UiContextMenu* cm = &ctx->context_menu;
    cm->items = (UiMenuItem*)calloc((size_t)n, sizeof(UiMenuItem));
    if (!cm->items) return;
    cm->item_cap = n;

    for (int i = 0; i < n; i++) {
        Value child = t->array_part[i];
        if (IS_NONE(child)) break;
        if (menu_item_parse(ctx->vm, child, &cm->items[cm->item_count]))
            cm->item_count++;
    }
    if (cm->item_count == 0) {
        free(cm->items);
        cm->items = NULL;
        cm->item_cap = 0;
        return;
    }

    cm->active     = true;
    cm->px         = px;
    cm->py         = py;
    cm->hover_idx  = -1;
    menu_path_reset(&cm->path, 0);
    cm->anim_t     = 0.0;
    cm->open_started_at = apex_now_seconds();

    if (ui_menubar_has_open(ctx)) ui_menubar_handle_escape(ctx);  // exclusive with menubar

    ctx->redraw_needed = true;
    ctx->full_repaint  = true;
}

// closes any open context menu
void ui_context_menu_close(UiContext* ctx) {
    UiContextMenu* cm = &ctx->context_menu;
    if (!cm->active) return;
    menu_items_free(cm->items, cm->item_count);
    cm->items = NULL;
    cm->item_count = 0;
    cm->item_cap = 0;
    cm->active = false;
    cm->hover_idx = -1;
    menu_path_reset(&cm->path, 0);
    ctx->redraw_needed = true;
    ctx->full_repaint  = true;
}

// releases every allocation and zeroes the menu (shutdown path)
void ui_context_menu_free(UiContext* ctx) {
    menu_items_free(ctx->context_menu.items, ctx->context_menu.item_count);
    memset(&ctx->context_menu, 0, sizeof(UiContextMenu));
    menu_path_reset(&ctx->context_menu.path, 0);
}

// true when a context menu is currently on screen
bool ui_context_menu_has_open(UiContext* ctx) {
    return ctx->context_menu.active;
}

// paints the context menu and its open submenu chain
void ui_context_menu_render(UiContext* ctx) {
    UiContextMenu* cm = &ctx->context_menu;
    if (!cm->active) return;

    int px, py, pw, ph;
    cm_panel_rect(ctx, cm, &px, &py, &pw, &ph);

    int hover     = menu_find_hover_item(ctx, cm->items, cm->item_count,
                                         px, py, pw, ph, ctx->input.x, ctx->input.y);
    int next_open = (cm->path.len > 0) ? cm->path.idx[0] : -1;

    menu_draw_dropdown_panel(ctx, cm->items, cm->item_count,
                             px, py, pw, ph, hover, next_open);

    if (next_open >= 0 && next_open < cm->item_count &&
        cm->items[next_open].submenu_count > 0)
        menu_draw_submenu_chain(ctx, &cm->path, 1, cm->items, next_open,
                                px, py, pw, ctx->input.x, ctx->input.y);
}

// handles a pointer move while the menu is open; always consumes
void ui_context_menu_handle_mousemove(UiContext* ctx, int x, int y) {
    UiContextMenu* cm = &ctx->context_menu;
    if (!cm->active) return;

    int px, py, pw, ph;
    cm_panel_rect(ctx, cm, &px, &py, &pw, &ph);

    int  old_hover    = cm->hover_idx;
    int  old_path_len = cm->path.len;
    int  old_path[8]; memcpy(old_path, cm->path.idx, sizeof(old_path));

    cm->hover_idx = menu_find_hover_item(ctx, cm->items, cm->item_count,
                                         px, py, pw, ph, x, y);
    menu_update_path_recursive(ctx, &cm->path, 0, cm->items, cm->item_count,
                               px, py, pw, ph, x, y);

    bool changed = (old_hover != cm->hover_idx) ||
                   (old_path_len != cm->path.len);
    if (!changed) {
        for (int i = 0; i < 8; i++)
            if (old_path[i] != cm->path.idx[i]) { changed = true; break; }
    }
    if (changed) {
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;
    }
}

// handles a pointer press while the menu is open; always consumes
bool ui_context_menu_handle_mousedown(UiContext* ctx, int x, int y) {
    UiContextMenu* cm = &ctx->context_menu;
    if (!cm->active) return false;

    int px, py, pw, ph;
    cm_panel_rect(ctx, cm, &px, &py, &pw, &ph);

    // click inside the top-level panel?
    if (x >= px && x < px + pw && y >= py && y < py + ph) {
        int h = menu_find_hover_item(ctx, cm->items, cm->item_count,
                                     px, py, pw, ph, x, y);
        if (h >= 0) {
            if (cm->items[h].submenu_count > 0) {
                cm->path.idx[0] = h;
                cm->path.len    = 1;
                for (int i = 1; i < 8; i++) cm->path.idx[i] = -1;
            } else {
                menu_fire_item_event(ctx, &cm->items[h]);
                ui_context_menu_close(ctx);
            }
        }
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;
        return true;
    }

    // click inside an open submenu?
    int r = menu_click_submenu_chain(ctx, &cm->path, 0,
                                     cm->items, cm->item_count,
                                     px, py, pw, x, y);
    if (r >= 0) {
        if (r == 1) ui_context_menu_close(ctx);
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;
        return true;
    }

    // click outside: dismiss and consume so the click doesn't reach the widget below
    ui_context_menu_close(ctx);
    return true;
}

// handles escape while a menu is open; returns true when consumed
bool ui_context_menu_handle_escape(UiContext* ctx) {
    if (!ctx->context_menu.active) return false;
    ui_context_menu_close(ctx);
    return true;
}
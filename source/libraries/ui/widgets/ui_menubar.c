// source/libraries/ui/widgets/ui_menubar.c
// Menu bar widget for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_menu.h"
#include "ui_core.h"
#include "ui_render.h"
#include "ui_text.h"
#include "ui_table.h"
#include <stdlib.h>
#include <string.h>

// one top-level menu in the bar
typedef struct {
    char*       label;                        // heap-owned caption
    char*       key;                          // heap-owned script key
    UiMenuItem* items;                        // heap-owned row array
    int         item_count;                   // live rows
    int         item_cap;                     // allocated capacity
} UiMenuTop;

// sentinel for open_menu / hover_menu
#define MENU_NONE      (-1)                   // nothing open or hovered
#define MENU_OVERFLOW  (-2)                   // the "..." chip is open or hovered

// full state of the menu bar
struct UiMenuBar {
    UiMenuTop* menus;                         // top-level entries
    int        menu_count;                    // live entries
    int        menu_cap;                      // allocated capacity

    int open_menu;                            // open top menu, MENU_NONE, or MENU_OVERFLOW
    int hover_menu;                           // hovered top menu, MENU_NONE, or MENU_OVERFLOW
    UiMenuPath path;                          // open submenu chain

    int mx, my, mw, mh;                       // top bar rect in window space
    int menu_x[32];                           // per-top x offset from mx
    int menu_w[32];                           // per-top measured width

    // overflow: top menus that no longer fit collapse behind a "..." chip
    UiMenuItem* overflow_items;               // one row per top menu; submenu borrowed
    int         overflow_x;                   // x offset of the chip, from mx
    int         overflow_w;                   // chip width
    bool        overflow_visible;             // true when at least one top menu is collapsed
    int         first_visible;                // number of top menus rendered inline
};

// replace the context's menu bar with a new one built from descr
void ui_menubar_set(UiContext* ctx, Value descr) {
    ui_menubar_free(ctx);                                       // drop any previous bar
    if (!IS_TABLE(descr)) return;                               // no descriptor: done
    VM* vm = ctx->vm;
    Table* t = AS_TABLE(descr);

    UiMenuBar* mb = (UiMenuBar*)calloc(1, sizeof(UiMenuBar));
    if (!mb) return;                                            // oom: leave the context empty
    mb->open_menu  = -1;                                        // no dropdown expanded
    mb->hover_menu = -1;                                        // nothing hovered
    menu_path_reset(&mb->path, 0);

    int n = t->array_part ? t->array_count : 0;
    for (int i = 0; i < n; i++) {
        if (mb->menu_count >= 32) break;                        // hard cap on top-level menus
        Value menu_v = t->array_part[i];
        if (IS_NONE(menu_v)) break;
        if (!IS_TABLE(menu_v)) continue;
        Table* mt = AS_TABLE(menu_v);

        if (mb->menu_count >= mb->menu_cap) {                   // grow the top-level array
            mb->menu_cap = mb->menu_cap ? mb->menu_cap * 2 : 8;
            mb->menus = (UiMenuTop*)realloc(mb->menus,
                            sizeof(UiMenuTop) * mb->menu_cap);
        }
        UiMenuTop* top = &mb->menus[mb->menu_count++];
        memset(top, 0, sizeof(*top));
        top->label = table_get_str_dup(vm, mt, "label");
        top->key   = table_get_str_dup(vm, mt, "key");

        Value items_v;
        Value k_items = ui_intern(vm, "items");
        if (table_get(mt, k_items, &items_v)) {
            if (IS_TABLE(items_v)) {
                Table* it = AS_TABLE(items_v);
                int m = it->array_part ? it->array_count : 0;
                for (int j = 0; j < m; j++) {
                    Value child = it->array_part[j];
                    if (IS_NONE(child)) break;
                    if (top->item_count >= top->item_cap) {
                        top->item_cap = top->item_cap ? top->item_cap * 2 : 4;
                        top->items = (UiMenuItem*)realloc(top->items,
                                        sizeof(UiMenuItem) * top->item_cap);
                    }
                    if (menu_item_parse(vm, child, &top->items[top->item_count]))
                        top->item_count++;
                }
            }
            value_decref(items_v);
        }
        value_decref(k_items);
    }

    // overflow alias: one row per top menu, whose submenu borrows that menu's items array
    if (mb->menu_count > 0) {
        mb->overflow_items = (UiMenuItem*)calloc((size_t)mb->menu_count, sizeof(UiMenuItem));
        if (mb->overflow_items) {
            for (int i = 0; i < mb->menu_count; i++) {
                UiMenuItem* row = &mb->overflow_items[i];
                row->kind             = MI_ITEM;
                row->label            = mb->menus[i].label
                                      ? strdup(mb->menus[i].label)
                                      : NULL;
                row->key              = NULL;                   // the parent's key carries through
                row->shortcut         = NULL;                   // top-level menus carry no hint
                row->submenu          = mb->menus[i].items;     // borrowed
                row->submenu_count    = mb->menus[i].item_count;
                row->submenu_cap      = mb->menus[i].item_count;
                row->borrowed_submenu = true;
            }
        }
    }
    mb->first_visible    = mb->menu_count;                      // layout will tighten this
    mb->overflow_visible = false;

    ctx->menubar        = mb;                                   // publish the new bar
    ctx->geometry_dirty = true;                                 // tree area shrank
    ctx->redraw_needed  = true;                                 // schedule a repaint
    ctx->full_repaint   = true;                                 // no prior pixels for the bar
}

// release every owned allocation of one top-level menu
static void menutop_free(UiMenuTop* m) {
    free(m->label);
    free(m->key);
    menu_items_free(m->items, m->item_count);
    memset(m, 0, sizeof(*m));
}

// release the menu bar and null the context pointer
void ui_menubar_free(UiContext* ctx) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb) return;                                            // nothing to free
    for (int i = 0; i < mb->menu_count; i++) menutop_free(&mb->menus[i]);
    free(mb->menus);

    menu_items_free(mb->overflow_items, mb->menu_count);        // borrowed submenus are safe

    free(mb);
    ctx->menubar = NULL;                                        // clear the context slot
}

// height reserved for the bar; zero, the bar lives in the titlebar now
int ui_menubar_height(UiContext* ctx) {
    (void)ctx;
    return 0;
}

// total horizontal space the bar would like, for reserving room in the titlebar
int ui_menubar_preferred_width(UiContext* ctx) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb || mb->menu_count == 0) return 0;
    int total = 0;
    for (int i = 0; i < mb->menu_count; i++) {
        int lw = menu_label_width(ctx, mb->menus[i].label);
        total += lw + UI_MENU_BAR_ITEM_PAD * 2;
        if (i > 0) total += UI_MENU_BAR_GAP;
    }
    return total;
}

// true while a dropdown is expanded
bool ui_menubar_has_open(UiContext* ctx) {
    return ctx->menubar && ctx->menubar->open_menu >= 0;
}

// resolve the per-top x offsets, widths, and overflow state
static void layout_menubar(UiContext* ctx, UiMenuBar* mb) {
    int start_x = UI_MENU_BAR_PAD;                              // lead-in from the window edge
    if (start_x < 0) start_x = 0;

    // right edge of the space the bar may occupy, given the title's extent
    int avail_right = ctx->win_w - UI_MENU_BAR_PAD;
    if (ctx->title_end_x > 0 && ctx->title_end_x < avail_right)
        avail_right = ctx->title_end_x;
    int avail = avail_right - start_x;
    if (avail < 0) avail = 0;

    mb->mx = start_x;
    mb->my = 0;                                                 // inside the titlebar
    mb->mw = ctx->win_w - start_x;
    mb->mh = UI_TITLEBAR_H;

    // full width of every top label plus the gaps between them
    int total = 0;
    for (int i = 0; i < mb->menu_count; i++) {
        int lw = menu_label_width(ctx, mb->menus[i].label);
        total += lw + UI_MENU_BAR_ITEM_PAD * 2;
        if (i > 0) total += UI_MENU_BAR_GAP;
    }

    // will the whole strip fit?
    bool overflow_needed = (mb->menu_count > 1) && (total > avail);
    int ovw = 0;                                                // width of the "..." chip + its lead-in
    if (overflow_needed) {
        ovw = UI_MENU_BAR_GAP
            + menu_label_width(ctx, "...")
            + UI_MENU_BAR_ITEM_PAD * 2;
    }

    int cx   = start_x;                                         // running x cursor
    int used = 0;                                               // running accumulated width
    int fit  = 0;                                               // number of inline labels
    bool did_overflow = false;

    for (int i = 0; i < mb->menu_count; i++) {
        int lw  = menu_label_width(ctx, mb->menus[i].label);
        int iw  = lw + UI_MENU_BAR_ITEM_PAD * 2;
        int add = iw + (i > 0 ? UI_MENU_BAR_GAP : 0);
        // reserve room for the chip if any menu would still follow this one
        int reserve = (i + 1 < mb->menu_count) ? ovw : 0;
        if (used + add + reserve > avail) {
            did_overflow = true;
            break;
        }
        mb->menu_x[i] = cx - start_x;
        mb->menu_w[i] = iw;
        cx   += add;
        used += add;
        fit++;
    }

    mb->first_visible    = fit;
    mb->overflow_visible = did_overflow && (fit < mb->menu_count);

    if (mb->overflow_visible) {                                 // place the "..." chip
        cx += UI_MENU_BAR_GAP;
        mb->overflow_x = cx - start_x;
        mb->overflow_w = menu_label_width(ctx, "...") + UI_MENU_BAR_ITEM_PAD * 2;
    } else {
        mb->overflow_x = 0;
        mb->overflow_w = 0;
    }

    // if the previously-interacted menu fell into the collapsed range, close it
    if (mb->open_menu != MENU_NONE) {
        bool open_gone = (mb->open_menu == MENU_OVERFLOW && !mb->overflow_visible) ||
                         (mb->open_menu >= 0 && mb->open_menu >= mb->first_visible);
        if (open_gone) {
            mb->open_menu = MENU_NONE;
            menu_path_reset(&mb->path, 0);
        }
    }
    if (mb->hover_menu == MENU_OVERFLOW && !mb->overflow_visible)
        mb->hover_menu = MENU_NONE;
    else if (mb->hover_menu >= 0 && mb->hover_menu >= mb->first_visible)
        mb->hover_menu = MENU_NONE;
}

// measure a top-level dropdown; returns its height and fills its rect
static int measure_dropdown(UiContext* ctx, UiMenuBar* mb, int mi,
                            int* out_x, int* out_y, int* out_w) {
    *out_x = mb->mx + mb->menu_x[mi];
    *out_y = mb->my + mb->mh;                                   // directly under the label
    UiMenuItem* items = mb->menus[mi].items;
    int count = mb->menus[mi].item_count;
    int max_w = 0;
    int h = UI_MENU_DROP_PAD * 2;                               // top and bottom padding
    for (int i = 0; i < count; i++) {
        if (items[i].kind == MI_SEPARATOR) { h += UI_MENU_SEP_H; continue; }
        int w = menu_item_row_width(ctx, &items[i]);
        if (w > max_w) max_w = w;
        h += UI_MENU_ITEM_H;
    }
    *out_w = max_w + UI_MENU_ITEM_PAD * 2;
    if (*out_w < UI_MENU_DROP_W_MIN) *out_w = UI_MENU_DROP_W_MIN;

    // clamp to the window: shift left when the right edge would overflow
    if (*out_x + *out_w > ctx->win_w - 4)
        *out_x = ctx->win_w - 4 - *out_w;
    if (*out_x < 4) *out_x = 4;
    if (*out_y + h > ctx->win_h - 4) {
        *out_y = ctx->win_h - 4 - h;
        if (*out_y < 4) *out_y = 4;
    }
    return h;
}

// measure the overflow panel anchored to the "..." chip
static int measure_overflow_panel(UiContext* ctx, UiMenuBar* mb,
                                  int* out_x, int* out_y, int* out_w) {
    UiMenuItem* items = mb->overflow_items + mb->first_visible;
    int count = mb->menu_count - mb->first_visible;
    *out_x = mb->mx + mb->overflow_x;
    *out_y = mb->my + mb->mh;
    int max_w = 0;
    int h = UI_MENU_DROP_PAD * 2;
    for (int i = 0; i < count; i++) {
        if (items[i].kind == MI_SEPARATOR) { h += UI_MENU_SEP_H; continue; }
        int w = menu_item_row_width(ctx, &items[i]);
        if (w > max_w) max_w = w;
        h += UI_MENU_ITEM_H;
    }
    *out_w = max_w + UI_MENU_ITEM_PAD * 2;
    if (*out_w < UI_MENU_DROP_W_MIN) *out_w = UI_MENU_DROP_W_MIN;
    if (*out_x + *out_w > ctx->win_w - 4)
        *out_x = ctx->win_w - 4 - *out_w;
    if (*out_x < 4) *out_x = 4;
    if (*out_y + h > ctx->win_h - 4) {                          // clamp to the bottom
        *out_y = ctx->win_h - 4 - h;
        if (*out_y < 4) *out_y = 4;
    }
    return h;
}

// paint the menu bar inside the titlebar and any open dropdowns below it
void ui_menubar_render(UiContext* ctx) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb) return;                                            // nothing to draw

    layout_menubar(ctx, mb);                                    // refresh per-label rects

    // the titlebar already painted the black strip; just overlay the labels
    for (int i = 0; i < mb->first_visible; i++) {               // only labels that fit
        int x = mb->mx + mb->menu_x[i];
        int w = mb->menu_w[i];
        bool hot = (i == mb->hover_menu) || (i == mb->open_menu);
        if (hot)
            ui_painter_fill_round(x + 1, 5, w - 2, UI_TITLEBAR_H - 10,
                                  UI_MENU_ITEM_RADIUS, UI_C_MENU_HOVER_BG);

        const char* lbl = mb->menus[i].label ? mb->menus[i].label : "";
        int lw = menu_label_width(ctx, lbl);
        float tx   = x + (w - lw) * 0.5f;
        float base = (UI_TITLEBAR_H + UI_FS_BODY * 0.7f) * 0.5f;
        ui_draw_text_at(ctx, tx, base, lbl, (int)strlen(lbl), UI_FS_BODY,
                        UI_C_MENUBAR_FG);
    }

    if (mb->overflow_visible) {                                 // the "..." chip
        int x = mb->mx + mb->overflow_x;
        int w = mb->overflow_w;
        bool hot = (mb->hover_menu == MENU_OVERFLOW) ||
                   (mb->open_menu  == MENU_OVERFLOW);
        if (hot)
            ui_painter_fill_round(x + 1, 5, w - 2, UI_TITLEBAR_H - 10,
                                  UI_MENU_ITEM_RADIUS, UI_C_MENU_HOVER_BG);
        const char* lbl = "...";
        int lw = menu_label_width(ctx, lbl);
        float tx   = x + (w - lw) * 0.5f;
        float base = (UI_TITLEBAR_H + UI_FS_BODY * 0.7f) * 0.5f;
        ui_draw_text_at(ctx, tx, base, lbl, 3, UI_FS_BODY, UI_C_MENUBAR_FG);
    }

    if (mb->open_menu == MENU_OVERFLOW && mb->overflow_visible) {
        int dx, dy, dw;
        int dh = measure_overflow_panel(ctx, mb, &dx, &dy, &dw);
        UiMenuItem* items = mb->overflow_items + mb->first_visible;
        int count = mb->menu_count - mb->first_visible;
        int hover = menu_find_hover_item(ctx, items, count, dx, dy, dw, dh,
                                         ctx->input.x, ctx->input.y);
        int next_open = (mb->path.len > 0) ? mb->path.idx[0] : -1;

        menu_draw_dropdown_panel(ctx, items, count, dx, dy, dw, dh, hover, next_open);

        if (next_open >= 0 && next_open < count && items[next_open].submenu_count > 0)
            menu_draw_submenu_chain(ctx, &mb->path, 1, items, next_open,
                                    dx, dy, dw, ctx->input.x, ctx->input.y);
        return;
    }

    if (mb->open_menu < 0 || mb->open_menu >= mb->menu_count) return;
    int mi = mb->open_menu;
    int dx, dy, dw;
    int dh = measure_dropdown(ctx, mb, mi, &dx, &dy, &dw);

    int hover     = menu_find_hover_item(ctx, mb->menus[mi].items, mb->menus[mi].item_count,
                                         dx, dy, dw, dh, ctx->input.x, ctx->input.y);
    int next_open = (mb->path.len > 0) ? mb->path.idx[0] : -1;

    menu_draw_dropdown_panel(ctx, mb->menus[mi].items, mb->menus[mi].item_count,
                             dx, dy, dw, dh, hover, next_open);

    if (next_open >= 0 && next_open < mb->menus[mi].item_count &&
        mb->menus[mi].items[next_open].submenu_count > 0)
        menu_draw_submenu_chain(ctx, &mb->path, 1, mb->menus[mi].items, next_open,
                                dx, dy, dw, ctx->input.x, ctx->input.y);
}

// handle a pointer move; returns true when the bar or an open dropdown absorbed it
bool ui_menubar_handle_mousemove(UiContext* ctx, int x, int y) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb) return false;

    layout_menubar(ctx, mb);                                    // refresh overflow state

    int  old_hover    = mb->hover_menu;
    int  old_open     = mb->open_menu;
    int  old_path[8]; memcpy(old_path, mb->path.idx, sizeof(old_path));
    int  old_path_len = mb->path.len;

    bool on_top_bar = (y >= mb->my && y < mb->my + mb->mh);
    bool on_content = false;                                    // over a label or the chip

    if (on_top_bar) {                                           // pointer on the top row
        int h = MENU_NONE;
        for (int i = 0; i < mb->first_visible; i++) {
            int mx = mb->mx + mb->menu_x[i];
            if (x >= mx && x < mx + mb->menu_w[i]) { h = i; break; }
        }
        if (h == MENU_NONE && mb->overflow_visible) {           // the "..." chip
            int ox = mb->mx + mb->overflow_x;
            if (x >= ox && x < ox + mb->overflow_w) h = MENU_OVERFLOW;
        }
        mb->hover_menu = h;
        on_content = (h != MENU_NONE);
        if (h != MENU_NONE && mb->open_menu != MENU_NONE && h != mb->open_menu) {
            mb->open_menu = h;                                  // switch to the hovered menu
            menu_path_reset(&mb->path, 0);
        }
    } else {
        mb->hover_menu = MENU_NONE;                             // off the top row
    }

    if (mb->open_menu == MENU_OVERFLOW && mb->overflow_visible) {
        int dx, dy, dw;
        int dh = measure_overflow_panel(ctx, mb, &dx, &dy, &dw);
        UiMenuItem* items = mb->overflow_items + mb->first_visible;
        int count = mb->menu_count - mb->first_visible;
        menu_update_path_recursive(ctx, &mb->path, 0, items, count, dx, dy, dw, dh, x, y);
    } else if (mb->open_menu >= 0 && mb->open_menu < mb->menu_count) {
        int mi = mb->open_menu;
        int dx, dy, dw;
        int dh = measure_dropdown(ctx, mb, mi, &dx, &dy, &dw);
        menu_update_path_recursive(ctx, &mb->path, 0, mb->menus[mi].items,
                                   mb->menus[mi].item_count, dx, dy, dw, dh, x, y);
    }

    bool changed = (old_hover != mb->hover_menu) ||
                   (old_open  != mb->open_menu)  ||
                   (old_path_len != mb->path.len);
    if (!changed) {
        for (int i = 0; i < 8; i++)
            if (old_path[i] != mb->path.idx[i]) { changed = true; break; }
    }
    if (changed) {
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;                              // dropdowns overlap the tree
    }

    // absorb the hover when the pointer is over the bar's content
    return on_content || mb->open_menu != MENU_NONE;
}

// handle a pointer press; returns true when the bar consumed the click
bool ui_menubar_handle_mousedown(UiContext* ctx, int x, int y) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb) return false;

    layout_menubar(ctx, mb);                                    // refresh overflow state

    if (y >= mb->my && y < mb->my + mb->mh) {                   // click on the top row
        int h = MENU_NONE;
        for (int i = 0; i < mb->first_visible; i++) {
            int mx = mb->mx + mb->menu_x[i];
            if (x >= mx && x < mx + mb->menu_w[i]) { h = i; break; }
        }
        if (h == MENU_NONE && mb->overflow_visible) {           // the "..." chip
            int ox = mb->mx + mb->overflow_x;
            if (x >= ox && x < ox + mb->overflow_w) h = MENU_OVERFLOW;
        }
        if (h != MENU_NONE) {                                   // toggle the clicked target
            mb->open_menu = (mb->open_menu == h) ? MENU_NONE : h;
            menu_path_reset(&mb->path, 0);
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;
            return true;
        }
        return false;                                           // click on the bar, not a label
    }

    if (mb->open_menu == MENU_OVERFLOW && mb->overflow_visible) {
        int dx, dy, dw;
        int dh = measure_overflow_panel(ctx, mb, &dx, &dy, &dw);
        UiMenuItem* items = mb->overflow_items + mb->first_visible;
        int count = mb->menu_count - mb->first_visible;
        if (x >= dx && x < dx + dw && y >= dy && y < dy + dh) {
            int h = menu_find_hover_item(ctx, items, count, dx, dy, dw, dh, x, y);
            if (h >= 0) {
                if (items[h].submenu_count > 0) {               // open the submenu
                    mb->path.idx[0] = h;
                    if (mb->path.len < 1) mb->path.len = 1;
                    for (int i = 1; i < 8; i++) mb->path.idx[i] = -1;
                } else {                                        // leaf: emit and close
                    menu_fire_item_event(ctx, &items[h]);
                    mb->open_menu = MENU_NONE;
                    menu_path_reset(&mb->path, 0);
                }
            }
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;
            return true;
        }
        int r_ov = menu_click_submenu_chain(ctx, &mb->path, 0, items, count, dx, dy, dw, x, y);
        if (r_ov >= 0) {
            if (r_ov == 1) { mb->open_menu = MENU_NONE; menu_path_reset(&mb->path, 0); }
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;
            return true;
        }
    } else if (mb->open_menu >= 0 && mb->open_menu < mb->menu_count) {
        int mi = mb->open_menu;
        int dx, dy, dw;
        int dh = measure_dropdown(ctx, mb, mi, &dx, &dy, &dw);
        if (x >= dx && x < dx + dw && y >= dy && y < dy + dh) {
            int h = menu_find_hover_item(ctx, mb->menus[mi].items,
                                         mb->menus[mi].item_count,
                                         dx, dy, dw, dh, x, y);
            if (h >= 0) {
                if (mb->menus[mi].items[h].submenu_count > 0) { // open the submenu
                    mb->path.idx[0] = h;
                    if (mb->path.len < 1) mb->path.len = 1;
                    for (int i = 1; i < 8; i++) mb->path.idx[i] = -1;
                } else {                                        // leaf: emit and close
                    menu_fire_item_event(ctx, &mb->menus[mi].items[h]);
                    mb->open_menu = MENU_NONE;
                    menu_path_reset(&mb->path, 0);
                }
            }
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;
            return true;
        }
        int r_dd = menu_click_submenu_chain(ctx, &mb->path, 0, mb->menus[mi].items,
                                            mb->menus[mi].item_count, dx, dy, dw, x, y);
        if (r_dd >= 0) {
            if (r_dd == 1) { mb->open_menu = MENU_NONE; menu_path_reset(&mb->path, 0); }
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;
            return true;
        }
    }

    if (mb->open_menu != MENU_NONE) {                           // click outside: close
        mb->open_menu = MENU_NONE;
        menu_path_reset(&mb->path, 0);
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;
    }
    return false;                                               // do not consume
}

// handle escape while a dropdown is open
bool ui_menubar_handle_escape(UiContext* ctx) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb || mb->open_menu < 0) return false;                 // nothing to close
    mb->open_menu = -1;
    menu_path_reset(&mb->path, 0);
    ctx->redraw_needed = true;
    ctx->full_repaint  = true;
    return true;
}
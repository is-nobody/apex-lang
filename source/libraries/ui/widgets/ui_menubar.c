// source/libraries/ui/widgets/ui_menubar.c
// Menu bar widget for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_render.h"
#include "ui_text.h"
#include "ui_table.h"
#include <stdlib.h>
#include <string.h>

// menu item kinds: a normal row or a horizontal rule
typedef enum { MI_ITEM = 0, MI_SEPARATOR = 1 } UiMenuItemKind;

// one row in a dropdown, optionally carrying a submenu
typedef struct UiMenuItem {
    UiMenuItemKind kind;                      // item vs separator
    char*          label;                     // heap-owned, null for separators
    char*          key;                       // heap-owned, null for separators
    struct UiMenuItem* submenu;               // null when this row is a leaf
    int            submenu_count;             // live entries in submenu
    int            submenu_cap;               // allocated capacity of submenu
    bool           borrowed_submenu;          // submenu is aliased; free() must not recurse
} UiMenuItem;

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
    int path[8];                              // submenu path per depth, -1 = closed
    int path_len;                             // number of live slots in path

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

// measure a label width in body font size
static int label_width(UiContext* ctx, const char* label) {
    if (!label || !*label) return 0;                              // empty label
    return (int)ui_text_measure_cached(ctx, label,
                                       (int)strlen(label), UI_FS_BODY);
}

// clear path slots from keep onward
static void reset_path(UiMenuBar* mb, int keep) {
    mb->path_len = keep;                      // keep the live prefix
    for (int i = keep; i < 8; i++)            // and clear the tail
        mb->path[i] = -1;
}

// parse one row descriptor into a UiMenuItem, recursing into submenus
static bool mb_parse_item(VM* vm, Value item_v, UiMenuItem* out) {
    if (!IS_TABLE(item_v)) return false;      // descriptors must be tables
    Table* t = AS_TABLE(item_v);

    bool is_sep = table_get_bool(vm, t, "separator", false);
    memset(out, 0, sizeof(*out));             // zero the row

    if (is_sep) { out->kind = MI_SEPARATOR; return true; }

    out->kind  = MI_ITEM;
    out->label = table_get_str_dup(vm, t, "label");
    out->key   = table_get_str_dup(vm, t, "key");

    Value items_v;
    Value k_items = ui_intern(vm, "items");
    if (table_get(t, k_items, &items_v)) {
        if (IS_TABLE(items_v)) {
            Table* it = AS_TABLE(items_v);
            int n = it->array_part ? it->array_count : 0;
            for (int i = 0; i < n; i++) {
                Value child = it->array_part[i];
                if (IS_NONE(child)) break;
                if (out->submenu_count >= out->submenu_cap) {
                    out->submenu_cap = out->submenu_cap ? out->submenu_cap * 2 : 4;
                    out->submenu = (UiMenuItem*)realloc(out->submenu,
                                    sizeof(UiMenuItem) * out->submenu_cap);
                }
                if (mb_parse_item(vm, child, &out->submenu[out->submenu_count]))
                    out->submenu_count++;
            }
        }
        value_decref(items_v);
    }
    value_decref(k_items);
    return true;
}

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
    reset_path(mb, 0);

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
                    if (mb_parse_item(vm, child, &top->items[top->item_count]))
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

// release every owned allocation of one row, recursively
static void menuitem_free(UiMenuItem* it) {
    free(it->label);
    free(it->key);
    if (!it->borrowed_submenu) {
        for (int i = 0; i < it->submenu_count; i++) menuitem_free(&it->submenu[i]);
        free(it->submenu);
    }
    memset(it, 0, sizeof(*it));
}

// release every owned allocation of one top-level menu
static void menutop_free(UiMenuTop* m) {
    free(m->label);
    free(m->key);
    for (int i = 0; i < m->item_count; i++) menuitem_free(&m->items[i]);
    free(m->items);
    memset(m, 0, sizeof(*m));
}

// release the menu bar and null the context pointer
void ui_menubar_free(UiContext* ctx) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb) return;                                            // nothing to free
    for (int i = 0; i < mb->menu_count; i++) menutop_free(&mb->menus[i]);
    free(mb->menus);

    if (mb->overflow_items) {                                   // release the alias array
        for (int i = 0; i < mb->menu_count; i++)
            menuitem_free(&mb->overflow_items[i]);              // borrowed submenus are safe
        free(mb->overflow_items);
    }

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
        int lw = label_width(ctx, mb->menus[i].label);
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
        int lw = label_width(ctx, mb->menus[i].label);
        total += lw + UI_MENU_BAR_ITEM_PAD * 2;
        if (i > 0) total += UI_MENU_BAR_GAP;
    }

    // will the whole strip fit?
    bool overflow_needed = (mb->menu_count > 1) && (total > avail);
    int ovw = 0;                                                // width of the "..." chip + its lead-in
    if (overflow_needed) {
        ovw = UI_MENU_BAR_GAP
            + label_width(ctx, "...")
            + UI_MENU_BAR_ITEM_PAD * 2;
    }

    int cx   = start_x;                                         // running x cursor
    int used = 0;                                               // running accumulated width
    int fit  = 0;                                               // number of inline labels
    bool did_overflow = false;

    for (int i = 0; i < mb->menu_count; i++) {
        int lw  = label_width(ctx, mb->menus[i].label);
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
        mb->overflow_w = label_width(ctx, "...") + UI_MENU_BAR_ITEM_PAD * 2;
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
            reset_path(mb, 0);
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
        int w = label_width(ctx, items[i].label);
        if (items[i].submenu_count > 0) w += 20;                // room for the arrow
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
        int w = label_width(ctx, items[i].label);
        if (items[i].submenu_count > 0) w += 20;                // room for the arrow
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

// measure a submenu anchored to item idx of the given list
static int measure_submenu(UiContext* ctx, UiMenuItem* items, int idx,
                           int px, int py, int pw,
                           int* out_x, int* out_y, int* out_w) {
    if (idx < 0 || !items[idx].submenu) {
        *out_x = *out_y = *out_w = 0;
        return 0;
    }
    UiMenuItem* sub = items[idx].submenu;
    int n = items[idx].submenu_count;

    int y_off = UI_MENU_DROP_PAD;                               // top padding of the parent
    for (int i = 0; i < idx; i++)                               // step past the rows above
        y_off += (items[i].kind == MI_SEPARATOR) ? UI_MENU_SEP_H : UI_MENU_ITEM_H;

    // measure the panel first, then decide which side to open on
    int max_w = 0;
    int h = UI_MENU_DROP_PAD * 2;
    for (int i = 0; i < n; i++) {
        if (sub[i].kind == MI_SEPARATOR) { h += UI_MENU_SEP_H; continue; }
        int w = label_width(ctx, sub[i].label);
        if (sub[i].submenu_count > 0) w += 20;
        if (w > max_w) max_w = w;
        h += UI_MENU_ITEM_H;
    }
    int sw = max_w + UI_MENU_ITEM_PAD * 2;
    if (sw < UI_MENU_DROP_W_MIN) sw = UI_MENU_DROP_W_MIN;

    // horizontal placement: right of the parent, unless it would overflow
    int sx = px + pw;
    if (sx + sw > ctx->win_w - 4) {                             // would run off the right edge
        sx = px - sw;                                           // flip to the left side
        if (sx < 4) sx = 4;                                     // clamp against the left edge
    }

    // vertical placement: aligned with the hovered row, clamped to the bottom
    int sy = py + y_off;
    if (sy + h > ctx->win_h - 4) {
        sy = ctx->win_h - 4 - h;
        if (sy < 4) sy = 4;
    }

    *out_x = sx;
    *out_y = sy;
    *out_w = sw;
    return h;
}

// index of the row under the pointer, or -1
static int find_hover_item(UiContext* ctx, UiMenuItem* items, int count,
                           int dx, int dy, int dw, int dh,
                           int mouse_x, int mouse_y) {
    (void)ctx;
    if (mouse_x < dx || mouse_x >= dx + dw) return -1;          // outside on x
    if (mouse_y < dy || mouse_y >= dy + dh) return -1;          // outside on y
    int yy = dy + UI_MENU_DROP_PAD;
    for (int i = 0; i < count; i++) {
        int h = (items[i].kind == MI_SEPARATOR) ? UI_MENU_SEP_H : UI_MENU_ITEM_H;
        if (mouse_y >= yy && mouse_y < yy + h)
            return items[i].kind == MI_SEPARATOR ? -1 : i;      // separators are not hittable
        yy += h;
    }
    return -1;
}

// stamp a small right-pointing arrow for submenu rows
static void draw_arrow_right(int cx, int cy, uint32_t c) {
    for (int k = 0; k < 3; k++) {
        ui_painter_fill_rect(cx + k, cy - 3 + k, 1, 1, c);
        ui_painter_fill_rect(cx + k, cy + 3 - k, 1, 1, c);
    }
}

// paint one dropdown panel with the given hover indices
static void draw_dropdown_panel(UiContext* ctx, UiMenuItem* items, int count,
                                int px, int py, int pw, int ph,
                                int hover_idx, int open_submenu_idx) {
    ui_painter_fill_round(px, py, pw, ph, UI_MENU_DROP_RADIUS, UI_C_MENU_DROP_BG);
    ui_painter_stroke_round(px, py, pw, ph, UI_MENU_DROP_RADIUS, UI_C_MENU_DROP_BORDER);

    int y = py + UI_MENU_DROP_PAD;
    for (int i = 0; i < count; i++) {
        if (items[i].kind == MI_SEPARATOR) {                    // horizontal rule
            int sep_y = y + UI_MENU_SEP_H / 2;
            ui_painter_fill_rect(px + UI_MENU_ITEM_PAD, sep_y,
                                 pw - UI_MENU_ITEM_PAD * 2, 1, UI_C_MENU_SEP);
            y += UI_MENU_SEP_H;
            continue;
        }

        // soft floating highlight, menubar style
        if (i == open_submenu_idx)
            ui_painter_fill_round(px + UI_MENU_ITEM_INSET_X,
                                  y  + UI_MENU_ITEM_INSET_Y,
                                  pw - 2 * UI_MENU_ITEM_INSET_X,
                                  UI_MENU_ITEM_H - 2 * UI_MENU_ITEM_INSET_Y,
                                  UI_MENU_ROW_RADIUS, UI_C_ACCENT);
        else if (i == hover_idx)
            ui_painter_fill_round(px + UI_MENU_ITEM_INSET_X,
                                  y  + UI_MENU_ITEM_INSET_Y,
                                  pw - 2 * UI_MENU_ITEM_INSET_X,
                                  UI_MENU_ITEM_H - 2 * UI_MENU_ITEM_INSET_Y,
                                  UI_MENU_ROW_RADIUS, UI_C_PANEL_ALT);

        if (items[i].label) {
            float tx   = (float)(px + UI_MENU_ITEM_PAD);
            float base = y + (UI_MENU_ITEM_H + UI_FS_BODY * 0.7f) * 0.5f;
            ui_draw_text_at(ctx, tx, base, items[i].label,
                            (int)strlen(items[i].label), UI_FS_BODY,
                            UI_C_MENU_ITEM_FG);
        }

        if (items[i].submenu_count > 0)
            draw_arrow_right(px + pw - UI_MENU_ITEM_PAD - 6,
                             y + UI_MENU_ITEM_H / 2, UI_C_FG_DIM);

        y += UI_MENU_ITEM_H;
    }
}

// recursively paint an open submenu chain below the given parent
static void draw_submenu_chain(UiContext* ctx, UiMenuBar* mb, int depth,
                               UiMenuItem* parent_items, int parent_idx,
                               int parent_x, int parent_y, int parent_w,
                               int mouse_x, int mouse_y) {
    if (depth >= 8 || parent_idx < 0) return;                   // guard against deep chains
    if (!parent_items[parent_idx].submenu) return;

    int sx, sy, sw;
    int sh = measure_submenu(ctx, parent_items, parent_idx,
                             parent_x, parent_y, parent_w,
                             &sx, &sy, &sw);
    UiMenuItem* sub = parent_items[parent_idx].submenu;
    int sub_n = parent_items[parent_idx].submenu_count;

    int hover     = find_hover_item(ctx, sub, sub_n, sx, sy, sw, sh, mouse_x, mouse_y);
    int next_open = (depth < mb->path_len) ? mb->path[depth] : -1;

    draw_dropdown_panel(ctx, sub, sub_n, sx, sy, sw, sh, hover, next_open);

    if (next_open >= 0 && next_open < sub_n && sub[next_open].submenu_count > 0)
        draw_submenu_chain(ctx, mb, depth + 1, sub, next_open, sx, sy, sw,
                           mouse_x, mouse_y);
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
        int lw = label_width(ctx, lbl);
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
        int lw = label_width(ctx, lbl);
        float tx   = x + (w - lw) * 0.5f;
        float base = (UI_TITLEBAR_H + UI_FS_BODY * 0.7f) * 0.5f;
        ui_draw_text_at(ctx, tx, base, lbl, 3, UI_FS_BODY, UI_C_MENUBAR_FG);
    }

    if (mb->open_menu == MENU_OVERFLOW && mb->overflow_visible) {
        int dx, dy, dw;
        int dh = measure_overflow_panel(ctx, mb, &dx, &dy, &dw);
        UiMenuItem* items = mb->overflow_items + mb->first_visible;
        int count = mb->menu_count - mb->first_visible;
        int hover = find_hover_item(ctx, items, count, dx, dy, dw, dh,
                                    ctx->input.x, ctx->input.y);
        int next_open = (mb->path_len > 0) ? mb->path[0] : -1;

        draw_dropdown_panel(ctx, items, count, dx, dy, dw, dh, hover, next_open);

        if (next_open >= 0 && next_open < count && items[next_open].submenu_count > 0)
            draw_submenu_chain(ctx, mb, 1, items, next_open,
                               dx, dy, dw, ctx->input.x, ctx->input.y);
        return;
    }

    if (mb->open_menu < 0 || mb->open_menu >= mb->menu_count) return;
    int mi = mb->open_menu;
    int dx, dy, dw;
    int dh = measure_dropdown(ctx, mb, mi, &dx, &dy, &dw);

    int hover     = find_hover_item(ctx, mb->menus[mi].items, mb->menus[mi].item_count,
                                    dx, dy, dw, dh, ctx->input.x, ctx->input.y);
    int next_open = (mb->path_len > 0) ? mb->path[0] : -1;

    draw_dropdown_panel(ctx, mb->menus[mi].items, mb->menus[mi].item_count,
                        dx, dy, dw, dh, hover, next_open);

    if (next_open >= 0 && next_open < mb->menus[mi].item_count &&
        mb->menus[mi].items[next_open].submenu_count > 0)
        draw_submenu_chain(ctx, mb, 1, mb->menus[mi].items, next_open,
                           dx, dy, dw, ctx->input.x, ctx->input.y);
}

// enqueue a click event for the given leaf item
static void fire_menuitem_event(UiContext* ctx, UiMenuItem* it) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));
    ev.kind = EV_CLICK;
    ev.widget_id = 0;
    strncpy(ev.widget_type, "menuitem", 15);
    if (it->key) strncpy(ev.key_name, it->key, 63);
    ev.mouse_x = ctx->input.x;
    ev.mouse_y = ctx->input.y;
    ui_event_push(ctx, &ev);
}

// walk the open submenu chain and refresh the path from the pointer position
static void update_path_recursive(UiContext* ctx, UiMenuBar* mb, int depth,
                                  UiMenuItem* items, int count,
                                  int dx, int dy, int dw, int dh,
                                  int mouse_x, int mouse_y) {
    if (depth >= 8) return;                                     // depth guard

    int h = find_hover_item(ctx, items, count, dx, dy, dw, dh, mouse_x, mouse_y);

    if (h < 0) {
        // pointer is not over this panel
        if (depth < mb->path_len) {
            int open_idx = mb->path[depth];
            if (open_idx >= 0 && open_idx < count &&
                items[open_idx].submenu_count > 0) {
                int sx, sy, sw;
                int sh = measure_submenu(ctx, items, open_idx, dx, dy, dw,
                                         &sx, &sy, &sw);
                bool in_y    = (mouse_y >= sy && mouse_y < sy + sh);
                bool inside  = (mouse_x >= sx && mouse_x < sx + sw);
                // deeper submenus of a right-opening submenu live further right
                bool past    = (mouse_x >= sx + sw) && (sx >= dx + dw);
                if (in_y && (inside || past)) {
                    update_path_recursive(ctx, mb, depth + 1,
                                          items[open_idx].submenu,
                                          items[open_idx].submenu_count,
                                          sx, sy, sw, sh, mouse_x, mouse_y);
                    return;                                 // keep the parent entry live
                }
            }
        }
        reset_path(mb, depth);                                  // pointer left this panel
        return;
    }

    // record the hovered row at this depth whether it is a leaf or a submenu parent
    mb->path[depth] = h;
    mb->path_len    = depth + 1;
    for (int i = depth + 1; i < 8; i++) mb->path[i] = -1;       // drop deeper path

    if (items[h].submenu_count > 0) {                           // recurse into the submenu
        int sx, sy, sw;
        int sh = measure_submenu(ctx, items, h, dx, dy, dw, &sx, &sy, &sw);
        update_path_recursive(ctx, mb, depth + 1,
                              items[h].submenu, items[h].submenu_count,
                              sx, sy, sw, sh, mouse_x, mouse_y);
    }
}

// handle a pointer move; returns true when the bar or an open dropdown absorbed it
bool ui_menubar_handle_mousemove(UiContext* ctx, int x, int y) {
    UiMenuBar* mb = ctx->menubar;
    if (!mb) return false;

    layout_menubar(ctx, mb);                                    // refresh overflow state

    int  old_hover    = mb->hover_menu;
    int  old_open     = mb->open_menu;
    int  old_path[8]; memcpy(old_path, mb->path, sizeof(old_path));
    int  old_path_len = mb->path_len;

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
            reset_path(mb, 0);
        }
    } else {
        mb->hover_menu = MENU_NONE;                             // off the top row
    }

    if (mb->open_menu == MENU_OVERFLOW && mb->overflow_visible) {
        int dx, dy, dw;
        int dh = measure_overflow_panel(ctx, mb, &dx, &dy, &dw);
        UiMenuItem* items = mb->overflow_items + mb->first_visible;
        int count = mb->menu_count - mb->first_visible;
        update_path_recursive(ctx, mb, 0, items, count, dx, dy, dw, dh, x, y);
    } else if (mb->open_menu >= 0 && mb->open_menu < mb->menu_count) {
        int mi = mb->open_menu;
        int dx, dy, dw;
        int dh = measure_dropdown(ctx, mb, mi, &dx, &dy, &dw);
        update_path_recursive(ctx, mb, 0, mb->menus[mi].items,
                              mb->menus[mi].item_count, dx, dy, dw, dh, x, y);
    }

    bool changed = (old_hover != mb->hover_menu) ||
                   (old_open  != mb->open_menu)  ||
                   (old_path_len != mb->path_len);
    if (!changed) {
        for (int i = 0; i < 8; i++)
            if (old_path[i] != mb->path[i]) { changed = true; break; }
    }
    if (changed) {
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;                              // dropdowns overlap the tree
    }

    // absorb the hover when the pointer is over the bar's content
    return on_content || mb->open_menu != MENU_NONE;
}

// click inside the open submenu chain; returns true if consumed
static bool click_submenu_chain(UiContext* ctx, UiMenuBar* mb, int depth,
                                UiMenuItem* items, int count,
                                int dx, int dy, int dw,
                                int mx, int my) {
    if (depth >= mb->path_len) return false;                    // nothing open at this depth
    int idx = mb->path[depth];
    if (idx < 0 || idx >= count) return false;
    if (!items[idx].submenu) return false;

    int sx, sy, sw;
    int sh = measure_submenu(ctx, items, idx, dx, dy, dw, &sx, &sy, &sw);
    UiMenuItem* sub = items[idx].submenu;
    int sub_n = items[idx].submenu_count;

    if (mx >= sx && mx < sx + sw && my >= sy && my < sy + sh) {
        int h = find_hover_item(ctx, sub, sub_n, sx, sy, sw, sh, mx, my);
        if (h >= 0) {
            if (sub[h].submenu_count > 0) {                     // deeper: open the submenu
                mb->path[depth + 1] = h;
                if (mb->path_len < depth + 2) mb->path_len = depth + 2;
                for (int i = depth + 2; i < 8; i++) mb->path[i] = -1;
            } else {                                            // leaf: emit and close
                fire_menuitem_event(ctx, &sub[h]);
                mb->open_menu = -1;
                reset_path(mb, 0);
            }
        }
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;
        return true;
    }
    return click_submenu_chain(ctx, mb, depth + 1, sub, sub_n, sx, sy, sw, mx, my);
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
            reset_path(mb, 0);
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
            int h = find_hover_item(ctx, items, count, dx, dy, dw, dh, x, y);
            if (h >= 0) {
                if (items[h].submenu_count > 0) {               // open the submenu
                    mb->path[0] = h;
                    if (mb->path_len < 1) mb->path_len = 1;
                    for (int i = 1; i < 8; i++) mb->path[i] = -1;
                } else {                                        // leaf: emit and close
                    fire_menuitem_event(ctx, &items[h]);
                    mb->open_menu = MENU_NONE;
                    reset_path(mb, 0);
                }
            }
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;
            return true;
        }
        if (click_submenu_chain(ctx, mb, 0, items, count, dx, dy, dw, x, y))
            return true;                                        // click landed on a submenu
    } else if (mb->open_menu >= 0 && mb->open_menu < mb->menu_count) {
        int mi = mb->open_menu;
        int dx, dy, dw;
        int dh = measure_dropdown(ctx, mb, mi, &dx, &dy, &dw);
        if (x >= dx && x < dx + dw && y >= dy && y < dy + dh) {
            int h = find_hover_item(ctx, mb->menus[mi].items,
                                    mb->menus[mi].item_count,
                                    dx, dy, dw, dh, x, y);
            if (h >= 0) {
                if (mb->menus[mi].items[h].submenu_count > 0) { // open the submenu
                    mb->path[0] = h;
                    if (mb->path_len < 1) mb->path_len = 1;
                    for (int i = 1; i < 8; i++) mb->path[i] = -1;
                } else {                                        // leaf: emit and close
                    fire_menuitem_event(ctx, &mb->menus[mi].items[h]);
                    mb->open_menu = MENU_NONE;
                    reset_path(mb, 0);
                }
            }
            ctx->redraw_needed = true;
            ctx->full_repaint  = true;
            return true;
        }
        if (click_submenu_chain(ctx, mb, 0, mb->menus[mi].items,
                                mb->menus[mi].item_count, dx, dy, dw, x, y))
            return true;                                        // click landed on a submenu
    }

    if (mb->open_menu != MENU_NONE) {                           // click outside: close
        mb->open_menu = MENU_NONE;
        reset_path(mb, 0);
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
    reset_path(mb, 0);
    ctx->redraw_needed = true;
    ctx->full_repaint  = true;
    return true;
}
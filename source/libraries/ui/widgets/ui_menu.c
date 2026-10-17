// source/libraries/ui/widgets/ui_menu.c
// Shared menu-item primitives for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_menu.h"
#include "ui_core.h"
#include "ui_render.h"
#include "ui_text.h"
#include "ui_table.h"
#include <stdlib.h>
#include <string.h>

// clears idx[keep..8) and sets len = keep
void menu_path_reset(UiMenuPath* p, int keep) {
    p->len = keep;                                          // keep the live prefix
    for (int i = keep; i < 8; i++) p->idx[i] = -1;          // clear the tail
}

// parses a table descriptor into one UiMenuItem, recursing into submenus
bool menu_item_parse(VM* vm, Value item_v, UiMenuItem* out) {
    if (!IS_TABLE(item_v)) return false;                    // descriptors must be tables
    Table* t = AS_TABLE(item_v);

    bool is_sep = table_get_bool(vm, t, "separator", false);
    memset(out, 0, sizeof(*out));                           // zero the row
    if (is_sep) { out->kind = MI_SEPARATOR; return true; }

    out->kind     = MI_ITEM;
    out->label    = table_get_str_dup(vm, t, "label");
    out->key      = table_get_str_dup(vm, t, "key");
    out->shortcut = table_get_str_dup(vm, t, "shortcut");

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
                if (menu_item_parse(vm, child, &out->submenu[out->submenu_count]))
                    out->submenu_count++;
            }
        }
        value_decref(items_v);
    }
    value_decref(k_items);
    return true;
}

// releases every heap string owned by one item, recursing into its submenu
void menu_item_free(UiMenuItem* it) {
    if (!it) return;
    free(it->label);
    free(it->key);
    free(it->shortcut);
    if (!it->borrowed_submenu) {
        for (int i = 0; i < it->submenu_count; i++) menu_item_free(&it->submenu[i]);
        free(it->submenu);
    }
    memset(it, 0, sizeof(*it));
}

// frees an array of items, then the array itself
void menu_items_free(UiMenuItem* items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) menu_item_free(&items[i]);
    free(items);
}

// measured width of a c-string in body font size
int menu_label_width(UiContext* ctx, const char* label) {
    if (!label || !*label) return 0;
    return (int)ui_text_measure_cached(ctx, label,
                                       (int)strlen(label), UI_FS_BODY);
}

// minimum row width: label + shortcut + optional submenu arrow
int menu_item_row_width(UiContext* ctx, UiMenuItem* it) {
    int w = menu_label_width(ctx, it->label);
    if (it->shortcut && *it->shortcut)
        w += UI_MENU_SHORTCUT_GAP + menu_label_width(ctx, it->shortcut);
    if (it->submenu_count > 0)
        w += 20;                                            // room for the right-pointing arrow
    return w;
}

// index of the row under the pointer, or -1
int menu_find_hover_item(UiContext* ctx, UiMenuItem* items, int count,
                         int dx, int dy, int dw, int dh,
                         int mouse_x, int mouse_y) {
    (void)ctx;
    if (mouse_x < dx || mouse_x >= dx + dw) return -1;
    if (mouse_y < dy || mouse_y >= dy + dh) return -1;
    int yy = dy + UI_MENU_DROP_PAD;
    for (int i = 0; i < count; i++) {
        int h = (items[i].kind == MI_SEPARATOR) ? UI_MENU_SEP_H : UI_MENU_ITEM_H;
        if (mouse_y >= yy && mouse_y < yy + h)
            return items[i].kind == MI_SEPARATOR ? -1 : i;
        yy += h;
    }
    return -1;
}

// measures a submenu anchored to item idx of the given list
int menu_measure_submenu(UiContext* ctx, UiMenuItem* items, int idx,
                         int px, int py, int pw,
                         int* out_x, int* out_y, int* out_w) {
    if (idx < 0 || !items[idx].submenu) {
        *out_x = *out_y = *out_w = 0;
        return 0;
    }
    UiMenuItem* sub = items[idx].submenu;
    int n = items[idx].submenu_count;

    int y_off = UI_MENU_DROP_PAD;
    for (int i = 0; i < idx; i++)
        y_off += (items[i].kind == MI_SEPARATOR) ? UI_MENU_SEP_H : UI_MENU_ITEM_H;

    int max_w = 0;
    int h = UI_MENU_DROP_PAD * 2;
    for (int i = 0; i < n; i++) {
        if (sub[i].kind == MI_SEPARATOR) { h += UI_MENU_SEP_H; continue; }
        int w = menu_item_row_width(ctx, &sub[i]);
        if (w > max_w) max_w = w;
        h += UI_MENU_ITEM_H;
    }
    int sw = max_w + UI_MENU_ITEM_PAD * 2;
    if (sw < UI_MENU_DROP_W_MIN) sw = UI_MENU_DROP_W_MIN;

    int sx = px + pw;
    if (sx + sw > ctx->win_w - 4) { sx = px - sw; if (sx < 4) sx = 4; }
    int sy = py + y_off;
    if (sy + h > ctx->win_h - 4) { sy = ctx->win_h - 4 - h; if (sy < 4) sy = 4; }

    *out_x = sx;
    *out_y = sy;
    *out_w = sw;
    return h;
}

// measures a free-floating panel; out_w is written, the height is returned
int menu_measure_panel(UiContext* ctx, UiMenuItem* items, int count, int* out_w) {
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
    return h;
}

// stamps a right-pointing arrow for submenu rows
void menu_draw_arrow_right(int cx, int cy, uint32_t c) {
    for (int k = 0; k < 3; k++) {
        ui_painter_fill_rect(cx + k, cy - 3 + k, 1, 1, c);
        ui_painter_fill_rect(cx + k, cy + 3 - k, 1, 1, c);
    }
}

// paints one dropdown panel with the given hover indices
void menu_draw_dropdown_panel(UiContext* ctx, UiMenuItem* items, int count,
                              int px, int py, int pw, int ph,
                              int hover_idx, int open_submenu_idx) {
    ui_painter_fill_round(px, py, pw, ph, UI_MENU_DROP_RADIUS, UI_C_MENU_DROP_BG);
    ui_painter_stroke_round(px, py, pw, ph, UI_MENU_DROP_RADIUS, UI_C_MENU_DROP_BORDER);

    int y = py + UI_MENU_DROP_PAD;
    for (int i = 0; i < count; i++) {
        if (items[i].kind == MI_SEPARATOR) {
            int sep_y = y + UI_MENU_SEP_H / 2;
            ui_painter_fill_rect(px + UI_MENU_ITEM_PAD, sep_y,
                                 pw - UI_MENU_ITEM_PAD * 2, 1, UI_C_MENU_SEP);
            y += UI_MENU_SEP_H;
            continue;
        }
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

        int right_x = px + pw - UI_MENU_ITEM_PAD;
        if (items[i].submenu_count > 0) {
            menu_draw_arrow_right(right_x - 6, y + UI_MENU_ITEM_H / 2, UI_C_FG_DIM);
            right_x -= 20;
        }
        if (items[i].shortcut && *items[i].shortcut) {
            int   sw   = menu_label_width(ctx, items[i].shortcut);
            float tx   = (float)(right_x - sw);
            float base = y + (UI_MENU_ITEM_H + UI_FS_BODY * 0.7f) * 0.5f;
            ui_draw_text_at(ctx, tx, base, items[i].shortcut,
                            (int)strlen(items[i].shortcut), UI_FS_BODY,
                            UI_C_FG_DIM);
        }
        y += UI_MENU_ITEM_H;
    }
}

// recursively paints an open submenu chain below the given parent
void menu_draw_submenu_chain(UiContext* ctx, UiMenuPath* path, int depth,
                             UiMenuItem* parent_items, int parent_idx,
                             int parent_x, int parent_y, int parent_w,
                             int mouse_x, int mouse_y) {
    if (depth >= 8 || parent_idx < 0) return;
    if (!parent_items[parent_idx].submenu) return;

    int sx, sy, sw;
    int sh = menu_measure_submenu(ctx, parent_items, parent_idx,
                                  parent_x, parent_y, parent_w,
                                  &sx, &sy, &sw);
    UiMenuItem* sub = parent_items[parent_idx].submenu;
    int sub_n = parent_items[parent_idx].submenu_count;

    int hover     = menu_find_hover_item(ctx, sub, sub_n, sx, sy, sw, sh, mouse_x, mouse_y);
    int next_open = (depth < path->len) ? path->idx[depth] : -1;

    menu_draw_dropdown_panel(ctx, sub, sub_n, sx, sy, sw, sh, hover, next_open);

    if (next_open >= 0 && next_open < sub_n && sub[next_open].submenu_count > 0)
        menu_draw_submenu_chain(ctx, path, depth + 1, sub, next_open,
                                sx, sy, sw, mouse_x, mouse_y);
}

// enqueues a click event for the given leaf item
void menu_fire_item_event(UiContext* ctx, UiMenuItem* it) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));
    ev.kind = EV_CLICK;
    ev.widget_id = 0;
    strncpy(ev.widget_type, "menuitem", 15);
    if (it->key) strncpy(ev.key_name, it->key, 63);
    ev.mouse_x = ctx->input.x;
    ev.mouse_y = ctx->input.y;
    ui_event_push(ctx, &ev);
}

// walks the open submenu chain and refreshes the path from the pointer position
void menu_update_path_recursive(UiContext* ctx, UiMenuPath* path, int depth,
                                UiMenuItem* items, int count,
                                int dx, int dy, int dw, int dh,
                                int mouse_x, int mouse_y) {
    if (depth >= 8) return;

    int h = menu_find_hover_item(ctx, items, count, dx, dy, dw, dh, mouse_x, mouse_y);

    if (h < 0) {
        if (depth < path->len) {
            int open_idx = path->idx[depth];
            if (open_idx >= 0 && open_idx < count &&
                items[open_idx].submenu_count > 0) {
                int sx, sy, sw;
                int sh = menu_measure_submenu(ctx, items, open_idx, dx, dy, dw,
                                              &sx, &sy, &sw);
                bool in_y    = (mouse_y >= sy && mouse_y < sy + sh);
                bool inside  = (mouse_x >= sx && mouse_x < sx + sw);
                bool past    = (mouse_x >= sx + sw) && (sx >= dx + dw);
                if (in_y && (inside || past)) {
                    menu_update_path_recursive(ctx, path, depth + 1,
                                               items[open_idx].submenu,
                                               items[open_idx].submenu_count,
                                               sx, sy, sw, sh, mouse_x, mouse_y);
                    return;
                }
            }
        }
        menu_path_reset(path, depth);
        return;
    }

    path->idx[depth] = h;
    path->len        = depth + 1;
    for (int i = depth + 1; i < 8; i++) path->idx[i] = -1;

    if (items[h].submenu_count > 0) {
        int sx, sy, sw;
        int sh = menu_measure_submenu(ctx, items, h, dx, dy, dw, &sx, &sy, &sw);
        menu_update_path_recursive(ctx, path, depth + 1,
                                   items[h].submenu, items[h].submenu_count,
                                   sx, sy, sw, sh, mouse_x, mouse_y);
    }
}

// click inside the open submenu chain.
// returns -1 (not consumed), 0 (submenu opened, menu stays), 1 (leaf clicked)
int menu_click_submenu_chain(UiContext* ctx, UiMenuPath* path, int depth,
                             UiMenuItem* items, int count,
                             int dx, int dy, int dw,
                             int mx, int my) {
    if (depth >= path->len) return -1;
    int idx = path->idx[depth];
    if (idx < 0 || idx >= count) return -1;
    if (!items[idx].submenu) return -1;

    int sx, sy, sw;
    int sh = menu_measure_submenu(ctx, items, idx, dx, dy, dw, &sx, &sy, &sw);
    UiMenuItem* sub = items[idx].submenu;
    int sub_n = items[idx].submenu_count;

    if (mx >= sx && mx < sx + sw && my >= sy && my < sy + sh) {
        int h = menu_find_hover_item(ctx, sub, sub_n, sx, sy, sw, sh, mx, my);
        if (h >= 0) {
            if (sub[h].submenu_count > 0) {
                path->idx[depth + 1] = h;
                if (path->len < depth + 2) path->len = depth + 2;
                for (int i = depth + 2; i < 8; i++) path->idx[i] = -1;
                return 0;                                   // submenu opened
            }
            menu_fire_item_event(ctx, &sub[h]);
            return 1;                                       // leaf clicked: close the parent
        }
        return 0;                                           // click inside but not on a row
    }
    return menu_click_submenu_chain(ctx, path, depth + 1, sub, sub_n,
                                    sx, sy, sw, mx, my);
}
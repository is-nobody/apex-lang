// source/libraries/ui/widgets/ui_menu.h
// Shared menu-item primitives for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_MENU_H
#define UI_MENU_H

#include "ui_internal.h"

// clears idx[keep..8) and sets len = keep
void menu_path_reset(UiMenuPath* p, int keep);

// parses a table descriptor into one UiMenuItem, recursing into submenus
bool menu_item_parse(VM* vm, Value item_v, UiMenuItem* out);

// releases every heap string owned by one item, recursing into its submenu
void menu_item_free(UiMenuItem* it);

// frees an array of items, then the array itself
void menu_items_free(UiMenuItem* items, int count);

// measured width of a c-string in body font size
int  menu_label_width(UiContext* ctx, const char* label);

// minimum row width: label + shortcut + optional submenu arrow
int  menu_item_row_width(UiContext* ctx, UiMenuItem* it);

// index of the row under the pointer, or -1
int  menu_find_hover_item(UiContext* ctx, UiMenuItem* items, int count,
                          int dx, int dy, int dw, int dh,
                          int mouse_x, int mouse_y);

// measures a submenu anchored to item idx of the given list
int  menu_measure_submenu(UiContext* ctx, UiMenuItem* items, int idx,
                          int px, int py, int pw,
                          int* out_x, int* out_y, int* out_w);

// measures a free-floating panel; out_w is written, the height is returned
int  menu_measure_panel(UiContext* ctx, UiMenuItem* items, int count,
                        int* out_w);

// stamps a right-pointing arrow for submenu rows
void menu_draw_arrow_right(int cx, int cy, uint32_t c);

// paints one dropdown panel with the given hover indices
void menu_draw_dropdown_panel(UiContext* ctx, UiMenuItem* items, int count,
                              int px, int py, int pw, int ph,
                              int hover_idx, int open_submenu_idx);

// recursively paints an open submenu chain below the given parent
void menu_draw_submenu_chain(UiContext* ctx, UiMenuPath* path, int depth,
                             UiMenuItem* parent_items, int parent_idx,
                             int parent_x, int parent_y, int parent_w,
                             int mouse_x, int mouse_y);

// enqueues a click event for the given leaf item
void menu_fire_item_event(UiContext* ctx, UiMenuItem* it);

// walks the open submenu chain and refreshes the path from the pointer position
void menu_update_path_recursive(UiContext* ctx, UiMenuPath* path, int depth,
                                UiMenuItem* items, int count,
                                int dx, int dy, int dw, int dh,
                                int mouse_x, int mouse_y);

// click inside the open submenu chain
int  menu_click_submenu_chain(UiContext* ctx, UiMenuPath* path, int depth,
                              UiMenuItem* items, int count,
                              int dx, int dy, int dw,
                              int mx, int my);

#endif
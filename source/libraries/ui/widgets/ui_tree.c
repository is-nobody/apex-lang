// source/libraries/ui/widgets/ui_tree.c
// Tree widget: descriptor flattening, hit testing, click handling, and rendering
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_table.h"
#include "ui_text.h"
#include "ui_render.h"
#include <string.h>
#include <stdlib.h>

// copies n bytes into the per-frame arena, null-terminated
static Str tree_arena_dup(UiContext* ctx, const char* s, int n) {
    char* p = (char*)ui_arena_alloc(ctx, (size_t)n + 1);
    if (n > 0) memcpy(p, s, (size_t)n);
    p[n] = 0;
    Str r; r.ptr = p; r.len = n;
    return r;
}

// counts the visible rows in a node array, honoring each node's expanded flag
static int tree_count_visible(UiContext* ctx, Table* arr) {
    int n = arr->array_part ? arr->array_count : 0;
    int total = 0;
    for (int i = 0; i < n; i++) {
        Value nv = arr->array_part[i];
        if (!IS_TABLE(nv)) continue;
        Table* nt = AS_TABLE(nv);
        total++;
        if (!table_get_bool_key(nt, ctx->key_cache[UK_EXPANDED], false)) continue;
        Value cv = MAKE_NONE();
        if (!table_get(nt, ctx->key_cache[UK_NODES], &cv)) continue;
        if (IS_TABLE(cv)) total += tree_count_visible(ctx, AS_TABLE(cv));
        value_decref(cv);
    }
    return total;
}

// walks a node array depth-first, filling w->tree_nodes and only descending
// into nodes whose expanded flag is set
static void tree_fill_nodes(UiContext* ctx, Widget* w, Table* arr,
                            int depth, int* cursor) {
    int n = arr->array_part ? arr->array_count : 0;
    for (int i = 0; i < n; i++) {
        Value nv = arr->array_part[i];
        if (!IS_TABLE(nv)) continue;
        Table* nt = AS_TABLE(nv);

        StrView lv, kv;
        Str label = {NULL, 0}, key = {NULL, 0};
        if (table_get_str_view(nt, ctx->key_cache[UK_LABEL], &lv))
            label = tree_arena_dup(ctx, lv.ptr, lv.len);
        if (table_get_str_view(nt, ctx->key_cache[UK_KEY], &kv))
            key = tree_arena_dup(ctx, kv.ptr, kv.len);

        bool expanded = table_get_bool_key(nt, ctx->key_cache[UK_EXPANDED], false);

        Value cv = MAKE_NONE();
        Table* ct = NULL;
        bool has_children = false;
        if (table_get(nt, ctx->key_cache[UK_NODES], &cv) && IS_TABLE(cv)) {
            ct = AS_TABLE(cv);
            has_children = (ct->array_part && ct->array_count > 0);
        }

        TreeNode* node = &w->tree_nodes[(*cursor)++];
        node->label        = label;
        node->key          = key;
        node->depth        = depth;
        node->has_children = has_children;
        node->expanded     = expanded;

        if (expanded && ct)
            tree_fill_nodes(ctx, w, ct, depth + 1, cursor);

        value_decref(cv);
    }
}

// flattens a node descriptor array into w->tree_nodes
void tree_build_nodes(UiContext* ctx, Widget* w, Table* arr) {
    int total = tree_count_visible(ctx, arr);
    if (total <= 0) {
        w->tree_nodes = NULL;
        w->tree_node_count = 0;
        return;
    }
    w->tree_nodes = (TreeNode*)ui_arena_alloc(ctx, sizeof(TreeNode) * (size_t)total);
    memset(w->tree_nodes, 0, sizeof(TreeNode) * (size_t)total);
    w->tree_node_count = total;
    int cursor = 0;
    tree_fill_nodes(ctx, w, arr, 0, &cursor);
}

// row index for a local y offset, or -1 when outside the row range
static int tree_row_at(Widget* w, int local_y) {
    if (local_y < 1) return -1;                            // top 1px is the border
    int idx = (local_y - 1) / UI_TREE_ROW_H;
    if (idx < 0 || idx >= w->tree_node_count) return -1;
    return idx;
}

// draws the small disclosure triangle at the row's indent slot
static void tree_draw_disclosure(int x, int y, bool expanded, uint32_t c) {
    if (expanded) {                                    // ▼
        ui_painter_fill_rect(x,     y - 1, 7, 1, c);
        ui_painter_fill_rect(x + 1, y,     5, 1, c);
        ui_painter_fill_rect(x + 2, y + 1, 3, 1, c);
        ui_painter_fill_rect(x + 3, y + 2, 1, 1, c);
    } else {                                           // ▶
        ui_painter_fill_rect(x,     y - 3, 1, 7, c);
        ui_painter_fill_rect(x + 1, y - 2, 1, 5, c);
        ui_painter_fill_rect(x + 2, y - 1, 1, 3, c);
        ui_painter_fill_rect(x + 3, y,     1, 1, c);
    }
}

// emits the click or change event for a press on the tree widget
void tree_handle_press(UiContext* ctx, Widget* w) {
    int ly = ctx->input.y - (int)w->y;
    int row = tree_row_at(w, ly);
    if (row < 0) return;

    TreeNode* n = &w->tree_nodes[row];

    UiEvent ev; memset(&ev, 0, sizeof(ev));
    ev.widget_id = w->id;
    ev.mouse_x = ctx->input.x;
    ev.mouse_y = ctx->input.y;
    strncpy(ev.widget_type, "tree", 15);
    if (n->key.ptr && n->key.len > 0) {
        int copy = n->key.len < 63 ? n->key.len : 63;
        memcpy(ev.key_name, n->key.ptr, (size_t)copy);
        ev.key_name[copy] = 0;
    }

    if (n->has_children) {
        ev.kind       = EV_CHANGE;                       // whole row toggles for parents
        ev.bool_value = !n->expanded;
    } else {
        ev.kind = EV_CLICK;                              // leaves just get selected
    }

    ui_event_push(ctx, &ev);
    ctx->redraw_needed = true;
}

// draws the entire tree into its own rectangle
void tree_render(UiContext* ctx, Widget* w, int x, int y, int ww, int hh) {
    if (ww <= 0 || hh <= 0) return;

    ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_PANEL);     // body
    ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);  // outline
    if (w->tree_node_count == 0) return;                            // nothing to draw

    // hover row is tracked in widget state by the mousemove handler
    int hover_row = w->hovered ? w->hover_row : -1;
    int press_row = w->pressed
                  ? tree_row_at(w, ctx->input.y - y)
                  : -1;

    int rx = x + 1;                                                 // inside the 1px border
    int rw = ww - 2;

    for (int i = 0; i < w->tree_node_count; i++) {
        TreeNode* n = &w->tree_nodes[i];
        int ry = y + 1 + i * UI_TREE_ROW_H;

        // row highlight, in priority order: pressed, selected, hovered
        bool selected = (w->tree_selected.len > 0) &&
                        (n->key.len == w->tree_selected.len) &&
                        (memcmp(n->key.ptr, w->tree_selected.ptr,
                                (size_t)n->key.len) == 0);
        uint32_t bg = 0;
        if      (i == press_row) bg = UI_C_ACCENT_A;
        else if (selected)       bg = UI_C_ACCENT;
        else if (i == hover_row) bg = UI_C_PANEL_ALT;
        if (bg) ui_painter_fill_rect(rx, ry, rw, UI_TREE_ROW_H, bg);

        // hairline between rows
        if (i > 0)
            ui_painter_fill_rect(rx, ry, rw, 1, UI_C_BORDER);

        // indentation, then the optional disclosure triangle
        int indent_px = UI_TREE_INNER_PAD + n->depth * w->tree_indent;
        if (n->has_children)
            tree_draw_disclosure(rx + indent_px + 3, ry + UI_TREE_ROW_H / 2,
                                 n->expanded, UI_C_FG_DIM);

        // label, starting right of the disclosure slot
        float lx   = (float)(rx + indent_px + UI_TREE_DISC_W);
        float base = ry + (UI_TREE_ROW_H + UI_FS_BODY * 0.7f) * 0.5f;
        if (n->label.ptr && n->label.len > 0)
            ui_draw_text_at(ctx, lx, base, n->label.ptr, n->label.len,
                            UI_FS_BODY, UI_C_FG);
    }
}
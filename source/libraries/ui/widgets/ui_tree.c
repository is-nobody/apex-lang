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
    if (local_y < UI_TREE_PAD) return -1;              // above the first row
    int idx = (local_y - UI_TREE_PAD) / UI_TREE_ROW_H;
    if (idx < 0 || idx >= w->tree_node_count) return -1;
    return idx;
}

// draws a sharp, pixel-perfect 1px chevron without anti-aliasing
static void tree_draw_disclosure(int x, int y, bool expanded, uint32_t c) {
    if (expanded) { // ▼
        ui_painter_fill_rect(x - 3, y - 1, 1, 1, c);
        ui_painter_fill_rect(x - 2, y,     1, 1, c);
        ui_painter_fill_rect(x - 1, y + 1, 1, 1, c);
        ui_painter_fill_rect(x,     y + 2, 1, 1, c);
        ui_painter_fill_rect(x + 1, y + 1, 1, 1, c);
        ui_painter_fill_rect(x + 2, y,     1, 1, c);
        ui_painter_fill_rect(x + 3, y - 1, 1, 1, c);
    } else {        // ▶
        ui_painter_fill_rect(x - 2, y - 3, 1, 1, c);
        ui_painter_fill_rect(x - 1, y - 2, 1, 1, c);
        ui_painter_fill_rect(x,     y - 1, 1, 1, c);
        ui_painter_fill_rect(x + 1, y,     1, 1, c);
        ui_painter_fill_rect(x,     y + 1, 1, 1, c);
        ui_painter_fill_rect(x - 1, y + 2, 1, 1, c);
        ui_painter_fill_rect(x - 2, y + 3, 1, 1, c);
    }
}

// emits the click, right-click or change event for a press on the tree widget
void tree_handle_press(UiContext* ctx, Widget* w, int button) {
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

    if (button == 3) {
        ev.kind = EV_RIGHT_CLICK;                        // never toggle on right-click
    } else if (n->has_children) {
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

    // panel background with a subtle border
    ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_PANEL);
    ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);
    if (w->tree_node_count == 0) return;                            // nothing to draw

    int pad     = UI_TREE_PAD;                                       // inner padding of the row area
    int inner_x = x + pad;                                           // left edge of the row area
    int inner_w = ww - pad * 2;                                      // row area width
    int inner_y = y + pad;                                           // top edge of the first row

    int hover_row = w->hovered ? w->hover_row : -1;                  // hovered row, -1 for none
    int press_row = w->pressed ? tree_row_at(w, ctx->input.y - y) : -1;  // pressed row, -1 for none

    // guides pass: draw every indent guide first so row highlights paint over them
    for (int i = 0; i < w->tree_node_count; i++) {
        TreeNode* n = &w->tree_nodes[i];
        int ry = inner_y + i * UI_TREE_ROW_H;
        for (int d = 0; d < n->depth; d++) {                         // one guide per ancestor depth
            int gx = inner_x + UI_TREE_INNER_PAD + d * w->tree_indent + 4;  // align with chevron center
            int gy = ry;
            int gh = UI_TREE_ROW_H;
            // first child of a parent: extend the guide up to reach the parent's disclosure
            if (i > 0 && w->tree_nodes[i-1].depth <= d) {
                gy -= UI_TREE_ROW_H / 2;
                gh += UI_TREE_ROW_H / 2;
            }
            ui_painter_fill_rect(gx, gy, 1, gh, UI_C_TREE_GUIDE);    // 1px vertical guide
        }
    }

    // highlight + content pass
    for (int i = 0; i < w->tree_node_count; i++) {
        TreeNode* n = &w->tree_nodes[i];
        int ry = inner_y + i * UI_TREE_ROW_H;

        int indent_x = inner_x + UI_TREE_INNER_PAD + n->depth * w->tree_indent;

        // rounded selection / hover / press highlight, inset from the row edges
        bool selected = (w->tree_selected.len > 0) &&
                        (n->key.len == w->tree_selected.len) &&
                        (memcmp(n->key.ptr, w->tree_selected.ptr, (size_t)n->key.len) == 0);
        uint32_t bg = 0;
        if      (i == press_row) bg = UI_C_TREE_PRESS_BG;            // pressed: strongest fill
        else if (selected)       bg = UI_C_TREE_SEL_BG;              // selected: persistent highlight
        else if (i == hover_row) bg = UI_C_TREE_HOVER_BG;            // hovered: subtle wash

        if (bg) {
            // move the selection box further right for parents so it doesn't touch the arrow
            int hl_x = indent_x + (n->has_children ? UI_TREE_DISC_W - 2 : 2);
            int hl_w = inner_x + inner_w - 2 - hl_x;
            if (hl_w > 0)
                ui_painter_fill_round(hl_x, ry + 1, hl_w,
                                      UI_TREE_ROW_H - 2, UI_TREE_SEL_RADIUS, bg);
        }

        // disclosure triangle at the row's indent slot
        if (n->has_children)
            tree_draw_disclosure(indent_x + 4, ry + UI_TREE_ROW_H / 2,
                                 n->expanded, UI_C_TREE_CHEVRON);

        // label starting right of the disclosure slot
        float lx   = (float)(indent_x + UI_TREE_DISC_W);
        float base = ry + (UI_TREE_ROW_H + UI_FS_BODY * 0.7f) * 0.5f;
        
        if (n->label.ptr && n->label.len > 0 && ctx->font) {
            uint32_t fg = selected ? UI_C_TREE_SEL_FG : UI_C_FG;      // brighter text when selected
            int label_max_w = (inner_x + inner_w - 2) - (int)lx;     // available width before the right edge
            
            if (label_max_w > 0) {
                float full_w = ui_text_measure_cached(ctx, n->label.ptr, n->label.len, UI_FS_BODY);
                
                if (full_w <= (float)label_max_w + 0.5f) {
                    // whole label fits
                    ui_draw_text_at(ctx, lx, base, n->label.ptr, n->label.len, UI_FS_BODY, fg);
                } else {
                    // truncate with "..." like Chrome
                    const char ell[] = "...";
                    float ell_w = ui_text_measure_cached(ctx, ell, 3, UI_FS_BODY);
                    
                    if ((float)label_max_w > ell_w) {
                        float budget = (float)label_max_w - ell_w;   // space left for the prefix
                        int cut = 0;                                 // byte length of the kept prefix
                        float cut_w = 0.0f;                          // measured width of that prefix
                        int j = 0;                                   // byte cursor
                        
                        while (j < n->label.len) {                   // grow the prefix one codepoint at a time
                            int next = ui_utf8_next(n->label.ptr, j, n->label.len);
                            float w2 = ui_text_measure_cached(ctx, n->label.ptr, next, UI_FS_BODY);
                            if (w2 > budget) break;                  // this codepoint would overflow
                            j = next;
                            cut = j;
                            cut_w = w2;
                        }
                        
                        if (cut > 0) {                               // some prefix survived
                            ui_draw_text_at(ctx, lx, base, n->label.ptr, cut, UI_FS_BODY, fg);
                            ui_draw_text_at(ctx, lx + cut_w, base, ell, 3, UI_FS_BODY, fg);
                        } else if ((float)label_max_w >= ell_w) {    // first char too wide, but ellipsis fits
                            ui_draw_text_at(ctx, lx, base, ell, 3, UI_FS_BODY, fg);
                        }
                    }
                }
            }
        }
    }
}
// source/libraries/ui/widgets/ui_table.c
// Table column layout helpers for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_text.h"
#include <string.h>

// computes the natural (content-hugging) width of every column into out[]
void table_natural_widths(UiContext* ctx, Widget* w, float* out) {
    int nc = w->table_col_count;                                // number of columns
    int nr = w->table_row_count;                                // number of body rows
    for (int c = 0; c < nc; c++) {
        TableColumn* col = &w->table_cols[c];                   // this column's metadata
        if (col->width > 0) {                                   // fixed width: use it as-is
            out[c] = col->width;
            continue;
        }
        float natural = UI_TABLE_CELL_PAD * 2;                  // minimum: padding on both sides
        if (w->table_header && col->label.ptr && col->label.len > 0) {   // header label width
            float lw = ui_text_measure_cached(ctx, col->label.ptr, col->label.len, UI_FS_BODY);
            if (lw + UI_TABLE_CELL_PAD * 2 > natural)
                natural = lw + UI_TABLE_CELL_PAD * 2;           // widest so far is the header
        }
        for (int r = 0; r < nr; r++) {                          // widest cell in this column
            Str cell = w->table_cells[(size_t)r * (size_t)nc + (size_t)c];
            if (cell.ptr && cell.len > 0) {
                float cw = ui_text_measure_cached(ctx, cell.ptr, cell.len, UI_FS_BODY);
                if (cw + UI_TABLE_CELL_PAD * 2 > natural)
                    natural = cw + UI_TABLE_CELL_PAD * 2;
            }
        }
        out[c] = natural;                                       // publish the natural width
    }
}

// resolves the final per-column widths so the table fits into avail pixels
void table_resolve_widths(UiContext* ctx, Widget* w, float avail, float* out) {
    int nc = w->table_col_count;
    if (nc <= 0) return;                                        // nothing to resolve

    table_natural_widths(ctx, w, out);                          // start from natural widths

    float fixed_sum = 0;                                        // total width of fixed columns
    float auto_sum  = 0;                                        // total width of auto columns
    int   auto_n    = 0;                                        // number of auto columns
    for (int c = 0; c < nc; c++) {
        if (w->table_cols[c].width > 0) fixed_sum += out[c];
        else { auto_sum += out[c]; auto_n++; }
    }
    if (auto_n == 0) return;                                    // all fixed: nothing to spread

    float target = avail - 2.0f;                                // inner width after borders
    if (target <= fixed_sum) {                                  // too tight: shrink auto columns
        float budget = target - fixed_sum;
        float floor_b = (float)auto_n * 40.0f;                  // never shrink below 40px each
        if (budget < floor_b) budget = floor_b;
        float scale = auto_sum > 0.0f ? budget / auto_sum : 1.0f;
        for (int c = 0; c < nc; c++)
            if (w->table_cols[c].width <= 0) out[c] *= scale;
    } else {                                                    // spare room: spread evenly
        float extra = target - fixed_sum - auto_sum;
        if (extra > 0.0f) {
            float add = extra / (float)auto_n;
            for (int c = 0; c < nc; c++)
                if (w->table_cols[c].width <= 0) out[c] += add;
        }
    }
}
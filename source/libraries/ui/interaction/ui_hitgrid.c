// source/libraries/ui/interaction/ui_hitgrid.c
// Uniform-grid hit testing for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_interaction.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// grid cell size in pixels. 64 keeps typical widget bboxes confined to 1-2 cells.
#define HIT_CELL_SIZE 64

// releases every allocation owned by ctx->hit_grid
void hit_grid_free(UiContext* ctx) {
    free(ctx->hit_grid.cell_start);
    free(ctx->hit_grid.cell_items);
    memset(&ctx->hit_grid, 0, sizeof(ctx->hit_grid));
}

// first pass: count how many widgets land in each cell, and count cells-per-widget along the way
static void hit_grid_count(Widget* w, int cx0, int cy0, int cx1, int cy1,
                           int* counts, int cols) {
    if (!w || !w->visible) return;

    int wx0 = (int)floorf(w->x + 0.5f);
    int wy0 = (int)floorf(w->y + 0.5f);
    int wx1 = (int)floorf(w->x + w->w + 0.5f);
    int wy1 = (int)floorf(w->y + w->h + 0.5f);
    if (wx0 < cx0) wx0 = cx0;
    if (wy0 < cy0) wy0 = cy0;
    if (wx1 > cx1) wx1 = cx1;
    if (wy1 > cy1) wy1 = cy1;

    int ncx0 = cx0, ncy0 = cy0, ncx1 = cx1, ncy1 = cy1;
    if (wx1 > wx0 && wy1 > wy0) {                         // visible portion exists
        int c0 = wx0 / HIT_CELL_SIZE;
        int r0 = wy0 / HIT_CELL_SIZE;
        int c1 = (wx1 - 1) / HIT_CELL_SIZE;
        int r1 = (wy1 - 1) / HIT_CELL_SIZE;
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++)
                counts[r * cols + c]++;

        if (w->kind == WK_SCROLL) {                       // tighten clip for the subtree
            if (wx0 > ncx0) ncx0 = wx0;
            if (wy0 > ncy0) ncy0 = wy0;
            if (wx1 < ncx1) ncx1 = wx1;
            if (wy1 < ncy1) ncy1 = wy1;
        }
    }

    for (int i = 0; i < w->child_count; i++)              // draw order: normal children first
        if (!w->children[i]->on_top)
            hit_grid_count(w->children[i], ncx0, ncy0, ncx1, ncy1, counts, cols);
    for (int i = 0; i < w->child_count; i++)              // ...then overlay children
        if (w->children[i]->on_top)
            hit_grid_count(w->children[i], ncx0, ncy0, ncx1, ncy1, counts, cols);
}

// second pass: fill cell_items in the same order the first pass counted
static void hit_grid_fill(Widget* w, int cx0, int cy0, int cx1, int cy1,
                          int* cell_cursor, Widget** items, int cols) {
    if (!w || !w->visible) return;

    int wx0 = (int)floorf(w->x + 0.5f);
    int wy0 = (int)floorf(w->y + 0.5f);
    int wx1 = (int)floorf(w->x + w->w + 0.5f);
    int wy1 = (int)floorf(w->y + w->h + 0.5f);
    if (wx0 < cx0) wx0 = cx0;
    if (wy0 < cy0) wy0 = cy0;
    if (wx1 > cx1) wx1 = cx1;
    if (wy1 > cy1) wy1 = cy1;

    int ncx0 = cx0, ncy0 = cy0, ncx1 = cx1, ncy1 = cy1;
    if (wx1 > wx0 && wy1 > wy0) {
        int c0 = wx0 / HIT_CELL_SIZE;
        int r0 = wy0 / HIT_CELL_SIZE;
        int c1 = (wx1 - 1) / HIT_CELL_SIZE;
        int r1 = (wy1 - 1) / HIT_CELL_SIZE;
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++) {
                int cell = r * cols + c;
                items[cell_cursor[cell]++] = w;
            }

        if (w->kind == WK_SCROLL) {
            if (wx0 > ncx0) ncx0 = wx0;
            if (wy0 > ncy0) ncy0 = wy0;
            if (wx1 < ncx1) ncx1 = wx1;
            if (wy1 < ncy1) ncy1 = wy1;
        }
    }

    for (int i = 0; i < w->child_count; i++)
        if (!w->children[i]->on_top)
            hit_grid_fill(w->children[i], ncx0, ncy0, ncx1, ncy1, cell_cursor, items, cols);
    for (int i = 0; i < w->child_count; i++)
        if (w->children[i]->on_top)
            hit_grid_fill(w->children[i], ncx0, ncy0, ncx1, ncy1, cell_cursor, items, cols);
}

// builds ctx->hit_grid from the laid-out tree. called by ui_layout after arranging.
void hit_grid_build(UiContext* ctx, Widget* root) {
    HitGrid* g = &ctx->hit_grid;
    int ww = ctx->win_w > 0 ? ctx->win_w : 1;
    int wh = ctx->win_h > 0 ? ctx->win_h : 1;

    // early out if grid shape is unchanged: reuse the existing cells
    int cols = (ww + HIT_CELL_SIZE - 1) / HIT_CELL_SIZE;
    int rows = (wh + HIT_CELL_SIZE - 1) / HIT_CELL_SIZE;
    int ncells = cols * rows;

    if (g->cols != cols || g->rows != rows || !g->cell_start) {
        free(g->cell_start);
        free(g->cell_items);
        g->cell_start = (int*)calloc((size_t)ncells + 1, sizeof(int));
        g->cell_items = NULL;
        g->cols = cols;
        g->rows = rows;
        g->win_w = ww;
        g->win_h = wh;
    } else {
        memset(g->cell_start, 0, sizeof(int) * ((size_t)ncells + 1));
        g->win_w = ww;
        g->win_h = wh;
    }
    if (!g->cell_start) return;                           // allocation failed: leave the flag clear
    ctx->hit_grid_valid = true;                           // grid memory is live and shaped for this window
    if (!root) return;                                    // empty tree: nothing to fill in

    // pass 1: per-cell counts
    hit_grid_count(root, 0, 0, ww, wh, g->cell_start, cols);

    // prefix sum: cell_start[i] = start of cell i in cell_items
    int total = 0;
    for (int i = 0; i < ncells; i++) {
        int n = g->cell_start[i];
        g->cell_start[i] = total;
        total += n;
    }
    g->cell_start[ncells] = total;

    // allocate the flat item array (reuse the old buffer if big enough)
    g->cell_items = (Widget**)realloc(g->cell_items, sizeof(Widget*) * (size_t)(total ? total : 1));

    // pass 2: fill in draw order using a per-cell write cursor
    int* cursor = (int*)malloc(sizeof(int) * (size_t)ncells);
    memcpy(cursor, g->cell_start, sizeof(int) * (size_t)ncells);
    hit_grid_fill(root, 0, 0, ww, wh, cursor, g->cell_items, cols);
    free(cursor);
}

// finds the topmost visible widget containing (x, y) via the uniform grid
static Widget* hit_test_tree(Widget* w, int x, int y) {
    if (!w || !w->visible) return NULL;
    if (x < w->x || x >= w->x + w->w || y < w->y || y >= w->y + w->h) return NULL;

    for (int i = w->child_count - 1; i >= 0; i--) {
        if (w->children[i]->on_top) continue;                // overlay children are considered last
        Widget* r = hit_test_tree(w->children[i], x, y);
        if (r) return r;
    }
    for (int i = w->child_count - 1; i >= 0; i--) {
        if (!w->children[i]->on_top) continue;
        Widget* r = hit_test_tree(w->children[i], x, y);
        if (r) return r;
    }
    return w;
}

Widget* hit_test(UiContext* ctx, int x, int y) {
    HitGrid* g = &ctx->hit_grid;
    if (!g->cell_start || g->cols <= 0 || g->rows <= 0)       // grid not ready
        return hit_test_tree(ctx->root, x, y);

    if (x < 0 || y < 0 || x >= g->win_w || y >= g->win_h) return NULL;

    int c = x / HIT_CELL_SIZE;
    int r = y / HIT_CELL_SIZE;
    if (c < 0 || c >= g->cols || r < 0 || r >= g->rows) return NULL;

    int cell  = r * g->cols + c;
    int start = g->cell_start[cell];
    int end   = g->cell_start[cell + 1];

    // reverse iteration: the last-written widget in the cell is the topmost drawn
    for (int i = end - 1; i >= start; i--) {
        Widget* w = g->cell_items[i];
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            return w;
    }
    return NULL;
}
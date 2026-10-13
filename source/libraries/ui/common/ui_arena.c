// source/libraries/ui/common/ui_arena.c
// Per-frame bump allocator for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_arena.h"
#include <stdlib.h>

// default payload capacity of one arena block
#define UI_ARENA_BLOCK_SIZE (64 * 1024)

// allocates n bytes (8-aligned) from the per-frame arena
void* ui_arena_alloc(UiContext* ctx, size_t n) {
    n = (n + 7u) & ~(size_t)7u;                          // round up to 8 bytes
    UiArenaBlock* b = ctx->arena;                        // current (newest) block
    if (!b || b->used + (int)n > b->cap) {               // no block or no room: allocate one
        int cap = UI_ARENA_BLOCK_SIZE;
        if ((int)n > cap) cap = (int)n;                  // oversized request: one-off block
        UiArenaBlock* nb = (UiArenaBlock*)malloc(sizeof(UiArenaBlock) + (size_t)cap);
        nb->next = ctx->arena;                           // link older blocks behind
        nb->used = 0;
        nb->cap  = cap;
        ctx->arena = nb;                                 // publish the new head
        b = nb;
    }
    void* p = b->data + b->used;                         // bump the cursor inside this block
    b->used += (int)n;                                   // advance the block cursor
    return p;
}
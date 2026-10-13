// source/libraries/ui/common/ui_arena.h
// Per-frame bump allocator for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_ARENA_H
#define UI_ARENA_H

#include "ui_internal.h"

// allocates n bytes (8-aligned) from the per-frame arena
void* ui_arena_alloc(UiContext* ctx, size_t n);

#endif
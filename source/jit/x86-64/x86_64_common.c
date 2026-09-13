// source/jit/x86-64/x86_64_common.c
// Implementation of XmmCache state machine for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_common.h"

// clears cache state without touching memory
void x86_cache_clear(XmmCache* c) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) c->reg_slot[i] = -1;
    for (int i = 0; i < JIT_MAX_SLOTS; i++) {
        c->slot_reg[i] = -1;
        c->slot_dirty[i] = false;
    }
}

// checks two cache states for structural equality
bool x86_cache_eq(const XmmCache* a, const XmmCache* b) {
    for (int i = 0; i < XMM_CACHE_REGS; i++)
        if (a->reg_slot[i] != b->reg_slot[i]) return false;
    for (int i = 0; i < JIT_MAX_SLOTS; i++) {
        if (a->slot_reg[i] != b->slot_reg[i]) return false;
        if (a->slot_dirty[i] != b->slot_dirty[i]) return false;
    }
    return true;
}

// finds or evicts an xmm register, avoiding two registers that are live
int x86_cache_alloc_excl(XmmCache* c, CodeBuf* cb, int excl1, int excl2) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) {              // prefer free registers
        if (i == excl1 || i == excl2) continue;
        if (c->reg_slot[i] == -1) return i;
    }
    int victim = -1;                                        // otherwise evict
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        if (i == excl1 || i == excl2) continue;
        if (victim < 0 || c->reg_slot[i] < c->reg_slot[victim]) victim = i;
    }
    if (victim < 0) return -1;                              // all excluded
    int s = c->reg_slot[victim];
    if (c->slot_dirty[s]) x86_emit_movsd_store(cb, victim, x86_slot_disp(s));
    c->reg_slot[victim] = -1;
    c->slot_reg[s] = -1;
    c->slot_dirty[s] = false;
    return victim;
}

// returns an xmm holding slot s, loading from memory if needed
int x86_cache_load_excl(XmmCache* c, CodeBuf* cb, int s, int excl1, int excl2) {
    if (s < 0 || s >= JIT_MAX_SLOTS) return -1;
    if (c->slot_reg[s] >= 0) return c->slot_reg[s];         // cache hit
    int x = x86_cache_alloc_excl(c, cb, excl1, excl2);
    if (x < 0) return -1;
    x86_emit_movsd_load(cb, x, x86_slot_disp(s));           // cache miss: load
    c->reg_slot[x] = s;
    c->slot_reg[s] = x;
    c->slot_dirty[s] = false;
    return x;
}

// convenience wrapper around x86_cache_load_excl with no exclusions
int x86_cache_load(XmmCache* c, CodeBuf* cb, int s) {
    return x86_cache_load_excl(c, cb, s, -1, -1);
}

// marks xmm x as holding slot s (dirty), invalidating prior mappings
void x86_cache_put(XmmCache* c, int x, int s) {
    if (s < 0 || s >= JIT_MAX_SLOTS) return;
    int old = c->reg_slot[x];
    if (old >= 0 && old != s) c->slot_reg[old] = -1;        // x no longer holds old slot
    int prev = c->slot_reg[s];
    if (prev >= 0 && prev != x) c->reg_slot[prev] = -1;     // that xmm no longer holds s
    c->reg_slot[x] = s;
    c->slot_reg[s] = x;
    c->slot_dirty[s] = true;
}

// writes back all dirty slots to memory and clears the cache
void x86_cache_flush(XmmCache* c, CodeBuf* cb) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = c->reg_slot[i];
        if (s >= 0 && c->slot_dirty[s]) x86_emit_movsd_store(cb, i, x86_slot_disp(s));
    }
    x86_cache_clear(c);
}
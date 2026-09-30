// source/compiler/opt_const_cache.c
// Per-function dedup caches for numeric and string constants
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// derive cache_floor from the highest register currently pinned by a live entry
static void recompute_cache_floor(CodeGenerator* cg) {
    int highest = -1;                                                                       // no pins yet
    for (int i = 0; i < cg->str_cache.count; i++) {                                         // scan string pins
        if (cg->str_cache.regs[i] > highest) highest = cg->str_cache.regs[i];               // track highest
    }
    for (int i = 0; i < cg->num_cache.count; i++) {                                         // scan numeric pins
        if (cg->num_cache.regs[i] > highest) highest = cg->num_cache.regs[i];               // track highest
    }
    cg->cache_floor = highest + 1;                                                          // floor above every live pin
}

// find cached numeric constant, return its register or -1
int num_cache_lookup(CodeGenerator* cg, double value) {
    for (int i = 0; i < cg->num_cache.count; i++) {                                         // scan cached entries
        if (cg->num_cache.values[i] == value) {                                             // match by exact value
            return cg->num_cache.regs[i];                                                   // return cached register
        }
    }
    return -1;                                                                              // not cached
}

// pin a numeric constant to a register, capping the cache at 8 entries
void num_cache_add(CodeGenerator* cg, double value, int reg) {
    if (cg->num_cache.count >= 8) return;                                                   // cap at 8 to bound pressure
    if (cg->num_cache.count >= cg->num_cache.capacity) {                                    // need more space
        cg->num_cache.capacity = cg->num_cache.capacity == 0 ? 8 : cg->num_cache.capacity * 2;  // double capacity
        cg->num_cache.values = (double*)realloc(cg->num_cache.values,                       // grow values
                                                sizeof(double) * cg->num_cache.capacity);
        cg->num_cache.regs   = (int*)   realloc(cg->num_cache.regs,                         // grow regs
                                                sizeof(int) * cg->num_cache.capacity);
    }
    cg->num_cache.values[cg->num_cache.count] = value;                                      // store constant value
    cg->num_cache.regs[cg->num_cache.count] = reg;                                          // store register
    cg->num_cache.count++;                                                                  // advance count
    if (cg->cache_floor <= reg) cg->cache_floor = reg + 1;                                  // keep this register alive
}

// drop every numeric-cache entry whose register has just been overwritten
void num_cache_invalidate(CodeGenerator* cg, int written_reg) {
    for (int i = 0; i < cg->num_cache.count; i++) {                                         // scan entries
        if (cg->num_cache.regs[i] == written_reg) {                                         // same register clobbered
            cg->num_cache.count--;                                                          // shrink count
            cg->num_cache.values[i] = cg->num_cache.values[cg->num_cache.count];            // move last value in
            cg->num_cache.regs[i]   = cg->num_cache.regs[cg->num_cache.count];              // move last reg in
            cg->num_cache.values[cg->num_cache.count] = 0.0;                                // clear moved-from value
            cg->num_cache.regs[cg->num_cache.count]   = -1;                                 // clear moved-from reg
            i--;                                                                            // recheck swapped entry
        }
    }
}

// drop numeric-cache entries at indices >= new_count and recompute cache_floor
void num_cache_truncate(CodeGenerator* cg, int new_count) {
    if (new_count < cg->num_cache.count) {                                                  // only shrink
        cg->num_cache.count = new_count;                                                    // drop tail
        recompute_cache_floor(cg);                                                          // floor must drop too
    }
}

// find cached string literal, return its register or -1
int str_cache_lookup(CodeGenerator* cg, const char* value) {
    for (int i = 0; i < cg->str_cache.count; i++) {                                         // scan cached entries
        if (strcmp(cg->str_cache.values[i], value) == 0) {                                  // match by content
            return cg->str_cache.regs[i];                                                   // return cached register
        }
    }
    return -1;                                                                              // not cached
}

// pin a string literal to a register, capping the cache at 8 entries
void str_cache_add(CodeGenerator* cg, const char* value, int reg) {
    if (cg->str_cache.count >= 8) return;                                                   // cap at 8 to bound pressure
    if (cg->str_cache.count >= cg->str_cache.capacity) {                                    // need more space
        cg->str_cache.capacity = cg->str_cache.capacity == 0 ? 8 : cg->str_cache.capacity * 2;  // double capacity
        cg->str_cache.values = (char**)realloc(cg->str_cache.values,                        // grow values
                                               sizeof(char*) * cg->str_cache.capacity);
        cg->str_cache.regs   = (int*)   realloc(cg->str_cache.regs,                         // grow regs
                                                sizeof(int) * cg->str_cache.capacity);
    }
    cg->str_cache.values[cg->str_cache.count] = strdup(value);                              // own a copy of the content
    cg->str_cache.regs[cg->str_cache.count]   = reg;                                        // store register
    cg->str_cache.count++;                                                                  // advance count
    if (cg->cache_floor <= reg) cg->cache_floor = reg + 1;                                  // keep this register alive
}

// drop every string-cache entry whose register has just been overwritten
void str_cache_invalidate(CodeGenerator* cg, int written_reg) {
    for (int i = 0; i < cg->str_cache.count; i++) {                                         // scan entries
        if (cg->str_cache.regs[i] == written_reg) {                                         // same register clobbered
            free(cg->str_cache.values[i]);                                                  // release owned copy
            cg->str_cache.count--;                                                          // shrink count
            cg->str_cache.values[i] = cg->str_cache.values[cg->str_cache.count];            // move last value in
            cg->str_cache.regs[i]   = cg->str_cache.regs[cg->str_cache.count];              // move last reg in
            cg->str_cache.values[cg->str_cache.count] = NULL;                               // clear moved-from value
            cg->str_cache.regs[cg->str_cache.count]   = -1;                                 // clear moved-from reg
            i--;                                                                            // recheck swapped entry
        }
    }
}

// free string-cache entries at indices >= new_count and recompute cache_floor
void str_cache_truncate(CodeGenerator* cg, int new_count) {
    for (int i = new_count; i < cg->str_cache.count; i++) {                                 // walk dropped range
        free(cg->str_cache.values[i]);                                                      // release owned copy
        cg->str_cache.values[i] = NULL;                                                     // clear slot
        cg->str_cache.regs[i]   = -1;                                                       // clear slot
    }
    if (new_count < cg->str_cache.count) cg->str_cache.count = new_count;                   // shrink count
    recompute_cache_floor(cg);                                                              // floor must drop too
}
// source/compiler/opt_const_cache.c
// Per-function dedup caches for numeric and string constants
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// looks up a numeric constant in the per-function cache, returns register or -1
int num_cache_lookup(CodeGenerator* cg, double value) {
    for (int i = 0; i < cg->num_cache.count; i++) {              // scan cached entries
        if (cg->num_cache.values[i] == value) {                  // match by exact value
            return cg->num_cache.regs[i];                        // return cached register
        }
    }
    return -1;                                                   // not cached
}

// records a numeric constant and pins its register for the rest of the function
void num_cache_add(CodeGenerator* cg, double value, int reg) {
    if (cg->num_cache.count >= 16) return;                       // cap to limit register pressure
    if (cg->num_cache.count >= cg->num_cache.capacity) {         // need more space
        cg->num_cache.capacity = cg->num_cache.capacity == 0 ? 8 : cg->num_cache.capacity * 2;
        cg->num_cache.values = (double*)realloc(cg->num_cache.values,
                                                sizeof(double) * cg->num_cache.capacity);
        cg->num_cache.regs   = (int*)   realloc(cg->num_cache.regs,
                                                sizeof(int) * cg->num_cache.capacity);
    }
    cg->num_cache.values[cg->num_cache.count] = value;           // store constant value
    cg->num_cache.regs[cg->num_cache.count] = reg;               // store register
    cg->num_cache.count++;                                       // advance count
    if (cg->cache_floor <= reg) cg->cache_floor = reg + 1;       // pin above cache floor
}

// drops every numeric-cache entry whose register has just been overwritten
void num_cache_invalidate(CodeGenerator* cg, int written_reg) {
    for (int i = 0; i < cg->num_cache.count; i++) {
        if (cg->num_cache.regs[i] == written_reg) {
            cg->num_cache.count--;
            cg->num_cache.values[i] = cg->num_cache.values[cg->num_cache.count];
            cg->num_cache.regs[i]   = cg->num_cache.regs[cg->num_cache.count];
            cg->num_cache.values[cg->num_cache.count] = 0.0;
            cg->num_cache.regs[cg->num_cache.count]   = -1;
            i--;
        }
    }
}

// looks up a string literal in the per-function cache, returns register or -1
int str_cache_lookup(CodeGenerator* cg, const char* value) {
    for (int i = 0; i < cg->str_cache.count; i++) {              // scan cached entries
        if (strcmp(cg->str_cache.values[i], value) == 0) {       // match by content
            return cg->str_cache.regs[i];                        // return cached register
        }
    }
    return -1;                                                   // not cached
}

// records a string literal and pins its register for the rest of the function
void str_cache_add(CodeGenerator* cg, const char* value, int reg) {
    if (cg->str_cache.count >= 16) return;                       // cap to limit register pressure
    if (cg->str_cache.count >= cg->str_cache.capacity) {         // need more space
        cg->str_cache.capacity = cg->str_cache.capacity == 0 ? 8 : cg->str_cache.capacity * 2;
        cg->str_cache.values = (char**)realloc(cg->str_cache.values,
                                               sizeof(char*) * cg->str_cache.capacity);
        cg->str_cache.regs   = (int*)   realloc(cg->str_cache.regs,
                                                sizeof(int) * cg->str_cache.capacity);
    }
    cg->str_cache.values[cg->str_cache.count] = strdup(value);   // own a copy of the content
    cg->str_cache.regs[cg->str_cache.count]   = reg;             // store register
    cg->str_cache.count++;                                       // advance count
    if (cg->cache_floor <= reg) cg->cache_floor = reg + 1;       // pin above cache floor
}

// drops every string-cache entry whose register has just been overwritten
void str_cache_invalidate(CodeGenerator* cg, int written_reg) {
    for (int i = 0; i < cg->str_cache.count; i++) {
        if (cg->str_cache.regs[i] == written_reg) {
            free(cg->str_cache.values[i]);                               // release owned copy
            cg->str_cache.count--;
            cg->str_cache.values[i] = cg->str_cache.values[cg->str_cache.count];
            cg->str_cache.regs[i]   = cg->str_cache.regs[cg->str_cache.count];
            cg->str_cache.values[cg->str_cache.count] = NULL;            // clear moved-from slot
            cg->str_cache.regs[cg->str_cache.count]   = -1;              // so a later count restore cannot resurrect it
            i--;                                                         // recheck swapped-in entry
        }
    }
}

// frees str_cache entries at indices >= new_count
void str_cache_truncate(CodeGenerator* cg, int new_count) {
    for (int i = new_count; i < cg->str_cache.count; i++) {
        free(cg->str_cache.values[i]);
        cg->str_cache.values[i] = NULL;
        cg->str_cache.regs[i]   = -1;
    }
    if (new_count < cg->str_cache.count) cg->str_cache.count = new_count;
}
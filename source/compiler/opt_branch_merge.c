// source/compiler/opt_branch_merge.c
// Per-local numeric-ness snapshots and merge points
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// captures the current per-local numeric-ness and const-ness state
LocalNumSnap snap_numbers(CodeGenerator* cg) {
    LocalNumSnap s;
    s.count = cg->locals.count;
    s.flags     = s.count ? (bool*)malloc(sizeof(bool) * s.count)     : NULL;
    s.int_flags = s.count ? (bool*)malloc(sizeof(bool) * s.count)     : NULL;
    s.cflags    = s.count ? (bool*)malloc(sizeof(bool) * s.count)     : NULL;
    s.cvals     = s.count ? (double*)malloc(sizeof(double) * s.count) : NULL;
    if (s.flags)     memcpy(s.flags,     cg->locals.is_number,   sizeof(bool) * s.count);
    if (s.int_flags) memcpy(s.int_flags, cg->locals.is_integer,  sizeof(bool) * s.count);
    if (s.cflags)    memcpy(s.cflags,    cg->locals.const_known, sizeof(bool) * s.count);
    if (s.cvals)     memcpy(s.cvals,     cg->locals.const_value, sizeof(double) * s.count);
    return s;
}

// restores per-local flags from a snapshot
void restore_numbers(CodeGenerator* cg, LocalNumSnap s) {
    int n = cg->locals.count < s.count ? cg->locals.count : s.count;
    if (n > 0 && s.flags)     memcpy(cg->locals.is_number,   s.flags,     sizeof(bool) * n);
    if (n > 0 && s.int_flags) memcpy(cg->locals.is_integer,  s.int_flags, sizeof(bool) * n);
    if (n > 0 && s.cflags)    memcpy(cg->locals.const_known, s.cflags,    sizeof(bool) * n);
    if (n > 0 && s.cvals)     memcpy(cg->locals.const_value, s.cvals,     sizeof(double) * n);
}

// intersects two snapshots into cg (used at control-flow merge points)
void merge_numbers(CodeGenerator* cg, LocalNumSnap a, LocalNumSnap b) {
    int n = cg->locals.count;
    if (a.count < n) n = a.count;
    if (b.count < n) n = b.count;
    for (int i = 0; i < n; i++) {
        cg->locals.is_number[i]  = a.flags[i]     && b.flags[i];
        cg->locals.is_integer[i] = a.int_flags[i] && b.int_flags[i];
        // const survives only if both sides agree on the same value
        bool ca = a.cflags ? a.cflags[i] : false;
        bool cb = b.cflags ? b.cflags[i] : false;
        if (ca && cb && a.cvals[i] == b.cvals[i]) {
            cg->locals.const_known[i] = true;
            cg->locals.const_value[i] = a.cvals[i];
        } else {
            cg->locals.const_known[i] = false;
        }
    }
}

// frees a numeric-ness snapshot
void free_snap(LocalNumSnap s) {
    free(s.flags);
    free(s.int_flags);
    free(s.cflags);
    free(s.cvals);
}
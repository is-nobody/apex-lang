// source/compiler/opt_linear_scan.c
// Linear-scan register allocation over block-local live ranges
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>

// per-slot first/last statement index, in program order; control-flow counts as one stmt
void liveness_walk(CodeGenerator* cg, ASTNode* node, int* stmt_idx,
                   int* first, int* last) {
    if (!node) return;
    switch (node->type) {
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                liveness_walk(cg, node->block.statements->nodes[i], stmt_idx, first, last);
            return;
        case AST_FUNCTION_DECL:
            return;                              // nested function has its own locals
        default: {
            (*stmt_idx)++;
            int idx = *stmt_idx;
            for (int i = 0; i < cg->locals.count; i++) {
                if (stmt_references_local(node, cg->locals.names[i])) {
                    if (first[i] < 0) first[i] = idx;
                    last[i] = idx;
                }
            }
            return;
        }
    }
}

// overlap-aware linear scan: body locals share regs when live ranges don't intersect; params pin 0..n-1
void assign_registers_linear_scan(CodeGenerator* cg, int n_params,
                                  int* first, int* last) {
    int n = cg->locals.count;
    int body_count = n - n_params;
    if (body_count <= 0) return;

    int* order = (int*)malloc(sizeof(int) * body_count);
    for (int i = 0; i < body_count; i++) order[i] = n_params + i;

    // insertion sort body-local slots by first use (unused go last)
    for (int i = 1; i < body_count; i++) {
        int key = order[i];
        int kf = first[key]; if (kf < 0) kf = 0x7fffffff;
        int j = i - 1;
        while (j >= 0) {
            int jf = first[order[j]]; if (jf < 0) jf = 0x7fffffff;
            if (jf > kf) { order[j + 1] = order[j]; j--; }
            else break;
        }
        order[j + 1] = key;
    }

    // holder[r] = slot holding r; release[r] = stmt index at which r becomes dead
    int cap = n + 16;
    int* holder  = (int*)malloc(sizeof(int) * cap);
    int* release = (int*)malloc(sizeof(int) * cap);
    for (int r = 0; r < cap; r++) { holder[r] = -1; release[r] = -1; }

    for (int p = 0; p < n_params; p++) {           // seed params, protect their registers
        holder[p]  = p;
        release[p] = (last[p] > 0) ? last[p] : 0;
    }

    int max_used = n_params - 1;
    for (int i = 0; i < body_count; i++) {
        int slot = order[i];
        int f = first[slot];
        if (f < 0) { cg->locals.registers[slot] = 0; continue; }   // never used

        for (int r = n_params; r < cap; r++) {                     // release dead registers
            if (holder[r] >= 0 && release[r] < f) {
                holder[r]  = -1;
                release[r] = -1;
            }
        }
        int assigned = -1;
        for (int r = n_params; r < cap; r++) {                     // lowest free
            if (holder[r] < 0) { assigned = r; break; }
        }
        if (assigned < 0) assigned = cap++;                        // safety, should not hit
        holder[assigned]  = slot;
        release[assigned] = last[slot];
        cg->locals.registers[slot] = assigned;
        if (assigned > max_used) max_used = assigned;
    }

    cg->next_register = max_used + 1;
    cg->max_registers = cg->next_register;          // reset; body emission bumps it again

    free(order);
    free(holder);
    free(release);
}
// source/compiler/opt_jump_targets.c
// Jump-target bitset used by peephole fusion and patching
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// follows a chain of unconditional jumps: while pc points at an OP_JUMP
int resolve_jump_target(CodeGenerator* cg, int pc) {
    for (int hop = 0; hop < 16; hop++) {
        if (pc < 0 || pc >= cg->chunk->code_count) break;
        Instruction* t = &cg->chunk->code[pc];
        if (t->opcode != OP_JUMP) break;             // not a plain unconditional jump
        int next = t->operands[0];
        if (next == 0 || next == pc) break;          // placeholder or self-loop
        pc = next;
    }
    return pc;
}

// records that `pc` is a jump target; grows the bitset on demand
void mark_jump_target(CodeGenerator* cg, int pc) {
    if (pc < 0) return;
    if (pc >= cg->jump_targets_cap) {
        int new_cap = cg->jump_targets_cap < 256 ? 256 : cg->jump_targets_cap;
        while (new_cap <= pc) new_cap *= 2;
        cg->jump_targets = (uint8_t*)realloc(cg->jump_targets, new_cap);
        memset(cg->jump_targets + cg->jump_targets_cap, 0,
               new_cap - cg->jump_targets_cap);
        cg->jump_targets_cap = new_cap;
    }
    cg->jump_targets[pc] = 1;
}

// true when some jump in the chunk targets `target` (O(1) lookup)
bool code_has_jump_to(CodeGenerator* cg, int target) {
    return target >= 0 && target < cg->jump_targets_cap &&
           cg->jump_targets[target] != 0;
}
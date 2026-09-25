// source/compiler/opt_bc_dce.c
// Bytecode-level local dead-store elimination
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// true when an instruction is a semantic no-op
static bool inst_is_nop(Instruction* inst) {
    return inst->opcode == OP_MOVE &&
           inst->operands[0] == inst->operands[1];
}

// true when control does not fall through past this opcode
static bool inst_is_terminator(Opcode op) {
    switch (op) {
        case OP_JUMP:
        case OP_RETURN: case OP_RETURN_NUM: case OP_RETURN_NUM_IMM:
        case OP_RETURN_BOOL: case OP_RETURN_NONE:
        case OP_HALT:
            return true;
        default:
            return false;
    }
}

// replaces a dead store with MOVE R0, R0 (peephole-friendly no-op)
static void inst_make_nop(Instruction* inst) {
    inst->opcode = OP_MOVE;
    inst->operands[0] = 0;
    inst->operands[1] = 0;
    inst->operands[2] = 0;
}

// removes pure stores whose destination is never read before being overwritten
int dce_local_range(CodeGenerator* cg, int from_pc, int to_pc) {
    if (from_pc >= to_pc) return 0;                          // empty range
    if (!inst_is_terminator(cg->chunk->code[to_pc - 1].opcode))
        return 0;                                            // fall-through: unsafe

    int removed = 0;                                         // stores killed
    for (int pc = from_pc; pc < to_pc; pc++) {
        Instruction* in = &cg->chunk->code[pc];              // candidate store
        if (inst_is_nop(in)) continue;                       // already a no-op
        if (!op_writes_dest_reg(in->opcode)) continue;       // must write operands[0]
        if (!op_is_pure(in->opcode)) continue;               // must be side-effect free

        int d = in->operands[0];                             // destination register
        if (d < cg->cache_floor) continue;                   // pinned by a const cache

        bool dead = true;                                    // assume dead until proven live
        for (int q = pc + 1; q < to_pc; q++) {               // scan forward
            Instruction* nx = &cg->chunk->code[q];
            if (inst_is_nop(nx)) continue;                   // no-ops neither read nor write
            if (inst_reads_reg(nx, d)) { dead = false; break; }   // value used
            if (op_writes_dest_reg(nx->opcode) &&
                nx->operands[0] == d) break;                 // overwritten: dead
        }
        if (dead) {                                          // store is dead
            imm_lvn_invalidate(cg, d);                       // drop LVN entries for d
            inst_make_nop(in);                               // replace with MOVE R0, R0
            removed++;
        }
    }
    return removed;
}

// true when the opcode's operands[0] is a code offset that must be remapped
bool op_has_pc_in_op0(Opcode op) {
    if (op == OP_JUMP) return true;
    if (op >= OP_JUMP_IF_FALSE && op <= OP_JUMP_IF_GTE) return true;
    if (op >= OP_JUMP_IF_EQ_IMM && op <= OP_JUMP_IF_GTE_IMM) return true;
    if (op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
        op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) return true;
    return false;
}

// drops every MOVE R,R no-op and remaps all code offsets
void compact_bytecode(CodeGenerator* cg) {
    int total = cg->chunk->code_count;
    if (total == 0) return;

    int* map = (int*)malloc(sizeof(int) * (total + 1));       // old pc -> new pc
    int new_count = 0;
    for (int pc = 0; pc < total; pc++) {
        Instruction* in = &cg->chunk->code[pc];
        map[pc] = new_count;                                  // dropped pc maps to next kept
        if (inst_is_nop(in)) continue;                        // skip MOVE R,R
        if (new_count != pc) cg->chunk->code[new_count] = *in;
        new_count++;
    }
    map[total] = new_count;                                   // sentinel for end-of-chunk
    cg->chunk->code_count = new_count;

    for (int pc = 0; pc < new_count; pc++) {                  // rewrite every PC-bearing operand
        Instruction* in = &cg->chunk->code[pc];
        if (op_has_pc_in_op0(in->opcode)) {
            int old = in->operands[0];
            if (old >= 0 && old <= total) in->operands[0] = map[old];
        }
        if (in->opcode == OP_FOR_NEXT || in->opcode == OP_FOR_NEXT_LOOP) {
            int old = in->operands[1];
            if (old >= 0 && old <= total) in->operands[1] = map[old];
        }
        if (in->opcode == OP_TABLE_ITER_NEXT) {
            int old = in->operands[2];
            if (old >= 0 && old <= total) in->operands[2] = map[old];
        }
    }

    for (int i = 0; i < cg->chunk->func_count; i++) {         // function entry addresses
        int old = cg->chunk->functions[i].address;
        if (old >= 0 && old <= total) cg->chunk->functions[i].address = map[old];
    }

    if (cg->jump_targets) {                                   // rebuild the bitset at new pcs
        memset(cg->jump_targets, 0, cg->jump_targets_cap);
        for (int pc = 0; pc < new_count; pc++) {
            Instruction* in = &cg->chunk->code[pc];
            if (op_has_pc_in_op0(in->opcode)) mark_jump_target(cg, in->operands[0]);
            if (in->opcode == OP_FOR_NEXT || in->opcode == OP_FOR_NEXT_LOOP)
                mark_jump_target(cg, in->operands[1]);
            if (in->opcode == OP_TABLE_ITER_NEXT)
                mark_jump_target(cg, in->operands[2]);
        }
    }

    free(map);
}
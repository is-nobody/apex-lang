// source/jit/jit_analysis.c
// Implementation of JIT analysis pass for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit_internal.h"
#include <string.h>
#include <stdlib.h>

// checks if a function contains only numeric ops and in-range jumps
static void compute_function_range(BytecodeChunk* chunk, int func_idx,
                                   int* start, int* end) {
    *start = chunk->functions[func_idx].address;                 // entry point
    if (func_idx == 0) { *end = *start; return; }                // entry fn never JITted

    int prev = *start - 1;                                       // instruction before entry
    if (prev >= 0 && prev < chunk->code_count &&
        chunk->code[prev].opcode == OP_JUMP) {                   // jump-over marker present
        int target = chunk->code[prev].operands[0];              // jump target = range end
        if (target > *start && target <= chunk->code_count) {
            *end = target;                                       // accept as range end
            return;
        }
    }
    if (func_idx + 1 < chunk->func_count)                        // fallback: to next fn
        *end = chunk->functions[func_idx + 1].address;
    else
        *end = chunk->code_count;                                // last fn: to end of code
}

// checks whether an instruction is allowed inside a native loop body
static bool is_pure_loop_instr(BytecodeChunk* chunk, int pc) {
    Instruction* inst = &chunk->code[pc];
    switch (inst->opcode) {
        case OP_MOVE:
        case OP_LOAD_NUM_IMM:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_NEG: case OP_INC: case OP_DEC:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_EQ:     case OP_CMP_NEQ:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_JUMP:
        case OP_JUMP_IF_EQ:     case OP_JUMP_IF_NEQ:
        case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_FOR_NEXT:
            return true;
        case OP_LOAD_NUM: {
            int idx = inst->operands[1];
            return idx >= 0 && idx < chunk->const_count &&
                   chunk->constants[idx].type == CONST_NUMBER;
        }
        default:
            return false;
    }
}

// checks if a function contains only numeric ops and in-range jumps
static bool function_is_initially_pure(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];                      // first pc of this fn
    int end   = ctx->range_end[func_idx];                        // one past last pc
    if (start >= end) return false;                              // empty range

    for (int pc = start; pc < end; pc++) {                       // scan every instruction
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_MOVE:                                        // trivial moves
            case OP_LOAD_NUM_IMM:                                // small int literals
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_NEG: case OP_INC: case OP_DEC:               // arithmetic
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // numeric compares
            case OP_CMP_EQ:     case OP_CMP_NEQ:                 // generic compares
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
            case OP_RETURN: case OP_RETURN_NUM:                  // returns
            case OP_RETURN_NONE:                                 // incl. implicit none
                break;                                           // always allowed

            case OP_LOAD_NUM: {                                  // only numeric constants
                int idx = inst->operands[1];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) return false;        // out of range
                if (chunk->constants[idx].type != CONST_NUMBER) return false;  // non-numeric
                break;
            }

            case OP_JUMP:                                        // all jumps must stay
            case OP_JUMP_IF_FALSE:                               // within this function
            case OP_JUMP_IF_EQ:     case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE: {
                int target = inst->operands[0];                     // jump target pc
                if (target < start || target >= end) return false;  // leaves the function
                break;
            }

            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:  // call targets validated
                break;                                       // by the fixpoint pass

            default:
                return false;                                // any other op: reject
        }
    }
    return true;
}

// infers how a function returns its value; returns false on mixed kinds
#define JIT_MAX_REGS_SCAN 512

static bool infer_return_type(BytecodeChunk* chunk, int start, int end,
                              JitReturnType* out) {
    bool is_bool[JIT_MAX_REGS_SCAN];                             // bool-flag per register
    memset(is_bool, 0, sizeof(is_bool));                         // no regs are bool initially

    bool any_bool = false;                                       // saw a bool RETURN
    bool any_num  = false;                                       // saw a numeric RETURN
    bool any_none = false;                                       // saw a RETURN_NONE

    for (int pc = start; pc < end; pc++) {                       // walk the function body
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // destination register
        int a = inst->operands[1];                               // first source register

        switch (inst->opcode) {
            case OP_CMP_EQ:     case OP_CMP_NEQ:                 // all comparisons produce
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // a bool
            case OP_CMP_LT: case OP_CMP_GT:
            case OP_CMP_LTE: case OP_CMP_GTE:
                if (d < JIT_MAX_REGS_SCAN) is_bool[d] = true;    // dest holds bool
                break;

            case OP_MOVE:                                        // move preserves bool-ness
                if (d < JIT_MAX_REGS_SCAN)                       // dest in range
                    is_bool[d] = (a < JIT_MAX_REGS_SCAN) ? is_bool[a] : false;    // copy flag
                break;

            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:              // numeric producers
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_NEG: case OP_INC: case OP_DEC:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:      // treated as numeric here
                if (d < JIT_MAX_REGS_SCAN) is_bool[d] = false;   // dest is not bool
                break;

            case OP_RETURN:                                      // return kind depends on reg
                if (d < JIT_MAX_REGS_SCAN && is_bool[d]) any_bool = true;
                else                                     any_num  = true;
                break;

            case OP_RETURN_NUM:                                  // explicitly numeric
                any_num = true;
                break;

            case OP_RETURN_NONE:                                 // explicitly none
                any_none = true;
                break;

            default: break;                                      // other ops don't matter
        }
    }

    int kinds = (any_bool ? 1 : 0) + (any_num ? 1 : 0) + (any_none ? 1 : 0);
    if (kinds != 1) return false;                                // mixed or no return -> reject

    *out = any_none ? JIT_RET_NONE
         : any_bool ? JIT_RET_BOOL
                    : JIT_RET_NUMBER;
    return true;
}

// checks if an opcode can start a native loop
static bool is_loop_entry_op(Opcode op) {
    return op == OP_FOR_NEXT ||
           (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE);
}

// walks the loop body, marks read and written slots
static void analyze_loop_regs(JITContext* ctx, JitLoopInfo* info) {
    BytecodeChunk* chunk = ctx->chunk;
    uint64_t reads = 0, writes = 0;
    for (int pc = info->entry_pc; pc <= info->back_edge_pc; pc++) {
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];
        int a = inst->operands[1];
        int b = inst->operands[2];
        switch (inst->opcode) {
            case OP_MOVE: case OP_NEG:
                if (a >= 0 && a < 64) reads  |= 1ULL << a;
                if (d >= 0 && d < 64) writes |= 1ULL << d;
                break;
            case OP_INC: case OP_DEC:
                if (d >= 0 && d < 64) reads |= writes |= 1ULL << d;
                break;
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_CMP_EQ: case OP_CMP_NEQ:
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                if (a >= 0 && a < 64) reads  |= 1ULL << a;
                if (b >= 0 && b < 64) reads  |= 1ULL << b;
                if (d >= 0 && d < 64) writes |= 1ULL << d;
                break;
            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
                if (d >= 0 && d < 64) writes |= 1ULL << d;
                break;
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
                if (a >= 0 && a < 64) reads |= 1ULL << a;
                if (b >= 0 && b < 64) reads |= 1ULL << b;
                break;
            case OP_FOR_NEXT:
                if (d >= 0 && d < 64) reads |= writes |= 1ULL << d;
                if (info->for_end_reg  >= 0 && info->for_end_reg  < 64) reads |= 1ULL << info->for_end_reg;
                if (info->for_step_reg >= 0 && info->for_step_reg < 64) reads |= 1ULL << info->for_step_reg;
                break;
            case OP_JUMP:
                break;
            default: break;
        }
    }
    info->live_in  = reads;
    info->live_out = writes;
}

// registers a loop, running the slot analysis
static void add_loop(JITContext* ctx, int entry, int back_edge, int exit_pc,
                     bool is_for_next, int for_end_reg, int for_step_reg,
                     int nregs) {
    if (ctx->loop_count >= ctx->loop_capacity) {
        int new_cap = ctx->loop_capacity == 0 ? 8 : ctx->loop_capacity * 2;
        JitLoopInfo* new_arr = (JitLoopInfo*)realloc(ctx->loops, sizeof(JitLoopInfo) * new_cap);
        if (!new_arr) return;
        ctx->loops = new_arr;
        ctx->loop_capacity = new_cap;
    }
    JitLoopInfo* info = &ctx->loops[ctx->loop_count++];
    memset(info, 0, sizeof(*info));
    info->entry_pc = entry;
    info->back_edge_pc = back_edge;
    info->exit_pc = exit_pc;
    info->is_for_next = is_for_next;
    info->for_end_reg = for_end_reg;
    info->for_step_reg = for_step_reg;
    info->nregs = nregs;
    analyze_loop_regs(ctx, info);
}

// scans one function for numeric loops
static void detect_loops_in_function(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];
    int end   = ctx->range_end[func_idx];
    int nregs = chunk->functions[func_idx].max_registers;
    if (nregs < 1) nregs = 1;
    if (nregs > JIT_MAX_SLOTS - 2) return;

    for (int pc = start; pc < end; pc++) {                       // scan for back edges
        if (chunk->code[pc].opcode != OP_JUMP) continue;         // only plain JUMP
        int entry = chunk->code[pc].operands[0];                 // target pc
        if (entry >= pc || entry < start) continue;              // must be backward and in-range
        if (!is_loop_entry_op(chunk->code[entry].opcode)) continue;  // valid entry opcode

        Opcode entry_op = chunk->code[entry].opcode;
        int exit_pc;
        int for_end_reg = -1, for_step_reg = -1;
        if (entry_op == OP_FOR_NEXT) {
            if (chunk->code[entry].operands[2] != 0) continue;   // non-numeric for — reject
            exit_pc = chunk->code[entry].operands[1];
        } else {
            exit_pc = chunk->code[entry].operands[0];
        }
        if (exit_pc != pc + 1) continue;                         // exit must be right after back edge

        bool ok = true;
        for (int i = entry + 1; i < pc && ok; i++) {             // no other jumps in body
            Opcode op = chunk->code[i].opcode;
            if (op == OP_JUMP || op == OP_FOR_NEXT ||
                (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE)) {
                ok = false;                                      // nested loop or internal branch
            }
        }
        if (!ok) continue;

        for (int i = entry; i <= pc && ok; i++) {                // every body instruction must be loop-pure
            if (!is_pure_loop_instr(chunk, i)) ok = false;
        }
        if (!ok) continue;

        if (entry_op == OP_FOR_NEXT) {                           // extra checks for FOR_NEXT
            int fi = entry - 1;
            if (fi < start) continue;
            if (chunk->code[fi].opcode != OP_FOR_INIT) continue;
            int var_reg  = chunk->code[fi].operands[0];
            int end_reg  = chunk->code[fi].operands[1];
            int step_reg = chunk->code[fi].operands[2];
            if (chunk->code[entry].operands[0] != var_reg) continue;
            int p = fi - 1;                                      // step must be provably positive
            if (p < start) continue;
            if (chunk->code[p].opcode != OP_LOAD_NUM_IMM) continue;
            if (chunk->code[p].operands[0] != step_reg) continue;
            if (chunk->code[p].operands[1] <= 0) continue;
            for_end_reg = end_reg;
            for_step_reg = step_reg;
        }

        add_loop(ctx, entry, pc, exit_pc,
                 entry_op == OP_FOR_NEXT, for_end_reg, for_step_reg, nregs);
    }
}

// runs the full analysis pipeline; returns false if there is nothing to compile
bool jit_analyze(JITContext* ctx) {
    BytecodeChunk* chunk = ctx->chunk;
    int n = ctx->func_count;                                     // number of functions

    for (int i = 0; i < n; i++)                                  // ranges for every function
        compute_function_range(chunk, i, &ctx->range_start[i], &ctx->range_end[i]);

    for (int i = 1; i < n; i++)                                  // initial purity
        ctx->pure[i] = function_is_initially_pure(ctx, i);       // fn 0 never JITted

    bool changed = true;                                         // purity fixpoint
    while (changed) {
        changed = false;
        for (int i = 1; i < n; i++) {                            // check each function
            if (!ctx->pure[i]) continue;                         // already rejected
            for (int pc = ctx->range_start[i]; pc < ctx->range_end[i]; pc++) {
                Instruction* inst = &chunk->code[pc];
                if (inst->opcode == OP_CALL_0 ||                 // check every call's target
                    inst->opcode == OP_CALL_1 ||
                    inst->opcode == OP_CALL_2) {
                    int target = inst->operands[1];              // callee function index
                    if (target < 0 || target >= n || !ctx->pure[target]) {
                        ctx->pure[i] = false;                    // callee not pure
                        changed = true;                          // need another pass
                        break;
                    }
                }
            }
        }
    }

    for (int i = 1; i < n; i++) {                                // return-type detection
        if (!ctx->pure[i]) continue;                             // skip non-pure
        if (!infer_return_type(chunk, ctx->range_start[i], ctx->range_end[i],
                               &ctx->return_type[i])) {
            ctx->pure[i] = false;                                // mixed return -> reject
        }
    }

    for (int i = 1; i < n; i++) {                                // loop detection for non-pure fns
        if (ctx->pure[i]) continue;                              // pure fns are fully compiled
        detect_loops_in_function(ctx, i);
    }

    int candidate_count = 0;                                     // anything to emit?
    for (int i = 0; i < n; i++) if (ctx->pure[i]) candidate_count++;
    return candidate_count > 0 || ctx->loop_count > 0;
}
// source/jit/jit_analysis.c
// Implementation of JIT analysis pass for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit_internal.h"
#include "vm.h"
#include <string.h>
#include <stdlib.h>

// checks if a function contains only numeric ops and in-range jumps
static void compute_function_range(BytecodeChunk* chunk, int func_idx,
                                   int* start, int* end) {
    *start = chunk->functions[func_idx].address;                 // entry point
    if (func_idx == 0) {                                         // entry fn: loop detection still needs a full range
        *end = chunk->code_count;                                // cover all code so loops after inner fns are seen
        return;
    }

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
static bool is_pure_loop_instr(JITContext* ctx, int pc) {
    BytecodeChunk* chunk = ctx->chunk;
    Instruction* inst = &chunk->code[pc];
    switch (inst->opcode) {
        case OP_MOVE:                                        // reg-to-reg copy
        case OP_LOAD_NUM_IMM:                                // small int literal
        case OP_LOAD_BOOL:                                   // boolean literal
        case OP_LOAD_NONE:                                   // none literal
        case OP_AND: case OP_OR:                             // logical ops (bool-producing)
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:   // arithmetic with immediate
        case OP_DIV_IMM: case OP_MOD_IMM:                    // arithmetic with immediate
        case OP_NEG: case OP_INC: case OP_DEC:               // arithmetic
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // numeric compares
        case OP_CMP_EQ:     case OP_CMP_NEQ:                 // generic compares
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_JUMP:                                        // unconditional
        case OP_JUMP_IF_FALSE:                               // short-circuit exit test
        case OP_JUMP_IF_EQ:     case OP_JUMP_IF_NEQ:         // conditional branches
        case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:     // conditional branches with imm
        case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
        case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
        case OP_JUMP_IF_NONE:                                // none-test conditional branches
        case OP_JUMP_IF_NOT_NONE:
            return true;
        case OP_FOR_NEXT:                                    // numeric-for entry
        case OP_FOR_NEXT_LOOP:                               // loop-inverted back edge
        case OP_TABLE_GET:                                   // validated by analyze_loop_regs
        case OP_TABLE_GET_INT:                               // fixed index into array part
        case OP_TABLE_GET_NUM:                               // same semantics, numeric key
        case OP_TABLE_SET:                                   // dynamic key, validated later by analyze_loop_regs
        case OP_TABLE_SET_INT:
        case OP_TABLE_SET_NUM:                               // same semantics, numeric key
        case OP_TABLE_GET_CONST:                             // interned string key, hash part
        case OP_TABLE_SET_CONST:                             // interned string key, hash part
            return true;
        case OP_TABLE_ITER_NEXT:                             // loop entry for table iteration
            return true;
        case OP_TABLE_GET_KEY_STR:                           // fresh string key + get
        case OP_TABLE_SET_KEY_STR:                           // fresh string key + set
            return true;
        case OP_LOAD_GLOBAL:                                 // reads vm->globals[idx]
        case OP_STORE_GLOBAL:                                // writes vm->globals[idx]
            return true;
        case OP_CONCAT:                                      // string building via helper
        case OP_NEW_TABLE:                                   // fresh table via helper
        case OP_LOAD_TABLE:                                  // table literal via helper
        case OP_TABLE_APPEND:                                // positional append via helper
        case OP_CALL:                                        // many-arg call via coroutine
        case OP_ASYNC_CALL:                                  // pending future capture
            return true;                                    // basic tier: helper lowering exists
        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2: {    // calls allowed to compiled fns
            int target = inst->operands[1];                  // callee function index
            return target >= 0 && target < ctx->func_count &&
                   (ctx->pure[target] || ctx->basic[target]); // callee has a native entry
        }
        case OP_LOAD_NUM: {                                  // only numeric constants
            int idx = inst->operands[1];
            return idx >= 0 && idx < chunk->const_count &&
                   chunk->constants[idx].type == CONST_NUMBER;
        }
        case OP_LOAD_CONST: {                                // scalar constants (no tables)
            int idx = inst->operands[1];
            if (idx < 0 || idx >= chunk->const_count) return false;
            ConstantType t = chunk->constants[idx].type;
            return t == CONST_NUMBER || t == CONST_BOOL ||
                   t == CONST_NONE   || t == CONST_STRING;
        }
        case OP_PUSH_ARG:                                    // vm args stack push
        case OP_CALL_BUILTIN:                                // vm builtin dispatch
        case OP_ASYNC_CALL_BUILTIN:
            return true;
        default:
            return false;                                    // anything else: reject
    }
}

// true when a constant is a jump table whose every target lies inside the compiled bytecode range
static bool jump_table_in_range(BytecodeChunk* chunk, int idx, int start, int end) {
    if (idx < 0 || idx >= chunk->const_count) return false;
    Constant* c = &chunk->constants[idx];
    if (c->type != CONST_JUMP_TABLE) return false;
    for (int i = 0; i < c->jump_table.count; i++) {
        int t = c->jump_table.addresses[i];
        if (t < start || t > end) return false;  // end is a valid fallthrough
    }
    return true;
}

// true when every case and default target of a switch table lies within [start, end]
static bool switch_targets_in_range(BytecodeChunk* chunk, int table_idx,
                                    int start, int end) {
    if (table_idx < 0 || table_idx >= chunk->const_count) return false;
    Constant* c = &chunk->constants[table_idx];
    if (c->type == CONST_STR_SWITCH) {
        for (int i = 0; i <= c->str_switch.mask; i++) {
            int tgt = c->str_switch.entries[i].target;
            if (tgt < 0) continue;                               // empty slot, skipped
            if (tgt < start || tgt >= end) return false;         // case body out of range
        }
        int def = c->str_switch.default_pc;
        if (def < start || def > end) return false;              // end is a valid fallthrough
        return true;
    }
    if (c->type == CONST_NUM_SWITCH) {
        for (int i = 0; i <= c->num_switch.mask; i++) {
            int tgt = c->num_switch.entries[i].target;
            if (tgt < 0) continue;
            if (tgt < start || tgt >= end) return false;
        }
        int def = c->num_switch.default_pc;
        if (def < start || def > end) return false;
        return true;
    }
    return false;                                                // wrong constant kind
}

// checks if a function uses only the "JIT-able" op set (call graph validated separately)
static bool function_is_initially_pure(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];                      // first pc of this fn
    int end   = ctx->range_end[func_idx];                        // one past last pc
    if (start >= end) return false;                              // empty range
    if (chunk->functions[func_idx].arity > JIT_MAX_CALL_ARITY) return false;  // no caller can reach the entry

    for (int pc = start; pc < end; pc++) {                       // scan every instruction
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_MOVE:                                        // trivial moves
            case OP_LOAD_NUM_IMM:                                // small int literals
            case OP_LOAD_BOOL:                                   // boolean literals
            case OP_LOAD_NONE:                                   // none literal
            case OP_AND: case OP_OR:                             // logical ops
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:   // arithmetic with immediate
            case OP_DIV_IMM: case OP_MOD_IMM:                    // arithmetic with immediate
            case OP_NEG: case OP_INC: case OP_DEC:               // arithmetic
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // numeric compares
            case OP_CMP_EQ:     case OP_CMP_NEQ:                 // generic compares
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
            case OP_RETURN: case OP_RETURN_NUM:                  // returns
            case OP_RETURN_NUM_IMM:                              // incl. numeric immediate
            case OP_RETURN_BOOL:                                 // incl. boolean
            case OP_RETURN_NONE:                                 // incl. implicit none
                break;                                           // always allowed

            case OP_LOAD_NUM: {                                  // only numeric constants
                int idx = inst->operands[1];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) return false;        // out of range
                if (chunk->constants[idx].type != CONST_NUMBER) return false;  // non-numeric
                break;
            }

            case OP_LOAD_CONST: {                                // scalar constants incl. strings
                int idx = inst->operands[1];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) return false;        // out of range
                ConstantType t = chunk->constants[idx].type;     // which kind of scalar?
                if (t != CONST_NUMBER && t != CONST_BOOL &&      // numeric / bool / none are
                    t != CONST_NONE   && t != CONST_STRING) {    // fine; anything else (tables,
                    return false;                                // functions) is rejected
                }
                break;
            }

            case OP_JUMP:                                        // all jumps must stay
            case OP_JUMP_IF_FALSE:                               // within this function
            case OP_JUMP_IF_EQ:     case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:     // imm-jump variants
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
            case OP_JUMP_IF_NONE:                                 // none-test variants
            case OP_JUMP_IF_NOT_NONE:
            case OP_JUMP_MATCH_BOOL:                              // match jumps share the range check
            case OP_JUMP_MATCH_NONE: {
                int target = inst->operands[0];                     // jump target pc
                if (target < start || target > end) return false;   // end is a valid exit
                break;
            }

            case OP_JUMP_MATCH_NUM: {                            // subject compared to a number constant
                int target = inst->operands[0];                  // jump target pc
                if (target < start || target >= end) return false;
                int idx = inst->operands[2];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) return false;
                if (chunk->constants[idx].type != CONST_NUMBER) return false;
                break;
            }
            case OP_JUMP_MATCH_STR: {                            // subject compared to a string constant
                int target = inst->operands[0];                  // jump target pc
                if (target < start || target >= end) return false;
                int idx = inst->operands[2];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) return false;
                if (chunk->constants[idx].type != CONST_STRING) return false;
                break;
            }
            case OP_JUMP_TABLE: {                                // dense integer match dispatch
                int idx = inst->operands[2];                     // constant pool index
                if (!jump_table_in_range(chunk, idx, start, end)) return false;
                break;
            }

            case OP_SWITCH_STR:                                  // perfect-hash string dispatch
            case OP_SWITCH_NUM: {                                // perfect-hash numeric dispatch
                int idx = inst->operands[1];                     // constant pool index
                if (!switch_targets_in_range(chunk, idx, start, end)) return false;
                break;
            }

            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:  // call targets validated
                break;                                       // by the fixpoint pass

            case OP_PUSH_ARG:                                // push arg onto vm args stack
            case OP_CALL_BUILTIN:                            // dispatch to vm builtin handler
            case OP_ASYNC_CALL_BUILTIN:                      // dispatch to async builtin handler
                break;

            default:
                return false;                                // any other op: reject
        }
    }
    return true;
}

// true when the opcode has no structural tie to the interpreter loop
static bool op_is_basic_lowerable(Opcode op) {
    switch (op) {
        case OP_MOVE: case OP_LOAD_CONST: case OP_LOAD_NUM_IMM:    // data movement
        case OP_LOAD_NUM: case OP_LOAD_BOOL: case OP_LOAD_NONE:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:          // arithmetic
        case OP_DIV_IMM: case OP_MOD_IMM:
        case OP_NEG: case OP_INC: case OP_DEC:
        case OP_AND: case OP_OR:
        case OP_CMP_EQ: case OP_CMP_NEQ:                            // comparisons
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_CONCAT:                                             // strings
        case OP_TABLE_GET: case OP_TABLE_GET_CONST:                 // tables
        case OP_TABLE_GET_INT: case OP_TABLE_GET_NUM:
        case OP_TABLE_GET_KEY_STR:
        case OP_TABLE_SET: case OP_TABLE_SET_CONST:
        case OP_TABLE_SET_INT: case OP_TABLE_SET_NUM:
        case OP_TABLE_SET_KEY_STR: case OP_TABLE_APPEND:
        case OP_NEW_TABLE: case OP_LOAD_TABLE:
        case OP_FOR_INIT: case OP_FOR_NEXT: case OP_FOR_NEXT_LOOP:  // loops via helpers
        case OP_TABLE_ITER_INIT: case OP_TABLE_ITER_NEXT:
        case OP_POP_ITER: case OP_POP_TABLE_ITER:
        case OP_LOAD_GLOBAL: case OP_STORE_GLOBAL:                  // globals
        case OP_PUSH_ARG: case OP_CALL_BUILTIN:                     // builtins
        case OP_ASYNC_CALL_BUILTIN:
        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:             // direct calls (fixpoint-checked)
        case OP_CALL:                                               // many-arg call via coroutines
        case OP_ASYNC_CALL:                                         // pending future capture
        case OP_RETURN: case OP_RETURN_NUM: case OP_RETURN_NUM_IMM:  // returns
        case OP_RETURN_BOOL: case OP_RETURN_NONE:
        case OP_JUMP: case OP_JUMP_IF_FALSE:                        // branches
        case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
        case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
        case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
        case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
        case OP_JUMP_IF_NONE: case OP_JUMP_IF_NOT_NONE:
        case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:              // match dispatch
        case OP_JUMP_MATCH_BOOL: case OP_JUMP_MATCH_NONE:
        case OP_JUMP_TABLE: case OP_SWITCH_STR: case OP_SWITCH_NUM:
            return true;                                            // everything above lowers
        default:
            return false;                                           // call/await/async/halt/match stay out
    }
}

// returns the branch target of an opcode, or -1 when it is not a branch
static int branch_target_of(Instruction* inst) {
    switch (inst->opcode) {
        case OP_JUMP: case OP_JUMP_IF_FALSE:                        // plain branches
        case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
        case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
        case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
        case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
        case OP_JUMP_IF_NONE: case OP_JUMP_IF_NOT_NONE:
        case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:
        case OP_JUMP_MATCH_BOOL: case OP_JUMP_MATCH_NONE:
            return inst->operands[0];                               // target in op1
        case OP_FOR_NEXT:                                           // exit address in op2
            return inst->operands[1];
        case OP_FOR_NEXT_LOOP:                                      // body target in op2
            return inst->operands[1];
        case OP_TABLE_ITER_NEXT:                                    // exit address in op3
            return inst->operands[2];
        default:
            return -1;                                              // not a branch
    }
}

// checks if a function can be compiled with the standard-opcode (basic) tier
static bool function_is_basic_compilable(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];                        // first pc of this fn
    int end   = ctx->range_end[func_idx];                          // one past last pc
    if (func_idx == 0) return false;                               // entry fn runs via loops only
    if (start >= end) return false;                                // empty range
    if (chunk->functions[func_idx].arity > JIT_MAX_CALL_ARITY) return false;  // stub covers 3..8; above stays interpreted
    if (chunk->functions[func_idx].is_async) return false;         // coroutines run via the scheduler
    if (chunk->functions[func_idx].max_registers > JIT_MAX_SLOTS) return false;  // frame too big

    for (int pc = start; pc < end; pc++) {                         // scan every instruction
        Instruction* inst = &chunk->code[pc];
        if (!op_is_basic_lowerable(inst->opcode)) return false;    // structural opcode: reject

        int target = branch_target_of(inst);                       // branch target pc
        if (target >= 0) {                                         // this is a branch
            if (target < start || target > end) return false;      // must stay inside the body
            if (inst->opcode == OP_FOR_NEXT_LOOP && target == end) return false;  // back edge needs a body
        }
    }
    return true;                                                   // every op has a lowering
}

// checks whether an instruction writes a value to operands[0]
static bool loop_prefix_op_writes_dest(Opcode op) {
    switch (op) {
        case OP_MOVE:
        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM: case OP_LOAD_BOOL:
        case OP_LOAD_NONE: case OP_LOAD_CONST: case OP_LOAD_GLOBAL:
        case OP_LOAD_TABLE: case OP_NEW_TABLE:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM:
        case OP_NEG: case OP_INC: case OP_DEC:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_AND: case OP_OR:
        case OP_CONCAT:
        case OP_TABLE_GET: case OP_TABLE_GET_NUM: case OP_TABLE_GET_INT:
        case OP_TABLE_GET_KEY_STR: case OP_TABLE_GET_CONST:
        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
        case OP_CALL_BUILTIN: case OP_ASYNC_CALL_BUILTIN:
        case OP_ASYNC_CALL: case OP_AWAIT:
        case OP_FOR_NEXT: case OP_FOR_NEXT_LOOP:
            return true;                                         // destination is operands[0]
        default:
            return false;                                        // no frame destination
    }
}

// simple per-slot type for the cfg type-flow below
typedef enum {
    SP_NUM = 0,   // provably a plain number
    SP_BOOL,      // provably a bool (from loads, cmp, logical ops)
    SP_NONE,      // provably none (from loads)
    SP_STR,       // provably a string (from loads or concat)
    SP_UNK,       // unknown / mixed / tagged
} SPType;

// return-kind bits observed while flowing through a function
#define TF_RET_NUM   0x1   // a provably-numeric return
#define TF_RET_BOOL  0x2   // a provably-bool return
#define TF_RET_NONE  0x4   // a provably-none return
#define TF_RET_OTHER 0x8   // strings, tables, or unprovable slots

// outcome of one type-flow run over a function
typedef struct {
    bool ok;          // no strict-unsafe instruction on any reachable path
    bool saw_return;  // at least one return opcode was reached
    int  ret_kinds;   // OR of the TF_RET_* bits above
} TypeFlowResult;

// meets two slot types at a control-flow join
static SPType sp_meet(SPType a, SPType b) {
    return a == b ? a : SP_UNK;                               // agreeing types survive, mixes collapse
}

// reads a slot's type, treating out-of-range slots as unknown
static SPType tf_get(SPType* s, int slot) {
    return (slot >= 0 && slot < JIT_MAX_SLOTS) ? s[slot] : SP_UNK;   // untracked slots stay unknown
}

// writes a slot's type when it is trackable
static void tf_set(SPType* s, int slot, SPType t) {
    if (slot >= 0 && slot < JIT_MAX_SLOTS) s[slot] = t;              // untracked slots stay unknown
}

// interpreter result type of arithmetic: numbers stay numbers, surely-tagged operands yield none
static SPType tf_arith_type(SPType x, SPType y) {
    if (x == SP_NUM && y == SP_NUM) return SP_NUM;              // plain arithmetic
    if (x == SP_UNK || y == SP_UNK) return SP_UNK;              // maybe numeric at runtime
    return SP_NONE;                                             // tagged operand: interpreter yields none
}

// true when an opcode unconditionally transfers control away (no fallthrough edge)
static bool tf_is_terminator(Opcode op) {
    switch (op) {
        case OP_JUMP:                                           // sole edge is its target
        case OP_RETURN: case OP_RETURN_NUM: case OP_RETURN_NUM_IMM:   // leaves the function
        case OP_RETURN_BOOL: case OP_RETURN_NONE:
        case OP_HALT:
        case OP_SWITCH_STR: case OP_SWITCH_NUM:                 // dispatch ends in an unconditional default jump
            return true;
        default:
            return false;
    }
}

// adds one cfg edge: meets the out-state into the target's in-state
static void tf_edge(BytecodeChunk* chunk, int start, int end, int target,
                    SPType* in, bool* reached, bool* in_wl,
                    int* wl, int* wl_top, SPType* out) {
    if (target < start || target >= end) return;               // exit or out of range: ignore
    int t = target - start;                                    // row index of the target
    if (!reached[t]) {                                         // first arrival seeds the row
        memcpy(in + (size_t)t * JIT_MAX_SLOTS, out, JIT_MAX_SLOTS * sizeof(SPType));
        reached[t] = true;                                     // row now holds the out-state
        if (!in_wl[t]) { in_wl[t] = true; wl[(*wl_top)++] = t; }  // visit it later
        return;
    }
    SPType* row = in + (size_t)t * JIT_MAX_SLOTS;              // target's current in-state
    bool weakened = false;                                     // did any slot get weaker?
    for (int s = 0; s < JIT_MAX_SLOTS; s++)
        if (row[s] != out[s] && sp_meet(row[s], out[s]) != row[s]) {   // meet weakened some slot
            row[s] = sp_meet(row[s], out[s]);                   // keep the weaker type
            weakened = true;                                    // one revisit covers all slots
        }
    if (weakened && !in_wl[t]) {                               // state changed: revisit once
        in_wl[t] = true;
        wl[(*wl_top)++] = t;
    }
}

// runs the per-slot type dataflow over one function's cfg
static void type_flow_analyze(JITContext* ctx, int func_idx, bool params_numeric,
                              const bool* callee_numret, TypeFlowResult* res) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];                     // first pc of this fn
    int end   = ctx->range_end[func_idx];                       // one past last pc
    int arity = chunk->functions[func_idx].arity;               // parameter count
    int n     = end - start;                                    // pcs in the body

    memset(res, 0, sizeof(*res));                               // start from a clean verdict
    res->ok = true;                                             // rejects only accumulate below
    if (n <= 0 || arity > JIT_MAX_SLOTS) {                      // empty body or too many params
        res->ok = false;
        return;
    }

    SPType* in = (SPType*)malloc((size_t)n * JIT_MAX_SLOTS * sizeof(SPType));  // per-pc in-states
    bool*  reached = (bool*)calloc(n, 1);                       // reachability per pc
    bool*  in_wl   = (bool*)calloc(n, 1);                       // pending-in-worklist marks
    int*   wl = (int*)malloc(sizeof(int) * (size_t)n);          // worklist (deduped by in_wl)
    SPType cur[JIT_MAX_SLOTS];                                  // scratch out-state
    if (!in || !reached || !in_wl || !wl) {                     // allocation failed
        free(in); free(reached); free(in_wl); free(wl);
        res->ok = false;
        return;
    }

    int wl_top = 0;                                             // worklist is empty
    for (int s = 0; s < JIT_MAX_SLOTS; s++)
        cur[s] = (params_numeric && s < arity) ? SP_NUM : SP_UNK;   // entry state: params per contract
    memcpy(in, cur, JIT_MAX_SLOTS * sizeof(SPType));            // seed pc 0 (function entry)
    reached[0] = true;
    in_wl[0] = true;
    wl[wl_top++] = 0;

    while (wl_top > 0) {                                        // classic worklist dataflow
        int pi = wl[--wl_top];                                  // pop one pc
        in_wl[pi] = false;                                      // no longer pending
        int pc = start + pi;                                    // absolute pc
        memcpy(cur, in + (size_t)pi * JIT_MAX_SLOTS, JIT_MAX_SLOTS * sizeof(SPType));  // load in-state

        Instruction* inst = &chunk->code[pc];                   // current instruction
        int d = inst->operands[0];                              // destination operand
        int a = inst->operands[1];                              // first source operand
        int b = inst->operands[2];                              // second source operand

        switch (inst->opcode) {
            case OP_MOVE:                                       // reg-to-reg copy keeps the type
                tf_set(cur, d, tf_get(cur, a));
                break;

            case OP_LOAD_NUM_IMM:                               // small int literal
                tf_set(cur, d, SP_NUM);
                break;

            case OP_LOAD_NUM: {                                 // numeric constant
                bool plain = a >= 0 && a < chunk->const_count &&
                             chunk->constants[a].type == CONST_NUMBER &&
                             !isnan(chunk->constants[a].number_value);  // nan constants arrive nan-tagged
                tf_set(cur, d, plain ? SP_NUM : SP_UNK);
                break;
            }

            case OP_LOAD_CONST: {                               // scalar constants by pool type
                if (a < 0 || a >= chunk->const_count) {         // out of range: unknown
                    tf_set(cur, d, SP_UNK);
                    break;
                }
                switch (chunk->constants[a].type) {
                    case CONST_NUMBER:                           // nan numbers arrive nan-tagged
                        tf_set(cur, d, !isnan(chunk->constants[a].number_value) ? SP_NUM : SP_UNK);
                        break;
                    case CONST_BOOL:   tf_set(cur, d, SP_BOOL); break;
                    case CONST_NONE:   tf_set(cur, d, SP_NONE); break;
                    case CONST_STRING: tf_set(cur, d, SP_STR);  break;
                    default:           tf_set(cur, d, SP_UNK);  break;   // tables, functions
                }
                break;
            }

            case OP_LOAD_BOOL:
                tf_set(cur, d, SP_BOOL);
                break;

            case OP_LOAD_NONE:
                tf_set(cur, d, SP_NONE);
                break;

            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
                if (tf_get(cur, a) != SP_NUM || tf_get(cur, b) != SP_NUM)
                    res->ok = false;                            // unchecked emit needs numeric operands
                tf_set(cur, d, tf_arith_type(tf_get(cur, a), tf_get(cur, b)));
                break;

            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
            case OP_NEG: {
                SPType x = tf_get(cur, a);                      // the single value operand
                if (x != SP_NUM) res->ok = false;               // unchecked emit needs numeric operands
                tf_set(cur, d, tf_arith_type(x, SP_NUM));
                break;
            }

            case OP_INC: case OP_DEC: {
                SPType x = tf_get(cur, d);                      // in-place: dest is also the source
                if (x != SP_NUM) res->ok = false;               // unchecked emit needs numeric operands
                tf_set(cur, d, tf_arith_type(x, SP_NUM));
                break;
            }

            case OP_CMP_EQ: case OP_CMP_NEQ:                    // all comparisons
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                if (tf_get(cur, a) != SP_NUM || tf_get(cur, b) != SP_NUM)
                    res->ok = false;                            // ucomisd emit needs numeric operands
                tf_set(cur, d, SP_BOOL);
                break;

            case OP_AND: case OP_OR:                             // bit-0 test works on any known type
                if (tf_get(cur, a) == SP_UNK || tf_get(cur, b) == SP_UNK)
                    res->ok = false;                            // never-assigned bits are garbage
                tf_set(cur, d, SP_BOOL);
                break;

            case OP_JUMP:                                           // unconditional branch
            case OP_HALT:                                           // interpreter stop
                break;                                              // no operands to observe

            case OP_JUMP_IF_FALSE:                               // falsy-bit test
                if (tf_get(cur, a) == SP_UNK) res->ok = false;  // never-assigned bits are garbage
                break;

            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:            // ucomisd-based branches
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
                if (tf_get(cur, a) != SP_NUM || tf_get(cur, b) != SP_NUM)
                    res->ok = false;                            // raw equality needs numeric operands
                break;

            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:             // raw double compares
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
                if (tf_get(cur, a) == SP_UNK || tf_get(cur, b) == SP_UNK)
                    res->ok = false;                            // never-assigned bits are garbage
                break;

            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:    // immediate compares
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
            case OP_JUMP_IF_NONE: case OP_JUMP_IF_NOT_NONE:     // none-bit compares
                if (tf_get(cur, a) == SP_UNK) res->ok = false;  // never-assigned bits are garbage
                break;

            case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:     // match dispatch
            case OP_JUMP_MATCH_BOOL: case OP_JUMP_MATCH_NONE:
                if (tf_get(cur, a) == SP_UNK) res->ok = false;  // never-assigned bits are garbage
                break;

            case OP_JUMP_TABLE:                                 // dense integer dispatch
                if (tf_get(cur, d) == SP_UNK) res->ok = false;  // subject is operands[0] here
                break;

            case OP_SWITCH_STR: case OP_SWITCH_NUM:             // perfect-hash dispatch
                if (tf_get(cur, d) == SP_UNK) res->ok = false;  // subject is operands[0] here
                break;

            case OP_CALL_0:                                     // calls
                tf_set(cur, d, (callee_numret && a >= 0 && a < ctx->func_count &&
                                callee_numret[a]) ? SP_NUM : SP_UNK);   // numeric callee contract
                break;

            case OP_CALL_1:
                if (tf_get(cur, b) != SP_NUM) res->ok = false;   // raw arg must be a plain number
                tf_set(cur, d, (callee_numret && a >= 0 && a < ctx->func_count &&
                                callee_numret[a]) ? SP_NUM : SP_UNK);   // numeric callee contract
                break;

            case OP_CALL_2:
                if (tf_get(cur, b) != SP_NUM || tf_get(cur, b + 1) != SP_NUM)
                    res->ok = false;                            // raw args must be plain numbers
                tf_set(cur, d, (callee_numret && a >= 0 && a < ctx->func_count &&
                                callee_numret[a]) ? SP_NUM : SP_UNK);   // numeric callee contract
                break;

            case OP_CONCAT:                                     // string building
                tf_set(cur, d, SP_STR);
                res->ok = false;                                // helper-lowered: not strict
                break;

            case OP_RETURN: {                                   // returns the slot's value
                res->saw_return = true;
                switch (tf_get(cur, d)) {
                    case SP_NUM:  res->ret_kinds |= TF_RET_NUM;  break;
                    case SP_BOOL: res->ret_kinds |= TF_RET_BOOL; break;
                    case SP_NONE: res->ret_kinds |= TF_RET_NONE; break;
                    default:      res->ret_kinds |= TF_RET_OTHER; break;   // strings or unprovable
                }
                break;
            }

            case OP_RETURN_NUM: case OP_RETURN_NUM_IMM:         // compiler-proven numeric return
                res->saw_return = true;
                res->ret_kinds |= TF_RET_NUM;
                break;

            case OP_RETURN_BOOL:                                 // compiler-proven bool return
                res->saw_return = true;
                res->ret_kinds |= TF_RET_BOOL;
                break;

            case OP_RETURN_NONE:                                 // compiler-proven none return
                res->saw_return = true;
                res->ret_kinds |= TF_RET_NONE;
                break;

            default: {                                          // helper-lowered ops stay checked
                if (loop_prefix_op_writes_dest(inst->opcode))   // only writers change a slot
                    tf_set(cur, d, SP_UNK);                      // helper results are unprovable
                if (inst->opcode != OP_PUSH_ARG &&              // arg pushes read, never write
                    inst->opcode != OP_STORE_GLOBAL &&          // global writes read the value slot
                    inst->opcode != OP_TABLE_SET &&
                    inst->opcode != OP_TABLE_SET_NUM &&
                    inst->opcode != OP_TABLE_SET_INT &&
                    inst->opcode != OP_TABLE_SET_KEY_STR &&
                    inst->opcode != OP_TABLE_SET_CONST &&
                    inst->opcode != OP_TABLE_APPEND &&
                    inst->opcode != OP_POP_ITER &&
                    inst->opcode != OP_POP_TABLE_ITER &&
                    inst->opcode != OP_FOR_INIT &&
                    inst->opcode != OP_TABLE_ITER_INIT)
                    res->ok = false;                            // anything else: strict tier rejects
                break;
            }
        }

        // cfg edges out of this pc
        int target = branch_target_of(inst);                    // classic branch targets
        if (target >= 0) {                                      // this is a branch
            tf_edge(chunk, start, end, target, in, reached, in_wl, wl, &wl_top, cur);
            if (!tf_is_terminator(inst->opcode))                // conditional: fallthrough too
                tf_edge(chunk, start, end, pc + 1, in, reached, in_wl, wl, &wl_top, cur);
        } else if (inst->opcode == OP_JUMP_MATCH_NUM ||         // match jumps: target + fallthrough
                   inst->opcode == OP_JUMP_MATCH_STR ||
                   inst->opcode == OP_JUMP_MATCH_BOOL ||
                   inst->opcode == OP_JUMP_MATCH_NONE) {
            tf_edge(chunk, start, end, d, in, reached, in_wl, wl, &wl_top, cur);
            tf_edge(chunk, start, end, pc + 1, in, reached, in_wl, wl, &wl_top, cur);
        } else if (inst->opcode == OP_JUMP_TABLE) {             // dense dispatch: all targets + miss
            int idx = inst->operands[2];                        // constant pool index
            if (idx >= 0 && idx < chunk->const_count &&
                chunk->constants[idx].type == CONST_JUMP_TABLE) {
                for (int i = 0; i < chunk->constants[idx].jump_table.count; i++)
                    tf_edge(chunk, start, end, chunk->constants[idx].jump_table.addresses[i],
                            in, reached, in_wl, wl, &wl_top, cur);     // one edge per case
            }
            tf_edge(chunk, start, end, pc + 1, in, reached, in_wl, wl, &wl_top, cur);  // miss falls through
        } else if (inst->opcode == OP_SWITCH_STR) {             // hash dispatch: cases + default
            int idx = inst->operands[1];
            if (idx >= 0 && idx < chunk->const_count &&
                chunk->constants[idx].type == CONST_STR_SWITCH) {
                for (int i = 0; i <= chunk->constants[idx].str_switch.mask; i++) {
                    int tgt = chunk->constants[idx].str_switch.entries[i].target;
                    if (tgt >= 0)                               // empty slot: skip
                        tf_edge(chunk, start, end, tgt, in, reached, in_wl, wl, &wl_top, cur);
                }
                tf_edge(chunk, start, end, chunk->constants[idx].str_switch.default_pc,
                        in, reached, in_wl, wl, &wl_top, cur);         // default arm
            }
        } else if (inst->opcode == OP_SWITCH_NUM) {
            int idx = inst->operands[1];
            if (idx >= 0 && idx < chunk->const_count &&
                chunk->constants[idx].type == CONST_NUM_SWITCH) {
                for (int i = 0; i <= chunk->constants[idx].num_switch.mask; i++) {
                    int tgt = chunk->constants[idx].num_switch.entries[i].target;
                    if (tgt >= 0)
                        tf_edge(chunk, start, end, tgt, in, reached, in_wl, wl, &wl_top, cur);
                }
                tf_edge(chunk, start, end, chunk->constants[idx].num_switch.default_pc,
                        in, reached, in_wl, wl, &wl_top, cur);
            }
        } else if (!tf_is_terminator(inst->opcode)) {           // plain fallthrough
            tf_edge(chunk, start, end, pc + 1, in, reached, in_wl, wl, &wl_top, cur);
        }
    }

    free(in);                                                   // flow buffers are per-run
    free(reached);
    free(in_wl);
    free(wl);
}

// infers how a function returns its value; false on mixed or missing kinds
static bool infer_return_type(JITContext* ctx, int func_idx, JitReturnType* out) {
    TypeFlowResult res;
    type_flow_analyze(ctx, func_idx, false, NULL, &res);        // params unproven, callees unknown
    if (!res.saw_return) return false;                          // no return reached at all
    if (res.ret_kinds & TF_RET_OTHER) {                         // unprovable value: runtime check
        *out = JIT_RET_ANY;
        return true;
    }
    int kinds = ((res.ret_kinds & TF_RET_NUM)  ? 1 : 0) +       // count distinct known kinds
                ((res.ret_kinds & TF_RET_BOOL) ? 1 : 0) +
                ((res.ret_kinds & TF_RET_NONE) ? 1 : 0);
    if (kinds != 1) return false;                               // mixed return -> reject
    *out = (res.ret_kinds & TF_RET_NUM)  ? JIT_RET_NUMBER :     // single kind wins
           (res.ret_kinds & TF_RET_BOOL) ? JIT_RET_BOOL
                                         : JIT_RET_NONE;
    return true;
}

// infers register types along a prefix & across completed numeric loops
static bool infer_loop_entry_types(JITContext* ctx, int function_start,
                                   int function_end, int entry_pc,
                                   int back_edge_pc, SPType* types) {
    BytecodeChunk* chunk = ctx->chunk;
    for (int i = 0; i < JIT_MAX_SLOTS; i++) types[i] = SP_UNK;

    // a later branch must not enter the loop body while bypassing this prefix
    for (int scan = back_edge_pc + 1; scan < function_end; scan++) {
        Instruction* inst = &chunk->code[scan];
        int target = -1;
        switch (inst->opcode) {
            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
            case OP_JUMP_IF_NONE: case OP_JUMP_IF_NOT_NONE:
                target = inst->operands[0];
                break;
            case OP_FOR_NEXT_LOOP:
                target = inst->operands[1];
                break;
            case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:
            case OP_JUMP_MATCH_BOOL: case OP_JUMP_MATCH_NONE:
                return false;                               // switch targets need full control-flow analysis
            default:
                break;
        }
        if (target >= function_start && target <= back_edge_pc)
            return false;                                   // external edge can bypass a proven assignment
    }

    int pc = function_start;
    while (pc < entry_pc) {
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];
        int a = inst->operands[1];

        switch (inst->opcode) {
            case OP_JUMP: {
                int target = inst->operands[0];
                if (target <= pc || target > entry_pc || target >= chunk->code_count)
                    return false;                           // only follow a forward jump to this prefix
                pc = target;
                continue;
            }

            case OP_MOVE:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = (a >= 0 && a < JIT_MAX_SLOTS) ? types[a] : SP_UNK;
                break;

            case OP_LOAD_NUM_IMM:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = SP_NUM;
                break;

            case OP_LOAD_NUM: {
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                int idx = inst->operands[1];
                types[d] = (idx >= 0 && idx < chunk->const_count &&
                            chunk->constants[idx].type == CONST_NUMBER &&
                            !isnan(chunk->constants[idx].number_value)) ? SP_NUM : SP_UNK;
                break;
            }

            case OP_LOAD_CONST: {
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                int idx = inst->operands[1];
                types[d] = (idx >= 0 && idx < chunk->const_count &&
                            chunk->constants[idx].type == CONST_NUMBER &&
                            !isnan(chunk->constants[idx].number_value)) ? SP_NUM : SP_UNK;
                break;
            }

            case OP_LOAD_BOOL:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = SP_BOOL;
                break;

            case OP_LOAD_NONE:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = SP_NONE;
                break;

            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM: case OP_NEG:
            case OP_INC: case OP_DEC:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = SP_UNK;                              // arithmetic outputs are not trusted by this entry proof
                break;

            case OP_CMP_EQ: case OP_CMP_NEQ:
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
            case OP_AND: case OP_OR:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = SP_BOOL;
                break;

            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
            case OP_CALL_BUILTIN: case OP_ASYNC_CALL_BUILTIN:
            case OP_ASYNC_CALL: case OP_AWAIT:
                for (int i = 0; i < JIT_MAX_SLOTS; i++) types[i] = SP_UNK;
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = SP_UNK;                              // calls may return tagged values or mutate vm state
                break;

            case OP_FOR_NEXT:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                if (types[d] != SP_NUM) types[d] = SP_UNK;      // zero-iteration exit preserves the prior value
                break;

            case OP_FOR_NEXT_LOOP: {
                int body_start = inst->operands[1];             // body target for this loop
                if (d < 0 || d >= JIT_MAX_SLOTS ||
                    body_start < function_start || body_start >= pc) return false;
                for (int body_pc = body_start; body_pc < pc; body_pc++) {
                    Instruction* body_inst = &chunk->code[body_pc];
                    if (!loop_prefix_op_writes_dest(body_inst->opcode)) continue;
                    int body_dest = body_inst->operands[0];
                    if (body_dest < 0 || body_dest >= JIT_MAX_SLOTS) return false;
                    types[body_dest] = SP_UNK;                  // zero or more iterations merge here
                }
                break;                                          // retain the counter type unless the body overwrote it
            }

            case OP_LOAD_GLOBAL: case OP_LOAD_TABLE: case OP_NEW_TABLE:
            case OP_CONCAT:
            case OP_TABLE_GET: case OP_TABLE_GET_NUM: case OP_TABLE_GET_INT:
            case OP_TABLE_GET_KEY_STR: case OP_TABLE_GET_CONST:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                types[d] = SP_UNK;
                break;

            case OP_PUSH_ARG: case OP_FOR_INIT: case OP_STORE_GLOBAL:
            case OP_POP_ITER: case OP_POP_TABLE_ITER: case OP_TABLE_ITER_INIT:
            case OP_TABLE_APPEND: case OP_TABLE_SET: case OP_TABLE_SET_NUM:
            case OP_TABLE_SET_INT: case OP_TABLE_SET_KEY_STR: case OP_TABLE_SET_CONST:
                break;                                          // these instructions do not replace a register

            default:
                return false;                                   // branches or unsupported writes need runtime guards
        }
        pc++;
    }
    return true;
}

// checks if an opcode can start a native loop
static bool is_loop_entry_op(Opcode op) {
    return op == OP_FOR_NEXT ||
           op == OP_TABLE_ITER_NEXT ||
           (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_NOT_NONE); // widened: includes imm and none variants
}

// walks the loop body, marks read and written slots, and collects table uses
static void analyze_loop_regs(JITContext* ctx, JitLoopInfo* info,
                              int function_start, int function_end) {
    BytecodeChunk* chunk = ctx->chunk;
    uint64_t live_in_mask = 0, written_mask = 0;                 // accumulated per-slot bitmasks
    uint64_t helper_live_reads = 0, fast_live_reads = 0;         // live-in reads split by path

    info->table.used               = false;                      // no table op seen yet
    info->table.slot               = -1;                         // table register (unknown yet)
    info->table.min_count          = 0;                          // required array_count
    info->table.max_idx            = 0;                          // required array_capacity
    info->table.written            = false;                      // has OP_TABLE_SET_INT
    info->table.indexed_by_counter = false;                      // t[i] with i == loop counter
    info->ref_writes               = 0;                          // slots whose value is refcounted
    info->touches_tables           = false;                      // any table op at all
    info->str_slots                = 0;                          // slots that receive string Values
    info->has_basic_ops            = false;                      // no helper-lowered op yet
    info->owned_writes             = 0;                          // no owned-ref writers yet
    info->globals_via_helper       = false;                      // globals stay on the fast path
    memset(info->live_in_kind, 0, sizeof(info->live_in_kind));   // all slots default to NUM

    for (int pc = info->entry_pc; pc <= info->back_edge_pc; pc++) {
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // destination register
        int a = inst->operands[1];                               // first source register
        int b = inst->operands[2];                               // second source register
        uint64_t pc_reads = 0, pc_writes = 0;                    // this instruction's masks
        bool pc_helper_reads = false;                            // reads go through a helper
        switch (inst->opcode) {
            case OP_MOVE: case OP_NEG:                           // reads a, writes d
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
            case OP_INC: case OP_DEC:                            // read-modify-write d
                if (d >= 0 && d < 64) pc_reads |= pc_writes |= 1ULL << d;
                break;
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_CMP_EQ: case OP_CMP_NEQ:
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;    // left operand
                if (b >= 0 && b < 64) pc_reads  |= 1ULL << b;    // right operand
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;    // result
                break;
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:    // d = a OP imm
            case OP_DIV_IMM: case OP_MOD_IMM:                     // only a is a register
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;    // left operand (a)
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;    // result (d)
                break;
            case OP_AND: case OP_OR:                              // logical ops read a,b
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;
                if (b >= 0 && b < 64) pc_reads  |= 1ULL << b;
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
            case OP_LOAD_CONST: {                                 // writes d only
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                int idx = inst->operands[1];                     // constant pool index
                if (d >= 0 && d < 64 && idx >= 0 && idx < chunk->const_count &&
                    chunk->constants[idx].type == CONST_STRING)  // string literal
                    info->str_slots |= 1ULL << d;                // string value in d
                break;
            }
            case OP_PUSH_ARG:                                     // reads d (source value)
                if (d >= 0 && d < 64) pc_reads |= 1ULL << d;
                break;
            case OP_CALL_BUILTIN: case OP_ASYNC_CALL_BUILTIN:     // writes d (result)
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:              // writes d only
            case OP_LOAD_BOOL: case OP_LOAD_NONE:                // writes d only
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
            case OP_JUMP_IF_FALSE:                               // reads cond register only
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;
                break;
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;     // left operand
                if (b >= 0 && b < 64) pc_reads |= 1ULL << b;     // right operand
                break;
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:     // imm-jump: only left is a register
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;     // left operand
                break;
            case OP_FOR_NEXT:                                    // counter and bounds updated by vm
                if (d >= 0 && d < 64) pc_reads |= pc_writes |= 1ULL << d;
                if (info->for_end_reg  >= 0 && info->for_end_reg  < 64) pc_reads |= 1ULL << info->for_end_reg;
                if (info->for_step_reg >= 0 && info->for_step_reg < 64) pc_reads |= 1ULL << info->for_step_reg;
                break;
            case OP_CALL_0:                                      // no args, writes d
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
            case OP_CALL_1:                                      // arg in b, writes d
                if (b >= 0 && b < 64) pc_reads  |= 1ULL << b;
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
            case OP_CALL_2:                                      // args in b, b+1, writes d
                if (b >= 0 && b < 64)         pc_reads |= 1ULL << b;
                if (b + 1 >= 0 && b + 1 < 64) pc_reads |= 1ULL << (b + 1);
                if (d >= 0 && d < 64)         pc_writes |= 1ULL << d;
                break;
            case OP_TABLE_ITER_NEXT:                             // element written each iteration
                if (d >= 0 && d < 64) {
                    pc_writes |= 1ULL << d;
                    info->ref_writes |= 1ULL << d;               // element may be heap-allocated
                }
                info->table.used     = true;                     // marks loop as table-touching
                info->touches_tables = true;
                break;
            case OP_LOAD_GLOBAL:                                 // d = dest, a = global idx
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                info->uses_globals = true;
                break;
            case OP_STORE_GLOBAL:                                // d = src, a = global idx
                if (d >= 0 && d < 64) pc_reads |= 1ULL << d;
                info->uses_globals = true;
                break;
            case OP_TABLE_GET_KEY_STR:                           // d = tbl["prefix" .. num]
                if (d >= 0 && d < 64) {
                    pc_writes |= 1ULL << d;
                    info->ref_writes |= 1ULL << d;               // value may be heap-allocated
                }
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;     // table slot read
                {
                    int num_reg = b & 0xFFFF;                    // number register
                    if (num_reg >= 0 && num_reg < 64) pc_reads |= 1ULL << num_reg;
                }
                info->table.used     = true;
                info->touches_tables = true;
                if (a >= 0 && a < JIT_MAX_SLOTS) info->live_in_kind[a] = JIT_SLOT_TABLE_ANY;
                if (info->table.slot < 0) info->table.slot = a;
                if (info->table.min_count < 1) info->table.min_count = 1;
                break;
            case OP_TABLE_SET_KEY_STR:                           // tbl["prefix" .. num] = val
                if (d >= 0 && d < 64) pc_reads |= 1ULL << d;     // table slot read (operands[0])
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;     // value slot read (operands[1])
                {
                    int num_reg = b & 0xFFFF;                    // number register (low half of packed)
                    if (num_reg >= 0 && num_reg < 64) pc_reads |= 1ULL << num_reg;
                }
                info->table.used     = true;
                info->touches_tables = true;
                info->table.written  = true;
                if (d >= 0 && d < JIT_MAX_SLOTS) info->live_in_kind[d] = JIT_SLOT_TABLE_ANY;
                if (info->table.slot < 0) info->table.slot = d;
                if (info->table.min_count < 1) info->table.min_count = 1;
                break;
            case OP_TABLE_GET_NUM:                               // same register pattern as GET
            case OP_TABLE_GET:
                // d = dest, a = table reg, b = key reg
                if (d >= 0 && d < 64) {
                    pc_writes |= 1ULL << d;
                    info->ref_writes |= 1ULL << d;               // value may be heap-allocated
                }
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;    // table slot read
                if (b >= 0 && b < 64) pc_reads  |= 1ULL << b;    // key slot read
                info->table.used = true;
                info->touches_tables = true;
                if (a >= 0 && a < JIT_MAX_SLOTS) info->live_in_kind[a] = JIT_SLOT_TABLE;
                if (info->table.slot < 0) info->table.slot = a;  // first table slot seen
                if (info->table.min_count < 1) info->table.min_count = 1;  // array_part must be non-null
                if (b == info->for_var_reg) info->table.indexed_by_counter = true;
                break;
            case OP_TABLE_GET_INT:
                if (d >= 0 && d < 64) {
                    pc_writes |= 1ULL << d;
                    info->ref_writes |= 1ULL << d;               // value may be heap-allocated
                }
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;    // table slot read
                info->table.used = true;
                info->touches_tables = true;
                if (a >= 0 && a < JIT_MAX_SLOTS) info->live_in_kind[a] = JIT_SLOT_TABLE;
                if (info->table.slot < 0) info->table.slot = a;  // first table slot seen
                if (info->table.min_count < 1) info->table.min_count = 1;
                if (b > info->table.min_count) info->table.min_count = b;  // track max index
                break;
            case OP_TABLE_SET_NUM:                               // same register pattern as SET
            case OP_TABLE_SET:
                // d = table, a = key, b = value
                if (d >= 0 && d < 64) pc_reads |= 1ULL << d;     // table slot read
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;     // key slot read
                if (b >= 0 && b < 64) pc_reads |= 1ULL << b;     // value slot read
                info->table.used     = true;
                info->touches_tables = true;
                info->table.written  = true;
                if (d >= 0 && d < JIT_MAX_SLOTS) info->live_in_kind[d] = JIT_SLOT_TABLE;
                if (info->table.slot < 0) info->table.slot = d;  // first table slot seen
                if (info->table.min_count < 1) info->table.min_count = 1;
                if (a == info->for_var_reg) info->table.indexed_by_counter = true;
                break;
            case OP_TABLE_SET_INT:
                if (d >= 0 && d < 64) pc_reads |= 1ULL << d;     // table slot read
                if (b >= 0 && b < 64) pc_reads |= 1ULL << b;     // value slot read
                info->table.used = true;
                info->touches_tables = true;
                info->table.written = true;                      // guard must check array_capacity
                if (d >= 0 && d < JIT_MAX_SLOTS) info->live_in_kind[d] = JIT_SLOT_TABLE;
                if (info->table.slot < 0) info->table.slot = d;  // first table slot seen
                if (info->table.min_count < 1) info->table.min_count = 1;
                if (a > info->table.max_idx) info->table.max_idx = a;  // track max index
                break;
            case OP_TABLE_GET_CONST:
                // d = dest, a = table reg, b = key pool index
                if (d >= 0 && d < 64) {
                    pc_writes |= 1ULL << d;
                    info->ref_writes |= 1ULL << d;               // value may be heap-allocated
                }
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;     // table slot read
                info->table.used     = true;
                info->touches_tables = true;
                if (a >= 0 && a < JIT_MAX_SLOTS) info->live_in_kind[a] = JIT_SLOT_TABLE_ANY;
                if (info->table.slot < 0) info->table.slot = a;
                if (info->table.min_count < 1) info->table.min_count = 1;
                break;
            case OP_TABLE_SET_CONST:
                // d = table, a = key pool index, b = value reg
                if (d >= 0 && d < 64) pc_reads |= 1ULL << d;     // table slot read
                if (b >= 0 && b < 64) pc_reads |= 1ULL << b;     // value slot read
                info->table.used     = true;
                info->touches_tables = true;
                info->table.written  = true;
                if (d >= 0 && d < JIT_MAX_SLOTS) info->live_in_kind[d] = JIT_SLOT_TABLE_ANY;
                if (info->table.slot < 0) info->table.slot = d;
                if (info->table.min_count < 1) info->table.min_count = 1;
                break;
            case OP_JUMP:
                break;                                           // no slots touched
            case OP_CONCAT:                                      // d = a .. b via helper
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;    // left operand
                if (b >= 0 && b < 64) pc_reads  |= 1ULL << b;    // right operand
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;    // fresh string lands in d
                if (d >= 0 && d < 64) info->str_slots |= 1ULL << d;  // string value in d
                info->has_basic_ops = true;                      // helper-lowered op present
                if (d >= 0 && d < 64) info->owned_writes |= 1ULL << d;  // slot owns the new ref
                pc_helper_reads = true;                          // operands checked at runtime
                break;
            case OP_NEW_TABLE:                                   // d = {} via helper
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;    // fresh table lands in d
                info->has_basic_ops = true;                      // helper-lowered op present
                if (d >= 0 && d < 64) info->owned_writes |= 1ULL << d;  // slot owns the new ref
                break;
            case OP_LOAD_TABLE:                                  // d = literal via helper
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;    // fresh table lands in d
                info->has_basic_ops = true;                      // helper-lowered op present
                if (d >= 0 && d < 64) info->owned_writes |= 1ULL << d;  // slot owns the new ref
                break;
            case OP_TABLE_APPEND:                                // tbl[d] += a via helper
                if (d >= 0 && d < 64) pc_reads  |= 1ULL << d;    // table slot read
                if (a >= 0 && a < 64) pc_reads  |= 1ULL << a;    // value slot read
                info->has_basic_ops = true;                      // helper-lowered op present
                pc_helper_reads = true;                          // operands checked at runtime
                break;
            case OP_CALL:                                        // dst = fn(args) via coroutine
            case OP_ASYNC_CALL:                                  // dst = pending future
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;    // result lands in d
                info->has_basic_ops = true;                      // helper-lowered op present
                if (d >= 0 && d < 64) info->owned_writes |= 1ULL << d;  // result transfers its ref
                break;
            default:
                break;
        }
        live_in_mask |= pc_reads & ~written_mask;                // read before first write → live-in
        if (pc_helper_reads) helper_live_reads |= pc_reads & ~written_mask;  // helper-checked read
        else                 fast_live_reads   |= pc_reads & ~written_mask;  // fast-path read
        written_mask |= pc_writes;                               // slot has been produced
    }
    info->live_in  = live_in_mask;                               // slots read before first write
    info->live_out = written_mask;                               // slots produced inside the loop
    info->table.volatile_slot =                                   // primary table recreated per
        info->table.used && info->table.slot >= 0 &&              // iteration: pointer caches must
        (written_mask & (1ULL << info->table.slot)) != 0;         // not pin its array_part

    // when rbx is reserved for a table's array_part, globals must go through
    // helpers instead of the inline rbx-relative fast path
    info->globals_via_helper = info->uses_globals &&
                               (info->table.used || info->has_basic_ops);

    // slots only ever read by helper-lowered ops need no numeric entry guard:
    // the helpers perform the interpreter's own type dispatch at runtime
    uint64_t any_mask = helper_live_reads & ~fast_live_reads;     // helper-only live-in reads
    while (any_mask) {                                           // downgrade pure-helper reads
        int s = __builtin_ctzll(any_mask);
        any_mask &= any_mask - 1;
        if (s < 64 && info->live_in_kind[s] == JIT_SLOT_NUM)
            info->live_in_kind[s] = JIT_SLOT_ANY;                // any type is acceptable
    }

    SPType entry_types[JIT_MAX_SLOTS];                            // types before the loop starts
    if (infer_loop_entry_types(ctx, function_start, function_end,
                               info->entry_pc, info->back_edge_pc, entry_types)) {
        uint64_t numeric_inputs = info->live_in;                  // values are checked at loop entry, before body writes
        while (numeric_inputs) {
            int slot = __builtin_ctzll(numeric_inputs);
            numeric_inputs &= numeric_inputs - 1;
            if (slot >= JIT_MAX_SLOTS) continue;
            if (info->live_in_kind[slot] != JIT_SLOT_NUM) continue; // keep table guards intact
            if (entry_types[slot] == SP_NUM) {
                info->live_in_kind[slot] = JIT_SLOT_NUM_PROVEN;    // proven numeric entry value needs no guard
            }
        }
    }
}

// registers a loop and runs the slot analysis on its body
static void add_loop(JITContext* ctx, int function_start, int function_end,
                     int entry, int back_edge,
                     int exit_pc, JitLoopKind kind, int for_var_reg, int for_end_reg,
                     int for_step_reg, int step_sign, int step_value, int nregs,
                     int init_table_reg) {
    for (int i = 0; i < ctx->loop_count; i++) {                   // one record per bytecode loop
        if (ctx->loops[i].entry_pc == entry &&
            ctx->loops[i].back_edge_pc == back_edge) return;
    }

    if (ctx->loop_count >= ctx->loop_capacity) {                 // grow loop array if needed
        int new_cap = ctx->loop_capacity == 0 ? 8 : ctx->loop_capacity * 2;
        JitLoopInfo* new_arr = (JitLoopInfo*)realloc(ctx->loops, sizeof(JitLoopInfo) * new_cap);
        if (!new_arr) return;                                    // allocation failed, skip this loop
        ctx->loops = new_arr;
        ctx->loop_capacity = new_cap;
    }
    JitLoopInfo* info = &ctx->loops[ctx->loop_count++];
    memset(info, 0, sizeof(*info));                              // zero-init before filling fields
    info->entry_pc     = entry;                                  // first pc of loop body
    info->back_edge_pc = back_edge;                              // JUMP returning to entry_pc
    info->exit_pc      = exit_pc;                                // pc reached after loop exits
    info->kind         = kind;                                   // numeric-for / condition / table-iter
    info->for_var_reg  = for_var_reg;                            // loop counter slot (-1 for non-for)
    info->for_end_reg  = for_end_reg;                            // end bound slot (-1 for non-for)
    info->for_step_reg = for_step_reg;                           // step slot (-1 for non-for)
    info->for_end_cache_slot  = -1;                              // assigned by the numeric-loop emitter
    info->for_step_cache_slot = -1;                              // assigned by the numeric-loop emitter
    info->step_sign        = step_sign;                          // static step sign, 0 if unknown
    info->for_step_value   = step_value;                         // exact literal step value
    info->for_var_gpr      = -1;                                 // no gpr-counter yet
    info->for_var_gpr_slot = -1;                                 // no save slot yet
    info->nregs        = nregs;                                  // function frame size
    info->table.slot   = (init_table_reg >= 0) ? init_table_reg : -1;
    info->table.used   = (init_table_reg >= 0);                  // mark as touched
    analyze_loop_regs(ctx, info, function_start, function_end); // fill live_in/out and table use
}

// checks whether a table-iter body uses no table ops beyond the loop iterator
static bool table_iter_body_is_clean(BytecodeChunk* chunk, int entry, int back_edge) {
    for (int pc = entry + 1; pc < back_edge; pc++) {               // skip entry instr itself
        Opcode op = chunk->code[pc].opcode;
        switch (op) {
            // rejects — rbx holds the iterated table's array_part for fast paths
            case OP_TABLE_GET_INT: case OP_TABLE_SET_INT:          // rbx would hold another table
            // rejects — nested iteration needs internal branches
            case OP_TABLE_ITER_NEXT: case OP_TABLE_ITER_INIT:
                return false;
            default:
                break;                                             // helpers keep rbx/r12/r14/r15 intact
        }
    }
    return true;
}

// validates refcount bookkeeping for a loop with helper-lowered ops
static bool basic_loop_is_consistent(JITContext* ctx, JitLoopInfo* info) {
    if (!info->has_basic_ops) return true;                         // nothing to validate
    BytecodeChunk* chunk = ctx->chunk;

    for (int pc = info->entry_pc; pc <= info->back_edge_pc; pc++) { // move dests join the owned set
        Instruction* inst = &chunk->code[pc];
        if (inst->opcode != OP_MOVE) continue;                     // only MOVE propagates ownership
        int d = inst->operands[0];
        if (d >= 0 && d < 64) info->owned_writes |= 1ULL << d;    // helper does refcounted stores
    }

    for (int pc = info->entry_pc; pc <= info->back_edge_pc; pc++) { // check every writer
        Instruction* inst = &chunk->code[pc];
        if (branch_target_of(inst) >= 0) continue;                 // control flow: operands are pcs
        int d = inst->operands[0];
        if (d < 0 || d >= 64) continue;                            // no frame destination
        if (!(info->owned_writes & (1ULL << d))) continue;         // not an owned slot

        switch (inst->opcode) {                                    // who writes this slot?
            case OP_MOVE:                                          // refcount-aware helper
            case OP_CONCAT: case OP_NEW_TABLE: case OP_LOAD_TABLE: // owned-producing helpers
            case OP_CALL: case OP_ASYNC_CALL:                      // results transfer ownership
                continue;                                          // these writers are fine
            case OP_TABLE_SET: case OP_TABLE_SET_INT:              // table register is only read;
            case OP_TABLE_SET_NUM: case OP_TABLE_SET_CONST:        // mutation happens inside the
            case OP_TABLE_SET_KEY_STR: case OP_TABLE_APPEND:      // table object itself
            case OP_STORE_GLOBAL: case OP_TABLE_ITER_INIT:         // source / table registers
            case OP_PUSH_ARG: case OP_FOR_INIT:                    // are reads, not slot writes
            case OP_FOR_NEXT: case OP_FOR_NEXT_LOOP:               // counter slots stay numeric
            case OP_SWITCH_STR: case OP_SWITCH_NUM:                // dispatch subject is a read
                continue;                                          // not a writer of the slot
            default:
                return false;                                      // raw store would leak the owned ref
        }
    }

    if (info->owned_writes & info->ref_writes) return false;       // owned and borrowed views clash
    return true;                                                   // bookkeeping is consistent
}

// checks whether a numeric-for body uses only numeric table access (no string keys)
static bool body_only_uses_counter_index(BytecodeChunk* chunk, int entry,
                                         int back_edge, int for_var_reg) {
    (void)chunk; (void)entry; (void)back_edge; (void)for_var_reg;   // no longer restrictive
    return true;                                             // GET_CONST/SET_CONST now JIT-able
}

// scans one function for numeric loops and table loops
static void detect_loops_in_function(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];
    int end   = ctx->range_end[func_idx];
    int nregs = chunk->functions[func_idx].max_registers;
    if (nregs < 1) nregs = 1;                                    // at least one slot
    if (nregs > JIT_MAX_SLOTS - 2) return;                       // too big for the slot cache

    for (int pc = start; pc < end; pc++) {                       // scan for back edges
        if (func_idx == 0) {                                     // entry range also spans declared functions
            bool inside_function = false;
            for (int fi = 1; fi < ctx->func_count; fi++) {
                if (pc >= ctx->range_start[fi] && pc < ctx->range_end[fi]) {
                    inside_function = true;
                    break;
                }
            }
            if (inside_function) continue;                      // that function gets its own loop scan
        }

        Opcode loop_op = chunk->code[pc].opcode;                 // candidate back-edge opcode
        int entry = -1;                                          // loop entry pc
        if (loop_op == OP_JUMP) {                                // classic JUMP back edge
            entry = chunk->code[pc].operands[0];
            if (entry >= pc || entry < start) continue;          // must be backward and in-range
        } else if (loop_op == OP_FOR_NEXT_LOOP) {                // loop-inverted back edge
            int body_target = chunk->code[pc].operands[1];       // body start (target on success)
            if (body_target >= pc || body_target < start) continue;
            if (body_target - 1 < start) continue;
            if (chunk->code[body_target - 1].opcode != OP_FOR_NEXT) continue;
            entry = body_target - 1;
        } else {
            continue;                                            // not a back edge
        }

        Opcode entry_op = chunk->code[entry].opcode;
        int exit_pc = -1;
        int for_var_reg = -1, for_end_reg = -1, for_step_reg = -1, step_sign = 0;
        int step_value = 0;                                  // literal step value, 0 if unknown
        int init_table_reg = -1;                             // table-iter: slot of iterated table
        JitLoopKind kind = JIT_LOOP_CONDITION;

        if (entry_op == OP_FOR_NEXT) {                           // numeric-for loop
            if (chunk->code[entry].operands[2] != 0) continue;   // non-numeric for — reject
            exit_pc = chunk->code[entry].operands[1];            // exit address in op2
            kind = JIT_LOOP_NUMERIC_FOR;
        } else if (entry_op == OP_TABLE_ITER_NEXT) {             // table iteration loop
            exit_pc = chunk->code[entry].operands[2];            // exit address in op3
            kind = JIT_LOOP_TABLE_ITER;
        } else if (is_loop_entry_op(entry_op)) {                 // classic condition-entry
            exit_pc = chunk->code[entry].operands[0];            // exit address in op1
            kind = JIT_LOOP_CONDITION;
            if (exit_pc != pc + 1 &&                             // entry jump exits elsewhere:
                loop_op == OP_JUMP &&                            // rotated body with a
                exit_pc >= entry && exit_pc <= pc) {             // guard skipping inside the body
                int found_exit = -1;                            // fall-in guard: the first
                for (int i = entry + 1; i <= pc; i++) {          // conditional jump inside the
                    Opcode op = chunk->code[i].opcode;           // body that targets pc+1 is
                    bool is_cond = (op == OP_JUMP_IF_FALSE) ||   // the real exit test
                                   (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE_IMM);
                    if (!is_cond) continue;
                    int tgt = chunk->code[i].operands[0];
                    if (tgt <= pc) continue;                     // internal skip
                    if (found_exit < 0) found_exit = tgt;
                    else if (found_exit != tgt) { found_exit = -2; break; }
                }
                if (found_exit == pc + 1) {                       // body is do-while shaped
                    exit_pc = pc + 1;                            // entry jump is an internal skip
                    kind = JIT_LOOP_COND_ENTER;
                }
            }
        } else if (loop_op == OP_JUMP && entry_op != OP_JUMP_IF_FALSE) {
            int found_exit = -1;
            for (int i = entry + 1; i <= pc; i++) {
                Opcode op = chunk->code[i].opcode;
                bool is_cond = (op == OP_JUMP_IF_FALSE) ||
                               (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE_IMM);
                if (!is_cond) continue;
                int tgt = chunk->code[i].operands[0];
                if (tgt <= pc) continue;                         // internal skip, not an exit
                if (found_exit < 0) found_exit = tgt;
                else if (found_exit != tgt) { found_exit = -2; break; }  // multiple exits
            }
            if (found_exit < 0) continue;                        // no exit found
            exit_pc = found_exit;
            kind = JIT_LOOP_COND_ENTER;
        } else {
            continue;
        }

        if (exit_pc != pc + 1) continue;                         // exit must be right after back edge

        // reclassify: if the back-edge target is a plain op but the very next instruction
        if (kind == JIT_LOOP_COND_ENTER && entry + 1 < pc) {
            Opcode e1 = chunk->code[entry + 1].opcode;
            if (e1 >= OP_JUMP_IF_EQ && e1 <= OP_JUMP_IF_GTE_IMM &&
                chunk->code[entry + 1].operands[0] == exit_pc) {
                // the skipped op only runs on the fall-in edge
                bool invariant = true;                          // verdict for the skip
                Instruction* skip = &chunk->code[entry];
                int reads[3];                                  // slots the skipped op reads
                int n_reads = 0;                               // count of valid entries
                switch (skip->opcode) {
                    case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:    // pure producers: no reads
                    case OP_LOAD_BOOL: case OP_LOAD_NONE:
                    case OP_LOAD_CONST:
                        break;
                    case OP_INC: case OP_DEC:                  // read-modify-write dest
                        reads[n_reads++] = skip->operands[0];
                        break;
                    case OP_MOVE: case OP_NEG:                 // single-register reads
                        reads[n_reads++] = skip->operands[1];
                        break;
                    default:                                    // conservative: every operand
                        reads[n_reads++] = skip->operands[0];
                        reads[n_reads++] = skip->operands[1];
                        reads[n_reads++] = skip->operands[2];
                        break;
                }
                for (int i = entry + 1; i <= pc && invariant; i++) {   // body write scan
                    Opcode wop = chunk->code[i].opcode;
                    int dw = -1;                               // slot this op writes, if any
                    switch (wop) {
                        case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
                        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
                        case OP_LOAD_BOOL: case OP_LOAD_NONE: case OP_LOAD_CONST:
                        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
                        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
                        case OP_DIV_IMM: case OP_MOD_IMM:
                        case OP_CMP_EQ: case OP_CMP_NEQ: case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
                        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                        case OP_AND: case OP_OR:
                        case OP_CONCAT: case OP_NEW_TABLE: case OP_LOAD_TABLE:
                        case OP_TABLE_GET: case OP_TABLE_GET_INT: case OP_TABLE_GET_NUM:
                        case OP_TABLE_GET_CONST: case OP_TABLE_GET_KEY_STR:
                        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
                        case OP_CALL: case OP_ASYNC_CALL:
                        case OP_CALL_BUILTIN: case OP_ASYNC_CALL_BUILTIN:
                        case OP_LOAD_GLOBAL:
                            dw = chunk->code[i].operands[0];    // dest register slot
                            break;
                        default:
                            break;                              // jumps and reads: no dest
                    }
                    if (dw < 0) continue;                       // not a slot writer
                    for (int k = 0; k < n_reads && invariant; k++)
                        if (reads[k] == dw) invariant = false; // body rewrites an input
                }
                if (invariant) {
                    entry += 1;
                    kind  = JIT_LOOP_CONDITION;
                }
            }
        }

        bool ok = true;
        for (int i = entry + 1; i < pc && ok; i++) {             // internal branch scan
            Opcode op = chunk->code[i].opcode;
            if (op == OP_JUMP_IF_FALSE ||
                (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_NOT_NONE)) {
                if (kind != JIT_LOOP_COND_ENTER) { ok = false; break; }
                int tgt = chunk->code[i].operands[0];
                if (tgt >= entry && tgt <= pc) continue;         // internal forward skip
                if (tgt == exit_pc) continue;                    // loop exit
                ok = false; break;
            }
            if (op == OP_JUMP) {                                 // unconditional internal branch
                if (kind == JIT_LOOP_COND_ENTER) {               // if/else join, ternary skip
                    int jt = chunk->code[i].operands[0];
                    if (jt >= entry && jt <= pc) continue;       // forward join inside the body
                    if (jt == exit_pc) continue;                 // direct exit (break-style)
                }
                ok = false;                                      // backward: nested loop
                continue;
            }
            if (op == OP_FOR_NEXT ||                             // nested numeric for
                op == OP_TABLE_ITER_NEXT ||                      // nested table iter
                op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
                op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) {
                ok = false;                                      // nested loop or match dispatch
            }
        }
        if (!ok) continue;

        for (int i = entry; i <= pc && ok; i++) {                // every body instruction must be loop-pure
            if (!is_pure_loop_instr(ctx, i)) ok = false;
        }
        if (!ok) continue;

        if (kind == JIT_LOOP_NUMERIC_FOR) {                      // extra checks for FOR_NEXT
            int fi = entry - 1;
            if (fi < start) continue;
            if (chunk->code[fi].opcode != OP_FOR_INIT) continue;
            for_var_reg  = chunk->code[fi].operands[0];
            for_end_reg  = chunk->code[fi].operands[1];
            for_step_reg = chunk->code[fi].operands[2];
            if (chunk->code[entry].operands[0] != for_var_reg) continue;

            for (int p = fi - 1; p >= start && p >= fi - 8; p--) {
                Instruction* ip = &chunk->code[p];
                if (ip->opcode != OP_LOAD_NUM_IMM) continue;   // skip interleaved loads
                if (ip->operands[0] != for_step_reg) continue; // not a write to step
                int lit = ip->operands[1];
                if (lit > 0) step_sign = +1;
                else if (lit < 0) step_sign = -1;
                step_value = lit;                              // remember exact literal
                break;                                         // last write to step wins
            }

            if (!body_only_uses_counter_index(chunk, entry, pc, for_var_reg)) continue;
        } else if (kind == JIT_LOOP_TABLE_ITER) {                // extra checks for TABLE_ITER
            int fi = entry - 1;
            if (fi < start) continue;
            if (chunk->code[fi].opcode != OP_TABLE_ITER_INIT) continue;
            if (!table_iter_body_is_clean(chunk, entry, pc)) continue;
            init_table_reg = chunk->code[fi].operands[0];        // iterated table's frame slot
        }

        add_loop(ctx, start, end, entry, pc, exit_pc, kind,      // register the loop
                 for_var_reg, for_end_reg, for_step_reg, step_sign, step_value, nregs,
                 init_table_reg);
        if (!basic_loop_is_consistent(ctx, &ctx->loops[ctx->loop_count - 1]))
            ctx->loop_count--;                                   // refcount clash: leave to interpreter
    }
}

// true when at least one detected native loop starts inside the function's range;
// a specialized native loop always beats the helper-per-opcode body, so such
// functions stay with the interpreter, which enters those loops directly
static bool function_has_native_loop(JITContext* ctx, int func_idx) {
    int start = ctx->range_start[func_idx];                          // first pc of this fn
    int end   = ctx->range_end[func_idx];                            // one past last pc
    for (int li = 0; li < ctx->loop_count; li++)                     // scan detected loops
        if (ctx->loops[li].entry_pc >= start && ctx->loops[li].entry_pc < end)
            return true;                                             // loop belongs to this fn
    return false;                                                    // no native loop inside
}

// runs the full analysis pipeline; returns false if there is nothing to compile
bool jit_analyze(JITContext* ctx) {
    BytecodeChunk* chunk = ctx->chunk;
    int n = ctx->func_count;                                     // number of functions

    for (int i = 0; i < n; i++)                                  // ranges for every function
        compute_function_range(chunk, i, &ctx->range_start[i], &ctx->range_end[i]);

    for (int i = 1; i < n; i++)                                  // fast tier, unchanged shape
        ctx->pure[i] = function_is_initially_pure(ctx, i);       // numeric op set (optimized tier)

    // purity fixpoint: a pure caller can only call pure callees, because the
    // fast tier emits direct native calls without runtime checks
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
                        ctx->pure[i] = false;                    // callee has no native code
                        changed = true;                          // need another pass
                        break;
                    }
                }
            }
        }
    }

    for (int i = 1; i < n; i++) {                                // return-type detection
        if (!ctx->pure[i]) continue;                             // skip non-pure
        if (!infer_return_type(ctx, i, &ctx->return_type[i])) {
            ctx->pure[i] = false;                                // mixed return -> reject
        }
    }

    // strict purity: cfg type-flow proves every unchecked operand numeric
    bool* numret = NULL;                                           // callee returns numbers under the contract
    if (n > 0) numret = (bool*)calloc((size_t)n, sizeof(bool));     // positive count keeps the range tight
    if (!numret) return false;                                      // allocation failed
    for (int i = 0; i < n; i++) numret[i] = ctx->pure[i];         // optimistic start: every pure fn qualifies
    bool numret_changed = true;                                  // fixpoint driver
    while (numret_changed) {
        numret_changed = false;
        for (int i = 1; i < n; i++) {
            if (!ctx->pure[i]) continue;                         // only pure fns hold the contract
            TypeFlowResult res;
            type_flow_analyze(ctx, i, true, numret, &res);       // numeric-args contract
            bool now = res.saw_return && res.ret_kinds == TF_RET_NUM;   // every return is numeric
            if (now != numret[i]) {
                numret[i] = now;                                 // contract verdict changed
                numret_changed = true;
            }
        }
    }
    for (int i = 0; i < n; i++) {
        if (!ctx->pure[i]) { ctx->strict_pure[i] = false; continue; }
        TypeFlowResult res;
        type_flow_analyze(ctx, i, true, numret, &res);           // final verdict with stable callees
        ctx->strict_pure[i] = res.ok;
        if (ctx->strict_pure[i] && ctx->return_type[i] == JIT_RET_ANY) {
            if      (res.ret_kinds == TF_RET_NUM)  ctx->return_type[i] = JIT_RET_NUMBER;  // numeric return proven
            else if (res.ret_kinds == TF_RET_BOOL) ctx->return_type[i] = JIT_RET_BOOL;   // bool return proven
            else if (res.ret_kinds == TF_RET_NONE) ctx->return_type[i] = JIT_RET_NONE;   // none return proven
        }
    }
    free(numret);                                                 // contract scratch is done

    // basic tier eligibility, computed before loop detection
    for (int i = 1; i < n; i++)
        ctx->basic[i] = function_is_basic_compilable(ctx, i);     // standard helpers (basic tier)

    for (int i = 0; i < n; i++) {                                // loop detection, incl. entry fn
        if (ctx->pure[i]) continue;                              // pure fns compile their own loops
        detect_loops_in_function(ctx, i);
    }

    // a function that owns a native loop keeps the interpreter driving it
    for (int i = 1; i < n; i++) {
        if (ctx->pure[i]) { ctx->basic[i] = false; continue; }    // fast tier owns this fn
        if (ctx->basic[i] && function_has_native_loop(ctx, i))
            ctx->basic[i] = false;                                // interpreter + native loop wins
    }

    // return types for basic fns: mixed returns box at runtime
    for (int i = 1; i < n; i++) {
        if (!ctx->basic[i]) continue;                             // only basic tier here
        if (!infer_return_type(ctx, i, &ctx->return_type[i])) {
            ctx->return_type[i] = JIT_RET_ANY;                   // boxes in jit_op_call
        }
    }

    // loop bodies emit unchecked direct calls, so a loop whose callee lost its native entry
    for (int li = 0; li < ctx->loop_count; li++) {
        JitLoopInfo* loop = &ctx->loops[li];                      // loop to validate
        bool keep = true;                                         // verdict for this loop
        for (int pc = loop->entry_pc; pc <= loop->back_edge_pc && keep; pc++) {
            Instruction* inst = &chunk->code[pc];
            if (inst->opcode == OP_CALL_0 ||                      // direct call inside the body
                inst->opcode == OP_CALL_1 ||
                inst->opcode == OP_CALL_2) {
                int target = inst->operands[1];                   // callee function index
                if (target < 0 || target >= n ||
                    (!ctx->pure[target] && !ctx->basic[target]))
                    keep = false;                                 // no native entry remains
            }
        }
        if (!keep) {                                              // remove the loop record
            memmove(&ctx->loops[li], &ctx->loops[li + 1],
                    sizeof(JitLoopInfo) * (ctx->loop_count - li - 1));
            ctx->loop_count--;                                    // shrink the array
            li--;                                                 // re-check this index
        }
    }
    int candidate_count = 0;                                     // anything to emit?
    for (int i = 0; i < n; i++) if (ctx->pure[i] || ctx->basic[i]) candidate_count++;
    return candidate_count > 0 || ctx->loop_count > 0;
}
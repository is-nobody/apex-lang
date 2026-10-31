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
        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2: {    // calls allowed only to pure fns
            int target = inst->operands[1];                  // callee function index
            return target >= 0 && target < ctx->func_count && ctx->pure[target];
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

// infers how a function returns its value; returns false on mixed kinds
#define JIT_MAX_REGS_SCAN 512

static bool infer_return_type(BytecodeChunk* chunk, int start, int end,
                              JitReturnType* out) {
    bool is_bool[JIT_MAX_REGS_SCAN];                             // bool-flag per register
    bool is_none[JIT_MAX_REGS_SCAN];                             // none-flag per register
    bool is_num [JIT_MAX_REGS_SCAN];                             // numeric-flag per register
    memset(is_bool, 0, sizeof(is_bool));                         // no regs are bool initially
    memset(is_none, 0, sizeof(is_none));                         // no regs are none initially
    memset(is_num,  0, sizeof(is_num));                          // no regs are number initially

    bool any_bool = false;                                       // saw a provably-bool RETURN
    bool any_num  = false;                                       // saw a provably-number RETURN
    bool any_none = false;                                       // saw a RETURN_NONE
    bool any_any  = false;                                       // saw a RETURN with unprovable type

    for (int pc = start; pc < end; pc++) {                       // walk the function body
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // destination register
        int a = inst->operands[1];                               // first source register

        switch (inst->opcode) {
            case OP_CMP_EQ:     case OP_CMP_NEQ:                 // all comparisons produce
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // a bool
            case OP_CMP_LT: case OP_CMP_GT:
            case OP_CMP_LTE: case OP_CMP_GTE:
            case OP_AND: case OP_OR:                             // logical ops produce bool
                if (d < JIT_MAX_REGS_SCAN) {
                    is_bool[d] = true;  is_none[d] = false; is_num[d] = false;
                }
                break;

            case OP_MOVE:                                        // move preserves all three flags
                if (d < JIT_MAX_REGS_SCAN) {
                    is_bool[d] = (a < JIT_MAX_REGS_SCAN) ? is_bool[a] : false;
                    is_none[d] = (a < JIT_MAX_REGS_SCAN) ? is_none[a] : false;
                    is_num [d] = (a < JIT_MAX_REGS_SCAN) ? is_num [a] : false;
                }
                break;

            case OP_LOAD_BOOL:
                if (d < JIT_MAX_REGS_SCAN) {
                    is_bool[d] = true;  is_none[d] = false; is_num[d] = false;
                }
                break;

            case OP_LOAD_NONE:
                if (d < JIT_MAX_REGS_SCAN) {
                    is_bool[d] = false; is_none[d] = true;  is_num[d] = false;
                }
                break;

            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:              // provably numeric producers
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
            case OP_NEG: case OP_INC: case OP_DEC:
                if (d < JIT_MAX_REGS_SCAN) {
                    is_bool[d] = false; is_none[d] = false; is_num[d] = true;
                }
                break;

            case OP_LOAD_CONST: {                                // type is decided by the pool entry
                if (d >= JIT_MAX_REGS_SCAN) break;
                int idx = inst->operands[1];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) {      // out of range: unknown
                    is_bool[d] = false; is_none[d] = false; is_num[d] = false;
                    break;
                }
                switch (chunk->constants[idx].type) {
                    case CONST_NUMBER:
                        is_bool[d] = false; is_none[d] = false; is_num[d] = true;  break;
                    case CONST_BOOL:
                        is_bool[d] = true;  is_none[d] = false; is_num[d] = false; break;
                    case CONST_NONE:
                        is_bool[d] = false; is_none[d] = true;  is_num[d] = false; break;
                    default:                                     // string / function: unknown
                        is_bool[d] = false; is_none[d] = false; is_num[d] = false; break;
                }
                break;
            }

            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:      // callee return type unknown
            case OP_CALL_BUILTIN: case OP_ASYNC_CALL_BUILTIN:    // builtin result type unknown
                if (d < JIT_MAX_REGS_SCAN) {
                    is_bool[d] = false; is_none[d] = false; is_num[d] = false;
                }
                break;

            case OP_RETURN:                                      // return kind depends on reg
                if (d >= JIT_MAX_REGS_SCAN) { any_any = true; break; }
                if      (is_none[d]) any_none = true;
                else if (is_bool[d]) any_bool = true;
                else if (is_num [d]) any_num  = true;
                else                 any_any  = true;            // can't prove -> runtime check
                break;

            case OP_RETURN_NUM:                                  // explicitly numeric
            case OP_RETURN_NUM_IMM:                              // numeric immediate
                any_num = true;
                break;

            case OP_RETURN_BOOL:                                 // explicitly boolean
                any_bool = true;
                break;

            case OP_RETURN_NONE:                                 // explicitly none
                any_none = true;
                break;

            default: break;                                      // other ops don't matter
        }
    }

    // if any return came from a register whose type isn't provable, fall back to jit_ret_any
    if (any_any) {
        *out = JIT_RET_ANY;
        return true;
    }

    int kinds = (any_bool ? 1 : 0) + (any_num ? 1 : 0) + (any_none ? 1 : 0);
    if (kinds != 1) return false;                                // mixed return -> reject

    *out = any_none ? JIT_RET_NONE
         : any_bool ? JIT_RET_BOOL
                    : JIT_RET_NUMBER;
    return true;
}

// simple per-slot type for strict-purity analysis
typedef enum {
    SP_NUM = 0,   // provably a plain number
    SP_BOOL,      // provably a bool (from LOAD_BOOL or CMP_*)
    SP_NONE,      // provably none (from LOAD_NONE)
    SP_UNK,       // unknown type (params before binding, call results, etc.)
} SPType;

// strict purity: verify every arithmetic operand is numeric (via simple flow)
// so the optimized emit (which assumes numeric operands) is safe
static bool function_is_strictly_pure(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];
    int end   = ctx->range_end[func_idx];
    int arity = chunk->functions[func_idx].arity;
    if (arity > JIT_MAX_SLOTS) return false;                  // too many params to track

    SPType types[JIT_MAX_SLOTS];                              // per-slot type
    for (int i = 0; i < JIT_MAX_SLOTS; i++) types[i] = SP_UNK;
    for (int i = 0; i < arity; i++) types[i] = SP_NUM;         // native entry accepts only numeric args

    // a self-recursive call can preserve the function's numeric result type
    bool uses_numeric_self_call = false;
    bool all_returns_numeric = true;
    bool saw_return = false;

    for (int pc = start; pc < end; pc++) {
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];
        int a = inst->operands[1];
        int b = inst->operands[2];
        switch (inst->opcode) {
            case OP_MOVE:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                if (a < 0 || a >= JIT_MAX_SLOTS) { types[d] = SP_UNK; break; }
                types[d] = types[a];
                break;
            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_NUM;
                break;
            case OP_LOAD_CONST: {                                // strict-pure: only scalar loads
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                int idx = inst->operands[1];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) return false;
                switch (chunk->constants[idx].type) {
                    case CONST_NUMBER: types[d] = SP_NUM;  break;
                    case CONST_BOOL:   types[d] = SP_BOOL; break;
                    case CONST_NONE:   types[d] = SP_NONE; break;
                    default:           types[d] = SP_UNK;  break;  // string -> unknown
                }
                break;
            }
            case OP_LOAD_BOOL:
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_BOOL;
                break;
            case OP_LOAD_NONE:
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_NONE;
                break;
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
                if (a < 0 || a >= JIT_MAX_SLOTS) return false;
                if (b < 0 || b >= JIT_MAX_SLOTS) return false;
                if (types[a] != SP_NUM || types[b] != SP_NUM) return false;
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_NUM;
                break;
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
                if (a < 0 || a >= JIT_MAX_SLOTS) return false;
                if (types[a] != SP_NUM) return false;
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_NUM;
                break;
            case OP_NEG:
                if (a < 0 || a >= JIT_MAX_SLOTS) return false;
                if (types[a] != SP_NUM) return false;
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_NUM;
                break;
            case OP_INC: case OP_DEC:
                if (d < 0 || d >= JIT_MAX_SLOTS) return false;
                if (types[d] != SP_NUM) return false;
                break;
            case OP_CMP_EQ: case OP_CMP_NEQ:
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                // comparisons need numeric operands for the optimized emit
                if (a < 0 || a >= JIT_MAX_SLOTS) return false;
                if (b < 0 || b >= JIT_MAX_SLOTS) return false;
                if (types[a] != SP_NUM || types[b] != SP_NUM) return false;
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_BOOL;
                break;
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
                // eq/neq jumps use dispatch in interpreter; pure emit needs numeric operands
                if (a < 0 || a >= JIT_MAX_SLOTS) return false;
                if (b < 0 || b >= JIT_MAX_SLOTS) return false;
                if (types[a] != SP_NUM || types[b] != SP_NUM) return false;
                break;
            case OP_AND: case OP_OR:                       // bit-0 test, works on any type
                if (a < 0 || a >= JIT_MAX_SLOTS) return false;
                if (b < 0 || b >= JIT_MAX_SLOTS) return false;
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_BOOL;
                break;
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
            case OP_JUMP_IF_NONE: case OP_JUMP_IF_NOT_NONE:
            case OP_JUMP_IF_FALSE: case OP_JUMP:
                // other jumps: pure emit matches interpreter (bits comparison / falsy-bit test)
                break;
            case OP_RETURN: case OP_RETURN_NUM:
                saw_return = true;
                if (d < 0 || d >= JIT_MAX_SLOTS || types[d] != SP_NUM)
                    all_returns_numeric = false;
                break;
            case OP_RETURN_NUM_IMM:
                saw_return = true;                              // immediate return is numeric
                break;
            case OP_RETURN_BOOL: case OP_RETURN_NONE:
                saw_return = true;
                all_returns_numeric = false;
                break;
            case OP_CALL_0:
                // no args; the call result type is unknown
                if (d >= 0 && d < JIT_MAX_SLOTS) types[d] = SP_UNK;
                break;
            case OP_CALL_1:
                if (b < 0 || b >= JIT_MAX_SLOTS) return false;
                if (types[b] != SP_NUM) return false;
                if (d >= 0 && d < JIT_MAX_SLOTS) {
                    if (a == func_idx) {
                        types[d] = SP_NUM;
                        uses_numeric_self_call = true;
                    } else {
                        types[d] = SP_UNK;
                    }
                }
                break;
            case OP_CALL_2:
                if (b < 0 || b >= JIT_MAX_SLOTS) return false;
                if (b + 1 >= JIT_MAX_SLOTS) return false;
                if (types[b] != SP_NUM || types[b + 1] != SP_NUM) return false;
                if (d >= 0 && d < JIT_MAX_SLOTS) {
                    if (a == func_idx) {
                        types[d] = SP_NUM;
                        uses_numeric_self_call = true;
                    } else {
                        types[d] = SP_UNK;
                    }
                }
                break;
            default:
                return false;                             // any op outside pure set: not strict
        }
    }

    if (uses_numeric_self_call && (!saw_return || !all_returns_numeric))
        return false;                                             // no numeric induction base / mixed return
    return true;
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

    info->table.used               = false;                      // no table op seen yet
    info->table.slot               = -1;                         // table register (unknown yet)
    info->table.min_count          = 0;                          // required array_count
    info->table.max_idx            = 0;                          // required array_capacity
    info->table.written            = false;                      // has OP_TABLE_SET_INT
    info->table.indexed_by_counter = false;                      // t[i] with i == loop counter
    info->ref_writes               = 0;                          // slots whose value is refcounted
    info->touches_tables           = false;                      // any table op at all
    info->str_slots                = 0;                          // slots that receive string Values
    memset(info->live_in_kind, 0, sizeof(info->live_in_kind));   // all slots default to NUM

    for (int pc = info->entry_pc; pc <= info->back_edge_pc; pc++) {
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // destination register
        int a = inst->operands[1];                               // first source register
        int b = inst->operands[2];                               // second source register
        uint64_t pc_reads = 0, pc_writes = 0;                    // this instruction's masks
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
            case OP_LOAD_CONST:                                   // writes d only
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
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
            default:
                break;
        }
        live_in_mask |= pc_reads & ~written_mask;                // read before first write → live-in
        written_mask |= pc_writes;                               // slot has been produced
    }
    info->live_in  = live_in_mask;                               // slots read before first write
    info->live_out = written_mask;                               // slots produced inside the loop

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
            // rejects — need a base the emitter is not carrying
            case OP_TABLE_GET_INT: case OP_TABLE_SET_INT:          // use rbx as array_part of another table
            case OP_LOAD_GLOBAL: case OP_STORE_GLOBAL:             // use rbx as globals base
            // rejects — no lowering exists in emit_loop_body_instr
            case OP_TABLE_GET_CONST: case OP_TABLE_SET_CONST:
            case OP_TABLE_APPEND: case OP_NEW_TABLE:
            // rejects — nested iteration
            case OP_TABLE_ITER_NEXT: case OP_TABLE_ITER_INIT:
            // rejects — helper clobbers rdx (the counter mirror) or r8 (NONE bits)
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
                return false;
            default:
                break;
        }
    }
    return true;
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
                entry += 1;
                kind  = JIT_LOOP_CONDITION;
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
            if (op == OP_JUMP || op == OP_FOR_NEXT ||            // unconditional or nested for
                op == OP_TABLE_ITER_NEXT ||                      // nested table iter
                op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
                op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) {
                ok = false;                                      // nested loop or internal branch
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

    // strict purity: numeric-flow check; enables the optimized (unchecked) emit
    for (int i = 0; i < n; i++) {
        if (!ctx->pure[i]) { ctx->strict_pure[i] = false; continue; }
        ctx->strict_pure[i] = function_is_strictly_pure(ctx, i);
    }

    for (int i = 0; i < n; i++) {                                // loop detection, incl. entry fn
        if (ctx->pure[i]) continue;                              // pure fns are fully compiled
        detect_loops_in_function(ctx, i);
    }

    // strict-pure callees consume raw doubles
    bool strict_edge_changed;
    do {
        strict_edge_changed = false;
        for (int target = 1; target < n; target++) {
            if (!ctx->strict_pure[target]) continue;
            bool unsafe_call_edge = false;

            for (int caller = 1; caller < n && !unsafe_call_edge; caller++) {
                if (caller == target || !ctx->pure[caller] || ctx->strict_pure[caller])
                    continue;                                   // interpreter or numeric-proven caller is safe
                for (int pc = ctx->range_start[caller]; pc < ctx->range_end[caller]; pc++) {
                    Instruction* inst = &chunk->code[pc];
                    if ((inst->opcode == OP_CALL_1 || inst->opcode == OP_CALL_2) &&
                        inst->operands[1] == target) {
                        unsafe_call_edge = true;
                        break;
                    }
                }
            }

            // calls inside native loop bodies bypass the interpreter's is_number dispatch too
            for (int li = 0; li < ctx->loop_count && !unsafe_call_edge; li++) {
                JitLoopInfo* loop = &ctx->loops[li];
                for (int pc = loop->entry_pc; pc <= loop->back_edge_pc; pc++) {
                    Instruction* inst = &chunk->code[pc];
                    if ((inst->opcode == OP_CALL_1 || inst->opcode == OP_CALL_2) &&
                        inst->operands[1] == target) {
                        unsafe_call_edge = true;
                        break;
                    }
                }
            }

            if (unsafe_call_edge) {
                ctx->strict_pure[target] = false;                // retain checked arithmetic for safety
                strict_edge_changed = true;
            }
        }
    } while (strict_edge_changed);                              // propagate non-strict callers to their callees

    int candidate_count = 0;                                     // anything to emit?
    for (int i = 0; i < n; i++) if (ctx->pure[i]) candidate_count++;
    return candidate_count > 0 || ctx->loop_count > 0;
}
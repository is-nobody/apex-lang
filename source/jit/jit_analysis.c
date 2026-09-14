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
        case OP_MOVE:                                        // reg-to-reg copy
        case OP_LOAD_NUM_IMM:                                // small int literal
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_NEG: case OP_INC: case OP_DEC:               // arithmetic
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // numeric compares
        case OP_CMP_EQ:     case OP_CMP_NEQ:                 // generic compares
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_JUMP:                                        // unconditional
        case OP_JUMP_IF_EQ:     case OP_JUMP_IF_NEQ:         // conditional branches
        case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_FOR_NEXT:                                    // numeric-for entry
        case OP_TABLE_GET:                                   // validated by analyze_loop_regs
        case OP_TABLE_GET_INT:                               // fixed index into array part
        case OP_TABLE_SET:                                   // dynamic key, validated later by analyze_loop_regs
        case OP_TABLE_SET_INT:
            return true;
        case OP_TABLE_ITER_NEXT:                             // loop entry for table iteration
            return true;
        case OP_LOAD_NUM: {                                  // only numeric constants
            int idx = inst->operands[1];
            return idx >= 0 && idx < chunk->const_count &&
                   chunk->constants[idx].type == CONST_NUMBER;
        }
        default:
            return false;                                    // anything else: reject
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
            case OP_RETURN_BOOL:                                 // incl. boolean
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

            case OP_RETURN_BOOL:                                 // explicitly boolean
                any_bool = true;
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
           op == OP_TABLE_ITER_NEXT ||
           (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE);
}

// walks the loop body, marks read and written slots, and collects table uses
static void analyze_loop_regs(JITContext* ctx, JitLoopInfo* info) {
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
            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:              // writes d only
                if (d >= 0 && d < 64) pc_writes |= 1ULL << d;
                break;
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
                if (a >= 0 && a < 64) pc_reads |= 1ULL << a;     // left operand
                if (b >= 0 && b < 64) pc_reads |= 1ULL << b;     // right operand
                break;
            case OP_FOR_NEXT:                                    // counter and bounds updated by vm
                if (d >= 0 && d < 64) pc_reads |= pc_writes |= 1ULL << d;
                if (info->for_end_reg  >= 0 && info->for_end_reg  < 64) pc_reads |= 1ULL << info->for_end_reg;
                if (info->for_step_reg >= 0 && info->for_step_reg < 64) pc_reads |= 1ULL << info->for_step_reg;
                break;
            case OP_TABLE_ITER_NEXT:                             // element written each iteration
                if (d >= 0 && d < 64) {
                    pc_writes |= 1ULL << d;
                    info->ref_writes |= 1ULL << d;               // element may be heap-allocated
                }
                info->table.used     = true;                     // marks loop as table-touching
                info->touches_tables = true;
                break;
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
}

// registers a loop and runs the slot analysis on its body
static void add_loop(JITContext* ctx, int entry, int back_edge, int exit_pc,
                     JitLoopKind kind, int for_var_reg, int for_end_reg, int for_step_reg,
                     int nregs) {
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
    info->nregs        = nregs;                                  // function frame size
    info->table.slot   = -1;                                     // no table seen yet
    analyze_loop_regs(ctx, info);                                // fill live_in/out and table use
}

// checks whether a table-iter body uses no table ops beyond the loop iterator
static bool table_iter_body_is_clean(BytecodeChunk* chunk, int entry, int back_edge) {
    for (int pc = entry + 1; pc < back_edge; pc++) {               // skip entry instr itself
        Opcode op = chunk->code[pc].opcode;
        if (op == OP_TABLE_GET || op == OP_TABLE_GET_INT ||        // any table read
            op == OP_TABLE_SET_INT || op == OP_TABLE_ITER_NEXT ||  // any table write or nested iter
            op == OP_TABLE_ITER_INIT) {                            // nested table iteration
            return false;                                          // body touches table directly
        }
    }
    return true;                                                   // only the entry touches the table
}

// checks whether a numeric-for body uses only numeric table access (no string keys)
static bool body_only_uses_counter_index(BytecodeChunk* chunk, int entry,
                                         int back_edge, int for_var_reg) {
    (void)for_var_reg;                                       // kept for call-site parity, unused
    for (int pc = entry + 1; pc < back_edge; pc++) {         // scan every body instruction
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_TABLE_GET_CONST:
            case OP_TABLE_SET_CONST:
                return false;                                // string keys land in the hash part
            default:
                break;                                       // other ops don't touch tables
        }
    }
    return true;                                             // only numeric table access seen
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
        if (chunk->code[pc].opcode != OP_JUMP) continue;         // only plain JUMP
        int entry = chunk->code[pc].operands[0];                 // target pc
        if (entry >= pc || entry < start) continue;              // must be backward and in-range
        if (!is_loop_entry_op(chunk->code[entry].opcode)) continue;  // valid entry opcode

        Opcode entry_op = chunk->code[entry].opcode;
        int exit_pc = -1;
        int for_var_reg = -1, for_end_reg = -1, for_step_reg = -1;
        JitLoopKind kind = JIT_LOOP_CONDITION;                   // default: conditional loop

        if (entry_op == OP_FOR_NEXT) {                           // numeric-for loop
            if (chunk->code[entry].operands[2] != 0) continue;   // non-numeric for — reject
            exit_pc = chunk->code[entry].operands[1];            // exit address in op2
            kind = JIT_LOOP_NUMERIC_FOR;
        } else if (entry_op == OP_TABLE_ITER_NEXT) {             // table iteration loop
            exit_pc = chunk->code[entry].operands[2];            // exit address in op3
            kind = JIT_LOOP_TABLE_ITER;
        } else {                                                 // conditional loop
            exit_pc = chunk->code[entry].operands[0];            // exit address in op1
        }
        if (exit_pc != pc + 1) continue;                         // exit must be right after back edge

        bool ok = true;
        for (int i = entry + 1; i < pc && ok; i++) {             // no jumps inside body
            Opcode op = chunk->code[i].opcode;
            if (op == OP_JUMP || op == OP_FOR_NEXT ||            // unconditional or nested for
                op == OP_TABLE_ITER_NEXT ||                      // nested table iter
                (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE)) { // conditional branch
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
            if (fi < start) continue;                            // no room for FOR_INIT
            if (chunk->code[fi].opcode != OP_FOR_INIT) continue; // must be preceded by FOR_INIT
            for_var_reg  = chunk->code[fi].operands[0];          // loop counter slot
            for_end_reg  = chunk->code[fi].operands[1];          // end bound slot
            for_step_reg = chunk->code[fi].operands[2];          // step slot
            if (chunk->code[entry].operands[0] != for_var_reg) continue;  // FOR_NEXT must use same var
            int p = fi - 1;                                      // step must be provably positive
            if (p < start) continue;                             // no room for the step literal
            if (chunk->code[p].opcode != OP_LOAD_NUM_IMM) continue;  // step must be an imm
            if (chunk->code[p].operands[0] != for_step_reg) continue;  // and write to step slot
            if (chunk->code[p].operands[1] <= 0) continue;       // step must be positive

            // reject dynamic table access that isn't the loop counter
            if (!body_only_uses_counter_index(chunk, entry, pc, for_var_reg)) continue;
        } else if (entry_op == OP_TABLE_ITER_NEXT) {             // extra checks for TABLE_ITER
            int fi = entry - 1;
            if (fi < start) continue;                            // no room for ITER_INIT
            if (chunk->code[fi].opcode != OP_TABLE_ITER_INIT) continue;  // must be preceded by ITER_INIT
            // body must not access any table other than through ITER_NEXT
            if (!table_iter_body_is_clean(chunk, entry, pc)) continue;
        }

        add_loop(ctx, entry, pc, exit_pc, kind,                  // register the loop
                 for_var_reg, for_end_reg, for_step_reg, nregs);
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
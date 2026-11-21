// source/jit/x86-64/x86_64_emit_function.c
// Function emitter driver for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include "vm.h"
#include <stddef.h>
#include <string.h>
// callee-saved gprs usable as slot pockets on all supported abis (exported for call handlers)
const int x86_gpr_pocket_regs[X86_N_GPR_POCKETS] = {
    X86_RBX, X86_R12, X86_R13, X86_R14, X86_R15
};

// emits load r64, [rbp+disp32] for each assigned pocket gpr before leave; ret
static void emit_gpr_pocket_restores(CodeBuf* cb, int pocket_slot_base,
                                     int n_pockets) {
    for (int g = 0; g < n_pockets; g++) {                            // restore saved gprs
        int gpr = x86_gpr_pocket_regs[g];                            // pocket gpr number
        int disp = -8 * (pocket_slot_base + g + 1);                  // matching save slot
        x86_emit_load_r64_rbp(cb, gpr, disp);
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
        case OP_FOR_NEXT_LOOP:                                      // body target in op2
            return inst->operands[1];
        case OP_TABLE_ITER_NEXT:                                    // exit address in op3
            return inst->operands[2];
        default:
            return -1;                                              // not a branch
    }
}

// true when the interpreter's handler decrefs the old destination value before overwriting it
static bool op_needs_pre_decref(Opcode op) {
    switch (op) {
        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:                    // literal loads
        case OP_LOAD_BOOL: case OP_LOAD_NONE: case OP_LOAD_CONST:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:          // arithmetic
        case OP_DIV_IMM: case OP_MOD_IMM:
        case OP_NEG: case OP_INC: case OP_DEC:
        case OP_CMP_EQ: case OP_CMP_NEQ:                            // comparisons
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:             // call results
            return true;                                            // handler releases old dest
        default:
            return false;                                           // helpers or no dest
    }
}

// emit one body instruction through the category dispatcher
static void x86_func_dispatch(X86FuncEmitState* st) {
    switch (st->op) {
        case OP_MOVE:                                                // reg-to-reg copy
        case OP_LOAD_NUM_IMM:                                        // small int literal
        case OP_LOAD_NUM:                                            // full double constant
        case OP_LOAD_BOOL:                                           // boolean literal
        case OP_LOAD_NONE:                                           // none literal
        case OP_LOAD_CONST:                                          // scalar constant incl. string
            if (st->op == OP_MOVE && st->basic_mode) {
                x86_func_emit_basic(st);                             // refcount-aware move
                break;
            }
            x86_func_emit_move(st);
            break;

        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:          // binary arithmetic
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:           // immediate arithmetic
        case OP_DIV_IMM: case OP_MOD_IMM: case OP_MOD:               // mod ops
        case OP_NEG: case OP_INC: case OP_DEC:                       // unary / in-place
        case OP_AND: case OP_OR:                                     // logical ops
            x86_func_emit_arith(st);
            break;

        case OP_CMP_EQ: case OP_CMP_EQ_NUM:                          // numeric compares
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT:
        case OP_CMP_LTE: case OP_CMP_GTE:
            x86_func_emit_cmp(st);
            break;

        case OP_JUMP:                                                // unconditional
        case OP_JUMP_IF_FALSE:                                       // falsy test
        case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:                     // reg-reg compare jumps
        case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:             // imm-compare jumps
        case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
        case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
        case OP_JUMP_IF_NONE: case OP_JUMP_IF_NOT_NONE:              // none-test jumps
            x86_func_emit_jump(st);
            break;

        case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:              // match jumps
        case OP_JUMP_MATCH_BOOL: case OP_JUMP_MATCH_NONE:
        case OP_JUMP_TABLE:                                          // dense int dispatch
        case OP_SWITCH_STR: case OP_SWITCH_NUM:                      // perfect-hash dispatch
            x86_func_emit_match(st);
            break;

        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:              // direct calls
        case OP_PUSH_ARG:                                            // push arg onto vm args stack
        case OP_CALL_BUILTIN:                                        // builtin call dispatch
        case OP_ASYNC_CALL_BUILTIN:                                  // async builtin call dispatch
        case OP_RETURN: case OP_RETURN_NUM: case OP_RETURN_BOOL:     // returns
        case OP_RETURN_NUM_IMM: case OP_RETURN_NONE:
            x86_func_emit_call(st);
            break;

        case OP_CONCAT:                                              // string concatenation
        case OP_NEW_TABLE: case OP_LOAD_TABLE:                       // table construction
        case OP_TABLE_GET: case OP_TABLE_GET_CONST:                   // table reads
        case OP_TABLE_GET_INT: case OP_TABLE_GET_NUM:
        case OP_TABLE_GET_KEY_STR:
        case OP_TABLE_SET: case OP_TABLE_SET_CONST:                   // table writes
        case OP_TABLE_SET_INT: case OP_TABLE_SET_NUM:
        case OP_TABLE_SET_KEY_STR: case OP_TABLE_APPEND:
        case OP_LOAD_GLOBAL: case OP_STORE_GLOBAL:                    // globals
        case OP_FOR_INIT: case OP_FOR_NEXT: case OP_FOR_NEXT_LOOP:    // numeric loops
        case OP_TABLE_ITER_INIT: case OP_TABLE_ITER_NEXT:             // table iteration
        case OP_POP_ITER: case OP_POP_TABLE_ITER:                     // iterator cleanup
        case OP_CALL: case OP_ASYNC_CALL:                             // coroutine-driven calls
            x86_func_emit_basic(st);                                 // standard-opcode helpers
            break;

        default:
            JIT_FATAL("unreachable opcode %d in pure function %d at pc=%d",
                      st->op, st->func_idx, st->pc);
    }
}

// fused CMP_* + JUMP_IF_FALSE: emit an inverted jcc and skip the branch when the comparison dest is dead
static bool try_emit_fused_cmp_jump(X86FuncEmitState* st) {
    if (st->pc + 1 >= st->end) return false;                         // no successor
    if (st->is_target[st->pc + 1 - st->start]) return false;         // pc+1 is a jump target
    if (st->chunk->code[st->pc + 1].opcode != OP_JUMP_IF_FALSE) return false;
    if (st->chunk->code[st->pc + 1].operands[1] != st->d) return false;  // branch not on our dest

    uint8_t fused_jcc = 0;                                           // inverted jcc for "jump on false"
    switch (st->op) {
        case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  fused_jcc = 0x85; break;  // jne
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: fused_jcc = 0x84; break;  // je
        case OP_CMP_LT:  fused_jcc = 0x83; break;                    // jae
        case OP_CMP_GT:  fused_jcc = 0x86; break;                    // jbe
        case OP_CMP_LTE: fused_jcc = 0x87; break;                    // ja
        case OP_CMP_GTE: fused_jcc = 0x82; break;                    // jb
        default: return false;
    }
    int fuse_target = st->chunk->code[st->pc + 1].operands[0];       // branch target pc
    bool d_dead = fuse_target >= 0 &&
                  !slot_read_before_write(st->chunk, st->pc + 2, st->end, st->d) &&
                  (fuse_target < st->pc + 2 ||
                   !slot_read_before_write(st->chunk, fuse_target, st->end, st->d));
    if (!d_dead) return false;                                       // dest not dead, no fusion

    XmmCache* cache = st->cache;
    CodeBuf* cb = st->cb;

    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: fused CMP a=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: fused CMP b=%d (func=%d pc=%d)",
                          st->b, st->func_idx, st->pc);
    x86_emit_ucomisd_rr(cb, xa, xb);                                 // ucomisd a, b

    if (fuse_target >= st->start && fuse_target < st->end) {         // in-range target
        if (st->needs_flush[fuse_target - st->start])                // multi-pred: flush
            x86_cache_flush(cache, cb);
        else if (st->use_snap[fuse_target - st->start])              // unique forward: snapshot
            x86_cache_snap(&st->jump_snap[fuse_target - st->start], cache);
    } else {
        x86_cache_flush(cache, cb);                                  // out-of-range: safest
    }
    emit_u8(cb, 0x0F); emit_u8(cb, fused_jcc);                       // jcc rel32
    x86_func_jump_fixup(st, cb->len, fuse_target);                   // record placeholder
    emit_i32(cb, 0);                                                 // placeholder

    st->pc++;                                                        // skip the JUMP_IF_FALSE
    if (st->pc + 1 < st->end && st->needs_flush[st->pc + 1 - st->start])
        x86_cache_flush(cache, cb);                                  // fall-through merge flush
    st->did_flush = true;                                            // suppress main merge flush
    return true;
}

// emits native x86-64 code for a single numeric-pure function
bool x86_64_emit_function(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                          int func_idx, void** out_fn) {
    int*          pred_count  = NULL;                                // predecessor counts per label
    uint8_t*      needs_flush = NULL;                                // labels needing empty cache
    uint8_t*      use_snap    = NULL;                                // labels restoring from snapshot
    JitCacheSnap* jump_snap   = NULL;                                // per-label cache snapshots

    jmp_buf local_bail;                                              // local catch frame
    jmp_buf* prev_bail = jit_bail_jmp;                               // previous outer frame
    jit_bail_jmp = &local_bail;
    if (setjmp(local_bail) != 0) {                                   // on jit_fatal: unwind here
        jit_bail_jmp = prev_bail;
        free(pred_count);
        free(needs_flush);
        free(use_snap);
        free(jump_snap);
        return false;
    }

    // basic tier scan must run after the bail frame exists: helpers below fatal on overflow
    BytecodeChunk* chunk = ctx->chunk;                               // bytecode being compiled
    int start = ctx->range_start[func_idx];                          // first bytecode pc
    int end   = ctx->range_end[func_idx];                            // one past the last pc
    int arity = chunk->functions[func_idx].arity;                    // parameter count
    int nregs = chunk->functions[func_idx].max_registers;            // frame slot count
    if (nregs < arity) nregs = arity;                                // params need slots too
    if (nregs < 1) nregs = 1;                                        // at least one slot
    if (nregs > JIT_MAX_SLOTS) return false;                         // cache can't track this

    int range_size = end - start;                                    // number of bytecodes
    if (range_size <= 0) return false;                               // empty body

    // tier selection
    bool basic_mode = !ctx->pure[func_idx] && ctx->basic[func_idx];  // standard-opcode tier
    bool needs_cleanup = basic_mode;                                 // walk slots at every exit
    if (!basic_mode) {                                              // pure tier may still hold refs
        for (int pc = start; pc < end; pc++) {
            Opcode op = chunk->code[pc].opcode;
            if (op == OP_CALL_0 || op == OP_CALL_1 || op == OP_CALL_2) {
                int target = chunk->code[pc].operands[1];             // callee index
                if (target >= 0 && target < ctx->func_count &&
                    ctx->return_type[target] == JIT_RET_ANY)
                    needs_cleanup = true;                            // callee may return a mortal ref
                if ((op == OP_CALL_1 || op == OP_CALL_2) &&
                    target >= 0 && target < ctx->func_count &&
                    !ctx->strict_pure[func_idx] && ctx->strict_pure[target])
                    needs_cleanup = true;                            // strict callee: checked slow path bails
            } else if (op == OP_CALL_BUILTIN || op == OP_ASYNC_CALL_BUILTIN) {
                needs_cleanup = true;                                // builtin results may be mortal
            }
        }
    }
    // the walk releases every slot, so it must never observe stale stack data
    bool needs_init = !ctx->strict_pure[func_idx] || needs_cleanup;   // uninit slots must read as none

    if (cb->len + (size_t)range_size * 256 + 512 + 256 > cb->cap)     // capacity incl. many-arg stub
        JIT_FATAL("code buffer too small for function %d", func_idx);

    int32_t func_imms[8];                                            // distinct immediates
    int     func_n_imms = 0;                                         // number collected so far
    bool    func_imm_ok = true;                                      // all fit in the pool?
    for (int pc = start; pc < end; pc++) {
        Instruction* inst = &chunk->code[pc];                        // current instruction
        int32_t imm;                                                 // immediate operand
        switch (inst->opcode) {
            case OP_LOAD_NUM_IMM:
            case OP_RETURN_NUM_IMM: imm = inst->operands[1]; break;
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
                imm = inst->operands[2]; break;
            default: continue;
        }
        bool found = false;                                          // already pooled?
        for (int i = 0; i < func_n_imms; i++)
            if (func_imms[i] == imm) { found = true; break; }
        if (found) continue;
        if (func_n_imms >= 8) { func_imm_ok = false; break; }        // pool full
        func_imms[func_n_imms++] = imm;
    }
    if (!func_imm_ok) func_n_imms = 0;                               // give up on pooling

    int slot_pocket_gpr[JIT_MAX_SLOTS];                              // slot -> pocket gpr index, or -1
    int slot_to_pocket[X86_N_GPR_POCKETS];                           // pocket gpr index -> slot, or -1
    int n_pockets = 0;                                               // number of assigned pockets
    for (int s = 0; s < nregs; s++) slot_pocket_gpr[s] = -1;         // no pockets assigned
    for (int g = 0; g < X86_N_GPR_POCKETS; g++) slot_to_pocket[g] = -1;

    bool* is_target    = ctx->scratch_is_target;                     // shared jump-target marks
    int32_t* label_off = ctx->scratch_label_off;                     // shared label offset array
    JumpFixup* fixups  = ctx->scratch_fixups;                        // shared jump fixup array
    if (!is_target || !label_off || !fixups)                         // scratch arrays must exist
        JIT_FATAL("scratch arrays missing for function %d", func_idx);

    memset(is_target, 0, range_size * sizeof(bool));                 // clear jump-target marks

    pred_count  = (int*)         calloc(range_size, sizeof(int));    // per-label predecessor count
    needs_flush = (uint8_t*)     calloc(range_size, 1);              // per-label flush flag
    use_snap    = (uint8_t*)     calloc(range_size, 1);              // per-label snapshot flag
    jump_snap   = (JitCacheSnap*)calloc(range_size, sizeof(JitCacheSnap));  // per-label snapshots
    if (!pred_count || !needs_flush || !use_snap || !jump_snap)
        JIT_FATAL("scratch allocation failed for function %d", func_idx);

    for (int pc = start; pc < end; pc++) {                           // collect jump targets
        int t = branch_target_of(&chunk->code[pc]);                   // target per opcode shape
        if (t >= start && t < end) is_target[t - start] = true;      // mark as merge point
    }

    for (int pc = start + 1; pc < end; pc++) {                       // fall-through predecessors
        if (op_is_terminator(chunk->code[pc - 1].opcode)) continue;
        pred_count[pc - start]++;
    }
    for (int pc = start; pc < end; pc++) {                           // add jump predecessors
        int t = branch_target_of(&chunk->code[pc]);                   // target per opcode shape
        if (t < start || t >= end) continue;                         // out of range
        is_target[t - start] = true;                                 // mark as merge point
        pred_count[t - start]++;
    }

    for (int i = 0; i < range_size; i++) {                           // classify labels
        if (!is_target[i]) continue;                                 // pc is not a label
        int tgt = start + i;                                         // absolute target pc
        if (has_backward_jump(chunk, start, end, tgt) || pred_count[i] > 1)
            needs_flush[i] = 1;                                      // multi-pred / backward: flush
        else
            use_snap[i] = 1;                                         // unique forward jump: snapshot
    }

    for (int i = 0; i <= range_size; i++) label_off[i] = -1;         // incl. pc==end sentinel

    bool func_has_backward_jump = false;                             // dead-store elim gate
    for (int pc = start; pc < end; pc++) {
        Opcode op = chunk->code[pc].opcode;
        if (op == OP_JUMP ||
            (op >= OP_JUMP_IF_FALSE && op <= OP_JUMP_IF_GTE) ||
            (op >= OP_JUMP_IF_EQ_IMM && op <= OP_JUMP_IF_GTE_IMM) ||
            op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
            op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE ||
            op == OP_FOR_NEXT_LOOP ||                                 // loop back edges
            op == OP_TABLE_ITER_NEXT) {                               // iter exit branches
            func_has_backward_jump = true; break;
        }
    }

    size_t mark = cb->len;                                           // rollback point

    XmmCache cache_local;                                            // register cache state
    XmmCache* cache = &cache_local;
    x86_cache_clear(cache);                                          // start empty

    int pocket_slot_base = nregs;                                    // first slot for gpr saves
    int extra_slots = 0;                                             // basic-tier scratch slots
    int ret_slot = -1, pair_slot = -1;                               // cleanup staging slots
    int iter_depth_slot = -1, titer_depth_slot = -1;                 // iterator depth saves
    if (basic_mode || needs_cleanup) {                               // tier needs the scratch area
        ret_slot        = pocket_slot_base + n_pockets + extra_slots++;
        pair_slot       = pocket_slot_base + n_pockets + extra_slots++;  // pair[1] (val), higher addr
        extra_slots++;                                               // pair[0] (num) at pair_slot+1
        iter_depth_slot = pocket_slot_base + n_pockets + extra_slots++;
        titer_depth_slot = pocket_slot_base + n_pockets + extra_slots++;
    }
    int base_frame  = align16(8 * (nregs + n_pockets + extra_slots)); // incl. saves and scratch
    int frame_size  = base_frame + abi->frame_extra;                 // plus abi extras

    emit_prologue(cb, abi, frame_size, base_frame);                  // push rbp; mov rbp, rsp; sub rsp

    for (int g = 0; g < n_pockets; g++) {                            // save callee-saved gprs
        int gpr = x86_gpr_pocket_regs[g];                            // gpr to save
        int disp = -8 * (pocket_slot_base + g + 1);                  // matching stack slot
        x86_emit_store_r64_rbp(cb, gpr, disp);
    }

    if (needs_init) {                                                // unassigned slots read as none
        x86_emit_movabs_rax(cb, X86_NONE_BITS);                      // rax = none bit pattern
        for (int s = arity; s < nregs; s++)                          // args arrive in xmm regs
            x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(s));   // frame[s] = none
    }

    if (iter_depth_slot >= 0) {                                      // snapshot iterator depths
        x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&ctx->vm);      // rax = &ctx->vm
        x86_emit_load_r64_base(cb, X86_RAX, X86_RAX, 0);             // rax = ctx->vm
        emit_u8(cb, 0x8B); emit_u8(cb, 0x80);                        // mov eax, [rax+disp32]
        emit_i32(cb, (int32_t)offsetof(VM, iterator_depth));         // load iterator_depth
        x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(iter_depth_slot));
        x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&ctx->vm);      // rax = &ctx->vm
        x86_emit_load_r64_base(cb, X86_RAX, X86_RAX, 0);             // rax = ctx->vm
        emit_u8(cb, 0x8B); emit_u8(cb, 0x80);                        // mov eax, [rax+disp32]
        emit_i32(cb, (int32_t)offsetof(VM, table_iter_depth));       // load table_iter_depth
        x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(titer_depth_slot));
    }

    int cached_args = arity < XMM_CACHE_REGS ? arity : XMM_CACHE_REGS;  // args fitting in cache
    for (int i = 0; i < cached_args; i++)                            // cache incoming args
        x86_cache_put(cache, i, i);                                  // xmm<i> = slot i, dirty
    for (int i = cached_args; i < arity; i++)                        // spill extras
        x86_emit_movsd_store(cb, i, x86_slot_disp(i));               // xmm<i> -> slot i

    X86FuncEmitState st;                                             // one state for the whole fn
    memset(&st, 0, sizeof(st));
    st.abi = abi;  st.ctx = ctx;  st.cb = cb;  st.chunk = chunk;
    st.func_idx = func_idx;  st.start = start;  st.end = end;
    st.arity = arity;  st.nregs = nregs;  st.range_size = range_size;  st.mark = mark;
    st.is_pure = ctx->strict_pure[func_idx];                         // pick optimized vs checked emit path
    st.basic_mode = basic_mode;                                      // standard-opcode tier
    st.needs_cleanup = needs_cleanup;                                // walk slots at exits
    st.ret_slot = ret_slot;  st.pair_slot = pair_slot;               // scratch slot indices
    st.iter_depth_slot = iter_depth_slot;  st.titer_depth_slot = titer_depth_slot;
    st.n_bail_fixups = 0;                                            // no bail-outs yet
    st.bail_fixup_at = ctx->scratch_bail;                            // shared, one entry per pc
    st.n_ret_fixups = 0;                                            // no cleanup jumps yet
    st.ret_fixup_at = ctx->scratch_ret;                              // shared, one entry per pc
    st.is_target = is_target;  st.label_off = label_off;
    st.fixups = fixups;  st.fixup_count = 0;
    st.needs_flush = needs_flush;  st.use_snap = use_snap;  st.jump_snap = jump_snap;
    st.func_n_imms = func_n_imms;
    memcpy(st.func_imms, func_imms, sizeof(st.func_imms));
    st.rip_fixup_count = 0;  st.tab_fixup_count = 0;
    st.cache = cache;
    st.slot_pocket_gpr = slot_pocket_gpr;  st.slot_to_pocket = slot_to_pocket;
    st.n_pockets = n_pockets;
    st.pocket_slot_base = pocket_slot_base;  st.base_frame = base_frame;
    st.func_has_backward_jump = func_has_backward_jump;

    for (int pc = start; pc < end; pc++) {                           // emit every bytecode
        label_off[pc - start] = (int32_t)cb->len;                    // record label address

        if (use_snap[pc - start])                                    // unique forward-jump label
            x86_cache_restore(cache, &jump_snap[pc - start]);        // restore snapshotted state

        Instruction* inst = &chunk->code[pc];                        // current instruction
        st.pc = pc;
        st.inst = inst;
        st.d = inst->operands[0];                                    // destination operand
        st.a = inst->operands[1];                                    // first source operand
        st.b = inst->operands[2];                                    // second source operand
        st.op = inst->opcode;                                        // opcode shorthand
        st.did_flush = false;                                        // no flush yet this step
        st.prefer_xmm0 = false;                                      // default: no xmm0 hint

        if (!func_has_backward_jump &&                               // dead-store elimination
            op_writes_dest(st.op) &&
            (st.op == OP_MOVE || st.op == OP_LOAD_NUM_IMM || st.op == OP_LOAD_NUM ||
             st.op == OP_LOAD_BOOL || st.op == OP_LOAD_NONE ||
             st.op == OP_ADD || st.op == OP_SUB || st.op == OP_MUL || st.op == OP_DIV ||
             st.op == OP_ADD_IMM || st.op == OP_SUB_IMM || st.op == OP_MUL_IMM ||
             st.op == OP_DIV_IMM || st.op == OP_NEG || st.op == OP_INC || st.op == OP_DEC ||
             st.op == OP_CMP_EQ || st.op == OP_CMP_NEQ ||
             st.op == OP_CMP_EQ_NUM || st.op == OP_CMP_NEQ_NUM ||
             st.op == OP_CMP_LT || st.op == OP_CMP_GT ||
             st.op == OP_CMP_LTE || st.op == OP_CMP_GTE) &&
            st.d >= 0 && st.d < JIT_MAX_SLOTS &&
            !slot_read_before_write(chunk, pc + 1, end, st.d)) {
            int xi = cache->slot_reg[st.d];                          // cached register?
            if (xi >= 0) {                                           // invalidate without spill
                cache->reg_slot[xi] = -1;
                cache->slot_reg[st.d]  = -1;
                cache->slot_dirty[st.d] = false;
            }
            continue;                                                // skip dead instruction
        }

        if (pc + 1 < end) {                                          // prefer xmm0 for next RETURN?
            Instruction* nx = &chunk->code[pc + 1];
            if ((nx->opcode == OP_RETURN || nx->opcode == OP_RETURN_NUM ||
                 nx->opcode == OP_RETURN_BOOL) &&
                nx->operands[0] == st.d) {
                st.prefer_xmm0 = true;                               // next op returns our dest
            }
        }

        if (st.is_pure &&                                            // only fused in strict-pure bodies
            (st.op == OP_CMP_EQ || st.op == OP_CMP_EQ_NUM ||         // try cmp+jump fusion
             st.op == OP_CMP_NEQ || st.op == OP_CMP_NEQ_NUM ||
             st.op == OP_CMP_LT || st.op == OP_CMP_GT ||
             st.op == OP_CMP_LTE || st.op == OP_CMP_GTE) &&
            try_emit_fused_cmp_jump(&st)) {
            pc = st.pc;                                              // fusion advanced pc
            continue;
        }

        if (st.basic_mode && op_needs_pre_decref(st.op))               // interpreter decrefs
            x86_func_emit_pre_decref(&st);                           // old dest before storing

        x86_func_dispatch(&st);                                      // fan out to category handler

        if (!st.did_flush && pc + 1 < end && needs_flush[pc + 1 - start])
            x86_cache_flush(cache, cb);                              // merge-point flush
    }

    label_off[range_size] = (int32_t)cb->len;                        // pc==end target (fall through)

    if (needs_cleanup) {                                             // shared cleanup epilogue
        // bail / fallthrough stub: stage 0.0, then jump to the cleanup walk
        size_t bail_stub = cb->len;                                  // bail-outs land here
        x86_emit_movabs_rax(cb, 0x0ULL);                             // rax = 0.0 bits
        x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(ret_slot)); // ret_slot = 0.0
        emit_u8(cb, 0xE9);                                           // jmp cleanup_entry
        size_t bail_jmp_patch = cb->len;                             // forward jump placeholder
        emit_i32(cb, 0);

        size_t cleanup_entry = cb->len;                              // all exits converge here

        if (iter_depth_slot >= 0) {                                  // restore iterator depths
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&ctx->vm);  // rax = &ctx->vm
            x86_emit_load_r64_base(cb, X86_RAX, X86_RAX, 0);         // rax = ctx->vm
            x86_emit_load_r64_rbp(cb, X86_RCX, x86_slot_disp(iter_depth_slot));   // saved depth
            emit_u8(cb, 0x89); emit_u8(cb, 0x88);                    // mov [rax+disp32], ecx
            emit_i32(cb, (int32_t)offsetof(VM, iterator_depth));     // store iterator_depth
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&ctx->vm);  // rax = &ctx->vm
            x86_emit_load_r64_base(cb, X86_RAX, X86_RAX, 0);         // rax = ctx->vm
            x86_emit_load_r64_rbp(cb, X86_RCX, x86_slot_disp(titer_depth_slot));  // saved depth
            emit_u8(cb, 0x89); emit_u8(cb, 0x88);                    // mov [rax+disp32], ecx
            emit_i32(cb, (int32_t)offsetof(VM, table_iter_depth));   // store table_iter_depth
        }

        // walk every frame slot and release mortal references
        emit_u8(cb, 0x48); emit_u8(cb, 0xC7);                        // mov qword [rbp+disp32], 0
        emit_u8(cb, 0x85);                                           // modrm: [rbp+disp32], /0
        emit_i32(cb, (int32_t)x86_slot_disp(pair_slot));             // counter slot
        emit_i32(cb, 0);                                             // initial value zero
        size_t walk_top = cb->len;                                   // walk loop head
        x86_emit_load_r32_base(cb, X86_RCX, X86_RBP, (int32_t)x86_slot_disp(pair_slot));  // ecx = counter
        emit_u8(cb, 0x81); emit_u8(cb, 0xF9);                        // cmp ecx, nregs
        emit_i32(cb, nregs);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x83);                        // jae walk_done
        size_t walk_done_patch = cb->len;
        emit_i32(cb, 0);
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC8);     // mov rax, rcx
        emit_u8(cb, 0x48); emit_u8(cb, 0xC1); emit_u8(cb, 0xE0);     // shl rax, 3
        emit_u8(cb, 0x03);                                           // shift amount
        x86_emit_mov_r64_r64(cb, X86_RDX, X86_RBP);                  // rdx = rbp
        emit_u8(cb, 0x48); emit_u8(cb, 0x29); emit_u8(cb, 0xC2);     // sub rdx, rax
        emit_u8(cb, 0x48); emit_u8(cb, 0x8B); emit_u8(cb, 0x72);     // mov rsi, [rdx-8]
        emit_u8(cb, 0xF8);                                           // disp8 = -8
        x86_emit_movabs_r11(cb, X86_QNAN_MASK);                      // r11 = tag mask
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xF7);     // mov rdi, rsi
        emit_u8(cb, 0x4C); emit_u8(cb, 0x21); emit_u8(cb, 0xDF);     // and rdi, r11
        x86_emit_cmp_r64(cb, X86_RDI, X86_R11);                      // (v & qnan) == qnan?
        emit_u8(cb, 0x0F); emit_u8(cb, 0x85);                        // jne skip (untagged)
        size_t walk_skip_patch = cb->len;
        emit_i32(cb, 0);
        x86_emit_mov_r64_r64(cb, X86_RDI, X86_RSI);                  // arg0 = slot value
        x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&value_decref); // rax = &value_decref
        emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                        // call rax
        int32_t skip_rel = (int32_t)cb->len - (int32_t)(walk_skip_patch + 4);
        memcpy(cb->buf + walk_skip_patch, &skip_rel, 4);             // patch skip jump
        x86_emit_load_r32_base(cb, X86_RCX, X86_RBP, (int32_t)x86_slot_disp(pair_slot));  // ecx = counter
        emit_u8(cb, 0xFF); emit_u8(cb, 0xC1);                        // inc ecx
        x86_emit_store_r64_rbp(cb, X86_RCX, x86_slot_disp(pair_slot));  // counter = ecx
        emit_u8(cb, 0xE9);                                           // jmp walk_top
        emit_i32(cb, (int32_t)walk_top - (int32_t)(cb->len + 4));    // back edge rel32
        int32_t done_rel = (int32_t)cb->len - (int32_t)(walk_done_patch + 4);
        memcpy(cb->buf + walk_done_patch, &done_rel, 4);             // patch done jump

        x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(ret_slot)); // rax = staged return value
        x86_emit_movq_xmm_rax(cb, 0);                                // xmm0 = return value
        emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);   // restore pockets before leaving
        emit_leave_ret(cb, abi, base_frame);                         // leave; ret

        // patch the stub's forward jump and every recorded bail-out / return
        int32_t stub_rel = (int32_t)cleanup_entry - (int32_t)(bail_jmp_patch + 4);
        memcpy(cb->buf + bail_jmp_patch, &stub_rel, 4);              // stub jumps into the walk
        for (int i = 0; i < st.n_bail_fixups; i++) {                 // point every bail-out at the stub
            int32_t rel = (int32_t)bail_stub - (int32_t)(st.bail_fixup_at[i] + 4);
            memcpy(cb->buf + st.bail_fixup_at[i], &rel, 4);          // je -> zero-return stub
        }
        for (int i = 0; i < st.n_ret_fixups; i++) {                  // returns jump to the walk
            int32_t rel = (int32_t)cleanup_entry - (int32_t)(st.ret_fixup_at[i] + 4);
            memcpy(cb->buf + st.ret_fixup_at[i], &rel, 4);           // jmp -> cleanup entry
        }
    } else {
        emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);   // restore pockets before leaving
        emit_return_zero(abi, cb, base_frame);                       // safety fallthrough
    }

    size_t pool_off = cb->len;                                       // constant pool starts here
    for (int i = 0; i < func_n_imms; i++) {                          // append double constants
        double dv = (double)func_imms[i];
        uint64_t bits; memcpy(&bits, &dv, 8);
        emit_u64(cb, bits);
    }
    for (int i = 0; i < st.rip_fixup_count; i++) {                   // patch rip-relative disp32s
        size_t patch_at = st.rip_fixup_at[i];
        size_t imm_at   = pool_off + 8 * (size_t)st.rip_fixup_imm[i];
        int32_t rel = (int32_t)imm_at - (int32_t)(patch_at + 4);
        memcpy(cb->buf + patch_at, &rel, 4);
    }

    for (int i = 0; i < st.fixup_count; i++) {                       // patch every pending jump
        JumpFixup* fx = &fixups[i];
        int tidx = fx->target_pc - start;                            // index within range
        if (tidx < 0 || tidx > range_size || label_off[tidx] < 0)
            JIT_FATAL("jump fixup out of range: func=%d target_pc=%d",
                      func_idx, fx->target_pc);
        int32_t rel = (int32_t)label_off[tidx] - (int32_t)(fx->patch_at + 4);
        memcpy(cb->buf + fx->patch_at, &rel, 4);
    }

    for (int i = 0; i < st.tab_fixup_count; i++) {                   // patch every dispatch slot
        int tidx = st.tab_fixup_pc[i] - start;
        if (tidx < 0 || tidx > range_size || label_off[tidx] < 0)
            JIT_FATAL("jump-table slot out of range: func=%d target_pc=%d",
                      func_idx, st.tab_fixup_pc[i]);
        uint64_t addr = (uint64_t)(uintptr_t)(cb->buf + label_off[tidx]);
        memcpy(cb->buf + st.tab_fixup_at[i], &addr, 8);
    }

    free(pred_count); free(needs_flush); free(use_snap); free(jump_snap);

    // many-arg entry stub: rdi (sysv) / rcx (win64) points at an array of raw
    // value bits whose references transfer to the callee
    if (arity <= JIT_MAX_CALL_ARITY) {                                 // stub only needed up to the cap
        size_t stub = cb->len;                                        // stub entry offset
#if defined(_WIN32) || defined(_WIN64)
        int arg_reg = X86_RCX;                                        // win64 first integer parameter
#else
        int arg_reg = X86_RDI;                                        // sysv first integer parameter
#endif
        for (int i = 0; i < arity; i++) {                             // stage every argument
            emit_u8(cb, 0xF3); emit_u8(cb, 0x0F); emit_u8(cb, 0x7E);  // movq xmm, m64 opcode
            emit_u8(cb, (uint8_t)(0x40 | (i << 3) | arg_reg));        // modrm: mod=01, xmm<i>, [arg_reg+disp8]
            emit_u8(cb, (uint8_t)(8 * i));                            // displacement byte
        }
        x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)(cb->buf + mark));  // rax = main body entry
        emit_u8(cb, 0xFF); emit_u8(cb, 0xE0);                         // jmp rax (tail call)
        ctx->many_table[func_idx] = (void*)(cb->buf + stub);          // publish many-arg entry
    }

    *out_fn = (void*)(cb->buf + mark);                               // publish general entry
    jit_bail_jmp = prev_bail;                                        // leave the TLS as we found it
    return true;
}
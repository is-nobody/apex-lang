// source/jit/x86-64/x86_64_emit_function.c
// Function emitter for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include "vm.h"
#include <stddef.h>
#include <string.h>

// callee-saved gprs usable as slot pockets on all supported abis
#define X86_N_GPR_POCKETS 5

static const int x86_gpr_pocket_regs[X86_N_GPR_POCKETS] = {
    X86_RBX, X86_R12, X86_R13, X86_R14, X86_R15
};

// emits load r64, [rbp+disp32] for each assigned pocket gpr before leave; ret
static void emit_gpr_pocket_restores(CodeBuf* cb, int pocket_slot_base,
                                     int n_pockets) {
    for (int g = 0; g < n_pockets; g++) {                    // restore saved gprs
        int gpr = x86_gpr_pocket_regs[g];
        int disp = -8 * (pocket_slot_base + g + 1);
        x86_emit_load_r64_rbp(cb, gpr, disp);
    }
}

// emits native x86-64 code for a single numeric-pure function
bool x86_64_emit_function(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                          int func_idx, void** out_fn) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];                      // first bytecode pc
    int end   = ctx->range_end[func_idx];                        // one past last pc
    int arity = chunk->functions[func_idx].arity;                // parameter count
    int nregs = chunk->functions[func_idx].max_registers;        // max register index + 1
    if (nregs < arity) nregs = arity;                            // params need slots too
    if (nregs < 1) nregs = 1;                                    // at least one slot
    if (nregs > JIT_MAX_SLOTS) return false;                     // cache can't track this

    int range_size = end - start;                                // number of bytecodes
    if (range_size <= 0) return false;                           // empty body

    if (cb->len + (size_t)range_size * 256 + 512 > cb->cap)
        JIT_FATAL("code buffer too small for function %d (need≈%zu, cap=%zu)",
                func_idx, cb->len + (size_t)range_size * 256 + 512, cb->cap);

    // precompute distinct immediates used in this function
    int32_t func_imms[8];
    int     func_n_imms = 0;
    bool    func_imm_ok = true;
    for (int pc = start; pc < end; pc++) {
        Instruction* inst = &chunk->code[pc];
        int32_t imm;
        switch (inst->opcode) {
            case OP_LOAD_NUM_IMM:
            case OP_RETURN_NUM_IMM:
                imm = inst->operands[1]; break;
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
                imm = inst->operands[2]; break;
            default: continue;
        }
        bool found = false;
        for (int i = 0; i < func_n_imms; i++)
            if (func_imms[i] == imm) { found = true; break; }
        if (found) continue;
        if (func_n_imms >= 8) { func_imm_ok = false; break; }
        func_imms[func_n_imms++] = imm;
    }
    if (!func_imm_ok) func_n_imms = 0;

    // gpr pockets are disabled: on float-heavy bodies the xmm<->gpr round-trip costs more than it saves
    int slot_pocket_gpr[JIT_MAX_SLOTS];                      // slot -> pocket gpr index, or -1
    int slot_to_pocket[X86_N_GPR_POCKETS];                   // pocket gpr index -> slot, or -1
    int n_pockets = 0;                                       // number of assigned pockets
    for (int s = 0; s < nregs; s++) slot_pocket_gpr[s] = -1;
    for (int g = 0; g < X86_N_GPR_POCKETS; g++) slot_to_pocket[g] = -1;

    bool* is_target     = ctx->scratch_is_target;                // shared jump-target marks
    int32_t* label_off  = ctx->scratch_label_off;                // shared label offset array
    JumpFixup* fixups   = ctx->scratch_fixups;                   // shared jump fixup array
    if (!is_target || !label_off || !fixups) {
        JIT_FATAL("scratch arrays missing for function %d", func_idx);
    }

    memset(is_target, 0, range_size * sizeof(bool));             // clear jump-target marks

    int*          pred_count  = (int*)         calloc(range_size, sizeof(int));
    uint8_t*      needs_flush = (uint8_t*)     calloc(range_size, 1);
    uint8_t*      use_snap    = (uint8_t*)     calloc(range_size, 1);
    JitCacheSnap* jump_snap   = (JitCacheSnap*)calloc(range_size, sizeof(JitCacheSnap));
    if (!pred_count || !needs_flush || !use_snap || !jump_snap) {
        JIT_FATAL("scratch allocation failed for function %d (range_size=%d)", func_idx, range_size);
    }
    for (int pc = start; pc < end; pc++) {                       // collect jump targets
        Opcode op = chunk->code[pc].opcode;
        if (op == OP_JUMP || op == OP_JUMP_IF_FALSE ||
            op == OP_JUMP_IF_EQ || op == OP_JUMP_IF_NEQ ||
            op == OP_JUMP_IF_EQ_NUM || op == OP_JUMP_IF_NEQ_NUM ||
            op == OP_JUMP_IF_LT || op == OP_JUMP_IF_GT ||
            op == OP_JUMP_IF_LTE || op == OP_JUMP_IF_GTE ||
            op == OP_JUMP_IF_EQ_IMM || op == OP_JUMP_IF_NEQ_IMM ||
            op == OP_JUMP_IF_LT_IMM || op == OP_JUMP_IF_GT_IMM ||
            op == OP_JUMP_IF_LTE_IMM || op == OP_JUMP_IF_GTE_IMM ||
            op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
            op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) {
            int t = chunk->code[pc].operands[0];                 // jump target
            if (t >= start && t < end) is_target[t - start] = true;  // mark as merge point
        }
    }

    for (int pc = start + 1; pc < end; pc++) {                   // fall-through predecessors
        if (op_is_terminator(chunk->code[pc - 1].opcode)) continue;
        pred_count[pc - start]++;
    }
    for (int pc = start; pc < end; pc++) {                       // add jump predecessors
        Opcode op = chunk->code[pc].opcode;
        if (op == OP_JUMP || op == OP_JUMP_IF_FALSE ||
            op == OP_JUMP_IF_EQ || op == OP_JUMP_IF_NEQ ||
            op == OP_JUMP_IF_EQ_NUM || op == OP_JUMP_IF_NEQ_NUM ||
            op == OP_JUMP_IF_LT || op == OP_JUMP_IF_GT ||
            op == OP_JUMP_IF_LTE || op == OP_JUMP_IF_GTE ||
            op == OP_JUMP_IF_EQ_IMM || op == OP_JUMP_IF_NEQ_IMM ||
            op == OP_JUMP_IF_LT_IMM || op == OP_JUMP_IF_GT_IMM ||
            op == OP_JUMP_IF_LTE_IMM || op == OP_JUMP_IF_GTE_IMM ||
            op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
            op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) {
            int t = chunk->code[pc].operands[0];                 // jump target
            if (t < start || t >= end) continue;                 // out of range, ignore
            is_target[t - start] = true;                         // mark as merge point
            pred_count[t - start]++;
        }
    }

    // needs_flush: label must be entered with an empty cache (multi-pred or backward)
    for (int i = 0; i < range_size; i++) {                       // classify every label
        if (!is_target[i]) continue;                             // pc is not a label
        int tgt = start + i;                                     // absolute target pc
        if (has_backward_jump(chunk, start, end, tgt) || pred_count[i] > 1)
            needs_flush[i] = 1;                                  // multi-pred / backward: flush
        else
            use_snap[i] = 1;                                     // unique forward jump: snapshot
    }

    for (int i = 0; i <= range_size; i++) label_off[i] = -1;     // incl. the pc==end sentinel
    int fixup_count = 0;                                         // pending jumps count

    // dead-store elimination below assumes straight-line code: disable it
    // whenever the function contains any jump (forward OR backward)
    bool func_has_backward_jump = false;
    for (int pc = start; pc < end; pc++) {
        Opcode op = chunk->code[pc].opcode;
        if (op == OP_JUMP ||
            (op >= OP_JUMP_IF_FALSE && op <= OP_JUMP_IF_GTE) ||
            (op >= OP_JUMP_IF_EQ_IMM && op <= OP_JUMP_IF_GTE_IMM) ||
            op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
            op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) {
            func_has_backward_jump = true; break;
        }
    }

    size_t rip_fixup_at[64];                                     // rip-relative disp32 offsets
    int    rip_fixup_imm[64];                                    // imm index per fixup
    int    rip_fixup_count = 0;                                  // pending rip fixups

    size_t tab_fixup_at[256];                                    // inline dispatch tables: 8-byte slots
    int    tab_fixup_pc[256];                                    // bytecode pc each slot must point at
    int    tab_fixup_count = 0;                                  // number of pending table slots

    // try the int-specialized body first: on success it lands before the general body
    size_t int_off   = (size_t)-1;                               // int body offset, -1 if absent
    int    int_max_n = 0;                                        // wrapper upper bound
    if (matches_int_self_recursive(ctx, func_idx)) {
        if (x86_64_emit_int_self_recursive(abi, ctx, cb, func_idx, &int_off)) {
            int_max_n = 40;                                      // conservative: keeps results < 2^53
        } else {
            int_off = (size_t)-1;
        }
    }

    size_t mark = cb->len;                                       // rollback point

    XmmCache cache;                                              // register cache state
    (void)jump_snap;                                             // read on labels, written at jumps
    x86_cache_clear(&cache);                                     // start empty

    int pocket_slot_base = nregs;                                // first slot for gpr saves
    int base_frame  = align16(8 * (nregs + n_pockets));          // incl. gpr save slots only
    int frame_size  = base_frame + abi->frame_extra;             // plus abi extras

    emit_prologue(cb, abi, frame_size, base_frame);

    for (int g = 0; g < n_pockets; g++) {                        // save callee-saved gprs
        int gpr = x86_gpr_pocket_regs[g];
        int disp = -8 * (pocket_slot_base + g + 1);
        x86_emit_store_r64_rbp(cb, gpr, disp);
    }

    int cached_args = arity < XMM_CACHE_REGS ? arity : XMM_CACHE_REGS;  // args fit in cache
    for (int i = 0; i < cached_args; i++) {                      // cache incoming args
        x86_cache_put(&cache, i, i);                             // xmm<i> = slot i, dirty
    }
    for (int i = cached_args; i < arity; i++) {                  // spill extras
        x86_emit_movsd_store(cb, i, x86_slot_disp(i));           // xmm<i> -> slot i
    }

    for (int pc = start; pc < end; pc++) {                       // emit each bytecode
        label_off[pc - start] = (int32_t)cb->len;                // record label address

        if (use_snap[pc - start]) {                              // unique forward-jump label
            x86_cache_restore(&cache, &jump_snap[pc - start]);   // restore snapshotted state
        }

        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // first operand
        int a = inst->operands[1];                               // second operand
        int b = inst->operands[2];                               // third operand
        Opcode op = inst->opcode;                                // opcode shorthand
        bool did_flush = false;                                  // cache flushed this step

        // dead xmm write: pure op writing to a slot never read before its next overwrite
        if (!func_has_backward_jump &&
            op_writes_dest(op) &&
            (op == OP_MOVE || op == OP_LOAD_NUM_IMM || op == OP_LOAD_NUM ||
             op == OP_LOAD_BOOL || op == OP_LOAD_NONE ||
             op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV ||
             op == OP_ADD_IMM || op == OP_SUB_IMM || op == OP_MUL_IMM ||
             op == OP_DIV_IMM || op == OP_NEG || op == OP_INC || op == OP_DEC ||
             op == OP_CMP_EQ || op == OP_CMP_NEQ ||
             op == OP_CMP_EQ_NUM || op == OP_CMP_NEQ_NUM ||
             op == OP_CMP_LT || op == OP_CMP_GT ||
             op == OP_CMP_LTE || op == OP_CMP_GTE ||
             op == OP_TABLE_GET || op == OP_TABLE_GET_INT || op == OP_TABLE_GET_NUM) &&
            d >= 0 && d < JIT_MAX_SLOTS &&
            !slot_read_before_write(chunk, pc + 1, end, d)) {
            int xi = cache.slot_reg[d];
            if (xi >= 0) {                                       // invalidate without spill
                cache.reg_slot[xi] = -1;
                cache.slot_reg[d]  = -1;
                cache.slot_dirty[d] = false;
            }
            continue;                                            // skip to next instruction
        }

        bool prefer_xmm0 = false;                                // next op returns this dest?
        if (pc + 1 < end) {                                      // next instruction exists
            Instruction* nx = &chunk->code[pc + 1];
            if ((nx->opcode == OP_RETURN || nx->opcode == OP_RETURN_NUM ||
                 nx->opcode == OP_RETURN_BOOL) &&
                nx->operands[0] == d) {                          // next returns our dest
                prefer_xmm0 = true;                              // prefer xmm0 as result
            }
        }

        // fused emit: CMP_* d,a,b directly followed by JUMP_IF_FALSE d
        if (pc + 1 < end && !is_target[pc + 1 - start] &&
            chunk->code[pc + 1].opcode == OP_JUMP_IF_FALSE &&
            chunk->code[pc + 1].operands[1] == d) {
            uint8_t fused_jcc = 0;                               // inverted jcc for "jump on false"
            switch (op) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  fused_jcc = 0x85; break;  // jne
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: fused_jcc = 0x84; break;  // je
                case OP_CMP_LT:  fused_jcc = 0x83; break;                       // jae
                case OP_CMP_GT:  fused_jcc = 0x86; break;                       // jbe
                case OP_CMP_LTE: fused_jcc = 0x87; break;                       // ja
                case OP_CMP_GTE: fused_jcc = 0x82; break;                       // jb
                default: break;
            }
            int fuse_target = fused_jcc ? chunk->code[pc + 1].operands[0] : -1;
            bool d_dead = fused_jcc && fuse_target >= 0 &&
                          !slot_read_before_write(chunk, pc + 2, end, d) &&
                          (fuse_target < pc + 2 ||
                           !slot_read_before_write(chunk, fuse_target, end, d));
            if (d_dead) {
                int xa = x86_cache_load(&cache, cb, a);          // load left operand
                if (xa < 0) JIT_FATAL("cache full: fused CMP a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load right, avoid xa
                if (xb < 0) JIT_FATAL("cache full: fused CMP b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (fuse_target >= start && fuse_target < end) {
                    if (needs_flush[fuse_target - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[fuse_target - start]) x86_cache_snap(&jump_snap[fuse_target - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, fused_jcc);       // jcc rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = fuse_target;     // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                pc++;                                            // skip the JUMP_IF_FALSE
                if (pc + 1 < end && needs_flush[pc + 1 - start]) {
                    x86_cache_flush(&cache, cb);                 // fall-through merge flush
                }
                continue;                                        // skip the normal switch
            }
        }

        switch (op) {
            case OP_MOVE: {                                      // reg-to-reg copy
                int xa = x86_cache_load(&cache, cb, a);          // find xmm for source
                if (xa < 0) JIT_FATAL("cache full: MOVE src=%d (func=%d pc=%d)", a, func_idx, pc);
                x86_cache_put(&cache, xa, d);                    // relabel xmm as dest
                break;
            }
            case OP_LOAD_NUM_IMM: {                              // small int literal
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) JIT_FATAL("cache full: LOAD_NUM_IMM dst=%d (func=%d pc=%d)", d, func_idx, pc);
                int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == a) { imm_idx = i; break; }
                if (imm_idx >= 0) {
                    size_t at = x86_emit_movsd_load_rip(cb, x);  // movsd xmm, [rip+disp32]
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                } else {
                    x86_emit_load_double_imm(cb, x, a);
                }
                x86_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_LOAD_NUM: {                                  // full double constant
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) JIT_FATAL("cache full: LOAD_NUM dst=%d (func=%d pc=%d)", d, func_idx, pc);
                double v = chunk->constants[a].number_value;     // fetch constant
                uint64_t bits; memcpy(&bits, &v, 8);             // reinterpret as u64
                x86_emit_movabs_rax(cb, bits);                   // rax = bit pattern
                x86_emit_movq_xmm_rax(cb, x);                    // xmmX = rax
                x86_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_LOAD_BOOL: {                                 // boolean literal
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) JIT_FATAL("cache full: LOAD_BOOL dst=%d (func=%d pc=%d)", d, func_idx, pc);
                uint64_t bits = X86_BOOL_BITS | (a ? 1ULL : 0ULL);  // bit 0 carries the value
                x86_emit_movabs_rax(cb, bits);                   // rax = nan-boxed bool
                x86_emit_movq_xmm_rax(cb, x);                    // xmmX = rax
                x86_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_LOAD_NONE: {                                 // none literal
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) JIT_FATAL("cache full: LOAD_NONE dst=%d (func=%d pc=%d)", d, func_idx, pc);
                x86_emit_movabs_rax(cb, X86_NONE_BITS);          // rax = NONE bit pattern
                x86_emit_movq_xmm_rax(cb, x);                    // xmmX = rax
                x86_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV: {                                       // binary arithmetic
                int xa = x86_cache_load(&cache, cb, a);          // load left operand
                if (xa < 0) JIT_FATAL("cache full: arith a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load right, avoid xa
                if (xb < 0) JIT_FATAL("cache full: arith b=%d (func=%d pc=%d op=%d)", b, func_idx, pc, op);
                uint8_t arith_op;                                // sse opcode
                switch (op) {
                    case OP_ADD: arith_op = 0x58; break;         // addsd
                    case OP_SUB: arith_op = 0x5C; break;         // subsd
                    case OP_MUL: arith_op = 0x59; break;         // mulsd
                    default:     arith_op = 0x5E; break;         // divsd
                }
                bool commutative = (op == OP_ADD || op == OP_MUL);  // swap-safe op?
                if (prefer_xmm0 && commutative && xb == 0 && xa != 0) {
                    // xmm0 (holding b) is overwritten with the result
                    if (d != b && cache.slot_dirty[b] &&
                        slot_read_before_write(chunk, pc + 1, end, b)) {
                        x86_emit_movsd_store(cb, 0, x86_slot_disp(b));  // preserve b
                        cache.slot_dirty[b] = false;
                    }
                    x86_emit_sse_arith_rr(cb, arith_op, 0, xa);  // xmm0 = xmm0 op xa
                    x86_cache_put(&cache, 0, d);                 // relabel xmm0 as dest
                } else {
                    // xa (holding a) is overwritten with the result
                    if (d != a && cache.slot_dirty[a] &&
                        slot_read_before_write(chunk, pc + 1, end, a)) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));  // preserve a
                        cache.slot_dirty[a] = false;
                    }
                    x86_emit_sse_arith_rr(cb, arith_op, xa, xb); // xa op= xb
                    x86_cache_put(&cache, xa, d);                // relabel xa as dest
                }
                break;
            }
            case OP_ADD_IMM:
            case OP_SUB_IMM:
            case OP_MUL_IMM:
            case OP_DIV_IMM: {                                   // binary arithmetic with immediate
                int xa = x86_cache_load(&cache, cb, a);
                if (xa < 0) JIT_FATAL("cache full: arith_imm a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                uint8_t arith_op;
                switch (op) {
                    case OP_ADD_IMM: arith_op = 0x58; break;
                    case OP_SUB_IMM: arith_op = 0x5C; break;
                    case OP_MUL_IMM: arith_op = 0x59; break;
                    default:         arith_op = 0x5E; break;
                }
                int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == b) { imm_idx = i; break; }
                bool preserve = (d != a) &&
                    slot_read_before_write(chunk, pc + 1, end, a);
                if (imm_idx >= 0) {
                    if (preserve) {
                        int xd = x86_cache_alloc_excl(&cache, cb, xa, -1);
                        if (xd >= 0) {
                            x86_emit_sse66_rr(cb, 0x28, xd, xa);
                            size_t at = x86_emit_sse_arith_rip(cb, arith_op, xd);
                            rip_fixup_at[rip_fixup_count] = at;
                            rip_fixup_imm[rip_fixup_count] = imm_idx;
                            rip_fixup_count++;
                            x86_cache_put(&cache, xd, d);
                            break;
                        }
                    }
                    size_t at = x86_emit_sse_arith_rip(cb, arith_op, xa);
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                    x86_cache_put(&cache, xa, d);
                    break;
                }
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                if (preserve) {
                    int xd = x86_cache_alloc_excl(&cache, cb, xa, XMM_SCRATCH);
                    if (xd >= 0) {
                        x86_emit_sse66_rr(cb, 0x28, xd, xa);
                        x86_emit_sse_arith_rr(cb, arith_op, xd, XMM_SCRATCH);
                        x86_cache_put(&cache, xd, d);
                        break;
                    }
                }
                x86_emit_sse_arith_rr(cb, arith_op, xa, XMM_SCRATCH);
                x86_cache_put(&cache, xa, d);
                break;
            }
            case OP_MOD_IMM: {                                   // modulo with immediate: a - trunc(a/imm)*imm
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: MOD_IMM a=%d (func=%d pc=%d)", a, func_idx, pc);
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);    // xmm7 = (double)b (cvtsi2sd)
                int xt = x86_cache_alloc_excl(&cache, cb, xa, XMM_SCRATCH);  // temp for a/imm
                if (xt < 0) JIT_FATAL("cache full: MOD_IMM temp (func=%d pc=%d)", func_idx, pc);
                x86_emit_sse66_rr(cb, 0x28, xt, xa);             // xt = a
                x86_emit_sse_arith_rr(cb, 0x5E, xt, XMM_SCRATCH);// xt = a / imm
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, xt, xt);                          // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);                               // roundsd opcode
                emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));  // round xt, xt
                emit_u8(cb, 0x03);                               // round mode: truncate
                x86_emit_sse_arith_rr(cb, 0x59, xt, XMM_SCRATCH);// xt = trunc(a/imm) * imm
                // dest != source: a must survive the in-place subtract
                if (d != a && cache.slot_dirty[a] &&
                    slot_read_before_write(chunk, pc + 1, end, a)) {
                    x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                    cache.slot_dirty[a] = false;
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xa, xt);         // a = a - xt
                x86_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_MOD: {                                       // x - trunc(x/y)*y
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: MOD a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: MOD b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);    // movapd scratch, a
                x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);  // divsd  scratch, b
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, XMM_SCRATCH, XMM_SCRATCH);        // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);                               // roundsd opcode
                emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3) | (XMM_SCRATCH & 7));  // modrm scratch, scratch
                emit_u8(cb, 0x03);                               // round mode: truncate
                x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);  // mulsd  scratch, b
                // dest != source: a must survive the in-place subtract
                if (d != a && cache.slot_dirty[a] &&
                    slot_read_before_write(chunk, pc + 1, end, a)) {
                    x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                    cache.slot_dirty[a] = false;
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);  // subsd  a, scratch
                x86_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_NEG: {                                       // unary minus
                int xa = x86_cache_load(&cache, cb, a);          // load operand
                if (xa < 0) JIT_FATAL("cache full: NEG a=%d (func=%d pc=%d)", a, func_idx, pc);
                x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // xorpd scratch, scratch
                x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);  // subsd scratch, a
                // dest != source: a must survive the in-place movapd
                if (d != a && cache.slot_dirty[a] &&
                    slot_read_before_write(chunk, pc + 1, end, a)) {
                    x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                    cache.slot_dirty[a] = false;
                }
                x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);    // movapd a, scratch
                x86_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_INC:
            case OP_DEC: {                                       // in-place +1 / -1
                int xd = x86_cache_load(&cache, cb, d);          // load dest
                if (xd < 0) JIT_FATAL("cache full: INC/DEC dst=%d (func=%d pc=%d op=%d)", d, func_idx, pc, op);
                x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);  // rax = 1.0 bits
                x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);          // scratch = 1.0
                uint8_t arith_op = (op == OP_INC) ? 0x58 : 0x5C; // addsd / subsd
                x86_emit_sse_arith_rr(cb, arith_op, xd, XMM_SCRATCH);  // xd op= 1.0
                x86_cache_put(&cache, xd, d);                    // mark dirty
                break;
            }
            case OP_CMP_EQ:
            case OP_CMP_EQ_NUM: {                                // d = (a == b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: CMP a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: CMP b=%d (func=%d pc=%d op=%d)", b, func_idx, pc, op);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (func=%d pc=%d op=%d)", d, func_idx, pc, op);
                x86_emit_cmp_box_result(cb, xd, 0x94);           // sete al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_NEQ:
            case OP_CMP_NEQ_NUM: {                               // d = (a != b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: CMP a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: CMP b=%d (func=%d pc=%d op=%d)", b, func_idx, pc, op);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (func=%d pc=%d op=%d)", d, func_idx, pc, op);
                x86_emit_cmp_box_result(cb, xd, 0x95);           // setne al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LT: {                                    // d = (a < b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: CMP a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: CMP b=%d (func=%d pc=%d op=%d)", b, func_idx, pc, op);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (func=%d pc=%d op=%d)", d, func_idx, pc, op);
                x86_emit_cmp_box_result(cb, xd, 0x92);           // setb al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GT: {                                    // d = (a > b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: CMP a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: CMP b=%d (func=%d pc=%d op=%d)", b, func_idx, pc, op);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (func=%d pc=%d op=%d)", d, func_idx, pc, op);
                x86_emit_cmp_box_result(cb, xd, 0x97);           // seta al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LTE: {                                   // d = (a <= b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: CMP a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: CMP b=%d (func=%d pc=%d op=%d)", b, func_idx, pc, op);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (func=%d pc=%d op=%d)", d, func_idx, pc, op);
                x86_emit_cmp_box_result(cb, xd, 0x96);           // setbe al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GTE: {                                   // d = (a >= b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: CMP a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: CMP b=%d (func=%d pc=%d op=%d)", b, func_idx, pc, op);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (func=%d pc=%d op=%d)", d, func_idx, pc, op);
                x86_emit_cmp_box_result(cb, xd, 0x93);           // setae al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_JUMP: {                                      // unconditional jump
                if (d >= start && d < end) {                     // in-range target only
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);                 // out of range: safest
                }
                emit_u8(cb, 0xE9);                               // jmp rel32 opcode
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_FALSE: {                             // branch when cond is false
                int xa = x86_cache_load(&cache, cb, a);          // load condition
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_FALSE cond=%d (func=%d pc=%d)", a, func_idx, pc);
                x86_emit_movq_rax_xmm(cb, xa);                   // rax = raw 64-bit slot
                emit_u8(cb, 0xA8); emit_u8(cb, 0x01);            // test al, 1
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je rel32 (bit0 == 0 -> false)
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_EQ:
            case OP_JUMP_IF_EQ_NUM: {                            // branch if a == b
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_EQ a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: JUMP_IF_EQ b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_NEQ_NUM: {                           // branch if a != b
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_NEQ a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: JUMP_IF_NEQ b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x85);            // jne rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_LT: {                                // branch if a < b
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_LT a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: JUMP_IF_LT b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x82);            // jb rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_GT: {                                // branch if a > b
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_GT a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: JUMP_IF_GT b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x87);            // ja rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_LTE: {                               // branch if a <= b
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_LTE a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: JUMP_IF_LTE b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x86);            // jbe rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_GTE: {                               // branch if a >= b
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_GTE a=%d (func=%d pc=%d)", a, func_idx, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) JIT_FATAL("cache full: JUMP_IF_GTE b=%d (func=%d pc=%d)", b, func_idx, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x83);            // jae rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_EQ_IMM:                              // branch if a == imm
            case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM:
            case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM:
            case OP_JUMP_IF_GTE_IMM: {                           // imm-jump variants share the pattern
                int xa = x86_cache_load(&cache, cb, a);          // load left operand
                if (xa < 0) JIT_FATAL("cache full: JUMP_IF_IMM a=%d (func=%d pc=%d op=%d)", a, func_idx, pc, op);
                int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == b) { imm_idx = i; break; }
                if (imm_idx >= 0) {
                    size_t at = x86_emit_ucomisd_rip(cb, xa);    // ucomisd xa, [rip+disp32]
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                } else {
                    x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                    x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);
                }
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                uint8_t jcc;
                switch (op) {
                    case OP_JUMP_IF_EQ_IMM:  jcc = 0x84; break;  // je
                    case OP_JUMP_IF_NEQ_IMM: jcc = 0x85; break;  // jne
                    case OP_JUMP_IF_LT_IMM:  jcc = 0x82; break;  // jb
                    case OP_JUMP_IF_GT_IMM:  jcc = 0x87; break;  // ja
                    case OP_JUMP_IF_LTE_IMM: jcc = 0x86; break;  // jbe
                    default:                 jcc = 0x83; break;  // jae (gte)
                }
                emit_u8(cb, 0x0F); emit_u8(cb, jcc);             // conditional jump
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_NUM: {                            // jump if R[a] is a number == const[b]
                int xa = x86_cache_load(&cache, cb, a);          // load subject
                if (xa < 0) JIT_FATAL("cache full: JUMP_MATCH_NUM subj=%d (func=%d pc=%d)", a, func_idx, pc);
                double c = chunk->constants[b].number_value;     // fetch constant
                uint64_t cbits; memcpy(&cbits, &c, 8);           // reinterpret as u64
                x86_emit_movabs_rax(cb, cbits);                  // rax = constant bits
                x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);          // scratch = constant
                x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);        // ucomisd subj, const
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x8A);            // jp skip (subj was NaN-tagged)
                size_t skip_patch = cb->len;
                emit_i32(cb, 0);
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je target
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                int32_t skip_rel = (int32_t)cb->len - (int32_t)(skip_patch + 4);
                memcpy(cb->buf + skip_patch, &skip_rel, 4);      // patch skip
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_STR: {                            // jump if R[a] is a string == const[b]
                x86_cache_flush(&cache, cb);                     // call clobbers xmm
#if defined(_WIN32) || defined(_WIN64)
                x86_emit_load_r64_rbp(cb, X86_RCX, x86_slot_disp(a));  // win64: rcx = subject
#else
                x86_emit_load_r64_rbp(cb, X86_RDI, x86_slot_disp(a));  // sysv: rdi = subject
#endif
                uint64_t addr = (uint64_t)(uintptr_t)&chunk->constants[b].cached_str;
                x86_emit_movabs_rax(cb, addr);                   // rax = &cached_str
#if defined(_WIN32) || defined(_WIN64)
                x86_emit_load_r64_base(cb, X86_RDX, X86_RAX, 0); // win64: rdx = cached_str
#else
                x86_emit_load_r64_base(cb, X86_RSI, X86_RAX, 0); // sysv: rsi = cached_str
#endif
                uint64_t helper = (uint64_t)(uintptr_t)&jit_match_str;
                x86_emit_movabs_rax(cb, helper);                 // rax = helper
                emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);            // call rax
                emit_u8(cb, 0x85); emit_u8(cb, 0xC0);            // test eax, eax
                x86_cache_clear(&cache);                         // callee clobbered xmm
                emit_u8(cb, 0x0F); emit_u8(cb, 0x85);            // jne target  (match → jump)
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_BOOL: {                           // jump if R[a] == MAKE_BOOL(b)
                int xa = x86_cache_load(&cache, cb, a);
                if (xa < 0) JIT_FATAL("cache full: JUMP_MATCH_BOOL subj=%d (func=%d pc=%d)", a, func_idx, pc);
                x86_emit_movq_rax_xmm(cb, xa);                   // rax = subject bits
                uint64_t expect = X86_BOOL_BITS | (b ? 1ULL : 0ULL);
                x86_emit_movabs_r11(cb, expect);                 // r11 = expected bool bits
                emit_u8(cb, 0x4C); emit_u8(cb, 0x39); emit_u8(cb, 0xD8);  // cmp rax, r11
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je target
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_NONE: {                           // jump if R[a] == MAKE_NONE()
                int xa = x86_cache_load(&cache, cb, a);
                if (xa < 0) JIT_FATAL("cache full: JUMP_MATCH_NONE subj=%d (func=%d pc=%d)", a, func_idx, pc);
                x86_emit_movq_rax_xmm(cb, xa);                   // rax = subject bits
                x86_emit_movabs_r11(cb, X86_NONE_BITS);          // r11 = NONE bits
                emit_u8(cb, 0x4C); emit_u8(cb, 0x39); emit_u8(cb, 0xD8);  // cmp rax, r11
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je target
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_JUMP_TABLE: {                                // dense integer match dispatch
                int subj_reg = inst->operands[0];
                int min_val  = inst->operands[1];
                int tbl_idx  = inst->operands[2];
                Constant* tbl = &chunk->constants[tbl_idx];
                if (tbl->type != CONST_JUMP_TABLE)
                    JIT_FATAL("OP_JUMP_TABLE without CONST_JUMP_TABLE (func=%d pc=%d)", func_idx, pc);
                int range = tbl->jump_table.count;

                int xa = x86_cache_load(&cache, cb, subj_reg);
                if (xa < 0) JIT_FATAL("cache full: JUMP_TABLE subj=%d (func=%d pc=%d)", subj_reg, func_idx, pc);

                // spill dirty slots to memory before any branch
                x86_cache_flush(&cache, cb);

                // verify the value was a whole number by round-tripping
                x86_emit_cvttsd2si_eax(cb, xa);                  // eax = (int)subj
                emit_u8(cb, 0xF2);                               // cvtsi2sd xmm15, eax
                x86_rex_r(cb, XMM_SCRATCH);
                emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3));
                x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);        // ucomisd subj, truncated
                emit_u8(cb, 0x0F); emit_u8(cb, 0x85);            // jne miss
                size_t miss_patch = cb->len;
                emit_i32(cb, 0);

                if (min_val != 0) {                              // sub eax, min
                    if (min_val >= -128 && min_val <= 127) {
                        emit_u8(cb, 0x83); emit_u8(cb, 0xE8);
                        emit_u8(cb, (uint8_t)min_val);
                    } else {
                        emit_u8(cb, 0x2D); emit_u32(cb, (uint32_t)min_val);
                    }
                }
                if (range <= 127) {                              // cmp eax, range
                    emit_u8(cb, 0x83); emit_u8(cb, 0xF8);
                    emit_u8(cb, (uint8_t)range);
                } else {
                    emit_u8(cb, 0x3D); emit_u32(cb, (uint32_t)range);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x83);            // jae miss
                size_t bounds_patch = cb->len;
                emit_i32(cb, 0);

                emit_u8(cb, 0x4C); emit_u8(cb, 0x8D);            // lea r11, [rip + table_disp]
                emit_u8(cb, 0x1D);
                size_t table_disp_at = cb->len;
                emit_i32(cb, 0);
                emit_u8(cb, 0x41); emit_u8(cb, 0xFF);            // jmp qword [r11 + rax*8]
                emit_u8(cb, 0x24);
                emit_u8(cb, 0xC3);                               // sib: scale=8, index=rax, base=r11

                size_t table_start = cb->len;                    // inline absolute-address table
                for (int i = 0; i < range; i++) {
                    if (tab_fixup_count >= 256)
                        JIT_FATAL("jump-table slot overflow in function %d", func_idx);
                    tab_fixup_at[tab_fixup_count] = cb->len;
                    tab_fixup_pc[tab_fixup_count] = tbl->jump_table.addresses[i];
                    tab_fixup_count++;
                    emit_u64(cb, 0);                             // absolute target, patched later
                }
                size_t after_table = cb->len;

                int32_t tdisp = (int32_t)table_start - (int32_t)(table_disp_at + 4);
                memcpy(cb->buf + table_disp_at, &tdisp, 4);
                int32_t mrel = (int32_t)after_table - (int32_t)(miss_patch + 4);
                memcpy(cb->buf + miss_patch, &mrel, 4);
                int32_t brel = (int32_t)after_table - (int32_t)(bounds_patch + 4);
                memcpy(cb->buf + bounds_patch, &brel, 4);

                did_flush = true;                                // cache already flushed above
                break;
            }
            case OP_CALL_0: {                                    // call with no args
                x86_cache_flush(&cache, cb);                     // spill everything before call
                emit_call_or_self(cb, ctx, a, func_idx, mark);   // direct self-call or via table
                x86_cache_put(&cache, 0, d);                     // xmm0 = result
                break;
            }
            case OP_CALL_1: {                                    // call with one arg
                bool arg_live = slot_read_before_write(chunk, pc + 1, end, b);  // arg used after?
                int arg_xmm = cache.slot_reg[b];                 // where arg lives now
                for (int i = 0; i < XMM_CACHE_REGS; i++) {       // spill live dirty slots
                    int s = cache.reg_slot[i];
                    if (s < 0) continue;                         // empty slot, skip
                    if (s == b && !arg_live) continue;           // dead arg — memory stays stale
                    if (!cache.slot_dirty[s]) continue;          // clean slot, nothing to spill
                    int g = (s < JIT_MAX_SLOTS) ? slot_pocket_gpr[s] : -1;
                    if (g >= 0) x86_emit_movq_gpr_xmm(cb, x86_gpr_pocket_regs[g], i);
                    else        x86_emit_movsd_store(cb, i, x86_slot_disp(s));
                    cache.slot_dirty[s] = false;
                }
                if (arg_xmm >= 0 && arg_xmm != 0) {              // arg in cache, not xmm0
                    x86_emit_sse66_rr(cb, 0x28, 0, arg_xmm);     // movapd xmm0, arg_xmm
                } else if (arg_xmm < 0) {                        // arg not cached
                    int g = (b >= 0 && b < JIT_MAX_SLOTS) ? slot_pocket_gpr[b] : -1;
                    if (g >= 0) x86_emit_movq_xmm_gpr(cb, 0, x86_gpr_pocket_regs[g]);
                    else        x86_emit_movsd_load(cb, 0, x86_slot_disp(b));
                }                                                // else arg already in xmm0
                x86_cache_clear(&cache);                         // xmm regs clobbered by callee
                emit_call_or_self(cb, ctx, a, func_idx, mark);   // direct self-call or via table
                x86_cache_put(&cache, 0, d);                     // xmm0 = result
                for (int g = 0; g < n_pockets; g++) {            // restore pockets live after call
                    int s = slot_to_pocket[g];
                    if (s < 0 || s == d) continue;               // result already holds d
                    if (!slot_read_before_write(chunk, pc + 1, end, s)) continue;
                    int x = x86_cache_alloc_excl(&cache, cb, -1, -1);
                    if (x < 0) continue;                         // register cache full
                    x86_emit_movq_xmm_gpr(cb, x, x86_gpr_pocket_regs[g]);
                    cache.reg_slot[x] = s;
                    cache.slot_reg[s] = x;
                    cache.slot_dirty[s] = false;
                }
                break;
            }
            case OP_CALL_2: {                                    // call with two args
                bool arg0_live = slot_read_before_write(chunk, pc + 1, end, b);      // arg0 used after?
                bool arg1_live = slot_read_before_write(chunk, pc + 1, end, b + 1);  // arg1 used after?
                int arg0_xmm = cache.slot_reg[b];                // where arg0 lives
                int arg1_xmm = cache.slot_reg[b + 1];            // where arg1 lives
                for (int i = 0; i < XMM_CACHE_REGS; i++) {       // spill live dirty slots
                    int s = cache.reg_slot[i];
                    if (s < 0) continue;                         // empty, skip
                    if (s == b     && !arg0_live) continue;      // dead arg0 — memory stays stale
                    if (s == b + 1 && !arg1_live) continue;      // dead arg1 — memory stays stale
                    if (!cache.slot_dirty[s]) continue;          // clean slot, nothing to spill
                    int g = (s < JIT_MAX_SLOTS) ? slot_pocket_gpr[s] : -1;
                    if (g >= 0) x86_emit_movq_gpr_xmm(cb, x86_gpr_pocket_regs[g], i);
                    else        x86_emit_movsd_store(cb, i, x86_slot_disp(s));
                    cache.slot_dirty[s] = false;
                }
                if (arg0_xmm >= 0) {                             // arg0 in cache
                    x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, arg0_xmm);
                } else {                                         // arg0 not cached
                    int g = (b >= 0 && b < JIT_MAX_SLOTS) ? slot_pocket_gpr[b] : -1;
                    if (g >= 0) x86_emit_movq_xmm_gpr(cb, XMM_SCRATCH, x86_gpr_pocket_regs[g]);
                    else        x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(b));
                }
                if (arg1_xmm >= 0 && arg1_xmm != 1) {            // arg1 in cache, not xmm1
                    x86_emit_sse66_rr(cb, 0x28, 1, arg1_xmm);
                } else if (arg1_xmm < 0) {                       // arg1 not cached
                    int g = (b + 1 >= 0 && b + 1 < JIT_MAX_SLOTS) ? slot_pocket_gpr[b + 1] : -1;
                    if (g >= 0) x86_emit_movq_xmm_gpr(cb, 1, x86_gpr_pocket_regs[g]);
                    else        x86_emit_movsd_load(cb, 1, x86_slot_disp(b + 1));
                }
                x86_emit_sse66_rr(cb, 0x28, 0, XMM_SCRATCH);     // arg0 from scratch to xmm0
                x86_cache_clear(&cache);                         // xmm regs clobbered
                emit_call_or_self(cb, ctx, a, func_idx, mark);   // direct self-call or via table
                x86_cache_put(&cache, 0, d);                     // xmm0 = result
                for (int g = 0; g < n_pockets; g++) {            // restore pockets live after call
                    int s = slot_to_pocket[g];
                    if (s < 0 || s == d) continue;
                    if (!slot_read_before_write(chunk, pc + 1, end, s)) continue;
                    int x = x86_cache_alloc_excl(&cache, cb, -1, -1);
                    if (x < 0) continue;
                    x86_emit_movq_xmm_gpr(cb, x, x86_gpr_pocket_regs[g]);
                    cache.reg_slot[x] = s;
                    cache.slot_reg[s] = x;
                    cache.slot_dirty[s] = false;
                }
                break;
            }
            case OP_RETURN:                                      // return slot value
            case OP_RETURN_NUM:                                  // return number from register
            case OP_RETURN_BOOL: {
                int xa = x86_cache_load(&cache, cb, d);          // load return value
                if (xa < 0) xa = 0;                              // fall back to xmm0
                if (xa != 0) x86_emit_sse66_rr(cb, 0x28, 0, xa); // movapd xmm0, xa
                emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);                         // clear state on exit
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_RETURN_NUM_IMM: {                            // return number immediate
                int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == a) { imm_idx = i; break; }
                if (imm_idx >= 0) {
                    size_t at = x86_emit_movsd_load_rip(cb, 0);  // movsd xmm0, [rip+disp32]
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                } else {
                    x86_emit_load_double_imm(cb, 0, a);
                }
                emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);
                did_flush = true;
                break;
            }
            case OP_RETURN_NONE: {                               // return none, no value
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
                emit_u8(cb, 0x57); emit_u8(cb, 0xC0);            // xorpd xmm0, xmm0
                emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);                         // clear state on exit
                did_flush = true;                                // suppress merge flush
                break;
            }
            default:                                             // unreachable in pure fn
                JIT_FATAL("unreachable opcode %d in pure function %d at pc=%d", op, func_idx, pc);
        }

        // flush if the next instruction is a merge point — the incoming cache must be empty
        if (!did_flush && pc + 1 < end && needs_flush[pc + 1 - start]) {
            x86_cache_flush(&cache, cb);
        }
    }

    label_off[range_size] = (int32_t)cb->len;                    // pc==end target (fall through past body)

    emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);   // restore before leaving
    emit_return_zero(abi, cb, base_frame);                       // safety fallthrough

    size_t pool_off = cb->len;                                   // constant pool starts here
    for (int i = 0; i < func_n_imms; i++) {                      // append pool of double constants
        double dv = (double)func_imms[i];
        uint64_t bits; memcpy(&bits, &dv, 8);
        emit_u64(cb, bits);
    }
    for (int i = 0; i < rip_fixup_count; i++) {                  // patch rip-relative displacements
        size_t patch_at = rip_fixup_at[i];
        size_t imm_at   = pool_off + 8 * (size_t)rip_fixup_imm[i];
        int32_t rel = (int32_t)imm_at - (int32_t)(patch_at + 4);
        memcpy(cb->buf + patch_at, &rel, 4);
    }

    for (int i = 0; i < fixup_count; i++) {                      // patch every pending jump
        JumpFixup* fx = &fixups[i];
        int tidx = fx->target_pc - start;                        // index within range
        if (tidx < 0 || tidx > range_size || label_off[tidx] < 0) {
            JIT_FATAL("jump fixup out of range: func=%d target_pc=%d range=[%d,%d)",
                    func_idx, fx->target_pc, start, end);
        }
        int32_t rel = (int32_t)label_off[tidx] - (int32_t)(fx->patch_at + 4);  // rel32 distance
        memcpy(cb->buf + fx->patch_at, &rel, 4);                 // write rel32
    }

    for (int i = 0; i < tab_fixup_count; i++) {                  // patch every dispatch slot
        int tidx = tab_fixup_pc[i] - start;
        if (tidx < 0 || tidx > range_size || label_off[tidx] < 0) {
            JIT_FATAL("jump-table slot out of range: func=%d target_pc=%d range=[%d,%d)",
                    func_idx, tab_fixup_pc[i], start, end);
        }
        uint64_t addr = (uint64_t)(uintptr_t)(cb->buf + label_off[tidx]);
        memcpy(cb->buf + tab_fixup_at[i], &addr, 8);             // write 8-byte absolute
    }

    free(pred_count); free(needs_flush); free(use_snap); free(jump_snap);

    if (int_off != (size_t)-1) {                                 // wrap with an int dispatcher
        size_t wrapper_off = 0;
        emit_int_wrapper(cb, mark, int_off, int_max_n,
                          chunk->functions[func_idx].arity, &wrapper_off,
                          abi);
        *out_fn = (void*)(cb->buf + wrapper_off);                // entry is the wrapper
    } else {
        *out_fn = (void*)(cb->buf + mark);                       // publish general entry
    }
    return true;                                                 // emission successful
}
// source/jit/x86-64/x86_64_emit_jump.c
// Jump emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"

// emits the jcc displacement placeholder and records the fixup against st->d
static void emit_jcc_fixup(X86FuncEmitState* st, uint8_t jcc) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    emit_u8(cb, 0x0F); emit_u8(cb, jcc);                             // jcc rel32 opcode
    x86_func_jump_fixup(st, cb->len, st->d);                         // record placeholder offset
    emit_i32(cb, 0);                                                 // rel32 placeholder
}

// pre-branch cache flush / snap for a forward or backward jump target
static void prepare_jump_target(X86FuncEmitState* st, int target) {
    if (target >= st->start && target < st->end) {                   // in-range target
        if (st->needs_flush[target - st->start])                     // multi-pred / backward
            x86_cache_flush(st->cache, st->cb);
        else if (st->use_snap[target - st->start])                   // unique forward: snapshot
            x86_cache_snap(&st->jump_snap[target - st->start], st->cache);
    } else {
        x86_cache_flush(st->cache, st->cb);                          // out-of-range: safest
    }
}

// loads st->a into xa and st->b into xb for a compare jump
static void load_cmp_operands(X86FuncEmitState* st, int* out_xa, int* out_xb) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: JUMP_IF a=%d (func=%d pc=%d op=%d)",
                          st->a, st->func_idx, st->pc, st->op);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: JUMP_IF b=%d (func=%d pc=%d op=%d)",
                          st->b, st->func_idx, st->pc, st->op);
    *out_xa = xa; *out_xb = xb;
}

// base-path emit for reg-reg comparison jumps when !is_pure
static void func_emit_jump_cmp_base(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache

    uint64_t helper = 0;                                             // helper fn address
    switch (st->op) {
        case OP_JUMP_IF_EQ:  helper = (uint64_t)(uintptr_t)&jit_cmp_eq_base;  break;
        case OP_JUMP_IF_NEQ: helper = (uint64_t)(uintptr_t)&jit_cmp_ne_base;  break;
        case OP_JUMP_IF_LT:  helper = (uint64_t)(uintptr_t)&jit_cmp_lt_base;  break;
        case OP_JUMP_IF_GT:  helper = (uint64_t)(uintptr_t)&jit_cmp_gt_base;  break;
        case OP_JUMP_IF_LTE: helper = (uint64_t)(uintptr_t)&jit_cmp_lte_base; break;
        case OP_JUMP_IF_GTE: helper = (uint64_t)(uintptr_t)&jit_cmp_gte_base; break;
        default:
            JIT_FATAL("base jump-if: unexpected opcode %d (func=%d pc=%d)",
                      st->op, st->func_idx, st->pc);
    }

    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: base jump-if load a=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: base jump-if load b=%d (func=%d pc=%d)",
                          st->b, st->func_idx, st->pc);

#if defined(_WIN32) || defined(_WIN64)
    x86_emit_movq_gpr_xmm(cb, X86_RCX, xa);                          // win64 arg0 = rcx
    x86_emit_movq_gpr_xmm(cb, X86_RDX, xb);                          // win64 arg1 = rdx
#else
    x86_emit_movq_gpr_xmm(cb, X86_RDI, xa);                          // sysv arg0 = rdi
    x86_emit_movq_gpr_xmm(cb, X86_RSI, xb);                          // sysv arg1 = rsi
#endif

    x86_cache_flush(cache, cb);                                      // flush dirty slots before the call
    x86_emit_movabs_rax(cb, helper);                                 // rax = helper address
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax (result in rax)

    // capture the current (empty) cache state at the branch point
    prepare_jump_target(st, st->d);

    emit_u8(cb, 0xA8); emit_u8(cb, 0x01);                            // test al, 1 (bit 0 = bool value)
    emit_u8(cb, 0x0F); emit_u8(cb, 0x85);                            // jne rel32 (jump if true)
    x86_func_jump_fixup(st, cb->len, st->d);                         // record placeholder
    emit_i32(cb, 0);                                                 // rel32 placeholder

    st->did_flush = true;                                            // suppress merge flush
}

// function body dispatch for the jump category
void x86_func_emit_jump(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache

    // non-strict-pure: use base path for reg-reg non-num comparison jumps
    if (!st->is_pure &&
        (st->op == OP_JUMP_IF_EQ || st->op == OP_JUMP_IF_NEQ ||
         st->op == OP_JUMP_IF_LT || st->op == OP_JUMP_IF_GT ||
         st->op == OP_JUMP_IF_LTE || st->op == OP_JUMP_IF_GTE)) {
        func_emit_jump_cmp_base(st);
        return;
    }

    switch (st->op) {
        case OP_JUMP: {                                              // unconditional jump
            prepare_jump_target(st, st->d);                          // snapshot / flush cache
            emit_u8(cb, 0xE9);                                       // jmp rel32 opcode
            x86_func_jump_fixup(st, cb->len, st->d);                 // record placeholder offset
            emit_i32(cb, 0);                                         // rel32 placeholder
            st->did_flush = true;                                    // suppress merge flush
            break;
        }
        case OP_JUMP_IF_FALSE: {                                     // branch when cond is falsy
            int xa = x86_cache_load(cache, cb, st->a);               // load condition
            if (xa < 0) JIT_FATAL("cache full: JUMP_IF_FALSE cond=%d (func=%d pc=%d)",
                                  st->a, st->func_idx, st->pc);
            x86_emit_movq_rax_xmm(cb, xa);                           // rax = raw 64-bit slot
            emit_u8(cb, 0xA8); emit_u8(cb, 0x01);                    // test al, 1
            prepare_jump_target(st, st->d);                          // snapshot / flush cache
            emit_jcc_fixup(st, 0x84);                                // je rel32 (bit0 == 0 -> false)
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_EQ:                                          // branch if a == b
        case OP_JUMP_IF_EQ_NUM: {
            int xa, xb;
            load_cmp_operands(st, &xa, &xb);                         // load both operands
            x86_emit_ucomisd_rr(cb, xa, xb);                         // ucomisd a, b
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x84);                                // je rel32
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_NEQ:                                         // branch if a != b
        case OP_JUMP_IF_NEQ_NUM: {
            int xa, xb;
            load_cmp_operands(st, &xa, &xb);
            x86_emit_ucomisd_rr(cb, xa, xb);
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x85);                                // jne rel32
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_LT: {                                        // branch if a < b
            int xa, xb;
            load_cmp_operands(st, &xa, &xb);
            x86_emit_ucomisd_rr(cb, xa, xb);
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x82);                                // jb rel32
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_GT: {                                        // branch if a > b
            int xa, xb;
            load_cmp_operands(st, &xa, &xb);
            x86_emit_ucomisd_rr(cb, xa, xb);
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x87);                                // ja rel32
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_LTE: {                                       // branch if a <= b
            int xa, xb;
            load_cmp_operands(st, &xa, &xb);
            x86_emit_ucomisd_rr(cb, xa, xb);
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x86);                                // jbe rel32
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_GTE: {                                       // branch if a >= b
            int xa, xb;
            load_cmp_operands(st, &xa, &xb);
            x86_emit_ucomisd_rr(cb, xa, xb);
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x83);                                // jae rel32
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_EQ_IMM:                                      // branch if a == imm
        case OP_JUMP_IF_NEQ_IMM:
        case OP_JUMP_IF_LT_IMM:
        case OP_JUMP_IF_GT_IMM:
        case OP_JUMP_IF_LTE_IMM:
        case OP_JUMP_IF_GTE_IMM: {                                   // imm-jump variants share the pattern
            int xa = x86_cache_load(cache, cb, st->a);               // load left operand
            if (xa < 0) JIT_FATAL("cache full: JUMP_IF_IMM a=%d (func=%d pc=%d op=%d)",
                                  st->a, st->func_idx, st->pc, st->op);
            int imm_idx = -1;                                        // index into the rip pool
            for (int i = 0; i < st->func_n_imms; i++)
                if (st->func_imms[i] == st->b) { imm_idx = i; break; }
            if (imm_idx >= 0) {
                size_t at = x86_emit_ucomisd_rip(cb, xa);            // ucomisd xa, [rip+disp32]
                x86_func_rip_fixup(st, at, imm_idx);
            } else {
                x86_emit_load_double_imm(cb, XMM_SCRATCH, st->b);    // not pooled
                x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);
            }
            prepare_jump_target(st, st->d);
            uint8_t jcc;                                             // exit-condition jcc
            switch (st->op) {
                case OP_JUMP_IF_EQ_IMM:  jcc = 0x84; break;          // je
                case OP_JUMP_IF_NEQ_IMM: jcc = 0x85; break;          // jne
                case OP_JUMP_IF_LT_IMM:  jcc = 0x82; break;          // jb
                case OP_JUMP_IF_GT_IMM:  jcc = 0x87; break;          // ja
                case OP_JUMP_IF_LTE_IMM: jcc = 0x86; break;          // jbe
                default:                 jcc = 0x83; break;          // jae (gte)
            }
            emit_jcc_fixup(st, jcc);
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_NONE: {                                      // branch if a is none
            int xa = x86_cache_load(cache, cb, st->a);
            if (xa < 0) JIT_FATAL("cache full: JUMP_IF_NONE a=%d (pc=%d)", st->a, st->pc);
            x86_emit_movq_rax_xmm(cb, xa);                           // rax = raw 64-bit value
            x86_emit_movabs_r11(cb, X86_NONE_BITS);                  // r11 = NONE bit pattern
            x86_emit_cmp_rax_r11(cb);                                // cmp rax, r11
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x84);                                // je rel32
            st->did_flush = true;
            break;
        }
        case OP_JUMP_IF_NOT_NONE: {                                  // branch if a is not none
            int xa = x86_cache_load(cache, cb, st->a);
            if (xa < 0) JIT_FATAL("cache full: JUMP_IF_NOT_NONE a=%d (pc=%d)", st->a, st->pc);
            x86_emit_movq_rax_xmm(cb, xa);
            x86_emit_movabs_r11(cb, X86_NONE_BITS);
            x86_emit_cmp_rax_r11(cb);
            prepare_jump_target(st, st->d);
            emit_jcc_fixup(st, 0x85);                                // jne rel32
            st->did_flush = true;
            break;
        }
        default:
            JIT_FATAL("jump category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
}
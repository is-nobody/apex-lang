// source/jit/x86-64/x86_64_emit_cmp.c
// Comparison emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"

// returns the setcc opcode for a comparison opcode
static uint8_t setcc_for_cmp(Opcode op) {
    switch (op) {
        case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  return 0x94;           // sete
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: return 0x95;           // setne
        case OP_CMP_LT: return 0x92;                                 // setb
        case OP_CMP_GT: return 0x97;                                 // seta
        case OP_CMP_LTE:return 0x96;                                 // setbe
        default:        return 0x93;                                 // setae (gte)
    }
}

// base-path emit for comparisons when !is_pure: dispatches through a runtime helper
static void func_emit_cmp_base(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache

    uint64_t helper = 0;                                             // helper fn address
    switch (st->op) {
        case OP_CMP_EQ:  helper = (uint64_t)(uintptr_t)&jit_cmp_eq_base;  break;
        case OP_CMP_NEQ: helper = (uint64_t)(uintptr_t)&jit_cmp_ne_base;  break;
        case OP_CMP_LT:  helper = (uint64_t)(uintptr_t)&jit_cmp_lt_base;  break;
        case OP_CMP_GT:  helper = (uint64_t)(uintptr_t)&jit_cmp_gt_base;  break;
        case OP_CMP_LTE: helper = (uint64_t)(uintptr_t)&jit_cmp_lte_base; break;
        case OP_CMP_GTE: helper = (uint64_t)(uintptr_t)&jit_cmp_gte_base; break;
        default:
            JIT_FATAL("base cmp: unexpected opcode %d (func=%d pc=%d)",
                      st->op, st->func_idx, st->pc);
    }

    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: base cmp load a=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: base cmp load b=%d (func=%d pc=%d)",
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

    int xd = x86_cache_dest_reg(cache, cb, st->d, -1, -1);           // pick a destination xmm
    if (xd < 0) JIT_FATAL("cache full: base cmp dest=%d (func=%d pc=%d)",
                          st->d, st->func_idx, st->pc);
    x86_emit_movq_xmm_rax(cb, xd);                                   // xd = raw rax bits (MAKE_BOOL)
    x86_cache_put(cache, xd, st->d);                                 // xd now holds the result
}

// function body dispatch for the compare category
void x86_func_emit_cmp(X86FuncEmitState* st) {
    // non-strict-pure: use base path for all non-num comparisons
    if (!st->is_pure && st->op != OP_CMP_EQ_NUM && st->op != OP_CMP_NEQ_NUM) {
        func_emit_cmp_base(st);
        return;
    }
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: CMP a=%d (func=%d pc=%d op=%d)",
                          st->a, st->func_idx, st->pc, st->op);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: CMP b=%d (func=%d pc=%d op=%d)",
                          st->b, st->func_idx, st->pc, st->op);
    x86_emit_ucomisd_rr(cb, xa, xb);                                 // ucomisd a, b
    int xd = x86_cache_alloc_excl(cache, cb, xa, xb);                // pick dest register
    if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (func=%d pc=%d op=%d)",
                          st->d, st->func_idx, st->pc, st->op);
    x86_emit_cmp_box_result(cb, xd, setcc_for_cmp(st->op));          // boxed MAKE_BOOL into xd
    x86_cache_put(cache, xd, st->d);
}

// base-path emit for a loop body comparison
static void loop_emit_cmp_base(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    x86_cache_flush(st->cache, cb);                                  // spill dirty slots before the call

    uint64_t helper = 0;                                             // helper fn address
    switch (st->op) {
        case OP_CMP_EQ:  helper = (uint64_t)(uintptr_t)&jit_cmp_eq_base;  break;
        case OP_CMP_NEQ: helper = (uint64_t)(uintptr_t)&jit_cmp_ne_base;  break;
        case OP_CMP_LT:  helper = (uint64_t)(uintptr_t)&jit_cmp_lt_base;  break;
        case OP_CMP_GT:  helper = (uint64_t)(uintptr_t)&jit_cmp_gt_base;  break;
        case OP_CMP_LTE: helper = (uint64_t)(uintptr_t)&jit_cmp_lte_base; break;
        case OP_CMP_GTE: helper = (uint64_t)(uintptr_t)&jit_cmp_gte_base; break;
        case OP_CMP_EQ_NUM:  helper = (uint64_t)(uintptr_t)&jit_cmp_eq_base;  break;
        case OP_CMP_NEQ_NUM: helper = (uint64_t)(uintptr_t)&jit_cmp_ne_base;  break;
        default:
            JIT_FATAL("loop base cmp: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }

    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save frame_reg
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_load_r64_rbp(cb, X86_RCX, x86_slot_disp(st->a));
    x86_emit_load_r64_rbp(cb, X86_RDX, x86_slot_disp(st->b));
#else
    x86_emit_load_r64_rbp(cb, X86_RDI, x86_slot_disp(st->a));
    x86_emit_load_r64_rbp(cb, X86_RSI, x86_slot_disp(st->b));
#endif
    x86_emit_movabs_rax(cb, helper);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));
    x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(st->d));       // frame[d] = boxed bool
}

// loop body dispatch for the compare category
void x86_loop_emit_cmp(X86LoopBodyState* st) {
    if (!st->is_pure) {                                              // base path (checked comparison)
        loop_emit_cmp_base(st);
        return;
    }
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: CMP a=%d (pc=%d op=%d)", st->a, st->pc, st->op);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: CMP b=%d (pc=%d op=%d)", st->b, st->pc, st->op);
    x86_emit_ucomisd_rr(cb, xa, xb);                                 // ucomisd a, b
    int xd = x86_cache_lookup(cache, st->d);                         // reuse d's register if cached
    if (xd == xa || xd == xb) xd = -1;                               // dest aliases a source
    if (xd < 0) xd = x86_cache_dest_reg(cache, cb, st->d, xa, xb);   // prefer d's home
    if (xd < 0) JIT_FATAL("cache full: CMP dst=%d (pc=%d op=%d)", st->d, st->pc, st->op);
    x86_emit_cmp_box_result(cb, xd, setcc_for_cmp(st->op));          // boxed MAKE_BOOL into xd
    x86_cache_put(cache, xd, st->d);
}
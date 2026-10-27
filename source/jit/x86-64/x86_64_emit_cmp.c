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

// function body dispatch for the compare category
void x86_func_emit_cmp(X86FuncEmitState* st) {
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

// loop body dispatch for the compare category
void x86_loop_emit_cmp(X86LoopBodyState* st) {
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
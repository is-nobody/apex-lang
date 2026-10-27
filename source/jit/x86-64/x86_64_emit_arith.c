// source/jit/x86-64/x86_64_emit_arith.c
// Arithmetic emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"

// emits OP_ADD/SUB/MUL/DIV for a function body instruction
static void func_emit_bin_arith(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: arith a=%d (func=%d pc=%d op=%d)",
                          st->a, st->func_idx, st->pc, st->op);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: arith b=%d (func=%d pc=%d op=%d)",
                          st->b, st->func_idx, st->pc, st->op);
    uint8_t arith_op;                                                // sse opcode
    switch (st->op) {
        case OP_ADD: arith_op = 0x58; break;                         // addsd
        case OP_SUB: arith_op = 0x5C; break;                         // subsd
        case OP_MUL: arith_op = 0x59; break;                         // mulsd
        default:     arith_op = 0x5E; break;                         // divsd
    }
    bool commutative = (st->op == OP_ADD || st->op == OP_MUL);       // swap-safe op?
    if (st->prefer_xmm0 && commutative && xb == 0 && xa != 0) {      // xmm0 already holds b
        if (st->d != st->b && cache->slot_dirty[st->b] &&
            slot_read_before_write(st->chunk, st->pc + 1, st->end, st->b)) {
            x86_emit_movsd_store(cb, 0, x86_slot_disp(st->b));       // preserve b
            cache->slot_dirty[st->b] = false;
        }
        x86_emit_sse_arith_rr(cb, arith_op, 0, xa);                  // xmm0 = xmm0 op xa
        x86_cache_put(cache, 0, st->d);                              // relabel xmm0 as dest
    } else {
        if (st->d != st->a && cache->slot_dirty[st->a] &&
            slot_read_before_write(st->chunk, st->pc + 1, st->end, st->a)) {
            x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));      // preserve a
            cache->slot_dirty[st->a] = false;
        }
        x86_emit_sse_arith_rr(cb, arith_op, xa, xb);                 // xa op= xb
        x86_cache_put(cache, xa, st->d);
    }
}

// emits OP_ADD_IMM/SUB_IMM/MUL_IMM/DIV_IMM for a function body instruction
static void func_emit_imm_arith(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: arith_imm a=%d (func=%d pc=%d op=%d)",
                          st->a, st->func_idx, st->pc, st->op);
    uint8_t arith_op;                                                // sse opcode
    switch (st->op) {
        case OP_ADD_IMM: arith_op = 0x58; break;                     // addsd
        case OP_SUB_IMM: arith_op = 0x5C; break;                     // subsd
        case OP_MUL_IMM: arith_op = 0x59; break;                     // mulsd
        default:         arith_op = 0x5E; break;                     // divsd
    }
    int imm_idx = -1;                                                // index into the rip pool
    for (int i = 0; i < st->func_n_imms; i++)
        if (st->func_imms[i] == st->b) { imm_idx = i; break; }
    bool preserve = (st->d != st->a) &&
        slot_read_before_write(st->chunk, st->pc + 1, st->end, st->a);
    if (imm_idx >= 0) {                                              // pooled: rip-relative op
        if (preserve) {
            int xd = x86_cache_alloc_excl(cache, cb, xa, -1);        // fresh dest register
            if (xd >= 0) {
                x86_emit_sse66_rr(cb, 0x28, xd, xa);                 // movapd xd, xa
                size_t at = x86_emit_sse_arith_rip(cb, arith_op, xd);
                x86_func_rip_fixup(st, at, imm_idx);
                x86_cache_put(cache, xd, st->d);
                return;
            }
        }
        size_t at = x86_emit_sse_arith_rip(cb, arith_op, xa);
        x86_func_rip_fixup(st, at, imm_idx);
        x86_cache_put(cache, xa, st->d);
        return;
    }
    x86_emit_load_double_imm(cb, XMM_SCRATCH, st->b);                // not pooled: build from imm
    if (preserve) {
        int xd = x86_cache_alloc_excl(cache, cb, xa, XMM_SCRATCH);   // fresh dest, keep scratch
        if (xd >= 0) {
            x86_emit_sse66_rr(cb, 0x28, xd, xa);
            x86_emit_sse_arith_rr(cb, arith_op, xd, XMM_SCRATCH);
            x86_cache_put(cache, xd, st->d);
            return;
        }
    }
    x86_emit_sse_arith_rr(cb, arith_op, xa, XMM_SCRATCH);
    x86_cache_put(cache, xa, st->d);
}

// emits OP_MOD_IMM: a - trunc(a/imm)*imm
static void func_emit_mod_imm(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load a
    if (xa < 0) JIT_FATAL("cache full: MOD_IMM a=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    x86_emit_load_double_imm(cb, XMM_SCRATCH, st->b);                // xmm15 = (double)b
    int xt = x86_cache_alloc_excl(cache, cb, xa, XMM_SCRATCH);       // temp for a/imm
    if (xt < 0) JIT_FATAL("cache full: MOD_IMM temp (func=%d pc=%d)",
                          st->func_idx, st->pc);
    x86_emit_sse66_rr(cb, 0x28, xt, xa);                             // xt = a
    x86_emit_sse_arith_rr(cb, 0x5E, xt, XMM_SCRATCH);                // xt = a / imm
    emit_u8(cb, 0x66);                                               // roundsd legacy prefix
    x86_rex_rb(cb, xt, xt);                                          // rex.r/rex.b
    emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
    emit_u8(cb, 0x0B);                                               // roundsd opcode
    emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));                  // round xt, xt
    emit_u8(cb, 0x03);                                               // round mode: truncate
    x86_emit_sse_arith_rr(cb, 0x59, xt, XMM_SCRATCH);                // xt = trunc(a/imm) * imm
    if (st->d != st->a && cache->slot_dirty[st->a] &&
        slot_read_before_write(st->chunk, st->pc + 1, st->end, st->a)) {
        x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));          // preserve a's value
        cache->slot_dirty[st->a] = false;
    }
    x86_emit_sse_arith_rr(cb, 0x5C, xa, xt);                         // a = a - xt
    x86_cache_put(cache, xa, st->d);
}

// emits OP_MOD: x - trunc(x/y)*y
static void func_emit_mod(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load a
    if (xa < 0) JIT_FATAL("cache full: MOD a=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load b, avoid xa
    if (xb < 0) JIT_FATAL("cache full: MOD b=%d (func=%d pc=%d)",
                          st->b, st->func_idx, st->pc);
    x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);                    // movapd scratch, a
    x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);                // divsd scratch, b
    emit_u8(cb, 0x66);
    x86_rex_rb(cb, XMM_SCRATCH, XMM_SCRATCH);
    emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
    emit_u8(cb, 0x0B);
    emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3) | (XMM_SCRATCH & 7));
    emit_u8(cb, 0x03);                                               // truncate
    x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);                // mulsd scratch, b
    if (st->d != st->a && cache->slot_dirty[st->a] &&
        slot_read_before_write(st->chunk, st->pc + 1, st->end, st->a)) {
        x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));          // preserve a's value
        cache->slot_dirty[st->a] = false;
    }
    x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);                // subsd a, scratch
    x86_cache_put(cache, xa, st->d);
}

// emits OP_NEG
static void func_emit_neg(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load operand
    if (xa < 0) JIT_FATAL("cache full: NEG a=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);           // xorpd scratch, scratch
    x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);                // subsd scratch, a
    if (st->d != st->a && cache->slot_dirty[st->a] &&
        slot_read_before_write(st->chunk, st->pc + 1, st->end, st->a)) {
        x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));          // preserve a's value
        cache->slot_dirty[st->a] = false;
    }
    x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);                    // movapd a, scratch
    x86_cache_put(cache, xa, st->d);
}

// emits OP_INC / OP_DEC
static void func_emit_incdec(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xd = x86_cache_load(cache, cb, st->d);                       // load dest
    if (xd < 0) JIT_FATAL("cache full: INC/DEC dst=%d (func=%d pc=%d)",
                          st->d, st->func_idx, st->pc);
    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);                  // rax = 1.0 bits
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                          // scratch = 1.0
    uint8_t arith_op = (st->op == OP_INC) ? 0x58 : 0x5C;             // addsd / subsd
    x86_emit_sse_arith_rr(cb, arith_op, xd, XMM_SCRATCH);            // xd op= 1.0
    x86_cache_put(cache, xd, st->d);                                 // mark dest dirty
}

// function body dispatch for the arithmetic category
void x86_func_emit_arith(X86FuncEmitState* st) {
    switch (st->op) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
            func_emit_bin_arith(st); break;
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM: case OP_DIV_IMM:
            func_emit_imm_arith(st); break;
        case OP_MOD_IMM: func_emit_mod_imm(st); break;
        case OP_MOD:     func_emit_mod(st); break;
        case OP_NEG:     func_emit_neg(st); break;
        case OP_INC: case OP_DEC: func_emit_incdec(st); break;
        default:
            JIT_FATAL("arith category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
}

// emits OP_ADD/SUB/MUL/DIV for a loop body instruction
static void loop_emit_bin_arith(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: arith a=%d (pc=%d op=%d)", st->a, st->pc, st->op);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load right, avoid xa
    if (xb < 0) JIT_FATAL("cache full: arith b=%d (pc=%d op=%d)", st->b, st->pc, st->op);
    uint8_t op;                                                      // sse opcode
    switch (st->op) {
        case OP_ADD: op = 0x58; break;                               // addsd
        case OP_SUB: op = 0x5C; break;                               // subsd
        case OP_MUL: op = 0x59; break;                               // mulsd
        default:     op = 0x5E; break;                               // divsd
    }
    int xd = xa;                                                     // result in xa by default
    if (st->d != st->a) {
        xd = x86_cache_dest_reg(cache, cb, st->d, xa, xb);           // separate dest
        if (xd < 0) {                                                // no room: spill a
            if (cache->slot_dirty[st->a]) {
                x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));
                cache->slot_dirty[st->a] = false;
            }
            xd = xa;
        } else if (xd != xa) {
            x86_emit_sse66_rr(cb, 0x28, xd, xa);                     // movapd xd, xa
        }
    }
    x86_emit_sse_arith_rr(cb, op, xd, xb);                           // xd op= xb
    x86_cache_put(cache, xd, st->d);
    st->touched_nan_check = true;                                    // arithmetic can produce NaN
}

// emits OP_ADD_IMM/SUB_IMM/MUL_IMM/DIV_IMM for a loop body instruction
static void loop_emit_imm_arith(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load left operand
    if (xa < 0) JIT_FATAL("cache full: arith_imm a=%d (pc=%d op=%d)", st->a, st->pc, st->op);
    uint8_t op;                                                      // sse opcode
    switch (st->op) {
        case OP_ADD_IMM: op = 0x58; break;                           // addsd
        case OP_SUB_IMM: op = 0x5C; break;                           // subsd
        case OP_MUL_IMM: op = 0x59; break;                           // mulsd
        default:         op = 0x5E; break;                           // divsd
    }
    int slot = -1;                                                   // pooled immediate slot
    if (st->info->imm_opt_ok) {
        for (int i = 0; i < st->info->n_imms; i++)
            if (st->info->imms[i].value == st->b) { slot = st->info->imms[i].slot; break; }
    }
    int xd = xa;                                                     // result in xa by default
    if (st->d != st->a) {
        xd = x86_cache_dest_reg(cache, cb, st->d, xa, XMM_SCRATCH);  // separate dest
        if (xd < 0) {                                                // no room: spill a
            if (cache->slot_dirty[st->a]) {
                x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));
                cache->slot_dirty[st->a] = false;
            }
            xd = xa;
        } else if (xd != xa) {
            x86_emit_sse66_rr(cb, 0x28, xd, xa);                     // movapd xd, xa
        }
    }
    if (slot >= 0) {                                                 // pooled: memory operand
        x86_emit_sse_arith_mem(cb, op, xd, x86_slot_disp(slot));
    } else {
        x86_emit_load_double_imm(cb, XMM_SCRATCH, st->b);            // not pooled
        x86_emit_sse_arith_rr(cb, op, xd, XMM_SCRATCH);
    }
    x86_cache_put(cache, xd, st->d);
    st->touched_nan_check = true;                                    // arithmetic can produce NaN
}

// emits OP_MOD_IMM for a loop body instruction
static void loop_emit_mod_imm(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load a
    if (xa < 0) JIT_FATAL("cache full: MOD_IMM a=%d (pc=%d)", st->a, st->pc);
    int slot = -1;                                                   // pooled immediate slot
    if (st->info->imm_opt_ok) {
        for (int i = 0; i < st->info->n_imms; i++)
            if (st->info->imms[i].value == st->b) { slot = st->info->imms[i].slot; break; }
    }
    int xt;                                                          // temp for a/imm
    if (slot >= 0) {                                                 // pooled: memory operand
        xt = x86_cache_alloc_excl(cache, cb, xa, -1);
        if (xt < 0) JIT_FATAL("cache full: MOD_IMM temp (pc=%d a=%d)", st->pc, st->a);
        x86_emit_sse66_rr(cb, 0x28, xt, xa);                         // xt = a
        x86_emit_sse_arith_mem(cb, 0x5E, xt, x86_slot_disp(slot));   // xt = a / imm
        emit_u8(cb, 0x66);                                           // roundsd prefix
        x86_rex_rb(cb, xt, xt);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x3A); emit_u8(cb, 0x0B);
        emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));
        emit_u8(cb, 0x03);                                           // truncate
        x86_emit_sse_arith_mem(cb, 0x59, xt, x86_slot_disp(slot));   // xt *= imm
    } else {
        x86_emit_load_double_imm(cb, XMM_SCRATCH, st->b);            // not pooled
        xt = x86_cache_alloc_excl(cache, cb, xa, XMM_SCRATCH);
        if (xt < 0) JIT_FATAL("cache full: MOD_IMM temp scratch (pc=%d a=%d)", st->pc, st->a);
        x86_emit_sse66_rr(cb, 0x28, xt, xa);
        x86_emit_sse_arith_rr(cb, 0x5E, xt, XMM_SCRATCH);
        emit_u8(cb, 0x66);
        x86_rex_rb(cb, xt, xt);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x3A); emit_u8(cb, 0x0B);
        emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));
        emit_u8(cb, 0x03);
        x86_emit_sse_arith_rr(cb, 0x59, xt, XMM_SCRATCH);
    }
    if (st->d == st->a) {                                            // dest == source
        x86_emit_sse_arith_rr(cb, 0x5C, xa, xt);                     // a = a - xt
        x86_cache_put(cache, xa, st->d);
    } else {                                                         // dest != source
        int xd = x86_cache_dest_reg(cache, cb, st->d, xa, xt);
        if (xd < 0) {                                                // no room: spill a
            if (cache->slot_dirty[st->a]) {
                x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));
                cache->slot_dirty[st->a] = false;
            }
            xd = xa;
        } else if (xd != xa) {
            x86_emit_sse66_rr(cb, 0x28, xd, xa);                     // movapd xd, xa
        }
        x86_emit_sse_arith_rr(cb, 0x5C, xd, xt);                     // xd = xd - xt
        x86_cache_put(cache, xd, st->d);
    }
    st->touched_nan_check = true;
}

// emits OP_MOD for a loop body instruction
static void loop_emit_mod(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load a
    if (xa < 0) JIT_FATAL("cache full: MOD a=%d (pc=%d)", st->a, st->pc);
    int xb = x86_cache_load_excl(cache, cb, st->b, xa, -1);          // load b, avoid xa
    if (xb < 0) JIT_FATAL("cache full: MOD b=%d (pc=%d)", st->b, st->pc);
    x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);                    // scratch = a
    x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);                // scratch = a / b
    emit_u8(cb, 0x66);
    x86_rex_rb(cb, XMM_SCRATCH, XMM_SCRATCH);
    emit_u8(cb, 0x0F); emit_u8(cb, 0x3A); emit_u8(cb, 0x0B);
    emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3) | (XMM_SCRATCH & 7));
    emit_u8(cb, 0x03);
    x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);                // scratch = trunc(a/b) * b
    if (st->d == st->a) {                                            // dest == source
        x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);            // a = a - scratch
        x86_cache_put(cache, xa, st->d);
    } else {                                                         // dest != source
        int xd = x86_cache_dest_reg(cache, cb, st->d, xa, -1);
        if (xd < 0) {                                                // no room: spill a
            if (cache->slot_dirty[st->a]) {
                x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));
                cache->slot_dirty[st->a] = false;
            }
            xd = xa;
        } else if (xd != xa) {
            x86_emit_sse66_rr(cb, 0x28, xd, xa);
        }
        x86_emit_sse_arith_rr(cb, 0x5C, xd, XMM_SCRATCH);
        x86_cache_put(cache, xd, st->d);
    }
    st->touched_nan_check = true;
}

// emits OP_NEG for a loop body instruction
static void loop_emit_neg(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load operand
    if (xa < 0) JIT_FATAL("cache full: NEG a=%d (pc=%d)", st->a, st->pc);
    x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);           // scratch = 0
    x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);                // scratch = 0 - a
    if (st->d == st->a) {                                            // dest == source
        x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);                // a = -a
        x86_cache_put(cache, xa, st->d);
    } else {                                                         // dest != source
        int xd = x86_cache_dest_reg(cache, cb, st->d, xa, -1);
        if (xd < 0) {                                                // no room: spill a
            if (cache->slot_dirty[st->a]) {
                x86_emit_movsd_store(cb, xa, x86_slot_disp(st->a));
                cache->slot_dirty[st->a] = false;
            }
            xd = xa;
        }
        x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);                // xd = -a
        x86_cache_put(cache, xd, st->d);
    }
    st->touched_nan_check = true;
}

// emits OP_INC / OP_DEC for a loop body instruction
static void loop_emit_incdec(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xd = x86_cache_load(cache, cb, st->d);                       // load dest
    if (xd < 0) JIT_FATAL("cache full: INC/DEC dst=%d (pc=%d)", st->d, st->pc);
    uint8_t op = (st->op == OP_INC) ? 0x58 : 0x5C;                   // addsd / subsd
    x86_emit_sse_arith_mem(cb, op, xd, x86_slot_disp(st->const_slot));  // xd op= 1.0 (from slot)
    x86_cache_put(cache, xd, st->d);
    st->touched_nan_check = true;
}

// loop body dispatch for the arithmetic category
void x86_loop_emit_arith(X86LoopBodyState* st) {
    switch (st->op) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
            loop_emit_bin_arith(st); break;
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM: case OP_DIV_IMM:
            loop_emit_imm_arith(st); break;
        case OP_MOD_IMM: loop_emit_mod_imm(st); break;
        case OP_MOD:     loop_emit_mod(st); break;
        case OP_NEG:     loop_emit_neg(st); break;
        case OP_INC: case OP_DEC: loop_emit_incdec(st); break;
        default:
            JIT_FATAL("loop arith category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
}
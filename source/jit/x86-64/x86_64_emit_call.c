// source/jit/x86-64/x86_64_emit_call.c
// Call and return emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"

// spills every live dirty slot before an impending call
static void spill_dirty_for_call(X86FuncEmitState* st, int live_a, int live_b) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = cache->reg_slot[i];                                  // slot held in xmm<i>
        if (s < 0) continue;                                         // empty, skip
        if (s == live_a || s == live_b) continue;                    // dead arg, memory stays stale
        if (!cache->slot_dirty[s]) continue;                         // clean slot, nothing to spill
        int g = (s < JIT_MAX_SLOTS) ? st->slot_pocket_gpr[s] : -1;   // pocket gpr index, or -1
        if (g >= 0) x86_emit_movq_gpr_xmm(cb, x86_gpr_pocket_regs[g], i);  // pocket store
        else        x86_emit_movsd_store(cb, i, x86_slot_disp(s));   // memory store
        cache->slot_dirty[s] = false;
    }
}

// emits OP_CALL_1 for a function body instruction
static void func_emit_call_1(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int arg_live = slot_read_before_write(st->chunk, st->pc + 1, st->end, st->b);  // arg used after?
    int arg_xmm = cache->slot_reg[st->b];                            // where arg lives now
    spill_dirty_for_call(st, st->b, arg_live ? -2 : -1);             // spill everything except live arg

    if (arg_xmm >= 0 && arg_xmm != 0) {                              // arg cached, not xmm0
        x86_emit_sse66_rr(cb, 0x28, 0, arg_xmm);                     // movapd xmm0, arg_xmm
    } else if (arg_xmm < 0) {                                        // arg not cached
        int g = (st->b >= 0 && st->b < JIT_MAX_SLOTS) ? st->slot_pocket_gpr[st->b] : -1;
        if (g >= 0) x86_emit_movq_xmm_gpr(cb, 0, x86_gpr_pocket_regs[g]);
        else        x86_emit_movsd_load(cb, 0, x86_slot_disp(st->b));
    }
    x86_cache_clear(cache);                                          // xmm regs clobbered by callee
    emit_call_or_self(cb, st->ctx, st->a, st->func_idx, st->mark);   // direct self-call or via table
    x86_cache_put(cache, 0, st->d);                                  // xmm0 = result

    for (int g = 0; g < st->n_pockets; g++) {                        // restore pockets live after call
        int s = st->slot_to_pocket[g];                               // slot held in this pocket
        if (s < 0 || s == st->d) continue;                           // empty or overwritten by result
        if (!slot_read_before_write(st->chunk, st->pc + 1, st->end, s)) continue;
        int x = x86_cache_alloc_excl(cache, cb, -1, -1);             // pick a cache register
        if (x < 0) continue;                                         // register cache full
        x86_emit_movq_xmm_gpr(cb, x, x86_gpr_pocket_regs[g]);        // xmm<x> = pocket gpr
        cache->reg_slot[x] = s;                                      // restore cache mapping
        cache->slot_reg[s] = x;
        cache->slot_dirty[s] = false;
    }
}

// emits OP_CALL_2 for a function body instruction
static void func_emit_call_2(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int arg0_live = slot_read_before_write(st->chunk, st->pc + 1, st->end, st->b);      // arg0 used after?
    int arg1_live = slot_read_before_write(st->chunk, st->pc + 1, st->end, st->b + 1);  // arg1 used after?
    int arg0_xmm = cache->slot_reg[st->b];                           // where arg0 lives
    int arg1_xmm = cache->slot_reg[st->b + 1];                       // where arg1 lives
    spill_dirty_for_call(st, st->b, st->b + 1);                      // spill live dirty slots

    if (arg0_xmm >= 0) {                                             // arg0 in cache
        x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, arg0_xmm);          // movapd scratch, arg0
    } else {                                                         // arg0 not cached
        int g = (st->b >= 0 && st->b < JIT_MAX_SLOTS) ? st->slot_pocket_gpr[st->b] : -1;
        if (g >= 0) x86_emit_movq_xmm_gpr(cb, XMM_SCRATCH, x86_gpr_pocket_regs[g]);
        else        x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(st->b));
    }
    if (arg1_xmm >= 0 && arg1_xmm != 1) {                            // arg1 in cache, not xmm1
        x86_emit_sse66_rr(cb, 0x28, 1, arg1_xmm);                    // movapd xmm1, arg1
    } else if (arg1_xmm < 0) {                                       // arg1 not cached
        int g = (st->b + 1 >= 0 && st->b + 1 < JIT_MAX_SLOTS) ? st->slot_pocket_gpr[st->b + 1] : -1;
        if (g >= 0) x86_emit_movq_xmm_gpr(cb, 1, x86_gpr_pocket_regs[g]);
        else        x86_emit_movsd_load(cb, 1, x86_slot_disp(st->b + 1));
    }
    x86_emit_sse66_rr(cb, 0x28, 0, XMM_SCRATCH);                     // arg0 from scratch to xmm0
    x86_cache_clear(cache);                                          // xmm regs clobbered
    emit_call_or_self(cb, st->ctx, st->a, st->func_idx, st->mark);   // direct self-call or via table
    x86_cache_put(cache, 0, st->d);                                  // xmm0 = result

    (void)arg0_live;                                                 // checked in spill helper
    (void)arg1_live;                                                 // checked in spill helper

    for (int g = 0; g < st->n_pockets; g++) {                        // restore pockets live after call
        int s = st->slot_to_pocket[g];
        if (s < 0 || s == st->d) continue;
        if (!slot_read_before_write(st->chunk, st->pc + 1, st->end, s)) continue;
        int x = x86_cache_alloc_excl(cache, cb, -1, -1);
        if (x < 0) continue;
        x86_emit_movq_xmm_gpr(cb, x, x86_gpr_pocket_regs[g]);
        cache->reg_slot[x] = s;
        cache->slot_reg[s] = x;
        cache->slot_dirty[s] = false;
    }
}

// emits OP_CALL_0 for a function body instruction
static void func_emit_call_0(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    x86_cache_flush(st->cache, cb);                                  // spill everything before call
    emit_call_or_self(cb, st->ctx, st->a, st->func_idx, st->mark);   // direct self-call or via table
    x86_cache_put(st->cache, 0, st->d);                              // xmm0 = result
}

// emits OP_RETURN / OP_RETURN_NUM / OP_RETURN_BOOL / OP_RETURN_NUM_IMM / OP_RETURN_NONE
static void func_emit_return(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    switch (st->op) {
        case OP_RETURN:                                              // return slot value
        case OP_RETURN_NUM:                                          // return number from register
        case OP_RETURN_BOOL: {                                       // return bool from register
            int xa = x86_cache_load(cache, cb, st->d);               // load return value
            if (xa < 0) xa = 0;                                      // fall back to xmm0
            if (xa != 0) x86_emit_sse66_rr(cb, 0x28, 0, xa);         // movapd xmm0, xa
            break;
        }
        case OP_RETURN_NUM_IMM: {                                    // return number immediate
            int imm_idx = -1;                                        // index into the rip pool
            for (int i = 0; i < st->func_n_imms; i++)
                if (st->func_imms[i] == st->a) { imm_idx = i; break; }
            if (imm_idx >= 0) {
                size_t at = x86_emit_movsd_load_rip(cb, 0);          // movsd xmm0, [rip+disp32]
                x86_func_rip_fixup(st, at, imm_idx);
            } else {
                x86_emit_load_double_imm(cb, 0, st->a);              // not pooled
            }
            break;
        }
        case OP_RETURN_NONE: {                                       // return none, no value
            emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
            emit_u8(cb, 0x57); emit_u8(cb, 0xC0);                    // xorpd xmm0, xmm0
            break;
        }
        default: break;
    }
    x86_cache_clear(cache);                                          // clear state on exit
    st->did_flush = true;                                            // suppress merge flush
}

// emits OP_PUSH_ARG for a non-strict-pure function body instruction
static void func_emit_push_arg(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    x86_cache_flush(st->cache, cb);                                  // spill dirty slots before the call

    // arg0 = vm* (loaded via ctx->vm so a NULL bake-time pointer is avoided)
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&st->ctx->vm);      // rax = &ctx->vm
    x86_emit_load_r64_base(cb, X86_RAX, X86_RAX, 0);                 // rax = *rax = ctx->vm
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RCX, X86_RAX);                      // win64 arg0 = rcx
    x86_emit_load_r64_rbp(cb, X86_RDX, x86_slot_disp(st->d));        // win64 arg1 = frame[d]
#else
    x86_emit_mov_r64_r64(cb, X86_RDI, X86_RAX);                      // sysv arg0 = rdi
    x86_emit_load_r64_rbp(cb, X86_RSI, x86_slot_disp(st->d));        // sysv arg1 = frame[d]
#endif

    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_push_arg_base);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
}

// emits OP_CALL_BUILTIN / OP_ASYNC_CALL_BUILTIN for a non-strict-pure function body instruction
static void func_emit_builtin_call(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    bool async = (st->op == OP_ASYNC_CALL_BUILTIN);                  // async variant toggles builtin_async
    x86_cache_flush(st->cache, cb);                                  // spill dirty slots before the call

    // arg0 = vm* (loaded via ctx->vm)
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&st->ctx->vm);      // rax = &ctx->vm
    x86_emit_load_r64_base(cb, X86_RAX, X86_RAX, 0);                 // rax = ctx->vm
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RCX, X86_RAX);                      // win64 arg0 = rcx
    emit_u8(cb, 0xBA); emit_u32(cb, (uint32_t)st->a);                // win64 arg1 (edx) = name_idx
    emit_u8(cb, 0x41); emit_u8(cb, 0xB8);                            // win64 arg2 (r8d) = arg_count
    emit_u32(cb, (uint32_t)st->b);
    x86_emit_lea_r64_base(cb, X86_R9, X86_RBP, x86_slot_disp(st->d));// win64 arg3 = &frame[d]
#else
    x86_emit_mov_r64_r64(cb, X86_RDI, X86_RAX);                      // sysv arg0 = rdi
    emit_u8(cb, 0xBE); emit_u32(cb, (uint32_t)st->a);                // sysv arg1 (esi) = name_idx
    emit_u8(cb, 0xBA); emit_u32(cb, (uint32_t)st->b);                // sysv arg2 (edx) = arg_count
    x86_emit_lea_r64_base(cb, X86_RCX, X86_RBP, x86_slot_disp(st->d));// sysv arg3 = &frame[d]
#endif

    uint64_t helper = async
        ? (uint64_t)(uintptr_t)&jit_async_call_builtin_store
        : (uint64_t)(uintptr_t)&jit_call_builtin_store;
    x86_emit_movabs_rax(cb, helper);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax

    // result now lives in frame[d]
    int xd = x86_cache_alloc_excl(st->cache, cb, -1, -1);
    if (xd >= 0) {
        x86_emit_movsd_load(cb, xd, x86_slot_disp(st->d));           // xmm<xd> = frame[d]
        x86_cache_put(st->cache, xd, st->d);                         // cache as dest
    }
}

// function body dispatch for the call/return category
void x86_func_emit_call(X86FuncEmitState* st) {
    switch (st->op) {
        case OP_CALL_0: func_emit_call_0(st); break;                 // zero-arg call
        case OP_CALL_1: func_emit_call_1(st); break;                 // one-arg call
        case OP_CALL_2: func_emit_call_2(st); break;                 // two-arg call
        case OP_PUSH_ARG: func_emit_push_arg(st); break;             // push arg for builtin call
        case OP_CALL_BUILTIN:                                        // builtin call
        case OP_ASYNC_CALL_BUILTIN:                                  // async builtin call
            func_emit_builtin_call(st); break;
        case OP_RETURN: case OP_RETURN_NUM: case OP_RETURN_BOOL:
        case OP_RETURN_NUM_IMM: case OP_RETURN_NONE:
            func_emit_return(st);                                    // shared return logic
            emit_leave_ret(st->cb, st->abi, st->base_frame);         // leave; ret
            break;
        default:
            JIT_FATAL("call category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
}

// loop body dispatch for call opcodes
void x86_loop_emit_call(X86LoopBodyState* st) {
    if (st->save_slot < 0)                                           // helper needs a spill slot
        JIT_FATAL("no save slot for CALL (pc=%d op=%d)", st->pc, st->op);
    CodeBuf* cb = st->cb;                                            // destination code buffer
    x86_cache_flush(st->cache, cb);                                  // spill dirty slots to frame
    if (st->op == OP_CALL_1 || st->op == OP_CALL_2)
        x86_emit_movsd_load(cb, 0, x86_slot_disp(st->b));            // xmm0 = arg0
    if (st->op == OP_CALL_2)
        x86_emit_movsd_load(cb, 1, x86_slot_disp(st->b + 1));        // xmm1 = arg1
    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save frame_reg
    emit_call_func(cb, st->ctx, st->a);                              // indirect call through func_table
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));   // restore frame_reg
    x86_cache_clear(st->cache);                                      // callee clobbered all xmm regs
    x86_cache_put(st->cache, 0, st->d);                              // xmm0 holds the result
}
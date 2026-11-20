// source/jit/x86-64/x86_64_emit_basic.c
// Standard (basic) opcode emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include "jit_ops.h"
#include <stddef.h>

// argument registers for the first four integer parameters
#if defined(_WIN32) || defined(_WIN64)
#define BARG0 X86_RCX
#define BARG1 X86_RDX
#define BARG2 X86_R8
#define BARG3 X86_R9
#else
#define BARG0 X86_RDI
#define BARG1 X86_RSI
#define BARG2 X86_RDX
#define BARG3 X86_RCX
#endif

// emits movabs rax, fn; call rax
static void emit_call_helper(CodeBuf* cb, uint64_t fn) {
    x86_emit_movabs_rax(cb, fn);                                   // rax = helper address
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                          // call rax
}

// loads the vm pointer into dst through the ctx->vm indirection
static void emit_load_vm(CodeBuf* cb, JITContext* ctx, int dst) {
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&ctx->vm);        // rax = &ctx->vm
    x86_emit_load_r64_base(cb, dst, X86_RAX, 0);                   // dst = ctx->vm
}

// emits lea dst, [rbp + disp]
static void emit_lea_frame(CodeBuf* cb, int dst, int slot) {
    x86_emit_lea_r64_base(cb, dst, X86_RBP, x86_slot_disp(slot));  // dst = &frame[slot]
}

// emits cmp byte [vm->running], 0
static size_t emit_running_check_branch(CodeBuf* cb, JITContext* ctx, uint8_t jcc) {
    emit_load_vm(cb, ctx, X86_RAX);                                // rax = vm
    emit_u8(cb, 0x80); emit_u8(cb, 0xB8);                          // cmp byte [rax+disp32], ib
    emit_i32(cb, (int32_t)offsetof(VM, running));                  // offset of the running flag
    emit_u8(cb, 0x00);                                             // compare against zero
    emit_u8(cb, 0x0F); emit_u8(cb, jcc);                           // jcc rel32
    size_t at = cb->len;                                           // placeholder offset
    emit_i32(cb, 0);                                               // patched by the caller
    return at;
}

// function tier: records a bail-out to the shared cleanup epilogue
static void func_emit_running_check(X86FuncEmitState* st) {
    size_t at = emit_running_check_branch(st->cb, st->ctx, 0x84);  // je cleanup (running == 0)
    if (st->n_bail_fixups >= st->range_size)                       // one per pc is the hard bound
        JIT_FATAL("bail fixup overflow in function %d", st->func_idx);
    st->bail_fixup_at[st->n_bail_fixups++] = at;                   // patch once cleanup is emitted
}

// loop tier: records a bail-out to the loop's exit stub
static void loop_emit_running_check(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;
    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save frame_reg
    size_t at = emit_running_check_branch(cb, st->ctx, 0x84);      // je exit (running == 0)
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));   // restore frame_reg
    if (st->info->n_bail_patches >= st->ctx->chunk->code_count)    // one per pc is the hard bound
        JIT_FATAL("bail fixup overflow in loop at pc=%d", st->pc);
    st->info->bail_patch_at[st->info->n_bail_patches++] = at;      // patch once the exit is known
}

// function tier: emits a table read helper call with dst pointer in arg0
static void func_emit_table_get(X86FuncEmitState* st, uint64_t fn,
                                int n_args, int tbl_slot, int key_slot) {
    CodeBuf* cb = st->cb;
    x86_cache_flush(st->cache, cb);                                // spill before the call
    emit_lea_frame(cb, BARG0, st->d);                              // arg0 = &frame[d]
    if (tbl_slot >= 0)                                             // table value argument
        x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(tbl_slot)); // arg1 = frame[tbl]
    if (n_args >= 3 && key_slot >= 0)                              // key value argument
        x86_emit_load_r64_rbp(cb, BARG2, x86_slot_disp(key_slot)); // arg2 = frame[key]
    emit_call_helper(cb, fn);
    x86_cache_clear(st->cache);                                    // callee clobbered all xmm
}

// function tier: emits a table write helper call with the vm in arg0
static void func_emit_table_set(X86FuncEmitState* st, uint64_t fn,
                                int tbl_slot, int key_slot, int val_slot,
                                bool check_running) {
    CodeBuf* cb = st->cb;
    x86_cache_flush(st->cache, cb);                                // spill before the call
    emit_load_vm(cb, st->ctx, BARG0);                              // arg0 = vm
    x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(tbl_slot));     // arg1 = frame[tbl]
    if (key_slot >= 0)                                             // key value argument
        x86_emit_load_r64_rbp(cb, BARG2, x86_slot_disp(key_slot)); // arg2 = frame[key]
    if (val_slot >= 0)                                             // value argument
        x86_emit_load_r64_rbp(cb, BARG3, x86_slot_disp(val_slot)); // arg3 = frame[val]
    emit_call_helper(cb, fn);
    x86_cache_clear(st->cache);                                    // callee clobbered all xmm
    if (check_running) func_emit_running_check(st);                // error path stops the vm
}

// function tier: emits FOR_NEXT / FOR_NEXT_LOOP / TABLE_ITER_NEXT as a helper call
static void func_emit_iter_branch(X86FuncEmitState* st, uint64_t fn,
                                  int var_slot, int target_pc, bool jump_on_true) {
    CodeBuf* cb = st->cb;
    x86_cache_flush(st->cache, cb);                                // spill before the call
    emit_load_vm(cb, st->ctx, BARG0);                              // arg0 = vm
    emit_lea_frame(cb, BARG1, var_slot);                           // arg1 = &frame[var]
    emit_call_helper(cb, fn);                                      // al = continue?
    x86_cache_clear(st->cache);                                    // callee clobbered all xmm
    emit_u8(cb, 0x84); emit_u8(cb, 0xC0);                          // test al, al
    emit_u8(cb, 0x0F); emit_u8(cb, jump_on_true ? 0x85 : 0x84);    // jnz / jz rel32
    x86_func_jump_fixup(st, cb->len, target_pc);                   // resolve against the label
    emit_i32(cb, 0);                                               // rel32 placeholder
    st->did_flush = true;                                          // helper cleared the cache
}

// basic tier: releases the value currently held by the destination slot
void x86_func_emit_pre_decref(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;
    if (st->d < 0 || st->d >= JIT_MAX_SLOTS) return;               // no dest slot
    x86_cache_flush(st->cache, cb);                                // spill dirty slots
    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(st->d));      // rax = old value
    x86_emit_mov_r64_r64(cb, X86_RCX, X86_RAX);                    // rcx = old value
    x86_emit_movabs_r11(cb, X86_QNAN_MASK);                        // r11 = tag mask
    emit_u8(cb, 0x4C); emit_u8(cb, 0x21); emit_u8(cb, 0xD9);       // and rcx, r11
    x86_emit_cmp_r64(cb, X86_RCX, X86_R11);                        // (old & qnan) == qnan?
    emit_u8(cb, 0x0F); emit_u8(cb, 0x85);                          // jne skip (untagged)
    size_t skip_patch = cb->len;                                   // skip jump placeholder
    emit_i32(cb, 0);
    x86_emit_mov_r64_r64(cb, X86_RDI, X86_RAX);                    // arg0 = old value
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&value_decref);   // rax = &value_decref
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                          // call rax
    x86_cache_clear(st->cache);                                    // xmm regs clobbered
    int32_t skip_rel = (int32_t)cb->len - (int32_t)(skip_patch + 4);
    memcpy(cb->buf + skip_patch, &skip_rel, 4);                    // patch skip jump
}

// basic tier: true when a call must go through the checked args-stack path
bool x86_func_call_needs_check(X86FuncEmitState* st) {
    JITContext* ctx = st->ctx;                                     // jit context with tiers
    int target = st->a;                                            // callee function index
    if (target < 0 || target >= ctx->func_count) return true;      // unknown callee: stay safe
    if (ctx->strict_pure[target]) return true;                     // strict: runtime arg check
    return !(ctx->pure[target] || ctx->basic[target]);             // interpreted: coroutine path
}

// basic tier: emits a call through the interpreter's checked path
void x86_func_emit_call_checked(X86FuncEmitState* st, int argc) {
    CodeBuf* cb = st->cb;                                          // destination code buffer
    x86_cache_flush(st->cache, cb);                                // spill before the calls
    for (int i = 0; i < argc; i++) {                               // push args left to right
        emit_load_vm(cb, st->ctx, BARG0);                          // arg0 = vm
        x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->b + i)); // arg1 = frame[b+i]
        emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_push_arg_base);
    }
    emit_load_vm(cb, st->ctx, BARG0);                              // arg0 = vm
    emit_lea_frame(cb, BARG1, st->d);                              // arg1 = &frame[d]
    x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);   // arg2 = func idx
    x86_emit_mov_r64_imm32(cb, BARG3, (uint64_t)(int64_t)argc);    // arg3 = arg count
    emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_call);       // checked dispatch + boxing
    x86_cache_clear(st->cache);                                    // callee clobbered all xmm
    func_emit_running_check(st);                                   // callee errors stop the vm
    st->did_flush = true;                                          // helpers cleared the cache
}

// function body dispatch for the standard (basic) opcode tier
void x86_func_emit_basic(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                          // destination code buffer

    switch (st->op) {
        case OP_MOVE: {                                            // refcount-aware copy
            x86_cache_flush(st->cache, cb);                        // spill before the call
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->a)); // arg1 = frame[a]
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_move);
            x86_cache_clear(st->cache);                            // callee clobbered all xmm
            break;
        }
        case OP_CONCAT: {                                          // string concatenation
            x86_cache_flush(st->cache, cb);
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            emit_lea_frame(cb, BARG1, st->a);                      // arg1 = &frame[a]
            emit_lea_frame(cb, BARG2, st->b);                      // arg2 = &frame[b]
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_concat);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_NEW_TABLE: {                                        // fresh empty table
            x86_cache_flush(st->cache, cb);
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_new_table);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_LOAD_TABLE: {                                       // table literal
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = const idx
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_load_table);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_TABLE_GET: {                                        // generic keyed read
            func_emit_table_get(st, (uint64_t)(uintptr_t)&jit_op_table_get,
                                3, st->a, st->b);
            break;
        }
        case OP_TABLE_GET_INT: {                                    // fixed index read
            x86_cache_flush(st->cache, cb);
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->a)); // arg1 = table
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->b);  // arg2 = index
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_get_int);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_TABLE_GET_NUM: {                                    // numeric-key read
            func_emit_table_get(st, (uint64_t)(uintptr_t)&jit_op_table_get_num,
                                3, st->a, st->b);
            break;
        }
        case OP_TABLE_GET_CONST: {                                  // interned key read
            x86_cache_flush(st->cache, cb);
            StringObject* key = (StringObject*)st->chunk->constants[st->b].cached_str;
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->a)); // arg1 = table
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)key);     // rax = interned key
            x86_emit_mov_r64_r64(cb, BARG2, X86_RAX);              // arg2 = key
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_get_const);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_TABLE_GET_KEY_STR: {                                // fused prefix read
            x86_cache_flush(st->cache, cb);
            int prefix_idx = (int)((uint32_t)st->b >> 16);         // packed prefix index
            int num_reg    = st->b & 0xFFFF;                       // packed number register
            StringObject* prefix =
                (StringObject*)st->chunk->constants[prefix_idx].cached_str;
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->a)); // arg1 = table
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)prefix);  // rax = prefix
            x86_emit_mov_r64_r64(cb, BARG2, X86_RAX);              // arg2 = prefix
            x86_emit_load_r64_rbp(cb, BARG3, x86_slot_disp(num_reg)); // arg3 = num bits
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_get_key_str);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_TABLE_SET: {                                        // generic keyed write
            func_emit_table_set(st, (uint64_t)(uintptr_t)&jit_op_table_set,
                                st->d, st->a, st->b, true);
            break;
        }
        case OP_TABLE_SET_INT: {                                    // fixed index write
            CodeBuf* cb2 = st->cb;
            x86_cache_flush(st->cache, cb2);
            emit_load_vm(cb2, st->ctx, BARG0);                     // arg0 = vm
            x86_emit_load_r64_rbp(cb2, BARG1, x86_slot_disp(st->d)); // arg1 = table
            x86_emit_mov_r64_imm32(cb2, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = index
            x86_emit_load_r64_rbp(cb2, BARG3, x86_slot_disp(st->b)); // arg3 = value
            emit_call_helper(cb2, (uint64_t)(uintptr_t)&jit_op_table_set_int);
            x86_cache_clear(st->cache);
            func_emit_running_check(st);                           // error path stops the vm
            break;
        }
        case OP_TABLE_SET_NUM: {                                    // numeric-key write
            func_emit_table_set(st, (uint64_t)(uintptr_t)&jit_op_table_set_num,
                                st->d, st->a, st->b, true);
            break;
        }
        case OP_TABLE_SET_CONST: {                                  // interned key write
            x86_cache_flush(st->cache, cb);
            StringObject* key = (StringObject*)st->chunk->constants[st->a].cached_str;
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = table
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)key);     // rax = interned key
            x86_emit_mov_r64_r64(cb, BARG2, X86_RAX);              // arg2 = key
            x86_emit_load_r64_rbp(cb, BARG3, x86_slot_disp(st->b)); // arg3 = value
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_set_const);
            x86_cache_clear(st->cache);
            func_emit_running_check(st);                           // error path stops the vm
            break;
        }
        case OP_TABLE_SET_KEY_STR: {                                // fused prefix write
            x86_cache_flush(st->cache, cb);
            int prefix_idx = (int)((uint32_t)st->b >> 16);         // packed prefix index
            int num_reg    = st->b & 0xFFFF;                       // packed number register
            StringObject* prefix =
                (StringObject*)st->chunk->constants[prefix_idx].cached_str;
            int pair_slot = st->pair_slot;                          // scratch [num, val] pair
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(num_reg)); // rax = num bits
            x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(pair_slot + 1));   // pair[0] = num (base)
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(st->a)); // rax = value bits
            x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(pair_slot));       // pair[1] = val (base+8)
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = table
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)prefix);  // rax = prefix
            x86_emit_mov_r64_r64(cb, BARG2, X86_RAX);              // arg2 = prefix
            emit_lea_frame(cb, BARG3, pair_slot + 1);              // arg3 = &pair[0]
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_set_key_str);
            x86_cache_clear(st->cache);
            func_emit_running_check(st);                           // error path stops the vm
            break;
        }
        case OP_TABLE_APPEND: {                                     // positional append
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = table
            x86_emit_load_r64_rbp(cb, BARG2, x86_slot_disp(st->a)); // arg2 = value
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_append);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_LOAD_GLOBAL: {                                      // global read
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = global idx
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_load_global);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_STORE_GLOBAL: {                                     // global write
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = value
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = global idx
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_store_global);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_FOR_INIT: {                                         // push iterator frame
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = start
            x86_emit_load_r64_rbp(cb, BARG2, x86_slot_disp(st->a)); // arg2 = end
            x86_emit_load_r64_rbp(cb, BARG3, x86_slot_disp(st->b)); // arg3 = step
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_for_init);
            x86_cache_clear(st->cache);
            func_emit_running_check(st);                           // overflow stops the vm
            break;
        }
        case OP_FOR_NEXT: {                                         // advance, exit on end
            func_emit_iter_branch(st, (uint64_t)(uintptr_t)&jit_op_for_next,
                                  st->d, st->a, false);            // jz -> exit target
            break;
        }
        case OP_FOR_NEXT_LOOP: {                                    // inverted back edge
            func_emit_iter_branch(st, (uint64_t)(uintptr_t)&jit_op_for_next_loop,
                                  st->d, st->a, true);             // jnz -> body target
            break;
        }
        case OP_POP_ITER: {                                         // pop numeric iterator
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_pop_iter);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_TABLE_ITER_INIT: {                                  // push table iterator
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = table
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_iter_init);
            x86_cache_clear(st->cache);
            func_emit_running_check(st);                           // overflow stops the vm
            break;
        }
        case OP_TABLE_ITER_NEXT: {                                  // advance, exit on end
            func_emit_iter_branch(st, (uint64_t)(uintptr_t)&jit_op_table_iter_next,
                                  st->d, st->b, false);            // jz -> exit target
            break;
        }
        case OP_POP_TABLE_ITER: {                                   // pop table iterator
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_pop_table_iter);
            x86_cache_clear(st->cache);
            break;
        }
        case OP_CALL: {                                             // many-arg call via coroutine
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = func idx
            x86_emit_mov_r64_imm32(cb, BARG3, (uint64_t)(int64_t)st->b);  // arg3 = arg count
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_call);
            x86_cache_clear(st->cache);
            func_emit_running_check(st);                           // callee errors stop the vm
            break;
        }
        case OP_ASYNC_CALL: {                                       // pending future capture
            x86_cache_flush(st->cache, cb);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = func idx
            x86_emit_mov_r64_imm32(cb, BARG3, (uint64_t)(int64_t)st->b);  // arg3 = arg count
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_async_call);
            x86_cache_clear(st->cache);
            break;
        }
        default:
            JIT_FATAL("basic category: unexpected opcode %d (func=%d pc=%d)",
                      st->op, st->func_idx, st->pc);
    }
}

// loop tier: shared prologue for helper calls inside a native loop body
static void loop_helper_prologue(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;
    x86_cache_flush(st->cache, cb);                                // spill dirty slots
    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save frame_reg
}

// loop tier: shared epilogue for helper calls inside a native loop body
static void loop_helper_epilogue(X86LoopBodyState* st, bool check_running) {
    CodeBuf* cb = st->cb;
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));   // restore frame_reg
    x86_cache_clear(st->cache);                                    // callee clobbered all xmm
    if (check_running) loop_emit_running_check(st);                // error path bails to the exit
}

// loop body dispatch for the standard (basic) opcode tier
void x86_loop_emit_basic(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                          // destination code buffer

    switch (st->op) {
        case OP_MOVE: {                                            // refcount-aware copy
            loop_helper_prologue(st);
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->a)); // arg1 = frame[a]
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_move);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_CONCAT: {                                          // string concatenation
            loop_helper_prologue(st);
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            emit_lea_frame(cb, BARG1, st->a);                      // arg1 = &frame[a]
            emit_lea_frame(cb, BARG2, st->b);                      // arg2 = &frame[b]
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_concat);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_NEW_TABLE: {                                        // fresh empty table
            loop_helper_prologue(st);
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_new_table);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_LOAD_TABLE: {                                       // table literal
            loop_helper_prologue(st);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = const idx
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_load_table);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_TABLE_APPEND: {                                     // positional append
            loop_helper_prologue(st);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = table
            x86_emit_load_r64_rbp(cb, BARG2, x86_slot_disp(st->a)); // arg2 = value
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_append);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_TABLE_GET: {                                        // generic keyed read
            loop_helper_prologue(st);
            emit_lea_frame(cb, BARG0, st->d);                      // arg0 = &frame[d]
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->a)); // arg1 = table
            x86_emit_load_r64_rbp(cb, BARG2, x86_slot_disp(st->b)); // arg2 = key
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_get);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_TABLE_SET: {                                        // generic keyed write
            loop_helper_prologue(st);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = table
            x86_emit_load_r64_rbp(cb, BARG2, x86_slot_disp(st->a)); // arg2 = key
            x86_emit_load_r64_rbp(cb, BARG3, x86_slot_disp(st->b)); // arg3 = value
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_table_set);
            loop_helper_epilogue(st, true);                        // error path bails out
            break;
        }
        case OP_LOAD_GLOBAL: {                                      // helper-based global read
            loop_helper_prologue(st);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = global idx
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_load_global);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_STORE_GLOBAL: {                                     // helper-based global write
            loop_helper_prologue(st);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            x86_emit_load_r64_rbp(cb, BARG1, x86_slot_disp(st->d)); // arg1 = value
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = global idx
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_store_global);
            loop_helper_epilogue(st, false);
            break;
        }
        case OP_CALL: {                                             // many-arg call via coroutine
            loop_helper_prologue(st);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = func idx
            x86_emit_mov_r64_imm32(cb, BARG3, (uint64_t)(int64_t)st->b);  // arg3 = arg count
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_call);
            loop_helper_epilogue(st, true);                        // callee errors bail out
            break;
        }
        case OP_ASYNC_CALL: {                                       // pending future capture
            loop_helper_prologue(st);
            emit_load_vm(cb, st->ctx, BARG0);                      // arg0 = vm
            emit_lea_frame(cb, BARG1, st->d);                      // arg1 = &frame[d]
            x86_emit_mov_r64_imm32(cb, BARG2, (uint64_t)(int64_t)st->a);  // arg2 = func idx
            x86_emit_mov_r64_imm32(cb, BARG3, (uint64_t)(int64_t)st->b);  // arg3 = arg count
            emit_call_helper(cb, (uint64_t)(uintptr_t)&jit_op_async_call);
            loop_helper_epilogue(st, false);
            break;
        }
        default:
            JIT_FATAL("loop basic category: unexpected opcode %d (pc=%d)",
                      st->op, st->pc);
    }
}
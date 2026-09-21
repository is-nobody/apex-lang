// source/jit/x86-64/x86_64_emit_loop.c
// Loop emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include "vm.h"
#include <stddef.h>
#include <string.h>

// emits one non-jump loop-body instruction using the xmm register cache
void emit_loop_body_instr(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                          XmmCache* cache, int pc, int const_slot, int save_slot,
                          JitLoopInfo* info) {
    (void)abi;                                                   // reserved for ABI-specific opcodes
    BytecodeChunk* chunk = ctx->chunk;
    Instruction* inst = &chunk->code[pc];
    int d = inst->operands[0];
    int a = inst->operands[1];
    int b = inst->operands[2];

    bool touched_nan_check = false;                              // set by arithmetic ops

    switch (inst->opcode) {
        case OP_MOVE: {
            if (d == a) break;                               // self-move is a no-op
            int xa = x86_cache_load(cache, cb, a);           // load source slot
            if (xa < 0) return;                              // register cache full
            if (!slot_live_in_loop(chunk, info->entry_pc,
                                   info->back_edge_pc, pc, a)) {
                x86_cache_put(cache, xa, d);                 // a is dead: relabel
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, -1);
                if (xd < 0) return;                          // register cache full
                if (xd != xa)
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);     // movapd xd, xa
                x86_cache_put(cache, xd, d);                 // a survives in xa
            }
            break;
        }
        case OP_LOAD_NUM_IMM: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            x86_emit_load_double_imm(cb, x, a);                  // xmmX = (double)a (cvtsi2sd)
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_LOAD_NUM: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            double v = chunk->constants[a].number_value;         // fetch pool constant
            uint64_t bits; memcpy(&bits, &v, 8);                 // reinterpret as u64
            x86_emit_movabs_rax(cb, bits);                       // rax = bit pattern
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_LOAD_BOOL: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            uint64_t bits = X86_BOOL_BITS | (a ? 1ULL : 0ULL);   // bit 0 carries value
            x86_emit_movabs_rax(cb, bits);                       // rax = nan-boxed bool
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_LOAD_NONE: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            x86_emit_movabs_rax(cb, X86_NONE_BITS);              // rax = NONE bit pattern
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_TABLE_SET_KEY_STR: {                         // tbl["prefix" .. num] = val
            if (save_slot < 0) return;
            x86_cache_flush(cache, cb);

            int table_reg  = d;                              // operands[0]
            int val_reg    = a;                              // operands[1]
            int prefix_idx = (int)((uint32_t)b >> 16);       // unpack
            int num_reg    = b & 0xFFFF;

            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save caller frame pointer BEFORE clobbering

            // rdi = raw Table*
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));
            x86_emit_clear_high16_rax(cb);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);  // mov rcx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);  // mov rdi, rax
#endif

            // rsi / rdx = prefix
            uint64_t prefix_addr = (uint64_t)(uintptr_t)chunk->constants[prefix_idx].cached_str;
            x86_emit_movabs_rax(cb, prefix_addr);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

            // xmm0 = num (sysv) / xmm2 = num (win64: arg 2 is a float)
            x86_emit_movsd_load(cb, 0, x86_slot_disp(num_reg));
#if defined(_WIN32) || defined(_WIN64)
            x86_emit_sse66_rr(cb, 0x28, 2, 0);               // win64: xmm2 = num
#endif

            // last arg: value
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(val_reg));
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x49); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);  // mov r9, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#endif

            uint64_t helper = (uint64_t)(uintptr_t)&jit_table_set_key_str;
            x86_emit_movabs_rax(cb, helper);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore caller frame pointer

            x86_cache_clear(cache);
            break;
        }
        case OP_TABLE_GET_KEY_STR: {                         // d = tbl["prefix" .. num]
            if (save_slot < 0) return;
            x86_cache_flush(cache, cb);

            int table_reg  = a;                              // operands[1]
            int prefix_idx = (int)((uint32_t)b >> 16);       // unpack
            int num_reg    = b & 0xFFFF;

            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save caller frame pointer BEFORE clobbering

            // rdi = raw Table*
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));
            x86_emit_clear_high16_rax(cb);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);  // mov rcx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);  // mov rdi, rax
#endif

            // rsi / rdx = prefix
            uint64_t prefix_addr = (uint64_t)(uintptr_t)chunk->constants[prefix_idx].cached_str;
            x86_emit_movabs_rax(cb, prefix_addr);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

            // xmm0 = num (sysv) / xmm2 = num (win64: arg 2 is a float)
            x86_emit_movsd_load(cb, 0, x86_slot_disp(num_reg));
#if defined(_WIN32) || defined(_WIN64)
            x86_emit_sse66_rr(cb, 0x28, 2, 0);               // win64: xmm2 = num
#endif

            uint64_t helper = (uint64_t)(uintptr_t)&jit_table_get_key_str;
            x86_emit_movabs_rax(cb, helper);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

            // value result returned in rax; publish it to the destination slot
            x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(d));
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore caller frame pointer

            x86_cache_clear(cache);
            break;
        }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: {
            int xa = x86_cache_load(cache, cb, a);               // load left operand
            if (xa < 0) return;                                  // register cache full
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load right, avoid xa
            if (xb < 0) return;                                  // register cache full
            uint8_t op;                                          // sse opcode
            switch (inst->opcode) {
                case OP_ADD: op = 0x58; break;                   // addsd
                case OP_SUB: op = 0x5C; break;                   // subsd
                case OP_MUL: op = 0x59; break;                   // mulsd
                default:     op = 0x5E; break;                   // divsd
            }
            int xd = xa;                                         // result register, xa by default
            if (d != a) {
                // dest != left source: prefer a separate xmm so a survives
                xd = x86_cache_dest_reg(cache, cb, d, xa, xb);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);         // movapd xd, xa
                }
            }
            x86_emit_sse_arith_rr(cb, op, xd, xb);               // xd op= xb
            x86_cache_put(cache, xd, d);                         // publish dest
            touched_nan_check = true;
            break;
        }
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM: case OP_DIV_IMM: {
            int xa = x86_cache_load(cache, cb, a);               // load left operand
            if (xa < 0) return;                                  // register cache full
            uint8_t op;                                          // sse opcode
            switch (inst->opcode) {
                case OP_ADD_IMM: op = 0x58; break;               // addsd
                case OP_SUB_IMM: op = 0x5C; break;               // subsd
                case OP_MUL_IMM: op = 0x59; break;               // mulsd
                default:         op = 0x5E; break;               // divsd
            }
            int slot = -1;
            if (info->imm_opt_ok) {
                for (int i = 0; i < info->n_imms; i++)
                    if (info->imms[i].value == b) { slot = info->imms[i].slot; break; }
            }
            // dest != left source: spill a before the in-place update
            int xd = xa;                                         // result register, xa by default
            if (d != a) {
                xd = x86_cache_dest_reg(cache, cb, d, xa, XMM_SCRATCH);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);         // movapd xd, xa
                }
            }
            if (slot >= 0) {
                x86_emit_sse_arith_mem(cb, op, xd, x86_slot_disp(slot));  // xd op= [imm]
            } else {
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                x86_emit_sse_arith_rr(cb, op, xd, XMM_SCRATCH);
            }
            x86_cache_put(cache, xd, d);                         // publish dest
            touched_nan_check = true;
            break;
        }
        case OP_MOD_IMM: {                                       // modulo with immediate
            int xa = x86_cache_load(cache, cb, a);               // load a
            if (xa < 0) return;                                  // register cache full
            int slot = -1;
            if (info->imm_opt_ok) {
                for (int i = 0; i < info->n_imms; i++)
                    if (info->imms[i].value == b) { slot = info->imms[i].slot; break; }
            }
            int xt;
            if (slot >= 0) {
                xt = x86_cache_alloc_excl(cache, cb, xa, -1);    // temp for a/imm
                if (xt < 0) return;
                x86_emit_sse66_rr(cb, 0x28, xt, xa);             // xt = a
                x86_emit_sse_arith_mem(cb, 0x5E, xt, x86_slot_disp(slot));  // xt = a / imm
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, xt, xt);                          // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);
                emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));
                emit_u8(cb, 0x03);                               // truncate
                x86_emit_sse_arith_mem(cb, 0x59, xt, x86_slot_disp(slot));  // xt *= imm
            } else {
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                xt = x86_cache_alloc_excl(cache, cb, xa, XMM_SCRATCH);
                if (xt < 0) return;
                x86_emit_sse66_rr(cb, 0x28, xt, xa);
                x86_emit_sse_arith_rr(cb, 0x5E, xt, XMM_SCRATCH);
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, xt, xt);                          // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);
                emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));
                emit_u8(cb, 0x03);
                x86_emit_sse_arith_rr(cb, 0x59, xt, XMM_SCRATCH);
            }
            // dest != left source: preserve a's original value in its frame slot
            if (d == a) {
                x86_emit_sse_arith_rr(cb, 0x5C, xa, xt);         // a = a - xt
                x86_cache_put(cache, xa, d);
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, xt);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);         // movapd xd, xa
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xd, xt);         // xd = xd - xt
                x86_cache_put(cache, xd, d);
            }
            touched_nan_check = true;
            break;
        }
        case OP_MOD: {
            int xa = x86_cache_load(cache, cb, a);               // load a
            if (xa < 0) return;                                  // register cache full
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load b, avoid xa
            if (xb < 0) return;                                  // register cache full
            x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);        // scratch = a
            x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);    // scratch = a / b
            emit_u8(cb, 0x66);                                   // roundsd legacy prefix
            x86_rex_rb(cb, XMM_SCRATCH, XMM_SCRATCH);            // rex.r/rex.b for xmm8-xmm15
            emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
            emit_u8(cb, 0x0B);                                   // roundsd opcode
            emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3) | (XMM_SCRATCH & 7));  // round scratch
            emit_u8(cb, 0x03);                                   // round mode: truncate
            x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);    // scratch = trunc(a/b) * b
            // dest != left source: preserve a's original value in its frame slot
            if (d == a) {
                x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);   // a = a - scratch
                x86_cache_put(cache, xa, d);
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, -1);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);             // movapd xd, xa
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xd, XMM_SCRATCH);   // xd = xd - scratch
                x86_cache_put(cache, xd, d);
            }
            touched_nan_check = true;
            break;
        }
        case OP_NEG: {
            int xa = x86_cache_load(cache, cb, a);               // load operand
            if (xa < 0) return;                                  // register cache full
            x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // scratch = 0
            x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);    // scratch = 0 - a
            // dest != source: preserve a's original value in its frame slot
            if (d == a) {
                x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);        // a = -a
                x86_cache_put(cache, xa, d);
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, -1);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                }
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // xd = -a
                x86_cache_put(cache, xd, d);
            }
            touched_nan_check = true;
            break;
        }
        case OP_INC: case OP_DEC: {
            int xd = x86_cache_load(cache, cb, d);               // load dest
            if (xd < 0) return;                                  // register cache full
            uint8_t op = (inst->opcode == OP_INC) ? 0x58 : 0x5C; // addsd / subsd
            x86_emit_sse_arith_mem(cb, op, xd, x86_slot_disp(const_slot));  // xd op= 1.0
            x86_cache_put(cache, xd, d);                         // mark dest dirty
            touched_nan_check = true;
            break;
        }
        case OP_CMP_EQ: case OP_CMP_EQ_NUM:
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE: {
            int xa = x86_cache_load(cache, cb, a);               // load a
            if (xa < 0) return;                                  // register cache full
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load b, avoid xa
            if (xb < 0) return;                                  // register cache full
            x86_emit_ucomisd_rr(cb, xa, xb);                     // ucomisd a, b
            uint8_t cc;                                          // setcc opcode
            switch (inst->opcode) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  cc = 0x94; break;  // sete
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: cc = 0x95; break;  // setne
                case OP_CMP_LT:  cc = 0x92; break;               // setb
                case OP_CMP_GT:  cc = 0x97; break;               // seta
                case OP_CMP_LTE: cc = 0x96; break;               // setbe
                default:         cc = 0x93; break;               // setae (gte)
            }
            // reuse the xmm currently holding d, if any: keeps the dest in
            int xd = x86_cache_lookup(cache, d);
            if (xd == xa || xd == xb) xd = -1;                   // dest aliases a source
            if (xd < 0) xd = x86_cache_dest_reg(cache, cb, d, xa, xb); // prefer d's home
            if (xd < 0) return;                                  // register cache full
            x86_emit_cmp_box_result(cb, xd, cc);                 // nan-boxed MAKE_BOOL into xd
            x86_cache_put(cache, xd, d);                         // cache dest
            break;
        }
        case OP_CALL_0:                                      // call with no args
        case OP_CALL_1:                                      // call with one arg
        case OP_CALL_2: {                                    // call with two args
            if (save_slot < 0) return;                       // no save slot reserved, bail
            x86_cache_flush(cache, cb);                      // spill dirty slots to frame
            if (inst->opcode == OP_CALL_1 || inst->opcode == OP_CALL_2) {
                x86_emit_movsd_load(cb, 0, x86_slot_disp(b));  // xmm0 = arg0
            }
            if (inst->opcode == OP_CALL_2) {
                x86_emit_movsd_load(cb, 1, x86_slot_disp(b + 1));  // xmm1 = arg1
            }
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg
            emit_call_func(cb, ctx, a);                      // indirect call through func_table
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));   // restore frame_reg
            x86_cache_clear(cache);                          // callee clobbered all xmm regs
            x86_cache_put(cache, 0, d);                      // xmm0 holds the result
            break;
        }
        case OP_TABLE_GET_NUM:                               // same semantics as OP_TABLE_GET
        case OP_TABLE_GET: {
            int table_reg = a;                               // table register
            int key_reg   = b;                               // key register

            // fast path: counter key + this loop's primary table
            if (key_reg == info->for_var_reg && table_reg == info->table.slot) {
                int xk = cache->slot_reg[key_reg];           // key already in cache?
                if (xk < 0) {
                    xk = x86_cache_load(cache, cb, key_reg); // load key from frame
                    if (xk < 0) return;                      // register cache full
                }
                x86_emit_cvttsd2si_eax(cb, xk);              // eax = (int)key
                x86_emit_dec_eax(cb);                        // 1-based -> 0-based
                int xd = x86_cache_dest_reg(cache, cb, d, xk, -1); // prefer d's home
                if (xd < 0) return;                          // register cache full
                x86_emit_movsd_load_idx8(cb, xd, X86_RBX, X86_RAX);  // xd = array_part[rax]
                x86_cache_put(cache, xd, d);                 // cache dest
                break;
            }

            // general path: fetch the table pointer, convert the key, index array_part
            int xk = cache->slot_reg[key_reg];
            if (xk < 0) {
                xk = x86_cache_load(cache, cb, key_reg);
                if (xk < 0) return;                          // register cache full
            }

            int xt = cache->slot_reg[table_reg];             // table still in an xmm?
            if (xt >= 0) {
                x86_emit_movq_rax_xmm(cb, xt);               // rax = table bits
            } else {
                x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));  // rax = frame[table_reg]
            }
            x86_emit_clear_high16_rax(cb);                   // strip nan-box tag
            x86_emit_load_r64_base(cb, X86_RAX, X86_RAX,
                                (int32_t)offsetof(Table, array_part));  // rax = array_part
            x86_emit_cvttsd2si_edx(cb, xk);                  // edx = (int)key
            x86_emit_dec_edx(cb);                            // 1-based -> 0-based
            int xd = x86_cache_dest_reg(cache, cb, d, xk, -1);  // prefer d's home
            if (xd < 0) return;                              // register cache full
            x86_emit_movsd_load_idx8(cb, xd, X86_RAX, X86_RDX);  // xd = array_part[rdx]
            x86_cache_put(cache, xd, d);                     // cache dest
            break;
        }
        case OP_TABLE_GET_INT: {
            int xd = x86_cache_dest_reg(cache, cb, d, -1, -1);   // prefer d's home
            if (xd < 0) return;                                  // register cache full
            x86_emit_movsd_load_base(cb, X86_RBX, xd, (b - 1) * 8);  // xd = array_part[b-1]
            x86_cache_put(cache, xd, d);                         // cache dest
            break;
        }
        case OP_TABLE_SET_NUM:                               // same semantics as OP_TABLE_SET
        case OP_TABLE_SET: {
            // d = table, a = key, b = value
            int table_reg = d;                               // table register
            int key_reg   = a;                               // key register
            int val_reg   = b;                               // value register

            // fast path: counter key + this loop's primary table
            if (key_reg == info->for_var_reg && table_reg == info->table.slot) {
                int xk = cache->slot_reg[key_reg];
                if (xk < 0) {
                    xk = x86_cache_load(cache, cb, key_reg);
                    if (xk < 0) return;                      // register cache full
                }
                x86_emit_cvttsd2si_eax(cb, xk);              // eax = (int)key
                x86_emit_dec_eax(cb);                        // 1-based -> 0-based
                int xv = x86_cache_load_excl(cache, cb, val_reg, xk, -1);  // load value, keep key
                if (xv < 0) return;                          // register cache full
                x86_emit_movsd_store_idx8(cb, X86_RBX, X86_RAX, xv);  // array_part[rax] = xv
                break;
            }

            // general path: call the runtime helper so array_part is grown safely
            if (save_slot < 0) return;                    // no save slot reserved, bail
            int xk = cache->slot_reg[key_reg];
            if (xk < 0) {
                xk = x86_cache_load(cache, cb, key_reg);
                if (xk < 0) return;                       // register cache full
            }
            int xv = cache->slot_reg[val_reg];
            if (xv < 0) {
                xv = x86_cache_load(cache, cb, val_reg);
                if (xv < 0) return;                       // register cache full
            }

            x86_cache_flush(cache, cb);                   // spill dirty slots before the call
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

#if defined(_WIN32) || defined(_WIN64)
            x86_emit_cvttsd2si_edx(cb, xk);               // edx = (int)key
            x86_emit_dec_edx(cb);                         // 1-based -> 0-based
            // rdx = index (upper 32 already zeroed)
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));  // rax = frame[table_reg]
            x86_emit_clear_high16_rax(cb);                // strip nan-box tag
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);       // mov rcx, rax
            x86_emit_movq_rax_xmm(cb, xv);                // rax = value bits
            emit_u8(cb, 0x49); emit_u8(cb, 0x89); emit_u8(cb, 0xC0);       // mov r8, rax
#else
            x86_emit_cvttsd2si_edx(cb, xk);               // edx = (int)key
            x86_emit_dec_edx(cb);                         // 1-based -> 0-based
            emit_u8(cb, 0x89); emit_u8(cb, 0xD6);         // mov esi, edx
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));  // rax = frame[table_reg]
            x86_emit_clear_high16_rax(cb);                // strip nan-box tag
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);       // mov rdi, rax
            x86_emit_movq_rax_xmm(cb, xv);                // rax = value bits
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);       // mov rdx, rax
#endif
            uint64_t addr = (uint64_t)(uintptr_t)&table_set_int;  // helper that grows array_part
            x86_emit_movabs_rax(cb, addr);                // rax = helper
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);         // call rax
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore frame_reg
            x86_cache_clear(cache);                       // the call clobbers all xmm
            break;
        }
        case OP_TABLE_SET_INT: {
            int xv = x86_cache_load(cache, cb, b);               // load value to store
            if (xv < 0) return;                                  // register cache full
            x86_emit_movsd_store_base(cb, X86_RBX, xv, (a - 1) * 8);  // array_part[a-1] = xv
            break;
        }
        case OP_LOAD_GLOBAL: {
            int idx = a;                                         // global index
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            x86_emit_movsd_load_base(cb, X86_RBX, x, idx * 8);   // rbx = globals base, x = globals[idx]
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_STORE_GLOBAL: {
            int idx = a;                                         // global index
            int xv = x86_cache_load(cache, cb, d);               // load source slot
            if (xv < 0) return;                                  // register cache full
            x86_emit_movsd_store_base(cb, X86_RBX, xv, idx * 8); // globals[idx] = xv
            break;
        }
        default:
            break;                                               // unreachable in native loops
    }

    if (touched_nan_check && info->touches_tables) {         // NaN propagation matches interpreter
        int xd = cache->slot_reg[d];                         // dest slot still in cache?
        if (xd >= 0) emit_nan_check(cb, xd);                 // rewrite NaN to NONE bits
    }
}

// emits one iteration (entry test + body) and returns the fixup offset for the exit jump
size_t emit_loop_iteration(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                           XmmCache* cache, JitLoopInfo* info, int const_slot,
                           int save_slot, bool iter_in_xmm, int step_sign,
                           bool needs_helper) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry = info->entry_pc;                              // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry

    size_t exit_patch;                                       // offset of the exit jump placeholder
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {                // counter-based for loop
        int var_reg = chunk->code[entry].operands[0];        // loop counter slot
        int end_reg = info->for_end_reg;                     // end bound slot
        int step_reg = info->for_step_reg;                   // step slot

        int xa = x86_cache_load(cache, cb, end_reg);         // load end bound
        if (xa < 0) return (size_t)-1;                       // register cache full
        int xb = x86_cache_load_excl(cache, cb, step_reg, xa, -1);  // load step, avoid xa
        if (xb < 0) return (size_t)-1;                       // register cache full
        int xc = x86_cache_alloc_for(cache, cb, var_reg, xa, xb);   // allocate reg, no load
        if (xc < 0) return (size_t)-1;                       // register cache full

        if (!iter_in_xmm) {
            x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(info->nregs));  // xmm15 = iterator
        }
        x86_emit_ucomisd_rr(cb, XMM_SCRATCH, xa);            // ucomisd xmm15, xa
        emit_u8(cb, 0x0F);
        emit_u8(cb, (step_sign > 0) ? 0x87 : 0x82);          // ja exit / jb exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32

        x86_emit_sse66_rr(cb, 0x28, xc, XMM_SCRATCH);        // movapd xc, xmm15 (R[var] = c)
        x86_cache_put(cache, xc, var_reg);                   // var became dirty
        x86_emit_movsd_store(cb, xc, x86_slot_disp(var_reg)); // frame[var] = i
        cache->slot_dirty[var_reg] = false;                  // memory is in sync

        x86_emit_sse_arith_rr(cb, 0x58, XMM_SCRATCH, xb);    // addsd xmm15, xb (step)
        if (!iter_in_xmm) {
            x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->nregs));  // store iterator
        }
    } else if (entry_op_is_imm(chunk->code[entry].opcode)) { // imm-compare loop entry
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];                     // left operand slot
        int imm = entry_inst->operands[2];                   // immediate
        int xa = x86_cache_load(cache, cb, a);               // load left operand
        if (xa < 0) return (size_t)-1;                       // register cache full
        x86_emit_load_double_imm(cb, XMM_SCRATCH, imm);      // xmm15 = (double)imm
        x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);            // ucomisd xa, xmm15
        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);  // exit condition opcode
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                 // conditional exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32
    } else {                                                 // condition-based loop (register-register)
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];                     // left operand slot
        int b = entry_inst->operands[2];                     // right operand slot
        int xa = x86_cache_load(cache, cb, a);               // load left operand
        if (xa < 0) return (size_t)-1;                       // register cache full
        int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load right, avoid xa
        if (xb < 0) return (size_t)-1;                       // register cache full

        x86_emit_ucomisd_rr(cb, xa, xb);                     // ucomisd xa, xb

        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);  // exit condition opcode
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                 // conditional exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32
    }

    for (int pc = entry + 1; pc < back_edge; pc++) {         // body
        emit_loop_body_instr(abi, ctx, cb, cache, pc, const_slot, save_slot, info);
    }

    // helpers read their args from the frame, so spill live dirty slots before
    if (needs_helper) {
        x86_cache_flush(cache, cb);
    }

    return exit_patch;                                       // caller patches the exit jump
}

// rejects loops that would need runtime checks the emitter does not produce
bool loop_is_safe_to_emit(JITContext* ctx, JitLoopInfo* info) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry     = info->entry_pc;                          // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry

    if (info->table.used && info->globals_count > 0) return false;

    if (info->kind == JIT_LOOP_NUMERIC_FOR) {
        for (int pc = entry + 1; pc < back_edge; pc++) {     // scan body for writes
            Instruction* inst = &chunk->code[pc];
            if (!op_writes_dest(inst->opcode)) continue;     // op does not produce a dest
            int d = inst->operands[0];                       // destination slot
            if (d == info->for_end_reg || d == info->for_step_reg) return false;
        }
    }

    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        if (inst->opcode == OP_TABLE_SET_INT) {       // rbx holds only info->table.slot's array_part
            if (inst->operands[0] != info->table.slot) return false;
        } else if (inst->opcode == OP_TABLE_GET_INT) {       // same rbx issue as SET_INT
            if (inst->operands[1] != info->table.slot) return false;
        } else if (inst->opcode == OP_CALL_0 ||              // call targets must be JIT-compiled
                   inst->opcode == OP_CALL_1 ||
                   inst->opcode == OP_CALL_2) {
            int target = inst->operands[1];                  // callee function index
            if (target < 0 || target >= ctx->func_count) return false;
            if (!ctx->func_table[target]) return false;      // callee has no native code
        }
        // OP_TABLE_SET now always uses the general helper path, no rbx assumption
    }
    return true;                                             // only safe operations seen
}

// emits native code for a numeric loop (with optional table access)
bool x86_64_emit_numeric_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                               JitLoopInfo* info, int step_sign, void** out_fn) {
    int entry = info->entry_pc;                              // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry
    int nregs = info->nregs;                                 // function frame size
    int extra_slots = (info->kind == JIT_LOOP_NUMERIC_FOR) ? 1 : 0;  // iterator temp slot
    int const_slot = nregs + extra_slots;                    // reserved slot for constant 1.0

    bool needs_helper = false;                               // does the body call a runtime helper?
    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &ctx->chunk->code[pc];
        switch (inst->opcode) {
            case OP_TABLE_GET_KEY_STR:                       // fused string-key op: helper
            case OP_TABLE_SET_KEY_STR:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:  // user calls clobber frame_reg
                needs_helper = true;
                break;
            case OP_TABLE_GET: case OP_TABLE_SET:
            case OP_TABLE_GET_NUM: case OP_TABLE_SET_NUM: {
                int key_reg  = (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM)
                             ? inst->operands[2] : inst->operands[1];
                int tbl_reg  = (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM)
                             ? inst->operands[1] : inst->operands[0];
                bool key_is_str = (key_reg >= 0 && key_reg < 64) &&
                                  ((info->str_slots >> key_reg) & 1ULL);
                bool is_fast = (key_reg == info->for_var_reg) &&
                               (tbl_reg == info->table.slot) &&
                               !key_is_str;
                if (!is_fast) needs_helper = true;
                break;
            }
            default: break;
        }
        if (needs_helper) break;
    }

    // precompute distinct immediates used by ADD_IMM/SUB_IMM/MUL_IMM/DIV_IMM/MOD_IMM
    info->n_imms = 0;
    info->imm_opt_ok = true;
    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &ctx->chunk->code[pc];
        int32_t imm;
        switch (inst->opcode) {
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
                imm = inst->operands[2];
                break;
            default: continue;
        }
        bool found = false;
        for (int i = 0; i < info->n_imms; i++)
            if (info->imms[i].value == imm) { found = true; break; }
        if (found) continue;
        if (info->n_imms >= JIT_MAX_IMMS) { info->imm_opt_ok = false; info->n_imms = 0; break; }
        info->imms[info->n_imms++].value = imm;
    }

    int imm_base_slot = const_slot + 1;                      // first slot for precomputed immediates
    for (int i = 0; i < info->n_imms; i++) info->imms[i].slot = imm_base_slot + i;

    int save_slot = -1;                                      // stack slot to stash frame_reg across helper call
    int frame_slots = const_slot + 1 + info->n_imms;         // incl. constant slot + immediates
    uint64_t writeback_mask = info->live_out & ~info->ref_writes;  // slots whose old value must be released
    if (needs_helper || writeback_mask != 0) {
        save_slot = frame_slots;                             // reserve one more slot
        frame_slots++;
    }

    int gpr_saves_bytes = (info->table.used || info->globals_count > 0) ? 8 : 0;  // rbx save if table or globals access

    int range_size = back_edge - entry + 1;
    if (range_size <= 0) return false;                       // empty range, nothing to emit

    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;  // buffer too small

    if (!loop_is_safe_to_emit(ctx, info)) return false;      // needs runtime checks the emitter lacks

    // scan body to decide if the iterator can stay in xmm15 across iterations
    bool iter_in_xmm = false;
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {                // only numeric-for has iterator
        bool clobbers = false;
        for (int pc = entry + 1; pc < back_edge; pc++) {
            Opcode op = ctx->chunk->code[pc].opcode;
            // imm ops stay off XMM_SCRATCH only when every immediate was precomputed;
            bool op_is_imm = (op == OP_ADD_IMM || op == OP_SUB_IMM ||
                              op == OP_MUL_IMM || op == OP_DIV_IMM ||
                              op == OP_MOD_IMM);
            if (op_is_imm) {
                if (info->imm_opt_ok) continue;              // memory-operand form, scratch-free
                clobbers = true;                             // imm load clobbers XMM_SCRATCH
                break;
            }
            if (op == OP_MOD || op == OP_NEG ||              // use XMM_SCRATCH
                op == OP_TABLE_GET_KEY_STR ||                // helper call clobbers xmm
                op == OP_TABLE_SET_KEY_STR ||                // helper call clobbers xmm
                op == OP_TABLE_SET ||                        // general path calls table_set_int
                op == OP_TABLE_SET_NUM ||                    // general path calls table_set_int
                op == OP_NEW_TABLE ||                        // helper call clobbers xmm
                op == OP_CALL_0 || op == OP_CALL_1 ||        // calls clobber all xmm regs
                op == OP_CALL_2) {
                clobbers = true;
                break;                                       // iterator must live in memory
            }
        }
        iter_in_xmm = !clobbers;
    }

    // fixpoint pass: emit into scratch until the cache state stabilizes
    uint8_t* scratch_buf = ctx->scratch_code_buf;            // shared scratch buffer
    size_t scratch_size  = ctx->scratch_code_buf_cap;
    if (!scratch_buf || scratch_size < (size_t)range_size * 256 + 1024) return false;  // scratch too small
    CodeBuf scratch = { scratch_buf, 0, scratch_size };

    XmmCache cache_start;
    x86_cache_clear(&cache_start);                           // start from empty cache
    bool converged = false;
    bool flush_at_back_edge = false;                         // fallback policy when the fixpoint oscillates
    for (int iter = 0; iter < 8; iter++) {                   // fixpoint loop
        XmmCache cache = cache_start;
        scratch.len = 0;
        size_t patch = emit_loop_iteration(abi, ctx, &scratch, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign, needs_helper);
        if (patch == (size_t)-1) return false;               // emit failed
        if (flush_at_back_edge) x86_cache_clear(&cache);     // simulate the spill before the back edge
        if (x86_cache_eq(&cache, &cache_start)) { converged = true; break; }  // stable state reached
        cache_start = cache;                                 // try again with new state
    }
    if (!converged) {
        x86_cache_clear(&cache_start);                       // empty incoming state
        flush_at_back_edge = true;                           // force a spill before the back edge
        converged = true;                                    // empty start + flush at back edge is stable
    }

    // real emit
    size_t mark = cb->len;                                   // rollback point

    int base_frame = align16(8 * frame_slots);
    int frame_size = align16(base_frame + abi->frame_extra + gpr_saves_bytes);

    emit_prologue(cb, abi, frame_size, base_frame);

    // save rbx if we touch tables, and preload array_part into rbx
    int rbx_slot_off = base_frame + abi->frame_extra + 8;
    if (info->table.used && info->table.slot >= 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);  // save caller's rbx
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, info->table.slot * 8);  // rax = regs[slot]
        x86_emit_clear_high16_rax(cb);                       // strip nan-box tag
        x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));  // rbx = array_part
    } else if (info->globals_count > 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);  // save caller's rbx
        emit_u8(cb, 0x48); emit_u8(cb, 0x89);                // mov rbx, abi->globals_reg
        emit_u8(cb, 0xC0 | (abi->globals_reg << 3) | X86_RBX);
    }

    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);          // rax = bits of 1.0
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                  // xmm7 = 1.0
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));  // const_slot = 1.0

    // store each precomputed immediate into its slot
    for (int i = 0; i < info->n_imms; i++) {
        x86_emit_load_double_imm(cb, XMM_SCRATCH, info->imms[i].value);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->imms[i].slot));
    }

    // copy live-in AND live-out slots from vm regs to stack frame
    uint64_t m = info->live_in | info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);                          // next slot to seed
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);  // xmm0 = regs[s]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));       // frame[s] = xmm0
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && !iter_in_xmm) {  // seed iterator in memory
        int var_reg = ctx->chunk->code[entry].operands[0];
        x86_emit_movsd_load(cb, 0, x86_slot_disp(var_reg));  // xmm0 = R[var]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(nregs));   // frame[nregs] = xmm0
    }

    // preload the fixpoint cache state
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = cache_start.reg_slot[i];
        if (s >= 0) x86_emit_movsd_load(cb, i, x86_slot_disp(s));  // xmm<i> = frame[s]
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && iter_in_xmm) {  // load iterator to xmm15
        int var_reg = ctx->chunk->code[entry].operands[0];
        x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(var_reg));  // xmm15 = R[var]
    }

    int loop_top = (int)cb->len;                             // loop start address
    XmmCache cache = cache_start;
    size_t entry_patch = emit_loop_iteration(abi, ctx, cb, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign, needs_helper);
    if (entry_patch == (size_t)-1) { cb->len = mark; return false; }  // emit failed, rollback

    if (flush_at_back_edge) x86_cache_flush(&cache, cb);     // spill all before back edge
    emit_u8(cb, 0xE9);                                       // jmp loop_top
    size_t back_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder
    int32_t back_rel = loop_top - (int32_t)(back_patch + 4);
    memcpy(cb->buf + back_patch, &back_rel, 4);              // patch back edge

    int exit_label = (int)cb->len;                           // exit target
    int32_t exit_rel = exit_label - (int32_t)(entry_patch + 4);
    memcpy(cb->buf + entry_patch, &exit_rel, 4);             // patch exit jump

    x86_cache_flush(&cache, cb);                             // spill dirty slots to frame

    m = info->live_out;                                      // copy written slots back to vm
    while (m) {
        int s = __builtin_ctzll(m);                          // next written slot
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        if (info->ref_writes & (1ULL << s)) continue;        // refcounted value — keep vm's copy

        if (save_slot >= 0) {
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

            // arg0: &pool[s] = frame_reg + s*8
            if (s != 0) {
                emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
                emit_u8(cb, 0xC1);                                 // add rcx, imm32
#else
                emit_u8(cb, 0xC7);                                 // add rdi, imm32
#endif
                emit_i32(cb, s * 8);
            }

            // arg1: frame[s]
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore frame_reg
        } else {
            // no save slot reserved: fall back to plain store (no refcount)
            x86_emit_movsd_load(cb, 0, x86_slot_disp(s));
            x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);
        }
    }

    if ((info->table.used && info->table.slot >= 0) || info->globals_count > 0) {
        x86_emit_load_r64_rbp(cb, X86_RBX, -rbx_slot_off);   // restore caller's rbx
    }

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;                                             // emission successful
}

// emits native code for a table iteration loop (for v in t)
bool x86_64_emit_table_iter_loop(const X86_64Abi* abi, JITContext* ctx,
                                  CodeBuf* cb, JitLoopInfo* info, void** out_fn) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry = info->entry_pc;                              // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry
    int nregs = info->nregs;                                 // function frame size
    int table_slot = info->table.slot;                       // register holding the table
    int var_reg = chunk->code[entry].operands[0];            // loop variable slot

    if (table_slot < 0) return false;                        // no table detected, bail

    int range_size = back_edge - entry + 1;
    if (range_size <= 0) return false;                       // empty range, nothing to emit
    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;  // buffer too small

    size_t mark = cb->len;                                   // rollback point
    int const_slot = nregs;                                  // unused here, kept for layout parity
    int save_slot  = nregs + 1;                              // stack slot to stash frame_reg across helper call
    int frame_slots = save_slot + 1;                         // incl. the unused constant slot and save slot
    int base_frame = align16(8 * frame_slots);

    int saves_bytes = 24;                                    // rbx + r12 + r13
    int frame_size = align16(base_frame + abi->frame_extra + saves_bytes);

    emit_prologue(cb, abi, frame_size, base_frame);

    int off_rbx = base_frame + abi->frame_extra + 8;         // save slot for rbx
    int off_r12 = base_frame + abi->frame_extra + 16;        // save slot for r12
    int off_r13 = base_frame + abi->frame_extra + 24;        // save slot for r13
    x86_emit_store_r64_rbp(cb, X86_RBX, -off_rbx);           // save caller's rbx
    x86_emit_store_r64_rbp(cb, X86_R12, -off_r12);           // save caller's r12
    x86_emit_store_r64_rbp(cb, X86_R13, -off_r13);           // save caller's r13

    // unpack the table pointer and preload its fields into dedicated gprs
    x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, table_slot * 8);  // rax = regs[table_slot]
    x86_emit_clear_high16_rax(cb);                           // strip nan-box tag
    x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));   // rbx = array_part
    x86_emit_load_r64_base(cb, X86_R12, X86_RAX, (int32_t)offsetof(Table, array_count));  // r12 = array_count

    emit_u8(cb, 0x31); emit_u8(cb, 0xD2);                    // xor edx, edx
    // rdx = 0 (iteration counter, caller-saved: no save/restore needed)
    x86_emit_movabs_r8(cb, X86_NONE_BITS);                   // r8 = NONE bits, for hole-skip

    // copy live-in AND live-out slots from vm regs to frame, except table and loop var
    uint64_t m = info->live_in | info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);                          // next slot to seed
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        if (s == table_slot) continue;                       // table lives in rbx, not frame
        if (s == var_reg)   continue;                        // var written each iteration
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);  // xmm0 = regs[s]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));       // frame[s] = xmm0
    }

    int loop_top = (int)cb->len;                             // loop start address

    // if counter >= array_count, exit
    x86_emit_cmp_rdx_r12d(cb);                               // cmp rdx, r12
    emit_u8(cb, 0x0F); emit_u8(cb, 0x83);                    // jae rel32
    size_t exit_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder

    x86_emit_load_rax_idx8(cb, X86_RBX, X86_RDX);            // rax = array_part[rdx]
    // if element is NONE, skip to next iteration
    x86_emit_cmp_rax_r8(cb);                                 // cmp rax, NONE bits
    emit_u8(cb, 0x0F); emit_u8(cb, 0x84);                    // je rel32 (skip)
    size_t skip_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder

    x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(var_reg));  // frame[var_reg] = element

    XmmCache cache;
    x86_cache_clear(&cache);                                 // body starts with empty cache

    int xv = x86_cache_alloc_excl(&cache, cb, -1, -1);       // xmm to hold the element
    if (xv < 0) { cb->len = mark; return false; }            // no free xmm, rollback
    x86_emit_movq_xmm_rax(cb, xv);                           // xv = element bits (as double)
    x86_cache_put(&cache, xv, var_reg);                      // cache var_reg in xv

    for (int pc = entry + 1; pc < back_edge; pc++) {         // emit body
        emit_loop_body_instr(abi, ctx, cb, &cache, pc, const_slot, -1, info);
    }

    x86_cache_flush(&cache, cb);                             // spill dirty slots to frame

    int skip_label = (int)cb->len;                           // target for hole-skip
    int32_t skip_rel = skip_label - (int32_t)(skip_patch + 4);
    memcpy(cb->buf + skip_patch, &skip_rel, 4);              // patch skip jump

    emit_u8(cb, 0x48); emit_u8(cb, 0xFF); emit_u8(cb, 0xC2); // inc rdx
    emit_u8(cb, 0xE9);                                       // jmp rel32
    size_t back_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder
    int32_t back_rel = loop_top - (int32_t)(back_patch + 4);
    memcpy(cb->buf + back_patch, &back_rel, 4);              // patch back edge

    int exit_label = (int)cb->len;                           // exit target
    int32_t exit_rel = exit_label - (int32_t)(exit_patch + 4);
    memcpy(cb->buf + exit_patch, &exit_rel, 4);              // patch exit jump

    m = info->live_out;                                      // copy written slots back to vm
    while (m) {
        int s = __builtin_ctzll(m);                          // next written slot
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        if (s == var_reg) continue;                          // already written by the loop body
        if (info->ref_writes & (1ULL << s)) continue;        // refcounted value — keep vm's copy

        x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

        // arg0: &pool[s] = frame_reg + s*8
        if (s != 0) {
            emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0xC1);                                    // add rcx, imm32
#else
            emit_u8(cb, 0xC7);                                    // add rdi, imm32
#endif
            emit_i32(cb, s * 8);
        }

        // arg1: frame[s]
        x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));
#if defined(_WIN32) || defined(_WIN64)
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

        x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
        emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

        x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore frame_reg
    }

    x86_emit_load_r64_rbp(cb, X86_RBX, -off_rbx);             // restore caller's rbx
    x86_emit_load_r64_rbp(cb, X86_R12, -off_r12);             // restore caller's r12
    x86_emit_load_r64_rbp(cb, X86_R13, -off_r13);             // restore caller's r13

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;                                             // emission successful
}

// emits native code for a condition-entry loop: no implicit entry test, the
bool x86_64_emit_cond_enter_loop(const X86_64Abi* abi, JITContext* ctx,
                                  CodeBuf* cb, JitLoopInfo* info, void** out_fn) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry     = info->entry_pc;                          // first body pc
    int back_edge = info->back_edge_pc;                      // JUMP back to entry
    int nregs     = info->nregs;                             // function frame size
    int const_slot = nregs;                                  // reserved slot for constant 1.0

    if (back_edge <= entry) return false;                    // empty body
    int range_size = back_edge - entry + 1;
    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;  // buffer too small

    if (!loop_is_safe_to_emit(ctx, info)) return false;      // needs checks the emitter lacks

    // precompute distinct immediates used by ADD_IMM/SUB_IMM/MUL_IMM/DIV_IMM/MOD_IMM
    info->n_imms = 0;
    info->imm_opt_ok = true;
    for (int pc = entry; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        int32_t imm;
        switch (inst->opcode) {
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
                imm = inst->operands[2];
                break;
            default: continue;
        }
        bool found = false;
        for (int i = 0; i < info->n_imms; i++)
            if (info->imms[i].value == imm) { found = true; break; }
        if (found) continue;
        if (info->n_imms >= JIT_MAX_IMMS) { info->imm_opt_ok = false; info->n_imms = 0; break; }
        info->imms[info->n_imms++].value = imm;
    }
    int imm_base_slot = const_slot + 1;
    for (int i = 0; i < info->n_imms; i++) info->imms[i].slot = imm_base_slot + i;

    // scan for helper calls (same rule as the numeric emitter)
    bool needs_helper = false;
    for (int pc = entry; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_TABLE_GET_KEY_STR:
            case OP_TABLE_SET_KEY_STR:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
                needs_helper = true;
                break;
            case OP_TABLE_GET: case OP_TABLE_SET:
            case OP_TABLE_GET_NUM: case OP_TABLE_SET_NUM: {
                int key_reg, tbl_reg;
                if (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM) {
                    tbl_reg = inst->operands[1]; key_reg = inst->operands[2];
                } else {
                    tbl_reg = inst->operands[0]; key_reg = inst->operands[1];
                }
                bool is_fast = (key_reg == info->for_var_reg) &&
                               (tbl_reg == info->table.slot);
                if (!is_fast) needs_helper = true;
                break;
            }
            default: break;
        }
        if (needs_helper) break;
    }

    int save_slot = -1;                                      // stack slot to stash frame_reg across helper call
    int frame_slots = const_slot + 1 + info->n_imms;
    uint64_t writeback_mask = info->live_out & ~info->ref_writes;
    if (needs_helper || writeback_mask != 0) {
        save_slot = frame_slots;                             // reserve one more slot
        frame_slots++;
    }

    int gpr_saves_bytes = (info->table.used || info->globals_count > 0) ? 8 : 0;  // rbx save if table or globals access

    size_t mark = cb->len;                                   // rollback point

    int base_frame = align16(8 * frame_slots);
    int frame_size = align16(base_frame + abi->frame_extra + gpr_saves_bytes);

    emit_prologue(cb, abi, frame_size, base_frame);

    // save rbx if we touch tables or globals, and preload the relevant base
    int rbx_slot_off = base_frame + abi->frame_extra + 8;
    if (info->table.used && info->table.slot >= 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, info->table.slot * 8);
        x86_emit_clear_high16_rax(cb);
        x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));
    } else if (info->globals_count > 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);
        emit_u8(cb, 0x48); emit_u8(cb, 0x89);
        emit_u8(cb, 0xC0 | (abi->globals_reg << 3) | X86_RBX);
    }

    // materialize the 1.0 constant and any precomputed immediates
    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));
    for (int i = 0; i < info->n_imms; i++) {
        x86_emit_load_double_imm(cb, XMM_SCRATCH, info->imms[i].value);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->imms[i].slot));
    }

    // seed live-in AND live-out slots from the vm's register frame
    uint64_t m = info->live_in | info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));
    }

    XmmCache cache;
    x86_cache_clear(&cache);

    // local label bookkeeping borrowed from the shared scratch pool
    int32_t* label_off = ctx->scratch_label_off;
    JumpFixup* fixups  = ctx->scratch_fixups;
    if (!label_off || !fixups) { cb->len = mark; return false; }
    for (int i = 0; i <= range_size; i++) label_off[i] = -1;
    int nfix = 0;

    int loop_top = (int)cb->len;                             // loop start address

    for (int pc = entry; pc < back_edge; pc++) {
        label_off[pc - entry] = (int32_t)cb->len;
        Instruction* inst = &chunk->code[pc];

        // fused emit: CMP_* d,a,b directly followed by JUMP_IF_FALSE d
        if (pc + 1 < back_edge &&
            chunk->code[pc + 1].opcode == OP_JUMP_IF_FALSE &&
            chunk->code[pc + 1].operands[1] == inst->operands[0]) {
            uint8_t fused_jcc = 0;                               // inverted jcc for "jump on false"
            switch (inst->opcode) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  fused_jcc = 0x85; break;  // jne
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: fused_jcc = 0x84; break;  // je
                case OP_CMP_LT:  fused_jcc = 0x83; break;                       // jae
                case OP_CMP_GT:  fused_jcc = 0x86; break;                       // jbe
                case OP_CMP_LTE: fused_jcc = 0x87; break;                       // ja
                case OP_CMP_GTE: fused_jcc = 0x82; break;                       // jb
                default: break;
            }
            bool pc1_is_target = false;                          // is pc+1 already a jump target?
            if (fused_jcc) {
                for (int q = entry; q < pc + 1; q++) {
                    if (chunk->code[q].opcode == OP_JUMP_IF_FALSE &&
                        chunk->code[q].operands[0] == pc + 1) { pc1_is_target = true; break; }
                }
            }
            int fuse_target = fused_jcc ? chunk->code[pc + 1].operands[0] : -1;
            bool d_dead = fused_jcc && !pc1_is_target && fuse_target >= 0 &&
                          !slot_read_before_write(chunk, pc + 2, back_edge, inst->operands[0]) &&
                          (fuse_target < pc + 2 ||
                           !slot_read_before_write(chunk, fuse_target, back_edge, inst->operands[0]));
            if (d_dead) {
                int a = inst->operands[1];
                int b = inst->operands[2];
                int xa = x86_cache_load(&cache, cb, a);          // load left operand
                if (xa < 0) { cb->len = mark; return false; }
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load right, avoid xa
                if (xb < 0) { cb->len = mark; return false; }
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                x86_cache_flush(&cache, cb);                     // target may be a merge point
                emit_u8(cb, 0x0F); emit_u8(cb, fused_jcc);       // jcc rel32
                if (nfix >= range_size) { cb->len = mark; return false; }
                fixups[nfix].patch_at  = cb->len;                // record placeholder
                fixups[nfix].target_pc = fuse_target;            // remember target pc
                nfix++;                                          // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                pc++;                                            // skip the JUMP_IF_FALSE
                continue;                                        // skip the normal dispatch
            }
        }

        if (inst->opcode == OP_JUMP_IF_FALSE) {
            int cond_reg = inst->operands[1];                // condition register
            int tgt      = inst->operands[0];                // forward target
            int xa = x86_cache_load(&cache, cb, cond_reg);
            if (xa < 0) { cb->len = mark; return false; }
            x86_emit_movq_rax_xmm(cb, xa);                   // rax = raw 64-bit slot
            emit_u8(cb, 0xA8); emit_u8(cb, 0x01);            // test al, 1
            // local skip over a single CMP writing the same register: the
            bool local_skip = (tgt == pc + 2) &&
                              (chunk->code[pc + 1].operands[0] == cond_reg) &&
                              (chunk->code[pc + 1].opcode == OP_CMP_EQ ||
                               chunk->code[pc + 1].opcode == OP_CMP_EQ_NUM ||
                               chunk->code[pc + 1].opcode == OP_CMP_NEQ ||
                               chunk->code[pc + 1].opcode == OP_CMP_NEQ_NUM ||
                               chunk->code[pc + 1].opcode == OP_CMP_LT ||
                               chunk->code[pc + 1].opcode == OP_CMP_GT ||
                               chunk->code[pc + 1].opcode == OP_CMP_LTE ||
                               chunk->code[pc + 1].opcode == OP_CMP_GTE);
            if (!local_skip) x86_cache_flush(&cache, cb);    // flush before branch
            emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je rel32 (bit0 == 0 -> false)
            if (nfix >= range_size) { cb->len = mark; return false; }
            fixups[nfix].patch_at  = cb->len;
            fixups[nfix].target_pc = tgt;                    // exit vs internal decided at patch time
            nfix++;
            emit_i32(cb, 0);
        } else {
            emit_loop_body_instr(abi, ctx, cb, &cache, pc, const_slot, save_slot, info);
        }
    }
    label_off[back_edge - entry] = (int32_t)cb->len;         // label for the back-edge pc

    x86_cache_flush(&cache, cb);                             // spill any remaining dirty slots

    // back edge jump
    emit_u8(cb, 0xE9);
    int32_t back_rel = loop_top - (int32_t)(cb->len + 4);
    emit_i32(cb, back_rel);

    int exit_label = (int)cb->len;                           // exit target

    // patch every JUMP_IF_FALSE: exit -> exit_label, internal -> local label
    for (int i = 0; i < nfix; i++) {
        int tgt = fixups[i].target_pc;
        int32_t rel;
        if (tgt == info->exit_pc) {
            rel = exit_label - (int32_t)(fixups[i].patch_at + 4);
        } else {
            int idx = tgt - entry;
            if (idx < 0 || idx > range_size || label_off[idx] < 0) {
                cb->len = mark; return false;
            }
            rel = label_off[idx] - (int32_t)(fixups[i].patch_at + 4);
        }
        memcpy(cb->buf + fixups[i].patch_at, &rel, 4);
    }

    // write back live_out slots to the vm's registers
    m = info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        if (info->ref_writes & (1ULL << s)) continue;        // refcounted value — keep vm's copy

        if (save_slot >= 0) {
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));

            // arg0: &pool[s] = frame_reg + s*8
            if (s != 0) {
                emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
                emit_u8(cb, 0xC1);                            // add rcx, imm32
#else
                emit_u8(cb, 0xC7);                            // add rdi, imm32
#endif
                emit_i32(cb, s * 8);
            }

            // arg1: frame[s]
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));
        } else {
            x86_emit_movsd_load(cb, 0, x86_slot_disp(s));
            x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);
        }
    }

    if ((info->table.used && info->table.slot >= 0) || info->globals_count > 0) {
        x86_emit_load_r64_rbp(cb, X86_RBX, -rbx_slot_off);   // restore caller's rbx
    }

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;
}

// emits native code for a single native loop, dispatching by kind
bool x86_64_emit_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                      JitLoopInfo* info, int step_sign, void** out_fn) {
    if (info->kind == JIT_LOOP_TABLE_ITER) {                 // table iteration loop
        return x86_64_emit_table_iter_loop(abi, ctx, cb, info, out_fn);
    }
    if (info->kind == JIT_LOOP_COND_ENTER) {                 // condition-entry loop
        return x86_64_emit_cond_enter_loop(abi, ctx, cb, info, out_fn);
    }
    if (matches_int_accum_loop(ctx, info)) {                 // int64-specializable loop
        return x86_64_emit_int_loop(abi, ctx, cb, info, out_fn);
    }
    return x86_64_emit_numeric_loop(abi, ctx, cb, info, step_sign, out_fn);  // numeric or condition loop
}
// source/jit/x86-64/x86_64_emit_move.c
// Move/load emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"

// emits OP_MOVE / OP_LOAD_* for a function body instruction
void x86_func_emit_move(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache

    switch (st->op) {
        case OP_MOVE: {                                              // reg-to-reg copy
            int xa = x86_cache_load(cache, cb, st->a);               // find xmm for source
            if (xa < 0) JIT_FATAL("cache full: MOVE src=%d (func=%d pc=%d)",
                                  st->a, st->func_idx, st->pc);
            x86_cache_put(cache, xa, st->d);                         // relabel xmm as dest
            break;
        }
        case OP_LOAD_NUM_IMM: {                                      // small int literal
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);         // pick a register
            if (x < 0) JIT_FATAL("cache full: LOAD_NUM_IMM dst=%d (func=%d pc=%d)",
                                  st->d, st->func_idx, st->pc);
            int imm_idx = -1;                                        // index into the rip pool
            for (int i = 0; i < st->func_n_imms; i++)
                if (st->func_imms[i] == st->a) { imm_idx = i; break; }
            if (imm_idx >= 0) {                                      // pooled: rip-relative load
                size_t at = x86_emit_movsd_load_rip(cb, x);
                x86_func_rip_fixup(st, at, imm_idx);
            } else {
                x86_emit_load_double_imm(cb, x, st->a);              // not pooled: build from imm
            }
            x86_cache_put(cache, x, st->d);                          // cache dest
            break;
        }
        case OP_LOAD_NUM: {                                          // full double constant
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_NUM dst=%d (func=%d pc=%d)",
                                  st->d, st->func_idx, st->pc);
            double v = st->chunk->constants[st->a].number_value;     // fetch constant
            uint64_t bits; memcpy(&bits, &v, 8);                     // reinterpret as u64
            x86_emit_movabs_rax(cb, bits);                           // rax = bit pattern
            x86_emit_movq_xmm_rax(cb, x);                            // xmmX = rax
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_BOOL: {                                         // boolean literal
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_BOOL dst=%d (func=%d pc=%d)",
                                  st->d, st->func_idx, st->pc);
            uint64_t bits = X86_BOOL_BITS | (st->a ? 1ULL : 0ULL);   // bit 0 carries the value
            x86_emit_movabs_rax(cb, bits);                           // rax = nan-boxed bool
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_NONE: {                                         // none literal
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_NONE dst=%d (func=%d pc=%d)",
                                  st->d, st->func_idx, st->pc);
            x86_emit_movabs_rax(cb, X86_NONE_BITS);                  // rax = NONE bit pattern
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_CONST: {                                        // scalar constant from the pool
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_CONST dst=%d (func=%d pc=%d)",
                                  st->d, st->func_idx, st->pc);
            Constant* c = &st->chunk->constants[st->a];              // fetch constant entry
            uint64_t bits;                                           // nan-boxed bit pattern
            switch (c->type) {
                case CONST_NUMBER: {                                 // numeric constant
                    double v = c->number_value;
                    memcpy(&bits, &v, 8);
                    break;
                }
                case CONST_BOOL:                                     // bool constant
                    bits = X86_BOOL_BITS | (c->bool_value ? 1ULL : 0ULL);
                    break;
                case CONST_NONE:                                     // none constant
                    bits = X86_NONE_BITS;
                    break;
                case CONST_STRING:                                   // interned string pointer
                    bits = MAKE_STRING((StringObject*)c->cached_str);
                    break;
                default:
                    JIT_FATAL("LOAD_CONST with unsupported type %d (func=%d pc=%d)",
                              c->type, st->func_idx, st->pc);
            }
            x86_emit_movabs_rax(cb, bits);                           // rax = nan-boxed payload
            x86_emit_movq_xmm_rax(cb, x);                            // xmmX = rax
            x86_cache_put(cache, x, st->d);
            break;
        }
        default:
            JIT_FATAL("move category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
}

// emits OP_MOVE / OP_LOAD_* / OP_LOAD_GLOBAL / OP_STORE_GLOBAL inside a loop body
void x86_loop_emit_move(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    BytecodeChunk* chunk = st->chunk;                                // bytecode being compiled

    switch (st->op) {
        case OP_MOVE: {                                              // reg-to-reg copy
            if (st->d == st->a) break;                               // self-move is a no-op
            int xa = x86_cache_load(cache, cb, st->a);               // load source slot
            if (xa < 0) JIT_FATAL("cache full: MOVE src=%d (pc=%d)", st->a, st->pc);
            if (!slot_live_in_loop(chunk, st->info->entry_pc,        // source dead after this?
                                   st->info->back_edge_pc, st->pc, st->a)) {
                x86_cache_put(cache, xa, st->d);                     // relabel, a is dead
            } else {
                int xd = x86_cache_dest_reg(cache, cb, st->d, xa, -1);  // separate dest register
                if (xd < 0) JIT_FATAL("cache full: MOVE dst=%d src=%d (pc=%d)",
                                       st->d, st->a, st->pc);
                if (xd != xa) x86_emit_sse66_rr(cb, 0x28, xd, xa);   // movapd xd, xa
                x86_cache_put(cache, xd, st->d);                     // a survives in xa
            }
            break;
        }
        case OP_LOAD_NUM_IMM: {                                      // small int literal
            int x = x86_cache_dest_reg(cache, cb, st->d, -1, -1);    // prefer d's existing home
            if (x < 0) JIT_FATAL("cache full: LOAD_NUM_IMM dst=%d (pc=%d)", st->d, st->pc);
            x86_emit_load_double_imm(cb, x, st->a);                  // xmmX = (double)a
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_NUM: {                                          // full double constant
            int x = x86_cache_dest_reg(cache, cb, st->d, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_NUM dst=%d (pc=%d)", st->d, st->pc);
            double v = chunk->constants[st->a].number_value;         // fetch pool constant
            uint64_t bits; memcpy(&bits, &v, 8);                     // reinterpret as u64
            x86_emit_movabs_rax(cb, bits);
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_BOOL: {                                         // boolean literal
            int x = x86_cache_dest_reg(cache, cb, st->d, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_BOOL dst=%d (pc=%d)", st->d, st->pc);
            uint64_t bits = X86_BOOL_BITS | (st->a ? 1ULL : 0ULL);   // bit 0 carries value
            x86_emit_movabs_rax(cb, bits);
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_NONE: {                                         // none literal
            int x = x86_cache_dest_reg(cache, cb, st->d, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_NONE dst=%d (pc=%d)", st->d, st->pc);
            x86_emit_movabs_rax(cb, X86_NONE_BITS);
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_CONST: {                                        // scalar constant (num/bool/none/str)
            int x = x86_cache_dest_reg(cache, cb, st->d, -1, -1);
            if (x < 0) JIT_FATAL("cache full: LOAD_CONST dst=%d (pc=%d)", st->d, st->pc);
            Constant* c = &chunk->constants[st->a];
            uint64_t bits;
            switch (c->type) {
                case CONST_NUMBER: {
                    double v = c->number_value;
                    memcpy(&bits, &v, 8);
                    break;
                }
                case CONST_BOOL:
                    bits = X86_BOOL_BITS | (c->bool_value ? 1ULL : 0ULL);
                    break;
                case CONST_NONE:
                    bits = X86_NONE_BITS;
                    break;
                case CONST_STRING:
                    bits = MAKE_STRING((StringObject*)c->cached_str);
                    break;
                default:
                    JIT_FATAL("LOAD_CONST unsupported type %d (pc=%d)", c->type, st->pc);
            }
            x86_emit_movabs_rax(cb, bits);
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_LOAD_GLOBAL: {                                       // read vm->globals[idx]
            int idx = st->a;                                         // global index
            int x = x86_cache_dest_reg(cache, cb, st->d, -1, -1);    // prefer d's existing home
            if (x < 0) JIT_FATAL("cache full: LOAD_GLOBAL dst=%d (pc=%d)", st->d, st->pc);
            x86_emit_movsd_load_base(cb, X86_RBX, x, idx * 8);       // rbx = globals base
            x86_cache_put(cache, x, st->d);
            break;
        }
        case OP_STORE_GLOBAL: {                                      // write vm->globals[idx]
            int idx = st->a;                                         // global index
            int xv = x86_cache_load(cache, cb, st->d);               // load source slot
            if (xv < 0) JIT_FATAL("cache full: STORE_GLOBAL src=%d (pc=%d)", st->d, st->pc);
            x86_emit_movsd_store_base(cb, X86_RBX, xv, idx * 8);     // globals[idx] = xv
            break;
        }
        default:
            JIT_FATAL("loop move category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
}
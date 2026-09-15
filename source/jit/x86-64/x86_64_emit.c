// source/jit/x86-64/x86_64_emit.c
// Implementation of x86-64 function and loop emitters for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_common.h"
#include "x86_64_abi.h"
#include "vm.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// rounds n up to the next multiple of 16
static int align16(int n) { return (n + 15) & ~15; }

// emits the shared prologue then any abi-specific callee-saved register saves=
static void emit_prologue(CodeBuf* cb, const X86_64Abi* abi,
                          int frame_size, int base_frame) {
    emit_u8(cb, 0x55);                                       // push rbp
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xE5); // mov rbp, rsp
    emit_u8(cb, 0x48); emit_u8(cb, 0x81); emit_u8(cb, 0xEC); // sub rsp, imm32
    emit_i32(cb, frame_size);                                // frame size immediate
    if (abi->emit_prologue_saves) abi->emit_prologue_saves(cb, base_frame);
}

// emits abi-specific restores, then leave; ret
static void emit_leave_ret(CodeBuf* cb, const X86_64Abi* abi, int base_frame) {
    if (abi->emit_epilogue_restores) abi->emit_epilogue_restores(cb, base_frame);
    emit_u8(cb, 0xC9);                                       // leave
    emit_u8(cb, 0xC3);                                       // ret
}

// fallback epilogue: returns xmm0 = 0.0
static void emit_return_zero(const X86_64Abi* abi, CodeBuf* cb, int base_frame) {
    emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
    emit_u8(cb, 0x57); emit_u8(cb, 0xC0);                    // xorpd xmm0, xmm0
    emit_leave_ret(cb, abi, base_frame);
}

// emits indirect call through func_table[func_idx] (args in xmm0/xmm1, result in xmm0)
static void emit_call_func(CodeBuf* cb, JITContext* ctx, int func_idx) {
    uint64_t slot_addr = (uint64_t)(uintptr_t)&ctx->func_table[func_idx];  // address of the slot
    x86_emit_movabs_rax(cb, slot_addr);                      // rax = &func_table[idx]
    emit_u8(cb, 0x48); emit_u8(cb, 0x8B); emit_u8(cb, 0x00); // rax = *rax
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                    // call rax
}

// returns the jcc opcode that exits the loop when the entry condition is met
static uint8_t jcc_for_entry_op(Opcode op) {
    switch (op) {
        case OP_JUMP_IF_EQ:     case OP_JUMP_IF_EQ_NUM:  return 0x84;  // je
        case OP_JUMP_IF_NEQ:    case OP_JUMP_IF_NEQ_NUM: return 0x85;  // jne
        case OP_JUMP_IF_LT:  return 0x82;  // jb
        case OP_JUMP_IF_GT:  return 0x87;  // ja
        case OP_JUMP_IF_LTE: return 0x86;  // jbe
        case OP_JUMP_IF_GTE: return 0x83;  // jae
        default:             return 0x84;
    }
}

// checks if slot s is read between pc_after and the first write to s (inclusive scan, stops at write)
static bool slot_read_before_write(BytecodeChunk* chunk, int pc_after, int end, int s) {
    for (int pc = pc_after; pc < end; pc++) {                    // scan forward
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // destination register
        int a = inst->operands[1];                               // first source register
        int b = inst->operands[2];                               // second source register

        bool reads = false;                                      // does this read s?
        switch (inst->opcode) {
            case OP_MOVE: case OP_NEG:                           // reads a
                reads = (a == s);
                break;
            case OP_INC: case OP_DEC:                            // reads d
                reads = (d == s);
                break;
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_CMP_EQ: case OP_CMP_NEQ:
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:            // reads a, b
                reads = (a == s || b == s);
                break;
            case OP_JUMP_IF_FALSE:                               // reads a
                reads = (a == s);
                break;
            case OP_RETURN: case OP_RETURN_NUM:                  // reads d
            case OP_RETURN_BOOL:                                 // incl. boolean
                reads = (d == s);
                break;
            case OP_CALL_1:                                      // arg in b
                reads = (b == s);
                break;
            case OP_CALL_2:                                      // args in b, b+1
                reads = (b == s || b + 1 == s);
                break;
            default:                                             // no register reads
                break;
        }
        if (reads) return true;                                  // slot live — read before write

        bool writes = false;                                     // does this write s?
        switch (inst->opcode) {
            case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_CMP_EQ: case OP_CMP_NEQ:
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:      // writes d
                writes = (d == s);
                break;
            default:                                             // no register writes
                break;
        }
        if (writes) return false;                                // overwritten before read — dead
    }
    return false;                                                // never read in range — dead
}

// rewrites xmm_d to the NONE bit pattern on NaN; matches interpreter semantics
static void emit_nan_check(CodeBuf* cb, int xmm_d) {
    emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
    emit_u8(cb, 0x2E); emit_u8(cb, 0xC0 | (xmm_d << 3) | xmm_d);  // ucomisd xmm_d, xmm_d
    emit_u8(cb, 0x0F); emit_u8(cb, 0x8B);                          // jnp +rel32
    size_t patch_at = cb->len;                                     // record jump placeholder
    emit_i32(cb, 0);
    x86_emit_movabs_rax(cb, X86_NONE_BITS);                        // rax = NONE bits
    x86_emit_movq_xmm_rax(cb, xmm_d);                              // xmm_d = NONE bits
    int32_t rel = (int32_t)cb->len - (int32_t)(patch_at + 4);      // rel32 to skip
    memcpy(cb->buf + patch_at, &rel, 4);
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

    if (cb->len + (size_t)range_size * 256 + 512 > cb->cap)      // conservative size check
        return false;

    bool* is_target     = ctx->scratch_is_target;                // shared jump-target marks
    int32_t* label_off  = ctx->scratch_label_off;                // shared label offset array
    JumpFixup* fixups   = ctx->scratch_fixups;                   // shared jump fixup array
    if (!is_target || !label_off || !fixups) return false;       // scratch not available

    memset(is_target, 0, range_size * sizeof(bool));             // clear jump-target marks
    for (int pc = start; pc < end; pc++) {                       // collect jump targets
        Opcode op = chunk->code[pc].opcode;
        if (op == OP_JUMP || op == OP_JUMP_IF_FALSE ||
            op == OP_JUMP_IF_EQ || op == OP_JUMP_IF_NEQ ||
            op == OP_JUMP_IF_EQ_NUM || op == OP_JUMP_IF_NEQ_NUM ||
            op == OP_JUMP_IF_LT || op == OP_JUMP_IF_GT ||
            op == OP_JUMP_IF_LTE || op == OP_JUMP_IF_GTE) {
            int t = chunk->code[pc].operands[0];                 // jump target
            if (t >= start && t < end) is_target[t - start] = true;  // mark as merge point
        }
    }

    for (int i = 0; i < range_size; i++) label_off[i] = -1;      // no label emitted yet
    int fixup_count = 0;                                         // pending jumps count

    size_t mark = cb->len;                                       // rollback point

    XmmCache cache;                                              // register cache state
    x86_cache_clear(&cache);                                     // start empty

    int base_frame  = align16(8 * nregs);                        // aligned slot region
    int frame_size  = base_frame + abi->frame_extra;             // plus abi extras (shadow space, saves)

    emit_prologue(cb, abi, frame_size, base_frame);

    int cached_args = arity < XMM_CACHE_REGS ? arity : XMM_CACHE_REGS;  // args fit in cache
    for (int i = 0; i < cached_args; i++) {                      // cache incoming args
        x86_cache_put(&cache, i, i);                             // xmm<i> = slot i, dirty
    }
    for (int i = cached_args; i < arity; i++) {                  // spill extras
        x86_emit_movsd_store(cb, i, x86_slot_disp(i));           // xmm<i> -> slot i
    }

    for (int pc = start; pc < end; pc++) {                       // emit each bytecode
        label_off[pc - start] = (int32_t)cb->len;                // record label address

        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // first operand
        int a = inst->operands[1];                               // second operand
        int b = inst->operands[2];                               // third operand
        Opcode op = inst->opcode;                                // opcode shorthand
        bool did_flush = false;                                  // cache flushed this step

        bool prefer_xmm0 = false;                                // next op returns this dest?
        if (pc + 1 < end) {                                      // next instruction exists
            Instruction* nx = &chunk->code[pc + 1];
            if ((nx->opcode == OP_RETURN || nx->opcode == OP_RETURN_NUM ||
                 nx->opcode == OP_RETURN_BOOL) &&
                nx->operands[0] == d) {                          // next returns our dest
                prefer_xmm0 = true;                              // prefer xmm0 as result
            }
        }

        switch (op) {
            case OP_MOVE: {                                      // reg-to-reg copy
                int xa = x86_cache_load(&cache, cb, a);          // find xmm for source
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_cache_put(&cache, xa, d);                    // relabel xmm as dest
                break;
            }
            case OP_LOAD_NUM_IMM: {                              // small int literal
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                double v = (double)a;                            // operand as double
                uint64_t bits; memcpy(&bits, &v, 8);             // reinterpret as u64
                x86_emit_movabs_rax(cb, bits);                   // rax = bit pattern
                x86_emit_movq_xmm_rax(cb, x);                    // xmmX = rax
                x86_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_LOAD_NUM: {                                  // full double constant
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                double v = chunk->constants[a].number_value;     // fetch constant
                uint64_t bits; memcpy(&bits, &v, 8);             // reinterpret as u64
                x86_emit_movabs_rax(cb, bits);                   // rax = bit pattern
                x86_emit_movq_xmm_rax(cb, x);                    // xmmX = rax
                x86_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV: {                                       // binary arithmetic
                int xa = x86_cache_load(&cache, cb, a);          // load left operand
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load right, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                uint8_t arith_op;                                // sse opcode
                switch (op) {
                    case OP_ADD: arith_op = 0x58; break;         // addsd
                    case OP_SUB: arith_op = 0x5C; break;         // subsd
                    case OP_MUL: arith_op = 0x59; break;         // mulsd
                    default:     arith_op = 0x5E; break;         // divsd
                }
                bool commutative = (op == OP_ADD || op == OP_MUL);  // swap-safe op?
                if (prefer_xmm0 && commutative && xb == 0 && xa != 0) {
                    x86_emit_sse_arith_rr(cb, arith_op, 0, xa);  // xmm0 = xmm0 op xa
                    x86_cache_put(&cache, 0, d);                 // relabel xmm0 as dest
                } else {
                    x86_emit_sse_arith_rr(cb, arith_op, xa, xb); // xa op= xb
                    x86_cache_put(&cache, xa, d);                // relabel xa as dest
                }
                break;
            }
            case OP_MOD: {                                       // x - trunc(x/y)*y
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);    // movapd scratch, a
                x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);  // divsd  scratch, b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);                               // roundsd opcode
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3) | XMM_SCRATCH);  // modrm scratch, scratch
                emit_u8(cb, 0x03);                               // round mode: truncate
                x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);  // mulsd  scratch, b
                x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);  // subsd  a, scratch
                x86_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_NEG: {                                       // unary minus
                int xa = x86_cache_load(&cache, cb, a);          // load operand
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // xorpd scratch, scratch
                x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);  // subsd scratch, a
                x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);    // movapd a, scratch
                x86_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_INC:
            case OP_DEC: {                                       // in-place +1 / -1
                int xd = x86_cache_load(&cache, cb, d);          // load dest
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
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
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x94); emit_u8(cb, 0xC0);  // sete al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);    // movapd d, scratch
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_NEQ:
            case OP_CMP_NEQ_NUM: {                               // d = (a != b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x95); emit_u8(cb, 0xC0);  // setne al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);    // movapd d, scratch
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LT: {                                    // d = (a < b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x92); emit_u8(cb, 0xC0);  // setb al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);    // movapd d, scratch
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GT: {                                    // d = (a > b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x97); emit_u8(cb, 0xC0);  // seta al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);    // movapd d, scratch
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LTE: {                                   // d = (a <= b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x96); emit_u8(cb, 0xC0);  // setbe al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);    // movapd d, scratch
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GTE: {                                   // d = (a >= b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x93); emit_u8(cb, 0xC0);  // setae al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);    // movapd d, scratch
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_JUMP: {                                      // unconditional jump
                x86_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0xE9);                               // jmp rel32 opcode
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_FALSE: {                             // branch when cond == 0
                int xa = x86_cache_load(&cache, cb, a);          // load condition
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // xorpd scratch, scratch
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | XMM_SCRATCH);     // ucomisd a, 0
                x86_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je rel32
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
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                x86_cache_flush(&cache, cb);                     // flush before branch
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
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                x86_cache_flush(&cache, cb);                     // flush before branch
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
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                x86_cache_flush(&cache, cb);                     // flush before branch
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
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                x86_cache_flush(&cache, cb);                     // flush before branch
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
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                x86_cache_flush(&cache, cb);                     // flush before branch
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
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                x86_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0x0F); emit_u8(cb, 0x83);            // jae rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_CALL_0: {                                    // call with no args
                x86_cache_flush(&cache, cb);                     // spill everything before call
                emit_call_func(cb, ctx, a);                      // result in xmm0
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
                    if (cache.slot_dirty[s]) x86_emit_movsd_store(cb, i, x86_slot_disp(s));
                }
                if (arg_xmm >= 0 && arg_xmm != 0) {              // arg in cache, not xmm0
                    x86_emit_sse66_rr(cb, 0x28, 0, arg_xmm);     // movapd xmm0, arg_xmm
                } else if (arg_xmm < 0) {                        // arg not cached
                    x86_emit_movsd_load(cb, 0, x86_slot_disp(b));  // reload from memory
                }                                                // else arg already in xmm0
                x86_cache_clear(&cache);                         // xmm regs clobbered by callee
                emit_call_func(cb, ctx, a);                      // result in xmm0
                x86_cache_put(&cache, 0, d);                     // xmm0 = result
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
                    if (cache.slot_dirty[s]) x86_emit_movsd_store(cb, i, x86_slot_disp(s));
                }
                if (arg0_xmm >= 0) {                             // arg0 in cache
                    x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, arg0_xmm);  // stash arg0 in scratch
                } else {                                         // arg0 not cached
                    x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(b));
                }
                if (arg1_xmm >= 0 && arg1_xmm != 1) {            // arg1 in cache, not xmm1
                    x86_emit_sse66_rr(cb, 0x28, 1, arg1_xmm);    // movapd xmm1, arg1_xmm
                } else if (arg1_xmm < 0) {                       // arg1 not cached
                    x86_emit_movsd_load(cb, 1, x86_slot_disp(b + 1));  // reload from memory
                }                                                // else arg1 already in xmm1
                x86_emit_sse66_rr(cb, 0x28, 0, XMM_SCRATCH);     // arg0 from scratch to xmm0
                x86_cache_clear(&cache);                         // xmm regs clobbered
                emit_call_func(cb, ctx, a);                      // result in xmm0
                x86_cache_put(&cache, 0, d);                     // xmm0 = result
                break;
            }
            case OP_RETURN:                                      // return slot value
            case OP_RETURN_NUM:
            case OP_RETURN_BOOL: {
                int xa = x86_cache_load(&cache, cb, d);          // load return value
                if (xa < 0) xa = 0;                              // fall back to xmm0
                if (xa != 0) x86_emit_sse66_rr(cb, 0x28, 0, xa); // movapd xmm0, xa
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);                         // clear state on exit
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_RETURN_NONE: {                               // return none, no value
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
                emit_u8(cb, 0x57); emit_u8(cb, 0xC0);            // xorpd xmm0, xmm0
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);                         // clear state on exit
                did_flush = true;                                // suppress merge flush
                break;
            }
            default:                                             // unreachable in pure fn
                emit_return_zero(abi, cb, base_frame);           // safe fallback
                break;
        }

        // flush if the next instruction is a jump target — the incoming cache state at a merge point must be consistent
        if (!did_flush && pc + 1 < end && is_target[pc + 1 - start]) {
            x86_cache_flush(&cache, cb);
        }
    }

    emit_return_zero(abi, cb, base_frame);                       // safety fallthrough

    for (int i = 0; i < fixup_count; i++) {                      // patch every pending jump
        JumpFixup* fx = &fixups[i];
        int tidx = fx->target_pc - start;                        // index within range
        if (tidx < 0 || tidx >= range_size || label_off[tidx] < 0) {
            cb->len = mark;                                      // rollback partial emission
            return false;                                        // invalid target
        }
        int32_t rel = (int32_t)label_off[tidx] - (int32_t)(fx->patch_at + 4);  // rel32 distance
        memcpy(cb->buf + fx->patch_at, &rel, 4);                 // write rel32
    }

    *out_fn = (void*)(cb->buf + mark);                           // publish entry pointer
    return true;                                                 // emission successful
}

// emits one non-jump loop-body instruction using the xmm register cache
static void emit_loop_body_instr(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
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
            int xa = x86_cache_load(cache, cb, a);               // load source slot
            if (xa < 0) return;                                  // register cache full
            x86_cache_put(cache, xa, d);                         // relabel xmm as dest
            break;
        }
        case OP_LOAD_NUM_IMM: {
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);     // pick a free xmm
            if (x < 0) return;                                   // register cache full
            double v = (double)a;                                // operand as double
            uint64_t bits; memcpy(&bits, &v, 8);                 // reinterpret as u64
            x86_emit_movabs_rax(cb, bits);                       // rax = bit pattern
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_LOAD_NUM: {
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);     // pick a free xmm
            if (x < 0) return;                                   // register cache full
            double v = chunk->constants[a].number_value;         // fetch pool constant
            uint64_t bits; memcpy(&bits, &v, 8);                 // reinterpret as u64
            x86_emit_movabs_rax(cb, bits);                       // rax = bit pattern
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
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
            x86_emit_sse_arith_rr(cb, op, xa, xb);               // xa op= xb
            x86_cache_put(cache, xa, d);                         // relabel xa as dest
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
            emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
            emit_u8(cb, 0x0B);                                   // roundsd opcode
            emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3) | XMM_SCRATCH);  // round scratch
            emit_u8(cb, 0x03);                                   // round mode: truncate
            x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);    // scratch = trunc(a/b) * b
            x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);    // a = a - scratch
            x86_cache_put(cache, xa, d);                         // relabel xa as dest
            touched_nan_check = true;
            break;
        }
        case OP_NEG: {
            int xa = x86_cache_load(cache, cb, a);               // load operand
            if (xa < 0) return;                                  // register cache full
            x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // scratch = 0
            x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);    // scratch = 0 - a
            x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);        // a = scratch
            x86_cache_put(cache, xa, d);                         // relabel xa as dest
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
            emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
            emit_u8(cb, 0xC0 | (xa << 3) | xb);                  // ucomisd a, b
            uint8_t cc;                                          // setcc opcode
            switch (inst->opcode) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  cc = 0x94; break;  // sete
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: cc = 0x95; break;  // setne
                case OP_CMP_LT:  cc = 0x92; break;               // setb
                case OP_CMP_GT:  cc = 0x97; break;               // seta
                case OP_CMP_LTE: cc = 0x96; break;               // setbe
                default:         cc = 0x93; break;               // setae (gte)
            }
            emit_u8(cb, 0x0F); emit_u8(cb, cc); emit_u8(cb, 0xC0);  // setcc al
            emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
            emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
            emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));              // cvtsi2sd scratch, eax
            int xd = x86_cache_alloc_excl(cache, cb, xa, xb);    // pick dest xmm
            if (xd < 0) return;                                  // register cache full
            x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // dest = scratch
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
        case OP_TABLE_GET: {
            int table_reg = a;                               // table register
            int key_reg   = b;                               // key register

            // fast path: counter key + this loop's primary table (rbx already holds its array_part)
            if (key_reg == info->for_var_reg && table_reg == info->table.slot) {
                int xk = cache->slot_reg[key_reg];           // key already in cache?
                if (xk < 0) {
                    xk = x86_cache_load(cache, cb, key_reg); // load key from frame
                    if (xk < 0) return;                      // register cache full
                }
                x86_emit_cvttsd2si_eax(cb, xk);              // eax = (int)key
                x86_emit_dec_eax(cb);                        // 1-based -> 0-based
                int xd = x86_cache_alloc_excl(cache, cb, xk, -1);  // pick dest, keep key
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
            int xd = x86_cache_alloc_excl(cache, cb, xk, -1);  // pick dest, keep key
            if (xd < 0) return;                              // register cache full
            x86_emit_movsd_load_idx8(cb, xd, X86_RAX, X86_RDX);  // xd = array_part[rdx]
            x86_cache_put(cache, xd, d);                     // cache dest
            break;
        }
        case OP_TABLE_GET_INT: {
            int xd = x86_cache_alloc_excl(cache, cb, -1, -1);    // pick a free xmm
            if (xd < 0) return;                                  // register cache full
            x86_emit_movsd_load_base(cb, X86_RBX, xd, (b - 1) * 8);  // xd = array_part[b-1]
            x86_cache_put(cache, xd, d);                         // cache dest
            break;
        }
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
            int xt = cache->slot_reg[table_reg];          // cache already flushed
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));  // rax = frame[table_reg]
            x86_emit_clear_high16_rax(cb);                // strip nan-box tag
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);       // mov rcx, rax
            x86_emit_movq_rax_xmm(cb, xv);                // rax = value bits
            emit_u8(cb, 0x49); emit_u8(cb, 0x89); emit_u8(cb, 0xC0);       // mov r8, rax
#else
            x86_emit_cvttsd2si_edx(cb, xk);               // edx = (int)key
            x86_emit_dec_edx(cb);                         // 1-based -> 0-based
            emit_u8(cb, 0x89); emit_u8(cb, 0xD6);         // mov esi, edx
            int xt = cache->slot_reg[table_reg];          // cache already flushed
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
        default:
            break;                                               // unreachable in native loops
    }

    if (touched_nan_check && info->touches_tables) {         // NaN propagation matches interpreter
        int xd = cache->slot_reg[d];                         // dest slot still in cache?
        if (xd >= 0) emit_nan_check(cb, xd);                 // rewrite NaN to NONE bits
    }
}

// emits one iteration (entry test + body) and returns the fixup offset for the exit jump
static size_t emit_loop_iteration(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                   XmmCache* cache, JitLoopInfo* info, int const_slot,
                                   int save_slot, bool iter_in_xmm, int step_sign) {
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
        int xc = x86_cache_load_excl(cache, cb, var_reg, xa, xb);   // load counter, avoid xa/xb
        if (xc < 0) return (size_t)-1;                       // register cache full

        if (!iter_in_xmm) {
            x86_emit_movsd_load(cb, 7, x86_slot_disp(info->nregs));  // xmm7 = iterator
        }
        emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
        emit_u8(cb, 0xC0 | (7 << 3) | xa);                   // ucomisd xmm7, xa
        emit_u8(cb, 0x0F);
        emit_u8(cb, (step_sign > 0) ? 0x87 : 0x82);          // ja exit / jb exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32

        x86_emit_sse66_rr(cb, 0x28, xc, 7);                  // movapd xc, xmm7 (R[var] = c)
        x86_cache_put(cache, xc, var_reg);                   // var became dirty
        x86_emit_movsd_store(cb, xc, x86_slot_disp(var_reg)); // frame[var] = i
        cache->slot_dirty[var_reg] = false;                  // memory is in sync

        x86_emit_sse_arith_rr(cb, 0x58, 7, xb);              // addsd xmm7, xb (step)
        if (!iter_in_xmm) {
            x86_emit_movsd_store(cb, 7, x86_slot_disp(info->nregs));  // store iterator
        }
    } else {                                                 // condition-based loop
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];                     // left operand slot
        int b = entry_inst->operands[2];                     // right operand slot
        int xa = x86_cache_load(cache, cb, a);               // load left operand
        if (xa < 0) return (size_t)-1;                       // register cache full
        int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load right, avoid xa
        if (xb < 0) return (size_t)-1;                       // register cache full

        emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
        emit_u8(cb, 0xC0 | (xa << 3) | xb);                  // ucomisd xa, xb

        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);  // exit condition opcode
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                 // conditional exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32
    }

    for (int pc = entry + 1; pc < back_edge; pc++) {         // body
        emit_loop_body_instr(abi, ctx, cb, cache, pc, const_slot, save_slot, info);
    }

    if (info->touches_tables) {                              // table ops need the frame in sync
        x86_cache_flush(cache, cb);
    }

    return exit_patch;                                       // caller patches the exit jump
}

// checks whether an opcode writes to its operands[0] destination
static bool op_writes_dest(Opcode op) {
    switch (op) {
        case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_TABLE_GET: case OP_TABLE_GET_INT:
        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
            return true;                                     // d is written by these ops
        default:
            return false;                                    // d is untouched
    }
}

// rejects loops that would need runtime checks the emitter does not produce
static bool loop_is_safe_to_emit(JITContext* ctx, JitLoopInfo* info) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry     = info->entry_pc;                          // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry

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
static bool x86_64_emit_numeric_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                      JitLoopInfo* info, int step_sign, void** out_fn) {
    int entry = info->entry_pc;                              // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry
    int nregs = info->nregs;                                 // function frame size
    int extra_slots = (info->kind == JIT_LOOP_NUMERIC_FOR) ? 1 : 0;  // iterator temp slot
    int const_slot = nregs + extra_slots;                    // reserved slot for constant 1.0

    bool needs_helper = false;                               // does the body call table_set_int / a function?
    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &ctx->chunk->code[pc];
        if (inst->opcode == OP_TABLE_SET) {
            if (inst->operands[1] != info->for_var_reg ||    // key is not the loop counter
                inst->operands[0] != info->table.slot) {     // table is not the primary table
                needs_helper = true;                         // general path needed
                break;
            }
        }
        if (inst->opcode == OP_CALL_0 ||                     // any call clobbers frame_reg
            inst->opcode == OP_CALL_1 ||
            inst->opcode == OP_CALL_2) {
            needs_helper = true;                             // reserve a save slot
            break;
        }
    }

    int save_slot = -1;                                      // stack slot to stash frame_reg across helper call
    int frame_slots = const_slot + 1;                        // incl. constant slot
    if (needs_helper) {
        save_slot = frame_slots;                             // reserve one more slot
        frame_slots++;
    }

    int table_saves_bytes = info->table.used ? 8 : 0;        // rbx save if table access

    int range_size = back_edge - entry + 1;
    if (range_size <= 0) return false;                       // empty range, nothing to emit

    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;  // buffer too small

    if (!loop_is_safe_to_emit(ctx, info)) return false;      // needs runtime checks the emitter lacks

    // scan body to decide if the iterator can stay in xmm7 across iterations
    bool iter_in_xmm = false;
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {                // only numeric-for has iterator
        bool clobbers = false;
        for (int pc = entry + 1; pc < back_edge; pc++) {
            Opcode op = ctx->chunk->code[pc].opcode;
            if (op == OP_MOD || op == OP_NEG ||              // use XMM_SCRATCH
                op == OP_CALL_0 || op == OP_CALL_1 ||        // calls clobber all xmm regs
                op == OP_CALL_2 ||
                op == OP_CMP_EQ || op == OP_CMP_NEQ ||       // also use XMM_SCRATCH
                op == OP_CMP_EQ_NUM || op == OP_CMP_NEQ_NUM ||
                op == OP_CMP_LT || op == OP_CMP_GT ||
                op == OP_CMP_LTE || op == OP_CMP_GTE) {
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
    for (int iter = 0; iter < 8; iter++) {                   // fixpoint loop
        XmmCache cache = cache_start;
        scratch.len = 0;
        size_t patch = emit_loop_iteration(abi, ctx, &scratch, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign);
        if (patch == (size_t)-1) return false;               // emit failed
        if (x86_cache_eq(&cache, &cache_start)) { converged = true; break; }  // stable state reached
        cache_start = cache;                                 // try again with new state
    }
    if (!converged) return false;                            // cache state oscillates, give up

    // real emit
    size_t mark = cb->len;                                   // rollback point

    int base_frame = align16(8 * frame_slots);
    int frame_size = align16(base_frame + abi->frame_extra + table_saves_bytes);

    emit_prologue(cb, abi, frame_size, base_frame);

    // save rbx if we touch tables, and preload array_part into rbx
    int rbx_slot_off = base_frame + abi->frame_extra + 8;
    if (info->table.used && info->table.slot >= 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);  // save caller's rbx
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, info->table.slot * 8);  // rax = regs[slot]
        x86_emit_clear_high16_rax(cb);                       // strip nan-box tag
        x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));  // rbx = array_part
    }

    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);          // rax = bits of 1.0
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                  // xmm7 = 1.0
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));  // const_slot = 1.0

    // copy live-in slots from vm regs to stack frame
    uint64_t m = info->live_in;
    while (m) {
        int s = __builtin_ctzll(m);                          // next live-in slot
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

    if (info->kind == JIT_LOOP_NUMERIC_FOR && iter_in_xmm) {  // load iterator to xmm7
        int var_reg = ctx->chunk->code[entry].operands[0];
        x86_emit_movsd_load(cb, 7, x86_slot_disp(var_reg));  // xmm7 = R[var]
    }

    int loop_top = (int)cb->len;                             // loop start address
    XmmCache cache = cache_start;
    size_t entry_patch = emit_loop_iteration(abi, ctx, cb, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign);
    if (entry_patch == (size_t)-1) { cb->len = mark; return false; }  // emit failed, rollback

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
        x86_emit_movsd_load(cb, 0, x86_slot_disp(s));        // xmm0 = frame[s]
        x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);  // regs[s] = xmm0
    }

    if (info->table.used && info->table.slot >= 0) {         // restore caller's rbx
        x86_emit_load_r64_rbp(cb, X86_RBX, -rbx_slot_off);
    }

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;                                             // emission successful
}

// emits native code for a table iteration loop (for v in t)
static bool x86_64_emit_table_iter_loop(const X86_64Abi* abi, JITContext* ctx,
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
    int frame_slots = const_slot + 1;                        // incl. the unused constant slot
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

    // copy live-in slots from vm regs to frame, except table and loop var
    uint64_t m = info->live_in;
    while (m) {
        int s = __builtin_ctzll(m);                          // next live-in slot
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
        x86_emit_movsd_load(cb, 0, x86_slot_disp(s));        // xmm0 = frame[s]
        x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);  // regs[s] = xmm0
    }

    x86_emit_load_r64_rbp(cb, X86_RBX, -off_rbx);             // restore caller's rbx
    x86_emit_load_r64_rbp(cb, X86_R12, -off_r12);             // restore caller's r12
    x86_emit_load_r64_rbp(cb, X86_R13, -off_r13);             // restore caller's r13

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;                                             // emission successful
}

// emits native code for a single native loop, dispatching by kind
bool x86_64_emit_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                      JitLoopInfo* info, int step_sign, void** out_fn) {
    if (info->kind == JIT_LOOP_TABLE_ITER) {                 // table iteration loop
        return x86_64_emit_table_iter_loop(abi, ctx, cb, info, out_fn);
    }
    return x86_64_emit_numeric_loop(abi, ctx, cb, info, step_sign, out_fn);  // numeric or condition loop
}
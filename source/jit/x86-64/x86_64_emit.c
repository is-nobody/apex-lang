// source/jit/x86-64/x86_64_emit.c
// Implementation of x86-64 function and loop emitters for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_common.h"
#include "x86_64_abi.h"
#include <string.h>

// rounds n up to the next multiple of 16
static int align16(int n) { return (n + 15) & ~15; }

// emits the shared prologue then any abi-specific callee-saved register saves
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
            if ((nx->opcode == OP_RETURN || nx->opcode == OP_RETURN_NUM) &&
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
            case OP_RETURN_NUM: {
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

// emits a single non-jump body instruction inside a native loop using the register cache
static void emit_loop_body_instr(JITContext* ctx, CodeBuf* cb, XmmCache* cache,
                                  int pc, int const_slot) {
    BytecodeChunk* chunk = ctx->chunk;
    Instruction* inst = &chunk->code[pc];
    int d = inst->operands[0];
    int a = inst->operands[1];
    int b = inst->operands[2];

    switch (inst->opcode) {
        case OP_MOVE: {
            int xa = x86_cache_load(cache, cb, a);
            if (xa < 0) return;
            x86_cache_put(cache, xa, d);
            break;
        }
        case OP_LOAD_NUM_IMM: {
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) return;
            double v = (double)a;
            uint64_t bits; memcpy(&bits, &v, 8);
            x86_emit_movabs_rax(cb, bits);
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, d);
            break;
        }
        case OP_LOAD_NUM: {
            int x = x86_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) return;
            double v = chunk->constants[a].number_value;
            uint64_t bits; memcpy(&bits, &v, 8);
            x86_emit_movabs_rax(cb, bits);
            x86_emit_movq_xmm_rax(cb, x);
            x86_cache_put(cache, x, d);
            break;
        }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: {
            int xa = x86_cache_load(cache, cb, a);
            if (xa < 0) return;
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);
            if (xb < 0) return;
            uint8_t op;
            switch (inst->opcode) {
                case OP_ADD: op = 0x58; break;
                case OP_SUB: op = 0x5C; break;
                case OP_MUL: op = 0x59; break;
                default:     op = 0x5E; break;
            }
            x86_emit_sse_arith_rr(cb, op, xa, xb);
            x86_cache_put(cache, xa, d);
            break;
        }
        case OP_MOD: {
            int xa = x86_cache_load(cache, cb, a);
            if (xa < 0) return;
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);
            if (xb < 0) return;
            x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);
            x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);
            emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
            emit_u8(cb, 0x0B);
            emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3) | XMM_SCRATCH);
            emit_u8(cb, 0x03);
            x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);
            x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);
            x86_cache_put(cache, xa, d);
            break;
        }
        case OP_NEG: {
            int xa = x86_cache_load(cache, cb, a);
            if (xa < 0) return;
            x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);
            x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);
            x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);
            x86_cache_put(cache, xa, d);
            break;
        }
        case OP_INC: case OP_DEC: {
            int xd = x86_cache_load(cache, cb, d);
            if (xd < 0) return;
            uint8_t op = (inst->opcode == OP_INC) ? 0x58 : 0x5C;
            x86_emit_sse_arith_mem(cb, op, xd, x86_slot_disp(const_slot));  // addsd/subsd xd, [1.0]
            x86_cache_put(cache, xd, d);
            break;
        }
        case OP_CMP_EQ: case OP_CMP_EQ_NUM:
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE: {
            int xa = x86_cache_load(cache, cb, a);
            if (xa < 0) return;
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);
            if (xb < 0) return;
            emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
            emit_u8(cb, 0xC0 | (xa << 3) | xb);
            uint8_t cc;
            switch (inst->opcode) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  cc = 0x94; break;
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: cc = 0x95; break;
                case OP_CMP_LT:  cc = 0x92; break;
                case OP_CMP_GT:  cc = 0x97; break;
                case OP_CMP_LTE: cc = 0x96; break;
                default:         cc = 0x93; break;
            }
            emit_u8(cb, 0x0F); emit_u8(cb, cc); emit_u8(cb, 0xC0);
            emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);
            emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
            emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));
            int xd = x86_cache_alloc_excl(cache, cb, xa, xb);
            if (xd < 0) return;
            x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);
            x86_cache_put(cache, xd, d);
            break;
        }
        default:
            break;
    }
}

// emits one iteration (entry test + body) and returns the fixup offset for the exit jump
static size_t emit_loop_iteration(JITContext* ctx, CodeBuf* cb, XmmCache* cache,
                                   JitLoopInfo* info, int const_slot,
                                   bool iter_in_xmm) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry = info->entry_pc;
    int back_edge = info->back_edge_pc;

    size_t exit_patch;
    if (info->is_for_next) {
        int var_reg = chunk->code[entry].operands[0];
        int end_reg = info->for_end_reg;
        int step_reg = info->for_step_reg;

        int xa = x86_cache_load(cache, cb, end_reg);
        if (xa < 0) return (size_t)-1;
        int xb = x86_cache_load_excl(cache, cb, step_reg, xa, -1);
        if (xb < 0) return (size_t)-1;
        int xc = x86_cache_load_excl(cache, cb, var_reg, xa, xb);
        if (xc < 0) return (size_t)-1;

        if (!iter_in_xmm) {
            x86_emit_movsd_load(cb, 7, x86_slot_disp(info->nregs));  // xmm7 = iterator
        }
        emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
        emit_u8(cb, 0xC0 | (7 << 3) | xa);                   // ucomisd xmm7, xa
        emit_u8(cb, 0x0F); emit_u8(cb, 0x87);                // ja exit
        exit_patch = cb->len;
        emit_i32(cb, 0);

        x86_emit_sse66_rr(cb, 0x28, xc, 7);                  // movapd xc, xmm7 (R[var] = c)
        x86_cache_put(cache, xc, var_reg);                   // var became dirty

        x86_emit_sse_arith_rr(cb, 0x58, 7, xb);              // addsd xmm7, xb (step)
        if (!iter_in_xmm) {
            x86_emit_movsd_store(cb, 7, x86_slot_disp(info->nregs));  // store iterator
        }
    } else {
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];
        int b = entry_inst->operands[2];
        int xa = x86_cache_load(cache, cb, a);
        if (xa < 0) return (size_t)-1;
        int xb = x86_cache_load_excl(cache, cb, b, xa, -1);
        if (xb < 0) return (size_t)-1;

        emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
        emit_u8(cb, 0xC0 | (xa << 3) | xb);                  // ucomisd xa, xb

        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                 // conditional exit
        exit_patch = cb->len;
        emit_i32(cb, 0);
    }

    for (int pc = entry + 1; pc < back_edge; pc++) {         // body
        emit_loop_body_instr(ctx, cb, cache, pc, const_slot);
    }
    return exit_patch;
}

// emits native x86-64 code for a single numeric loop
bool x86_64_emit_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                      JitLoopInfo* info) {
    int entry = info->entry_pc;
    int back_edge = info->back_edge_pc;
    int nregs = info->nregs;
    int extra_slots = info->is_for_next ? 1 : 0;             // iterator temp slot
    int const_slot = nregs + extra_slots;                    // reserved slot for constant 1.0
    int frame_slots = const_slot + 1;

    int range_size = back_edge - entry + 1;
    if (range_size <= 0) return false;

    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;

    // scan body to see if xmm7 (iterator cache) stays intact across the loop
    bool iter_in_xmm = false;
    if (info->is_for_next) {
        bool clobbers = false;
        for (int pc = entry + 1; pc < back_edge; pc++) {
            Opcode op = ctx->chunk->code[pc].opcode;
            if (op == OP_MOD || op == OP_NEG ||
                op == OP_CMP_EQ || op == OP_CMP_NEQ ||
                op == OP_CMP_EQ_NUM || op == OP_CMP_NEQ_NUM ||
                op == OP_CMP_LT || op == OP_CMP_GT ||
                op == OP_CMP_LTE || op == OP_CMP_GTE) {
                clobbers = true;
                break;
            }
        }
        iter_in_xmm = !clobbers;
    }

    // fixpoint pass in a scratch buffer to compute the steady-state cache
    uint8_t* scratch_buf = ctx->scratch_code_buf;                // shared scratch buffer
    size_t scratch_size  = ctx->scratch_code_buf_cap;
    if (!scratch_buf || scratch_size < (size_t)range_size * 256 + 1024) return false;
    CodeBuf scratch = { scratch_buf, 0, scratch_size };

    XmmCache cache_start;
    x86_cache_clear(&cache_start);
    bool converged = false;
    for (int iter = 0; iter < 8; iter++) {
        XmmCache cache = cache_start;
        scratch.len = 0;
        size_t patch = emit_loop_iteration(ctx, &scratch, &cache, info, const_slot, iter_in_xmm);
        if (patch == (size_t)-1) return false;
        if (x86_cache_eq(&cache, &cache_start)) { converged = true; break; }
        cache_start = cache;
    }
    if (!converged) return false;

    // real emit
    size_t mark = cb->len;

    int base_frame = align16(8 * frame_slots);
    int frame_size = base_frame + abi->frame_extra;
    emit_prologue(cb, abi, frame_size, base_frame);

    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);              // rax = bits of 1.0
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                      // xmm7 = 1.0
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));  // store 1.0 to reserved slot

    // copy live-in slots from vm regs (pointed to by abi->frame_reg) to stack
    uint64_t m = info->live_in;
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);  // xmm0 = regs[s]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));           // stack[s] = xmm0
    }

    if (info->is_for_next && !iter_in_xmm) {                 // seed iterator in memory
        int var_reg = ctx->chunk->code[entry].operands[0];
        x86_emit_movsd_load(cb, 0, x86_slot_disp(var_reg));
        x86_emit_movsd_store(cb, 0, x86_slot_disp(nregs));
    }

    // preload the fixpoint cache state
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = cache_start.reg_slot[i];
        if (s >= 0) x86_emit_movsd_load(cb, i, x86_slot_disp(s));
    }

    if (info->is_for_next && iter_in_xmm) {                  // load iterator directly to xmm7
        int var_reg = ctx->chunk->code[entry].operands[0];
        x86_emit_movsd_load(cb, 7, x86_slot_disp(var_reg));
    }

    int loop_top = (int)cb->len;
    XmmCache cache = cache_start;
    size_t entry_patch = emit_loop_iteration(ctx, cb, &cache, info, const_slot, iter_in_xmm);
    if (entry_patch == (size_t)-1) { cb->len = mark; return false; }

    emit_u8(cb, 0xE9);                                       // jmp loop_top
    size_t back_patch = cb->len;
    emit_i32(cb, 0);
    int32_t back_rel = loop_top - (int32_t)(back_patch + 4);
    memcpy(cb->buf + back_patch, &back_rel, 4);

    int exit_label = (int)cb->len;
    int32_t exit_rel = exit_label - (int32_t)(entry_patch + 4);
    memcpy(cb->buf + entry_patch, &exit_rel, 4);

    x86_cache_flush(&cache, cb);                             // spill dirty slots to stack

    m = info->live_out;                                      // copy modified slots back
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        x86_emit_movsd_load(cb, 0, x86_slot_disp(s));
        x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);
    }

    emit_leave_ret(cb, abi, base_frame);

    info->native_fn = (void (*)(uint64_t*))(cb->buf + mark);
    return true;
}
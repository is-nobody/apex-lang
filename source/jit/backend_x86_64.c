// source/jit/backend_x86_64.c
// x86-64 code emission — register cache, function and loop emitters.
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit_internal.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// xmm0..xmm6 used as slot cache, xmm7 reserved as scratch
#define XMM_CACHE_REGS 7
#define XMM_SCRATCH    7

// register cache state: which slot lives in which xmm, and which slots have been written since they were last flushed to memory
typedef struct {
    int  reg_slot[XMM_CACHE_REGS];  // xmm[i] holds slot reg_slot[i], or -1
    int  slot_reg[JIT_MAX_SLOTS];   // slot -> xmm index, or -1
    bool slot_dirty[JIT_MAX_SLOTS]; // slot written but not yet spilled
} XmmCache;

// converts register index to rbp-relative slot displacement
static inline int32_t slot_disp(int reg) { return -8 * (reg + 1); }

// emits movsd xmm<N>, [rbp+disp32] — load slot into sse register
static inline void emit_movsd_load(CodeBuf* b, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x10);  // movsd opcode
    emit_u8(b, 0x85 | (xmm << 3));                         // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits movsd [rbp+disp32], xmm<N> — store sse register into slot
static inline void emit_movsd_store(CodeBuf* b, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x11);  // movsd store opcode
    emit_u8(b, 0x85 | (xmm << 3));                         // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits movsd xmm<N>, [rdi+disp32] — load from the VM register frame
static inline void emit_movsd_load_rdi(CodeBuf* b, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x10);  // movsd opcode
    emit_u8(b, 0x87 | (xmm << 3));                         // modrm with rdi base
    emit_i32(b, disp);                                     // displacement
}

// emits movsd [rdi+disp32], xmm<N> — store into the VM register frame
static inline void emit_movsd_store_rdi(CodeBuf* b, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x11);  // movsd store opcode
    emit_u8(b, 0x87 | (xmm << 3));                         // modrm with rdi base
    emit_i32(b, disp);                                     // displacement
}

// emits <op>sd xmm_dst, [rbp+disp32] — scalar-double op with memory source
static inline void emit_sse_arith_mem(CodeBuf* b, uint8_t op,
                                       int xmm_dst, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, op);    // scalar-double opcode
    emit_u8(b, 0x85 | (xmm_dst << 3));                     // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits <op>sd xmm_dst, xmm_src — scalar-double op, register-register
static inline void emit_sse_arith_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, op);    // scalar-double opcode
    emit_u8(b, 0xC0 | (dst << 3) | src);                   // modrm, register-register
}

// emits packed-double register-register op (movapd, xorpd, etc)
static inline void emit_sse66_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0x66); emit_u8(b, 0x0F); emit_u8(b, op);    // packed-double opcode
    emit_u8(b, 0xC0 | (dst << 3) | src);                   // modrm, register-register
}

// emits movabs rax, imm64 — load 64-bit immediate into rax
static inline void emit_movabs_rax(CodeBuf* b, uint64_t v) {
    emit_u8(b, 0x48); emit_u8(b, 0xB8);                    // rex.w + movabs opcode
    emit_u64(b, v);                                        // 64-bit immediate
}

// emits movq xmm<N>, rax — move rax into sse register
static inline void emit_movq_xmm_rax(CodeBuf* b, int xmm) {
    emit_u8(b, 0x66); emit_u8(b, 0x48);                    // operand-size + rex.w
    emit_u8(b, 0x0F); emit_u8(b, 0x6E);                    // movq xmm, r/m64
    emit_u8(b, 0xC0 | (xmm << 3));                         // modrm, rm=rax
}

// clears cache state without touching memory
static void xmm_cache_clear(XmmCache* c) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) c->reg_slot[i] = -1;
    for (int i = 0; i < JIT_MAX_SLOTS; i++) {
        c->slot_reg[i] = -1;
        c->slot_dirty[i] = false;
    }
}

// checks two cache states for structural equality
static bool xmm_cache_eq(const XmmCache* a, const XmmCache* b) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        if (a->reg_slot[i] != b->reg_slot[i]) return false;
    }
    for (int i = 0; i < JIT_MAX_SLOTS; i++) {
        if (a->slot_reg[i] != b->slot_reg[i]) return false;
        if (a->slot_dirty[i] != b->slot_dirty[i]) return false;
    }
    return true;
}

// finds or evicts an xmm register, avoiding two registers that are live
static int xmm_cache_alloc_excl(XmmCache* c, CodeBuf* cb, int excl1, int excl2) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) {              // prefer free registers
        if (i == excl1 || i == excl2) continue;
        if (c->reg_slot[i] == -1) return i;
    }
    int victim = -1;                                        // otherwise evict
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        if (i == excl1 || i == excl2) continue;
        if (victim < 0 || c->reg_slot[i] < c->reg_slot[victim]) victim = i;
    }
    if (victim < 0) return -1;                              // all excluded
    int s = c->reg_slot[victim];
    if (c->slot_dirty[s]) emit_movsd_store(cb, victim, slot_disp(s));
    c->reg_slot[victim] = -1;
    c->slot_reg[s] = -1;
    c->slot_dirty[s] = false;
    return victim;
}

// returns an xmm holding slot s, loading from memory if needed
static int xmm_cache_load_excl(XmmCache* c, CodeBuf* cb, int s, int excl1, int excl2) {
    if (s < 0 || s >= JIT_MAX_SLOTS) return -1;
    if (c->slot_reg[s] >= 0) return c->slot_reg[s];         // cache hit
    int x = xmm_cache_alloc_excl(c, cb, excl1, excl2);
    if (x < 0) return -1;
    emit_movsd_load(cb, x, slot_disp(s));                   // cache miss: load
    c->reg_slot[x] = s;
    c->slot_reg[s] = x;
    c->slot_dirty[s] = false;
    return x;
}

// convenience wrapper around xmm_cache_load_excl with no exclusions
static int xmm_cache_load(XmmCache* c, CodeBuf* cb, int s) {
    return xmm_cache_load_excl(c, cb, s, -1, -1);
}

// marks xmm x as holding slot s (dirty), invalidating prior mappings
static void xmm_cache_put(XmmCache* c, int x, int s) {
    if (s < 0 || s >= JIT_MAX_SLOTS) return;
    int old = c->reg_slot[x];
    if (old >= 0 && old != s) c->slot_reg[old] = -1;        // x no longer holds old slot
    int prev = c->slot_reg[s];
    if (prev >= 0 && prev != x) c->reg_slot[prev] = -1;     // that xmm no longer holds s
    c->reg_slot[x] = s;
    c->slot_reg[s] = x;
    c->slot_dirty[s] = true;
}

// writes back all dirty slots to memory and clears the cache
static void xmm_cache_flush(XmmCache* c, CodeBuf* cb) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = c->reg_slot[i];
        if (s >= 0 && c->slot_dirty[s]) emit_movsd_store(cb, i, slot_disp(s));
    }
    xmm_cache_clear(c);
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

// fallback epilogue: returns xmm0 = 0.0
static void emit_return_zero(CodeBuf* cb) {
    emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
    emit_u8(cb, 0x57); emit_u8(cb, 0xC0);                        // xorpd xmm0, xmm0
    emit_u8(cb, 0xC9);                                           // leave
    emit_u8(cb, 0xC3);                                           // ret
}

// emits indirect call through func_table[func_idx]
static void emit_call_func(CodeBuf* cb, JITContext* ctx, int func_idx) {
    uint64_t slot_addr = (uint64_t)(uintptr_t)&ctx->func_table[func_idx];  // address of the slot
    emit_movabs_rax(cb, slot_addr);                              // rax = &func_table[idx]
    emit_u8(cb, 0x48); emit_u8(cb, 0x8B); emit_u8(cb, 0x00);     // rax = *rax
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                        // call rax
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

// emits native x86-64 code for a single numeric-pure function
static bool emit_function(JITContext* ctx, CodeBuf* cb, int func_idx, void** out_fn) {
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

    bool* is_target = (bool*)calloc(range_size, sizeof(bool));   // jump-target marks
    if (!is_target) return false;                                // allocation failed
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

    int32_t* label_off = (int32_t*)malloc(sizeof(int32_t) * range_size);     // label offsets
    JumpFixup* fixups  = (JumpFixup*)malloc(sizeof(JumpFixup) * range_size); // pending jumps
    if (!label_off || !fixups) {                                 // allocation failed
        free(is_target); free(label_off); free(fixups);
        return false;
    }
    for (int i = 0; i < range_size; i++) label_off[i] = -1;      // no label emitted yet
    int fixup_count = 0;                                         // pending jumps count

    size_t mark = cb->len;                                       // rollback point

    XmmCache cache;                                              // register cache state
    xmm_cache_clear(&cache);                                     // start empty

    emit_u8(cb, 0x55);                                           // push rbp
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xE5);     // mov rbp, rsp

    int frame_size = 8 * nregs;                                  // bytes for slots
    if (frame_size % 16) frame_size = (frame_size + 15) & ~15;   // align to 16 bytes
    emit_u8(cb, 0x48); emit_u8(cb, 0x81); emit_u8(cb, 0xEC);     // sub rsp, imm32
    emit_i32(cb, frame_size);                                    // frame size immediate

    int cached_args = arity < XMM_CACHE_REGS ? arity : XMM_CACHE_REGS;  // args fit in cache
    for (int i = 0; i < cached_args; i++) {                      // cache incoming args
        xmm_cache_put(&cache, i, i);                             // xmm<i> = slot i, dirty
    }
    for (int i = cached_args; i < arity; i++) {                  // spill extras
        emit_movsd_store(cb, i, slot_disp(i));                   // xmm<i> -> slot i
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
                int xa = xmm_cache_load(&cache, cb, a);          // find xmm for source
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                xmm_cache_put(&cache, xa, d);                    // relabel xmm as dest
                break;
            }
            case OP_LOAD_NUM_IMM: {                              // small int literal
                int x = xmm_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) { emit_return_zero(cb); break; }      // no register available
                double v = (double)a;                            // operand as double
                uint64_t bits; memcpy(&bits, &v, 8);             // reinterpret as u64
                emit_movabs_rax(cb, bits);                       // rax = bit pattern
                emit_movq_xmm_rax(cb, x);                        // xmmX = rax
                xmm_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_LOAD_NUM: {                                  // full double constant
                int x = xmm_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) { emit_return_zero(cb); break; }      // no register available
                double v = chunk->constants[a].number_value;     // fetch constant
                uint64_t bits; memcpy(&bits, &v, 8);             // reinterpret as u64
                emit_movabs_rax(cb, bits);                       // rax = bit pattern
                emit_movq_xmm_rax(cb, x);                        // xmmX = rax
                xmm_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV: {                                       // binary arithmetic
                int xa = xmm_cache_load(&cache, cb, a);          // load left operand
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load right, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                uint8_t arith_op;                                // sse opcode
                switch (op) {
                    case OP_ADD: arith_op = 0x58; break;         // addsd
                    case OP_SUB: arith_op = 0x5C; break;         // subsd
                    case OP_MUL: arith_op = 0x59; break;         // mulsd
                    default:     arith_op = 0x5E; break;         // divsd
                }
                bool commutative = (op == OP_ADD || op == OP_MUL);  // swap-safe op?
                if (prefer_xmm0 && commutative && xb == 0 && xa != 0) {
                    emit_sse_arith_rr(cb, arith_op, 0, xa);      // xmm0 = xmm0 op xa
                    xmm_cache_put(&cache, 0, d);                 // relabel xmm0 as dest
                } else {
                    emit_sse_arith_rr(cb, arith_op, xa, xb);     // xa op= xb
                    xmm_cache_put(&cache, xa, d);                // relabel xa as dest
                }
                break;
            }
            case OP_MOD: {                                       // x - trunc(x/y)*y
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);        // movapd scratch, a
                emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);    // divsd  scratch, b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);                               // roundsd opcode
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3) | XMM_SCRATCH);  // modrm scratch, scratch
                emit_u8(cb, 0x03);                               // round mode: truncate
                emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);    // mulsd  scratch, b
                emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);    // subsd  a, scratch
                xmm_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_NEG: {                                       // unary minus
                int xa = xmm_cache_load(&cache, cb, a);          // load operand
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // xorpd scratch, scratch
                emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);    // subsd scratch, a
                emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);        // movapd a, scratch
                xmm_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_INC:
            case OP_DEC: {                                       // in-place +1 / -1
                int xd = xmm_cache_load(&cache, cb, d);          // load dest
                if (xd < 0) { emit_return_zero(cb); break; }     // no register available
                emit_movabs_rax(cb, 0x3FF0000000000000ULL);      // rax = 1.0 bits
                emit_movq_xmm_rax(cb, XMM_SCRATCH);              // scratch = 1.0
                uint8_t arith_op = (op == OP_INC) ? 0x58 : 0x5C; // addsd / subsd
                emit_sse_arith_rr(cb, arith_op, xd, XMM_SCRATCH);  // xd op= 1.0
                xmm_cache_put(&cache, xd, d);                    // mark dirty
                break;
            }
            case OP_CMP_EQ:
            case OP_CMP_EQ_NUM: {                                // d = (a == b)
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x94); emit_u8(cb, 0xC0);  // sete al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = xmm_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // movapd d, scratch
                xmm_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_NEQ:
            case OP_CMP_NEQ_NUM: {                               // d = (a != b)
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x95); emit_u8(cb, 0xC0);  // setne al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = xmm_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // movapd d, scratch
                xmm_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LT: {                                    // d = (a < b)
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x92); emit_u8(cb, 0xC0);  // setb al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = xmm_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // movapd d, scratch
                xmm_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GT: {                                    // d = (a > b)
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x97); emit_u8(cb, 0xC0);  // seta al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = xmm_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // movapd d, scratch
                xmm_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LTE: {                                   // d = (a <= b)
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x96); emit_u8(cb, 0xC0);  // setbe al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = xmm_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // movapd d, scratch
                xmm_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GTE: {                                   // d = (a >= b)
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                emit_u8(cb, 0x0F); emit_u8(cb, 0x93); emit_u8(cb, 0xC0);  // setae al
                emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);  // movzbl eax, al
                emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
                emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));          // cvtsi2sd scratch, eax
                int xd = xmm_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // movapd d, scratch
                xmm_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_JUMP: {                                      // unconditional jump
                xmm_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0xE9);                               // jmp rel32 opcode
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_FALSE: {                             // branch when cond == 0
                int xa = xmm_cache_load(&cache, cb, a);          // load condition
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // xorpd scratch, scratch
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | XMM_SCRATCH);     // ucomisd a, 0
                xmm_cache_flush(&cache, cb);                     // flush before branch
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
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                xmm_cache_flush(&cache, cb);                     // flush before branch
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
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                xmm_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0x0F); emit_u8(cb, 0x85);            // jne rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_LT: {                                // branch if a < b
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                xmm_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0x0F); emit_u8(cb, 0x82);            // jb rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_GT: {                                // branch if a > b
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                xmm_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0x0F); emit_u8(cb, 0x87);            // ja rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_LTE: {                               // branch if a <= b
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                xmm_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0x0F); emit_u8(cb, 0x86);            // jbe rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_GTE: {                               // branch if a >= b
                int xa = xmm_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(cb); break; }     // no register available
                int xb = xmm_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(cb); break; }     // no register available
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
                emit_u8(cb, 0xC0 | (xa << 3) | xb);              // ucomisd a, b
                xmm_cache_flush(&cache, cb);                     // flush before branch
                emit_u8(cb, 0x0F); emit_u8(cb, 0x83);            // jae rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_CALL_0: {                                    // call with no args
                xmm_cache_flush(&cache, cb);                     // spill everything before call
                emit_call_func(cb, ctx, a);                      // result in xmm0
                xmm_cache_put(&cache, 0, d);                     // xmm0 = result
                break;
            }
            case OP_CALL_1: {                                    // call with one arg
                bool arg_live = slot_read_before_write(chunk, pc + 1, end, b);  // arg used after?
                int arg_xmm = cache.slot_reg[b];                 // where arg lives now
                for (int i = 0; i < XMM_CACHE_REGS; i++) {       // spill live dirty slots
                    int s = cache.reg_slot[i];
                    if (s < 0) continue;                         // empty slot, skip
                    if (s == b && !arg_live) continue;           // dead arg — memory stays stale
                    if (cache.slot_dirty[s]) emit_movsd_store(cb, i, slot_disp(s));
                }
                if (arg_xmm >= 0 && arg_xmm != 0) {              // arg in cache, not xmm0
                    emit_sse66_rr(cb, 0x28, 0, arg_xmm);         // movapd xmm0, arg_xmm
                } else if (arg_xmm < 0) {                        // arg not cached
                    emit_movsd_load(cb, 0, slot_disp(b));        // reload from memory
                }                                                // else arg already in xmm0
                xmm_cache_clear(&cache);                         // xmm regs clobbered by callee
                emit_call_func(cb, ctx, a);                      // result in xmm0
                xmm_cache_put(&cache, 0, d);                     // xmm0 = result
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
                    if (cache.slot_dirty[s]) emit_movsd_store(cb, i, slot_disp(s));
                }
                if (arg0_xmm >= 0) {                             // arg0 in cache
                    emit_sse66_rr(cb, 0x28, XMM_SCRATCH, arg0_xmm);  // stash arg0 in scratch
                } else {                                         // arg0 not cached
                    emit_movsd_load(cb, XMM_SCRATCH, slot_disp(b));
                }
                if (arg1_xmm >= 0 && arg1_xmm != 1) {            // arg1 in cache, not xmm1
                    emit_sse66_rr(cb, 0x28, 1, arg1_xmm);        // movapd xmm1, arg1_xmm
                } else if (arg1_xmm < 0) {                       // arg1 not cached
                    emit_movsd_load(cb, 1, slot_disp(b + 1));    // reload from memory
                }                                                // else arg1 already in xmm1
                emit_sse66_rr(cb, 0x28, 0, XMM_SCRATCH);         // arg0 from scratch to xmm0
                xmm_cache_clear(&cache);                         // xmm regs clobbered
                emit_call_func(cb, ctx, a);                      // result in xmm0
                xmm_cache_put(&cache, 0, d);                     // xmm0 = result
                break;
            }
            case OP_RETURN:                                      // return slot value
            case OP_RETURN_NUM: {
                int xa = xmm_cache_load(&cache, cb, d);          // load return value
                if (xa < 0) xa = 0;                              // fall back to xmm0
                if (xa != 0) emit_sse66_rr(cb, 0x28, 0, xa);     // movapd xmm0, xa
                emit_u8(cb, 0xC9);                               // leave
                emit_u8(cb, 0xC3);                               // ret
                xmm_cache_clear(&cache);                         // clear state on exit
                did_flush = true;                                // suppress merge flush
                break;
            }
            default:                                             // unreachable in pure fn
                emit_return_zero(cb);                            // safe fallback
                break;
        }

        // flush if the next instruction is a jump target — the incoming cache state at a merge point must be consistent
        if (!did_flush && pc + 1 < end && is_target[pc + 1 - start]) {
            xmm_cache_flush(&cache, cb);
        }
    }

    emit_return_zero(cb);                                        // safety fallthrough

    for (int i = 0; i < fixup_count; i++) {                      // patch every pending jump
        JumpFixup* fx = &fixups[i];
        int tidx = fx->target_pc - start;                        // index within range
        if (tidx < 0 || tidx >= range_size || label_off[tidx] < 0) {
            free(is_target); free(label_off); free(fixups);      // free analysis arrays
            cb->len = mark;                                      // rollback partial emission
            return false;                                        // invalid target
        }
        int32_t rel = (int32_t)label_off[tidx] - (int32_t)(fx->patch_at + 4);  // rel32 distance
        memcpy(cb->buf + fx->patch_at, &rel, 4);                 // write rel32
    }

    free(is_target);                                             // release target marks
    free(label_off);                                             // release label array
    free(fixups);                                                // release fixup array
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
            int xa = xmm_cache_load(cache, cb, a);
            if (xa < 0) return;
            xmm_cache_put(cache, xa, d);
            break;
        }
        case OP_LOAD_NUM_IMM: {
            int x = xmm_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) return;
            double v = (double)a;
            uint64_t bits; memcpy(&bits, &v, 8);
            emit_movabs_rax(cb, bits);
            emit_movq_xmm_rax(cb, x);
            xmm_cache_put(cache, x, d);
            break;
        }
        case OP_LOAD_NUM: {
            int x = xmm_cache_alloc_excl(cache, cb, -1, -1);
            if (x < 0) return;
            double v = chunk->constants[a].number_value;
            uint64_t bits; memcpy(&bits, &v, 8);
            emit_movabs_rax(cb, bits);
            emit_movq_xmm_rax(cb, x);
            xmm_cache_put(cache, x, d);
            break;
        }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: {
            int xa = xmm_cache_load(cache, cb, a);
            if (xa < 0) return;
            int xb = xmm_cache_load_excl(cache, cb, b, xa, -1);
            if (xb < 0) return;
            uint8_t arith_op;
            switch (inst->opcode) {
                case OP_ADD: arith_op = 0x58; break;
                case OP_SUB: arith_op = 0x5C; break;
                case OP_MUL: arith_op = 0x59; break;
                default:     arith_op = 0x5E; break;
            }
            emit_sse_arith_rr(cb, arith_op, xa, xb);
            xmm_cache_put(cache, xa, d);
            break;
        }
        case OP_MOD: {
            int xa = xmm_cache_load(cache, cb, a);
            if (xa < 0) return;
            int xb = xmm_cache_load_excl(cache, cb, b, xa, -1);
            if (xb < 0) return;
            emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);
            emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);
            emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
            emit_u8(cb, 0x0B);
            emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3) | XMM_SCRATCH);
            emit_u8(cb, 0x03);
            emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);
            emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);
            xmm_cache_put(cache, xa, d);
            break;
        }
        case OP_NEG: {
            int xa = xmm_cache_load(cache, cb, a);
            if (xa < 0) return;
            emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);
            emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);
            emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);
            xmm_cache_put(cache, xa, d);
            break;
        }
        case OP_INC: case OP_DEC: {
            int xd = xmm_cache_load(cache, cb, d);
            if (xd < 0) return;
            uint8_t arith_op = (inst->opcode == OP_INC) ? 0x58 : 0x5C;
            emit_sse_arith_mem(cb, arith_op, xd, slot_disp(const_slot));  // addsd/subsd xd, [1.0]
            xmm_cache_put(cache, xd, d);
            break;
        }
        case OP_CMP_EQ: case OP_CMP_EQ_NUM:
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE: {
            int xa = xmm_cache_load(cache, cb, a);
            if (xa < 0) return;
            int xb = xmm_cache_load_excl(cache, cb, b, xa, -1);
            if (xb < 0) return;
            emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
            emit_u8(cb, 0xC0 | (xa << 3) | xb);
            uint8_t setcc;
            switch (inst->opcode) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  setcc = 0x94; break;
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: setcc = 0x95; break;
                case OP_CMP_LT:  setcc = 0x92; break;
                case OP_CMP_GT:  setcc = 0x97; break;
                case OP_CMP_LTE: setcc = 0x96; break;
                default:         setcc = 0x93; break;
            }
            emit_u8(cb, 0x0F); emit_u8(cb, setcc); emit_u8(cb, 0xC0);
            emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);
            emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
            emit_u8(cb, 0xC0 | (XMM_SCRATCH << 3));
            int xd = xmm_cache_alloc_excl(cache, cb, xa, xb);
            if (xd < 0) return;
            emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);
            xmm_cache_put(cache, xd, d);
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

        int xa = xmm_cache_load(cache, cb, end_reg);
        if (xa < 0) return (size_t)-1;
        int xb = xmm_cache_load_excl(cache, cb, step_reg, xa, -1);
        if (xb < 0) return (size_t)-1;
        int xc = xmm_cache_load_excl(cache, cb, var_reg, xa, xb);
        if (xc < 0) return (size_t)-1;

        if (!iter_in_xmm) {
            emit_movsd_load(cb, 7, slot_disp(info->nregs));       // xmm7 = iterator
        }
        emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
        emit_u8(cb, 0xC0 | (7 << 3) | xa);                   // ucomisd xmm7, xa
        emit_u8(cb, 0x0F); emit_u8(cb, 0x87);                // ja exit
        exit_patch = cb->len;
        emit_i32(cb, 0);

        emit_sse66_rr(cb, 0x28, xc, 7);                      // movapd xc, xmm7 (R[var] = c)
        xmm_cache_put(cache, xc, var_reg);                   // var became dirty

        emit_sse_arith_rr(cb, 0x58, 7, xb);                  // addsd xmm7, xb (step)
        if (!iter_in_xmm) {
            emit_movsd_store(cb, 7, slot_disp(info->nregs)); // store iterator
        }
    } else {
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];
        int b = entry_inst->operands[2];
        int xa = xmm_cache_load(cache, cb, a);
        if (xa < 0) return (size_t)-1;
        int xb = xmm_cache_load_excl(cache, cb, b, xa, -1);
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
static bool emit_loop(JITContext* ctx, CodeBuf* cb, JitLoopInfo* info) {
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
    size_t scratch_size = (size_t)range_size * 256 + 1024;
    uint8_t* scratch_buf = (uint8_t*)malloc(scratch_size);
    if (!scratch_buf) return false;
    CodeBuf scratch = { scratch_buf, 0, scratch_size };

    XmmCache cache_start;
    xmm_cache_clear(&cache_start);
    bool converged = false;
    for (int iter = 0; iter < 8; iter++) {
        XmmCache cache = cache_start;
        scratch.len = 0;
        size_t patch = emit_loop_iteration(ctx, &scratch, &cache, info, const_slot, iter_in_xmm);
        if (patch == (size_t)-1) { free(scratch_buf); return false; }
        if (xmm_cache_eq(&cache, &cache_start)) { converged = true; break; }
        cache_start = cache;
    }
    free(scratch_buf);
    if (!converged) return false;

    // real emit
    size_t mark = cb->len;

    emit_u8(cb, 0x55);                                       // push rbp
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xE5); // mov rbp, rsp

    int frame_size = 8 * frame_slots;
    if (frame_size % 16) frame_size = (frame_size + 15) & ~15;
    emit_u8(cb, 0x48); emit_u8(cb, 0x81); emit_u8(cb, 0xEC); // sub rsp, imm32
    emit_i32(cb, frame_size);

    emit_movabs_rax(cb, 0x3FF0000000000000ULL);              // rax = bits of 1.0
    emit_movq_xmm_rax(cb, XMM_SCRATCH);                      // xmm7 = 1.0
    emit_movsd_store(cb, XMM_SCRATCH, slot_disp(const_slot));// store 1.0 to reserved slot

    // copy live-in slots from regs[rdi] to stack
    uint64_t m = info->live_in;
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        emit_movsd_load_rdi(cb, 0, s * 8);                   // xmm0 = regs[s]
        emit_movsd_store(cb, 0, slot_disp(s));               // stack[s] = xmm0
    }

    if (info->is_for_next && !iter_in_xmm) {                 // seed iterator in memory
        int var_reg = ctx->chunk->code[entry].operands[0];
        emit_movsd_load(cb, 0, slot_disp(var_reg));
        emit_movsd_store(cb, 0, slot_disp(nregs));
    }

    // preload the fixpoint cache state
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = cache_start.reg_slot[i];
        if (s >= 0) emit_movsd_load(cb, i, slot_disp(s));
    }

    if (info->is_for_next && iter_in_xmm) {                  // load iterator directly to xmm7
        int var_reg = ctx->chunk->code[entry].operands[0];
        emit_movsd_load(cb, 7, slot_disp(var_reg));
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

    xmm_cache_flush(&cache, cb);                             // spill dirty slots to stack

    m = info->live_out;                                      // copy modified slots back
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        emit_movsd_load(cb, 0, slot_disp(s));
        emit_movsd_store_rdi(cb, 0, s * 8);
    }

    emit_u8(cb, 0xC9);                                       // leave
    emit_u8(cb, 0xC3);                                       // ret

    info->native_fn = (void (*)(uint64_t*))(cb->buf + mark);
    return true;
}

// backend dispatch table for x86-64
const JitBackend jit_backend_x86_64 = {
    .name = "x86-64",
    .bytes_per_instruction = 256,
    .emit_function = emit_function,
    .emit_loop = emit_loop,
};
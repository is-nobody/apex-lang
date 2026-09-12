// source/jit/jit.c
// Self-contained x86-64 JIT for Apex numeric-pure functions.
// Emits machine code directly into an mmap'd RWX buffer.
// Uses a register cache: xmm0..xmm6 hold the hottest slots during a
// basic block, avoiding load/store for every arithmetic operand.
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>

// growable byte buffer used during code emission
typedef struct {
    uint8_t* buf;  // destination buffer (mmap'd)
    size_t   len;  // bytes written so far
    size_t   cap;  // total buffer capacity
} CodeBuf;

// pending jump whose target is not yet known
typedef struct {
    size_t patch_at;   // offset in code buffer of the 4-byte rel32 placeholder
    int    target_pc;  // bytecode pc that the jump should resolve to
} JumpFixup;

// xmm0..xmm6 used as slot cache, xmm7 reserved as scratch
#define XMM_CACHE_REGS 7
#define XMM_SCRATCH    7

// upper bound on slots the register cache can track
#define JIT_MAX_SLOTS 256

// register cache state: which slot lives in which xmm, and which slots have been written since they were last flushed to memory
typedef struct {
    int  reg_slot[XMM_CACHE_REGS];  // xmm[i] holds slot reg_slot[i], or -1
    int  slot_reg[JIT_MAX_SLOTS];   // slot -> xmm index, or -1
    bool slot_dirty[JIT_MAX_SLOTS]; // slot written but not yet spilled
} XmmCache;

// per-chunk JIT state
struct JITContext {
    BytecodeChunk* chunk;  // bytecode being compiled
    int  func_count;       // number of functions in the chunk

    bool*  pure;           // per-function: numeric-pure?
    bool*  has_native;     // per-function: has native code emitted?
    bool*  returns_bool;   // per-function: return type is bool?
    int*   range_start;    // per-function: first bytecode pc
    int*   range_end;      // per-function: one past last bytecode pc

    void**   func_table;   // runtime slots for each compiled function
    uint8_t* code;         // executable code buffer
    size_t   code_size;    // size of that buffer

    int compiled_count;    // number of functions successfully emitted
};

// appends one byte to the code buffer
static inline void emit_u8 (CodeBuf* b, uint8_t  v) { b->buf[b->len++] = v; }

// appends a little-endian u32 to the code buffer
static inline void emit_u32(CodeBuf* b, uint32_t v) { memcpy(b->buf+b->len, &v, 4); b->len += 4; }

// appends a little-endian u64 to the code buffer
static inline void emit_u64(CodeBuf* b, uint64_t v) { memcpy(b->buf+b->len, &v, 8); b->len += 8; }

// appends a little-endian i32 to the code buffer
static inline void emit_i32(CodeBuf* b, int32_t  v) { memcpy(b->buf+b->len, &v, 4); b->len += 4; }

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

// checks if a function contains only numeric ops and in-range jumps
static void compute_function_range(BytecodeChunk* chunk, int func_idx,
                                   int* start, int* end) {
    *start = chunk->functions[func_idx].address;                 // entry point
    if (func_idx == 0) { *end = *start; return; }                // entry fn never JITted

    int prev = *start - 1;                                       // instruction before entry
    if (prev >= 0 && prev < chunk->code_count &&
        chunk->code[prev].opcode == OP_JUMP) {                   // jump-over marker present
        int target = chunk->code[prev].operands[0];              // jump target = range end
        if (target > *start && target <= chunk->code_count) {
            *end = target;                                       // accept as range end
            return;
        }
    }
    if (func_idx + 1 < chunk->func_count)                        // fallback: to next fn
        *end = chunk->functions[func_idx + 1].address;
    else
        *end = chunk->code_count;                                // last fn: to end of code
}

// checks if a function contains only numeric ops and in-range jumps
static bool function_is_initially_pure(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];                      // first pc of this fn
    int end   = ctx->range_end[func_idx];                        // one past last pc
    if (start >= end) return false;                              // empty range

    for (int pc = start; pc < end; pc++) {                       // scan every instruction
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_MOVE:                                        // trivial moves
            case OP_LOAD_NUM_IMM:                                // small int literals
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_NEG: case OP_INC: case OP_DEC:               // arithmetic
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // numeric compares
            case OP_CMP_EQ:     case OP_CMP_NEQ:                 // generic compares
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
            case OP_RETURN: case OP_RETURN_NUM:                  // returns
                break;                                           // always allowed

            case OP_LOAD_NUM: {                                  // only numeric constants
                int idx = inst->operands[1];                     // constant pool index
                if (idx < 0 || idx >= chunk->const_count) return false;        // out of range
                if (chunk->constants[idx].type != CONST_NUMBER) return false;  // non-numeric
                break;
            }

            case OP_JUMP:                                        // all jumps must stay
            case OP_JUMP_IF_FALSE:                               // within this function
            case OP_JUMP_IF_EQ:     case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE: {
                int target = inst->operands[0];                     // jump target pc
                if (target < start || target >= end) return false;  // leaves the function
                break;
            }

            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:  // call targets validated
                break;                                       // by the fixpoint pass

            default:
                return false;                                // any other op: reject
        }
    }
    return true;
}

// checks if every return site in a function yields a boolean value
#define JIT_MAX_REGS_SCAN 512

static bool infer_returns_bool(BytecodeChunk* chunk, int start, int end) {
    bool is_bool[JIT_MAX_REGS_SCAN];                             // bool-flag per register
    memset(is_bool, 0, sizeof(is_bool));                         // no regs are bool initially

    bool any_bool = false;                                       // saw at least one bool return
    bool any_num  = false;                                       // saw at least one num return

    for (int pc = start; pc < end; pc++) {                       // walk the function body
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // destination register
        int a = inst->operands[1];                               // first source register

        switch (inst->opcode) {
            case OP_CMP_EQ:     case OP_CMP_NEQ:                 // all comparisons produce
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:             // a bool
            case OP_CMP_LT: case OP_CMP_GT:
            case OP_CMP_LTE: case OP_CMP_GTE:
                if (d < JIT_MAX_REGS_SCAN) is_bool[d] = true;    // dest holds bool
                break;

            case OP_MOVE:                                        // move preserves bool-ness
                if (d < JIT_MAX_REGS_SCAN)                       // dest in range
                    is_bool[d] = (a < JIT_MAX_REGS_SCAN) ? is_bool[a] : false;    // copy flag
                break;

            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:              // numeric producers
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_NEG: case OP_INC: case OP_DEC:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:      // treated as numeric here
                if (d < JIT_MAX_REGS_SCAN) is_bool[d] = false;   // dest is not bool
                break;

            case OP_RETURN:                                      // record return site's kind
                if (d < JIT_MAX_REGS_SCAN && is_bool[d]) any_bool = true;    // bool return
                else                                     any_num  = true;    // numeric return
                break;

            case OP_RETURN_NUM:                                  // explicitly numeric
                any_num = true;                                  // mark as numeric
                break;

            default: break;                                      // other ops don't matter
        }
    }

    return any_bool && !any_num;                                 // all returns are bool
}

// runs the full analysis: ranges, purity fixpoint, bool-return detection
static void analyze(JITContext* ctx) {
    BytecodeChunk* chunk = ctx->chunk;
    int n = ctx->func_count;                                     // number of functions

    for (int i = 0; i < n; i++)                                  // ranges for every function
        compute_function_range(chunk, i, &ctx->range_start[i], &ctx->range_end[i]);

    for (int i = 1; i < n; i++)                                  // initial purity
        ctx->pure[i] = function_is_initially_pure(ctx, i);       // fn 0 never JITted

    bool changed = true;                                         // purity fixpoint
    while (changed) {
        changed = false;
        for (int i = 1; i < n; i++) {                            // check each function
            if (!ctx->pure[i]) continue;                         // already rejected
            for (int pc = ctx->range_start[i]; pc < ctx->range_end[i]; pc++) {
                Instruction* inst = &chunk->code[pc];
                if (inst->opcode == OP_CALL_0 ||                 // check every call's target
                    inst->opcode == OP_CALL_1 ||
                    inst->opcode == OP_CALL_2) {
                    int target = inst->operands[1];              // callee function index
                    if (target < 0 || target >= n || !ctx->pure[target]) {
                        ctx->pure[i] = false;                    // callee not pure
                        changed = true;                          // need another pass
                        break;
                    }
                }
            }
        }
    }
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

// emits native x86-64 code for a single numeric-pure function
static bool emit_function(JITContext* ctx, CodeBuf* cb, int func_idx) {
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

    if (cb->len + (size_t)range_size * 200 + 512 > cb->cap)      // conservative size check
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
                emit_sse_arith_rr(cb, arith_op, xa, xb);         // xa op= xb
                xmm_cache_put(&cache, xa, d);                    // relabel xa as dest
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
                xmm_cache_flush(&cache, cb);                     // spill before call
                emit_call_func(cb, ctx, a);                      // result in xmm0
                xmm_cache_put(&cache, 0, d);                     // xmm0 = result
                break;
            }
            case OP_CALL_1: {                                    // call with one arg
                xmm_cache_flush(&cache, cb);                     // spill before call
                emit_movsd_load(cb, 0, slot_disp(b));            // arg in xmm0
                emit_call_func(cb, ctx, a);                      // result in xmm0
                xmm_cache_put(&cache, 0, d);                     // xmm0 = result
                break;
            }
            case OP_CALL_2: {                                    // call with two args
                xmm_cache_flush(&cache, cb);                     // spill before call
                emit_movsd_load(cb, 0, slot_disp(b));            // arg0 in xmm0
                emit_movsd_load(cb, 1, slot_disp(b + 1));        // arg1 in xmm1
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
            return false;                                        // invalid target
        }
        int32_t rel = (int32_t)label_off[tidx] - (int32_t)(fx->patch_at + 4);  // rel32 distance
        memcpy(cb->buf + fx->patch_at, &rel, 4);                 // write rel32
    }

    free(is_target);                                             // release target marks
    free(label_off);                                             // release label array
    free(fixups);                                                // release fixup array
    return true;                                                 // emission successful
}

// compiles all numeric-pure functions and returns a jit context (or null)
JITContext* jit_create(BytecodeChunk* chunk) {
    if (!chunk || chunk->func_count <= 1) return NULL;           // nothing to compile

    JITContext* ctx = (JITContext*)calloc(1, sizeof(JITContext));  // zero-initialised state
    if (!ctx) return NULL;                                         // allocation failed
    ctx->chunk = chunk;                                            // remember bytecode
    ctx->func_count = chunk->func_count;                           // number of functions

    int n = ctx->func_count;                                     // shorthand
    ctx->pure         = (bool*)calloc(n, sizeof(bool));          // per-fn purity
    ctx->has_native   = (bool*)calloc(n, sizeof(bool));          // per-fn code ptr
    ctx->range_start  = (int*) calloc(n, sizeof(int));           // per-fn pc start
    ctx->range_end    = (int*) calloc(n, sizeof(int));           // per-fn pc end
    ctx->func_table   = (void**)calloc(n, sizeof(void*));        // per-fn runtime slot
    ctx->returns_bool = (bool*)calloc(n, sizeof(bool));          // per-fn bool flag

    if (!ctx->pure || !ctx->has_native || !ctx->range_start ||   // verify allocations
        !ctx->range_end || !ctx->func_table || !ctx->returns_bool) {
        jit_destroy(ctx);                                        // cleanup on failure
        return NULL;
    }

    analyze(ctx);                                                // purity + ranges

    for (int i = 1; i < n; i++) {                                // bool-return detection
        if (!ctx->pure[i]) continue;                             // skip non-pure
        ctx->returns_bool[i] = infer_returns_bool(
            chunk, ctx->range_start[i], ctx->range_end[i]);      // scan this function
    }

    int pure_count = 0;                                          // count candidates
    for (int i = 0; i < n; i++) if (ctx->pure[i]) pure_count++;  // tally pure fns
    if (pure_count == 0) { jit_destroy(ctx); return NULL; }      // nothing to emit

    size_t cap = (size_t)chunk->code_count * 200 + 16384;        // generous RW buffer
    ctx->code = (uint8_t*)mmap(NULL, cap, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);  // anonymous page
    if (ctx->code == MAP_FAILED) {                               // mmap failed
        ctx->code = NULL;                                        // clear pointer
        jit_destroy(ctx);                                        // cleanup
        return NULL;
    }
    ctx->code_size = cap;                                        // remember for munmap

    CodeBuf cb = { ctx->code, 0, cap };                          // code emission state

    for (int i = 0; i < n; i++) {                                // emit each pure fn
        if (!ctx->pure[i]) continue;                             // skip non-pure
        size_t mark = cb.len;                                    // rollback point
        if (!emit_function(ctx, &cb, i)) {                       // attempt emission
            cb.len = mark;                                       // rewind and skip
            continue;
        }
        ctx->func_table[i] = (void*)(ctx->code + mark);          // runtime entry
        ctx->has_native[i] = true;                               // mark as compiled
        ctx->compiled_count++;                                   // count successful
    }

    if (ctx->compiled_count == 0) { jit_destroy(ctx); return NULL; }  // nothing usable

    if (mprotect(ctx->code, ctx->code_size, PROT_READ | PROT_EXEC) != 0) {
        jit_destroy(ctx);                                        // flip RW -> RX failed
        return NULL;
    }
    __builtin___clear_cache((char*)ctx->code, (char*)ctx->code + cb.len);  // icache flush

    return ctx;                                                  // success
}

// releases all resources held by the JIT context, including the code page
void jit_destroy(JITContext* ctx) {
    if (!ctx) return;                                            // null guard
    if (ctx->code) munmap(ctx->code, ctx->code_size);            // release executable page
    free(ctx->pure);                                             // free purity flags
    free(ctx->has_native);                                       // free native flags
    free(ctx->range_start);                                      // free range starts
    free(ctx->range_end);                                        // free range ends
    free(ctx->func_table);                                       // free runtime slots
    free(ctx->returns_bool);                                     // free bool flags
    free(ctx);                                                   // free context itself
}

// returns true if function `func_idx` has a native implementation
bool jit_has_native(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return false;  // validate args
    return ctx->has_native[func_idx];                            // true if code was emitted
}

// returns true if function `func_idx` was inferred to return a bool
bool jit_returns_bool(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return false;  // validate args
    return ctx->returns_bool[func_idx];                          // true if returns bool
}

// invokes a compiled function taking no arguments
double jit_call_0(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return 0.0;  // validate args
    void* p = ctx->func_table[func_idx];                         // fetch native entry pointer
    if (!p) return 0.0;                                          // function was not compiled
    double (*fn)(void);                                          // declare fn-ptr type
    memcpy(&fn, &p, sizeof(void*));                              // avoid fn-ptr cast UB
    return fn();                                                 // call native code
}

// invokes a compiled function taking one double argument
double jit_call_1(JITContext* ctx, int func_idx, double a) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return 0.0;  // validate args
    void* p = ctx->func_table[func_idx];                         // fetch native entry pointer
    if (!p) return 0.0;                                          // function was not compiled
    double (*fn)(double);                                        // declare fn-ptr type
    memcpy(&fn, &p, sizeof(void*));                              // avoid fn-ptr cast UB
    return fn(a);                                                // call native code with arg
}

// invokes a compiled function taking two double arguments
double jit_call_2(JITContext* ctx, int func_idx, double a, double b) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return 0.0;  // validate args
    void* p = ctx->func_table[func_idx];                         // fetch native entry pointer
    if (!p) return 0.0;                                          // function was not compiled
    double (*fn)(double, double);                                // declare fn-ptr type
    memcpy(&fn, &p, sizeof(void*));                              // avoid fn-ptr cast UB
    return fn(a, b);                                             // call native code with args
}

// returns the number of functions that were successfully JIT-compiled
int jit_compiled_count(JITContext* ctx) {
    return ctx ? ctx->compiled_count : 0;                        // number of JITted functions
}
// source/core/jit.c
// Self-contained x86-64 JIT for Apex numeric-pure functions.
// Emits machine code directly into an mmap'd RWX buffer.
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
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, op);  // scalar-double opcode
    emit_u8(b, 0x85 | (xmm_dst << 3));                   // modrm with rbp base
    emit_i32(b, disp);                                   // displacement
}

// emits <op>sd xmm_dst, xmm_src — scalar-double op, register-register
static inline void emit_sse_arith_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, op);  // scalar-double opcode
    emit_u8(b, 0xC0 | (dst << 3) | src);                 // modrm, register-register
}

// emits packed-double register-register op (movapd, xorpd, etc)
static inline void emit_sse66_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0x66); emit_u8(b, 0x0F); emit_u8(b, op);  // packed-double opcode
    emit_u8(b, 0xC0 | (dst << 3) | src);                 // modrm, register-register
}

// emits movabs rax, imm64 — load 64-bit immediate into rax
static inline void emit_movabs_rax(CodeBuf* b, uint64_t v) {
    emit_u8(b, 0x48); emit_u8(b, 0xB8);  // rex.w + movabs opcode
    emit_u64(b, v);                      // 64-bit immediate
}

// emits mov rax, [rbp+disp32] — load slot into rax
static inline void emit_mov_rax_slot(CodeBuf* b, int32_t disp) {
    emit_u8(b, 0x48); emit_u8(b, 0x8B); emit_u8(b, 0x85);  // mov rax, [rbp+disp]
    emit_i32(b, disp);                                     // displacement
}

// emits mov [rbp+disp32], rax — store rax into slot
static inline void emit_mov_slot_rax(CodeBuf* b, int32_t disp) {
    emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0x85);  // mov [rbp+disp], rax
    emit_i32(b, disp);                                     // displacement
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

// loads slot <reg> into xmm0, then executes leave/ret epilogue
static void emit_return_slot(CodeBuf* cb, int reg) {
    emit_movsd_load(cb, 0, slot_disp(reg));                      // xmm0 = slot value
    emit_u8(cb, 0xC9);                                           // leave
    emit_u8(cb, 0xC3);                                           // ret
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

// emits ucomisd + jcc and returns the offset of the rel32 placeholder
static size_t emit_ucomisd_jcc(CodeBuf* cb, uint8_t jcc) {
    emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);  // ucomisd
    emit_u8(cb, 0x0F); emit_u8(cb, jcc);                         // conditional jump
    size_t patch_at = cb->len;                                   // remember fixup offset
    emit_i32(cb, 0);                                             // placeholder rel32
    return patch_at;                                             // return fixup offset
}

// converts the last ucomisd flags into 0.0/1.0 and stores into slot <d>
static void emit_setcc_to_slot(CodeBuf* cb, uint8_t setcc, int d) {
    emit_u8(cb, 0x0F); emit_u8(cb, setcc); emit_u8(cb, 0xC0);    // setcc al
    emit_u8(cb, 0x0F); emit_u8(cb, 0xB6); emit_u8(cb, 0xC0);     // movzbl eax, al
    emit_u8(cb, 0xF2); emit_u8(cb, 0x0F); emit_u8(cb, 0x2A); emit_u8(cb, 0xC0);  // cvtsi2sd
    emit_movsd_store(cb, 0, slot_disp(d));                       // store into slot
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

    int range_size = end - start;                                // number of bytecodes
    if (range_size <= 0) return false;                           // empty body

    if (cb->len + (size_t)range_size * 128 + 256 > cb->cap)      // conservative size check
        return false;                                            // (128 bytes per bytecode)

    int32_t* label_off = (int32_t*)malloc(sizeof(int32_t) * range_size);     // label offsets
    JumpFixup* fixups  = (JumpFixup*)malloc(sizeof(JumpFixup) * range_size); // pending jumps
    if (!label_off || !fixups) { free(label_off); free(fixups); return false; }  // allocation failed
    for (int i = 0; i < range_size; i++) label_off[i] = -1;      // no label emitted yet
    int fixup_count = 0;                                         // pending jumps count

    emit_u8(cb, 0x55);                                           // push rbp
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xE5);     // mov rbp, rsp

    int frame_size = 8 * nregs;                                  // bytes for slots
    if (frame_size % 16) frame_size = (frame_size + 15) & ~15;   // align to 16 bytes
    emit_u8(cb, 0x48); emit_u8(cb, 0x81); emit_u8(cb, 0xEC);     // sub rsp, imm32
    emit_i32(cb, frame_size);                                    // frame size immediate

    for (int i = 0; i < arity; i++) {                            // spill incoming args
        emit_movsd_store(cb, i, slot_disp(i));                   // xmm<i> -> slot i
    }

    for (int pc = start; pc < end; pc++) {                       // emit each bytecode
        label_off[pc - start] = (int32_t)cb->len;                // record label address

        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // first operand
        int a = inst->operands[1];                               // second operand
        int b = inst->operands[2];                               // third operand

        switch (inst->opcode) {
            case OP_MOVE: {                                      // reg-to-reg copy
                emit_mov_rax_slot(cb, slot_disp(a));             // rax = slot[a]
                emit_mov_slot_rax(cb, slot_disp(d));             // slot[d] = rax
                break;
            }
            case OP_LOAD_NUM_IMM: {                              // small int literal
                double v = (double)a;                            // operand as double
                uint64_t bits; memcpy(&bits, &v, 8);             // reinterpret as u64
                emit_movabs_rax(cb, bits);                       // rax = bit pattern
                emit_mov_slot_rax(cb, slot_disp(d));             // slot[d] = rax
                break;
            }
            case OP_LOAD_NUM: {                                  // full double constant
                double v = chunk->constants[a].number_value;     // fetch constant
                uint64_t bits; memcpy(&bits, &v, 8);             // reinterpret as u64
                emit_movabs_rax(cb, bits);                       // rax = bit pattern
                emit_mov_slot_rax(cb, slot_disp(d));             // slot[d] = rax
                break;
            }
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV: {                                       // binary arithmetic
                uint8_t op;                                      // sse opcode
                switch (inst->opcode) {
                    case OP_ADD: op = 0x58; break;               // addsd
                    case OP_SUB: op = 0x5C; break;               // subsd
                    case OP_MUL: op = 0x59; break;               // mulsd
                    default:     op = 0x5E; break;               // divsd
                }
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = slot[a]
                emit_sse_arith_mem(cb, op, 0, slot_disp(b));     // xmm0 op= slot[b]
                emit_movsd_store(cb, 0, slot_disp(d));           // slot[d] = xmm0
                break;
            }
            case OP_MOD: {                                       // x - trunc(x/y)*y
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                emit_sse66_rr(cb, 0x28, 2, 0);                   // movapd xmm2, xmm0
                emit_sse_arith_rr(cb, 0x5E, 2, 1);               // divsd  xmm2, xmm1
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B); emit_u8(cb, 0xDA); emit_u8(cb, 0x03);  // roundsd trunc
                emit_sse_arith_rr(cb, 0x59, 2, 1);               // mulsd  xmm2, xmm1
                emit_sse_arith_rr(cb, 0x5C, 0, 2);               // subsd  xmm0, xmm2
                emit_movsd_store(cb, 0, slot_disp(d));           // slot[d] = xmm0
                break;
            }
            case OP_NEG: {                                       // unary minus
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
                emit_u8(cb, 0x57); emit_u8(cb, 0xC0);            // xorpd xmm0, xmm0
                emit_sse_arith_mem(cb, 0x5C, 0, slot_disp(a));   // subsd xmm0, [a]
                emit_movsd_store(cb, 0, slot_disp(d));           // slot[d] = xmm0
                break;
            }
            case OP_INC:
            case OP_DEC: {                                       // in-place +1 / -1
                uint8_t op = (inst->opcode == OP_INC) ? 0x58 : 0x5C;  // addsd / subsd
                emit_movsd_load(cb, 0, slot_disp(d));            // xmm0 = slot[d]
                emit_movabs_rax(cb, 0x3FF0000000000000ULL);      // rax = 1.0 bits
                emit_u8(cb, 0x66); emit_u8(cb, 0x48);
                emit_u8(cb, 0x0F); emit_u8(cb, 0x6E); emit_u8(cb, 0xC8);  // movq xmm1, rax
                emit_sse_arith_rr(cb, op, 0, 1);                 // addsd/subsd xmm0, xmm1
                emit_movsd_store(cb, 0, slot_disp(d));           // slot[d] = xmm0
                break;
            }

            case OP_JUMP: {                                      // unconditional jump
                emit_u8(cb, 0xE9);                               // jmp rel32 opcode
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                break;
            }

            case OP_JUMP_IF_FALSE: {                             // branch when cond == 0
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = slot[a]
                emit_sse66_rr(cb, 0x57, 1, 1);                   // xorpd xmm1, xmm1
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                break;
            }

            case OP_JUMP_IF_EQ:
            case OP_JUMP_IF_EQ_NUM: {                            // branch if a == b
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                size_t pa = emit_ucomisd_jcc(cb, 0x84);          // je
                fixups[fixup_count].patch_at  = pa;              // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                break;
            }
            case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_NEQ_NUM: {                           // branch if a != b
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                size_t pa = emit_ucomisd_jcc(cb, 0x85);          // jne
                fixups[fixup_count].patch_at  = pa;              // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                break;
            }
            case OP_JUMP_IF_LT: {                                // branch if a < b
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                size_t pa = emit_ucomisd_jcc(cb, 0x82);          // jb
                fixups[fixup_count].patch_at  = pa;              // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                break;
            }
            case OP_JUMP_IF_GT: {                                // branch if a > b
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                size_t pa = emit_ucomisd_jcc(cb, 0x87);          // ja
                fixups[fixup_count].patch_at  = pa;              // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                break;
            }
            case OP_JUMP_IF_LTE: {                               // branch if a <= b
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                size_t pa = emit_ucomisd_jcc(cb, 0x86);          // jbe
                fixups[fixup_count].patch_at  = pa;              // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                break;
            }
            case OP_JUMP_IF_GTE: {                               // branch if a >= b
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                size_t pa = emit_ucomisd_jcc(cb, 0x83);          // jae
                fixups[fixup_count].patch_at  = pa;              // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                break;
            }

            case OP_CMP_EQ:
            case OP_CMP_EQ_NUM: {                                // d = (a == b)
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);
                emit_setcc_to_slot(cb, 0x94, d);                 // sete
                break;
            }
            case OP_CMP_NEQ:
            case OP_CMP_NEQ_NUM: {                               // d = (a != b)
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);
                emit_setcc_to_slot(cb, 0x95, d);                 // setne
                break;
            }
            case OP_CMP_LT: {                                    // d = (a < b)
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);
                emit_setcc_to_slot(cb, 0x92, d);                 // setb
                break;
            }
            case OP_CMP_GT: {                                    // d = (a > b)
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);
                emit_setcc_to_slot(cb, 0x97, d);                 // seta
                break;
            }
            case OP_CMP_LTE: {                                   // d = (a <= b)
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);
                emit_setcc_to_slot(cb, 0x96, d);                 // setbe
                break;
            }
            case OP_CMP_GTE: {                                   // d = (a >= b)
                emit_movsd_load(cb, 0, slot_disp(a));            // xmm0 = a
                emit_movsd_load(cb, 1, slot_disp(b));            // xmm1 = b
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC1);
                emit_setcc_to_slot(cb, 0x93, d);                 // setae
                break;
            }

            case OP_CALL_0: {                                    // call with no args
                emit_call_func(cb, ctx, a);                      // result in xmm0
                emit_movsd_store(cb, 0, slot_disp(d));           // slot[d] = xmm0
                break;
            }
            case OP_CALL_1: {                                    // call with one arg
                emit_movsd_load(cb, 0, slot_disp(b));            // xmm0 = slot[b]
                emit_call_func(cb, ctx, a);                      // result in xmm0
                emit_movsd_store(cb, 0, slot_disp(d));           // slot[d] = xmm0
                break;
            }
            case OP_CALL_2: {                                    // call with two args
                emit_movsd_load(cb, 0, slot_disp(b));            // xmm0 = slot[b]
                emit_movsd_load(cb, 1, slot_disp(b + 1));        // xmm1 = slot[b+1]
                emit_call_func(cb, ctx, a);                      // result in xmm0
                emit_movsd_store(cb, 0, slot_disp(d));           // slot[d] = xmm0
                break;
            }

            case OP_RETURN:                                      // return slot value
            case OP_RETURN_NUM:
                emit_return_slot(cb, d);                         // emit epilogue
                break;

            default:                                             // unreachable in pure fn
                emit_return_zero(cb);                            // safe fallback
                break;
        }
    }

    emit_return_zero(cb);                                        // safety fallthrough

    for (int i = 0; i < fixup_count; i++) {                      // patch every pending jump
        JumpFixup* fx = &fixups[i];
        int tidx = fx->target_pc - start;                        // index within range
        if (tidx < 0 || tidx >= range_size || label_off[tidx] < 0) {
            free(label_off); free(fixups);                       // free analysis arrays
            return false;                                        // invalid target
        }
        int32_t rel = (int32_t)label_off[tidx] - (int32_t)(fx->patch_at + 4);  // rel32 distance
        memcpy(cb->buf + fx->patch_at, &rel, 4);                 // write rel32
    }

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

    size_t cap = (size_t)chunk->code_count * 128 + 16384;        // generous RW buffer
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
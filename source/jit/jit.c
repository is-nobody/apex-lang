// source/jit/jit.c
// Self-contained x86-64 JIT for Apex numeric-pure functions
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit.h"
#include "vm.h"
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

// one numeric loop detected inside a function
typedef struct {
    int  entry_pc;      // first bytecode of the loop body
    int  back_edge_pc;  // JUMP returning to entry_pc
    int  exit_pc;       // bytecode reached when the loop exits
    bool is_for_next;   // true if the entry opcode is FOR_NEXT
    int  for_end_reg;   // FOR_NEXT: register holding the end bound
    int  for_step_reg;  // FOR_NEXT: register holding the step
    int  nregs;         // function frame size (max_registers)

    uint64_t live_in;   // bitmask of slots read inside the loop
    uint64_t live_out;  // bitmask of slots written inside the loop

    void (*native_fn)(uint64_t*);  // compiled entry, NULL if emit failed
} JitLoopInfo;

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

    JitLoopInfo* loops;        // dynamic array of native loops
    int          loop_count;   // number of live entries
    int          loop_capacity;// allocated capacity
    int*         pc_to_loop;   // code_count entries: -1 or index into loops[]
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

// checks whether an instruction is allowed inside a native loop body
static bool is_pure_loop_instr(BytecodeChunk* chunk, int pc) {
    Instruction* inst = &chunk->code[pc];
    switch (inst->opcode) {
        case OP_MOVE:
        case OP_LOAD_NUM_IMM:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_NEG: case OP_INC: case OP_DEC:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_EQ:     case OP_CMP_NEQ:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_JUMP:
        case OP_JUMP_IF_EQ:     case OP_JUMP_IF_NEQ:
        case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_FOR_NEXT:
            return true;
        case OP_LOAD_NUM: {
            int idx = inst->operands[1];
            return idx >= 0 && idx < chunk->const_count &&
                   chunk->constants[idx].type == CONST_NUMBER;
        }
        default:
            return false;
    }
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

// checks if an opcode can start a native loop
static bool is_loop_entry_op(Opcode op) {
    return op == OP_FOR_NEXT ||
           (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE);
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

// walks the loop body, marks read and written slots
static void analyze_loop_regs(JITContext* ctx, JitLoopInfo* info) {
    BytecodeChunk* chunk = ctx->chunk;
    uint64_t reads = 0, writes = 0;
    for (int pc = info->entry_pc; pc <= info->back_edge_pc; pc++) {
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];
        int a = inst->operands[1];
        int b = inst->operands[2];
        switch (inst->opcode) {
            case OP_MOVE: case OP_NEG:
                if (a >= 0 && a < 64) reads  |= 1ULL << a;
                if (d >= 0 && d < 64) writes |= 1ULL << d;
                break;
            case OP_INC: case OP_DEC:
                if (d >= 0 && d < 64) reads |= writes |= 1ULL << d;
                break;
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_CMP_EQ: case OP_CMP_NEQ:
            case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
            case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                if (a >= 0 && a < 64) reads  |= 1ULL << a;
                if (b >= 0 && b < 64) reads  |= 1ULL << b;
                if (d >= 0 && d < 64) writes |= 1ULL << d;
                break;
            case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
                if (d >= 0 && d < 64) writes |= 1ULL << d;
                break;
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_EQ_NUM: case OP_JUMP_IF_NEQ_NUM:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
                if (a >= 0 && a < 64) reads |= 1ULL << a;
                if (b >= 0 && b < 64) reads |= 1ULL << b;
                break;
            case OP_FOR_NEXT:
                if (d >= 0 && d < 64) reads |= writes |= 1ULL << d;
                if (info->for_end_reg  >= 0 && info->for_end_reg  < 64) reads |= 1ULL << info->for_end_reg;
                if (info->for_step_reg >= 0 && info->for_step_reg < 64) reads |= 1ULL << info->for_step_reg;
                break;
            case OP_JUMP:
                break;
            default: break;
        }
    }
    info->live_in  = reads;
    info->live_out = writes;
}

// registers a loop, running the slot analysis
static void add_loop(JITContext* ctx, int entry, int back_edge, int exit_pc,
                     bool is_for_next, int for_end_reg, int for_step_reg,
                     int nregs) {
    if (ctx->loop_count >= ctx->loop_capacity) {
        int new_cap = ctx->loop_capacity == 0 ? 8 : ctx->loop_capacity * 2;
        JitLoopInfo* new_arr = (JitLoopInfo*)realloc(ctx->loops, sizeof(JitLoopInfo) * new_cap);
        if (!new_arr) return;
        ctx->loops = new_arr;
        ctx->loop_capacity = new_cap;
    }
    JitLoopInfo* info = &ctx->loops[ctx->loop_count++];
    memset(info, 0, sizeof(*info));
    info->entry_pc = entry;
    info->back_edge_pc = back_edge;
    info->exit_pc = exit_pc;
    info->is_for_next = is_for_next;
    info->for_end_reg = for_end_reg;
    info->for_step_reg = for_step_reg;
    info->nregs = nregs;
    analyze_loop_regs(ctx, info);
}

// scans one function for numeric loops
static void detect_loops_in_function(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];
    int end   = ctx->range_end[func_idx];
    int nregs = chunk->functions[func_idx].max_registers;
    if (nregs < 1) nregs = 1;
    if (nregs > JIT_MAX_SLOTS - 2) return;

    for (int pc = start; pc < end; pc++) {                       // scan for back edges
        if (chunk->code[pc].opcode != OP_JUMP) continue;         // only plain JUMP
        int entry = chunk->code[pc].operands[0];                 // target pc
        if (entry >= pc || entry < start) continue;              // must be backward and in-range
        if (!is_loop_entry_op(chunk->code[entry].opcode)) continue;  // valid entry opcode

        Opcode entry_op = chunk->code[entry].opcode;
        int exit_pc;
        int for_end_reg = -1, for_step_reg = -1;
        if (entry_op == OP_FOR_NEXT) {
            if (chunk->code[entry].operands[2] != 0) continue;   // non-numeric for — reject
            exit_pc = chunk->code[entry].operands[1];
        } else {
            exit_pc = chunk->code[entry].operands[0];
        }
        if (exit_pc != pc + 1) continue;                         // exit must be right after back edge

        bool ok = true;
        for (int i = entry + 1; i < pc && ok; i++) {             // no other jumps in body
            Opcode op = chunk->code[i].opcode;
            if (op == OP_JUMP || op == OP_FOR_NEXT ||
                (op >= OP_JUMP_IF_EQ && op <= OP_JUMP_IF_GTE)) {
                ok = false;                                      // nested loop or internal branch
            }
        }
        if (!ok) continue;

        for (int i = entry; i <= pc && ok; i++) {                // every body instruction must be loop-pure
            if (!is_pure_loop_instr(chunk, i)) ok = false;
        }
        if (!ok) continue;

        if (entry_op == OP_FOR_NEXT) {                           // extra checks for FOR_NEXT
            int fi = entry - 1;
            if (fi < start) continue;
            if (chunk->code[fi].opcode != OP_FOR_INIT) continue;
            int var_reg  = chunk->code[fi].operands[0];
            int end_reg  = chunk->code[fi].operands[1];
            int step_reg = chunk->code[fi].operands[2];
            if (chunk->code[entry].operands[0] != var_reg) continue;
            int p = fi - 1;                                      // step must be provably positive
            if (p < start) continue;
            if (chunk->code[p].opcode != OP_LOAD_NUM_IMM) continue;
            if (chunk->code[p].operands[0] != step_reg) continue;
            if (chunk->code[p].operands[1] <= 0) continue;
            for_end_reg = end_reg;
            for_step_reg = step_reg;
        }

        add_loop(ctx, entry, pc, exit_pc,
                 entry_op == OP_FOR_NEXT, for_end_reg, for_step_reg, nregs);
    }
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

    for (int i = 1; i < n; i++) {                                // loop detection for non-pure fns
        if (ctx->pure[i]) continue;                              // pure fns are fully compiled
        detect_loops_in_function(ctx, i);
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

    int candidate_count = 0;
    for (int i = 0; i < n; i++) if (ctx->pure[i]) candidate_count++;
    if (candidate_count == 0 && ctx->loop_count == 0) {
        jit_destroy(ctx);
        return NULL;
    }

    size_t cap = (size_t)chunk->code_count * 256 + 16384;        // generous RW buffer
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

    for (int i = 0; i < ctx->loop_count; i++) {                  // emit each loop
        size_t mark = cb.len;
        if (!emit_loop(ctx, &cb, &ctx->loops[i])) {
            cb.len = mark;
            ctx->loops[i].native_fn = NULL;
            continue;
        }
        ctx->compiled_count++;
    }

    if (ctx->compiled_count == 0) { jit_destroy(ctx); return NULL; }  // nothing usable

    // build the pc → loop lookup table for loops that compiled successfully
    if (ctx->loop_count > 0) {
        ctx->pc_to_loop = (int*)malloc(sizeof(int) * chunk->code_count);
        if (ctx->pc_to_loop) {
            for (int i = 0; i < chunk->code_count; i++) ctx->pc_to_loop[i] = -1;
            for (int i = 0; i < ctx->loop_count; i++) {
                if (!ctx->loops[i].native_fn) continue;
                ctx->pc_to_loop[ctx->loops[i].entry_pc] = i;
            }
        }
    }

    if (mprotect(ctx->code, ctx->code_size, PROT_READ | PROT_EXEC) != 0) {
        jit_destroy(ctx);                                        // flip RW -> RX failed
        return NULL;
    }
    __builtin___clear_cache((char*)ctx->code, (char*)ctx->code + cb.len);  // icache flush

    if (getenv("APEX_JIT_DEBUG")) {
        fprintf(stderr, "[jit] functions: %d, loops: %d (native: %d)\n",
                n, ctx->loop_count, ctx->compiled_count);
        for (int i = 0; i < ctx->loop_count; i++) {
            fprintf(stderr, "  loop entry=%d back=%d exit=%d for_next=%d native=%p\n",
                    ctx->loops[i].entry_pc,
                    ctx->loops[i].back_edge_pc,
                    ctx->loops[i].exit_pc,
                    ctx->loops[i].is_for_next,
                    (void*)ctx->loops[i].native_fn);
        }
    }

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
    free(ctx->loops);                                            // free loop info array
    free(ctx->pc_to_loop);                                       // free pc->loop lookup
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

// if the current bytecode pc is a native loop entry and all live-in slots are numeric, runs the loop natively and writes the exit pc to *exit_pc
JitLoopResult jit_try_native_loop(JITContext* ctx, int pc, uint64_t* regs, int* exit_pc) {
    if (!ctx || !ctx->pc_to_loop) return JIT_LOOP_NOT_APPLICABLE;
    if (pc < 0 || pc >= ctx->chunk->code_count) return JIT_LOOP_NOT_APPLICABLE;
    int li = ctx->pc_to_loop[pc];
    if (li < 0) return JIT_LOOP_NOT_APPLICABLE;
    JitLoopInfo* info = &ctx->loops[li];
    if (!info->native_fn) return JIT_LOOP_NOT_APPLICABLE;

    uint64_t m = info->live_in;                              // type-check live-in slots
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) return JIT_LOOP_NOT_APPLICABLE;
        if (IS_NUMBER(regs[s])) continue;
        return JIT_LOOP_NOT_APPLICABLE;                      // not a number — fall back
    }

    info->native_fn(regs);                                   // run the native loop
    *exit_pc = info->exit_pc;
    return info->is_for_next ? JIT_LOOP_RAN_FOR_NEXT : JIT_LOOP_RAN_NORMAL;
}
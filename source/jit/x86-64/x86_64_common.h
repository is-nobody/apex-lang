// source/jit/x86-64/x86_64_common.h
// shared x86-64 sse emitters and XmmCache. no abi/os assumptions here.
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_JIT_X86_64_COMMON_H
#define APEX_JIT_X86_64_COMMON_H

#include "jit_internal.h"

// modrm base register indices
#define X86_RAX 0
#define X86_RCX 1
#define X86_RDX 2
#define X86_RBX 3
#define X86_RSP 4
#define X86_RBP 5
#define X86_RSI 6
#define X86_RDI 7

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
static inline int32_t x86_slot_disp(int reg) { return -8 * (reg + 1); }

// emits movsd xmm<N>, [rbp+disp32] — load slot into sse register
static inline void x86_emit_movsd_load(CodeBuf* b, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x10);  // movsd opcode
    emit_u8(b, 0x85 | (xmm << 3));                         // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits movsd [rbp+disp32], xmm<N> — store sse register into slot
static inline void x86_emit_movsd_store(CodeBuf* b, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x11);  // movsd store opcode
    emit_u8(b, 0x85 | (xmm << 3));                         // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits movsd xmm<N>, [base+disp32] — load from arbitrary base register (rdi/rcx)
static inline void x86_emit_movsd_load_base(CodeBuf* b, int base, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x10);  // movsd opcode
    emit_u8(b, 0x80 | (xmm << 3) | (base & 7));            // modrm with base register
    emit_i32(b, disp);                                     // displacement
}

// emits movsd [base+disp32], xmm<N> — store to arbitrary base register (rdi/rcx)
static inline void x86_emit_movsd_store_base(CodeBuf* b, int base, int xmm, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x11);  // movsd store opcode
    emit_u8(b, 0x80 | (xmm << 3) | (base & 7));            // modrm with base register
    emit_i32(b, disp);                                     // displacement
}

// emits <op>sd xmm_dst, [rbp+disp32] — scalar-double op with memory source
static inline void x86_emit_sse_arith_mem(CodeBuf* b, uint8_t op,
                                           int xmm_dst, int32_t disp) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, op);    // scalar-double opcode
    emit_u8(b, 0x85 | (xmm_dst << 3));                     // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits <op>sd xmm_dst, xmm_src — scalar-double op, register-register
static inline void x86_emit_sse_arith_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, op);    // scalar-double opcode
    emit_u8(b, 0xC0 | (dst << 3) | src);                   // modrm, register-register
}

// emits packed-double register-register op (movapd, xorpd, etc)
static inline void x86_emit_sse66_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0x66); emit_u8(b, 0x0F); emit_u8(b, op);    // packed-double opcode
    emit_u8(b, 0xC0 | (dst << 3) | src);                   // modrm, register-register
}

// emits movabs rax, imm64 — load 64-bit immediate into rax
static inline void x86_emit_movabs_rax(CodeBuf* b, uint64_t v) {
    emit_u8(b, 0x48); emit_u8(b, 0xB8);                    // rex.w + movabs opcode
    emit_u64(b, v);                                        // 64-bit immediate
}

// emits movq xmm<N>, rax — move rax into sse register
static inline void x86_emit_movq_xmm_rax(CodeBuf* b, int xmm) {
    emit_u8(b, 0x66); emit_u8(b, 0x48);                    // operand-size + rex.w
    emit_u8(b, 0x0F); emit_u8(b, 0x6E);                    // movq xmm, r/m64
    emit_u8(b, 0xC0 | (xmm << 3));                         // modrm, rm=rax
}

// clears cache state without touching memory
void x86_cache_clear(XmmCache* c);

// checks two cache states for structural equality
bool x86_cache_eq(const XmmCache* a, const XmmCache* b);

// finds or evicts an xmm register, avoiding two registers that are live
int  x86_cache_alloc_excl(XmmCache* c, CodeBuf* cb, int excl1, int excl2);

// returns an xmm holding slot s, loading from memory if needed
int  x86_cache_load_excl(XmmCache* c, CodeBuf* cb, int s, int excl1, int excl2);

// convenience wrapper around x86_cache_load_excl with no exclusions
int  x86_cache_load(XmmCache* c, CodeBuf* cb, int s);

// marks xmm x as holding slot s (dirty), invalidating prior mappings
void x86_cache_put(XmmCache* c, int x, int s);

// writes back all dirty slots to memory and clears the cache
void x86_cache_flush(XmmCache* c, CodeBuf* cb);

#endif // APEX_JIT_X86_64_COMMON_H
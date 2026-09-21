// source/jit/x86-64/x86_64_common.h
// Implementation of shared x86-64 SSE emitters and XmmCache for Apex language
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
#define X86_R8  8
#define X86_R9  9
#define X86_R10 10
#define X86_R11 11
#define X86_R12 12
#define X86_R13 13
#define X86_R14 14
#define X86_R15 15

// xmm0..xmm14 used as slot cache, xmm15 reserved as scratch
#define XMM_CACHE_REGS 15
#define XMM_SCRATCH    15

// nan-boxed NONE bit pattern (QNAN | TAG_NONE<<48)
#define X86_NONE_BITS  0x7FF8000000000000ULL

// nan-boxed MAKE_BOOL base pattern (QNAN | TAG_BOOL<<48); bit 0 carries the value
#define X86_BOOL_BITS  0x7FFA000000000000ULL

// emits a rex prefix byte with rex.r set when the sse reg field needs xmm8-xmm15
static inline void x86_rex_r(CodeBuf* b, int xmm) {
    if (xmm >= 8) emit_u8(b, 0x44);
}

// emits a rex prefix byte with rex.r/rex.b set for a register-register sse form
static inline void x86_rex_rb(CodeBuf* b, int dst, int src) {
    if (dst >= 8 || src >= 8)
        emit_u8(b, 0x40 | (dst >= 8 ? 0x04 : 0) | (src >= 8 ? 0x01 : 0));
}

// emits a rex prefix byte with rex.b set when the sse rm field needs xmm8-xmm15
static inline void x86_rex_b(CodeBuf* b, int xmm) {
    if (xmm >= 8) emit_u8(b, 0x41);
}

// emits ucomisd xmm_a, xmm_b — scalar-double unordered compare, register-register
static inline void x86_emit_ucomisd_rr(CodeBuf* b, int a, int bb) {
    emit_u8(b, 0x66);
    x86_rex_rb(b, a, bb);                                  // rex.r/rex.b for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x2E);                    // ucomisd opcode
    emit_u8(b, 0xC0 | ((a & 7) << 3) | (bb & 7));          // modrm, register-register
}

// emits ucomisd xmm_a, [rbp+disp32] — scalar-double unordered compare, memory source
static inline void x86_emit_ucomisd_mem(CodeBuf* b, int a, int32_t disp) {
    emit_u8(b, 0x66);
    x86_rex_r(b, a);                                       // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x2E);                    // ucomisd opcode
    emit_u8(b, 0x85 | ((a & 7) << 3));                     // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

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
    emit_u8(b, 0xF2);                                      // movsd legacy prefix
    x86_rex_r(b, xmm);                                     // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x10);                    // movsd opcode
    emit_u8(b, 0x85 | ((xmm & 7) << 3));                   // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits movsd [rbp+disp32], xmm<N> — store sse register into slot
static inline void x86_emit_movsd_store(CodeBuf* b, int xmm, int32_t disp) {
    emit_u8(b, 0xF2);                                      // movsd legacy prefix
    x86_rex_r(b, xmm);                                     // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x11);                    // movsd store opcode
    emit_u8(b, 0x85 | ((xmm & 7) << 3));                   // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits movsd xmm<N>, [base+disp32] — load from arbitrary base register (rdi/rcx/rbx)
static inline void x86_emit_movsd_load_base(CodeBuf* b, int base, int xmm, int32_t disp) {
    emit_u8(b, 0xF2);                                      // movsd legacy prefix
    x86_rex_r(b, xmm);                                     // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x10);                    // movsd opcode
    emit_u8(b, 0x80 | ((xmm & 7) << 3) | (base & 7));      // modrm with base register
    emit_i32(b, disp);                                     // displacement
}

// emits movsd [base+disp32], xmm<N> — store to arbitrary base register
static inline void x86_emit_movsd_store_base(CodeBuf* b, int base, int xmm, int32_t disp) {
    emit_u8(b, 0xF2);                                      // movsd legacy prefix
    x86_rex_r(b, xmm);                                     // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x11);                    // movsd store opcode
    emit_u8(b, 0x80 | ((xmm & 7) << 3) | (base & 7));      // modrm with base register
    emit_i32(b, disp);                                     // displacement
}

// emits movsd [base + index*8], xmm — SIB addressing with scale=8
static inline void x86_emit_movsd_store_idx8(CodeBuf* b, int base, int index, int xmm) {
    emit_u8(b, 0xF2);                                       // movsd legacy prefix
    x86_rex_r(b, xmm);                                      // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x11);                     // movsd store opcode
    emit_u8(b, ((xmm & 7) << 3) | 0x04);                    // modrm: mod=00, rm=SIB
    emit_u8(b, (3 << 6) | ((index & 7) << 3) | (base & 7)); // sib: scale=8
}

// emits <op>sd xmm_dst, [rbp+disp32] — scalar-double op with memory source
static inline void x86_emit_sse_arith_mem(CodeBuf* b, uint8_t op,
                                           int xmm_dst, int32_t disp) {
    emit_u8(b, 0xF2);                                      // scalar-double legacy prefix
    x86_rex_r(b, xmm_dst);                                 // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, op);                      // scalar-double opcode
    emit_u8(b, 0x85 | ((xmm_dst & 7) << 3));               // modrm with rbp base
    emit_i32(b, disp);                                     // displacement
}

// emits <op>sd xmm_dst, [rip+disp32]; returns the offset of the disp32 field
static inline size_t x86_emit_sse_arith_rip(CodeBuf* b, uint8_t op, int xmm_dst) {
    emit_u8(b, 0xF2);                                      // scalar-double legacy prefix
    x86_rex_r(b, xmm_dst);                                 // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, op);                      // scalar-double opcode
    emit_u8(b, 0x05 | ((xmm_dst & 7) << 3));               // modrm: mod=00, rm=101 (rip-relative)
    size_t at = b->len;                                    // offset of the disp32 field
    emit_i32(b, 0);                                        // placeholder, patched later
    return at;                                             // caller stores it in the fixup list
}

// emits movsd xmm<N>, [rip+disp32]; returns the offset of the disp32 field
static inline size_t x86_emit_movsd_load_rip(CodeBuf* b, int xmm) {
    emit_u8(b, 0xF2);                                      // movsd legacy prefix
    x86_rex_r(b, xmm);                                     // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x10);                    // movsd load opcode
    emit_u8(b, 0x05 | ((xmm & 7) << 3));                   // modrm: mod=00, rm=101 (rip-relative)
    size_t at = b->len;                                    // offset of the disp32 field
    emit_i32(b, 0);                                        // placeholder, patched later
    return at;                                             // caller stores it in the fixup list
}

// emits ucomisd xmm_a, [rip+disp32]; returns the offset of the disp32 field
static inline size_t x86_emit_ucomisd_rip(CodeBuf* b, int xmm_a) {
    emit_u8(b, 0x66);                                      // operand-size prefix
    x86_rex_r(b, xmm_a);                                   // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x2E);                    // ucomisd opcode
    emit_u8(b, 0x05 | ((xmm_a & 7) << 3));                 // modrm: mod=00, rm=101 (rip-relative)
    size_t at = b->len;                                    // offset of the disp32 field
    emit_i32(b, 0);                                        // placeholder, patched later
    return at;                                             // caller stores it in the fixup list
}

// emits <op>sd xmm_dst, xmm_src — scalar-double op, register-register
static inline void x86_emit_sse_arith_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0xF2);                                      // scalar-double legacy prefix
    x86_rex_rb(b, dst, src);                               // rex.r/rex.b for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, op);                      // scalar-double opcode
    emit_u8(b, 0xC0 | ((dst & 7) << 3) | (src & 7));       // modrm, register-register
}

// emits packed-double register-register op (movapd, xorpd, etc)
static inline void x86_emit_sse66_rr(CodeBuf* b, uint8_t op, int dst, int src) {
    emit_u8(b, 0x66);                                      // packed-double legacy prefix
    x86_rex_rb(b, dst, src);                               // rex.r/rex.b for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, op);                      // packed-double opcode
    emit_u8(b, 0xC0 | ((dst & 7) << 3) | (src & 7));       // modrm, register-register
}

// emits movabs rax, imm64 — load 64-bit immediate into rax
static inline void x86_emit_movabs_rax(CodeBuf* b, uint64_t v) {
    emit_u8(b, 0x48); emit_u8(b, 0xB8);                    // rex.w + movabs opcode
    emit_u64(b, v);                                        // 64-bit immediate
}

// emits movabs r8, imm64 — load 64-bit immediate into r8
static inline void x86_emit_movabs_r8(CodeBuf* b, uint64_t v) {
    emit_u8(b, 0x49); emit_u8(b, 0xB8);                    // rex.wb + movabs r8
    emit_u64(b, v);
}

// emits movabs r11, imm64 — load 64-bit immediate into r11 (caller-saved scratch)
static inline void x86_emit_movabs_r11(CodeBuf* b, uint64_t v) {
    emit_u8(b, 0x49); emit_u8(b, 0xBB);                    // rex.wb + movabs r11
    emit_u64(b, v);
}

// emits movq xmm<N>, rax — move rax into sse register
static inline void x86_emit_movq_xmm_rax(CodeBuf* b, int xmm) {
    emit_u8(b, 0x66);                                      // operand-size prefix
    emit_u8(b, xmm >= 8 ? 0x4C : 0x48);                    // rex.w (+rex.r for xmm8-xmm15)
    emit_u8(b, 0x0F); emit_u8(b, 0x6E);                    // movq xmm, r/m64
    emit_u8(b, 0xC0 | ((xmm & 7) << 3));                   // modrm, rm=rax
}

// emits movq rax, xmm<N> — move sse register into rax (bit-exact)
static inline void x86_emit_movq_rax_xmm(CodeBuf* b, int xmm) {
    emit_u8(b, 0x66);                                      // operand-size prefix
    emit_u8(b, xmm >= 8 ? 0x4C : 0x48);                    // rex.w (+rex.r for xmm8-xmm15)
    emit_u8(b, 0x0F); emit_u8(b, 0x7E);                    // movq r/m64, xmm
    emit_u8(b, 0xC0 | ((xmm & 7) << 3));                   // modrm, reg=xmm, rm=rax
}

// emits movq gpr, xmm<N> — copy an sse register into any gpr, bit-exact
static inline void x86_emit_movq_gpr_xmm(CodeBuf* b, int gpr, int xmm) {
    emit_u8(b, 0x66);                                      // operand-size prefix
    uint8_t rex = 0x48 | ((xmm >= 8) ? 0x04 : 0) | ((gpr >= 8) ? 0x01 : 0);
    emit_u8(b, rex);                                       // rex.w + rex.r + rex.b
    emit_u8(b, 0x0F); emit_u8(b, 0x7E);                    // movq r/m64, xmm
    emit_u8(b, 0xC0 | ((xmm & 7) << 3) | (gpr & 7));       // modrm, reg=xmm, rm=gpr
}

// emits movq xmm<N>, gpr — copy a gpr into any sse register, bit-exact
static inline void x86_emit_movq_xmm_gpr(CodeBuf* b, int xmm, int gpr) {
    emit_u8(b, 0x66);                                      // operand-size prefix
    uint8_t rex = 0x48 | ((xmm >= 8) ? 0x04 : 0) | ((gpr >= 8) ? 0x01 : 0);
    emit_u8(b, rex);                                       // rex.w + rex.r + rex.b
    emit_u8(b, 0x0F); emit_u8(b, 0x6E);                    // movq xmm, r/m64
    emit_u8(b, 0xC0 | ((xmm & 7) << 3) | (gpr & 7));       // modrm, reg=xmm, rm=gpr
}

// emits mov [rbp+disp32], r64 — store callee-saved gpr into stack slot
static inline void x86_emit_store_r64_rbp(CodeBuf* b, int reg, int32_t disp) {
    uint8_t rex = 0x48 | ((reg >= 8) ? 0x04 : 0);          // rex.w + rex.r
    emit_u8(b, rex); emit_u8(b, 0x89);                     // mov r/m64, r64
    emit_u8(b, 0x80 | ((reg & 7) << 3) | 5);               // modrm: mod=10, rm=rbp
    emit_i32(b, disp);
}

// emits mov r64, [rbp+disp32] — load callee-saved gpr from stack slot
static inline void x86_emit_load_r64_rbp(CodeBuf* b, int reg, int32_t disp) {
    uint8_t rex = 0x48 | ((reg >= 8) ? 0x04 : 0);
    emit_u8(b, rex); emit_u8(b, 0x8B);                     // mov r64, r/m64
    emit_u8(b, 0x80 | ((reg & 7) << 3) | 5);
    emit_i32(b, disp);
}

// emits mov r64, [base+disp32] — load 64-bit gpr from arbitrary base
static inline void x86_emit_load_r64_base(CodeBuf* b, int dst, int base, int32_t disp) {
    uint8_t rex = 0x48 | ((dst  >= 8) ? 0x04 : 0) | ((base >= 8) ? 0x01 : 0);
    emit_u8(b, rex); emit_u8(b, 0x8B);
    emit_u8(b, 0x80 | ((dst & 7) << 3) | (base & 7));
    emit_i32(b, disp);
}

// emits cvttsd2si eax, xmm — double to int32 truncation (upper 32 bits of rax zeroed)
static inline void x86_emit_cvttsd2si_eax(CodeBuf* b, int xmm) {
    emit_u8(b, 0xF2);                                      // cvttsd2si legacy prefix
    x86_rex_b(b, xmm);                                     // rex.b: xmm is in the rm field
    emit_u8(b, 0x0F); emit_u8(b, 0x2C);                    // cvttsd2si r32, xmm
    emit_u8(b, 0xC0 | (xmm & 7));                          // modrm: reg=eax(000), rm=xmm
}

// emits cvttsd2si edx, xmm — double to int32 truncation into edx
static inline void x86_emit_cvttsd2si_edx(CodeBuf* b, int xmm) {
    emit_u8(b, 0xF2);                                       // cvttsd2si legacy prefix
    x86_rex_b(b, xmm);                                      // rex.b: xmm is in the rm field
    emit_u8(b, 0x0F); emit_u8(b, 0x2C);                     // cvttsd2si r32, xmm
    emit_u8(b, 0xC0 | (2 << 3) | (xmm & 7));                // modrm: reg=edx(010), rm=xmm
}

// emits the shortest sequence that loads (double)imm into xmm<N>
static inline void x86_emit_load_double_imm(CodeBuf* b, int xmm, int32_t imm) {
    if (imm >= 0 && imm <= 255) {
        emit_u8(b, 0x31); emit_u8(b, 0xC0);              // xor eax, eax
        emit_u8(b, 0xB0); emit_u8(b, (uint8_t)imm);      // mov al, imm8
    } else {
        emit_u8(b, 0xB8); emit_u32(b, (uint32_t)imm);    // mov eax, imm32
    }
    emit_u8(b, 0xF2);                                      // cvtsi2sd legacy prefix
    x86_rex_r(b, xmm);                                     // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x2A);                    // cvtsi2sd opcode
    emit_u8(b, 0xC0 | ((xmm & 7) << 3));                   // modrm: reg=xmm, rm=eax
}

// emits dec edx — edx -= 1
static inline void x86_emit_dec_edx(CodeBuf* b) {
    emit_u8(b, 0xFF); emit_u8(b, 0xCA);                     // dec edx
}

// emits sub eax, imm8 — eax -= small immediate
static inline void x86_emit_sub_eax_imm8(CodeBuf* b, int8_t imm) {
    emit_u8(b, 0x83); emit_u8(b, 0xE8); emit_u8(b, (uint8_t)imm);
}

// emits dec eax — eax -= 1
static inline void x86_emit_dec_eax(CodeBuf* b) {
    emit_u8(b, 0xFF); emit_u8(b, 0xC8);
}

// emits shl rax, 16; shr rax, 16 — clear high 16 bits (nan-box pointer unpack)
static inline void x86_emit_clear_high16_rax(CodeBuf* b) {
    emit_u8(b, 0x48); emit_u8(b, 0xC1); emit_u8(b, 0xE0); emit_u8(b, 16);
    emit_u8(b, 0x48); emit_u8(b, 0xC1); emit_u8(b, 0xE8); emit_u8(b, 16);
}

// emits movsd xmm, [base + index*8] — SIB addressing with scale=8
static inline void x86_emit_movsd_load_idx8(CodeBuf* b, int xmm, int base, int index) {
    emit_u8(b, 0xF2);                                      // movsd legacy prefix
    x86_rex_r(b, xmm);                                     // rex.r for xmm8-xmm15
    emit_u8(b, 0x0F); emit_u8(b, 0x10);                    // movsd opcode
    emit_u8(b, ((xmm & 7) << 3) | 0x04);                   // modrm: mod=00, rm=SIB
    emit_u8(b, (3 << 6) | ((index & 7) << 3) | (base & 7));// sib: scale=8
}

// emits mov rax, [base + index*8] — 64-bit load via SIB scale=8
static inline void x86_emit_load_rax_idx8(CodeBuf* b, int base, int index) {
    emit_u8(b, 0x48); emit_u8(b, 0x8B);                    // mov rax, r/m64
    emit_u8(b, 0x04);                                       // modrm: reg=rax, rm=SIB
    emit_u8(b, (3 << 6) | ((index & 7) << 3) | (base & 7));
}

// emits cmp rax, r8
static inline void x86_emit_cmp_rax_r8(CodeBuf* b) {
    emit_u8(b, 0x4C); emit_u8(b, 0x39); emit_u8(b, 0xC0);
}

// emits cmp rax, rdx
static inline void x86_emit_cmp_rax_rdx(CodeBuf* b) {
    emit_u8(b, 0x48); emit_u8(b, 0x39); emit_u8(b, 0xD0);
}

// emits cmp rdx, r12 (r12 in reg field, rdx in r/m field)
static inline void x86_emit_cmp_rdx_r12d(CodeBuf* b) {
    emit_u8(b, 0x4C); emit_u8(b, 0x39); emit_u8(b, 0xE2);
}

// finishes a comparison: setcc al; movzbl eax, al; or rax, BOOL_BITS; movq xmm_dst, rax
// leaves xmm_dst holding a nan-boxed MAKE_BOOL matching the interpreter's encoding
static inline void x86_emit_cmp_box_result(CodeBuf* b, int xmm_dst, uint8_t setcc_op) {
    emit_u8(b, 0x0F); emit_u8(b, setcc_op); emit_u8(b, 0xC0);      // setcc al
    emit_u8(b, 0x0F); emit_u8(b, 0xB6); emit_u8(b, 0xC0);          // movzbl eax, al
    x86_emit_movabs_r11(b, X86_BOOL_BITS);                         // r11 = QNAN|TAG_BOOL
    emit_u8(b, 0x4C); emit_u8(b, 0x09); emit_u8(b, 0xD8);          // or rax, r11
    x86_emit_movq_xmm_rax(b, xmm_dst);                             // xmm_dst = nan-boxed bool
}

// returns a cache register to receive slot d's new value, preferring d's
// existing home so the fixpoint pass converges in a small number of steps
int  x86_cache_dest_reg(XmmCache* c, CodeBuf* cb, int d, int excl1, int excl2);

// clears cache state without touching memory
void x86_cache_clear(XmmCache* c);

// checks two cache states for structural equality
bool x86_cache_eq(const XmmCache* a, const XmmCache* b);

// finds or evicts an xmm register, avoiding two registers that are live
int  x86_cache_alloc_excl(XmmCache* c, CodeBuf* cb, int excl1, int excl2);

// finds or evicts the preferred register for `slot`, spilling if it holds a dirty slot
int  x86_cache_alloc_for(XmmCache* c, CodeBuf* cb, int slot, int excl1, int excl2);

// returns an xmm holding slot s, loading from memory if needed
int  x86_cache_load_excl(XmmCache* c, CodeBuf* cb, int s, int excl1, int excl2);

// convenience wrapper around x86_cache_load_excl with no exclusions
int  x86_cache_load(XmmCache* c, CodeBuf* cb, int s);

// returns the xmm currently holding slot s, or -1 if not cached
int  x86_cache_lookup(const XmmCache* c, int s);

// marks xmm x as holding slot s (dirty), invalidating prior mappings
void x86_cache_put(XmmCache* c, int x, int s);

// writes back all dirty slots to memory and clears the cache
void x86_cache_flush(XmmCache* c, CodeBuf* cb);

#endif // APEX_JIT_X86_64_COMMON_H
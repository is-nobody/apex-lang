// source/jit/jit_disasm.c
// Tiny x86-64 disassembler used by trace builds to show what the JIT emitted.
// Handles exactly the instruction subset the x86-64 backend produces.
// MIT license

#include "jit_disasm.h"
#include <stdio.h>
#include <string.h>

static const char* const R64[16] = {
    "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
    "r8","r9","r10","r11","r12","r13","r14","r15"
};
static const char* const R32[16] = {
    "eax","ecx","edx","ebx","esp","ebp","esi","edi",
    "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"
};
static const char* const R16[16] = {
    "ax","cx","dx","bx","sp","bp","si","di",
    "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w"
};
static const char* const R8[16] = {
    "al","cl","dl","bl","spl","bpl","sil","dil",
    "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"
};
static const char* const XMM[16] = {
    "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
    "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"
};
static const char* const JCC[16] = {
    "jo","jno","jb","jae","je","jne","jbe","ja",
    "js","jns","jp","jnp","jl","jge","jle","jg"
};
static const char* const SETCC[16] = {
    "seto","setno","setb","setae","sete","setne","setbe","seta",
    "sets","setns","setp","setnp","setl","setge","setle","setg"
};

// decoder state: cursor plus the currently active legacy/rex prefixes
typedef struct {
    const uint8_t* code;
    size_t len, pos;
    int rex_w, rex_r, rex_x, rex_b;
    int p66, pf2, pf3;
} D;

// reads one byte, advancing the cursor; returns -1 on eof
static int fetch_u8(D* d) {
    if (d->pos >= d->len) return -1;
    return d->code[d->pos++];
}

// reads one signed byte; returns 0 past eof
static int8_t fetch_i8(D* d) {
    int b = fetch_u8(d);
    return (int8_t)(b < 0 ? 0 : b);
}

// reads one little-endian i32; clamps the cursor to eof on truncation
static int32_t fetch_i32(D* d) {
    if (d->pos + 4 > d->len) { d->pos = d->len; return 0; }
    int32_t v; memcpy(&v, d->code + d->pos, 4); d->pos += 4; return v;
}

// reads one little-endian u64; clamps the cursor to eof on truncation
static uint64_t fetch_u64(D* d) {
    if (d->pos + 8 > d->len) { d->pos = d->len; return 0; }
    uint64_t v; memcpy(&v, d->code + d->pos, 8); d->pos += 8; return v;
}

// maps the reg field of group1 (0x80/0x81/0x83) to a mnemonic
static const char* group1_name(int reg) {
    switch (reg & 7) {
        case 0: return "add";
        case 1: return "or";
        case 2: return "adc";
        case 3: return "sbb";
        case 4: return "and";
        case 5: return "sub";
        case 6: return "xor";
        default: return "cmp";
    }
}

// decodes modrm + optional sib + disp; regnames is the register table used
// for the register-mode r/m operand, memory operands always use R64.
// writes the formatted r/m into rm_out, the reg field into *reg_field.
// returns 1 on success, 0 on truncation.
static int decode_modrm(D* d, const char* const* regnames,
                        char* rm_out, size_t rmsz, int* reg_field)
{
    int modrm = fetch_u8(d);                                 // modrm byte
    if (modrm < 0) return 0;
    int mod = (modrm >> 6) & 3;                              // mod field
    int reg = ((modrm >> 3) & 7) | (d->rex_r ? 8 : 0);       // reg field, rex.r extended
    int rm  = modrm & 7;                                     // r/m field
    if (reg_field) *reg_field = reg;
    if (mod == 3) {                                          // register-direct form
        int full = rm | (d->rex_b ? 8 : 0);                  // rex.b extended
        snprintf(rm_out, rmsz, "%s", regnames[full]);
        return 1;
    }

    char disp[24] = "", base[16] = "", index[24] = "";       // memory operand parts
    int has_index = 0, disp_kind = 0;                        // 1 = disp8, 4 = disp32
    if (rm == 4) {                                           // sib byte follows
        int sib = fetch_u8(d);
        if (sib < 0) return 0;
        int scale   = (sib >> 6) & 3;                        // scale = 1 << scale
        int raw_idx = (sib >> 3) & 7;                        // raw index field
        int idx     = raw_idx | (d->rex_x ? 8 : 0);          // rex.x extended index
        int raw_bs  = sib & 7;                               // raw base field
        int bs      = raw_bs | (d->rex_b ? 8 : 0);           // rex.b extended base
        if (raw_bs == 5 && mod == 0) disp_kind = 4;          // no base, disp32 only
        else snprintf(base, sizeof(base), "%s", R64[bs]);
        if (!(raw_idx == 4 && !d->rex_x)) {                  // raw idx 4 with no rex.x = no index
            has_index = 1;
            if (scale == 0) snprintf(index, sizeof(index), "%s", R64[idx]);
            else            snprintf(index, sizeof(index), "%s*%d", R64[idx], 1 << scale);
        }
    } else if (rm == 5 && mod == 0) {
        disp_kind = 4;                                       // rip-relative not produced, treat as disp32
    } else {
        int full = rm | (d->rex_b ? 8 : 0);                  // plain base register
        snprintf(base, sizeof(base), "%s", R64[full]);
    }
    if (mod == 1) disp_kind = 1;                             // disp8 follows
    else if (mod == 2) disp_kind = 4;                        // disp32 follows
    if (disp_kind == 4) { int32_t v = fetch_i32(d); snprintf(disp, sizeof(disp), "%d", v); }
    else if (disp_kind == 1) { int8_t v = fetch_i8(d); snprintf(disp, sizeof(disp), "%d", v); }

    char tmp[64] = "";                                       // assembled "[base+index+disp]"
    if (base[0]) strncat(tmp, base, sizeof(tmp) - strlen(tmp) - 1);
    if (has_index) {
        if (tmp[0]) strncat(tmp, "+", sizeof(tmp) - strlen(tmp) - 1);
        strncat(tmp, index, sizeof(tmp) - strlen(tmp) - 1);
    }
    if (disp[0]) {
        if (tmp[0] && disp[0] != '-') strncat(tmp, "+", sizeof(tmp) - strlen(tmp) - 1);
        strncat(tmp, disp, sizeof(tmp) - strlen(tmp) - 1);
    }
    snprintf(rm_out, rmsz, "[%s]", tmp);
    return 1;
}

// decodes one instruction at code+off; returns bytes consumed (>=1), 0 on failure
static int disasm_one(const uint8_t* code, size_t len, size_t off,
                      char* out, size_t outsz)
{
    D d = { code, len, off, 0, 0, 0, 0, 0, 0, 0 };           // fresh decoder state
    int op = -1;                                             // primary opcode byte
    for (;;) {                                               // consume legacy/rex prefixes
        op = fetch_u8(&d);
        if (op < 0) return 0;
        if (op == 0x66) { d.p66 = 1; continue; }             // operand-size prefix
        if (op == 0xF2) { d.pf2 = 1; continue; }             // repne / scalar-double prefix
        if (op == 0xF3) { d.pf3 = 1; continue; }             // rep / scalar-single prefix
        if (op >= 0x40 && op <= 0x4F) {                      // rex prefix
            d.rex_w = (op >> 3) & 1;                         // 64-bit operand size
            d.rex_r = (op >> 2) & 1;                         // reg field extension
            d.rex_x = (op >> 1) & 1;                         // sib index extension
            d.rex_b = op & 1;                                // r/m or sib base extension
            continue;
        }
        break;
    }

    const char* const* gpr = d.rex_w ? R64 : (d.p66 ? R16 : R32);  // default gpr table
    char rm[80];                                             // formatted r/m operand
    int regf = 0;                                            // decoded reg field

    switch (op) {
        case 0x55: snprintf(out, outsz, "push rbp"); return (int)(d.pos - off);
        case 0x5D: snprintf(out, outsz, "pop rbp");  return (int)(d.pos - off);
        case 0xC9: snprintf(out, outsz, "leave");    return (int)(d.pos - off);
        case 0xC3: snprintf(out, outsz, "ret");      return (int)(d.pos - off);
        case 0xCC: snprintf(out, outsz, "int3");     return (int)(d.pos - off);
        case 0x90: snprintf(out, outsz, "nop");      return (int)(d.pos - off);

        case 0xE8: { int32_t r = fetch_i32(&d);              // call rel32
            snprintf(out, outsz, "call 0x%zx", d.pos + (size_t)(intptr_t)r);
            return (int)(d.pos - off); }
        case 0xE9: { int32_t r = fetch_i32(&d);              // jmp rel32
            snprintf(out, outsz, "jmp 0x%zx", d.pos + (size_t)(intptr_t)r);
            return (int)(d.pos - off); }
        case 0xEB: { int8_t r = fetch_i8(&d);                // jmp rel8
            snprintf(out, outsz, "jmp 0x%zx", d.pos + (size_t)(intptr_t)r);
            return (int)(d.pos - off); }

        case 0x70: case 0x71: case 0x72: case 0x73:          // jcc rel8 family
        case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7E: case 0x7F: { int8_t r = fetch_i8(&d);
            snprintf(out, outsz, "%s 0x%zx", JCC[op - 0x70], d.pos + (size_t)(intptr_t)r);
            return (int)(d.pos - off); }

        case 0xB8: case 0xB9: case 0xBA: case 0xBB:          // mov r32/r64, imm
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            int i = (op - 0xB8) | (d.rex_b ? 8 : 0);         // rex.b extended destination
            if (d.rex_w) { uint64_t v = fetch_u64(&d);       // movabs r64, imm64
                snprintf(out, outsz, "movabs %s, 0x%llx", R64[i], (unsigned long long)v);
            } else { uint32_t v = (uint32_t)fetch_i32(&d);   // mov r32, imm32
                snprintf(out, outsz, "mov %s, 0x%x", gpr[i], v); }
            return (int)(d.pos - off); }
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:          // mov r8, imm8
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
            int i = (op - 0xB0) | (d.rex_b ? 8 : 0);
            int v = fetch_u8(&d);
            snprintf(out, outsz, "mov %s, 0x%x", R8[i], v);
            return (int)(d.pos - off); }
        case 0xA8: { int v = fetch_u8(&d);                   // test al, imm8
            snprintf(out, outsz, "test al, 0x%x", v);
            return (int)(d.pos - off); }

        case 0x31: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // xor r/m, r
            snprintf(out, outsz, "xor %s, %s", rm, gpr[regf]); return (int)(d.pos - off);
        case 0x33: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // xor r, r/m
            snprintf(out, outsz, "xor %s, %s", gpr[regf], rm); return (int)(d.pos - off);
        case 0x09: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // or r/m, r
            snprintf(out, outsz, "or %s, %s", rm, gpr[regf]); return (int)(d.pos - off);
        case 0x0B: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // or r, r/m
            snprintf(out, outsz, "or %s, %s", gpr[regf], rm); return (int)(d.pos - off);
        case 0x39: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // cmp r/m, r
            snprintf(out, outsz, "cmp %s, %s", rm, gpr[regf]); return (int)(d.pos - off);
        case 0x3B: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // cmp r, r/m
            snprintf(out, outsz, "cmp %s, %s", gpr[regf], rm); return (int)(d.pos - off);
        case 0x89: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // mov r/m, r
            snprintf(out, outsz, "mov %s, %s", rm, gpr[regf]); return (int)(d.pos - off);
        case 0x8B: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // mov r, r/m
            snprintf(out, outsz, "mov %s, %s", gpr[regf], rm); return (int)(d.pos - off);
        case 0x81: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // group1 r/m, imm32
            { int32_t v = fetch_i32(&d);
              snprintf(out, outsz, "%s %s, 0x%x", group1_name(regf), rm, (unsigned)v); }
            return (int)(d.pos - off);
        case 0x83: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // group1 r/m, imm8
            { int8_t v = fetch_i8(&d);
              snprintf(out, outsz, "%s %s, %d", group1_name(regf), rm, v); }
            return (int)(d.pos - off);
        case 0xC1: if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;  // shift r/m, imm8
            { int v = fetch_u8(&d); const char* n = "sh?";
              switch (regf & 7) { case 4: n="shl"; break; case 5: n="shr"; break; case 7: n="sar"; break; }
              snprintf(out, outsz, "%s %s, %d", n, rm, v); }
            return (int)(d.pos - off);
        case 0xFF: {                                         // group5: inc/dec/call/jmp/push
            int is64_call = 0;                               // rex.w widens call/jmp/inc/dec
            if (!decode_modrm(&d, gpr, rm, sizeof(rm), &regf)) return 0;
            const char* n = "??";
            switch (regf & 7) {
                case 0: n = "inc";  break;                   // increment r/m
                case 1: n = "dec";  break;                   // decrement r/m
                case 2: n = "call"; is64_call = 1; break;    // indirect call
                case 4: n = "jmp";  is64_call = 1; break;    // indirect jump
                case 6: n = "push"; break;                   // push r/m
            }
            if (is64_call && rm[0] != '[') {                 // register-direct indirect call/jmp
                // decode_modrm already formatted with the gpr table; re-emit with R64
                int modrm = d.code[off + 1];                 // best-effort: mod=3 rm byte
                (void)modrm;
                // rm already holds a register name from `gpr`; if it is a 32-bit
                // alias, prefer the 64-bit one so `call eax` becomes `call rax`
                for (int i = 0; i < 16; i++) {
                    if (strcmp(rm, R32[i]) == 0) { snprintf(rm, sizeof(rm), "%s", R64[i]); break; }
                    if (strcmp(rm, R16[i]) == 0) { snprintf(rm, sizeof(rm), "%s", R64[i]); break; }
                }
            }
            snprintf(out, outsz, "%s %s", n, rm);
            return (int)(d.pos - off); }
        default: break;
    }

    if (op != 0x0F) {                                        // not a two-byte opcode: raw byte
        snprintf(out, outsz, "db 0x%02x", op);
        return (int)(d.pos - off);
    }

    int op2 = fetch_u8(&d);                                  // second opcode byte
    if (op2 < 0) return 0;

    if (op2 >= 0x80 && op2 <= 0x8F) {                        // jcc rel32 family
        int32_t r = fetch_i32(&d);
        snprintf(out, outsz, "%s 0x%zx", JCC[op2 - 0x80], d.pos + (size_t)(intptr_t)r);
        return (int)(d.pos - off);
    }
    if (op2 >= 0x90 && op2 <= 0x9F) {                        // setcc r/m8
        if (!decode_modrm(&d, R8, rm, sizeof(rm), &regf)) return 0;
        snprintf(out, outsz, "%s %s", SETCC[op2 - 0x90], rm);
        return (int)(d.pos - off);
    }
    if (op2 == 0xB6) {                                       // movzx r32/r64, r/m8
        if (!decode_modrm(&d, R8, rm, sizeof(rm), &regf)) return 0;
        snprintf(out, outsz, "movzx %s, %s", gpr[regf], rm);
        return (int)(d.pos - off);
    }
    if (op2 == 0x10 || op2 == 0x11) {                        // movss/movsd/movups load or store
        const char* n = d.pf3 ? "movss" : d.pf2 ? "movsd" : "movups";
        if (!decode_modrm(&d, XMM, rm, sizeof(rm), &regf)) return 0;
        if (op2 == 0x11) snprintf(out, outsz, "%s %s, %s", n, rm, XMM[regf]);
        else             snprintf(out, outsz, "%s %s, %s", n, XMM[regf], rm);
        return (int)(d.pos - off);
    }
    if (op2 == 0x28 || op2 == 0x2E || op2 == 0x57 || op2 == 0x58 ||  // sse 66/f2 group
        op2 == 0x59 || op2 == 0x5C || op2 == 0x5E) {
        const char* n = "?";
        switch (op2) {
            case 0x28: n = d.p66 ? "movapd" : "movaps"; break;       // packed move
            case 0x2E: n = d.p66 ? "ucomisd" : "ucomiss"; break;     // unordered compare
            case 0x57: n = d.p66 ? "xorpd" : "xorps"; break;         // packed xor
            case 0x58: n = d.pf2 ? "addsd" : "addps"; break;         // add
            case 0x59: n = d.pf2 ? "mulsd" : "mulps"; break;         // multiply
            case 0x5C: n = d.pf2 ? "subsd" : "subps"; break;         // subtract
            case 0x5E: n = d.pf2 ? "divsd" : "divps"; break;         // divide
        }
        if (!decode_modrm(&d, XMM, rm, sizeof(rm), &regf)) return 0;
        snprintf(out, outsz, "%s %s, %s", n, XMM[regf], rm);
        return (int)(d.pos - off);
    }
    if (op2 == 0x2A) {                                       // cvtsi2sd/ss xmm, r/m
        const char* n = d.pf2 ? "cvtsi2sd" : "cvtsi2ss";
        const char* const* rn = d.rex_w ? R64 : R32;         // source width follows rex.w
        if (!decode_modrm(&d, rn, rm, sizeof(rm), &regf)) return 0;
        snprintf(out, outsz, "%s %s, %s", n, XMM[regf], rm);
        return (int)(d.pos - off);
    }
    if (op2 == 0x2C) {                                       // cvttsd2si r32/r64, xmm
        const char* n = d.pf2 ? "cvttsd2si" : "cvttss2si";
        const char* const* rn = d.rex_w ? R64 : R32;
        if (!decode_modrm(&d, XMM, rm, sizeof(rm), &regf)) return 0;
        snprintf(out, outsz, "%s %s, %s", n, rn[regf], rm);
        return (int)(d.pos - off);
    }
    if (op2 == 0x6E) {                                       // movq xmm, r/m
        const char* const* rn = d.rex_w ? R64 : R32;
        if (!decode_modrm(&d, rn, rm, sizeof(rm), &regf)) return 0;
        snprintf(out, outsz, "movq %s, %s", XMM[regf], rm);
        return (int)(d.pos - off);
    }
    if (op2 == 0x7E) {                                       // movq r/m, xmm
        const char* const* rn = d.rex_w ? R64 : R32;
        if (!decode_modrm(&d, rn, rm, sizeof(rm), &regf)) return 0;
        snprintf(out, outsz, "movq %s, %s", rm, XMM[regf]);
        return (int)(d.pos - off);
    }
    if (op2 == 0x3A) {                                       // three-byte opcode escape
        int op3 = fetch_u8(&d);
        if (op3 < 0) return 0;
        if (op3 == 0x0B) {                                   // roundsd xmm, xmm/m, imm8
            if (!decode_modrm(&d, XMM, rm, sizeof(rm), &regf)) return 0;
            int imm = fetch_u8(&d);
            snprintf(out, outsz, "roundsd %s, %s, 0x%x", XMM[regf], rm, imm);
            return (int)(d.pos - off);
        }
        snprintf(out, outsz, "db 0x0f, 0x3a, 0x%02x", op3);
        return (int)(d.pos - off);
    }
    snprintf(out, outsz, "db 0x0f, 0x%02x", op2);            // unknown 0f xx: raw
    return (int)(d.pos - off);
}

// prints an objdump-style hex+mnemonic listing of len bytes at code to stderr
void jit_disasm_dump(const char* label, const uint8_t* code, size_t len)
{
    if (label) fprintf(stderr, "%s\n", label);               // section header
    if (len == 0) { fprintf(stderr, "  (empty)\n"); return; }
    size_t off = 0;
    while (off < len) {
        char mnem[200];                                      // decoded instruction text
        int n = disasm_one(code, len, off, mnem, sizeof(mnem));
        if (n <= 0) n = 1;                                   // never advance by zero
        char hex[64];                                        // hex bytes column
        size_t hp = 0;
        int shown = n < 8 ? n : 8;                           // up to 8 bytes per line
        for (int i = 0; i < shown; i++)
            hp += (size_t)snprintf(hex + hp, sizeof(hex) - hp, "%02x ", code[off + i]);
        for (int i = shown; i < 8; i++)                      // pad short instructions
            hp += (size_t)snprintf(hex + hp, sizeof(hex) - hp, "   ");
        fprintf(stderr, "  %04zx: %-24s %s\n", off, hex, mnem);
        off += (size_t)n;                                    // advance by decoded length
    }
}
// source/jit/x86-64/x86_64_emit_int.c
// Int64-specialized function and loop emitters for the x86-64 JIT
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include "vm.h"
#include <stddef.h>
#include <string.h>

// checks whether a function is a self-recursive numeric function whose body
bool matches_int_self_recursive(JITContext* ctx, int func_idx) {
    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];
    int end   = ctx->range_end[func_idx];
    if (start >= end) return false;
    if (ctx->return_type[func_idx] != JIT_RET_NUMBER) return false;
    if (chunk->functions[func_idx].arity > 2) return false;  // rdi/rsi only

    bool has_self_call = false;                              // needs at least one self-call
    for (int pc = start; pc < end; pc++) {
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_RETURN: case OP_RETURN_NUM: case OP_RETURN_NUM_IMM:
                break;                                       // numeric return: fine
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2: {
                if (inst->operands[1] != func_idx) return false;  // only self-calls
                has_self_call = true;
                break;
            }
            default:
                if (!op_is_int_safe(inst->opcode)) return false;
                break;
        }
    }
    return has_self_call;
}

// emits an int64 version of a self-recursive numeric function; internal
bool x86_64_emit_int_self_recursive(const X86_64Abi* abi, JITContext* ctx,
                                    CodeBuf* cb, int func_idx,
                                    size_t* out_int_off) {
    static const int arg_gprs_sysv[2]  = { X86_RDI, X86_RSI };
    static const int arg_gprs_win64[2] = { X86_RCX, X86_RDX };
    const int* arg_gprs = (abi->frame_reg == X86_RCX)
                        ? arg_gprs_win64 : arg_gprs_sysv;
    static const int cs_gprs[5]  = { X86_RBX, X86_R12, X86_R13, X86_R14, X86_R15 };
    static const int scratch_gprs[7] = {
        X86_RAX, X86_RCX, X86_RDX, X86_R8, X86_R9, X86_R10, X86_R11
    };

    BytecodeChunk* chunk = ctx->chunk;
    int start = ctx->range_start[func_idx];
    int end   = ctx->range_end[func_idx];
    int arity = chunk->functions[func_idx].arity;
    int nregs = chunk->functions[func_idx].max_registers;
    if (nregs < 1) nregs = 1;
    int range_size = end - start;

    // find slots live across any recursive call; arg slots are always live
    uint64_t live_across = 0;
    for (int i = 0; i < arity; i++) live_across |= 1ULL << i;
    for (int pc = start; pc < end; pc++) {
        Opcode op = chunk->code[pc].opcode;
        if (op != OP_CALL_0 && op != OP_CALL_1 && op != OP_CALL_2) continue;
        int d = chunk->code[pc].operands[0];
        for (int s = 0; s < nregs; s++) {
            if (s == d) continue;
            if (live_across & (1ULL << s)) continue;
            if (slot_read_before_write(chunk, pc + 1, end, s))
                live_across |= 1ULL << s;
        }
    }
    if (__builtin_popcountll(live_across) > 5) return false;

    int slot_gpr[JIT_MAX_SLOTS];
    for (int s = 0; s < JIT_MAX_SLOTS; s++) slot_gpr[s] = -1;

    uint64_t m = live_across;
    int cs_idx = 0;
    while (m) {
        int s = __builtin_ctzll(m); m &= m - 1;
        if (s >= nregs) return false;
        slot_gpr[s] = cs_gprs[cs_idx++];
    }

    // collect every slot the body touches; any not yet assigned gets a
    uint64_t all_slots = 0;
    for (int pc = start; pc < end; pc++) {
        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0], a = inst->operands[1], b = inst->operands[2];
        Opcode op = inst->opcode;
        if (d >= 0 && d < nregs) all_slots |= 1ULL << d;
        switch (op) {
            case OP_MOVE: case OP_NEG:
            case OP_ADD: case OP_SUB: case OP_MUL:
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_INC: case OP_DEC:
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
            case OP_CALL_1: case OP_CALL_2:
                if (a >= 0 && a < nregs) all_slots |= 1ULL << a;
                break;
            default: break;
        }
        if ((op == OP_ADD || op == OP_SUB || op == OP_MUL ||
             op == OP_JUMP_IF_EQ || op == OP_JUMP_IF_NEQ ||
             op == OP_JUMP_IF_LT || op == OP_JUMP_IF_GT ||
             op == OP_JUMP_IF_LTE || op == OP_JUMP_IF_GTE) &&
            b >= 0 && b < nregs) {
            all_slots |= 1ULL << b;
        }
        if (op == OP_CALL_2 && b + 1 >= 0 && b + 1 < nregs)
            all_slots |= 1ULL << (b + 1);
    }

    uint64_t rem = all_slots & ~live_across;
    int scr_idx = 0;
    while (rem) {
        int s = __builtin_ctzll(rem); rem &= rem - 1;
        if (s >= nregs) return false;
        if (scr_idx >= 7) return false;
        slot_gpr[s] = scratch_gprs[scr_idx++];
    }

    size_t int_off = cb->len;
    *out_int_off = int_off;

    // generic pre-prologue base case: any int-safe JUMP_IF_*_IMM R0, imm,
    bool base_at_top = false;
    Opcode guard_op  = 0;
    int32_t guard_imm = 0;
    bool base_returns_imm = false;
    int32_t base_imm = 0;
    if (arity >= 1 && end - start >= 3) {
        Instruction* i0 = &chunk->code[start];
        Instruction* i1 = &chunk->code[start + 1];
        if (i0->operands[0] == start + 2 && i0->operands[1] == 0) {
            switch (i0->opcode) {
                case OP_JUMP_IF_EQ_IMM:
                case OP_JUMP_IF_NEQ_IMM:
                case OP_JUMP_IF_LT_IMM:
                case OP_JUMP_IF_GT_IMM:
                case OP_JUMP_IF_LTE_IMM:
                case OP_JUMP_IF_GTE_IMM:
                    guard_op  = i0->opcode;
                    guard_imm = i0->operands[2];
                    if (i1->opcode == OP_RETURN && i1->operands[0] == 0) {
                        base_at_top = true;                       // return R0
                    } else if (i1->opcode == OP_RETURN_NUM_IMM) {
                        base_at_top = true;                       // return c
                        base_returns_imm = true;
                        base_imm = i1->operands[1];
                    }
                    break;
                default: break;
            }
        }
    }

    JumpFixup* fixups  = ctx->scratch_fixups;
    int32_t*   lbl_off = ctx->scratch_label_off;
    if (!fixups || !lbl_off) { cb->len = int_off; return false; }
    for (int i = 0; i < range_size; i++) lbl_off[i] = -1;
    int nfix = 0;

    if (base_at_top) {
        // arg0's home register depends on the ABI: rdi on sysv/macos,
        // rcx on win64. Use arg_gprs[0] instead of hardcoding rdi.
        int arg0 = arg_gprs[0];
        int cmpg = base_returns_imm ? arg0 : X86_RAX;
        if (!base_returns_imm) {
            uint8_t rex = 0x48 | (arg0 >= 8 ? 0x04 : 0);
            emit_u8(cb, rex); emit_u8(cb, 0x89);
            emit_u8(cb, 0xC0 | ((arg0 & 7) << 3) | X86_RAX);           // mov rax, arg0
        }
        if (guard_imm == 0) {
            uint8_t rex = 0x48 | (cmpg >= 8 ? 0x04 : 0) | (cmpg >= 8 ? 0x01 : 0);
            emit_u8(cb, rex); emit_u8(cb, 0x85);                    // test reg, reg
            emit_u8(cb, 0xC0 | ((cmpg & 7) << 3) | (cmpg & 7));
        } else if (guard_imm >= -128 && guard_imm <= 127) {
            emit_u8(cb, 0x48 | (cmpg >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x83);                                      // cmp reg, imm8
            emit_u8(cb, 0xF8 | (cmpg & 7));
            emit_u8(cb, (uint8_t)guard_imm);
        } else {
            emit_u8(cb, 0x48 | (cmpg >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x81);                                      // cmp reg, imm32
            emit_u8(cb, 0xF8 | (cmpg & 7));
            emit_i32(cb, guard_imm);
        }
        uint8_t jcc;
        switch (guard_op) {
            case OP_JUMP_IF_EQ_IMM:  jcc = 0x84; break;             // je
            case OP_JUMP_IF_NEQ_IMM: jcc = 0x85; break;             // jne
            case OP_JUMP_IF_LT_IMM:  jcc = 0x8C; break;             // jl
            case OP_JUMP_IF_GT_IMM:  jcc = 0x8F; break;             // jg
            case OP_JUMP_IF_LTE_IMM: jcc = 0x8E; break;             // jle
            default:                 jcc = 0x8D; break;             // jge
        }
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                        // jcc general
        if (nfix >= range_size) { cb->len = int_off; return false; }
        fixups[nfix].patch_at  = cb->len;
        fixups[nfix].target_pc = start + 2;
        nfix++;
        emit_i32(cb, 0);
        if (base_returns_imm) {
            if (base_imm == 0) {
                emit_u8(cb, 0x31); emit_u8(cb, 0xC0);               // xor eax, eax
            } else {
                emit_u8(cb, 0xB8); emit_u32(cb, (uint32_t)base_imm); // mov eax, imm32
            }
        }
        emit_u8(cb, 0xC3);                                          // ret
    }

    if (!base_at_top) {
        for (int i = 0; i < cs_idx; i++) {                          // push cs regs
            int g = cs_gprs[i];
            if (g >= 8) { emit_u8(cb, 0x41); emit_u8(cb, 0x50 | (g & 7)); }
            else        { emit_u8(cb, 0x50 | g); }
        }
        for (int i = 0; i < arity; i++) {                           // args into slot gprs
            int g = slot_gpr[i], ag = arg_gprs[i];
            if (g == ag) continue;
            emit_u8(cb, 0x48 | (ag >= 8 ? 0x04 : 0) | (g >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x89);
            emit_u8(cb, 0xC0 | ((ag & 7) << 3) | (g & 7));
        }
    }

    for (int pc = start; pc < end; pc++) {
        if (base_at_top && (pc == start || pc == start + 1)) continue;
        lbl_off[pc - start] = (int32_t)cb->len;                     // label at prologue
        if (base_at_top && pc == start + 2) {                       // general path start
            for (int i = 0; i < cs_idx; i++) {
                int g = cs_gprs[i];
                if (g >= 8) { emit_u8(cb, 0x41); emit_u8(cb, 0x50 | (g & 7)); }
                else        { emit_u8(cb, 0x50 | g); }
            }
            for (int i = 0; i < arity; i++) {
                int g = slot_gpr[i], ag = arg_gprs[i];
                if (g == ag) continue;
                emit_u8(cb, 0x48 | (ag >= 8 ? 0x04 : 0) | (g >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x89);
                emit_u8(cb, 0xC0 | ((ag & 7) << 3) | (g & 7));
            }
        }

        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0], a = inst->operands[1], b = inst->operands[2];
        Opcode op = inst->opcode;

        switch (op) {
            case OP_MOVE: {
                if (d == a) break;
                int gd = slot_gpr[d], ga = slot_gpr[a];
                emit_u8(cb, 0x48 | (ga >= 8 ? 0x04 : 0) | (gd >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x89);
                emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
                break;
            }
            case OP_LOAD_NUM_IMM: {
                int gd = slot_gpr[d];
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                emit_u8(cb, 0xC7);
                emit_u8(cb, 0xC0 | (gd & 7));
                emit_i32(cb, a);
                break;
            }
            case OP_ADD: case OP_SUB: case OP_MUL: {
                int gd = slot_gpr[d], ga = slot_gpr[a], gb = slot_gpr[b];
                if (d != a) {
                    emit_u8(cb, 0x48 | (ga >= 8 ? 0x04 : 0) | (gd >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89);
                    emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
                }
                if (op == OP_MUL) {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x04 : 0) | (gb >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x0F); emit_u8(cb, 0xAF);
                    emit_u8(cb, 0xC0 | ((gd & 7) << 3) | (gb & 7));
                } else {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (gb >= 8 ? 0x04 : 0));
                    emit_u8(cb, op == OP_ADD ? 0x01 : 0x29);
                    emit_u8(cb, 0xC0 | ((gb & 7) << 3) | (gd & 7));
                }
                break;
            }
            case OP_ADD_IMM: case OP_SUB_IMM: {
                int gd = slot_gpr[d], ga = slot_gpr[a];
                if (d != a) {
                    emit_u8(cb, 0x48 | (ga >= 8 ? 0x04 : 0) | (gd >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89);
                    emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
                }
                int32_t k = b;
                if (k >= -128 && k <= 127) {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x83);
                    emit_u8(cb, (op == OP_ADD_IMM ? 0xC0 : 0xE8) | (gd & 7));
                    emit_u8(cb, (uint8_t)k);
                } else {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x81);
                    emit_u8(cb, (op == OP_ADD_IMM ? 0xC0 : 0xE8) | (gd & 7));
                    emit_i32(cb, k);
                }
                break;
            }
            case OP_MUL_IMM: {
                int gd = slot_gpr[d], ga = slot_gpr[a];
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x04 : 0) | (ga >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x69);
                emit_u8(cb, 0xC0 | ((gd & 7) << 3) | (ga & 7));
                emit_i32(cb, b);
                break;
            }
            case OP_INC: case OP_DEC: {
                int gd = slot_gpr[d];
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x83);
                emit_u8(cb, (op == OP_INC ? 0xC0 : 0xE8) | (gd & 7));
                emit_u8(cb, 0x01);
                break;
            }
            case OP_JUMP: {
                emit_u8(cb, 0xE9);
                if (nfix >= range_size) { cb->len = int_off; return false; }
                fixups[nfix].patch_at = cb->len;
                fixups[nfix].target_pc = d; nfix++;
                emit_i32(cb, 0);
                break;
            }
            case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
            case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE: {
                int ga = slot_gpr[a], gb = slot_gpr[b];
                emit_u8(cb, 0x48 | (ga >= 8 ? 0x04 : 0) | (gb >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x39);
                emit_u8(cb, 0xC0 | ((gb & 7) << 3) | (ga & 7));
                uint8_t jcc = op == OP_JUMP_IF_EQ  ? 0x84 :
                              op == OP_JUMP_IF_NEQ ? 0x85 :
                              op == OP_JUMP_IF_LT  ? 0x8C :
                              op == OP_JUMP_IF_GT  ? 0x8F :
                              op == OP_JUMP_IF_LTE ? 0x8E : 0x8D;
                emit_u8(cb, 0x0F); emit_u8(cb, jcc);
                if (nfix >= range_size) { cb->len = int_off; return false; }
                fixups[nfix].patch_at = cb->len;
                fixups[nfix].target_pc = d; nfix++;
                emit_i32(cb, 0);
                break;
            }
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM: {
                int ga = slot_gpr[a];
                emit_u8(cb, 0x48 | (ga >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x83);
                emit_u8(cb, 0xF8 | (ga & 7));
                emit_u8(cb, (uint8_t)b);
                uint8_t jcc = op == OP_JUMP_IF_EQ_IMM  ? 0x84 :
                              op == OP_JUMP_IF_NEQ_IMM ? 0x85 :
                              op == OP_JUMP_IF_LT_IMM  ? 0x8C :
                              op == OP_JUMP_IF_GT_IMM  ? 0x8F :
                              op == OP_JUMP_IF_LTE_IMM ? 0x8E : 0x8D;
                emit_u8(cb, 0x0F); emit_u8(cb, jcc);
                if (nfix >= range_size) { cb->len = int_off; return false; }
                fixups[nfix].patch_at = cb->len;
                fixups[nfix].target_pc = d; nfix++;
                emit_i32(cb, 0);
                break;
            }
            case OP_CALL_0: {
                emit_u8(cb, 0xE8);
                int32_t back = (int32_t)int_off - (int32_t)(cb->len + 4);
                emit_i32(cb, back);
                int gd = slot_gpr[d];
                if (gd != X86_RAX) {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | (gd & 7));
                }
                break;
            }
            case OP_CALL_1: {
                int gb = slot_gpr[b], arg0 = arg_gprs[0];
                if (gb != arg0) {
                    emit_u8(cb, 0x48 | (gb >= 8 ? 0x04 : 0) | (arg0 >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89);
                    emit_u8(cb, 0xC0 | ((gb & 7) << 3) | (arg0 & 7));
                }
                emit_u8(cb, 0xE8);
                int32_t back = (int32_t)int_off - (int32_t)(cb->len + 4);
                emit_i32(cb, back);
                int gd = slot_gpr[d];
                if (gd != X86_RAX) {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | (gd & 7));
                }
                break;
            }
            case OP_CALL_2: {
                int gb = slot_gpr[b], gb1 = slot_gpr[b + 1];
                int arg0 = arg_gprs[0], arg1 = arg_gprs[1];
                emit_u8(cb, 0x48 | (gb >= 8 ? 0x04 : 0) | 0x01);        // mov r10, gb
                emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((gb & 7) << 3) | 2);
                emit_u8(cb, 0x48 | (gb1 >= 8 ? 0x04 : 0) | 0x01);       // mov r11, gb1
                emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((gb1 & 7) << 3) | 3);
                if (arg0 != X86_R10) {
                    emit_u8(cb, 0x4C | (arg0 >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((X86_R10 & 7) << 3) | (arg0 & 7));
                }
                if (arg1 != X86_R11) {
                    emit_u8(cb, 0x4C | (arg1 >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((X86_R11 & 7) << 3) | (arg1 & 7));
                }
                emit_u8(cb, 0xE8);
                int32_t back = (int32_t)int_off - (int32_t)(cb->len + 4);
                emit_i32(cb, back);
                int gd = slot_gpr[d];
                if (gd != X86_RAX) {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | (gd & 7));
                }
                break;
            }
            case OP_RETURN: case OP_RETURN_NUM: {
                int gd = slot_gpr[d];
                if (gd != X86_RAX) {
                    emit_u8(cb, 0x48 | (gd >= 8 ? 0x04 : 0) | (X86_RAX >= 8 ? 0x01 : 0));
                    emit_u8(cb, 0x89);
                    emit_u8(cb, 0xC0 | ((gd & 7) << 3) | (X86_RAX & 7));
                }
                for (int i = cs_idx - 1; i >= 0; i--) {                 // pop cs regs (reverse)
                    int g = cs_gprs[i];
                    if (g >= 8) { emit_u8(cb, 0x41); emit_u8(cb, 0x58 | (g & 7)); }
                    else        { emit_u8(cb, 0x58 | g); }
                }
                emit_u8(cb, 0xC3);
                break;
            }
            case OP_RETURN_NUM_IMM: {
                emit_u8(cb, 0xB8); emit_u32(cb, (uint32_t)a);           // mov eax, imm32
                for (int i = cs_idx - 1; i >= 0; i--) {                 // pop cs regs (reverse)
                    int g = cs_gprs[i];
                    if (g >= 8) { emit_u8(cb, 0x41); emit_u8(cb, 0x58 | (g & 7)); }
                    else        { emit_u8(cb, 0x58 | g); }
                }
                emit_u8(cb, 0xC3);
                break;
            }
            default:
                cb->len = int_off; return false;
        }
    }

    for (int i = 0; i < nfix; i++) {                                // patch internal jumps
        int tidx = fixups[i].target_pc - start;
        if (tidx < 0 || tidx >= range_size || lbl_off[tidx] < 0) {
            cb->len = int_off; return false;
        }
        int32_t rel = (int32_t)lbl_off[tidx] - (int32_t)(fixups[i].patch_at + 4);
        memcpy(cb->buf + fixups[i].patch_at, &rel, 4);
    }
    return true;
}

// emits the wrapper that dispatches between the int-specialized body and the
void emit_int_wrapper(CodeBuf* cb, size_t general_off, size_t int_off,
                      int max_n, int arity, size_t* out_wrapper_off,
                      const X86_64Abi* abi) {
    static const int arg_gprs_sysv[2]  = { X86_RDI, X86_RSI };
    static const int arg_gprs_win64[2] = { X86_RCX, X86_RDX };
    const int* arg_gprs = (abi->frame_reg == X86_RCX)
                        ? arg_gprs_win64 : arg_gprs_sysv;
    *out_wrapper_off = cb->len;

    size_t patches[8];
    int    n_patches = 0;

    for (int i = 0; i < arity; i++) {                            // check each arg
        int ag = arg_gprs[i];
        uint8_t rex1 = 0x48 | (ag >= 8 ? 0x04 : 0) | (i >= 8 ? 0x01 : 0);
        emit_u8(cb, 0xF2); emit_u8(cb, rex1);                    // cvttsd2si ag, xmm<i>
        emit_u8(cb, 0x0F); emit_u8(cb, 0x2C);
        emit_u8(cb, 0xC0 | ((ag & 7) << 3) | (i & 7));
        emit_u8(cb, 0xF2); emit_u8(cb, 0x4C | (ag >= 8 ? 0x01 : 0)); // cvtsi2sd xmm15, ag
        emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
        emit_u8(cb, 0xC0 | (7 << 3) | (ag & 7));
        emit_u8(cb, 0x66); emit_u8(cb, 0x44);                    // ucomisd xmm15, xmm<i>
        emit_u8(cb, 0x0F); emit_u8(cb, 0x2E);
        emit_u8(cb, 0xC0 | (7 << 3) | (i & 7));
        emit_u8(cb, 0x0F); emit_u8(cb, 0x8A);                    // jp fallback
        if (n_patches < 8) patches[n_patches++] = cb->len;
        emit_i32(cb, 0);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x85);                    // jne fallback
        if (n_patches < 8) patches[n_patches++] = cb->len;
        emit_i32(cb, 0);
    }

    if (max_n > 0 && arity >= 1) {                               // bound check arg0
        int ag = arg_gprs[0];
        emit_u8(cb, 0x48 | (ag >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x83); emit_u8(cb, 0xF8 | (ag & 7));         // cmp ag, imm8
        emit_u8(cb, (uint8_t)max_n);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x8F);                    // jg fallback
        if (n_patches < 8) patches[n_patches++] = cb->len;
        emit_i32(cb, 0);
    }

    emit_u8(cb, 0xE8);                                           // call int body
    int32_t call_rel = (int32_t)int_off - (int32_t)(cb->len + 4);
    emit_i32(cb, call_rel);
    emit_u8(cb, 0xF2); emit_u8(cb, 0x48);                        // cvtsi2sd xmm0, rax
    emit_u8(cb, 0x0F); emit_u8(cb, 0x2A); emit_u8(cb, 0xC0);
    emit_u8(cb, 0xC3);                                           // ret

    size_t fallback_at = cb->len;                                // fallback: jmp general
    for (int i = 0; i < n_patches; i++) {
        int32_t r = (int32_t)fallback_at - (int32_t)(patches[i] + 4);
        memcpy(cb->buf + patches[i], &r, 4);
    }
    emit_u8(cb, 0xE9);
    int32_t jmp_rel = (int32_t)general_off - (int32_t)(cb->len + 4);
    emit_i32(cb, jmp_rel);
}

// GPRs available to hold bytecode slots inside an int64-specialized loop.
// frame_reg MUST NOT appear here: the guard code reads regs[s] via frame_reg
// on every live-in slot, and using frame_reg as a slot GPR would clobber it
// after the first guard.
//  - SysV: frame_reg = RDI -> RDI excluded.
//  - Win64: frame_reg = RCX -> RCX excluded; RDI/RSI are callee-saved on
//    Win64 and can't be used without a save/restore.
static const int int_loop_gprs_sysv[] = {
    X86_RAX, X86_RCX, X86_RDX, X86_RSI, X86_R8, X86_R9, X86_R10, X86_R11
};
#define INT_LOOP_N_GPRS_SYSV 8

static const int int_loop_gprs_win64[] = {
    X86_RAX, X86_RDX, X86_R8, X86_R9, X86_R10, X86_R11
};
#define INT_LOOP_N_GPRS_WIN64 6

// checks whether every body instruction between entry and back_edge is
static bool loop_body_is_int_safe(BytecodeChunk* chunk, int entry, int back_edge) {
    for (int pc = entry + 1; pc < back_edge; pc++) {
        if (!op_is_int_safe(chunk->code[pc].opcode)) return false;
    }
    return true;
}

// assigns one GPR to every bytecode slot the loop touches; returns the
int assign_int_loop_gprs(JITContext* ctx, JitLoopInfo* info,
                         int slot_gpr[JIT_MAX_SLOTS], int frame_reg) {
    const int* gprs = (frame_reg == X86_RCX)
                    ? int_loop_gprs_win64 : int_loop_gprs_sysv;
    int n_gprs = (frame_reg == X86_RCX)
               ? INT_LOOP_N_GPRS_WIN64 : INT_LOOP_N_GPRS_SYSV;

    for (int i = 0; i < JIT_MAX_SLOTS; i++) slot_gpr[i] = -1;
    uint64_t used = info->live_in | info->live_out;
    if (info->for_var_reg  >= 0) used |= 1ULL << info->for_var_reg;
    if (info->for_end_reg  >= 0) used |= 1ULL << info->for_end_reg;
    if (info->for_step_reg >= 0) used |= 1ULL << info->for_step_reg;
    if (info->kind == JIT_LOOP_CONDITION) {                  // entry cmp reads a,b
        Instruction* e = &ctx->chunk->code[info->entry_pc];
        if (e->operands[1] >= 0 && e->operands[1] < 64) used |= 1ULL << e->operands[1];
        if (e->operands[2] >= 0 && e->operands[2] < 64) used |= 1ULL << e->operands[2];
    }
    int n = 0;
    while (used) {
        int s = __builtin_ctzll(used);
        used &= used - 1;
        if (n >= n_gprs) return -1;
        slot_gpr[s] = gprs[n++];
    }
    return n;
}

// fallback: matches_int_accum_loop is gone; the generic emitter below
bool matches_int_accum_loop(const X86_64Abi* abi, JITContext* ctx, JitLoopInfo* info) {
    if (info->kind != JIT_LOOP_NUMERIC_FOR &&
        info->kind != JIT_LOOP_CONDITION) return false;      // only those two
    if (info->kind == JIT_LOOP_NUMERIC_FOR && info->step_sign == -1)
        return false;                                        // int body assumes non-negative step
    if (!loop_body_is_int_safe(ctx->chunk, info->entry_pc, info->back_edge_pc))
        return false;                                        // body must be int-safe

    int slot_gpr[JIT_MAX_SLOTS];                             // scratch, just to count
    if (assign_int_loop_gprs(ctx, info, slot_gpr, abi->frame_reg) < 0) return false;
    return true;                                             // anything else: general
}

// emits a GPR-only version of a numeric loop body instruction; each bytecode
void emit_int_loop_body(CodeBuf* cb, BytecodeChunk* chunk, int pc,
                        const int* slot_gpr) {
    Instruction* inst = &chunk->code[pc];
    int d = inst->operands[0];
    int a = inst->operands[1];
    int b = inst->operands[2];
    int gd = slot_gpr[d];
    int ga = slot_gpr[a];

    switch (inst->opcode) {
        case OP_MOVE:                                        // gd = ga
            if (d == a) break;
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (ga >= 8 ? 0x04 : 0));
            emit_u8(cb, 0x89);                               // mov r/m64, r64
            emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
            break;
        case OP_LOAD_NUM_IMM: {                              // gd = imm32
            int32_t k = a;
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
            emit_u8(cb, 0xC7);                               // mov r/m64, imm32
            emit_u8(cb, 0xC0 | (gd & 7));
            emit_i32(cb, k);
            break;
        }
        case OP_ADD: {                                       // gd = ga + gb
            int gb = slot_gpr[b];
            if (d != a) {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (ga >= 8 ? 0x04 : 0));
                emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
            }
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (gb >= 8 ? 0x04 : 0));
            emit_u8(cb, 0x01); emit_u8(cb, 0xC0 | ((gb & 7) << 3) | (gd & 7));
            break;
        }
        case OP_SUB: {                                       // gd = ga - gb
            int gb = slot_gpr[b];
            if (d != a) {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (ga >= 8 ? 0x04 : 0));
                emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
            }
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (gb >= 8 ? 0x04 : 0));
            emit_u8(cb, 0x29); emit_u8(cb, 0xC0 | ((gb & 7) << 3) | (gd & 7));
            break;
        }
        case OP_MUL: {                                       // gd = ga * gb
            int gb = slot_gpr[b];
            if (d != a) {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (ga >= 8 ? 0x04 : 0));
                emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
            }
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x04 : 0) | (gb >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x0F); emit_u8(cb, 0xAF);            // imul r64, r/m64
            emit_u8(cb, 0xC0 | ((gd & 7) << 3) | (gb & 7));
            break;
        }
        case OP_ADD_IMM: {                                   // gd = ga + imm8
            int32_t k = b;
            if (d != a) {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (ga >= 8 ? 0x04 : 0));
                emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
            }
            if (k >= -128 && k <= 127) {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x83); emit_u8(cb, 0xC0 | (gd & 7));
                emit_u8(cb, (uint8_t)k);
            } else {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x81); emit_u8(cb, 0xC0 | (gd & 7));
                emit_i32(cb, k);
            }
            break;
        }
        case OP_SUB_IMM: {                                   // gd = ga - imm8
            int32_t k = b;
            if (d != a) {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0) | (ga >= 8 ? 0x04 : 0));
                emit_u8(cb, 0x89); emit_u8(cb, 0xC0 | ((ga & 7) << 3) | (gd & 7));
            }
            if (k >= -128 && k <= 127) {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x83); emit_u8(cb, 0xE8 | (gd & 7));
                emit_u8(cb, (uint8_t)k);
            } else {
                emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
                emit_u8(cb, 0x81); emit_u8(cb, 0xE8 | (gd & 7));
                emit_i32(cb, k);
            }
            break;
        }
        case OP_MUL_IMM: {                                   // gd = ga * imm
            int32_t k = b;
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x04 : 0) | (ga >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x69);                               // imul r64, r/m64, imm32
            emit_u8(cb, 0xC0 | ((gd & 7) << 3) | (ga & 7));
            emit_i32(cb, k);
            break;
        }
        case OP_INC: {                                       // gd += 1
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x83); emit_u8(cb, 0xC0 | (gd & 7));
            emit_u8(cb, 0x01);
            break;
        }
        case OP_DEC: {                                       // gd -= 1
            emit_u8(cb, 0x48 | (gd >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x83); emit_u8(cb, 0xE8 | (gd & 7));
            emit_u8(cb, 0x01);
            break;
        }
        case OP_JUMP:                                        // rel32 patched by caller
        case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ:
        case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
        case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
        case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
            // int-safe bodies never contain internal branches in the loops
            emit_u8(cb, 0x90);
            break;
        default:
            emit_u8(cb, 0x90);                               // nop on unsupported op
            break;
    }
}

// emits a full int64 loop: prologue guard, int body, writeback, and the
bool x86_64_emit_int_loop(const X86_64Abi* abi, JITContext* ctx,
                          CodeBuf* cb, JitLoopInfo* info,
                          int step_sign, void** out_fn) {
    int slot_gpr[JIT_MAX_SLOTS];
    int n_gpr = assign_int_loop_gprs(ctx, info, slot_gpr, abi->frame_reg);
    if (n_gpr < 0) return false;

    size_t mark = cb->len;
    size_t entry_off = cb->len;
    int regs = abi->frame_reg;

    // one guard per live-in slot (and per var/end/step for numeric-for)
    uint64_t live = info->live_in;
    if (info->for_var_reg  >= 0) live |= 1ULL << info->for_var_reg;
    if (info->for_end_reg  >= 0) live |= 1ULL << info->for_end_reg;
    if (info->for_step_reg >= 0) live |= 1ULL << info->for_step_reg;
    if (info->kind == JIT_LOOP_CONDITION) {
        Instruction* e = &ctx->chunk->code[info->entry_pc];
        if (e->operands[1] >= 0 && e->operands[1] < 64) live |= 1ULL << e->operands[1];
        if (e->operands[2] >= 0 && e->operands[2] < 64) live |= 1ULL << e->operands[2];
    }

    size_t patch_sites[64];                                  // two per guard, plus step
    int    n_patches = 0;
#define PUSH_PATCH(off) do { if (n_patches < 64) patch_sites[n_patches++] = (off); } while (0)

    while (live) {
        int s = __builtin_ctzll(live);
        live &= live - 1;
        int gpr = slot_gpr[s];
        x86_emit_movsd_load_base(cb, regs, 0, s * 8);        // xmm0 = regs[s]
        emit_u8(cb, 0xF2); emit_u8(cb, 0x48 | (gpr >= 8 ? 0x04 : 0));
        emit_u8(cb, 0x0F); emit_u8(cb, 0x2C); emit_u8(cb, 0xC0 | ((gpr & 7) << 3));
        emit_u8(cb, 0xF2); emit_u8(cb, 0x48 | (gpr >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x0F); emit_u8(cb, 0x2A); emit_u8(cb, 0xC8 | (gpr & 7));
        emit_u8(cb, 0x66); emit_u8(cb, 0x0F); emit_u8(cb, 0x2E); emit_u8(cb, 0xC8);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x85); PUSH_PATCH(cb->len); emit_i32(cb, 0);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x8A); PUSH_PATCH(cb->len); emit_i32(cb, 0);
    }

    // for numeric-for, additionally require step == 1 in int64 space
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {
        int gstep = slot_gpr[info->for_step_reg];
        emit_u8(cb, 0x48 | (gstep >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x83); emit_u8(cb, 0xF8 | (gstep & 7));  // cmp gstep, 1
        emit_u8(cb, 0x01);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x85); PUSH_PATCH(cb->len); emit_i32(cb, 0);
    }

    // loop structure
    int gvar = -1, gend = -1;
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {
        gvar = slot_gpr[info->for_var_reg];
        gend = slot_gpr[info->for_end_reg];
        emit_u8(cb, 0x48 | (gend >= 8 ? 0x04 : 0) | (gvar >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x39);                                   // cmp gvar, gend
        emit_u8(cb, 0xC0 | ((gend & 7) << 3) | (gvar & 7));
        emit_u8(cb, 0x0F); emit_u8(cb, 0x8F);                // jg .done
        size_t skip = cb->len; emit_i32(cb, 0);
        size_t loop_top = cb->len;
        for (int pc = info->entry_pc + 1; pc < info->back_edge_pc; pc++)
            emit_int_loop_body(cb, ctx->chunk, pc, slot_gpr);
        emit_u8(cb, 0x48 | (gvar >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x83); emit_u8(cb, 0xC0 | (gvar & 7));   // add gvar, 1
        emit_u8(cb, 0x01);
        emit_u8(cb, 0x48 | (gend >= 8 ? 0x04 : 0) | (gvar >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x39);
        emit_u8(cb, 0xC0 | ((gend & 7) << 3) | (gvar & 7));  // cmp gvar, gend
        emit_u8(cb, 0x0F); emit_u8(cb, 0x8E);                // jle .loop
        int32_t back = (int32_t)loop_top - (int32_t)(cb->len + 4);
        emit_i32(cb, back);
        emit_u8(cb, 0x48 | (gvar >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x83); emit_u8(cb, 0xE8 | (gvar & 7));   // sub gvar, 1
        emit_u8(cb, 0x01);
        int32_t jg = (int32_t)cb->len - (int32_t)(skip + 4);
        memcpy(cb->buf + skip, &jg, 4);
    } else {
        // condition loop: entry is JUMP_IF_<cond> a, b -> exit
        Instruction* e = &ctx->chunk->code[info->entry_pc];
        int ga = slot_gpr[e->operands[1]];
        int gb = (e->opcode >= OP_JUMP_IF_EQ_IMM &&
                  e->opcode <= OP_JUMP_IF_GTE_IMM)
               ? -1 : slot_gpr[e->operands[2]];             // IMM variants: use imm
        size_t loop_top = cb->len;
        if (gb >= 0) {
            emit_u8(cb, 0x48 | (ga >= 8 ? 0x01 : 0) | (gb >= 8 ? 0x04 : 0));
            emit_u8(cb, 0x39);                               // cmp ga, gb
            emit_u8(cb, 0xC0 | ((gb & 7) << 3) | (ga & 7));
        } else {
            int32_t k = e->operands[2];
            emit_u8(cb, 0x48 | (ga >= 8 ? 0x01 : 0));
            emit_u8(cb, 0x83); emit_u8(cb, 0xF8 | (ga & 7)); // cmp ga, imm8
            emit_u8(cb, (uint8_t)k);
        }
        uint8_t jcc;
        switch (e->opcode) {
            case OP_JUMP_IF_EQ:  case OP_JUMP_IF_EQ_IMM:  jcc = 0x84; break;
            case OP_JUMP_IF_NEQ: case OP_JUMP_IF_NEQ_IMM: jcc = 0x85; break;
            case OP_JUMP_IF_LT:  case OP_JUMP_IF_LT_IMM:  jcc = 0x8C; break;
            case OP_JUMP_IF_GT:  case OP_JUMP_IF_GT_IMM:  jcc = 0x8F; break;
            case OP_JUMP_IF_LTE: case OP_JUMP_IF_LTE_IMM: jcc = 0x8E; break;
            default:                                       jcc = 0x8D; break;
        }
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                 // exit if cond
        size_t exit_patch = cb->len; emit_i32(cb, 0);
        for (int pc = info->entry_pc + 1; pc < info->back_edge_pc; pc++)
            emit_int_loop_body(cb, ctx->chunk, pc, slot_gpr);
        emit_u8(cb, 0xE9);                                   // jmp loop_top
        int32_t back = (int32_t)loop_top - (int32_t)(cb->len + 4);
        emit_i32(cb, back);
        int32_t exit_rel = (int32_t)cb->len - (int32_t)(exit_patch + 4);
        memcpy(cb->buf + exit_patch, &exit_rel, 4);
    }

    // writeback: written slots -> double
    uint64_t written = info->live_out;
    if (info->for_var_reg >= 0) written |= 1ULL << info->for_var_reg;
    while (written) {
        int s = __builtin_ctzll(written);
        written &= written - 1;
        int gpr = slot_gpr[s];
        emit_u8(cb, 0xF2); emit_u8(cb, 0x48 | (gpr >= 8 ? 0x01 : 0));
        emit_u8(cb, 0x0F); emit_u8(cb, 0x2A); emit_u8(cb, 0xC0 | (gpr & 7));
        x86_emit_movsd_store_base(cb, regs, 0, s * 8);
    }
    emit_u8(cb, 0xC3);                                       // ret

    // fallback: general double loop, entered from any failed guard
    size_t fallback_at = cb->len;
    void* gen_fn = NULL;
    if (!x86_64_emit_numeric_loop(abi, ctx, cb, info, step_sign, &gen_fn)) {
        cb->len = mark;
        return false;
    }
    for (int i = 0; i < n_patches; i++) {
        int32_t r = (int32_t)fallback_at - (int32_t)(patch_sites[i] + 4);
        memcpy(cb->buf + patch_sites[i], &r, 4);
    }

    *out_fn = (void*)(cb->buf + entry_off);
    return true;
}

#undef PUSH_PATCH
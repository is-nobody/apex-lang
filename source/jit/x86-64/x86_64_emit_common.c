// source/jit/x86-64/x86_64_emit_common.c
// Shared helpers for the split x86-64 emit translation units
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// rounds n up to the next multiple of 16
int align16(int n) { return (n + 15) & ~15; }

// checks if an opcode ends a basic block (no fall-through to the next pc)
bool op_is_terminator(Opcode op) {
    return op == OP_JUMP ||
           op == OP_RETURN || op == OP_RETURN_NUM ||
           op == OP_RETURN_BOOL || op == OP_RETURN_NONE ||
           op == OP_RETURN_NUM_IMM;
}

// captures the current cache into a compact snapshot
void x86_cache_snap(JitCacheSnap* snap, const XmmCache* c) {
    memset(snap, 0, sizeof(*snap));
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = c->reg_slot[i];
        snap->reg_slot[i] = (int16_t)s;
        if (s >= 0 && c->slot_dirty[s]) snap->dirty_mask |= (uint16_t)(1u << i);
    }
}

// rebuilds the xmm cache from a compact snapshot, clearing prior state
void x86_cache_restore(XmmCache* c, const JitCacheSnap* snap) {
    x86_cache_clear(c);
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = snap->reg_slot[i];
        if (s < 0) continue;
        c->reg_slot[i] = s;
        c->slot_reg[s] = i;
        c->slot_dirty[s] = (snap->dirty_mask >> i) & 1;
    }
}

// checks whether any backward jump in [start,end) targets `target`
bool has_backward_jump(BytecodeChunk* chunk, int start, int end, int target) {
    (void)start;                                             // start is unused, kept for symmetry
    for (int pc = target + 1; pc < end; pc++) {
        Opcode op = chunk->code[pc].opcode;
        if (op == OP_JUMP || op == OP_JUMP_IF_FALSE ||
            op == OP_JUMP_IF_EQ || op == OP_JUMP_IF_NEQ ||
            op == OP_JUMP_IF_EQ_NUM || op == OP_JUMP_IF_NEQ_NUM ||
            op == OP_JUMP_IF_LT || op == OP_JUMP_IF_GT ||
            op == OP_JUMP_IF_LTE || op == OP_JUMP_IF_GTE ||
            op == OP_JUMP_IF_EQ_IMM || op == OP_JUMP_IF_NEQ_IMM ||
            op == OP_JUMP_IF_LT_IMM || op == OP_JUMP_IF_GT_IMM ||
            op == OP_JUMP_IF_LTE_IMM || op == OP_JUMP_IF_GTE_IMM ||
            op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
            op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) {
            if (chunk->code[pc].operands[0] == target) return true;
        }
    }
    return false;
}

// helper for OP_JUMP_MATCH_STR: returns 1 if the subject is a string whose
int jit_match_str(Value subj, StringObject* case_str) {
    if (!IS_STRING(subj)) return 0;                          // non-strings never match
    StringObject* s = AS_STRING(subj);                       // unwrap subject pointer
    if (s == case_str) return 1;                             // same interned pointer
    if (!s || !case_str) return 0;
    if (s->length != case_str->length) return 0;             // different lengths
    return memcmp(s->chars, case_str->chars, s->length) == 0;
}

// JIT helper: replace *dst with new_val
void jit_store_slot(Value* dst, Value new_val) {
    Value old = *dst;
    if ((new_val & QNAN) == QNAN) value_incref(new_val);
    if ((old & QNAN) == QNAN)     value_decref(old);
    *dst = new_val;
}

// two-digit lookup, eliminates one div per digit
static const char jit_digit_pairs[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9',
};

// fast unsigned decimal itoa; returns digits written, no null terminator
static int jit_uitoa(char* buf, unsigned long long n) {
    if (n < 10) { buf[0] = (char)('0' + (unsigned)n); return 1; }     // single digit fast path
    char tmp[24];                                                     // scratch, reversed
    int i = 0;                                                        // digits written into tmp
    while (n >= 100) {
        unsigned r = (unsigned)(n % 100);                             // last two digits
        n /= 100;                                                     // shift right by two
        tmp[i]     = jit_digit_pairs[r * 2 + 1];                      // low digit first (reversed)
        tmp[i + 1] = jit_digit_pairs[r * 2];                          // then the high digit
        i += 2;                                                       // two digits written
    }
    if (n >= 10) {                                                    // one two-digit group left
        tmp[i]     = (char)n;                                         // low digit of the pair
        tmp[i + 1] = '0' + (char)(n / 10);                            // high digit of the pair
        i += 2;
    } else {
        tmp[i++] = (char)('0' + (unsigned)n);                         // single trailing digit
    }
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];              // reverse into buf
    return i;                                                         // number of digits produced
}

// fast signed decimal itoa; handles INT64_MIN without overflow
static int jit_itoa(char* buf, long long n) {
    if (n < 0) {
        buf[0] = '-';                                                 // leading minus
        unsigned long long u = (unsigned long long)(-(n + 1)) + 1ULL; // negate without overflow
        return 1 + jit_uitoa(buf + 1, u);                             // sign + magnitude digits
    }
    return jit_uitoa(buf, (unsigned long long)n);                     // non-negative fast path
}

// JIT helper: tbl["prefix" .. num] with a synthetic key
Value jit_table_get_key_str(Table* t, StringObject* prefix, double num) {
    char nbuf[24];                                                    // tail buffer on the stack
    int nlen;                                                         // tail length in digits
    long long inum = (long long)num;                                  // truncate to integer
    if ((double)inum == num && inum > -1000000000000000LL && inum < 1000000000000000LL)
        nlen = jit_itoa(nbuf, inum);                                  // whole number: custom itoa
    else
        nlen = snprintf(nbuf, sizeof(nbuf), "%.15g", num);            // fractional/huge: snprintf

    Value val = MAKE_NONE();                                          // default = key not found
    table_get_concat_key(t, prefix, nbuf, nlen, &val);                // lookup without allocating key
    return val;                                                       // incref'd by the helper on hit
}

// JIT helper: tbl["prefix" .. num] = val, hash computed once
void jit_table_set_key_str(Table* t, StringObject* prefix, double num, Value val) {
    char nbuf[24];                                                    // tail buffer on the stack
    int nlen;                                                         // tail length in digits
    long long inum = (long long)num;                                  // truncate to integer
    if ((double)inum == num && inum > -1000000000000000LL && inum < 1000000000000000LL)
        nlen = jit_itoa(nbuf, inum);                                  // whole number: custom itoa
    else
        nlen = snprintf(nbuf, sizeof(nbuf), "%.15g", num);            // fractional/huge: snprintf

    table_set_concat_key(t, prefix, nbuf, nlen, val);                 // insert/update, alloc only if new
}

// emits the shared prologue then any abi-specific callee-saved register saves
void emit_prologue(CodeBuf* cb, const X86_64Abi* abi,
                   int frame_size, int base_frame) {
    emit_u8(cb, 0x55);                                       // push rbp
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xE5); // mov rbp, rsp
    emit_u8(cb, 0x48); emit_u8(cb, 0x81); emit_u8(cb, 0xEC); // sub rsp, imm32
    emit_i32(cb, frame_size);                                // frame size immediate
    if (abi->emit_prologue_saves) abi->emit_prologue_saves(cb, base_frame);
}

// emits abi-specific restores, then leave; ret
void emit_leave_ret(CodeBuf* cb, const X86_64Abi* abi, int base_frame) {
    if (abi->emit_epilogue_restores) abi->emit_epilogue_restores(cb, base_frame);
    emit_u8(cb, 0xC9);                                       // leave
    emit_u8(cb, 0xC3);                                       // ret
}

// fallback epilogue: returns xmm0 = 0.0
void emit_return_zero(const X86_64Abi* abi, CodeBuf* cb, int base_frame) {
    emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
    emit_u8(cb, 0x57); emit_u8(cb, 0xC0);                    // xorpd xmm0, xmm0
    emit_leave_ret(cb, abi, base_frame);
}

// emits indirect call through func_table[func_idx] (args in xmm0/xmm1, result in xmm0)
void emit_call_func(CodeBuf* cb, JITContext* ctx, int func_idx) {
    uint64_t slot_addr = (uint64_t)(uintptr_t)&ctx->func_table[func_idx];  // address of the slot
    x86_emit_movabs_r11(cb, slot_addr);                      // r11 = &func_table[idx]
    emit_u8(cb, 0x41); emit_u8(cb, 0xFF); emit_u8(cb, 0x13); // call [r11]
}

// emits a call to `callee`; if it is the function currently being emitted
void emit_call_or_self(CodeBuf* cb, JITContext* ctx, int callee,
                       int self_idx, size_t self_mark) {
    if (callee == self_idx) {
        emit_u8(cb, 0xE8);                                   // call rel32
        int32_t back = (int32_t)self_mark - (int32_t)(cb->len + 4);
        emit_i32(cb, back);                                  // rel32 to function entry
        return;
    }
    emit_call_func(cb, ctx, callee);
}

// returns the jcc opcode that exits the loop when the entry condition is met
uint8_t jcc_for_entry_op(Opcode op) {
    switch (op) {
        case OP_JUMP_IF_EQ:     case OP_JUMP_IF_EQ_NUM:  return 0x84;
        case OP_JUMP_IF_NEQ:    case OP_JUMP_IF_NEQ_NUM: return 0x85;
        case OP_JUMP_IF_LT:  return 0x82;
        case OP_JUMP_IF_GT:  return 0x87;
        case OP_JUMP_IF_LTE: return 0x86;
        case OP_JUMP_IF_GTE: return 0x83;
        case OP_JUMP_IF_EQ_IMM:  return 0x84;   // same jcc, different operand form
        case OP_JUMP_IF_NEQ_IMM: return 0x85;
        case OP_JUMP_IF_LT_IMM:  return 0x82;
        case OP_JUMP_IF_GT_IMM:  return 0x87;
        case OP_JUMP_IF_LTE_IMM: return 0x86;
        case OP_JUMP_IF_GTE_IMM: return 0x83;
        default:             return 0x84;
    }
}

// checks if slot s is read between pc_after and the first write to s (inclusive scan, stops at write)
bool slot_read_before_write(BytecodeChunk* chunk, int pc_after, int end, int s) {
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
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:   // reads a only, b is an immediate
            case OP_DIV_IMM: case OP_MOD_IMM:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
                reads = (a == s);
                break;
            case OP_JUMP_IF_FALSE:                               // reads a
                reads = (a == s);
                break;
            case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:      // reads subject in a
            case OP_JUMP_MATCH_BOOL: case OP_JUMP_MATCH_NONE:
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
            case OP_LOAD_BOOL: case OP_LOAD_NONE:
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:   // writes d
            case OP_DIV_IMM: case OP_MOD_IMM:
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

// checks whether slot s is live at pc inside a loop body: true if s is read
bool slot_live_in_loop(BytecodeChunk* chunk, int entry, int back_edge,
                       int pc, int s) {
    if (pc + 1 < back_edge &&
        slot_read_before_write(chunk, pc + 1, back_edge, s)) return true;
    if (entry + 1 < pc &&
        slot_read_before_write(chunk, entry + 1, pc, s)) return true;
    return false;
}

// rewrites xmm_d to the NONE bit pattern on NaN; matches interpreter semantics
void emit_nan_check(CodeBuf* cb, int xmm_d) {
    x86_emit_ucomisd_rr(cb, xmm_d, xmm_d);                        // ucomisd xmm_d, xmm_d
    emit_u8(cb, 0x0F); emit_u8(cb, 0x8B);                          // jnp +rel32
    size_t patch_at = cb->len;                                     // record jump placeholder
    emit_i32(cb, 0);
    x86_emit_movabs_rax(cb, X86_NONE_BITS);                        // rax = NONE bits
    x86_emit_movq_xmm_rax(cb, xmm_d);                              // xmm_d = NONE bits
    int32_t rel = (int32_t)cb->len - (int32_t)(patch_at + 4);      // rel32 to skip
    memcpy(cb->buf + patch_at, &rel, 4);
}

// returns true if an opcode can execute in int64 GPR mode without losing semantics
bool op_is_int_safe(Opcode op) {
    switch (op) {
        case OP_MOVE:
        case OP_LOAD_NUM_IMM:
        case OP_ADD:  case OP_SUB:  case OP_MUL:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_INC:  case OP_DEC:
        case OP_JUMP:
        case OP_JUMP_IF_EQ:  case OP_JUMP_IF_NEQ:
        case OP_JUMP_IF_LT:  case OP_JUMP_IF_GT:
        case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
        case OP_JUMP_IF_EQ_IMM:  case OP_JUMP_IF_NEQ_IMM:
        case OP_JUMP_IF_LT_IMM:  case OP_JUMP_IF_GT_IMM:
        case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
            return true;
        default:
            return false;
    }
}

// checks whether an opcode writes to its operands[0] destination
bool op_writes_dest(Opcode op) {
    switch (op) {
        case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
        case OP_LOAD_BOOL: case OP_LOAD_NONE:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_TABLE_GET: case OP_TABLE_GET_INT: case OP_TABLE_GET_NUM:
        case OP_TABLE_GET_KEY_STR:
        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
            return true;                                     // d is written by these ops
        default:
            return false;                                    // d is untouched
    }
}

// returns true when an opcode is an immediate-comparison jump entry
bool entry_op_is_imm(Opcode op) {
    return op == OP_JUMP_IF_EQ_IMM || op == OP_JUMP_IF_NEQ_IMM ||
           op == OP_JUMP_IF_LT_IMM || op == OP_JUMP_IF_GT_IMM ||
           op == OP_JUMP_IF_LTE_IMM || op == OP_JUMP_IF_GTE_IMM;
}
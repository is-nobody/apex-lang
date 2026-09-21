// source/jit/x86-64/x86_64_emit.c
// Implementation of x86-64 function and loop emitters for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_common.h"
#include "x86_64_abi.h"
#include "vm.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// rounds n up to the next multiple of 16
static int align16(int n) { return (n + 15) & ~15; }

// callee-saved gprs usable as slot pockets on all supported abis
#define X86_N_GPR_POCKETS 5

static const int x86_gpr_pocket_regs[X86_N_GPR_POCKETS] = {
    X86_RBX, X86_R12, X86_R13, X86_R14, X86_R15
};

// emits load r64, [rbp+disp32] for each assigned pocket gpr before leave; ret
static void emit_gpr_pocket_restores(CodeBuf* cb, int pocket_slot_base,
                                     int n_pockets) {
    for (int g = 0; g < n_pockets; g++) {                    // restore saved gprs
        int gpr = x86_gpr_pocket_regs[g];
        int disp = -8 * (pocket_slot_base + g + 1);
        x86_emit_load_r64_rbp(cb, gpr, disp);
    }
}

// checks if an opcode ends a basic block (no fall-through to the next pc)
static bool op_is_terminator(Opcode op) {
    return op == OP_JUMP ||
           op == OP_RETURN || op == OP_RETURN_NUM ||
           op == OP_RETURN_BOOL || op == OP_RETURN_NONE ||
           op == OP_RETURN_NUM_IMM;
}

// compact xmm cache snapshot stored at a unique forward jump site
typedef struct {
    int16_t  reg_slot[XMM_CACHE_REGS];  // xmm[i] holds slot reg_slot[i], or -1
    uint16_t dirty_mask;                // bit i: xmm[i] holds a dirty slot
} JitCacheSnap;

// captures the current cache into a compact snapshot
static void x86_cache_snap(JitCacheSnap* snap, const XmmCache* c) {
    memset(snap, 0, sizeof(*snap));
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = c->reg_slot[i];
        snap->reg_slot[i] = (int16_t)s;
        if (s >= 0 && c->slot_dirty[s]) snap->dirty_mask |= (uint16_t)(1u << i);
    }
}

// rebuilds the xmm cache from a compact snapshot, clearing prior state
static void x86_cache_restore(XmmCache* c, const JitCacheSnap* snap) {
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
static bool has_backward_jump(BytecodeChunk* chunk, int start, int end, int target) {
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
static void jit_store_slot(Value* dst, Value new_val) {
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

// emits the shared prologue then any abi-specific callee-saved register saves=
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
    x86_emit_movabs_r11(cb, slot_addr);                      // r11 = &func_table[idx]
    emit_u8(cb, 0x41); emit_u8(cb, 0xFF); emit_u8(cb, 0x13); // call [r11]
}

// emits a call to `callee`; if it is the function currently being emitted
static void emit_call_or_self(CodeBuf* cb, JITContext* ctx, int callee,
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
static uint8_t jcc_for_entry_op(Opcode op) {
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
// (before any write) either later in the same iteration, or earlier in the
// body — the latter covers loop-carried reads that the next iteration will
// hit before reaching pc again
static bool slot_live_in_loop(BytecodeChunk* chunk, int entry, int back_edge,
                              int pc, int s) {
    if (pc + 1 < back_edge &&
        slot_read_before_write(chunk, pc + 1, back_edge, s)) return true;
    if (entry + 1 < pc &&
        slot_read_before_write(chunk, entry + 1, pc, s)) return true;
    return false;
}

// rewrites xmm_d to the NONE bit pattern on NaN; matches interpreter semantics
static void emit_nan_check(CodeBuf* cb, int xmm_d) {
    x86_emit_ucomisd_rr(cb, xmm_d, xmm_d);                        // ucomisd xmm_d, xmm_d
    emit_u8(cb, 0x0F); emit_u8(cb, 0x8B);                          // jnp +rel32
    size_t patch_at = cb->len;                                     // record jump placeholder
    emit_i32(cb, 0);
    x86_emit_movabs_rax(cb, X86_NONE_BITS);                        // rax = NONE bits
    x86_emit_movq_xmm_rax(cb, xmm_d);                              // xmm_d = NONE bits
    int32_t rel = (int32_t)cb->len - (int32_t)(patch_at + 4);      // rel32 to skip
    memcpy(cb->buf + patch_at, &rel, 4);
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

    // precompute distinct immediates used in this function
    int32_t func_imms[8];
    int     func_n_imms = 0;
    bool    func_imm_ok = true;
    for (int pc = start; pc < end; pc++) {
        Instruction* inst = &chunk->code[pc];
        int32_t imm;
        switch (inst->opcode) {
            case OP_LOAD_NUM_IMM:
            case OP_RETURN_NUM_IMM:
                imm = inst->operands[1]; break;
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
            case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
                imm = inst->operands[2]; break;
            default: continue;
        }
        bool found = false;
        for (int i = 0; i < func_n_imms; i++)
            if (func_imms[i] == imm) { found = true; break; }
        if (found) continue;
        if (func_n_imms >= 8) { func_imm_ok = false; break; }
        func_imms[func_n_imms++] = imm;
    }
    if (!func_imm_ok) func_n_imms = 0;

    // gpr pockets are disabled: on float-heavy bodies the xmm<->gpr round-trip
    // costs more than an L1 spill/reload because it competes for the FP issue
    // ports with the arithmetic, while a store/load pair is hidden by the store
    // buffer; pocket code paths stay dormant while n_pockets is zero
    int slot_pocket_gpr[JIT_MAX_SLOTS];                      // slot -> pocket gpr index, or -1
    int slot_to_pocket[X86_N_GPR_POCKETS];                   // pocket gpr index -> slot, or -1
    int n_pockets = 0;                                       // number of assigned pockets
    for (int s = 0; s < nregs; s++) slot_pocket_gpr[s] = -1;
    for (int g = 0; g < X86_N_GPR_POCKETS; g++) slot_to_pocket[g] = -1;

    bool* is_target     = ctx->scratch_is_target;                // shared jump-target marks
    int32_t* label_off  = ctx->scratch_label_off;                // shared label offset array
    JumpFixup* fixups   = ctx->scratch_fixups;                   // shared jump fixup array
    if (!is_target || !label_off || !fixups) return false;       // scratch not available

    memset(is_target, 0, range_size * sizeof(bool));             // clear jump-target marks

    int*          pred_count  = (int*)         calloc(range_size, sizeof(int));
    uint8_t*      needs_flush = (uint8_t*)     calloc(range_size, 1);
    uint8_t*      use_snap    = (uint8_t*)     calloc(range_size, 1);
    JitCacheSnap* jump_snap   = (JitCacheSnap*)calloc(range_size, sizeof(JitCacheSnap));
    if (!pred_count || !needs_flush || !use_snap || !jump_snap) {  // scratch alloc failed
        free(pred_count); free(needs_flush); free(use_snap); free(jump_snap);
        return false;
    }
    for (int pc = start; pc < end; pc++) {                       // collect jump targets
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
            int t = chunk->code[pc].operands[0];                 // jump target
            if (t >= start && t < end) is_target[t - start] = true;  // mark as merge point
        }
    }

    for (int pc = start + 1; pc < end; pc++) {                   // fall-through predecessors
        if (op_is_terminator(chunk->code[pc - 1].opcode)) continue;
        pred_count[pc - start]++;
    }
    for (int pc = start; pc < end; pc++) {                       // add jump predecessors
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
            int t = chunk->code[pc].operands[0];                 // jump target
            if (t < start || t >= end) continue;                 // out of range, ignore
            is_target[t - start] = true;                         // mark as merge point
            pred_count[t - start]++;
        }
    }

    // needs_flush: label must be entered with an empty cache (multi-pred or backward)
    // use_snap:    label reached only by one forward jump, so its cache state is restored
    for (int i = 0; i < range_size; i++) {                       // classify every label
        if (!is_target[i]) continue;                             // pc is not a label
        int tgt = start + i;                                     // absolute target pc
        if (has_backward_jump(chunk, start, end, tgt) || pred_count[i] > 1)
            needs_flush[i] = 1;                                  // multi-pred / backward: flush
        else
            use_snap[i] = 1;                                     // unique forward jump: snapshot
    }

    for (int i = 0; i < range_size; i++) label_off[i] = -1;      // no label emitted yet
    int fixup_count = 0;                                         // pending jumps count

    size_t rip_fixup_at[64];                                     // rip-relative disp32 offsets
    int    rip_fixup_imm[64];                                    // imm index per fixup
    int    rip_fixup_count = 0;                                  // pending rip fixups

    size_t mark = cb->len;                                       // rollback point

    XmmCache cache;                                              // register cache state
    (void)jump_snap;                                             // read on labels, written at jumps
    x86_cache_clear(&cache);                                     // start empty

    int pocket_slot_base = nregs;                                // first slot for gpr saves
    int base_frame  = align16(8 * (nregs + n_pockets));          // incl. gpr save slots only
    int frame_size  = base_frame + abi->frame_extra;             // plus abi extras

    emit_prologue(cb, abi, frame_size, base_frame);

    for (int g = 0; g < n_pockets; g++) {                        // save callee-saved gprs
        int gpr = x86_gpr_pocket_regs[g];
        int disp = -8 * (pocket_slot_base + g + 1);
        x86_emit_store_r64_rbp(cb, gpr, disp);
    }

    int cached_args = arity < XMM_CACHE_REGS ? arity : XMM_CACHE_REGS;  // args fit in cache
    for (int i = 0; i < cached_args; i++) {                      // cache incoming args
        x86_cache_put(&cache, i, i);                             // xmm<i> = slot i, dirty
    }
    for (int i = cached_args; i < arity; i++) {                  // spill extras
        x86_emit_movsd_store(cb, i, x86_slot_disp(i));           // xmm<i> -> slot i
    }

    for (int pc = start; pc < end; pc++) {                       // emit each bytecode
        label_off[pc - start] = (int32_t)cb->len;                // record label address

        if (use_snap[pc - start]) {                              // unique forward-jump label
            x86_cache_restore(&cache, &jump_snap[pc - start]);   // restore snapshotted state
        }

        Instruction* inst = &chunk->code[pc];
        int d = inst->operands[0];                               // first operand
        int a = inst->operands[1];                               // second operand
        int b = inst->operands[2];                               // third operand
        Opcode op = inst->opcode;                                // opcode shorthand
        bool did_flush = false;                                  // cache flushed this step

        bool prefer_xmm0 = false;                                // next op returns this dest?
        if (pc + 1 < end) {                                      // next instruction exists
            Instruction* nx = &chunk->code[pc + 1];
            if ((nx->opcode == OP_RETURN || nx->opcode == OP_RETURN_NUM ||
                 nx->opcode == OP_RETURN_BOOL) &&
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
                if (x < 0) { emit_return_zero(abi, cb, base_frame); break; }
                int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == a) { imm_idx = i; break; }
                if (imm_idx >= 0) {
                    size_t at = x86_emit_movsd_load_rip(cb, x);  // movsd xmm, [rip+disp32]
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                } else {
                    x86_emit_load_double_imm(cb, x, a);
                }
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
            case OP_LOAD_BOOL: {                                 // boolean literal
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                uint64_t bits = X86_BOOL_BITS | (a ? 1ULL : 0ULL);  // bit 0 carries the value
                x86_emit_movabs_rax(cb, bits);                   // rax = nan-boxed bool
                x86_emit_movq_xmm_rax(cb, x);                    // xmmX = rax
                x86_cache_put(&cache, x, d);                     // cache dest
                break;
            }
            case OP_LOAD_NONE: {                                 // none literal
                int x = x86_cache_alloc_excl(&cache, cb, -1, -1);  // pick a register
                if (x < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_movabs_rax(cb, X86_NONE_BITS);          // rax = NONE bit pattern
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
                    // xmm0 (holding b) is overwritten with the result
                    if (d != b && cache.slot_dirty[b] &&
                        slot_read_before_write(chunk, pc + 1, end, b)) {
                        x86_emit_movsd_store(cb, 0, x86_slot_disp(b));  // preserve b
                        cache.slot_dirty[b] = false;
                    }
                    x86_emit_sse_arith_rr(cb, arith_op, 0, xa);  // xmm0 = xmm0 op xa
                    x86_cache_put(&cache, 0, d);                 // relabel xmm0 as dest
                } else {
                    // xa (holding a) is overwritten with the result
                    if (d != a && cache.slot_dirty[a] &&
                        slot_read_before_write(chunk, pc + 1, end, a)) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));  // preserve a
                        cache.slot_dirty[a] = false;
                    }
                    x86_emit_sse_arith_rr(cb, arith_op, xa, xb); // xa op= xb
                    x86_cache_put(&cache, xa, d);                // relabel xa as dest
                }
                break;
            }
            case OP_ADD_IMM:
            case OP_SUB_IMM:
            case OP_MUL_IMM:
            case OP_DIV_IMM: {                                   // binary arithmetic with immediate
                int xa = x86_cache_load(&cache, cb, a);
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }
                uint8_t arith_op;
                switch (op) {
                    case OP_ADD_IMM: arith_op = 0x58; break;
                    case OP_SUB_IMM: arith_op = 0x5C; break;
                    case OP_MUL_IMM: arith_op = 0x59; break;
                    default:         arith_op = 0x5E; break;
                }
                        int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == b) { imm_idx = i; break; }
                bool preserve = (d != a) &&
                    slot_read_before_write(chunk, pc + 1, end, a);
                if (imm_idx >= 0) {
                    if (preserve) {
                        int xd = x86_cache_alloc_excl(&cache, cb, xa, -1);
                        if (xd >= 0) {
                            x86_emit_sse66_rr(cb, 0x28, xd, xa);
                            size_t at = x86_emit_sse_arith_rip(cb, arith_op, xd);
                            rip_fixup_at[rip_fixup_count] = at;
                            rip_fixup_imm[rip_fixup_count] = imm_idx;
                            rip_fixup_count++;
                            x86_cache_put(&cache, xd, d);
                            break;
                        }
                    }
                    size_t at = x86_emit_sse_arith_rip(cb, arith_op, xa);
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                    x86_cache_put(&cache, xa, d);
                    break;
                }
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                if (preserve) {
                    int xd = x86_cache_alloc_excl(&cache, cb, xa, XMM_SCRATCH);
                    if (xd >= 0) {
                        x86_emit_sse66_rr(cb, 0x28, xd, xa);
                        x86_emit_sse_arith_rr(cb, arith_op, xd, XMM_SCRATCH);
                        x86_cache_put(&cache, xd, d);
                        break;
                    }
                }
                x86_emit_sse_arith_rr(cb, arith_op, xa, XMM_SCRATCH);
                x86_cache_put(&cache, xa, d);
                break;
            }
            case OP_MOD_IMM: {                                   // modulo with immediate: a - trunc(a/imm)*imm
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);    // xmm7 = (double)b (cvtsi2sd)
                int xt = x86_cache_alloc_excl(&cache, cb, xa, XMM_SCRATCH);  // temp for a/imm
                if (xt < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, xt, xa);             // xt = a
                x86_emit_sse_arith_rr(cb, 0x5E, xt, XMM_SCRATCH);// xt = a / imm
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, xt, xt);                          // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);                               // roundsd opcode
                emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));  // round xt, xt
                emit_u8(cb, 0x03);                               // round mode: truncate
                x86_emit_sse_arith_rr(cb, 0x59, xt, XMM_SCRATCH);// xt = trunc(a/imm) * imm
                // dest != source: a must survive the in-place subtract
                if (d != a && cache.slot_dirty[a] &&
                    slot_read_before_write(chunk, pc + 1, end, a)) {
                    x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                    cache.slot_dirty[a] = false;
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xa, xt);         // a = a - xt
                x86_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_MOD: {                                       // x - trunc(x/y)*y
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);    // movapd scratch, a
                x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);  // divsd  scratch, b
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, XMM_SCRATCH, XMM_SCRATCH);        // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);                               // roundsd opcode
                emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3) | (XMM_SCRATCH & 7));  // modrm scratch, scratch
                emit_u8(cb, 0x03);                               // round mode: truncate
                x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);  // mulsd  scratch, b
                // dest != source: a must survive the in-place subtract
                if (d != a && cache.slot_dirty[a] &&
                    slot_read_before_write(chunk, pc + 1, end, a)) {
                    x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                    cache.slot_dirty[a] = false;
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);  // subsd  a, scratch
                x86_cache_put(&cache, xa, d);                    // relabel xa as dest
                break;
            }
            case OP_NEG: {                                       // unary minus
                int xa = x86_cache_load(&cache, cb, a);          // load operand
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // xorpd scratch, scratch
                x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);  // subsd scratch, a
                // dest != source: a must survive the in-place movapd
                if (d != a && cache.slot_dirty[a] &&
                    slot_read_before_write(chunk, pc + 1, end, a)) {
                    x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                    cache.slot_dirty[a] = false;
                }
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
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_cmp_box_result(cb, xd, 0x94);           // setne? no: sete al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_NEQ:
            case OP_CMP_NEQ_NUM: {                               // d = (a != b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_cmp_box_result(cb, xd, 0x95);           // setne al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LT: {                                    // d = (a < b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_cmp_box_result(cb, xd, 0x92);           // setb al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GT: {                                    // d = (a > b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_cmp_box_result(cb, xd, 0x97);           // seta al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_LTE: {                                   // d = (a <= b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_cmp_box_result(cb, xd, 0x96);           // setbe al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_CMP_GTE: {                                   // d = (a >= b)
                int xa = x86_cache_load(&cache, cb, a);          // load a
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);  // load b, avoid xa
                if (xb < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                int xd = x86_cache_alloc_excl(&cache, cb, xa, xb);  // pick dest register
                if (xd < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_cmp_box_result(cb, xd, 0x93);           // setae al, boxed MAKE_BOOL
                x86_cache_put(&cache, xd, d);                    // cache dest
                break;
            }
            case OP_JUMP: {                                      // unconditional jump
                if (d >= start && d < end) {                     // in-range target only
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);                 // out of range: safest
                }
                emit_u8(cb, 0xE9);                               // jmp rel32 opcode
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_FALSE: {                             // branch when cond is false
                int xa = x86_cache_load(&cache, cb, a);          // load condition
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }  // no register available
                x86_emit_movq_rax_xmm(cb, xa);                   // rax = raw 64-bit slot
                emit_u8(cb, 0xA8); emit_u8(cb, 0x01);            // test al, 1
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je rel32 (bit0 == 0 -> false)
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
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
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
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
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
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
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
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
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
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
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
                x86_emit_ucomisd_rr(cb, xa, xb);                 // ucomisd a, b
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x83);            // jae rel32
                fixups[fixup_count].patch_at  = cb->len;         // record placeholder
                fixups[fixup_count].target_pc = d;               // remember target pc
                fixup_count++;                                   // one more pending fixup
                emit_i32(cb, 0);                                 // placeholder
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_JUMP_IF_EQ_IMM:                              // branch if a == imm
            case OP_JUMP_IF_NEQ_IMM:
            case OP_JUMP_IF_LT_IMM:
            case OP_JUMP_IF_GT_IMM:
            case OP_JUMP_IF_LTE_IMM:
            case OP_JUMP_IF_GTE_IMM: {                           // imm-jump variants share the pattern
                int xa = x86_cache_load(&cache, cb, a);          // load left operand
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }
                int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == b) { imm_idx = i; break; }
                if (imm_idx >= 0) {
                    size_t at = x86_emit_ucomisd_rip(cb, xa);    // ucomisd xa, [rip+disp32]
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                } else {
                    x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                    x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);
                }
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                uint8_t jcc;
                switch (op) {
                    case OP_JUMP_IF_EQ_IMM:  jcc = 0x84; break;  // je
                    case OP_JUMP_IF_NEQ_IMM: jcc = 0x85; break;  // jne
                    case OP_JUMP_IF_LT_IMM:  jcc = 0x82; break;  // jb
                    case OP_JUMP_IF_GT_IMM:  jcc = 0x87; break;  // ja
                    case OP_JUMP_IF_LTE_IMM: jcc = 0x86; break;  // jbe
                    default:                 jcc = 0x83; break;  // jae (gte)
                }
                emit_u8(cb, 0x0F); emit_u8(cb, jcc);             // conditional jump
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_NUM: {                            // jump if R[a] is a number == const[b]
                int xa = x86_cache_load(&cache, cb, a);          // load subject
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }
                double c = chunk->constants[b].number_value;     // fetch constant
                uint64_t cbits; memcpy(&cbits, &c, 8);           // reinterpret as u64
                x86_emit_movabs_rax(cb, cbits);                  // rax = constant bits
                x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);          // scratch = constant
                x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);        // ucomisd subj, const
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x8A);            // jp skip (subj was NaN-tagged)
                size_t skip_patch = cb->len;
                emit_i32(cb, 0);
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je target
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                int32_t skip_rel = (int32_t)cb->len - (int32_t)(skip_patch + 4);
                memcpy(cb->buf + skip_patch, &skip_rel, 4);      // patch skip
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_STR: {                            // jump if R[a] is a string == const[b]
                x86_cache_flush(&cache, cb);                     // call clobbers xmm
#if defined(_WIN32) || defined(_WIN64)
                x86_emit_load_r64_rbp(cb, X86_RCX, x86_slot_disp(a));  // win64: rcx = subject
#else
                x86_emit_load_r64_rbp(cb, X86_RDI, x86_slot_disp(a));  // sysv: rdi = subject
#endif
                uint64_t addr = (uint64_t)(uintptr_t)&chunk->constants[b].cached_str;
                x86_emit_movabs_rax(cb, addr);                   // rax = &cached_str
#if defined(_WIN32) || defined(_WIN64)
                x86_emit_load_r64_base(cb, X86_RDX, X86_RAX, 0); // win64: rdx = cached_str
#else
                x86_emit_load_r64_base(cb, X86_RSI, X86_RAX, 0); // sysv: rsi = cached_str
#endif
                uint64_t helper = (uint64_t)(uintptr_t)&jit_match_str;
                x86_emit_movabs_rax(cb, helper);                 // rax = helper
                emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);            // call rax
                emit_u8(cb, 0x85); emit_u8(cb, 0xC0);            // test eax, eax
                x86_cache_clear(&cache);                         // callee clobbered xmm
                emit_u8(cb, 0x0F); emit_u8(cb, 0x85);            // jne target  (match → jump)
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_BOOL: {                           // jump if R[a] == MAKE_BOOL(b)
                int xa = x86_cache_load(&cache, cb, a);
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }
                x86_emit_movq_rax_xmm(cb, xa);                   // rax = subject bits
                uint64_t expect = X86_BOOL_BITS | (b ? 1ULL : 0ULL);
                x86_emit_movabs_r11(cb, expect);                 // r11 = expected bool bits
                emit_u8(cb, 0x4C); emit_u8(cb, 0x39); emit_u8(cb, 0xD8);  // cmp rax, r11
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je target
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_JUMP_MATCH_NONE: {                           // jump if R[a] == MAKE_NONE()
                int xa = x86_cache_load(&cache, cb, a);
                if (xa < 0) { emit_return_zero(abi, cb, base_frame); break; }
                x86_emit_movq_rax_xmm(cb, xa);                   // rax = subject bits
                x86_emit_movabs_r11(cb, X86_NONE_BITS);          // r11 = NONE bits
                emit_u8(cb, 0x4C); emit_u8(cb, 0x39); emit_u8(cb, 0xD8);  // cmp rax, r11
                if (d >= start && d < end) {
                    if (needs_flush[d - start]) x86_cache_flush(&cache, cb);
                    else if (use_snap[d - start]) x86_cache_snap(&jump_snap[d - start], &cache);
                } else {
                    x86_cache_flush(&cache, cb);
                }
                emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je target
                fixups[fixup_count].patch_at  = cb->len;
                fixups[fixup_count].target_pc = d;
                fixup_count++;
                emit_i32(cb, 0);
                did_flush = true;
                break;
            }
            case OP_CALL_0: {                                    // call with no args
                x86_cache_flush(&cache, cb);                     // spill everything before call
                emit_call_or_self(cb, ctx, a, func_idx, mark);   // direct self-call or via table
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
                    if (!cache.slot_dirty[s]) continue;          // clean slot, nothing to spill
                    int g = (s < JIT_MAX_SLOTS) ? slot_pocket_gpr[s] : -1;
                    if (g >= 0) x86_emit_movq_gpr_xmm(cb, x86_gpr_pocket_regs[g], i);
                    else        x86_emit_movsd_store(cb, i, x86_slot_disp(s));
                    cache.slot_dirty[s] = false;
                }
                if (arg_xmm >= 0 && arg_xmm != 0) {              // arg in cache, not xmm0
                    x86_emit_sse66_rr(cb, 0x28, 0, arg_xmm);     // movapd xmm0, arg_xmm
                } else if (arg_xmm < 0) {                        // arg not cached
                    int g = (b >= 0 && b < JIT_MAX_SLOTS) ? slot_pocket_gpr[b] : -1;
                    if (g >= 0) x86_emit_movq_xmm_gpr(cb, 0, x86_gpr_pocket_regs[g]);
                    else        x86_emit_movsd_load(cb, 0, x86_slot_disp(b));
                }                                                // else arg already in xmm0
                x86_cache_clear(&cache);                         // xmm regs clobbered by callee
                emit_call_or_self(cb, ctx, a, func_idx, mark);   // direct self-call or via table
                x86_cache_put(&cache, 0, d);                     // xmm0 = result
                for (int g = 0; g < n_pockets; g++) {            // restore pockets live after call
                    int s = slot_to_pocket[g];
                    if (s < 0 || s == d) continue;               // result already holds d
                    if (!slot_read_before_write(chunk, pc + 1, end, s)) continue;
                    int x = x86_cache_alloc_excl(&cache, cb, -1, -1);
                    if (x < 0) continue;                         // register cache full
                    x86_emit_movq_xmm_gpr(cb, x, x86_gpr_pocket_regs[g]);
                    cache.reg_slot[x] = s;
                    cache.slot_reg[s] = x;
                    cache.slot_dirty[s] = false;
                }
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
                    if (!cache.slot_dirty[s]) continue;          // clean slot, nothing to spill
                    int g = (s < JIT_MAX_SLOTS) ? slot_pocket_gpr[s] : -1;
                    if (g >= 0) x86_emit_movq_gpr_xmm(cb, x86_gpr_pocket_regs[g], i);
                    else        x86_emit_movsd_store(cb, i, x86_slot_disp(s));
                    cache.slot_dirty[s] = false;
                }
                if (arg0_xmm >= 0) {                             // arg0 in cache
                    x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, arg0_xmm);
                } else {                                         // arg0 not cached
                    int g = (b >= 0 && b < JIT_MAX_SLOTS) ? slot_pocket_gpr[b] : -1;
                    if (g >= 0) x86_emit_movq_xmm_gpr(cb, XMM_SCRATCH, x86_gpr_pocket_regs[g]);
                    else        x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(b));
                }
                if (arg1_xmm >= 0 && arg1_xmm != 1) {            // arg1 in cache, not xmm1
                    x86_emit_sse66_rr(cb, 0x28, 1, arg1_xmm);
                } else if (arg1_xmm < 0) {                       // arg1 not cached
                    int g = (b + 1 >= 0 && b + 1 < JIT_MAX_SLOTS) ? slot_pocket_gpr[b + 1] : -1;
                    if (g >= 0) x86_emit_movq_xmm_gpr(cb, 1, x86_gpr_pocket_regs[g]);
                    else        x86_emit_movsd_load(cb, 1, x86_slot_disp(b + 1));
                }
                x86_emit_sse66_rr(cb, 0x28, 0, XMM_SCRATCH);     // arg0 from scratch to xmm0
                x86_cache_clear(&cache);                         // xmm regs clobbered
                emit_call_or_self(cb, ctx, a, func_idx, mark);   // direct self-call or via table
                x86_cache_put(&cache, 0, d);                     // xmm0 = result
                for (int g = 0; g < n_pockets; g++) {            // restore pockets live after call
                    int s = slot_to_pocket[g];
                    if (s < 0 || s == d) continue;
                    if (!slot_read_before_write(chunk, pc + 1, end, s)) continue;
                    int x = x86_cache_alloc_excl(&cache, cb, -1, -1);
                    if (x < 0) continue;
                    x86_emit_movq_xmm_gpr(cb, x, x86_gpr_pocket_regs[g]);
                    cache.reg_slot[x] = s;
                    cache.slot_reg[s] = x;
                    cache.slot_dirty[s] = false;
                }
                break;
            }
            case OP_RETURN:                                      // return slot value
            case OP_RETURN_NUM:                                  // return number from register
            case OP_RETURN_BOOL: {
                int xa = x86_cache_load(&cache, cb, d);          // load return value
                if (xa < 0) xa = 0;                              // fall back to xmm0
                if (xa != 0) x86_emit_sse66_rr(cb, 0x28, 0, xa); // movapd xmm0, xa
                emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);                         // clear state on exit
                did_flush = true;                                // suppress merge flush
                break;
            }
            case OP_RETURN_NUM_IMM: {                            // return number immediate
                int imm_idx = -1;                                // index into the rip pool
                for (int i = 0; i < func_n_imms; i++)
                    if (func_imms[i] == a) { imm_idx = i; break; }
                if (imm_idx >= 0) {
                    size_t at = x86_emit_movsd_load_rip(cb, 0);  // movsd xmm0, [rip+disp32]
                    rip_fixup_at[rip_fixup_count] = at;
                    rip_fixup_imm[rip_fixup_count] = imm_idx;
                    rip_fixup_count++;
                } else {
                    x86_emit_load_double_imm(cb, 0, a);
                }
                emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);
                did_flush = true;
                break;
            }
            case OP_RETURN_NONE: {                               // return none, no value
                emit_u8(cb, 0x66); emit_u8(cb, 0x0F);
                emit_u8(cb, 0x57); emit_u8(cb, 0xC0);            // xorpd xmm0, xmm0
                emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);
                emit_leave_ret(cb, abi, base_frame);
                x86_cache_clear(&cache);                         // clear state on exit
                did_flush = true;                                // suppress merge flush
                break;
            }
            default:                                             // unreachable in pure fn
                emit_return_zero(abi, cb, base_frame);           // safe fallback
                break;
        }

        // flush if the next instruction is a merge point — the incoming cache
        // state at a multi-predecessor label must be consistent across edges
        if (!did_flush && pc + 1 < end && needs_flush[pc + 1 - start]) {
            x86_cache_flush(&cache, cb);
        }
    }

    emit_gpr_pocket_restores(cb, pocket_slot_base, n_pockets);   // restore before leaving
    emit_return_zero(abi, cb, base_frame);                       // safety fallthrough

    size_t pool_off = cb->len;                                   // constant pool starts here
    for (int i = 0; i < func_n_imms; i++) {                      // append pool of double constants
        double dv = (double)func_imms[i];
        uint64_t bits; memcpy(&bits, &dv, 8);
        emit_u64(cb, bits);
    }
    for (int i = 0; i < rip_fixup_count; i++) {                  // patch rip-relative displacements
        size_t patch_at = rip_fixup_at[i];
        size_t imm_at   = pool_off + 8 * (size_t)rip_fixup_imm[i];
        int32_t rel = (int32_t)imm_at - (int32_t)(patch_at + 4);
        memcpy(cb->buf + patch_at, &rel, 4);
    }

    for (int i = 0; i < fixup_count; i++) {                      // patch every pending jump
        JumpFixup* fx = &fixups[i];
        int tidx = fx->target_pc - start;                        // index within range
        if (tidx < 0 || tidx >= range_size || label_off[tidx] < 0) {
            cb->len = mark;                                      // rollback partial emission
            free(pred_count); free(needs_flush); free(use_snap); free(jump_snap);
            return false;                                        // invalid target
        }
        int32_t rel = (int32_t)label_off[tidx] - (int32_t)(fx->patch_at + 4);  // rel32 distance
        memcpy(cb->buf + fx->patch_at, &rel, 4);                 // write rel32
    }

    free(pred_count); free(needs_flush); free(use_snap); free(jump_snap);

    *out_fn = (void*)(cb->buf + mark);                           // publish entry pointer
    return true;                                                 // emission successful
}

// emits one non-jump loop-body instruction using the xmm register cache
static void emit_loop_body_instr(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                  XmmCache* cache, int pc, int const_slot, int save_slot,
                                  JitLoopInfo* info) {
    (void)abi;                                                   // reserved for ABI-specific opcodes
    BytecodeChunk* chunk = ctx->chunk;
    Instruction* inst = &chunk->code[pc];
    int d = inst->operands[0];
    int a = inst->operands[1];
    int b = inst->operands[2];

    bool touched_nan_check = false;                              // set by arithmetic ops

    switch (inst->opcode) {
        case OP_MOVE: {
            if (d == a) break;                               // self-move is a no-op
            int xa = x86_cache_load(cache, cb, a);           // load source slot
            if (xa < 0) return;                              // register cache full
            if (!slot_live_in_loop(chunk, info->entry_pc,
                                   info->back_edge_pc, pc, a)) {
                x86_cache_put(cache, xa, d);                 // a is dead: relabel
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, -1);
                if (xd < 0) return;                          // register cache full
                if (xd != xa)
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);     // movapd xd, xa
                x86_cache_put(cache, xd, d);                 // a survives in xa
            }
            break;
        }
        case OP_LOAD_NUM_IMM: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            x86_emit_load_double_imm(cb, x, a);                  // xmmX = (double)a (cvtsi2sd)
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_LOAD_NUM: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            double v = chunk->constants[a].number_value;         // fetch pool constant
            uint64_t bits; memcpy(&bits, &v, 8);                 // reinterpret as u64
            x86_emit_movabs_rax(cb, bits);                       // rax = bit pattern
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_LOAD_BOOL: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            uint64_t bits = X86_BOOL_BITS | (a ? 1ULL : 0ULL);   // bit 0 carries value
            x86_emit_movabs_rax(cb, bits);                       // rax = nan-boxed bool
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_LOAD_NONE: {
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            x86_emit_movabs_rax(cb, X86_NONE_BITS);              // rax = NONE bit pattern
            x86_emit_movq_xmm_rax(cb, x);                        // xmmX = rax
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_TABLE_SET_KEY_STR: {                         // tbl["prefix" .. num] = val
            if (save_slot < 0) return;
            x86_cache_flush(cache, cb);

            int table_reg  = d;                              // operands[0]
            int val_reg    = a;                              // operands[1]
            int prefix_idx = (int)((uint32_t)b >> 16);       // unpack
            int num_reg    = b & 0xFFFF;

            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save caller frame pointer BEFORE clobbering

            // rdi = raw Table*
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));
            x86_emit_clear_high16_rax(cb);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);  // mov rcx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);  // mov rdi, rax
#endif

            // rsi / rdx = prefix
            uint64_t prefix_addr = (uint64_t)(uintptr_t)chunk->constants[prefix_idx].cached_str;
            x86_emit_movabs_rax(cb, prefix_addr);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

            // xmm0 = num (sysv) / xmm2 = num (win64: arg 2 is a float)
            x86_emit_movsd_load(cb, 0, x86_slot_disp(num_reg));
#if defined(_WIN32) || defined(_WIN64)
            x86_emit_sse66_rr(cb, 0x28, 2, 0);               // win64: xmm2 = num
#endif

            // last arg: value
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(val_reg));
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x49); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);  // mov r9, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#endif

            uint64_t helper = (uint64_t)(uintptr_t)&jit_table_set_key_str;
            x86_emit_movabs_rax(cb, helper);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore caller frame pointer

            x86_cache_clear(cache);
            break;
        }
        case OP_TABLE_GET_KEY_STR: {                         // d = tbl["prefix" .. num]
            if (save_slot < 0) return;
            x86_cache_flush(cache, cb);

            int table_reg  = a;                              // operands[1]
            int prefix_idx = (int)((uint32_t)b >> 16);       // unpack
            int num_reg    = b & 0xFFFF;

            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save caller frame pointer BEFORE clobbering

            // rdi = raw Table*
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));
            x86_emit_clear_high16_rax(cb);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);  // mov rcx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);  // mov rdi, rax
#endif

            // rsi / rdx = prefix
            uint64_t prefix_addr = (uint64_t)(uintptr_t)chunk->constants[prefix_idx].cached_str;
            x86_emit_movabs_rax(cb, prefix_addr);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

            // xmm0 = num (sysv) / xmm2 = num (win64: arg 2 is a float)
            x86_emit_movsd_load(cb, 0, x86_slot_disp(num_reg));
#if defined(_WIN32) || defined(_WIN64)
            x86_emit_sse66_rr(cb, 0x28, 2, 0);               // win64: xmm2 = num
#endif

            uint64_t helper = (uint64_t)(uintptr_t)&jit_table_get_key_str;
            x86_emit_movabs_rax(cb, helper);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

            // value result returned in rax; publish it to the destination slot
            x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(d));
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore caller frame pointer

            x86_cache_clear(cache);
            break;
        }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: {
            int xa = x86_cache_load(cache, cb, a);               // load left operand
            if (xa < 0) return;                                  // register cache full
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load right, avoid xa
            if (xb < 0) return;                                  // register cache full
            uint8_t op;                                          // sse opcode
            switch (inst->opcode) {
                case OP_ADD: op = 0x58; break;                   // addsd
                case OP_SUB: op = 0x5C; break;                   // subsd
                case OP_MUL: op = 0x59; break;                   // mulsd
                default:     op = 0x5E; break;                   // divsd
            }
            int xd = xa;                                         // result register, xa by default
            if (d != a) {
                // dest != left source: prefer a separate xmm so a survives
                xd = x86_cache_dest_reg(cache, cb, d, xa, xb);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);         // movapd xd, xa
                }
            }
            x86_emit_sse_arith_rr(cb, op, xd, xb);               // xd op= xb
            x86_cache_put(cache, xd, d);                         // publish dest
            touched_nan_check = true;
            break;
        }
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM: case OP_DIV_IMM: {
            int xa = x86_cache_load(cache, cb, a);               // load left operand
            if (xa < 0) return;                                  // register cache full
            uint8_t op;                                          // sse opcode
            switch (inst->opcode) {
                case OP_ADD_IMM: op = 0x58; break;               // addsd
                case OP_SUB_IMM: op = 0x5C; break;               // subsd
                case OP_MUL_IMM: op = 0x59; break;               // mulsd
                default:         op = 0x5E; break;               // divsd
            }
            int slot = -1;
            if (info->imm_opt_ok) {
                for (int i = 0; i < info->n_imms; i++)
                    if (info->imms[i].value == b) { slot = info->imms[i].slot; break; }
            }
            // dest != left source: spill a before the in-place update
            int xd = xa;                                         // result register, xa by default
            if (d != a) {
                xd = x86_cache_dest_reg(cache, cb, d, xa, XMM_SCRATCH);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);         // movapd xd, xa
                }
            }
            if (slot >= 0) {
                x86_emit_sse_arith_mem(cb, op, xd, x86_slot_disp(slot));  // xd op= [imm]
            } else {
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                x86_emit_sse_arith_rr(cb, op, xd, XMM_SCRATCH);
            }
            x86_cache_put(cache, xd, d);                         // publish dest
            touched_nan_check = true;
            break;
        }
        case OP_MOD_IMM: {                                       // modulo with immediate
            int xa = x86_cache_load(cache, cb, a);               // load a
            if (xa < 0) return;                                  // register cache full
            int slot = -1;
            if (info->imm_opt_ok) {
                for (int i = 0; i < info->n_imms; i++)
                    if (info->imms[i].value == b) { slot = info->imms[i].slot; break; }
            }
            int xt;
            if (slot >= 0) {
                xt = x86_cache_alloc_excl(cache, cb, xa, -1);    // temp for a/imm
                if (xt < 0) return;
                x86_emit_sse66_rr(cb, 0x28, xt, xa);             // xt = a
                x86_emit_sse_arith_mem(cb, 0x5E, xt, x86_slot_disp(slot));  // xt = a / imm
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, xt, xt);                          // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);
                emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));
                emit_u8(cb, 0x03);                               // truncate
                x86_emit_sse_arith_mem(cb, 0x59, xt, x86_slot_disp(slot));  // xt *= imm
            } else {
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                xt = x86_cache_alloc_excl(cache, cb, xa, XMM_SCRATCH);
                if (xt < 0) return;
                x86_emit_sse66_rr(cb, 0x28, xt, xa);
                x86_emit_sse_arith_rr(cb, 0x5E, xt, XMM_SCRATCH);
                emit_u8(cb, 0x66);                               // roundsd legacy prefix
                x86_rex_rb(cb, xt, xt);                          // rex.r/rex.b for xmm8-xmm15
                emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
                emit_u8(cb, 0x0B);
                emit_u8(cb, 0xC0 | ((xt & 7) << 3) | (xt & 7));
                emit_u8(cb, 0x03);
                x86_emit_sse_arith_rr(cb, 0x59, xt, XMM_SCRATCH);
            }
            // dest != left source: preserve a's original value in its frame slot
            if (d == a) {
                x86_emit_sse_arith_rr(cb, 0x5C, xa, xt);         // a = a - xt
                x86_cache_put(cache, xa, d);
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, xt);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);         // movapd xd, xa
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xd, xt);         // xd = xd - xt
                x86_cache_put(cache, xd, d);
            }
            touched_nan_check = true;
            break;
        }
        case OP_MOD: {
            int xa = x86_cache_load(cache, cb, a);               // load a
            if (xa < 0) return;                                  // register cache full
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load b, avoid xa
            if (xb < 0) return;                                  // register cache full
            x86_emit_sse66_rr(cb, 0x28, XMM_SCRATCH, xa);        // scratch = a
            x86_emit_sse_arith_rr(cb, 0x5E, XMM_SCRATCH, xb);    // scratch = a / b
            emit_u8(cb, 0x66);                                   // roundsd legacy prefix
            x86_rex_rb(cb, XMM_SCRATCH, XMM_SCRATCH);            // rex.r/rex.b for xmm8-xmm15
            emit_u8(cb, 0x0F); emit_u8(cb, 0x3A);
            emit_u8(cb, 0x0B);                                   // roundsd opcode
            emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3) | (XMM_SCRATCH & 7));  // round scratch
            emit_u8(cb, 0x03);                                   // round mode: truncate
            x86_emit_sse_arith_rr(cb, 0x59, XMM_SCRATCH, xb);    // scratch = trunc(a/b) * b
            // dest != left source: preserve a's original value in its frame slot
            if (d == a) {
                x86_emit_sse_arith_rr(cb, 0x5C, xa, XMM_SCRATCH);   // a = a - scratch
                x86_cache_put(cache, xa, d);
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, -1);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                } else if (xd != xa) {
                    x86_emit_sse66_rr(cb, 0x28, xd, xa);             // movapd xd, xa
                }
                x86_emit_sse_arith_rr(cb, 0x5C, xd, XMM_SCRATCH);   // xd = xd - scratch
                x86_cache_put(cache, xd, d);
            }
            touched_nan_check = true;
            break;
        }
        case OP_NEG: {
            int xa = x86_cache_load(cache, cb, a);               // load operand
            if (xa < 0) return;                                  // register cache full
            x86_emit_sse66_rr(cb, 0x57, XMM_SCRATCH, XMM_SCRATCH);  // scratch = 0
            x86_emit_sse_arith_rr(cb, 0x5C, XMM_SCRATCH, xa);    // scratch = 0 - a
            // dest != source: preserve a's original value in its frame slot
            if (d == a) {
                x86_emit_sse66_rr(cb, 0x28, xa, XMM_SCRATCH);        // a = -a
                x86_cache_put(cache, xa, d);
            } else {
                int xd = x86_cache_dest_reg(cache, cb, d, xa, -1);
                if (xd < 0) {                                    // no room: spill a
                    if (cache->slot_dirty[a]) {
                        x86_emit_movsd_store(cb, xa, x86_slot_disp(a));
                        cache->slot_dirty[a] = false;
                    }
                    xd = xa;
                }
                x86_emit_sse66_rr(cb, 0x28, xd, XMM_SCRATCH);        // xd = -a
                x86_cache_put(cache, xd, d);
            }
            touched_nan_check = true;
            break;
        }
        case OP_INC: case OP_DEC: {
            int xd = x86_cache_load(cache, cb, d);               // load dest
            if (xd < 0) return;                                  // register cache full
            uint8_t op = (inst->opcode == OP_INC) ? 0x58 : 0x5C; // addsd / subsd
            x86_emit_sse_arith_mem(cb, op, xd, x86_slot_disp(const_slot));  // xd op= 1.0
            x86_cache_put(cache, xd, d);                         // mark dest dirty
            touched_nan_check = true;
            break;
        }
        case OP_CMP_EQ: case OP_CMP_EQ_NUM:
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE: {
            int xa = x86_cache_load(cache, cb, a);               // load a
            if (xa < 0) return;                                  // register cache full
            int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load b, avoid xa
            if (xb < 0) return;                                  // register cache full
            x86_emit_ucomisd_rr(cb, xa, xb);                     // ucomisd a, b
            uint8_t cc;                                          // setcc opcode
            switch (inst->opcode) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  cc = 0x94; break;  // sete
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: cc = 0x95; break;  // setne
                case OP_CMP_LT:  cc = 0x92; break;               // setb
                case OP_CMP_GT:  cc = 0x97; break;               // seta
                case OP_CMP_LTE: cc = 0x96; break;               // setbe
                default:         cc = 0x93; break;               // setae (gte)
            }
            // reuse the xmm currently holding d, if any: keeps the dest in
            // the same register across a preceding JUMP_IF_FALSE local-skip,
            // so both paths reach the successor with an identical cache state
            int xd = x86_cache_lookup(cache, d);
            if (xd == xa || xd == xb) xd = -1;                   // dest aliases a source
            if (xd < 0) xd = x86_cache_dest_reg(cache, cb, d, xa, xb); // prefer d's home
            if (xd < 0) return;                                  // register cache full
            x86_emit_cmp_box_result(cb, xd, cc);                 // nan-boxed MAKE_BOOL into xd
            x86_cache_put(cache, xd, d);                         // cache dest
            break;
        }
        case OP_CALL_0:                                      // call with no args
        case OP_CALL_1:                                      // call with one arg
        case OP_CALL_2: {                                    // call with two args
            if (save_slot < 0) return;                       // no save slot reserved, bail
            x86_cache_flush(cache, cb);                      // spill dirty slots to frame
            if (inst->opcode == OP_CALL_1 || inst->opcode == OP_CALL_2) {
                x86_emit_movsd_load(cb, 0, x86_slot_disp(b));  // xmm0 = arg0
            }
            if (inst->opcode == OP_CALL_2) {
                x86_emit_movsd_load(cb, 1, x86_slot_disp(b + 1));  // xmm1 = arg1
            }
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg
            emit_call_func(cb, ctx, a);                      // indirect call through func_table
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));   // restore frame_reg
            x86_cache_clear(cache);                          // callee clobbered all xmm regs
            x86_cache_put(cache, 0, d);                      // xmm0 holds the result
            break;
        }
        case OP_TABLE_GET_NUM:                               // same semantics as OP_TABLE_GET
        case OP_TABLE_GET: {
            int table_reg = a;                               // table register
            int key_reg   = b;                               // key register

            // fast path: counter key + this loop's primary table
            if (key_reg == info->for_var_reg && table_reg == info->table.slot) {
                int xk = cache->slot_reg[key_reg];           // key already in cache?
                if (xk < 0) {
                    xk = x86_cache_load(cache, cb, key_reg); // load key from frame
                    if (xk < 0) return;                      // register cache full
                }
                x86_emit_cvttsd2si_eax(cb, xk);              // eax = (int)key
                x86_emit_dec_eax(cb);                        // 1-based -> 0-based
                int xd = x86_cache_dest_reg(cache, cb, d, xk, -1); // prefer d's home
                if (xd < 0) return;                          // register cache full
                x86_emit_movsd_load_idx8(cb, xd, X86_RBX, X86_RAX);  // xd = array_part[rax]
                x86_cache_put(cache, xd, d);                 // cache dest
                break;
            }

            // general path: fetch the table pointer, convert the key, index array_part
            int xk = cache->slot_reg[key_reg];
            if (xk < 0) {
                xk = x86_cache_load(cache, cb, key_reg);
                if (xk < 0) return;                          // register cache full
            }

            int xt = cache->slot_reg[table_reg];             // table still in an xmm?
            if (xt >= 0) {
                x86_emit_movq_rax_xmm(cb, xt);               // rax = table bits
            } else {
                x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));  // rax = frame[table_reg]
            }
            x86_emit_clear_high16_rax(cb);                   // strip nan-box tag
            x86_emit_load_r64_base(cb, X86_RAX, X86_RAX,
                                (int32_t)offsetof(Table, array_part));  // rax = array_part
            x86_emit_cvttsd2si_edx(cb, xk);                  // edx = (int)key
            x86_emit_dec_edx(cb);                            // 1-based -> 0-based
            int xd = x86_cache_dest_reg(cache, cb, d, xk, -1);  // prefer d's home
            if (xd < 0) return;                              // register cache full
            x86_emit_movsd_load_idx8(cb, xd, X86_RAX, X86_RDX);  // xd = array_part[rdx]
            x86_cache_put(cache, xd, d);                     // cache dest
            break;
        }
        case OP_TABLE_GET_INT: {
            int xd = x86_cache_dest_reg(cache, cb, d, -1, -1);   // prefer d's home
            if (xd < 0) return;                                  // register cache full
            x86_emit_movsd_load_base(cb, X86_RBX, xd, (b - 1) * 8);  // xd = array_part[b-1]
            x86_cache_put(cache, xd, d);                         // cache dest
            break;
        }
        case OP_TABLE_SET_NUM:                               // same semantics as OP_TABLE_SET
        case OP_TABLE_SET: {
            // d = table, a = key, b = value
            int table_reg = d;                               // table register
            int key_reg   = a;                               // key register
            int val_reg   = b;                               // value register

            // fast path: counter key + this loop's primary table
            if (key_reg == info->for_var_reg && table_reg == info->table.slot) {
                int xk = cache->slot_reg[key_reg];
                if (xk < 0) {
                    xk = x86_cache_load(cache, cb, key_reg);
                    if (xk < 0) return;                      // register cache full
                }
                x86_emit_cvttsd2si_eax(cb, xk);              // eax = (int)key
                x86_emit_dec_eax(cb);                        // 1-based -> 0-based
                int xv = x86_cache_load_excl(cache, cb, val_reg, xk, -1);  // load value, keep key
                if (xv < 0) return;                          // register cache full
                x86_emit_movsd_store_idx8(cb, X86_RBX, X86_RAX, xv);  // array_part[rax] = xv
                break;
            }

            // general path: call the runtime helper so array_part is grown safely
            if (save_slot < 0) return;                    // no save slot reserved, bail
            int xk = cache->slot_reg[key_reg];
            if (xk < 0) {
                xk = x86_cache_load(cache, cb, key_reg);
                if (xk < 0) return;                       // register cache full
            }
            int xv = cache->slot_reg[val_reg];
            if (xv < 0) {
                xv = x86_cache_load(cache, cb, val_reg);
                if (xv < 0) return;                       // register cache full
            }

            x86_cache_flush(cache, cb);                   // spill dirty slots before the call
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

#if defined(_WIN32) || defined(_WIN64)
            x86_emit_cvttsd2si_edx(cb, xk);               // edx = (int)key
            x86_emit_dec_edx(cb);                         // 1-based -> 0-based
            // rdx = index (upper 32 already zeroed)
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));  // rax = frame[table_reg]
            x86_emit_clear_high16_rax(cb);                // strip nan-box tag
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);       // mov rcx, rax
            x86_emit_movq_rax_xmm(cb, xv);                // rax = value bits
            emit_u8(cb, 0x49); emit_u8(cb, 0x89); emit_u8(cb, 0xC0);       // mov r8, rax
#else
            x86_emit_cvttsd2si_edx(cb, xk);               // edx = (int)key
            x86_emit_dec_edx(cb);                         // 1-based -> 0-based
            emit_u8(cb, 0x89); emit_u8(cb, 0xD6);         // mov esi, edx
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));  // rax = frame[table_reg]
            x86_emit_clear_high16_rax(cb);                // strip nan-box tag
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);       // mov rdi, rax
            x86_emit_movq_rax_xmm(cb, xv);                // rax = value bits
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);       // mov rdx, rax
#endif
            uint64_t addr = (uint64_t)(uintptr_t)&table_set_int;  // helper that grows array_part
            x86_emit_movabs_rax(cb, addr);                // rax = helper
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);         // call rax
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore frame_reg
            x86_cache_clear(cache);                       // the call clobbers all xmm
            break;
        }
        case OP_TABLE_SET_INT: {
            int xv = x86_cache_load(cache, cb, b);               // load value to store
            if (xv < 0) return;                                  // register cache full
            x86_emit_movsd_store_base(cb, X86_RBX, xv, (a - 1) * 8);  // array_part[a-1] = xv
            break;
        }
        case OP_LOAD_GLOBAL: {
            int idx = a;                                         // global index
            int x = x86_cache_dest_reg(cache, cb, d, -1, -1);    // prefer d's existing home
            if (x < 0) return;                                   // register cache full
            x86_emit_movsd_load_base(cb, X86_RBX, x, idx * 8);   // rbx = globals base, x = globals[idx]
            x86_cache_put(cache, x, d);                          // cache dest
            break;
        }
        case OP_STORE_GLOBAL: {
            int idx = a;                                         // global index
            int xv = x86_cache_load(cache, cb, d);               // load source slot
            if (xv < 0) return;                                  // register cache full
            x86_emit_movsd_store_base(cb, X86_RBX, xv, idx * 8); // globals[idx] = xv
            break;
        }
        default:
            break;                                               // unreachable in native loops
    }

    if (touched_nan_check && info->touches_tables) {         // NaN propagation matches interpreter
        int xd = cache->slot_reg[d];                         // dest slot still in cache?
        if (xd >= 0) emit_nan_check(cb, xd);                 // rewrite NaN to NONE bits
    }
}

// returns true when an opcode is an immediate-comparison jump entry
static bool entry_op_is_imm(Opcode op) {
    return op == OP_JUMP_IF_EQ_IMM || op == OP_JUMP_IF_NEQ_IMM ||
           op == OP_JUMP_IF_LT_IMM || op == OP_JUMP_IF_GT_IMM ||
           op == OP_JUMP_IF_LTE_IMM || op == OP_JUMP_IF_GTE_IMM;
}

// emits one iteration (entry test + body) and returns the fixup offset for the exit jump
static size_t emit_loop_iteration(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                   XmmCache* cache, JitLoopInfo* info, int const_slot,
                                   int save_slot, bool iter_in_xmm, int step_sign,
                                   bool needs_helper) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry = info->entry_pc;                              // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry

    size_t exit_patch;                                       // offset of the exit jump placeholder
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {                // counter-based for loop
        int var_reg = chunk->code[entry].operands[0];        // loop counter slot
        int end_reg = info->for_end_reg;                     // end bound slot
        int step_reg = info->for_step_reg;                   // step slot

        int xa = x86_cache_load(cache, cb, end_reg);         // load end bound
        if (xa < 0) return (size_t)-1;                       // register cache full
        int xb = x86_cache_load_excl(cache, cb, step_reg, xa, -1);  // load step, avoid xa
        if (xb < 0) return (size_t)-1;                       // register cache full
        int xc = x86_cache_alloc_for(cache, cb, var_reg, xa, xb);   // allocate reg, no load
        if (xc < 0) return (size_t)-1;                       // register cache full

        if (!iter_in_xmm) {
            x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(info->nregs));  // xmm15 = iterator
        }
        x86_emit_ucomisd_rr(cb, XMM_SCRATCH, xa);            // ucomisd xmm15, xa
        emit_u8(cb, 0x0F);
        emit_u8(cb, (step_sign > 0) ? 0x87 : 0x82);          // ja exit / jb exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32

        x86_emit_sse66_rr(cb, 0x28, xc, XMM_SCRATCH);        // movapd xc, xmm15 (R[var] = c)
        x86_cache_put(cache, xc, var_reg);                   // var became dirty
        x86_emit_movsd_store(cb, xc, x86_slot_disp(var_reg)); // frame[var] = i
        cache->slot_dirty[var_reg] = false;                  // memory is in sync

        x86_emit_sse_arith_rr(cb, 0x58, XMM_SCRATCH, xb);    // addsd xmm15, xb (step)
        if (!iter_in_xmm) {
            x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->nregs));  // store iterator
        }
    } else if (entry_op_is_imm(chunk->code[entry].opcode)) { // imm-compare loop entry
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];                     // left operand slot
        int imm = entry_inst->operands[2];                   // immediate
        int xa = x86_cache_load(cache, cb, a);               // load left operand
        if (xa < 0) return (size_t)-1;                       // register cache full
        x86_emit_load_double_imm(cb, XMM_SCRATCH, imm);      // xmm15 = (double)imm
        x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);            // ucomisd xa, xmm15
        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);  // exit condition opcode
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                 // conditional exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32
    } else {                                                 // condition-based loop (register-register)
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];                     // left operand slot
        int b = entry_inst->operands[2];                     // right operand slot
        int xa = x86_cache_load(cache, cb, a);               // load left operand
        if (xa < 0) return (size_t)-1;                       // register cache full
        int xb = x86_cache_load_excl(cache, cb, b, xa, -1);  // load right, avoid xa
        if (xb < 0) return (size_t)-1;                       // register cache full

        x86_emit_ucomisd_rr(cb, xa, xb);                     // ucomisd xa, xb

        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);  // exit condition opcode
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                 // conditional exit
        exit_patch = cb->len;                                // record placeholder offset
        emit_i32(cb, 0);                                     // placeholder for rel32
    }

    for (int pc = entry + 1; pc < back_edge; pc++) {         // body
        emit_loop_body_instr(abi, ctx, cb, cache, pc, const_slot, save_slot, info);
    }

    // helpers read their args from the frame, so spill live dirty slots before
    // returning; pure fast paths keep the cache live across iterations
    if (needs_helper) {
        x86_cache_flush(cache, cb);
    }

    return exit_patch;                                       // caller patches the exit jump
}

// checks whether an opcode writes to its operands[0] destination
static bool op_writes_dest(Opcode op) {
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

// rejects loops that would need runtime checks the emitter does not produce
static bool loop_is_safe_to_emit(JITContext* ctx, JitLoopInfo* info) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry     = info->entry_pc;                          // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry

    if (info->table.used && info->globals_count > 0) return false;

    if (info->kind == JIT_LOOP_NUMERIC_FOR) {
        for (int pc = entry + 1; pc < back_edge; pc++) {     // scan body for writes
            Instruction* inst = &chunk->code[pc];
            if (!op_writes_dest(inst->opcode)) continue;     // op does not produce a dest
            int d = inst->operands[0];                       // destination slot
            if (d == info->for_end_reg || d == info->for_step_reg) return false;
        }
    }

    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        if (inst->opcode == OP_TABLE_SET_INT) {       // rbx holds only info->table.slot's array_part
            if (inst->operands[0] != info->table.slot) return false;
        } else if (inst->opcode == OP_TABLE_GET_INT) {       // same rbx issue as SET_INT
            if (inst->operands[1] != info->table.slot) return false;
        } else if (inst->opcode == OP_CALL_0 ||              // call targets must be JIT-compiled
                   inst->opcode == OP_CALL_1 ||
                   inst->opcode == OP_CALL_2) {
            int target = inst->operands[1];                  // callee function index
            if (target < 0 || target >= ctx->func_count) return false;
            if (!ctx->func_table[target]) return false;      // callee has no native code
        }
        // OP_TABLE_SET now always uses the general helper path, no rbx assumption
    }
    return true;                                             // only safe operations seen
}

// emits native code for a numeric loop (with optional table access)
static bool x86_64_emit_numeric_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                      JitLoopInfo* info, int step_sign, void** out_fn) {
    int entry = info->entry_pc;                              // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry
    int nregs = info->nregs;                                 // function frame size
    int extra_slots = (info->kind == JIT_LOOP_NUMERIC_FOR) ? 1 : 0;  // iterator temp slot
    int const_slot = nregs + extra_slots;                    // reserved slot for constant 1.0

    bool needs_helper = false;                               // does the body call a runtime helper?
    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &ctx->chunk->code[pc];
        switch (inst->opcode) {
            case OP_TABLE_GET_KEY_STR:                       // fused string-key op: helper
            case OP_TABLE_SET_KEY_STR:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:  // user calls clobber frame_reg
                needs_helper = true;
                break;
            case OP_TABLE_GET: case OP_TABLE_SET:
            case OP_TABLE_GET_NUM: case OP_TABLE_SET_NUM: {
                int key_reg  = (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM)
                             ? inst->operands[2] : inst->operands[1];
                int tbl_reg  = (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM)
                             ? inst->operands[1] : inst->operands[0];
                bool key_is_str = (key_reg >= 0 && key_reg < 64) &&
                                  ((info->str_slots >> key_reg) & 1ULL);
                bool is_fast = (key_reg == info->for_var_reg) &&
                               (tbl_reg == info->table.slot) &&
                               !key_is_str;
                if (!is_fast) needs_helper = true;
                break;
            }
            default: break;
        }
        if (needs_helper) break;
    }

    // precompute distinct immediates used by ADD_IMM/SUB_IMM/MUL_IMM/DIV_IMM/MOD_IMM
    info->n_imms = 0;
    info->imm_opt_ok = true;
    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &ctx->chunk->code[pc];
        int32_t imm;
        switch (inst->opcode) {
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
                imm = inst->operands[2];
                break;
            default: continue;
        }
        bool found = false;
        for (int i = 0; i < info->n_imms; i++)
            if (info->imms[i].value == imm) { found = true; break; }
        if (found) continue;
        if (info->n_imms >= JIT_MAX_IMMS) { info->imm_opt_ok = false; info->n_imms = 0; break; }
        info->imms[info->n_imms++].value = imm;
    }

    int imm_base_slot = const_slot + 1;                      // first slot for precomputed immediates
    for (int i = 0; i < info->n_imms; i++) info->imms[i].slot = imm_base_slot + i;

    int save_slot = -1;                                      // stack slot to stash frame_reg across helper call
    int frame_slots = const_slot + 1 + info->n_imms;         // incl. constant slot + immediates
    uint64_t writeback_mask = info->live_out & ~info->ref_writes;  // slots whose old value must be released
    if (needs_helper || writeback_mask != 0) {
        save_slot = frame_slots;                             // reserve one more slot
        frame_slots++;
    }

    int gpr_saves_bytes = (info->table.used || info->globals_count > 0) ? 8 : 0;  // rbx save if table or globals access

    int range_size = back_edge - entry + 1;
    if (range_size <= 0) return false;                       // empty range, nothing to emit

    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;  // buffer too small

    if (!loop_is_safe_to_emit(ctx, info)) return false;      // needs runtime checks the emitter lacks

    // scan body to decide if the iterator can stay in xmm15 across iterations
    bool iter_in_xmm = false;
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {                // only numeric-for has iterator
        bool clobbers = false;
        for (int pc = entry + 1; pc < back_edge; pc++) {
            Opcode op = ctx->chunk->code[pc].opcode;
            // imm ops stay off XMM_SCRATCH only when every immediate was precomputed;
            // otherwise the fallback loads the immediate into XMM_SCRATCH and would
            // clobber the iterator
            bool op_is_imm = (op == OP_ADD_IMM || op == OP_SUB_IMM ||
                              op == OP_MUL_IMM || op == OP_DIV_IMM ||
                              op == OP_MOD_IMM);
            if (op_is_imm) {
                if (info->imm_opt_ok) continue;              // memory-operand form, scratch-free
                clobbers = true;                             // imm load clobbers XMM_SCRATCH
                break;
            }
            if (op == OP_MOD || op == OP_NEG ||              // use XMM_SCRATCH
                op == OP_TABLE_GET_KEY_STR ||                // helper call clobbers xmm
                op == OP_TABLE_SET_KEY_STR ||                // helper call clobbers xmm
                op == OP_TABLE_SET ||                        // general path calls table_set_int
                op == OP_TABLE_SET_NUM ||                    // general path calls table_set_int
                op == OP_NEW_TABLE ||                        // helper call clobbers xmm
                op == OP_CALL_0 || op == OP_CALL_1 ||        // calls clobber all xmm regs
                op == OP_CALL_2) {
                clobbers = true;
                break;                                       // iterator must live in memory
            }
        }
        iter_in_xmm = !clobbers;
    }

    // fixpoint pass: emit into scratch until the cache state stabilizes
    uint8_t* scratch_buf = ctx->scratch_code_buf;            // shared scratch buffer
    size_t scratch_size  = ctx->scratch_code_buf_cap;
    if (!scratch_buf || scratch_size < (size_t)range_size * 256 + 1024) return false;  // scratch too small
    CodeBuf scratch = { scratch_buf, 0, scratch_size };

    XmmCache cache_start;
    x86_cache_clear(&cache_start);                           // start from empty cache
    bool converged = false;
    bool flush_at_back_edge = false;                         // fallback policy when the fixpoint oscillates
    for (int iter = 0; iter < 8; iter++) {                   // fixpoint loop
        XmmCache cache = cache_start;
        scratch.len = 0;
        size_t patch = emit_loop_iteration(abi, ctx, &scratch, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign, needs_helper);
        if (patch == (size_t)-1) return false;               // emit failed
        if (flush_at_back_edge) x86_cache_clear(&cache);     // simulate the spill before the back edge
        if (x86_cache_eq(&cache, &cache_start)) { converged = true; break; }  // stable state reached
        cache_start = cache;                                 // try again with new state
    }
    if (!converged) {
        x86_cache_clear(&cache_start);                       // empty incoming state
        flush_at_back_edge = true;                           // force a spill before the back edge
        converged = true;                                    // empty start + flush at back edge is stable
    }

    // real emit
    size_t mark = cb->len;                                   // rollback point

    int base_frame = align16(8 * frame_slots);
    int frame_size = align16(base_frame + abi->frame_extra + gpr_saves_bytes);

    emit_prologue(cb, abi, frame_size, base_frame);

    // save rbx if we touch tables, and preload array_part into rbx
    int rbx_slot_off = base_frame + abi->frame_extra + 8;
    if (info->table.used && info->table.slot >= 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);  // save caller's rbx
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, info->table.slot * 8);  // rax = regs[slot]
        x86_emit_clear_high16_rax(cb);                       // strip nan-box tag
        x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));  // rbx = array_part
    } else if (info->globals_count > 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);  // save caller's rbx
        emit_u8(cb, 0x48); emit_u8(cb, 0x89);                // mov rbx, abi->globals_reg
        emit_u8(cb, 0xC0 | (abi->globals_reg << 3) | X86_RBX);
    }

    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);          // rax = bits of 1.0
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                  // xmm7 = 1.0
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));  // const_slot = 1.0

    // store each precomputed immediate into its slot
    for (int i = 0; i < info->n_imms; i++) {
        x86_emit_load_double_imm(cb, XMM_SCRATCH, info->imms[i].value);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->imms[i].slot));
    }

    // copy live-in AND live-out slots from vm regs to stack frame
    uint64_t m = info->live_in | info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);                          // next slot to seed
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);  // xmm0 = regs[s]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));       // frame[s] = xmm0
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && !iter_in_xmm) {  // seed iterator in memory
        int var_reg = ctx->chunk->code[entry].operands[0];
        x86_emit_movsd_load(cb, 0, x86_slot_disp(var_reg));  // xmm0 = R[var]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(nregs));   // frame[nregs] = xmm0
    }

    // preload the fixpoint cache state
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = cache_start.reg_slot[i];
        if (s >= 0) x86_emit_movsd_load(cb, i, x86_slot_disp(s));  // xmm<i> = frame[s]
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && iter_in_xmm) {  // load iterator to xmm15
        int var_reg = ctx->chunk->code[entry].operands[0];
        x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(var_reg));  // xmm15 = R[var]
    }

    int loop_top = (int)cb->len;                             // loop start address
    XmmCache cache = cache_start;
    size_t entry_patch = emit_loop_iteration(abi, ctx, cb, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign, needs_helper);
    if (entry_patch == (size_t)-1) { cb->len = mark; return false; }  // emit failed, rollback

    if (flush_at_back_edge) x86_cache_flush(&cache, cb);     // spill all before back edge
    emit_u8(cb, 0xE9);                                       // jmp loop_top
    size_t back_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder
    int32_t back_rel = loop_top - (int32_t)(back_patch + 4);
    memcpy(cb->buf + back_patch, &back_rel, 4);              // patch back edge

    int exit_label = (int)cb->len;                           // exit target
    int32_t exit_rel = exit_label - (int32_t)(entry_patch + 4);
    memcpy(cb->buf + entry_patch, &exit_rel, 4);             // patch exit jump

    x86_cache_flush(&cache, cb);                             // spill dirty slots to frame

    m = info->live_out;                                      // copy written slots back to vm
    while (m) {
        int s = __builtin_ctzll(m);                          // next written slot
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        if (info->ref_writes & (1ULL << s)) continue;        // refcounted value — keep vm's copy

        if (save_slot >= 0) {
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

            // arg0: &pool[s] = frame_reg + s*8
            if (s != 0) {
                emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
                emit_u8(cb, 0xC1);                                 // add rcx, imm32
#else
                emit_u8(cb, 0xC7);                                 // add rdi, imm32
#endif
                emit_i32(cb, s * 8);
            }

            // arg1: frame[s]
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore frame_reg
        } else {
            // no save slot reserved: fall back to plain store (no refcount)
            x86_emit_movsd_load(cb, 0, x86_slot_disp(s));
            x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);
        }
    }

    if ((info->table.used && info->table.slot >= 0) || info->globals_count > 0) {
        x86_emit_load_r64_rbp(cb, X86_RBX, -rbx_slot_off);   // restore caller's rbx
    }

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;                                             // emission successful
}

// emits native code for a table iteration loop (for v in t)
static bool x86_64_emit_table_iter_loop(const X86_64Abi* abi, JITContext* ctx,
                                         CodeBuf* cb, JitLoopInfo* info, void** out_fn) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry = info->entry_pc;                              // first body pc
    int back_edge = info->back_edge_pc;                      // jump back to entry
    int nregs = info->nregs;                                 // function frame size
    int table_slot = info->table.slot;                       // register holding the table
    int var_reg = chunk->code[entry].operands[0];            // loop variable slot

    if (table_slot < 0) return false;                        // no table detected, bail

    int range_size = back_edge - entry + 1;
    if (range_size <= 0) return false;                       // empty range, nothing to emit
    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;  // buffer too small

    size_t mark = cb->len;                                   // rollback point
    int const_slot = nregs;                                  // unused here, kept for layout parity
    int save_slot  = nregs + 1;                              // stack slot to stash frame_reg across helper call
    int frame_slots = save_slot + 1;                         // incl. the unused constant slot and save slot
    int base_frame = align16(8 * frame_slots);

    int saves_bytes = 24;                                    // rbx + r12 + r13
    int frame_size = align16(base_frame + abi->frame_extra + saves_bytes);

    emit_prologue(cb, abi, frame_size, base_frame);

    int off_rbx = base_frame + abi->frame_extra + 8;         // save slot for rbx
    int off_r12 = base_frame + abi->frame_extra + 16;        // save slot for r12
    int off_r13 = base_frame + abi->frame_extra + 24;        // save slot for r13
    x86_emit_store_r64_rbp(cb, X86_RBX, -off_rbx);           // save caller's rbx
    x86_emit_store_r64_rbp(cb, X86_R12, -off_r12);           // save caller's r12
    x86_emit_store_r64_rbp(cb, X86_R13, -off_r13);           // save caller's r13

    // unpack the table pointer and preload its fields into dedicated gprs
    x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, table_slot * 8);  // rax = regs[table_slot]
    x86_emit_clear_high16_rax(cb);                           // strip nan-box tag
    x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));   // rbx = array_part
    x86_emit_load_r64_base(cb, X86_R12, X86_RAX, (int32_t)offsetof(Table, array_count));  // r12 = array_count

    emit_u8(cb, 0x31); emit_u8(cb, 0xD2);                    // xor edx, edx
    // rdx = 0 (iteration counter, caller-saved: no save/restore needed)
    x86_emit_movabs_r8(cb, X86_NONE_BITS);                   // r8 = NONE bits, for hole-skip

    // copy live-in AND live-out slots from vm regs to frame, except table and loop var
    uint64_t m = info->live_in | info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);                          // next slot to seed
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        if (s == table_slot) continue;                       // table lives in rbx, not frame
        if (s == var_reg)   continue;                        // var written each iteration
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);  // xmm0 = regs[s]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));       // frame[s] = xmm0
    }

    int loop_top = (int)cb->len;                             // loop start address

    // if counter >= array_count, exit
    x86_emit_cmp_rdx_r12d(cb);                               // cmp rdx, r12
    emit_u8(cb, 0x0F); emit_u8(cb, 0x83);                    // jae rel32
    size_t exit_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder

    x86_emit_load_rax_idx8(cb, X86_RBX, X86_RDX);            // rax = array_part[rdx]
    // if element is NONE, skip to next iteration
    x86_emit_cmp_rax_r8(cb);                                 // cmp rax, NONE bits
    emit_u8(cb, 0x0F); emit_u8(cb, 0x84);                    // je rel32 (skip)
    size_t skip_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder

    x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(var_reg));  // frame[var_reg] = element

    XmmCache cache;
    x86_cache_clear(&cache);                                 // body starts with empty cache

    int xv = x86_cache_alloc_excl(&cache, cb, -1, -1);       // xmm to hold the element
    if (xv < 0) { cb->len = mark; return false; }            // no free xmm, rollback
    x86_emit_movq_xmm_rax(cb, xv);                           // xv = element bits (as double)
    x86_cache_put(&cache, xv, var_reg);                      // cache var_reg in xv

    for (int pc = entry + 1; pc < back_edge; pc++) {         // emit body
        emit_loop_body_instr(abi, ctx, cb, &cache, pc, const_slot, -1, info);
    }

    x86_cache_flush(&cache, cb);                             // spill dirty slots to frame

    int skip_label = (int)cb->len;                           // target for hole-skip
    int32_t skip_rel = skip_label - (int32_t)(skip_patch + 4);
    memcpy(cb->buf + skip_patch, &skip_rel, 4);              // patch skip jump

    emit_u8(cb, 0x48); emit_u8(cb, 0xFF); emit_u8(cb, 0xC2); // inc rdx
    emit_u8(cb, 0xE9);                                       // jmp rel32
    size_t back_patch = cb->len;
    emit_i32(cb, 0);                                         // placeholder
    int32_t back_rel = loop_top - (int32_t)(back_patch + 4);
    memcpy(cb->buf + back_patch, &back_rel, 4);              // patch back edge

    int exit_label = (int)cb->len;                           // exit target
    int32_t exit_rel = exit_label - (int32_t)(exit_patch + 4);
    memcpy(cb->buf + exit_patch, &exit_rel, 4);              // patch exit jump

    m = info->live_out;                                      // copy written slots back to vm
    while (m) {
        int s = __builtin_ctzll(m);                          // next written slot
        m &= m - 1;
        if (s >= 64) break;                                  // beyond tracked range
        if (s == var_reg) continue;                          // already written by the loop body
        if (info->ref_writes & (1ULL << s)) continue;        // refcounted value — keep vm's copy

        x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

        // arg0: &pool[s] = frame_reg + s*8
        if (s != 0) {
            emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0xC1);                                    // add rcx, imm32
#else
            emit_u8(cb, 0xC7);                                    // add rdi, imm32
#endif
            emit_i32(cb, s * 8);
        }

        // arg1: frame[s]
        x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));
#if defined(_WIN32) || defined(_WIN64)
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif

        x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
        emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

        x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore frame_reg
    }

    x86_emit_load_r64_rbp(cb, X86_RBX, -off_rbx);             // restore caller's rbx
    x86_emit_load_r64_rbp(cb, X86_R12, -off_r12);             // restore caller's r12
    x86_emit_load_r64_rbp(cb, X86_R13, -off_r13);             // restore caller's r13

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;                                             // emission successful
}

// emits native code for a condition-entry loop: no implicit entry test, the
// body starts at entry_pc, and the exit is a JUMP_IF_FALSE whose target lands
// past the back edge.  Internal JUMP_IF_FALSE targets get local forward labels.
static bool x86_64_emit_cond_enter_loop(const X86_64Abi* abi, JITContext* ctx,
                                         CodeBuf* cb, JitLoopInfo* info, void** out_fn) {
    BytecodeChunk* chunk = ctx->chunk;
    int entry     = info->entry_pc;                          // first body pc
    int back_edge = info->back_edge_pc;                      // JUMP back to entry
    int nregs     = info->nregs;                             // function frame size
    int const_slot = nregs;                                  // reserved slot for constant 1.0

    if (back_edge <= entry) return false;                    // empty body
    int range_size = back_edge - entry + 1;
    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap) return false;  // buffer too small

    if (!loop_is_safe_to_emit(ctx, info)) return false;      // needs checks the emitter lacks

    // precompute distinct immediates used by ADD_IMM/SUB_IMM/MUL_IMM/DIV_IMM/MOD_IMM
    info->n_imms = 0;
    info->imm_opt_ok = true;
    for (int pc = entry; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        int32_t imm;
        switch (inst->opcode) {
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
                imm = inst->operands[2];
                break;
            default: continue;
        }
        bool found = false;
        for (int i = 0; i < info->n_imms; i++)
            if (info->imms[i].value == imm) { found = true; break; }
        if (found) continue;
        if (info->n_imms >= JIT_MAX_IMMS) { info->imm_opt_ok = false; info->n_imms = 0; break; }
        info->imms[info->n_imms++].value = imm;
    }
    int imm_base_slot = const_slot + 1;
    for (int i = 0; i < info->n_imms; i++) info->imms[i].slot = imm_base_slot + i;

    // scan for helper calls (same rule as the numeric emitter)
    bool needs_helper = false;
    for (int pc = entry; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_TABLE_GET_KEY_STR:
            case OP_TABLE_SET_KEY_STR:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
                needs_helper = true;
                break;
            case OP_TABLE_GET: case OP_TABLE_SET:
            case OP_TABLE_GET_NUM: case OP_TABLE_SET_NUM: {
                int key_reg, tbl_reg;
                if (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM) {
                    tbl_reg = inst->operands[1]; key_reg = inst->operands[2];
                } else {
                    tbl_reg = inst->operands[0]; key_reg = inst->operands[1];
                }
                bool is_fast = (key_reg == info->for_var_reg) &&
                               (tbl_reg == info->table.slot);
                if (!is_fast) needs_helper = true;
                break;
            }
            default: break;
        }
        if (needs_helper) break;
    }

    int save_slot = -1;                                      // stack slot to stash frame_reg across helper call
    int frame_slots = const_slot + 1 + info->n_imms;
    uint64_t writeback_mask = info->live_out & ~info->ref_writes;
    if (needs_helper || writeback_mask != 0) {
        save_slot = frame_slots;                             // reserve one more slot
        frame_slots++;
    }

    int gpr_saves_bytes = (info->table.used || info->globals_count > 0) ? 8 : 0;  // rbx save if table or globals access

    size_t mark = cb->len;                                   // rollback point

    int base_frame = align16(8 * frame_slots);
    int frame_size = align16(base_frame + abi->frame_extra + gpr_saves_bytes);

    emit_prologue(cb, abi, frame_size, base_frame);

    // save rbx if we touch tables or globals, and preload the relevant base
    int rbx_slot_off = base_frame + abi->frame_extra + 8;
    if (info->table.used && info->table.slot >= 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, info->table.slot * 8);
        x86_emit_clear_high16_rax(cb);
        x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));
    } else if (info->globals_count > 0) {
        x86_emit_store_r64_rbp(cb, X86_RBX, -rbx_slot_off);
        emit_u8(cb, 0x48); emit_u8(cb, 0x89);
        emit_u8(cb, 0xC0 | (abi->globals_reg << 3) | X86_RBX);
    }

    // materialize the 1.0 constant and any precomputed immediates
    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));
    for (int i = 0; i < info->n_imms; i++) {
        x86_emit_load_double_imm(cb, XMM_SCRATCH, info->imms[i].value);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->imms[i].slot));
    }

    // seed live-in AND live-out slots from the vm's register frame
    uint64_t m = info->live_in | info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));
    }

    XmmCache cache;
    x86_cache_clear(&cache);

    // local label bookkeeping borrowed from the shared scratch pool
    int32_t* label_off = ctx->scratch_label_off;
    JumpFixup* fixups  = ctx->scratch_fixups;
    if (!label_off || !fixups) { cb->len = mark; return false; }
    for (int i = 0; i <= range_size; i++) label_off[i] = -1;
    int nfix = 0;

    int loop_top = (int)cb->len;                             // loop start address

    for (int pc = entry; pc < back_edge; pc++) {
        label_off[pc - entry] = (int32_t)cb->len;
        Instruction* inst = &chunk->code[pc];
        if (inst->opcode == OP_JUMP_IF_FALSE) {
            int cond_reg = inst->operands[1];                // condition register
            int tgt      = inst->operands[0];                // forward target
            int xa = x86_cache_load(&cache, cb, cond_reg);
            if (xa < 0) { cb->len = mark; return false; }
            x86_emit_movq_rax_xmm(cb, xa);                   // rax = raw 64-bit slot
            emit_u8(cb, 0xA8); emit_u8(cb, 0x01);            // test al, 1
            // local skip over a single CMP writing the same register: the
            // CMP reuses cond_reg's xmm (see emit_loop_body_instr), so both
            // paths reach pc+2 with d in the same xmm and no spill is needed
            bool local_skip = (tgt == pc + 2) &&
                              (chunk->code[pc + 1].operands[0] == cond_reg) &&
                              (chunk->code[pc + 1].opcode == OP_CMP_EQ ||
                               chunk->code[pc + 1].opcode == OP_CMP_EQ_NUM ||
                               chunk->code[pc + 1].opcode == OP_CMP_NEQ ||
                               chunk->code[pc + 1].opcode == OP_CMP_NEQ_NUM ||
                               chunk->code[pc + 1].opcode == OP_CMP_LT ||
                               chunk->code[pc + 1].opcode == OP_CMP_GT ||
                               chunk->code[pc + 1].opcode == OP_CMP_LTE ||
                               chunk->code[pc + 1].opcode == OP_CMP_GTE);
            if (!local_skip) x86_cache_flush(&cache, cb);    // flush before branch
            emit_u8(cb, 0x0F); emit_u8(cb, 0x84);            // je rel32 (bit0 == 0 -> false)
            if (nfix >= range_size) { cb->len = mark; return false; }
            fixups[nfix].patch_at  = cb->len;
            fixups[nfix].target_pc = tgt;                    // exit vs internal decided at patch time
            nfix++;
            emit_i32(cb, 0);
        } else {
            emit_loop_body_instr(abi, ctx, cb, &cache, pc, const_slot, save_slot, info);
        }
    }
    label_off[back_edge - entry] = (int32_t)cb->len;         // label for the back-edge pc

    x86_cache_flush(&cache, cb);                             // spill any remaining dirty slots

    // back edge jump
    emit_u8(cb, 0xE9);
    int32_t back_rel = loop_top - (int32_t)(cb->len + 4);
    emit_i32(cb, back_rel);

    int exit_label = (int)cb->len;                           // exit target

    // patch every JUMP_IF_FALSE: exit -> exit_label, internal -> local label
    for (int i = 0; i < nfix; i++) {
        int tgt = fixups[i].target_pc;
        int32_t rel;
        if (tgt == info->exit_pc) {
            rel = exit_label - (int32_t)(fixups[i].patch_at + 4);
        } else {
            int idx = tgt - entry;
            if (idx < 0 || idx > range_size || label_off[idx] < 0) {
                cb->len = mark; return false;
            }
            rel = label_off[idx] - (int32_t)(fixups[i].patch_at + 4);
        }
        memcpy(cb->buf + fixups[i].patch_at, &rel, 4);
    }

    // write back live_out slots to the vm's registers
    m = info->live_out;
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        if (info->ref_writes & (1ULL << s)) continue;        // refcounted value — keep vm's copy

        if (save_slot >= 0) {
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));

            // arg0: &pool[s] = frame_reg + s*8
            if (s != 0) {
                emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
                emit_u8(cb, 0xC1);                            // add rcx, imm32
#else
                emit_u8(cb, 0xC7);                            // add rdi, imm32
#endif
                emit_i32(cb, s * 8);
            }

            // arg1: frame[s]
            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);  // mov rdx, rax
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);  // mov rsi, rax
#endif
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));
        } else {
            x86_emit_movsd_load(cb, 0, x86_slot_disp(s));
            x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);
        }
    }

    if ((info->table.used && info->table.slot >= 0) || info->globals_count > 0) {
        x86_emit_load_r64_rbp(cb, X86_RBX, -rbx_slot_off);   // restore caller's rbx
    }

    emit_leave_ret(cb, abi, base_frame);

    *out_fn = (void*)(cb->buf + mark);                       // publish entry pointer
    return true;
}

// emits native code for a single native loop, dispatching by kind
bool x86_64_emit_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                      JitLoopInfo* info, int step_sign, void** out_fn) {
    if (info->kind == JIT_LOOP_TABLE_ITER) {                 // table iteration loop
        return x86_64_emit_table_iter_loop(abi, ctx, cb, info, out_fn);
    }
    if (info->kind == JIT_LOOP_COND_ENTER) {                 // condition-entry loop
        return x86_64_emit_cond_enter_loop(abi, ctx, cb, info, out_fn);
    }
    return x86_64_emit_numeric_loop(abi, ctx, cb, info, step_sign, out_fn);  // numeric or condition loop
}
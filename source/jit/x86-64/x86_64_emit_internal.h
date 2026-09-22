// source/jit/x86-64/x86_64_emit_internal.h
// Internal declarations shared by the split x86-64 emit translation units
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_JIT_X86_64_EMIT_INTERNAL_H
#define APEX_JIT_X86_64_EMIT_INTERNAL_H

#include "x86_64_common.h"
#include "x86_64_abi.h"
#include "jit_internal.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

#define JIT_FATAL(...) do {                                              \
    fprintf(stderr, "\033[31mJIT fatal error (%s:%d): ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                                        \
    fprintf(stderr, "\n\033[0m");                                        \
    exit(1);                                                             \
} while (0)

// rounds n up to the next multiple of 16
int align16(int n);

// checks if an opcode ends a basic block (no fall-through to the next pc)
bool op_is_terminator(Opcode op);

// checks whether any backward jump in [start,end) targets `target`
bool has_backward_jump(BytecodeChunk* chunk, int start, int end, int target);

// compact xmm cache snapshot stored at a unique forward jump site
typedef struct {
    int16_t  reg_slot[XMM_CACHE_REGS];  // xmm[i] holds slot reg_slot[i], or -1
    uint16_t dirty_mask;                // bit i: xmm[i] holds a dirty slot
} JitCacheSnap;

// captures the current cache into a compact snapshot
void x86_cache_snap(JitCacheSnap* snap, const XmmCache* c);

// rebuilds the xmm cache from a compact snapshot, clearing prior state
void x86_cache_restore(XmmCache* c, const JitCacheSnap* snap);

// helper for OP_JUMP_MATCH_STR: returns 1 if the subject is a string whose
int jit_match_str(Value subj, StringObject* case_str);

// JIT helper: replace *dst with new_val
void jit_store_slot(Value* dst, Value new_val);

// JIT helper: tbl["prefix" .. num] with a synthetic key
Value jit_table_get_key_str(Table* t, StringObject* prefix, double num);

// JIT helper: tbl["prefix" .. num] = val, hash computed once
void jit_table_set_key_str(Table* t, StringObject* prefix, double num, Value val);

// emits the shared prologue then any abi-specific callee-saved register saves
void emit_prologue(CodeBuf* cb, const X86_64Abi* abi, int frame_size, int base_frame);

// emits abi-specific restores, then leave; ret
void emit_leave_ret(CodeBuf* cb, const X86_64Abi* abi, int base_frame);

// fallback epilogue: returns xmm0 = 0.0
void emit_return_zero(const X86_64Abi* abi, CodeBuf* cb, int base_frame);

// emits indirect call through func_table[func_idx] (args in xmm0/xmm1, result in xmm0)
void emit_call_func(CodeBuf* cb, JITContext* ctx, int func_idx);

// emits a call to `callee`; if it is the function currently being emitted
void emit_call_or_self(CodeBuf* cb, JITContext* ctx, int callee, int self_idx, size_t self_mark);

// returns the jcc opcode that exits the loop when the entry condition is met
uint8_t jcc_for_entry_op(Opcode op);

// checks if slot s is read between pc_after and the first write to s (inclusive scan, stops at write)
bool slot_read_before_write(BytecodeChunk* chunk, int pc_after, int end, int s);

// checks whether slot s is live at pc inside a loop body
bool slot_live_in_loop(BytecodeChunk* chunk, int entry, int back_edge, int pc, int s);

// rewrites xmm_d to the NONE bit pattern on NaN; matches interpreter semantics
void emit_nan_check(CodeBuf* cb, int xmm_d);

// returns true if an opcode can execute in int64 GPR mode without losing semantics
bool op_is_int_safe(Opcode op);

// checks whether an opcode writes to its operands[0] destination
bool op_writes_dest(Opcode op);

// returns true when an opcode is an immediate-comparison jump entry
bool entry_op_is_imm(Opcode op);

// int loop emitters
bool matches_int_self_recursive(JITContext* ctx, int func_idx);
bool x86_64_emit_int_self_recursive(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb, int func_idx, size_t* out_int_off);
void emit_int_wrapper(CodeBuf* cb, size_t general_off, size_t int_off, int max_n, int arity, size_t* out_wrapper_off, const X86_64Abi* abi);
int assign_int_loop_gprs(JITContext* ctx, JitLoopInfo* info, int slot_gpr[JIT_MAX_SLOTS], int frame_reg);
bool matches_int_accum_loop(const X86_64Abi* abi, JITContext* ctx, JitLoopInfo* info);
void emit_int_loop_body(CodeBuf* cb, BytecodeChunk* chunk, int pc, const int* slot_gpr);
bool x86_64_emit_int_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb, JitLoopInfo* info, int step_sign, void** out_fn);

// loop emitters
void emit_loop_body_instr(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb, XmmCache* cache, int pc, int const_slot, int save_slot, JitLoopInfo* info);
size_t emit_loop_iteration(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb, XmmCache* cache, JitLoopInfo* info, int const_slot, int save_slot, bool iter_in_xmm, int step_sign, bool needs_helper);
bool loop_is_safe_to_emit(JITContext* ctx, JitLoopInfo* info);
bool x86_64_emit_numeric_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb, JitLoopInfo* info, int step_sign, void** out_fn);
bool x86_64_emit_table_iter_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb, JitLoopInfo* info, void** out_fn);
bool x86_64_emit_cond_enter_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb, JitLoopInfo* info, void** out_fn);

#endif // APEX_JIT_X86_64_EMIT_INTERNAL_H
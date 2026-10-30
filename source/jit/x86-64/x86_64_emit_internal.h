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

// jit_fatal reports an internal inconsistency and unwinds to the current bail frame
#define JIT_FATAL(...) do {                                              \
    fprintf(stderr, "\033[31mJIT error (%s:%d): ", __FILE__, __LINE__);  \
    fprintf(stderr, __VA_ARGS__);                                        \
    fprintf(stderr, " — falling back to interpreter\n\033[0m");          \
    if (jit_bail_jmp) longjmp(*jit_bail_jmp, 1);                         \
    abort();                                                             \
} while (0)

#define X86_N_GPR_POCKETS 5                                          // callee-saved gprs usable as slot pockets

extern const int x86_gpr_pocket_regs[X86_N_GPR_POCKETS];             // pocket gpr indices (rbx, r12-r15)

int  align16(int n);                                                 // rounds n up to the next multiple of 16
bool op_is_terminator(Opcode op);                                    // true if op ends a basic block
bool has_backward_jump(BytecodeChunk* chunk, int start, int end, int target);  // any backward jump to target?

// compact xmm cache snapshot stored at a unique forward jump site
typedef struct {
    int16_t  reg_slot[XMM_CACHE_REGS];                               // xmm[i] holds slot reg_slot[i], or -1
    uint16_t dirty_mask;                                             // bit i: xmm[i] holds a dirty slot
} JitCacheSnap;

void x86_cache_snap(JitCacheSnap* snap, const XmmCache* c);          // capture live cache into a snapshot
void x86_cache_restore(XmmCache* c, const JitCacheSnap* snap);       // rebuild cache from a snapshot

// runtime helpers called from emitted code (defined in x86_64_emit_common.c)
int   jit_match_str(Value subj, StringObject* case_str);             // 1 on string match
int   jit_switch_str(Value subj, BytecodeChunk* chunk, int table_idx);  // str-switch target pc or -1
int   jit_switch_num(Value subj, BytecodeChunk* chunk, int table_idx);  // num-switch target pc or -1
void  jit_store_slot(Value* dst, Value new_val);                     // refcount-aware store
Value jit_table_get_const(Table* t, StringObject* key);              // tbl[const_str]
void  jit_table_set_const(Table* t, StringObject* key, Value val);   // tbl[const_str] = val
Value jit_table_get_key_str(Table* t, StringObject* prefix, double num);     // tbl["prefix"..num]
void  jit_table_set_key_str(Table* t, StringObject* prefix, double num, Value val);  // tbl["prefix"..num] = val
void  jit_push_arg_base(VM* vm, Value src);                                  // push src onto vm args stack
void  jit_call_builtin_store(VM* vm, int name_idx, int arg_count, Value* dst);
void  jit_async_call_builtin_store(VM* vm, int name_idx, int arg_count, Value* dst);

// base-path runtime helpers (checked arithmetic that matches the interpreter's
// non-numeric fallback to NONE) - used for functions that aren't strictly numeric-pure
Value jit_add_base(Value a, Value b);                                // a + b or NONE
Value jit_sub_base(Value a, Value b);                                // a - b or NONE
Value jit_mul_base(Value a, Value b);                                // a * b or NONE
Value jit_div_base(Value a, Value b);                                // a / b or NONE
Value jit_mod_base(Value a, Value b);                                // a % b or NONE
Value jit_add_imm_base(Value a, int64_t imm);                        // a + imm or NONE
Value jit_sub_imm_base(Value a, int64_t imm);                        // a - imm or NONE
Value jit_mul_imm_base(Value a, int64_t imm);                        // a * imm or NONE
Value jit_div_imm_base(Value a, int64_t imm);                        // a / imm or NONE
Value jit_mod_imm_base(Value a, int64_t imm);                        // a % imm or NONE
Value jit_neg_base(Value a);                                         // -a or NONE
Value jit_inc_base(Value a);                                         // a + 1 or NONE
Value jit_dec_base(Value a);                                         // a - 1 or NONE
Value jit_cmp_eq_base(Value a, Value b);                             // MAKE_BOOL(a == b) with type dispatch
Value jit_cmp_ne_base(Value a, Value b);                             // MAKE_BOOL(a != b) with type dispatch
Value jit_cmp_lt_base(Value a, Value b);                             // MAKE_BOOL(raw-double a <  b)
Value jit_cmp_gt_base(Value a, Value b);                             // MAKE_BOOL(raw-double a >  b)
Value jit_cmp_lte_base(Value a, Value b);                            // MAKE_BOOL(raw-double a <= b)
Value jit_cmp_gte_base(Value a, Value b);                            // MAKE_BOOL(raw-double a >= b)

// prologue / epilogue / helper-call emitters (defined in x86_64_emit_common.c)
void emit_prologue(CodeBuf* cb, const X86_64Abi* abi, int frame_size, int base_frame);
void emit_leave_ret(CodeBuf* cb, const X86_64Abi* abi, int base_frame);
void emit_return_zero(const X86_64Abi* abi, CodeBuf* cb, int base_frame);
void emit_call_func(CodeBuf* cb, JITContext* ctx, int func_idx);
void emit_call_or_self(CodeBuf* cb, JITContext* ctx, int callee, int self_idx, size_t self_mark);
uint8_t jcc_for_entry_op(Opcode op);                                 // jcc opcode that exits the loop
bool slot_read_before_write(BytecodeChunk* chunk, int pc_after, int end, int s);
bool slot_live_in_loop(BytecodeChunk* chunk, int entry, int back_edge, int pc, int s);
void emit_nan_check(CodeBuf* cb, int xmm_d);                         // NaN -> NONE bits rewrite
bool op_is_int_safe(Opcode op);                                      // can run in int64 GPR mode
bool op_writes_dest(Opcode op);                                      // does the op write operands[0]?
bool entry_op_is_imm(Opcode op);                                     // is this an imm-compare jump?

// int-specialized function and loop emitters (defined in x86_64_emit_int.c)
bool matches_int_self_recursive(JITContext* ctx, int func_idx);
bool x86_64_emit_int_self_recursive(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                    int func_idx, size_t* out_int_off);
void emit_int_wrapper(CodeBuf* cb, size_t general_off, size_t int_off, int max_n, int arity,
                      size_t* out_wrapper_off, const X86_64Abi* abi);
int  assign_int_loop_gprs(JITContext* ctx, JitLoopInfo* info, int slot_gpr[JIT_MAX_SLOTS], int frame_reg);
bool matches_int_accum_loop(const X86_64Abi* abi, JITContext* ctx, JitLoopInfo* info);
void emit_int_loop_body(CodeBuf* cb, BytecodeChunk* chunk, int pc, const int* slot_gpr);
bool x86_64_emit_int_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                          JitLoopInfo* info, int step_sign, void** out_fn);

// per-function emit state, threaded through every category handler
typedef struct X86FuncEmitState {
    const X86_64Abi* abi;                                            // active abi descriptor
    JITContext*      ctx;                                            // owning jit context
    CodeBuf*         cb;                                             // destination code buffer
    BytecodeChunk*   chunk;                                          // bytecode being compiled
    int              func_idx;                                       // function being emitted
    int              start;                                          // first bytecode pc of the function
    int              end;                                            // one past the last bytecode pc
    int              arity;                                          // parameter count
    int              nregs;                                          // function frame size
    int              range_size;                                     // end - start
    size_t           mark;                                           // offset of the body entry (for self-call)
    bool             is_pure;                                        // true: strictly numeric (optimized emit); false: use checked base path

    bool*            is_target;                                      // jump-target marks per bytecode pc
    int32_t*         label_off;                                      // emitted label offset per pc (+1 sentinel)
    JumpFixup*       fixups;                                         // pending jump fixups
    int              fixup_count;                                    // number of fixups recorded so far
    uint8_t*         needs_flush;                                    // label requires an empty incoming cache
    uint8_t*         use_snap;                                       // label restores from a snapshot
    JitCacheSnap*    jump_snap;                                      // per-label cache snapshots

    size_t           rip_fixup_at[256];                              // rip-relative disp32 offsets awaiting patch
    int              rip_fixup_imm[256];                             // which pool immediate each fixup refers to
    int              rip_fixup_count;                                // pending rip fixups

    int32_t          func_imms[8];                                   // distinct immediates precomputed for this fn
    int              func_n_imms;                                    // number of pooled immediates

    size_t           tab_fixup_at[256];                              // inline jump-table slots awaiting patch
    int              tab_fixup_pc[256];                              // bytecode pc each slot must point at
    int              tab_fixup_count;                                // pending jump-table slots

    XmmCache*        cache;                                          // live xmm register cache

    int*             slot_pocket_gpr;                                // slot -> pocket gpr index, or -1
    int*             slot_to_pocket;                                 // pocket gpr index -> slot, or -1
    int              n_pockets;                                      // assigned pocket gprs
    int              pocket_slot_base;                               // first frame slot reserved for pocket saves
    int              base_frame;                                     // aligned rbp-relative frame base

    bool             func_has_backward_jump;                         // any jump present (disables dead-store elim)

    int              pc;                                             // current bytecode pc
    Instruction*     inst;                                           // current instruction
    int              d, a, b;                                        // operands[0], operands[1], operands[2]
    Opcode           op;                                             // current opcode

    bool             did_flush;                                      // handler already flushed / snapped the cache
    bool             prefer_xmm0;                                    // next op returns into this dest (xmm0 hint)
} X86FuncEmitState;

// per-instruction loop body emit state, shared by every loop category handler
typedef struct X86LoopBodyState {
    const X86_64Abi* abi;                                            // active abi descriptor
    JITContext*      ctx;                                            // owning jit context
    CodeBuf*         cb;                                             // destination code buffer
    XmmCache*        cache;                                          // live xmm register cache
    JitLoopInfo*     info;                                           // loop being emitted
    BytecodeChunk*   chunk;                                          // bytecode being compiled
    int              pc;                                             // bytecode pc of the current instruction
    int              const_slot;                                     // reserved frame slot holding 1.0
    int              save_slot;                                      // reserved frame slot for frame_reg spilling

    Instruction*     inst;                                           // current instruction
    int              d, a, b;                                        // operands[0], operands[1], operands[2]
    Opcode           op;                                             // current opcode

    bool             touched_nan_check;                              // emit NaN->none rewrite after the handler
} X86LoopBodyState;

static inline void x86_func_rip_fixup(X86FuncEmitState* st, size_t at, int imm_idx) {
    if (st->rip_fixup_count >= 256)                                  // pending rip fixups full?
        JIT_FATAL("rip fixup overflow in function %d", st->func_idx);
    st->rip_fixup_at[st->rip_fixup_count]  = at;                     // record disp32 field offset
    st->rip_fixup_imm[st->rip_fixup_count] = imm_idx;                // remember which pool entry
    st->rip_fixup_count++;
}

static inline void x86_func_jump_fixup(X86FuncEmitState* st, size_t at, int target_pc) {
    st->fixups[st->fixup_count].patch_at  = at;                      // record rel32 placeholder offset
    st->fixups[st->fixup_count].target_pc = target_pc;               // remember target pc
    st->fixup_count++;
}

static inline void x86_func_tab_fixup(X86FuncEmitState* st, size_t at, int target_pc) {
    if (st->tab_fixup_count >= 256)                                  // pending table slots full?
        JIT_FATAL("jump-table slot overflow in function %d", st->func_idx);
    st->tab_fixup_at[st->tab_fixup_count] = at;                      // record 8-byte slot offset
    st->tab_fixup_pc[st->tab_fixup_count] = target_pc;               // target pc each slot must resolve to
    st->tab_fixup_count++;
}

// function body category entry points
void x86_func_emit_move (X86FuncEmitState* st);                      // OP_MOVE, OP_LOAD_*
void x86_func_emit_arith(X86FuncEmitState* st);                      // OP_ADD..OP_DEC
void x86_func_emit_cmp  (X86FuncEmitState* st);                      // OP_CMP_*
void x86_func_emit_jump (X86FuncEmitState* st);                      // OP_JUMP*, OP_JUMP_IF_*
void x86_func_emit_match(X86FuncEmitState* st);                      // OP_JUMP_MATCH_*, OP_SWITCH_*
void x86_func_emit_call (X86FuncEmitState* st);                      // OP_CALL_*, OP_RETURN*

// loop body category entry points
void x86_loop_emit_move (X86LoopBodyState* st);                      // OP_MOVE, OP_LOAD_*, OP_LOAD/STORE_GLOBAL
void x86_loop_emit_arith(X86LoopBodyState* st);                      // OP_ADD..OP_DEC
void x86_loop_emit_cmp  (X86LoopBodyState* st);                      // OP_CMP_*
void x86_loop_emit_call (X86LoopBodyState* st);                      // OP_CALL_*
void x86_loop_emit_table(X86LoopBodyState* st);                      // OP_TABLE_*

// loop entry / dispatch helpers (defined in x86_64_emit_loop.c)
void emit_loop_body_instr(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                          XmmCache* cache, int pc, int const_slot, int save_slot,
                          JitLoopInfo* info);
size_t emit_loop_iteration(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                           XmmCache* cache, JitLoopInfo* info, int const_slot,
                           int save_slot, bool iter_in_xmm, int step_sign,
                           bool needs_helper, XmmCache* out_exit_cache);
bool loop_is_safe_to_emit(JITContext* ctx, JitLoopInfo* info);
bool x86_64_emit_numeric_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                              JitLoopInfo* info, int step_sign, void** out_fn);
bool x86_64_emit_table_iter_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                 JitLoopInfo* info, void** out_fn);
bool x86_64_emit_cond_enter_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                                 JitLoopInfo* info, void** out_fn);

#endif // APEX_JIT_X86_64_EMIT_INTERNAL_H
// source/compiler/codegen_internal.h
// Internal cross-file declarations for the codegen split
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef CODEGEN_INTERNAL_H
#define CODEGEN_INTERNAL_H

#include "codegen.h"
#include "vm.h"
#include <stdbool.h>
#include <stdint.h>

// snapshot of per-local numeric-ness flags for branch merging
typedef struct {
    int count;         // number of slots captured
    bool* flags;       // copied numeric-ness flags, or NULL when count is zero
    bool* int_flags;   // copied integer-ness flags, or NULL when count is zero
    bool* cflags;      // copied const-known flags, or NULL when count is zero
    double* cvals;     // copied const values, or NULL when count is zero
} LocalNumSnap;

// codegen.c
int  emit(CodeGenerator* cg, Instruction inst, int line);

// codegen_scope.c
int  alloc_register(CodeGenerator* cg);
void free_register(CodeGenerator* cg, int reg);
int  add_local(CodeGenerator* cg, const char* name);
int  find_local(CodeGenerator* cg, const char* name);
int  find_local_slot(CodeGenerator* cg, const char* name);
int  locals_high_water(CodeGenerator* cg);

// codegen_expr.c
int  codegen_expression(CodeGenerator* cg, ASTNode* node);
int  codegen_expression_into(CodeGenerator* cg, ASTNode* node, int dest_hint);
int  key_str_prefix_idx(CodeGenerator* cg, ASTNode* node);

// codegen_stmt.c
void codegen_block(CodeGenerator* cg, ASTNode* node);
void codegen_statement(CodeGenerator* cg, ASTNode* node);
int  codegen_assign_expr(CodeGenerator* cg, ASTNode* node, int dest_hint);

// codegen_if.c
void codegen_if_statement(CodeGenerator* cg, ASTNode* node);
int  codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line);

// codegen_for.c
void codegen_for_statement(CodeGenerator* cg, ASTNode* node);

// codegen_match.c
void codegen_match_statement(CodeGenerator* cg, ASTNode* node);
int  emit_match_check(CodeGenerator* cg, int subject_reg, ASTNode* pattern, int line);

// codegen_fn.c
void codegen_function_decl(CodeGenerator* cg, ASTNode* node);

// codegen_modules.c
bool is_known_builtin_module(const char* name);
void add_module_global(CodeGenerator* cg, const char* full_name);

// opt_const_fold.c
bool try_fold_number(CodeGenerator* cg, ASTNode* node, double* out);
bool try_fold_bool(CodeGenerator* cg, ASTNode* node, bool* out);
bool is_number_expression(CodeGenerator* cg, ASTNode* node);
bool is_integer_expression(CodeGenerator* cg, ASTNode* node);
bool ast_same_expr(ASTNode* a, ASTNode* b);

// opt_const_cache.c
int  num_cache_lookup(CodeGenerator* cg, double value);
void num_cache_add(CodeGenerator* cg, double value, int reg);
int  str_cache_lookup(CodeGenerator* cg, const char* value);
void str_cache_add(CodeGenerator* cg, const char* value, int reg);
void str_cache_invalidate(CodeGenerator* cg, int written_reg);

// opt_lvn.c
int  imm_lvn_lookup(CodeGenerator* cg, Opcode op, int left_reg, int right_reg, int imm);
void imm_lvn_add(CodeGenerator* cg, Opcode op, int left_reg, int right_reg, int imm, int result_reg);
void imm_lvn_invalidate(CodeGenerator* cg, int written_reg);
bool op_writes_dest_reg(Opcode op);

// opt_peephole.c
int  try_peephole_fuse(CodeGenerator* cg, Instruction* inst);

// opt_inline.c
bool function_is_inlinable(ASTNode* fn_decl);
bool try_inline_function(CodeGenerator* cg, int func_idx, ASTNodeList* arg_nodes,
                         int* arg_regs, int arg_count, int result_reg, int line);
bool try_fold_inline_call(CodeGenerator* cg, ASTNode* node, double* out);
bool expr_only_uses_params(ASTNode* node, ASTNodeList* params);

// opt_licm.c
bool body_unsafe_for_licm(ASTNode* node);
bool body_assigns_name(ASTNode* node, const char* name);
void collect_hoistable_numbers(CodeGenerator* cg, ASTNode* node);
void collect_hoistable_table_gets(CodeGenerator* cg, ASTNode* body);
void invalidate_loop_consts(CodeGenerator* cg, ASTNode* body, const char* loop_var);
bool for_is_zero_trip(CodeGenerator* cg, ASTNode* node);
bool expr_struct_eq(ASTNode* a, ASTNode* b);

// opt_dce.c
bool ast_references_local(ASTNode* node, const char* name);
bool stmt_references_local(ASTNode* node, const char* name);
bool is_local_dead_after_current_stmt(CodeGenerator* cg, const char* name);
bool ast_unsafe_direct_assign(ASTNode* node, const char* name);
bool codegen_expr_has_side_effect(ASTNode* node);

// opt_branch_merge.c
LocalNumSnap snap_numbers(CodeGenerator* cg);
void restore_numbers(CodeGenerator* cg, LocalNumSnap s);
void merge_numbers(CodeGenerator* cg, LocalNumSnap a, LocalNumSnap b);
void free_snap(LocalNumSnap s);

// opt_jump_targets.c
int  resolve_jump_target(CodeGenerator* cg, int pc);
void mark_jump_target(CodeGenerator* cg, int pc);
bool code_has_jump_to(CodeGenerator* cg, int target);

// wraps bytecode_patch_jump so every patched target is recorded in the bitset
#define PATCH_JUMP(cg, jump_idx, target) do {                       \
    int _t = resolve_jump_target((cg), (target));                   \
    bytecode_patch_jump((cg)->chunk, (jump_idx), (_t));             \
    mark_jump_target((cg), (_t));                                   \
} while (0)

#endif // CODEGEN_INTERNAL_H
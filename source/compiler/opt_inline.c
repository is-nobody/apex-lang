// source/compiler/opt_inline.c
// Inlining of small pure functions with parameter-only bodies
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// true when an expression is safe to duplicate at an inline site
static bool inline_expr_ok(ASTNode* node, const char** names, int name_count) {
    if (!node) return true;
    switch (node->type) {
        case AST_LITERAL_NUMBER:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
            return true;
        case AST_IDENTIFIER: {
            for (int i = 0; i < name_count; i++) {
                if (strcmp(names[i], node->identifier.name) == 0) return true;
            }
            return true;                          // unresolved names resolve to globals; same as callee
        }
        case AST_BINARY:
            return inline_expr_ok(node->binary.left,  names, name_count) &&
                   inline_expr_ok(node->binary.right, names, name_count);
        case AST_UNARY:
            return inline_expr_ok(node->unary.operand, names, name_count);
        case AST_TERNARY:
            return inline_expr_ok(node->ternary.condition,  names, name_count) &&
                   inline_expr_ok(node->ternary.true_expr,  names, name_count) &&
                   inline_expr_ok(node->ternary.false_expr, names, name_count);
        case AST_INDEX_ACCESS:
            return inline_expr_ok(node->access.object, names, name_count) &&
                   inline_expr_ok(node->access.member, names, name_count);
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++) {
                if (!inline_expr_ok(node->string_interp.parts->nodes[i], names, name_count)) return false;
            }
            return true;
        case AST_CALL:
            for (int i = 0; i < node->call.arguments->count; i++) {
                if (!inline_expr_ok(node->call.arguments->nodes[i], names, name_count)) return false;
            }
            return true;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++) {
                if (!inline_expr_ok(node->table_literal.items->nodes[i], names, name_count)) return false;
            }
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (!inline_expr_ok(kv->binary.left,  names, name_count)) return false;
                if (!inline_expr_ok(kv->binary.right, names, name_count)) return false;
            }
            return true;
        default:
            return false;
    }
}

// forward declaration
static bool inline_body_ok(ASTNode* body, const char** names, int* name_count, int* budget);

// true when a single statement is safe to inline; records any name it binds
static bool inline_stmt_ok(ASTNode* s, const char** names, int* name_count, int* budget) {
    if (*budget <= 0) return false;
    (*budget)--;
    switch (s->type) {
        case AST_VAR_DECL:
        case AST_ASSIGN:
            if (!s->var_assign.name || s->var_assign.access_path) return false;
            if (!inline_expr_ok(s->var_assign.value, names, *name_count)) return false;
            if (*name_count >= 24) return false;
            names[(*name_count)++] = s->var_assign.name;
            return true;
        case AST_RETURN_STMT:
            if (s->return_stmt.value &&
                !inline_expr_ok(s->return_stmt.value, names, *name_count)) return false;
            return true;
        case AST_IF_STMT:
            if (!inline_expr_ok(s->if_stmt.condition, names, *name_count)) return false;
            if (!inline_body_ok(s->if_stmt.then_branch, names, name_count, budget)) return false;
            if (s->if_stmt.elif_chain &&
                !inline_stmt_ok(s->if_stmt.elif_chain, names, name_count, budget)) return false;
            if (s->if_stmt.else_branch &&
                !inline_body_ok(s->if_stmt.else_branch, names, name_count, budget)) return false;
            return true;
        case AST_EXPR_STMT:
            return inline_expr_ok(s->expr_stmt.expression, names, *name_count);
        default:
            return false;
    }
}

// true when every statement in the block passes inline_stmt_ok
static bool inline_body_ok(ASTNode* body, const char** names, int* name_count, int* budget) {
    if (!body || body->type != AST_BLOCK) return false;
    for (int i = 0; i < body->block.statements->count; i++) {
        if (!inline_stmt_ok(body->block.statements->nodes[i], names, name_count, budget)) return false;
    }
    return true;
}

// true when a statement unconditionally transfers control away from this point
static bool stmt_always_returns(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_RETURN_STMT:
            return true;
        case AST_IF_STMT: {
            if (!stmt_always_returns(node->if_stmt.then_branch)) return false;
            ASTNode* elif = node->if_stmt.elif_chain;
            while (elif) {
                if (!stmt_always_returns(elif->if_stmt.then_branch)) return false;
                elif = elif->if_stmt.elif_chain;
            }
            return stmt_always_returns(node->if_stmt.else_branch);
        }
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++) {
                if (stmt_always_returns(node->block.statements->nodes[i])) return true;
            }
            return false;
        default:
            return false;
    }
}

// true when every identifier in expr matches one of the given parameter names
bool expr_only_uses_params(ASTNode* node, ASTNodeList* params) {
    if (!node) return true;                                                // empty expression is fine
    switch (node->type) {
        case AST_IDENTIFIER: {
            const char* n = node->identifier.name;                         // referenced name
            for (int i = 0; i < params->count; i++) {                      // scan parameter list
                if (strcmp(params->nodes[i]->param.name, n) == 0) return true;
            }
            return false;                                                  // not a parameter: reject
        }
        case AST_LITERAL_NUMBER:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
            return true;                                                   // literals are always safe
        case AST_BINARY:
            return expr_only_uses_params(node->binary.left, params) &&
                   expr_only_uses_params(node->binary.right, params);
        case AST_UNARY:
            return expr_only_uses_params(node->unary.operand, params);
        case AST_TERNARY:
            return expr_only_uses_params(node->ternary.condition, params) &&
                   expr_only_uses_params(node->ternary.true_expr, params) &&
                   expr_only_uses_params(node->ternary.false_expr, params);
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                if (!expr_only_uses_params(node->string_interp.parts->nodes[i], params)) return false;
            return true;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                if (!expr_only_uses_params(node->table_literal.items->nodes[i], params)) return false;
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (!expr_only_uses_params(kv->binary.left, params)) return false;
                if (!expr_only_uses_params(kv->binary.right, params)) return false;
            }
            return true;
        default:
            return false;                                                  // calls, index, await: reject
    }
}

// total AST node count of an expression; used for the code-bloat cap
static int ast_node_count(ASTNode* node) {
    if (!node) return 0;
    switch (node->type) {
        case AST_LITERAL_NUMBER:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
        case AST_IDENTIFIER:
            return 1;
        case AST_BINARY:
            return 1 + ast_node_count(node->binary.left) + ast_node_count(node->binary.right);
        case AST_UNARY:
            return 1 + ast_node_count(node->unary.operand);
        case AST_TERNARY:
            return 1 + ast_node_count(node->ternary.condition)
                     + ast_node_count(node->ternary.true_expr)
                     + ast_node_count(node->ternary.false_expr);
        case AST_STRING_INTERP: {
            int c = 1;
            for (int i = 0; i < node->string_interp.parts->count; i++)
                c += ast_node_count(node->string_interp.parts->nodes[i]);
            return c;
        }
        case AST_TABLE_LITERAL: {
            int c = 1;
            for (int i = 0; i < node->table_literal.items->count; i++)
                c += ast_node_count(node->table_literal.items->nodes[i]);
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                c += 1 + ast_node_count(kv->binary.left) + ast_node_count(kv->binary.right);
            }
            return c;
        }
        default:
            return 1;
    }
}

// true when a function can be inlined; result memoized per func_idx
bool function_is_inlinable(CodeGenerator* cg, int func_idx) {
    if (func_idx < 0 || func_idx >= cg->fn_decls_cap) return false;
    if (cg->fn_cache.inlinable[func_idx] >= 0)                     // already decided: O(1)
        return cg->fn_cache.inlinable[func_idx] == 1;

    ASTNode* fn_decl = cg->fn_decls[func_idx];                     // cached AST slot
    bool ok = false;
    if (fn_decl && fn_decl->type == AST_FUNCTION_DECL &&
        !fn_decl->function_decl.is_async) {
        ASTNodeList* params = fn_decl->function_decl.params;
        ASTNode* body = fn_decl->function_decl.body;
        if (params->count <= 8 && body && body->type == AST_BLOCK &&
            body->block.statements->count > 0 &&
            stmt_always_returns(body)) {                           // no fall-through
            const char* names[24];
            int name_count = 0;
            for (int i = 0; i < params->count && name_count < 24; i++)
                names[name_count++] = params->nodes[i]->param.name;
            int budget = 24;
            if (inline_body_ok(body, names, &name_count, &budget)) {
                int nc = ast_node_count(body);                     // compute cost once
                if (nc <= 60) {
                    cg->fn_cache.node_count[func_idx] = nc;        // stash for try_inline_function
                    ok = true;
                }
            }
        }
    }
    cg->fn_cache.inlinable[func_idx] = ok ? 1 : 0;                 // memoize decision
    return ok;
}

// total AST-node budget for a single top-level inline operation
#define INLINE_TOTAL_BUDGET 128

// inlines a function body at the call site: params are rebound to the caller's arg registers
bool try_inline_function(CodeGenerator* cg, int func_idx,
                         ASTNodeList* arg_nodes,
                         int* arg_regs, int arg_count,
                         int result_reg, int line) {
    (void)arg_nodes;                                               // kept for api stability; args already live in arg_regs
    if (cg->inline_depth >= 3) return false;                       // bound nesting depth

    ASTNode* fn_decl = cg->fn_decls[func_idx];
    ASTNodeList* params = fn_decl->function_decl.params;
    if (params->count != arg_count) return false;

    ASTNode* body = fn_decl->function_decl.body;

    // cumulative budget: reset only at the top-level inline, then let each nested inline decrement it
    if (cg->inline_depth == 0) cg->inline_budget = INLINE_TOTAL_BUDGET;
    int cost = cg->fn_cache.node_count[func_idx] >= 0              // reuse cached cost
             ? cg->fn_cache.node_count[func_idx]
             : ast_node_count(body);
    if (cost > cg->inline_budget) return false;                    // over budget: fall back to CALL
    cg->inline_budget -= cost;

    // save caller's locals table
    char**  saved_names    = cg->locals.names;
    int*    saved_regs     = cg->locals.registers;
    bool*   saved_is_num   = cg->locals.is_number;
    bool*   saved_is_int   = cg->locals.is_integer;
    bool*   saved_ck       = cg->locals.const_known;
    double* saved_cv       = cg->locals.const_value;
    bool*   saved_mat      = cg->locals.materialized;
    int     saved_count    = cg->locals.count;
    int     saved_cap      = cg->locals.capacity;

    // save other caller state
    int saved_next_reg    = cg->next_register;
    int saved_reg_floor   = cg->register_floor;
    int saved_cache_floor = cg->cache_floor;
    int saved_inline_reg  = cg->inline_result_reg;
    int saved_exit_count  = cg->inline_exit_count;

    // fresh locals table for params + room for body-defined names
    int cap = arg_count + 16;
    cg->locals.names        = (char**)malloc(sizeof(char*) * cap);
    cg->locals.registers    = (int*)malloc(sizeof(int) * cap);
    cg->locals.is_number    = (bool*)malloc(sizeof(bool) * cap);
    cg->locals.is_integer   = (bool*)malloc(sizeof(bool) * cap);
    cg->locals.const_known  = (bool*)malloc(sizeof(bool) * cap);
    cg->locals.const_value  = (double*)malloc(sizeof(double) * cap);
    cg->locals.materialized = (bool*)malloc(sizeof(bool) * cap);
    cg->locals.count        = arg_count;
    cg->locals.capacity     = cap;
    for (int i = 0; i < arg_count; i++) {
        ASTNode* p = params->nodes[i];
        cg->locals.names[i]        = strdup(p->param.name);
        cg->locals.registers[i]    = arg_regs[i];
        cg->locals.is_number[i]    = false;
        cg->locals.is_integer[i]   = false;
        cg->locals.const_known[i]  = false;
        cg->locals.const_value[i]  = 0.0;
        cg->locals.materialized[i] = true;
    }

    // protect the caller's live registers from temp resets inside the body
    int floor = cg->next_register;
    if (floor < result_reg + 1) floor = result_reg + 1;
    for (int i = 0; i < arg_count; i++) {
        if (floor < arg_regs[i] + 1) floor = arg_regs[i] + 1;
    }
    if (cg->register_floor < floor) cg->register_floor = floor;
    if (cg->next_register   < floor) cg->next_register   = floor;

    // derive the module prefix from the inlined function's registered name
    char inline_module[512];
    inline_module[0] = '\0';
    {
        const char* fname = cg->chunk->functions[func_idx].name;
        const char* fdot = strrchr(fname, '.');
        if (fdot) {
            size_t prefix_len = (size_t)(fdot - fname);
            if (prefix_len < sizeof(inline_module)) {
                memcpy(inline_module, fname, prefix_len);
                inline_module[prefix_len] = '\0';
            }
        }
    }
    char* saved_module = cg->current_module;
    if (inline_module[0]) cg->current_module = inline_module;

    // switch into inline mode: return statements now write to result_reg
    cg->inline_result_reg = result_reg;
    cg->inline_depth++;

    // emit the whole body; codegen_return routes each return to inline_exit
    codegen_block(cg, body);

    cg->inline_depth--;
    cg->inline_result_reg = saved_inline_reg;
    cg->current_module = saved_module;

    // patch every collected inline exit to point here
    int end_pc = bytecode_current_offset(cg->chunk);
    for (int i = saved_exit_count; i < cg->inline_exit_count; i++) {
        PATCH_JUMP(cg, cg->inline_exits[i], end_pc);
    }
    cg->inline_exit_count = saved_exit_count;

    // restore caller's locals table
    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);
    free(cg->locals.names);
    free(cg->locals.registers);
    free(cg->locals.is_number);
    free(cg->locals.is_integer);
    free(cg->locals.const_known);
    free(cg->locals.const_value);
    free(cg->locals.materialized);
    cg->locals.names        = saved_names;
    cg->locals.registers    = saved_regs;
    cg->locals.is_number    = saved_is_num;
    cg->locals.is_integer   = saved_is_int;
    cg->locals.const_known  = saved_ck;
    cg->locals.const_value  = saved_cv;
    cg->locals.materialized = saved_mat;
    cg->locals.count        = saved_count;
    cg->locals.capacity     = saved_cap;

    cg->next_register    = saved_next_reg;
    cg->register_floor   = saved_reg_floor;
    cg->cache_floor      = saved_cache_floor;
    (void)line;
    return true;
}

// folds a call to an inlinable single-return function whose arguments all fold to numbers
bool try_fold_inline_call(CodeGenerator* cg, ASTNode* node, double* out) {
    if (!cg || !node || node->type != AST_CALL) return false;
    ASTNode* callee = node->call.callee;
    if (callee->type != AST_IDENTIFIER) return false;
    const char* fname = callee->identifier.name;

    int func_idx = -1;
    for (int i = 0; i < cg->chunk->func_count; i++) {
        if (strcmp(cg->chunk->functions[i].name, fname) == 0) { func_idx = i; break; }
    }
    if (func_idx < 0 || func_idx >= cg->fn_decls_cap) return false;
    ASTNode* fn = cg->fn_decls[func_idx];
    if (!fn || !function_is_inlinable(cg, func_idx)) return false;

    ASTNodeList* params = fn->function_decl.params;
    ASTNodeList* args   = node->call.arguments;
    if (params->count != args->count || args->count > 8) return false;

    double arg_vals[8];
    for (int i = 0; i < args->count; i++) {
        if (!try_fold_number(cg, args->nodes[i], &arg_vals[i])) return false;
    }

    ASTNode* body = fn->function_decl.body;
    int stmt_count = body->block.statements->count;
    int n   = params->count;
    int cap = n + stmt_count;
    if (cap < 4) cap = 4;

    char**  s_names = cg->locals.names;
    int*    s_regs  = cg->locals.registers;
    bool*   s_num   = cg->locals.is_number;
    bool*   s_int   = cg->locals.is_integer;
    bool*   s_ck    = cg->locals.const_known;
    double* s_cv    = cg->locals.const_value;
    int     s_count = cg->locals.count;
    int     s_cap   = cg->locals.capacity;

    cg->locals.names        = (char**)malloc(sizeof(char*) * cap);
    cg->locals.registers    = (int*)malloc(sizeof(int) * cap);
    cg->locals.is_number    = (bool*)malloc(sizeof(bool) * cap);
    cg->locals.is_integer   = (bool*)malloc(sizeof(bool) * cap);
    cg->locals.const_known  = (bool*)malloc(sizeof(bool) * cap);
    cg->locals.const_value  = (double*)malloc(sizeof(double) * cap);
    cg->locals.count        = n;
    cg->locals.capacity     = cap;
    for (int i = 0; i < n; i++) {
        cg->locals.names[i]       = strdup(params->nodes[i]->param.name);
        cg->locals.registers[i]   = 0;
        cg->locals.is_number[i]   = true;
        cg->locals.is_integer[i]  = (arg_vals[i] == (double)(long long)arg_vals[i]);
        cg->locals.const_known[i] = true;
        cg->locals.const_value[i] = arg_vals[i];
    }

    bool ok = false;
    for (int i = 0; i < stmt_count && cg->locals.count < cap; i++) {
        ASTNode* stmt = body->block.statements->nodes[i];
        if (stmt->type == AST_VAR_DECL || stmt->type == AST_ASSIGN) {
            double cv;
            if (!try_fold_number(cg, stmt->var_assign.value, &cv)) break;
            int slot = find_local_slot(cg, stmt->var_assign.name);
            if (slot < 0) {
                slot = cg->locals.count++;
                cg->locals.names[slot]     = strdup(stmt->var_assign.name);
                cg->locals.registers[slot] = 0;
            }
            cg->locals.is_number[slot]    = true;
            cg->locals.is_integer[slot]   = (cv == (double)(long long)cv);
            cg->locals.const_known[slot]  = true;
            cg->locals.const_value[slot]  = cv;
        } else if (stmt->type == AST_RETURN_STMT) {
            ok = try_fold_number(cg, stmt->return_stmt.value, out);
            break;
        } else {
            break;                                                 // if/expr/etc: give up on the fold path
        }
    }

    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);
    free(cg->locals.names);
    free(cg->locals.registers);
    free(cg->locals.is_number);
    free(cg->locals.is_integer);
    free(cg->locals.const_known);
    free(cg->locals.const_value);
    cg->locals.names       = s_names;
    cg->locals.registers   = s_regs;
    cg->locals.is_number   = s_num;
    cg->locals.is_integer  = s_int;
    cg->locals.const_known = s_ck;
    cg->locals.const_value = s_cv;
    cg->locals.count       = s_count;
    cg->locals.capacity    = s_cap;
    return ok;
}
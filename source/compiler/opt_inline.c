// source/compiler/opt_inline.c
// Inlining of small pure functions with parameter-only bodies
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

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

// true when every identifier in expr matches one of the visible names
static bool expr_only_uses_names(ASTNode* node, const char** names, int name_count) {
    if (!node) return true;
    switch (node->type) {
        case AST_IDENTIFIER: {
            const char* n = node->identifier.name;
            for (int i = 0; i < name_count; i++) {
                if (strcmp(names[i], n) == 0) return true;
            }
            return false;
        }
        case AST_LITERAL_NUMBER:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
            return true;
        case AST_BINARY:
            return expr_only_uses_names(node->binary.left,  names, name_count) &&
                   expr_only_uses_names(node->binary.right, names, name_count);
        case AST_UNARY:
            return expr_only_uses_names(node->unary.operand, names, name_count);
        case AST_TERNARY:
            return expr_only_uses_names(node->ternary.condition,  names, name_count) &&
                   expr_only_uses_names(node->ternary.true_expr,  names, name_count) &&
                   expr_only_uses_names(node->ternary.false_expr, names, name_count);
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                if (!expr_only_uses_names(node->string_interp.parts->nodes[i], names, name_count)) return false;
            return true;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                if (!expr_only_uses_names(node->table_literal.items->nodes[i], names, name_count)) return false;
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (!expr_only_uses_names(kv->binary.left,  names, name_count)) return false;
                if (!expr_only_uses_names(kv->binary.right, names, name_count)) return false;
            }
            return true;
        default:
            return false;
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

// true when a function body is a sequence of pure assignments ending in return
static bool body_is_inlinable(ASTNode* body, ASTNodeList* params) {
    if (!body || body->type != AST_BLOCK) return false;
    int n = body->block.statements->count;
    if (n == 0 || n > 4) return false;                             // cap by statement count

    ASTNode* last = body->block.statements->nodes[n - 1];          // trailing statement
    if (last->type != AST_RETURN_STMT || !last->return_stmt.value) return false;

    const char* names[8];                                          // visible name set
    int name_count = 0;
    for (int i = 0; i < params->count && name_count < 8; i++) {
        names[name_count++] = params->nodes[i]->param.name;
    }

    for (int i = 0; i < n; i++) {
        ASTNode* stmt = body->block.statements->nodes[i];
        if (stmt->type == AST_VAR_DECL || stmt->type == AST_ASSIGN) {
            if (!stmt->var_assign.name || stmt->var_assign.access_path) return false;
            if (codegen_expr_has_side_effect(stmt->var_assign.value)) return false;
            if (!expr_only_uses_names(stmt->var_assign.value, names, name_count)) return false;
            if (name_count >= 8) return false;
            names[name_count++] = stmt->var_assign.name;
        } else if (stmt->type == AST_RETURN_STMT) {
            if (i != n - 1) return false;                          // return must be last
            if (!stmt->return_stmt.value) return false;
            if (codegen_expr_has_side_effect(stmt->return_stmt.value)) return false;
            if (!expr_only_uses_names(stmt->return_stmt.value, names, name_count)) return false;
        } else {
            return false;                                          // if/for/expr: reject
        }
    }
    return true;
}

// true when a function can be inlined: small, pure, params-only, no async
bool function_is_inlinable(ASTNode* fn_decl) {
    if (!fn_decl || fn_decl->type != AST_FUNCTION_DECL) return false;
    if (fn_decl->function_decl.is_async) return false;             // async returns a future: skip
    ASTNodeList* params = fn_decl->function_decl.params;
    if (params->count > 4) return false;                           // cap by arity to bound bloat
    ASTNode* body = fn_decl->function_decl.body;
    if (!body || body->type != AST_BLOCK) return false;
    if (!body_is_inlinable(body, params)) return false;
    if (ast_node_count(body) > 20) return false;                   // cap by AST size to bound bloat
    return true;
}

// inlines `return <expr>` at the call site by rebinding params to arg registers
bool try_inline_function(CodeGenerator* cg, int func_idx,
                         ASTNodeList* arg_nodes,
                         int* arg_regs, int arg_count,
                         int result_reg, int line) {
    ASTNode* fn_decl = cg->fn_decls[func_idx];
    ASTNodeList* params = fn_decl->function_decl.params;
    if (params->count != arg_count) return false;                  // arity mismatch: bail

    ASTNode* body = fn_decl->function_decl.body;
    int stmt_count = body->block.statements->count;

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

    // fresh locals table sized for params plus one slot per body statement
    int cap = arg_count + stmt_count;
    if (cap < 4) cap = 4;
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
        cg->locals.names[i]       = strdup(p->param.name);
        cg->locals.registers[i]   = arg_regs[i];                   // borrow the caller's arg reg
        cg->locals.is_number[i]   = true;                          // best-effort guess
        cg->locals.is_integer[i]  = false;
        cg->locals.const_known[i] = false;
        cg->locals.const_value[i] = 0.0;

        // seed constant-ness from a literal-number argument so the inlined body can be folded
        if (arg_nodes && i < arg_nodes->count) {
            ASTNode* a = arg_nodes->nodes[i];
            if (a && a->type == AST_LITERAL_NUMBER) {
                double v = a->literal_number.number_value;
                cg->locals.const_known[i] = true;
                cg->locals.const_value[i] = v;
                cg->locals.is_number[i]   = true;
                if (v == (double)(long long)v) cg->locals.is_integer[i] = true;
            }
        }
    }

    for (int i = 0; i < stmt_count; i++) {                         // emit body statements
        ASTNode* stmt = body->block.statements->nodes[i];
        if (stmt->type == AST_VAR_DECL || stmt->type == AST_ASSIGN) {
            int local_reg = add_local(cg, stmt->var_assign.name);   // fresh slot (or existing)
            int slot = find_local_slot(cg, stmt->var_assign.name);
            double cv;
            if (try_fold_number(cg, stmt->var_assign.value, &cv)) {
                // foldable: record the constant but emit no register write
                cg->locals.const_known[slot]  = true;
                cg->locals.const_value[slot]  = cv;
                cg->locals.is_number[slot]    = true;
                cg->locals.is_integer[slot]   = (cv == (double)(long long)cv);
                cg->locals.materialized[slot] = false;
            } else {
                codegen_expression_into(cg, stmt->var_assign.value, local_reg);
                cg->locals.is_number[slot]    = is_number_expression(cg, stmt->var_assign.value);
                cg->locals.is_integer[slot]   = is_integer_expression(cg, stmt->var_assign.value);
                cg->locals.const_known[slot]  = false;
                cg->locals.materialized[slot] = true;
            }
        } else if (stmt->type == AST_RETURN_STMT) {
            codegen_expression_into(cg, stmt->return_stmt.value, result_reg);
        }
    }

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
    if (!fn || !function_is_inlinable(fn)) return false;

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
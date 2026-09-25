// source/compiler/opt_recursion.c
// Compile-time constant folding for pure self-recursive functions.
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define REC_MEMO_MAX       4096
#define REC_FOLD_DEPTH_MAX 10000

// interpreter environment: local names -> numeric values
typedef struct {
    const char* names[16];
    double      values[16];
    int         count;
} RecEnv;

// bind or rebind a name in the interpreter env
static bool rec_env_bind(RecEnv* env, const char* name, double val) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) { env->values[i] = val; return true; }
    }
    if (env->count >= 16) return false;
    env->names[env->count]  = name;
    env->values[env->count] = val;
    env->count++;
    return true;
}

// resolve a callee identifier to a function index, or -1
static int resolve_callee_func_idx(CodeGenerator* cg, ASTNode* callee) {
    if (!callee || callee->type != AST_IDENTIFIER) return -1;
    const char* name = callee->identifier.name;
    for (int i = 0; i < cg->chunk->func_count; i++) {
        if (strcmp(cg->chunk->functions[i].name, name) == 0) return i;
    }
    if (cg->current_module && !strchr(name, '.')) {
        char q[512];
        snprintf(q, sizeof(q), "%s.%s", cg->current_module, name);
        for (int i = 0; i < cg->chunk->func_count; i++) {
            if (strcmp(cg->chunk->functions[i].name, q) == 0) return i;
        }
    }
    return -1;
}

// true when expr is a pure numeric tree with calls only to self_idx
static bool is_pure_expr(CodeGenerator* cg, ASTNode* n, int self_idx) {
    if (!n) return true;
    switch (n->type) {
        case AST_LITERAL_NUMBER:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
        case AST_IDENTIFIER:
            return true;
        case AST_UNARY:
            return is_pure_expr(cg, n->unary.operand, self_idx);
        case AST_BINARY:
            return is_pure_expr(cg, n->binary.left, self_idx) &&
                   is_pure_expr(cg, n->binary.right, self_idx);
        case AST_TERNARY:
            return is_pure_expr(cg, n->ternary.condition, self_idx) &&
                   is_pure_expr(cg, n->ternary.true_expr, self_idx) &&
                   is_pure_expr(cg, n->ternary.false_expr, self_idx);
        case AST_CALL: {
            if (resolve_callee_func_idx(cg, n->call.callee) != self_idx) return false;
            for (int i = 0; i < n->call.arguments->count; i++)
                if (!is_pure_expr(cg, n->call.arguments->nodes[i], self_idx)) return false;
            return true;
        }
        default:
            return false;                                 // tables, index, interp, await: reject
    }
}

// true when body is straight-line numeric with self-calls only
static bool is_pure_body(CodeGenerator* cg, ASTNode* n, int self_idx) {
    if (!n) return true;
    switch (n->type) {
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < n->block.statements->count; i++)
                if (!is_pure_body(cg, n->block.statements->nodes[i], self_idx)) return false;
            return true;
        case AST_VAR_DECL:
        case AST_ASSIGN:
            if (n->var_assign.access_path) return false;
            return is_pure_expr(cg, n->var_assign.value, self_idx);
        case AST_RETURN_STMT:
            return is_pure_expr(cg, n->return_stmt.value, self_idx);
        case AST_IF_STMT:
            return is_pure_expr(cg, n->if_stmt.condition, self_idx) &&
                   is_pure_body(cg, n->if_stmt.then_branch, self_idx) &&
                   is_pure_body(cg, n->if_stmt.elif_chain, self_idx) &&
                   is_pure_body(cg, n->if_stmt.else_branch, self_idx);
        case AST_EXPR_STMT:
            return is_pure_expr(cg, n->expr_stmt.expression, self_idx);
        default:
            return false;                                 // for, match, break, continue: reject
    }
}

// true when fn_decl is safe to memoize: pure, self-recursive, arity 1..N
static bool is_memoizable_fn(CodeGenerator* cg, int func_idx) {
    if (func_idx < 0 || func_idx >= cg->fn_decls_cap) return false;
    ASTNode* fn = cg->fn_decls[func_idx];
    if (!fn || fn->type != AST_FUNCTION_DECL) return false;
    if (fn->function_decl.is_async) return false;
    int argc = fn->function_decl.params->count;
    if (argc < 1 || argc > REC_MEMO_ARGS_MAX) return false;
    return is_pure_body(cg, fn->function_decl.body, func_idx);
}

// forward decls used by mutual recursion between expr and body walkers
static bool rec_fold_call(CodeGenerator* cg, ASTNode* call, RecEnv* env, double* out);
static bool rec_interp_expr(CodeGenerator* cg, ASTNode* n, RecEnv* env, double* out);
static bool rec_interp_body(CodeGenerator* cg, ASTNodeList* stmts, RecEnv* env, double* out);

// fold an expression with env extension; falls back to try_fold_number for outer consts
static bool rec_interp_expr(CodeGenerator* cg, ASTNode* n, RecEnv* env, double* out) {
    if (!n || !out) return false;
    switch (n->type) {
        case AST_LITERAL_NUMBER:
            *out = n->literal_number.number_value; return true;
        case AST_LITERAL_BOOL:
            *out = n->literal_bool.bool_value ? 1.0 : 0.0; return true;
        case AST_IDENTIFIER: {
            if (env) {
                for (int i = 0; i < env->count; i++)
                    if (strcmp(env->names[i], n->identifier.name) == 0) {
                        *out = env->values[i]; return true;
                    }
            }
            return try_fold_number(cg, n, out);
        }
        case AST_UNARY:
            if (n->unary.op == TOKEN_MINUS) {
                double v;
                if (!rec_interp_expr(cg, n->unary.operand, env, &v)) return false;
                *out = -v; return true;
            }
            if (n->unary.op == TOKEN_NOT) {
                double v;
                if (!rec_interp_expr(cg, n->unary.operand, env, &v)) return false;
                *out = (v == 0.0) ? 1.0 : 0.0; return true;
            }
            return false;
        case AST_BINARY: {
            double a, b;
            if (!rec_interp_expr(cg, n->binary.left,  env, &a)) return false;
            if (!rec_interp_expr(cg, n->binary.right, env, &b)) return false;
            switch (n->binary.op) {
                case TOKEN_PLUS:    *out = a + b; return true;
                case TOKEN_MINUS:   *out = a - b; return true;
                case TOKEN_STAR:    *out = a * b; return true;
                case TOKEN_SLASH:   if (b == 0.0) return false; *out = a / b; return true;
                case TOKEN_PERCENT: if (b == 0.0) return false; *out = fmod(a, b); return true;
                case TOKEN_EQUAL_EQUAL:   *out = (a == b) ? 1.0 : 0.0; return true;
                case TOKEN_NOT_EQUAL:     *out = (a != b) ? 1.0 : 0.0; return true;
                case TOKEN_LESS:          *out = (a <  b) ? 1.0 : 0.0; return true;
                case TOKEN_GREATER:       *out = (a >  b) ? 1.0 : 0.0; return true;
                case TOKEN_LESS_EQUAL:    *out = (a <= b) ? 1.0 : 0.0; return true;
                case TOKEN_GREATER_EQUAL: *out = (a >= b) ? 1.0 : 0.0; return true;
                case TOKEN_AND: *out = (a != 0.0 && b != 0.0) ? 1.0 : 0.0; return true;
                case TOKEN_OR:  *out = (a != 0.0 || b != 0.0) ? 1.0 : 0.0; return true;
                default: return false;
            }
        }
        case AST_CALL:
            return rec_fold_call(cg, n, env, out);
        default:
            return false;
    }
}

// walk a statement list; return first RETURN value into *out
static bool rec_interp_body(CodeGenerator* cg, ASTNodeList* stmts, RecEnv* env, double* out) {
    for (int i = 0; i < stmts->count; i++) {
        ASTNode* s = stmts->nodes[i];
        if (!s) continue;
        switch (s->type) {
            case AST_VAR_DECL:
            case AST_ASSIGN: {
                if (s->var_assign.access_path) return false;
                double v;
                if (!rec_interp_expr(cg, s->var_assign.value, env, &v)) return false;
                if (!rec_env_bind(env, s->var_assign.name, v)) return false;
                break;
            }
            case AST_RETURN_STMT:
                return rec_interp_expr(cg, s->return_stmt.value, env, out);
            case AST_IF_STMT: {
                double c;
                if (!rec_interp_expr(cg, s->if_stmt.condition, env, &c)) return false;
                ASTNode* branch = NULL;
                if (c != 0.0) branch = s->if_stmt.then_branch;
                else {
                    ASTNode* elif = s->if_stmt.elif_chain;
                    while (elif) {
                        double ec;
                        if (!rec_interp_expr(cg, elif->if_stmt.condition, env, &ec)) return false;
                        if (ec != 0.0) { branch = elif->if_stmt.then_branch; break; }
                        elif = elif->if_stmt.elif_chain;
                    }
                    if (!branch) branch = s->if_stmt.else_branch;
                }
                if (branch) {
                    if (rec_interp_body(cg, branch->block.statements, env, out)) return true;
                }
                break;                                    // fall through to next statement
            }
            case AST_EXPR_STMT:
                continue;
            default:
                return false;
        }
    }
    return false;                                         // no return seen
}

// reserve a slot in the compile-time memo table, or NULL when full
static RecMemoEntry* rec_memo_reserve(CodeGenerator* cg) {
    if (cg->rec_memo.count >= REC_MEMO_MAX) return NULL;
    if (cg->rec_memo.count >= cg->rec_memo.capacity) {
        int new_cap = cg->rec_memo.capacity == 0 ? 256 : cg->rec_memo.capacity * 2;
        if (new_cap > REC_MEMO_MAX) new_cap = REC_MEMO_MAX;
        void* p = realloc(cg->rec_memo.entries, sizeof(RecMemoEntry) * new_cap);
        if (!p) return NULL;
        cg->rec_memo.entries  = (RecMemoEntry*)p;
        cg->rec_memo.capacity = new_cap;
    }
    return &cg->rec_memo.entries[cg->rec_memo.count];
}

// compile-time evaluator: fold a call to a pure recursive fn with const args
static bool rec_fold_call(CodeGenerator* cg, ASTNode* call, RecEnv* env, double* out) {
    if (cg->rec_memo.depth >= REC_FOLD_DEPTH_MAX) return false;
    if (call->call.arguments->count > REC_MEMO_ARGS_MAX) return false;

    int fidx = resolve_callee_func_idx(cg, call->call.callee);
    if (fidx < 0) return false;
    if (!is_memoizable_fn(cg, fidx)) return false;

    int argc = call->call.arguments->count;
    double avals[REC_MEMO_ARGS_MAX];
    for (int i = 0; i < argc; i++) {
        if (!rec_interp_expr(cg, call->call.arguments->nodes[i], env, &avals[i])) return false;
    }

    for (int i = 0; i < cg->rec_memo.count; i++) {        // memo hit
        RecMemoEntry* e = &cg->rec_memo.entries[i];
        if (e->fidx != fidx || e->argc != argc) continue;
        bool same = true;
        for (int k = 0; k < argc; k++) if (e->args[k] != avals[k]) { same = false; break; }
        if (same) { *out = e->result; return true; }
    }

    ASTNode* fn = cg->fn_decls[fidx];
    ASTNodeList* params = fn->function_decl.params;
    RecEnv local = {0};
    for (int i = 0; i < argc; i++) {
        if (!rec_env_bind(&local, params->nodes[i]->param.name, avals[i])) return false;
    }

    cg->rec_memo.depth++;
    double result;
    bool ok = rec_interp_body(cg, fn->function_decl.body->block.statements, &local, &result);
    cg->rec_memo.depth--;
    if (!ok) return false;

    RecMemoEntry* e = rec_memo_reserve(cg);               // memoize result
    if (e) {
        e->fidx = fidx; e->argc = argc; e->result = result;
        for (int k = 0; k < argc; k++) e->args[k] = avals[k];
        cg->rec_memo.count++;
    }

    *out = result;
    return true;
}

// public entry: fold a call to a pure recursive fn with constant args
bool try_fold_recursive_call(CodeGenerator* cg, ASTNode* call, double* out) {
    if (!cg || !call || call->type != AST_CALL) return false;
    return rec_fold_call(cg, call, NULL, out);
}

// true when v is a boolean-shaped expression
static bool rec_expr_is_bool(ASTNode* v) {
    if (!v) return false;
    if (v->type == AST_LITERAL_BOOL) return true;
    if (v->type == AST_UNARY && v->unary.op == TOKEN_NOT) return true;
    if (v->type == AST_BINARY) {
        ApexTokenType op = v->binary.op;
        return op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL ||
               op == TOKEN_LESS || op == TOKEN_GREATER ||
               op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL ||
               op == TOKEN_AND || op == TOKEN_OR;
    }
    return false;
}

// all returns in the body must be boolean-shaped
static bool rec_body_returns_bool_only(CodeGenerator* cg, ASTNode* n) {
    (void)cg;
    if (!n) return true;
    switch (n->type) {
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < n->block.statements->count; i++)
                if (!rec_body_returns_bool_only(cg, n->block.statements->nodes[i])) return false;
            return true;
        case AST_RETURN_STMT:
            return rec_expr_is_bool(n->return_stmt.value);
        case AST_IF_STMT:
            return rec_body_returns_bool_only(cg, n->if_stmt.then_branch) &&
                   rec_body_returns_bool_only(cg, n->if_stmt.elif_chain) &&
                   rec_body_returns_bool_only(cg, n->if_stmt.else_branch);
        case AST_VAR_DECL:
        case AST_ASSIGN:
        case AST_EXPR_STMT:
            return true;                                  // non-return statement: skip
        default:
            return false;                                 // unknown shape: reject
    }
}

// true when a call targets a pure recursive fn whose body returns bool only
bool recursive_call_is_bool(CodeGenerator* cg, ASTNode* call) {
    if (!cg || !call || call->type != AST_CALL) return false;
    int fidx = resolve_callee_func_idx(cg, call->call.callee);
    if (fidx < 0) return false;
    if (!is_memoizable_fn(cg, fidx)) return false;
    return rec_body_returns_bool_only(cg, cg->fn_decls[fidx]->function_decl.body);
}

// fold a call to a bool-returning pure recursive fn with constant args
bool try_fold_recursive_bool(CodeGenerator* cg, ASTNode* call, bool* out) {
    if (!cg || !call || !out || call->type != AST_CALL) return false;
    int fidx = resolve_callee_func_idx(cg, call->call.callee);
    if (fidx < 0) return false;
    if (!is_memoizable_fn(cg, fidx)) return false;
    if (!rec_body_returns_bool_only(cg, cg->fn_decls[fidx]->function_decl.body)) return false;
    double v;
    if (!try_fold_recursive_call(cg, call, &v)) return false;
    *out = (v != 0.0);
    return true;
}
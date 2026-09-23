// source/core/codegen.c
// Implementation of Bytecode Code Generation for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// forward declarations for the dispatcher and helpers with dest_hint contract
static int add_local(CodeGenerator* cg, const char* name);
static int codegen_expression_into(CodeGenerator* cg, ASTNode* node, int dest_hint);
static void codegen_block(CodeGenerator* cg, ASTNode* node);
static int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line);
static int codegen_index_assign(CodeGenerator* cg, ASTNode* node, int dest_hint);
static int codegen_assign_expr(CodeGenerator* cg, ASTNode* node, int dest_hint);
static bool is_number_expression(CodeGenerator* cg, ASTNode* node);
static bool is_integer_expression(CodeGenerator* cg, ASTNode* node);

// checks if a name is a known built-in module root using first-char switch
static bool is_known_builtin_module(const char* name) {
    switch (name[0]) {
        case 'o':
            return strcmp(name, "os") == 0;        // os module
        case 's':
            return strcmp(name, "sys") == 0 ||     // sys module
                   strcmp(name, "string") == 0;    // string module
        case 'm':
            return strcmp(name, "math") == 0;      // math module
        case 't':
            return strcmp(name, "table") == 0;     // table module
        case 'r':
            return strcmp(name, "random") == 0 ||  // random module
                   strcmp(name, "regex") == 0;     // regex module
        case 'c':
            return strcmp(name, "csv") == 0 ||     // csv module
                   strcmp(name, "crypto") == 0;    // crypto module
        case 'j':
            return strcmp(name, "json") == 0;      // json module
        case 'x':
            return strcmp(name, "xml") == 0;       // xml module
        case 'b':
            return strcmp(name, "base") == 0;      // base module
        case 'z':
            return strcmp(name, "zip") == 0;       // zip module
        default:
            return false;                          // no builtin module matches
    }
}

// returns true if a subtree contains any function declaration
static bool block_has_function_decl(ASTNode* node) {
    if (!node) return false;                                      // null guard
    if (node->type == AST_FUNCTION_DECL) return true;             // found a nested function
    if (node->type == AST_BLOCK || node->type == AST_PROGRAM) {   // recurse into statement lists
        for (int i = 0; i < node->block.statements->count; i++) {
            if (block_has_function_decl(node->block.statements->nodes[i])) return true;
        }
        return false;
    }
    if (node->type == AST_IF_STMT) {                              // recurse into branches
        if (block_has_function_decl(node->if_stmt.then_branch)) return true;
        if (block_has_function_decl(node->if_stmt.elif_chain))  return true;
        if (block_has_function_decl(node->if_stmt.else_branch)) return true;
        return false;
    }
    if (node->type == AST_FOR_STMT) {                             // recurse into loop body
        return block_has_function_decl(node->for_stmt.body);
    }
    if (node->type == AST_MATCH_STMT) {                           // recurse into match cases
        if (block_has_function_decl(node->match_stmt.default_case)) return true;
        if (node->match_stmt.cases) {
            for (int i = 0; i < node->match_stmt.cases->count; i++) {
                if (block_has_function_decl(node->match_stmt.cases->nodes[i])) return true;
            }
        }
        return false;
    }
    if (node->type == AST_CASE) {                                 // recurse into case body
        return block_has_function_decl(node->case_stmt.body);
    }
    return false;                                                 // other nodes: no nested function
}

// recursively collects names assigned inside a function body, skipping nested fns
static void collect_local_names(CodeGenerator* cg, ASTNode* node) {
    if (!node) return;                                                      // null guard
    switch (node->type) {
        case AST_FUNCTION_DECL:
            return;                                                         // don't descend into nested fns
        case AST_VAR_DECL:
            if (node->var_assign.name) add_local(cg, node->var_assign.name);  // declaration binds a local
            collect_local_names(cg, node->var_assign.value);                // walk RHS
            break;
        case AST_ASSIGN:
            if (node->var_assign.name && !node->var_assign.access_path)
                add_local(cg, node->var_assign.name);                       // bare assign binds a local
            collect_local_names(cg, node->var_assign.value);                // walk RHS
            if (node->var_assign.access_path)                               // indexed assign: walk target
                collect_local_names(cg, node->var_assign.access_path);
            break;
        case AST_FOR_STMT:
            if (node->for_stmt.var_name) add_local(cg, node->for_stmt.var_name);  // loop var is a local
            collect_local_names(cg, node->for_stmt.start);                  // walk range start
            collect_local_names(cg, node->for_stmt.end);                    // walk range end
            collect_local_names(cg, node->for_stmt.step);                   // walk range step
            collect_local_names(cg, node->for_stmt.condition);              // walk condition loop
            collect_local_names(cg, node->for_stmt.body);                   // walk loop body
            break;
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)         // walk each statement
                collect_local_names(cg, node->block.statements->nodes[i]);
            break;
        case AST_IF_STMT:
            collect_local_names(cg, node->if_stmt.condition);               // walk condition
            collect_local_names(cg, node->if_stmt.then_branch);             // walk then branch
            collect_local_names(cg, node->if_stmt.elif_chain);              // walk elif chain
            collect_local_names(cg, node->if_stmt.else_branch);             // walk else branch
            break;
        case AST_MATCH_STMT:
            collect_local_names(cg, node->match_stmt.subject);              // walk subject
            for (int i = 0; i < node->match_stmt.cases->count; i++)         // walk each case
                collect_local_names(cg, node->match_stmt.cases->nodes[i]);
            collect_local_names(cg, node->match_stmt.default_case);         // walk default case
            break;
        case AST_CASE:
            collect_local_names(cg, node->case_stmt.body);                  // walk case body
            break;
        case AST_EXPR_STMT:
            collect_local_names(cg, node->expr_stmt.expression);            // walk expr
            break;
        case AST_RETURN_STMT:
            collect_local_names(cg, node->return_stmt.value);               // walk return value
            break;
        default:
            break;                                                          // other nodes: nothing to bind
    }
}

// returns true if an ast subtree reads a local variable by name
static bool ast_references_local(ASTNode* node, const char* name) {
    if (!node || !name) return false;                                       // guard
    switch (node->type) {
        case AST_IDENTIFIER:
            return strcmp(node->identifier.name, name) == 0;                // match by name
        case AST_LITERAL_NUMBER:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
            return false;                                                   // literals have no refs
        case AST_BINARY:
            return ast_references_local(node->binary.left, name) ||
                   ast_references_local(node->binary.right, name);          // either side
        case AST_UNARY:
            return ast_references_local(node->unary.operand, name);         // operand
        case AST_AWAIT:
            return ast_references_local(node->await_expr.expression, name); // awaited expression
        case AST_CALL:
            if (ast_references_local(node->call.callee, name)) return true; // callee
            for (int i = 0; i < node->call.arguments->count; i++) {         // any arg
                if (ast_references_local(node->call.arguments->nodes[i], name)) return true;
            }
            return false;
        case AST_INDEX_ACCESS:
            return ast_references_local(node->access.object, name) ||
                   ast_references_local(node->access.member, name);         // object or index
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++) {    // sequential items
                if (ast_references_local(node->table_literal.items->nodes[i], name)) return true;
            }
            for (int i = 0; i < node->table_literal.key_values->count; i++) {  // key-value pairs
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (ast_references_local(kv->binary.left, name)) return true;
                if (ast_references_local(kv->binary.right, name)) return true;
            }
            return false;
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++) {    // any part
                if (ast_references_local(node->string_interp.parts->nodes[i], name)) return true;
            }
            return false;
        case AST_TERNARY:
            return ast_references_local(node->ternary.condition, name) ||
                   ast_references_local(node->ternary.true_expr, name) ||
                   ast_references_local(node->ternary.false_expr, name);    // any branch
        case AST_ASSIGN:
            return ast_references_local(node->var_assign.value, name);      // nested assign RHS
        default:
            return false;                                                    // conservative
    }
}

// statement-level variant of ast_references_local: returns true if the statement
// reads or writes the named local, recursing into nested blocks
static bool stmt_references_local(ASTNode* node, const char* name) {
    if (!node || !name) return false;
    switch (node->type) {
        case AST_VAR_DECL:
            if (node->var_assign.name && strcmp(node->var_assign.name, name) == 0) return true;
            return ast_references_local(node->var_assign.value, name);
        case AST_ASSIGN:
            if (node->var_assign.name && strcmp(node->var_assign.name, name) == 0) return true;
            if (node->var_assign.access_path &&
                ast_references_local(node->var_assign.access_path, name)) return true;
            return ast_references_local(node->var_assign.value, name);
        case AST_EXPR_STMT:
            return ast_references_local(node->expr_stmt.expression, name);
        case AST_RETURN_STMT:
            return node->return_stmt.value &&
                   ast_references_local(node->return_stmt.value, name);
        case AST_IF_STMT:
            return ast_references_local(node->if_stmt.condition, name) ||
                   stmt_references_local(node->if_stmt.then_branch, name) ||
                   stmt_references_local(node->if_stmt.elif_chain, name) ||
                   stmt_references_local(node->if_stmt.else_branch, name);
        case AST_FOR_STMT:
            if (node->for_stmt.var_name && strcmp(node->for_stmt.var_name, name) == 0) return true;
            if (ast_references_local(node->for_stmt.start, name)) return true;
            if (ast_references_local(node->for_stmt.end, name)) return true;
            if (ast_references_local(node->for_stmt.step, name)) return true;
            if (ast_references_local(node->for_stmt.condition, name)) return true;
            return stmt_references_local(node->for_stmt.body, name);
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                if (stmt_references_local(node->block.statements->nodes[i], name)) return true;
            return false;
        case AST_MATCH_STMT:
            if (ast_references_local(node->match_stmt.subject, name)) return true;
            if (node->match_stmt.cases) {
                for (int i = 0; i < node->match_stmt.cases->count; i++)
                    if (stmt_references_local(node->match_stmt.cases->nodes[i], name)) return true;
            }
            return stmt_references_local(node->match_stmt.default_case, name);
        case AST_CASE:
            return stmt_references_local(node->case_stmt.body, name);
        case AST_FUNCTION_DECL:    // nested bodies have their own locals; outer locals
        case AST_BREAK_STMT:       // not visible inside
        case AST_CONTINUE_STMT:
        case AST_IMPORT_STMT:
            return false;
        default:
            return true;           // unknown: be conservative
    }
}

// true if `name` is never referenced (read or written) in any statement
// after the current position, walking up through enclosing blocks.
// returns false inside loops because the same code runs on later iterations
// and earlier statements in the same loop body are not visible here.
static bool is_local_dead_after_current_stmt(CodeGenerator* cg, const char* name) {
    if (!name || cg->loop_depth > 0) return false;
    int limit = cg->block_depth < 32 ? cg->block_depth : 32;
    for (int d = limit - 1; d >= 0; d--) {
        ASTNodeList* stmts = cg->block_stack[d].stmts;
        if (!stmts) continue;
        for (int i = cg->block_stack[d].index + 1; i < stmts->count; i++) {
            if (stmt_references_local(stmts->nodes[i], name)) return false;
        }
    }
    return true;
}

// true if writing `node`'s result into local `name` could expose a partial value
static bool ast_unsafe_direct_assign(ASTNode* node, const char* name) {
    if (!node || !name) return false;                                              // null guard
    switch (node->type) {
        case AST_STRING_INTERP:                                                    // check later parts only
            for (int i = 1; i < node->string_interp.parts->count; i++) {
                if (ast_references_local(node->string_interp.parts->nodes[i], name))
                    return true;                                               // later part reads name
            }
            return false;                                                          // safe to write directly

        case AST_TABLE_LITERAL:                                                    // check items and key-values
            for (int i = 0; i < node->table_literal.items->count; i++) {
                if (ast_references_local(node->table_literal.items->nodes[i], name))
                    return true;                                               // sequential item reads name
            }
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (ast_references_local(kv->binary.left,  name)) return true;     // key reads name
                if (ast_references_local(kv->binary.right, name)) return true;     // value reads name
            }
            return false;                                                          // safe to write directly

        default:
            return false;                                                          // dest written only at end
    }
}

// true when evaluating `node` can have an observable effect (call, await,
// nested assignment). Pure allocations and reads do not count.
static bool codegen_expr_has_side_effect(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_CALL:
        case AST_AWAIT:
        case AST_ASSIGN:
        case AST_VAR_DECL:
            return true;
        case AST_BINARY:
            return codegen_expr_has_side_effect(node->binary.left) ||
                   codegen_expr_has_side_effect(node->binary.right);
        case AST_UNARY:
            return codegen_expr_has_side_effect(node->unary.operand);
        case AST_INDEX_ACCESS:
            return codegen_expr_has_side_effect(node->access.object);
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                if (codegen_expr_has_side_effect(node->string_interp.parts->nodes[i])) return true;
            return false;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                if (codegen_expr_has_side_effect(node->table_literal.items->nodes[i])) return true;
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (codegen_expr_has_side_effect(kv->binary.left) ||
                    codegen_expr_has_side_effect(kv->binary.right)) return true;
            }
            return false;
        case AST_TERNARY:
            return codegen_expr_has_side_effect(node->ternary.condition) ||
                   codegen_expr_has_side_effect(node->ternary.true_expr) ||
                   codegen_expr_has_side_effect(node->ternary.false_expr);
        default:
            return false;                                    // identifiers, literals: pure
    }
}

// checks if a binary operator always produces a number result
static bool is_arithmetic_op(ApexTokenType op) {
    return op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR ||
           op == TOKEN_SLASH || op == TOKEN_PERCENT;
}

// tries to fold an expression into a compile-time numeric constant
static bool try_fold_number(ASTNode* node, double* out) {
    if (!node || !out) return false;                                   // null guard
    switch (node->type) {
        case AST_LITERAL_NUMBER:
            *out = node->literal_number.number_value;                  // literal value
            return true;
        case AST_UNARY:
            if (node->unary.op == TOKEN_MINUS) {                       // unary minus
                double v;
                if (!try_fold_number(node->unary.operand, &v)) return false;
                *out = -v;                                             // negate operand
                return true;
            }
            return false;                                              // not: operand may be non-number
        case AST_BINARY: {
            double l, r;
            if (!try_fold_number(node->binary.left,  &l)) return false;  // left must fold
            if (!try_fold_number(node->binary.right, &r)) return false;  // right must fold
            switch (node->binary.op) {                                 // evaluate arithmetic
                case TOKEN_PLUS:    *out = l + r;      return true;
                case TOKEN_MINUS:   *out = l - r;      return true;
                case TOKEN_STAR:    *out = l * r;      return true;
                case TOKEN_SLASH:                                          // division by zero
                    if (r == 0.0) return false;                        // falls back to runtime
                    *out = l / r;                      return true;
                case TOKEN_PERCENT:                                        // modulo by zero
                    if (r == 0.0) return false;                        // falls back to runtime
                    *out = fmod(l, r);                 return true;
                default:
                    return false;                                      // non-arithmetic op
            }
        }
        default:
            return false;                                              // identifiers, calls, etc.
    }
}

// tries to fold an expression into a compile-time boolean constant
// only bool literals, not, and numeric comparisons are considered
static bool try_fold_bool(ASTNode* node, bool* out) {
    if (!node || !out) return false;                                   // null guard
    switch (node->type) {
        case AST_LITERAL_BOOL:
            *out = node->literal_bool.bool_value;                      // literal value
            return true;
        case AST_UNARY:
            if (node->unary.op == TOKEN_NOT) {                         // logical not
                bool v;
                if (!try_fold_bool(node->unary.operand, &v)) return false;
                *out = !v;                                             // invert operand
                return true;
            }
            return false;                                              // unary minus is not a bool
        case AST_BINARY: {
            ApexTokenType op = node->binary.op;
            if (op == TOKEN_AND || op == TOKEN_OR) {                   // logical and/or
                bool l, r;
                if (!try_fold_bool(node->binary.left,  &l)) return false;
                if (!try_fold_bool(node->binary.right, &r)) return false;
                *out = (op == TOKEN_AND) ? (l && r) : (l || r);        // evaluate logical op
                return true;
            }
            if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL ||    // numeric comparisons
                op == TOKEN_LESS || op == TOKEN_GREATER ||
                op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
                double l, r;
                if (!try_fold_number(node->binary.left,  &l)) return false;
                if (!try_fold_number(node->binary.right, &r)) return false;
                switch (op) {                                          // evaluate comparison
                    case TOKEN_EQUAL_EQUAL:   *out = (l == r); return true;
                    case TOKEN_NOT_EQUAL:     *out = (l != r); return true;
                    case TOKEN_LESS:          *out = (l < r);  return true;
                    case TOKEN_GREATER:       *out = (l > r);  return true;
                    case TOKEN_LESS_EQUAL:    *out = (l <= r); return true;
                    case TOKEN_GREATER_EQUAL: *out = (l >= r); return true;
                    default: return false;                             // unreachable
                }
            }
            return false;                                              // non-bool binary op
        }
        default:
            return false;                                              // identifiers, calls, etc.
    }
}

// recursively collects foldable numeric constants in a loop body for hoisting
static void collect_hoistable_numbers(CodeGenerator* cg, ASTNode* node) {
    if (!node || cg->hoist.count >= 8) return;                    // null guard / cap hoisted registers

    double v;
    if (try_fold_number(node, &v)) {                              // pure numeric subtree?
        for (int i = 0; i < cg->hoist.count; i++) {               // dedupe by value
            if (cg->hoist.values[i] == v) return;                 // already collected
        }
        if (cg->hoist.count >= cg->hoist.capacity) {              // grow arrays
            cg->hoist.capacity = cg->hoist.capacity == 0 ? 8 : cg->hoist.capacity * 2;
            cg->hoist.values = (double*)realloc(cg->hoist.values, sizeof(double) * cg->hoist.capacity);
            cg->hoist.regs   = (int*)   realloc(cg->hoist.regs,   sizeof(int) * cg->hoist.capacity);
        }
        cg->hoist.values[cg->hoist.count] = v;                    // record value
        cg->hoist.regs[cg->hoist.count] = -1;                     // register assigned later
        cg->hoist.count++;
        return;                                                   // do not descend: subtree is fully constant
    }

    switch (node->type) {                                         // descend into non-constant subtrees
        case AST_BINARY:
            collect_hoistable_numbers(cg, node->binary.left);
            collect_hoistable_numbers(cg, node->binary.right);
            break;
        case AST_UNARY:
            collect_hoistable_numbers(cg, node->unary.operand);
            break;
        case AST_AWAIT:
            collect_hoistable_numbers(cg, node->await_expr.expression);
            break;
        case AST_CALL:
            for (int i = 0; i < node->call.arguments->count; i++)
                collect_hoistable_numbers(cg, node->call.arguments->nodes[i]);
            break;
        case AST_INDEX_ACCESS:
            collect_hoistable_numbers(cg, node->access.object);
            collect_hoistable_numbers(cg, node->access.member);
            break;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                collect_hoistable_numbers(cg, node->table_literal.items->nodes[i]);
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                collect_hoistable_numbers(cg, kv->binary.left);
                collect_hoistable_numbers(cg, kv->binary.right);
            }
            break;
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                collect_hoistable_numbers(cg, node->string_interp.parts->nodes[i]);
            break;
        case AST_TERNARY:
            collect_hoistable_numbers(cg, node->ternary.true_expr);
            collect_hoistable_numbers(cg, node->ternary.false_expr);
            break;
        case AST_ASSIGN:
        case AST_VAR_DECL:
            collect_hoistable_numbers(cg, node->var_assign.value);
            break;
        case AST_EXPR_STMT:
            collect_hoistable_numbers(cg, node->expr_stmt.expression);
            break;
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                collect_hoistable_numbers(cg, node->block.statements->nodes[i]);
            break;
        case AST_IF_STMT:
            collect_hoistable_numbers(cg, node->if_stmt.condition);
            collect_hoistable_numbers(cg, node->if_stmt.then_branch);
            collect_hoistable_numbers(cg, node->if_stmt.elif_chain);
            collect_hoistable_numbers(cg, node->if_stmt.else_branch);
            break;
        case AST_FOR_STMT:
            collect_hoistable_numbers(cg, node->for_stmt.start);
            collect_hoistable_numbers(cg, node->for_stmt.end);
            collect_hoistable_numbers(cg, node->for_stmt.step);
            collect_hoistable_numbers(cg, node->for_stmt.condition);
            collect_hoistable_numbers(cg, node->for_stmt.body);
            break;
        case AST_MATCH_STMT:
            collect_hoistable_numbers(cg, node->match_stmt.subject);
            for (int i = 0; i < node->match_stmt.cases->count; i++)
                collect_hoistable_numbers(cg, node->match_stmt.cases->nodes[i]);
            collect_hoistable_numbers(cg, node->match_stmt.default_case);
            break;
        case AST_CASE:
            collect_hoistable_numbers(cg, node->case_stmt.body);
            break;
        case AST_RETURN_STMT:
            collect_hoistable_numbers(cg, node->return_stmt.value);
            break;
        default:
            break;                                                // literals, identifiers: nothing
    }
}

// looks up a numeric constant in the per-function cache, returns register or -1
static int num_cache_lookup(CodeGenerator* cg, double value) {
    for (int i = 0; i < cg->num_cache.count; i++) {              // scan cached entries
        if (cg->num_cache.values[i] == value) {                  // match by exact value
            return cg->num_cache.regs[i];                        // return cached register
        }
    }
    return -1;                                                   // not cached
}

// records a numeric constant and pins its register for the rest of the function
static void num_cache_add(CodeGenerator* cg, double value, int reg) {
    if (cg->num_cache.count >= 16) return;                       // cap to limit register pressure
    if (cg->num_cache.count >= cg->num_cache.capacity) {         // need more space
        cg->num_cache.capacity = cg->num_cache.capacity == 0 ? 8 : cg->num_cache.capacity * 2;
        cg->num_cache.values = (double*)realloc(cg->num_cache.values,
                                                sizeof(double) * cg->num_cache.capacity);
        cg->num_cache.regs   = (int*)   realloc(cg->num_cache.regs,
                                                sizeof(int) * cg->num_cache.capacity);
    }
    cg->num_cache.values[cg->num_cache.count] = value;           // store constant value
    cg->num_cache.regs[cg->num_cache.count] = reg;               // store register
    cg->num_cache.count++;                                       // advance count
    if (cg->cache_floor <= reg) cg->cache_floor = reg + 1;       // pin above cache floor
}

// looks up a string literal in the per-function cache, returns register or -1
static int str_cache_lookup(CodeGenerator* cg, const char* value) {
    for (int i = 0; i < cg->str_cache.count; i++) {              // scan cached entries
        if (strcmp(cg->str_cache.values[i], value) == 0) {       // match by content
            return cg->str_cache.regs[i];                        // return cached register
        }
    }
    return -1;                                                   // not cached
}

// records a string literal and pins its register for the rest of the function
static void str_cache_add(CodeGenerator* cg, const char* value, int reg) {
    if (cg->str_cache.count >= 16) return;                       // cap to limit register pressure
    if (cg->str_cache.count >= cg->str_cache.capacity) {         // need more space
        cg->str_cache.capacity = cg->str_cache.capacity == 0 ? 8 : cg->str_cache.capacity * 2;
        cg->str_cache.values = (char**)realloc(cg->str_cache.values,
                                               sizeof(char*) * cg->str_cache.capacity);
        cg->str_cache.regs   = (int*)   realloc(cg->str_cache.regs,
                                                sizeof(int) * cg->str_cache.capacity);
    }
    cg->str_cache.values[cg->str_cache.count] = strdup(value);   // own a copy of the content
    cg->str_cache.regs[cg->str_cache.count]   = reg;             // store register
    cg->str_cache.count++;                                       // advance count
    if (cg->cache_floor <= reg) cg->cache_floor = reg + 1;       // pin above cache floor
}

// drops every string-cache entry whose register has just been overwritten
static void str_cache_invalidate(CodeGenerator* cg, int written_reg) {
    for (int i = 0; i < cg->str_cache.count; i++) {
        if (cg->str_cache.regs[i] == written_reg) {
            free(cg->str_cache.values[i]);                               // release owned copy
            cg->str_cache.values[i] = cg->str_cache.values[--cg->str_cache.count];
            cg->str_cache.regs[i]   = cg->str_cache.regs[cg->str_cache.count];
            i--;                                                         // recheck swapped-in entry
        }
    }
}

// looks up a previously computed arithmetic result with identical inputs
static int imm_lvn_lookup(CodeGenerator* cg, Opcode op, int left_reg, int right_reg, int imm) {
    for (int i = 0; i < cg->imm_lvn.count; i++) {                    // scan live entries
        if (cg->imm_lvn.entries[i].op == op &&
            cg->imm_lvn.entries[i].left_reg == left_reg &&
            cg->imm_lvn.entries[i].right_reg == right_reg &&
            cg->imm_lvn.entries[i].imm == imm) {
            return cg->imm_lvn.entries[i].result_reg;                // cache hit
        }
    }
    return -1;                                                       // not found
}

// records a freshly computed arithmetic result so later uses can reuse the register
static void imm_lvn_add(CodeGenerator* cg, Opcode op, int left_reg, int right_reg, int imm, int result_reg) {
    if (cg->imm_lvn.count >= 16) return;                             // cap to bound pressure
    cg->imm_lvn.entries[cg->imm_lvn.count].op         = op;          // opcode
    cg->imm_lvn.entries[cg->imm_lvn.count].left_reg   = left_reg;    // left operand
    cg->imm_lvn.entries[cg->imm_lvn.count].right_reg  = right_reg;   // right operand (-1 for *_IMM)
    cg->imm_lvn.entries[cg->imm_lvn.count].imm        = imm;         // immediate (0 for register-register)
    cg->imm_lvn.entries[cg->imm_lvn.count].result_reg = result_reg;  // register with result
    cg->imm_lvn.count++;                                             // one more entry
}

// drops every cache entry whose operand or result has just been overwritten
static void imm_lvn_invalidate(CodeGenerator* cg, int written_reg) {
    for (int i = 0; i < cg->imm_lvn.count; i++) {                    // compact in-place
        if (cg->imm_lvn.entries[i].left_reg == written_reg ||
            cg->imm_lvn.entries[i].right_reg == written_reg ||
            cg->imm_lvn.entries[i].result_reg == written_reg) {
            cg->imm_lvn.entries[i] = cg->imm_lvn.entries[--cg->imm_lvn.count];
            i--;                                                     // recheck swapped-in entry
        }
    }
}

// true when an opcode writes its operands[0] as a destination register
static bool op_writes_dest_reg(Opcode op) {
    switch (op) {
        case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
        case OP_LOAD_CONST: case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
        case OP_LOAD_BOOL: case OP_LOAD_NONE:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_CALL: case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
        case OP_CALL_BUILTIN: case OP_ASYNC_CALL: case OP_ASYNC_CALL_BUILTIN:
        case OP_LOAD_GLOBAL: case OP_TABLE_GET: case OP_TABLE_GET_CONST:
        case OP_TABLE_GET_INT: case OP_NEW_TABLE: case OP_CONCAT:
        case OP_AND: case OP_OR: case OP_NOT:
        case OP_AWAIT:
            return true;                                             // these write operands[0]
        default:
            return false;                                            // jumps, returns, stores do not
    }
}

// snapshot of per-local numeric-ness flags for branch merging
typedef struct {
    int count;         // number of slots captured
    bool* flags;       // copied numeric-ness flags, or NULL when count is zero
    bool* int_flags;   // copied integer-ness flags, or NULL when count is zero
} LocalNumSnap;

// default entry: no destination hint, caller owns the fresh temp
static int codegen_expression(CodeGenerator* cg, ASTNode* node) {
    return codegen_expression_into(cg, node, -1);                  // -1 means "fresh temp please"
}

// allocates a new virtual register for temporary values
static int alloc_register(CodeGenerator* cg) {
    if (cg->next_register < cg->cache_floor) {  // never allocate inside pinned cache
        cg->next_register = cg->cache_floor;    // bump up to the cache floor
    }
    int reg = cg->next_register++;   // allocate next register
    if (reg >= cg->max_registers) {  // track max used
        cg->max_registers = reg + 1;
    }
    return reg;                      // return register index
}

// releases a register if it's not a local variable and is the last allocated temp
static void free_register(CodeGenerator* cg, int reg) {
    for (int i = 0; i < cg->locals.count; i++) {  // check if local var
        if (cg->locals.registers[i] == reg) {
            return;                               // don't free local vars
        }
    }
    if (reg < cg->cache_floor) {                  // pinned by the numeric constant cache
        return;                                   // keep cached registers alive
    }
    for (int i = 0; i < cg->imm_lvn.count; i++) { // pinned by the LVN cache?
        if (cg->imm_lvn.entries[i].result_reg == reg) {
            return;                               // keep LVN-cached value alive
        }
    }
    if (reg == cg->next_register - 1) {           // only free last temp
        cg->next_register--;                      // decrement register count
    }
}

// finds the register holding a local variable by name, returns -1 if not found
static int find_local(CodeGenerator* cg, const char* name) {
    if (!name) return -1;                              // guard against null
    for (int i = 0; i < cg->locals.count; i++) {       // iterate locals
        if (strcmp(cg->locals.names[i], name) == 0) {  // compare names
            return cg->locals.registers[i];            // return register
        }
    }
    return -1;                                         // not found
}

// returns the slot index into cg->locals for the given name, or -1 if not found
static int find_local_slot(CodeGenerator* cg, const char* name) {
    if (!name) return -1;                              // guard against null
    for (int i = 0; i < cg->locals.count; i++) {       // iterate locals
        if (strcmp(cg->locals.names[i], name) == 0) {  // compare names
            return i;                                  // return slot index
        }
    }
    return -1;                                         // not found
}

// body may mutate an outer-scope table (call/await/nested fn/indexed assign)
static bool body_unsafe_for_licm(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_CALL: case AST_AWAIT: case AST_FUNCTION_DECL:
            return true;
        case AST_ASSIGN:
            if (node->var_assign.access_path) return true;   // indexed assign
            return body_unsafe_for_licm(node->var_assign.value);
        case AST_VAR_DECL:
            return body_unsafe_for_licm(node->var_assign.value);
        case AST_BINARY:
            return body_unsafe_for_licm(node->binary.left) ||
                   body_unsafe_for_licm(node->binary.right);
        case AST_UNARY:
            return body_unsafe_for_licm(node->unary.operand);
        case AST_INDEX_ACCESS:
            return body_unsafe_for_licm(node->access.object) ||
                   body_unsafe_for_licm(node->access.member);
        case AST_EXPR_STMT:
            return body_unsafe_for_licm(node->expr_stmt.expression);
        case AST_RETURN_STMT:
            return body_unsafe_for_licm(node->return_stmt.value);
        case AST_IF_STMT:
            return body_unsafe_for_licm(node->if_stmt.condition) ||
                   body_unsafe_for_licm(node->if_stmt.then_branch) ||
                   body_unsafe_for_licm(node->if_stmt.elif_chain) ||
                   body_unsafe_for_licm(node->if_stmt.else_branch);
        case AST_FOR_STMT:
            return body_unsafe_for_licm(node->for_stmt.start) ||
                   body_unsafe_for_licm(node->for_stmt.end) ||
                   body_unsafe_for_licm(node->for_stmt.step) ||
                   body_unsafe_for_licm(node->for_stmt.condition) ||
                   body_unsafe_for_licm(node->for_stmt.body);
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                if (body_unsafe_for_licm(node->block.statements->nodes[i])) return true;
            return false;
        case AST_MATCH_STMT:
            if (body_unsafe_for_licm(node->match_stmt.subject)) return true;
            if (node->match_stmt.cases) {
                for (int i = 0; i < node->match_stmt.cases->count; i++)
                    if (body_unsafe_for_licm(node->match_stmt.cases->nodes[i])) return true;
            }
            return body_unsafe_for_licm(node->match_stmt.default_case);
        case AST_CASE:
            return body_unsafe_for_licm(node->case_stmt.body);
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                if (body_unsafe_for_licm(node->string_interp.parts->nodes[i])) return true;
            return false;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                if (body_unsafe_for_licm(node->table_literal.items->nodes[i])) return true;
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (body_unsafe_for_licm(kv->binary.left) ||
                    body_unsafe_for_licm(kv->binary.right)) return true;
            }
            return false;
        case AST_TERNARY:
            return body_unsafe_for_licm(node->ternary.condition) ||
                   body_unsafe_for_licm(node->ternary.true_expr) ||
                   body_unsafe_for_licm(node->ternary.false_expr);
        default:
            return false;
    }
}

// true if `name` is assigned (bare or via index) anywhere in the body
static bool body_assigns_name(ASTNode* node, const char* name) {
    if (!node) return false;
    switch (node->type) {
        case AST_VAR_DECL:
        case AST_ASSIGN:
            if (node->var_assign.name && strcmp(node->var_assign.name, name) == 0)
                return true;
            if (body_assigns_name(node->var_assign.value, name)) return true;
            if (node->var_assign.access_path &&
                body_assigns_name(node->var_assign.access_path, name)) return true;
            return false;
        case AST_FOR_STMT:
            if (node->for_stmt.var_name && strcmp(node->for_stmt.var_name, name) == 0)
                return true;
            return body_assigns_name(node->for_stmt.body, name);
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                if (body_assigns_name(node->block.statements->nodes[i], name)) return true;
            return false;
        case AST_IF_STMT:
            return body_assigns_name(node->if_stmt.then_branch, name) ||
                   body_assigns_name(node->if_stmt.elif_chain, name) ||
                   body_assigns_name(node->if_stmt.else_branch, name);
        case AST_MATCH_STMT:
            if (node->match_stmt.cases) {
                for (int i = 0; i < node->match_stmt.cases->count; i++)
                    if (body_assigns_name(node->match_stmt.cases->nodes[i], name)) return true;
            }
            return body_assigns_name(node->match_stmt.default_case, name);
        case AST_CASE:
            return body_assigns_name(node->case_stmt.body, name);
        default:
            return false;
    }
}

// record a candidate; deduped by (table name, index)
static void add_hoistable_get(CodeGenerator* cg, const char* table, double idx) {
    for (int i = 0; i < cg->hoist.get_count; i++) {
        if (strcmp(cg->hoist.get_names[i], table) == 0 &&
            cg->hoist.get_indices[i] == idx) return;
    }
    if (cg->hoist.get_count >= cg->hoist.get_capacity) {
        cg->hoist.get_capacity = cg->hoist.get_capacity == 0 ? 8 : cg->hoist.get_capacity * 2;
        cg->hoist.get_names   = (const char**)realloc(cg->hoist.get_names,
                                                      sizeof(char*) * cg->hoist.get_capacity);
        cg->hoist.get_indices = (double*)realloc(cg->hoist.get_indices,
                                                 sizeof(double) * cg->hoist.get_capacity);
        cg->hoist.get_regs    = (int*)realloc(cg->hoist.get_regs,
                                              sizeof(int) * cg->hoist.get_capacity);
    }
    cg->hoist.get_names[cg->hoist.get_count]   = table;   // AST-owned string, alive
    cg->hoist.get_indices[cg->hoist.get_count] = idx;
    cg->hoist.get_regs[cg->hoist.get_count]    = -1;      // assigned later
    cg->hoist.get_count++;
}

// recursive walk collecting AST_INDEX_ACCESS nodes t[CONST] that qualify
static void scan_hoistable_gets(CodeGenerator* cg, ASTNode* node, ASTNode* body) {
    if (!node) return;
    if (node->type == AST_INDEX_ACCESS) {
        if (node->access.object->type == AST_IDENTIFIER &&
            node->access.member->type == AST_LITERAL_NUMBER) {
            const char* tname = node->access.object->identifier.name;
            int tslot = find_local_slot(cg, tname);
            double idx = node->access.member->literal_number.number_value;
            if (tslot >= 0 &&
                idx == (int)idx && idx >= 1 && idx <= 65535 &&
                !body_assigns_name(body, tname)) {
                add_hoistable_get(cg, tname, idx);
                return;                                  // accepted; don't recurse
            }
        }
    }
    switch (node->type) {
        case AST_BINARY:
            scan_hoistable_gets(cg, node->binary.left, body);
            scan_hoistable_gets(cg, node->binary.right, body);
            break;
        case AST_UNARY:
            scan_hoistable_gets(cg, node->unary.operand, body);
            break;
        case AST_INDEX_ACCESS:
            scan_hoistable_gets(cg, node->access.object, body);
            scan_hoistable_gets(cg, node->access.member, body);
            break;
        case AST_CALL:
            scan_hoistable_gets(cg, node->call.callee, body);
            for (int i = 0; i < node->call.arguments->count; i++)
                scan_hoistable_gets(cg, node->call.arguments->nodes[i], body);
            break;
        case AST_AWAIT:
            scan_hoistable_gets(cg, node->await_expr.expression, body);
            break;
        case AST_VAR_DECL:
        case AST_ASSIGN:
            scan_hoistable_gets(cg, node->var_assign.value, body);
            break;
        case AST_EXPR_STMT:
            scan_hoistable_gets(cg, node->expr_stmt.expression, body);
            break;
        case AST_RETURN_STMT:
            scan_hoistable_gets(cg, node->return_stmt.value, body);
            break;
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                scan_hoistable_gets(cg, node->block.statements->nodes[i], body);
            break;
        case AST_IF_STMT:
            scan_hoistable_gets(cg, node->if_stmt.condition, body);
            scan_hoistable_gets(cg, node->if_stmt.then_branch, body);
            scan_hoistable_gets(cg, node->if_stmt.elif_chain, body);
            scan_hoistable_gets(cg, node->if_stmt.else_branch, body);
            break;
        case AST_FOR_STMT:
            scan_hoistable_gets(cg, node->for_stmt.start, body);
            scan_hoistable_gets(cg, node->for_stmt.end, body);
            scan_hoistable_gets(cg, node->for_stmt.step, body);
            scan_hoistable_gets(cg, node->for_stmt.condition, body);
            scan_hoistable_gets(cg, node->for_stmt.body, body);
            break;
        case AST_MATCH_STMT:
            scan_hoistable_gets(cg, node->match_stmt.subject, body);
            if (node->match_stmt.cases) {
                for (int i = 0; i < node->match_stmt.cases->count; i++)
                    scan_hoistable_gets(cg, node->match_stmt.cases->nodes[i], body);
            }
            scan_hoistable_gets(cg, node->match_stmt.default_case, body);
            break;
        case AST_CASE:
            scan_hoistable_gets(cg, node->case_stmt.body, body);
            break;
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                scan_hoistable_gets(cg, node->string_interp.parts->nodes[i], body);
            break;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                scan_hoistable_gets(cg, node->table_literal.items->nodes[i], body);
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                scan_hoistable_gets(cg, kv->binary.left, body);
                scan_hoistable_gets(cg, kv->binary.right, body);
            }
            break;
        case AST_TERNARY:
            scan_hoistable_gets(cg, node->ternary.condition, body);
            scan_hoistable_gets(cg, node->ternary.true_expr, body);
            scan_hoistable_gets(cg, node->ternary.false_expr, body);
            break;
        default:
            break;
    }
}

// entry point: fills cg->hoist.get_* for the loop body
static void collect_hoistable_table_gets(CodeGenerator* cg, ASTNode* body) {
    if (!body) return;
    if (body_unsafe_for_licm(body)) return;   // calls, awaits, nested fns, indexed assigns
    scan_hoistable_gets(cg, body, body);
}

// per-slot first/last statement index, in program order; control-flow counts as one stmt.
static void liveness_walk(CodeGenerator* cg, ASTNode* node, int* stmt_idx,
                          int* first, int* last) {
    if (!node) return;
    switch (node->type) {
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                liveness_walk(cg, node->block.statements->nodes[i], stmt_idx, first, last);
            return;
        case AST_FUNCTION_DECL:
            return;                              // nested function has its own locals
        default: {
            (*stmt_idx)++;
            int idx = *stmt_idx;
            for (int i = 0; i < cg->locals.count; i++) {
                if (stmt_references_local(node, cg->locals.names[i])) {
                    if (first[i] < 0) first[i] = idx;
                    last[i] = idx;
                }
            }
            return;
        }
    }
}

// overlap-aware linear scan: body locals share regs when live ranges don't intersect; params pin 0..n-1.
static void assign_registers_linear_scan(CodeGenerator* cg, int n_params,
                                         int* first, int* last) {
    int n = cg->locals.count;
    int body_count = n - n_params;
    if (body_count <= 0) return;

    int* order = (int*)malloc(sizeof(int) * body_count);
    for (int i = 0; i < body_count; i++) order[i] = n_params + i;

    // insertion sort body-local slots by first use (unused go last)
    for (int i = 1; i < body_count; i++) {
        int key = order[i];
        int kf = first[key]; if (kf < 0) kf = 0x7fffffff;
        int j = i - 1;
        while (j >= 0) {
            int jf = first[order[j]]; if (jf < 0) jf = 0x7fffffff;
            if (jf > kf) { order[j + 1] = order[j]; j--; }
            else break;
        }
        order[j + 1] = key;
    }

    // holder[r] = slot holding r; release[r] = stmt index at which r becomes dead
    int cap = n + 16;
    int* holder  = (int*)malloc(sizeof(int) * cap);
    int* release = (int*)malloc(sizeof(int) * cap);
    for (int r = 0; r < cap; r++) { holder[r] = -1; release[r] = -1; }

    for (int p = 0; p < n_params; p++) {           // seed params, protect their registers
        holder[p]  = p;
        release[p] = (last[p] > 0) ? last[p] : 0;
    }

    int max_used = n_params - 1;
    for (int i = 0; i < body_count; i++) {
        int slot = order[i];
        int f = first[slot];
        if (f < 0) { cg->locals.registers[slot] = 0; continue; }   // never used

        for (int r = n_params; r < cap; r++) {                     // release dead registers
            if (holder[r] >= 0 && release[r] < f) {
                holder[r]  = -1;
                release[r] = -1;
            }
        }
        int assigned = -1;
        for (int r = n_params; r < cap; r++) {                     // lowest free
            if (holder[r] < 0) { assigned = r; break; }
        }
        if (assigned < 0) assigned = cap++;                        // safety, should not hit
        holder[assigned]  = slot;
        release[assigned] = last[slot];
        cg->locals.registers[slot] = assigned;
        if (assigned > max_used) max_used = assigned;
    }

    cg->next_register = max_used + 1;
    cg->max_registers = cg->next_register;          // reset; body emission bumps it again

    free(order);
    free(holder);
    free(release);
}

// captures the current per-local numeric-ness flags
static LocalNumSnap snap_numbers(CodeGenerator* cg) {
    LocalNumSnap s;
    s.count = cg->locals.count;                                              // size of current local table
    s.flags     = s.count ? (bool*)malloc(sizeof(bool) * s.count) : NULL;    // allocate snapshot buffer
    s.int_flags = s.count ? (bool*)malloc(sizeof(bool) * s.count) : NULL;    // allocate integer snapshot
    if (s.flags)     memcpy(s.flags,     cg->locals.is_number,  sizeof(bool) * s.count);  // copy number flags
    if (s.int_flags) memcpy(s.int_flags, cg->locals.is_integer, sizeof(bool) * s.count);  // copy integer flags
    return s;                                                                // return snapshot
}

// restores per-local numeric-ness flags from a snapshot
static void restore_numbers(CodeGenerator* cg, LocalNumSnap s) {
    int n = cg->locals.count < s.count ? cg->locals.count : s.count;         // clamp to smaller size
    if (n > 0 && s.flags)     memcpy(cg->locals.is_number,  s.flags,     sizeof(bool) * n);  // copy back
    if (n > 0 && s.int_flags) memcpy(cg->locals.is_integer, s.int_flags, sizeof(bool) * n);  // copy back
}

// intersects two snapshots into cg (used at control-flow merge points)
static void merge_numbers(CodeGenerator* cg, LocalNumSnap a, LocalNumSnap b) {
    int n = cg->locals.count;                                                // upper bound from current
    if (a.count < n) n = a.count;                                            // clamp to first snapshot
    if (b.count < n) n = b.count;                                            // clamp to second snapshot
    for (int i = 0; i < n; i++) {                                            // walk all common slots
        cg->locals.is_number[i]  = a.flags[i]     && b.flags[i];             // number only if both paths number
        cg->locals.is_integer[i] = a.int_flags[i] && b.int_flags[i];         // integer only if both paths integer
    }
}

// frees a numeric-ness snapshot
static void free_snap(LocalNumSnap s) {
    free(s.flags);                                                           // release numeric flags buffer
    free(s.int_flags);                                                       // release integer flags buffer
}

// adds a local variable and assigns it a register, returns the register
static int add_local(CodeGenerator* cg, const char* name) {
    int existing = find_local(cg, name);                                   // check if already exists
    if (existing >= 0) return existing;                                    // return existing register
    
    if (cg->locals.count >= cg->locals.capacity) {                                      // need more space
        cg->locals.capacity = cg->locals.capacity == 0 ? 16 : cg->locals.capacity * 2;  // double capacity
        cg->locals.names = (char**)realloc(cg->locals.names,                            // resize names array
                                           sizeof(char*) * cg->locals.capacity);
        cg->locals.registers = (int*)realloc(cg->locals.registers,                      // resize registers array
                                             sizeof(int) * cg->locals.capacity);
        cg->locals.is_number = (bool*)realloc(cg->locals.is_number,                     // resize numeric-ness array
                                             sizeof(bool) * cg->locals.capacity);
        cg->locals.is_integer = (bool*)realloc(cg->locals.is_integer,                   // resize integer-ness array
                                              sizeof(bool) * cg->locals.capacity);
    }
    int reg = alloc_register(cg);                                          // allocate new register
    cg->locals.names[cg->locals.count] = strdup(name);                     // copy name
    cg->locals.registers[cg->locals.count] = reg;                          // store register
    cg->locals.is_number[cg->locals.count] = false;                        // unknown until assigned
    cg->locals.is_integer[cg->locals.count] = false;                       // unknown until assigned
    cg->locals.count++;                                                    // increment count
    return reg;                                                            // return register
}

// returns next_register value with all locals preserved and no temps
static int locals_high_water(CodeGenerator* cg) {
    int highest = -1;                                          // no locals yet
    for (int i = 0; i < cg->locals.count; i++) {               // scan local slots
        if (cg->locals.registers[i] > highest) {               // track max
            highest = cg->locals.registers[i];
        }
    }
    return highest + 1;                                        // next free index
}

// pure ops: writes operands[0] with no observable side effect
static bool op_is_pure(Opcode op) {
    switch (op) {
        case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
        case OP_LOAD_CONST: case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
        case OP_LOAD_BOOL: case OP_LOAD_NONE: case OP_LOAD_GLOBAL:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_AND: case OP_OR: case OP_NOT:
        case OP_TABLE_GET: case OP_TABLE_GET_CONST: case OP_TABLE_GET_INT:
        case OP_CONCAT: case OP_NEW_TABLE:
            return true;
        default:
            return false;
    }
}

// does inst read `reg` as a source operand? op-specific, respects operand types
static bool inst_reads_reg(Instruction* inst, int reg) {
    switch (inst->opcode) {
        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM: case OP_LOAD_CONST:
        case OP_LOAD_BOOL: case OP_LOAD_NONE: case OP_LOAD_GLOBAL:
            return false;                                       // no source registers
        case OP_MOVE: case OP_NEG: case OP_ADD: case OP_SUB:
        case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_AND: case OP_OR: case OP_CONCAT:
            return inst->operands[1] == reg || inst->operands[2] == reg;
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM:
            return inst->operands[1] == reg;                    // operand 2 is immediate
        case OP_INC: case OP_DEC:
            return inst->operands[0] == reg;                    // reads its own dest
        case OP_TABLE_GET: case OP_TABLE_GET_NUM:
            return inst->operands[1] == reg || inst->operands[2] == reg;
        case OP_TABLE_GET_INT:
            return inst->operands[1] == reg;
        default:
            return true;                                        // conservative
    }
}

// records that `pc` is a jump target; grows the bitset on demand
static void mark_jump_target(CodeGenerator* cg, int pc) {
    if (pc < 0) return;
    if (pc >= cg->jump_targets_cap) {
        int new_cap = cg->jump_targets_cap < 256 ? 256 : cg->jump_targets_cap;
        while (new_cap <= pc) new_cap *= 2;
        cg->jump_targets = (uint8_t*)realloc(cg->jump_targets, new_cap);
        memset(cg->jump_targets + cg->jump_targets_cap, 0,
               new_cap - cg->jump_targets_cap);
        cg->jump_targets_cap = new_cap;
    }
    cg->jump_targets[pc] = 1;
}

// true when some jump in the chunk targets `target` (O(1) lookup)
static bool code_has_jump_to(CodeGenerator* cg, int target) {
    return target >= 0 && target < cg->jump_targets_cap &&
           cg->jump_targets[target] != 0;
}

// wraps bytecode_patch_jump so every patched target is recorded in the bitset
#define PATCH_JUMP(cg, jump_idx, target) do {                       \
    bytecode_patch_jump((cg)->chunk, (jump_idx), (target));         \
    mark_jump_target((cg), (target));                               \
} while (0)

// fuse this instruction into the previous one; returns fused pc or -1
static int try_peephole_fuse(CodeGenerator* cg, Instruction* inst) {
    if (cg->chunk->code_count == 0) return -1;
    Instruction* prev = &cg->chunk->code[cg->chunk->code_count - 1];
    int d = inst->operands[0], a = inst->operands[1], b = inst->operands[2];

    // bail when a jump lands on the slot we would drop
    if (code_has_jump_to(cg, cg->chunk->code_count)) return -1;

    // LOAD_NONE d + RETURN|RETURN_NUM|RETURN_BOOL d  ->  RETURN_NONE
    if ((inst->opcode == OP_RETURN || inst->opcode == OP_RETURN_NUM ||
         inst->opcode == OP_RETURN_BOOL) &&
        prev->opcode == OP_LOAD_NONE && prev->operands[0] == d) {
        prev->opcode = OP_RETURN_NONE;
        prev->operands[0] = prev->operands[1] = prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // LOAD_NUM_IMM d,k + RETURN|RETURN_NUM d  ->  RETURN_NUM_IMM k
    if ((inst->opcode == OP_RETURN || inst->opcode == OP_RETURN_NUM) &&
        prev->opcode == OP_LOAD_NUM_IMM && prev->operands[0] == d &&
        prev->operands[1] >= 0 && prev->operands[1] <= 65535) {
        int k = prev->operands[1];
        prev->opcode = OP_RETURN_NUM_IMM;
        prev->operands[0] = 0;
        prev->operands[1] = k;
        prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // MOVE d,a + RETURN|RETURN_NUM|RETURN_BOOL d  ->  RETURN* a
    if ((inst->opcode == OP_RETURN || inst->opcode == OP_RETURN_NUM ||
         inst->opcode == OP_RETURN_BOOL) &&
        prev->opcode == OP_MOVE && prev->operands[0] == d &&
        prev->operands[1] != d) {
        int ma = prev->operands[1];
        prev->opcode = inst->opcode;
        prev->operands[0] = ma;
        prev->operands[1] = prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // <any pure write> + RETURN_NONE  ->  RETURN_NONE
    if (inst->opcode == OP_RETURN_NONE && op_is_pure(prev->opcode)) {
        prev->opcode = OP_RETURN_NONE;
        prev->operands[0] = prev->operands[1] = prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // <pure write to d> + <write to d not reading d>  ->  drop prev, keep inst
    if (op_writes_dest_reg(inst->opcode) && op_is_pure(prev->opcode) &&
        op_writes_dest_reg(prev->opcode) && prev->operands[0] == d &&
        !inst_reads_reg(inst, d)) {
        prev->opcode = inst->opcode;
        prev->operands[0] = d;
        prev->operands[1] = a;
        prev->operands[2] = b;
        return cg->chunk->code_count - 1;
    }

    // MOVE d,a + <write d reading d>  ->  substitute a for d in sources
    if (prev->opcode == OP_MOVE && op_writes_dest_reg(inst->opcode) &&
        prev->operands[0] == d && prev->operands[1] != d) {
        int ma = prev->operands[1];
        prev->opcode = inst->opcode;
        prev->operands[0] = d;
        prev->operands[1] = (a == d) ? ma : a;
        prev->operands[2] = (b == d) ? ma : b;
        return cg->chunk->code_count - 1;
    }

    return -1;
}

// emits an instruction with source line info for debugging
static int emit(CodeGenerator* cg, Instruction inst, int line) {
    int fused_idx = try_peephole_fuse(cg, &inst);
    if (fused_idx >= 0) {
        Instruction* fused = &cg->chunk->code[fused_idx];
        if (op_writes_dest_reg(fused->opcode)) {
            imm_lvn_invalidate(cg, fused->operands[0]);
            str_cache_invalidate(cg, fused->operands[0]);
        }
        return fused_idx;
    }

    bool is_jump = (inst.opcode == OP_JUMP ||
                    (inst.opcode >= OP_JUMP_IF_FALSE && inst.opcode <= OP_JUMP_IF_GTE) ||
                    (inst.opcode >= OP_JUMP_IF_EQ_IMM && inst.opcode <= OP_JUMP_IF_GTE_IMM) ||
                    inst.opcode == OP_JUMP_MATCH_NUM || inst.opcode == OP_JUMP_MATCH_STR ||
                    inst.opcode == OP_JUMP_MATCH_BOOL || inst.opcode == OP_JUMP_MATCH_NONE);

    // backward jumps are emitted with their final target already in operands[0]
    if (is_jump && inst.operands[0] != 0) {
        mark_jump_target(cg, inst.operands[0]);
    }

    if (is_jump) {
        cg->imm_lvn.count = 0;
    } else if (op_writes_dest_reg(inst.opcode)) {
        imm_lvn_invalidate(cg, inst.operands[0]);
        str_cache_invalidate(cg, inst.operands[0]);
    }
    return bytecode_emit_line(cg->chunk, inst, line);
}

// creates a new code generator
CodeGenerator* codegen_create(BytecodeChunk* chunk) {
    CodeGenerator* cg = (CodeGenerator*)calloc(1, sizeof(CodeGenerator));  // allocate and zero
    cg->chunk = chunk;                                                     // store bytecode chunk
    cg->next_register = 0;                                                 // start at 0
    cg->max_registers = 0;                                                 // no registers yet
    cg->current_function = -1;                                             // no active function
    cg->label_counter = 0;                                                 // label counter
    cg->current_call_is_awaited = false;                                   // no awaited call in flight

    cg->loop_stack.break_capacity = 16;                                    // initial break capacity
    cg->loop_stack.break_jumps = (int*)malloc(sizeof(int) * cg->loop_stack.break_capacity);  // allocate breaks
    
    cg->current_module = NULL;                                             // no current module
    cg->imported_modules = NULL;                                           // no imports
    cg->module_count = 0;                                                  // zero imports
    cg->module_capacity = 0;                                               // no capacity
    cg->module_globals = NULL;                                             // no module globals
    cg->module_globals_count = 0;                                          // zero module globals
    cg->module_globals_capacity = 0;                                       // no capacity
    cg->register_floor = 0;                                                // no floor at top level
    cg->cache_floor    = 0;                                                // no cache pins yet
    cg->for_scope_depth = 0;                                               // not inside any for
    cg->jump_targets     = NULL;
    cg->jump_targets_cap = 0;
    cg->hoist.active   = false;
    cg->hoist.values   = NULL;
    cg->hoist.regs     = NULL;
    cg->hoist.count    = 0;
    cg->hoist.capacity = 0;
    cg->hoist.get_names    = NULL;
    cg->hoist.get_indices  = NULL;
    cg->hoist.get_regs     = NULL;
    cg->hoist.get_count    = 0;
    cg->hoist.get_capacity = 0;

    cg->num_cache.values   = NULL;
    cg->num_cache.regs     = NULL;
    cg->num_cache.count    = 0;
    cg->num_cache.capacity = 0;

    cg->str_cache.values   = NULL;
    cg->str_cache.regs     = NULL;
    cg->str_cache.count    = 0;
    cg->str_cache.capacity = 0;

    cg->locals.is_integer  = NULL;                                         // allocated lazily by add_local
    cg->locals.is_number   = NULL;

    return cg;                                                             // return generator
}

// frees all code generator resources
void codegen_destroy(CodeGenerator* cg) {
    if (!cg) return;                                                       // guard against null
    
    for (int i = 0; i < cg->locals.count; i++) {                           // free local names
        free(cg->locals.names[i]);
    }
    free(cg->locals.names);                                                // free names array
    free(cg->locals.registers);                                            // free registers array
    free(cg->locals.is_number);                                            // free numeric-ness flags array
    free(cg->locals.is_integer);                                           // free integer-ness flags array

    free(cg->hoist.values);                                                // free hoist arrays (defensive)
    free(cg->hoist.regs);
    free(cg->hoist.get_names);
    free(cg->hoist.get_indices);
    free(cg->hoist.get_regs);

    free(cg->num_cache.values);                                            // free numeric constant cache
    free(cg->num_cache.regs);

    for (int i = 0; i < cg->str_cache.count; i++) {                        // free string constant cache
        free(cg->str_cache.values[i]);
    }
    free(cg->str_cache.values);
    free(cg->str_cache.regs);

    free(cg->loop_stack.break_jumps);                                      // free break jumps
    free(cg->jump_targets);
    
    for (int i = 0; i < cg->module_count; i++) {                           // free imported modules
        free(cg->imported_modules[i]);
    }
    free(cg->imported_modules);                                            // free modules array
    
    for (int i = 0; i < cg->module_globals_count; i++) {                   // free module globals
        free(cg->module_globals[i]);
    }
    free(cg->module_globals);                                              // free globals array
    
    free(cg);                                                              // free generator
}

// emits a number literal into dest (or fresh temp)
static int codegen_literal_number(CodeGenerator* cg, ASTNode* node, int dest) {
    double val = node->literal_number.number_value;                        // extract value
    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    if (val == (int)val && val >= 0 && val <= 65535) {                     // fits in immediate
        emit(cg, INST(OP_LOAD_NUM_IMM, dest, (int)val, 0), node->line);    // load immediate
    } else {                                                               // large value
        int const_idx = bytecode_add_number_constant(cg->chunk, val);      // add to constant pool
        emit(cg, INST(OP_LOAD_NUM, dest, const_idx, 0), node->line);       // load from pool
    }
    return dest;                                                           // return destination
}

// emits a string literal into dest (or fresh temp), reusing a cached register when possible
static int codegen_literal_string(CodeGenerator* cg, ASTNode* node, int dest) {
    const char* value = node->literal_string.string_value;                 // literal text

    int cached = str_cache_lookup(cg, value);                              // already materialised?
    if (cached >= 0) {
        if (dest < 0 || dest == cached) return cached;                     // reuse cached register
        emit(cg, INST(OP_MOVE, dest, cached, 0), node->line);              // copy into requested dest
        return dest;                                                       // return destination
    }

    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    int const_idx = bytecode_add_string_constant(cg->chunk, value);        // add string constant
    emit(cg, INST(OP_LOAD_CONST, dest, const_idx, 0), node->line);         // load constant
    str_cache_add(cg, value, dest);                                        // cache the destination register
    return dest;                                                           // return destination
}

// emits a none literal into dest (or fresh temp)
static int codegen_literal_none(CodeGenerator* cg, ASTNode* node, int dest) {
    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    emit(cg, INST(OP_LOAD_NONE, dest, 0, 0), node->line);                  // load none directly
    return dest;                                                           // return destination
}

// emits a bool literal into dest (or fresh temp)
static int codegen_literal_bool(CodeGenerator* cg, ASTNode* node, int dest) {
    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    emit(cg, INST(OP_LOAD_BOOL, dest,                                      // load bool
                  node->literal_bool.bool_value ? 1 : 0, 0), node->line);
    return dest;                                                           // return destination
}

// emits an identifier, preferring local variables over globals
static int codegen_identifier(CodeGenerator* cg, ASTNode* node, int dest) {
    const char* name = node->identifier.name;                              // get identifier name
    
    int local_reg = find_local(cg, name);                                  // check local scope
    if (local_reg >= 0) {                                                  // found locally
        if (dest < 0 || dest == local_reg) return local_reg;               // no copy needed
        emit(cg, INST(OP_MOVE, dest, local_reg, 0), node->line);           // copy into requested dest
        return dest;                                                       // return destination
    }
    
    for (int i = 0; i < cg->module_count; i++) {                           // check imported modules
        char full_name[512];                                               // qualified name buffer
        snprintf(full_name, sizeof(full_name), "%s.%s", cg->imported_modules[i], name);
        
        int global_idx = bytecode_get_global(cg->chunk, full_name);        // lookup global
        if (global_idx >= 0) {                                             // found in module
            int reg = dest >= 0 ? dest : alloc_register(cg);               // use hint or fresh
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
            return reg;                                                    // return register
        }
    }
    
    if (cg->current_module && !strchr(name, '.')) {                        // inside module and bare name
        char qualified[512];                                               // buffer for qualified name
        snprintf(qualified, sizeof(qualified), "%s.%s", cg->current_module, name);
        int global_idx = bytecode_get_global(cg->chunk, qualified);        // lookup qualified global
        if (global_idx >= 0) {                                             // found qualified global
            int reg = dest >= 0 ? dest : alloc_register(cg);               // use hint or fresh
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
            return reg;                                                    // return register
        }
    }
    
    int global_idx = bytecode_get_global(cg->chunk, name);                 // lookup in global scope
    if (global_idx >= 0) {                                                 // found globally
        if (cg->current_function == 0) {                                   // top-level scope
            int reg = add_local(cg, name);                                 // add as local cache
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);  // load global into local
            if (dest < 0 || dest == reg) return reg;                       // local is the dest
            emit(cg, INST(OP_MOVE, dest, reg, 0), node->line);             // copy into requested dest
            return dest;                                                   // return destination
        }
        int reg = dest >= 0 ? dest : alloc_register(cg);                   // use hint or fresh
        emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
        return reg;                                                        // return register
    }
    
    if (cg->current_module && !strchr(name, '.')) {                        // inside module and bare name
        char qualified[512];                                               // buffer for qualified name
        snprintf(qualified, sizeof(qualified), "%s.%s", cg->current_module, name);
        global_idx = bytecode_add_global(cg->chunk, qualified);            // create qualified global slot
        if (cg->current_function == 0) {                                   // top-level scope
            int reg = add_local(cg, name);                                 // add as local cache
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
            if (dest < 0 || dest == reg) return reg;                       // local is the dest
            emit(cg, INST(OP_MOVE, dest, reg, 0), node->line);             // copy into requested dest
            return dest;                                                   // return destination
        }
        int reg = dest >= 0 ? dest : alloc_register(cg);                   // use hint or fresh
        emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
        return reg;                                                        // return register
    }
    
    // fallback: create bare global (for non-module code)
    global_idx = bytecode_add_global(cg->chunk, name);                     // add new global
    if (cg->current_function == 0) {                                       // top-level scope
        int reg = add_local(cg, name);                                     // add as local cache
        emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
        if (dest < 0 || dest == reg) return reg;                           // local is the dest
        emit(cg, INST(OP_MOVE, dest, reg, 0), node->line);                 // copy into requested dest
        return dest;                                                       // return destination
    }
    int reg = dest >= 0 ? dest : alloc_register(cg);                       // use hint or fresh
    emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
    return reg;                                                            // return register
}

// emits a function call, resolving builtins and user functions by name
static int codegen_call(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    ASTNodeList* args_list = node->call.arguments;                      // arguments list
    int arg_count = args_list->count;                                   // number of arguments
    int* arg_regs = NULL;                                               // argument registers
    
    // result register must be allocated BEFORE args so args land above it
    int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);   // result destination
    
    if (arg_count > 0) {                                                // has arguments
        arg_regs = (int*)malloc(sizeof(int) * arg_count);               // allocate arg array
        for (int i = 0; i < arg_count; i++) {                           // evaluate each arg
            arg_regs[i] = codegen_expression(cg, args_list->nodes[i]);  // store register
        }
    }
    
    char func_name[256] = "";                                           // function name buffer
    ASTNode* callee = node->call.callee;                                // callee expression
    if (callee->type == AST_IDENTIFIER) {                               // simple identifier
        strcpy(func_name, callee->identifier.name);                     // copy name
    } else if (callee->type == AST_INDEX_ACCESS) {                      // dotted access
        ASTNode* parts[32];                                             // path parts
        int part_count = 0;                                             // number of parts
        ASTNode* current = callee;                                      // current node
        
        while (current->type == AST_INDEX_ACCESS) {                     // traverse access chain
            if (current->access.member->type == AST_IDENTIFIER) {       // valid member
                parts[part_count++] = current->access.member;           // add part
            } else {
                part_count = 0;                                         // invalid
                break;                                                  // exit loop
            }
            current = current->access.object;                           // move to object
        }
        
        if (part_count > 0 && current->type == AST_IDENTIFIER) {        // valid path
            parts[part_count++] = current;                              // add last part
            
            func_name[0] = '\0';                                        // clear name
            for (int i = part_count - 1; i >= 0; i--) {                 // build from right
                if (i < part_count - 1) {                               // not first
                    strcat(func_name, ".");                             // add dot separator
                }
                strcat(func_name, parts[i]->identifier.name);           // add part name
            }
        }
    }

    bool is_builtin = false;  // check if this is a known builtin
    
    if (strcmp(func_name, "number") == 0 ||
        strcmp(func_name, "string") == 0 ||
        strcmp(func_name, "type") == 0) {
        is_builtin = true;
    }
    else if (strncmp(func_name, "os.", 3) == 0 ||
             strncmp(func_name, "sys.", 4) == 0 ||
             strncmp(func_name, "math.", 5) == 0 ||
             strncmp(func_name, "string.", 7) == 0 ||
             strncmp(func_name, "table.", 6) == 0 ||
             strncmp(func_name, "random.", 7) == 0 ||
             strncmp(func_name, "json.", 5) == 0 ||
             strncmp(func_name, "xml.", 4) == 0 ||
             strncmp(func_name, "csv.", 4) == 0 ||
             strncmp(func_name, "base.", 5) == 0 ||
             strncmp(func_name, "regex.", 6) == 0 ||
             strncmp(func_name, "crypto.", 7) == 0 ||
             strncmp(func_name, "zip.", 4) == 0) {
        is_builtin = true;
    }

    if (is_builtin) {                                                         // built-in function
        for (int i = 0; i < arg_count; i++) {                                 // push args
            emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
        }
        int name_idx = bytecode_add_string_constant(cg->chunk, func_name);    // add name constant
        int op = cg->current_call_is_awaited ? OP_ASYNC_CALL_BUILTIN : OP_CALL_BUILTIN;
        emit(cg, INST(op, result_reg, name_idx, arg_count), node->line);      // call builtin
    } else {                                                                           // user function
        int func_idx = -1;                                                             // function index
        
        for (int i = 0; i < cg->chunk->func_count; i++) {                              // search function table
            if (strcmp(cg->chunk->functions[i].name, func_name) == 0) {                // exact name match
                func_idx = i;                                                          // store function index
                break;                                                                 // exit loop
            }
        }
        
        if (func_idx < 0 && func_name[0] != '\0' && !strchr(func_name, '.') && cg->current_module) {
            char qualified[512];                                                             // buffer for qualified name
            snprintf(qualified, sizeof(qualified), "%s.%s", cg->current_module, func_name);  // build qualified name
            for (int i = 0; i < cg->chunk->func_count; i++) {                                // search function table
                if (strcmp(cg->chunk->functions[i].name, qualified) == 0) {                  // qualified name match
                    func_idx = i;                                                            // store function index
                    break;                                                                   // exit loop
                }
            }
        }
        
        if (func_idx < 0 && func_name[0] != '\0') {                                    // still not found
            const char* last_dot = strrchr(func_name, '.');                            // find last dot
            if (last_dot) {                                                            // dotted name
                const char* short_name = last_dot + 1;                                 // extract short name
                for (int i = 0; i < cg->chunk->func_count; i++) {                      // search function table
                    if (strcmp(cg->chunk->functions[i].name, short_name) == 0) {       // short name match
                        func_idx = i;                                                  // store function index
                        break;                                                         // exit loop
                    }
                }
            }
        }

        if (func_idx >= 0 && cg->chunk->functions[func_idx].is_async) {   // async call: defer body
            for (int i = 0; i < arg_count; i++) {                         // push captured args
                emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
            }
            emit(cg, INST(OP_ASYNC_CALL, result_reg, func_idx, arg_count), node->line);  // return pending future
            if (arg_regs) {                                               // free arg registers
                for (int i = 0; i < arg_count; i++) free_register(cg, arg_regs[i]);
                free(arg_regs);                                           // free arg array
            }
            return result_reg;                                            // caller gets a future
        }
        
        if (func_idx >= 0) {                                                   // function found
            if (arg_count == 0) {                                              // zero args
                emit(cg, INST(OP_CALL_0, result_reg, func_idx, 0), node->line);
            } else if (arg_count == 1) {                                       // one arg
                emit(cg, INST(OP_CALL_1, result_reg, func_idx, arg_regs[0]), node->line);
            } else if (arg_count == 2) {                                       // two args
                if (arg_regs[1] != arg_regs[0] + 1) {                          // not already contiguous
                    int t0 = alloc_register(cg);                               // fresh slot for arg0
                    int t1 = alloc_register(cg);                               // fresh slot for arg1
                    emit(cg, INST(OP_MOVE, t0, arg_regs[0], 0), node->line);   // copy arg0
                    emit(cg, INST(OP_MOVE, t1, arg_regs[1], 0), node->line);   // copy arg1
                    free_register(cg, arg_regs[1]);                            // free old arg1
                    free_register(cg, arg_regs[0]);                            // free old arg0
                    arg_regs[0] = t0;                                          // point to fresh slot
                    arg_regs[1] = t1;                                          // point to fresh slot
                }
                emit(cg, INST(OP_CALL_2, result_reg, func_idx, arg_regs[0]), node->line);
            } else {                                                           // many args
                for (int i = 0; i < arg_count; i++) {                          // push all args
                    emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
                }
                emit(cg, INST(OP_CALL, result_reg, func_idx, arg_count), node->line);
            }
        } else {                                                              // function not found
            for (int i = 0; i < arg_count; i++) {                             // push args
                emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
            }
            int name_idx = bytecode_add_string_constant(cg->chunk, func_name);             // add name constant
            emit(cg, INST(OP_CALL_BUILTIN, result_reg, name_idx, arg_count), node->line);  // fallback to builtin
        }
    }
    
    if (arg_regs) {                                                           // free arg registers
        for (int i = 0; i < arg_count; i++) {
            free_register(cg, arg_regs[i]);
        }
        free(arg_regs);                                                       // free arg array
    }
    return result_reg;                                                        // return result register
}

// emits string interpolation by concatenating parts, first part goes into dest
static int codegen_string_interp(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    if (node->string_interp.parts->count == 0) {                           // empty interpolation
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);         // use hint or fresh
        int empty_idx = bytecode_add_string_constant(cg->chunk, "");       // empty string constant
        emit(cg, INST(OP_LOAD_CONST, reg, empty_idx, 0), node->line);      // load empty
        return reg;                                                        // return register
    }

    int result_reg;                                                        // result register
    if (dest_hint >= 0) {                                                  // write first part into dest
        codegen_expression_into(cg, node->string_interp.parts->nodes[0], dest_hint);
        result_reg = dest_hint;                                            // result lives in dest
    } else {                                                               // no hint: fresh temp
        result_reg = alloc_register(cg);                                   // allocate a fresh destination
        codegen_expression_into(cg, node->string_interp.parts->nodes[0], result_reg);  // force first part into it
    }

    for (int i = 1; i < node->string_interp.parts->count; i++) {           // remaining parts
        ASTNode* part = node->string_interp.parts->nodes[i];               // current part
        int part_reg = codegen_expression(cg, part);                       // fresh temp for part
        emit(cg, INST(OP_CONCAT, result_reg, result_reg, part_reg),        // in-place concatenation
             node->line);
        free_register(cg, part_reg);                                       // free part temp
    }
    return result_reg;                                                     // return final result
}

// emits for loops, supporting both range loops and table iteration
static void codegen_for_statement(CodeGenerator* cg, ASTNode* node) {
    int prev_break_count = cg->loop_stack.break_count;                             // save break count
    int prev_continue_addr = cg->loop_stack.continue_addr;                         // save continue addr
    bool prev_is_fast = cg->loop_stack.is_fast;                                    // save fast flag
    cg->loop_depth++;                                                              // disable copy propagation inside loops
    
    cg->loop_stack.is_fast = (node->for_stmt.var_name != NULL);                    // set fast flag

    if (node->for_stmt.var_name) {                                                 // named variable loop
        if (node->for_stmt.end == NULL && !node->for_stmt.condition) {             // table iteration
            int saved_locals_count = cg->locals.count;                              // capture for-scope boundary
            int saved_next_register = cg->next_register;
            cg->for_scope_depth++;                                                  // enter for-scope

            LocalNumSnap table_entry = snap_numbers(cg);                           // snapshot before body
            int table_reg = codegen_expression(cg, node->for_stmt.start);          // evaluate table
            int var_reg = add_local(cg, node->for_stmt.var_name);                  // add loop variable
            
            emit(cg, INST(OP_TABLE_ITER_INIT, table_reg, 0, 0), node->line);       // init iterator
            
            int loop_start = bytecode_current_offset(cg->chunk);                   // loop start
            cg->loop_stack.continue_addr = loop_start;                             // set continue
            
            int iter_next_instr = bytecode_current_offset(cg->chunk);              // iter instruction
            emit(cg, INST(OP_TABLE_ITER_NEXT, var_reg, 0, 0), node->line);         // get next item
            
            codegen_block(cg, node->for_stmt.body);                                // emit body
            restore_numbers(cg, table_entry);                                      // body may reassign: reset flags
            
            emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);                 // jump back
            
            int exit_addr = bytecode_current_offset(cg->chunk);                    // exit address
            cg->chunk->code[iter_next_instr].operands[2] = exit_addr;              // patch exit
            mark_jump_target(cg, exit_addr);                                       // record in bitset
            
            cg->for_scope_depth--;                                                  // exit for-scope
            for (int i = saved_locals_count; i < cg->locals.count; i++) {           // drop loop var + body locals
                free(cg->locals.names[i]);
            }
            cg->locals.count = saved_locals_count;
            cg->next_register = saved_next_register;

            free_snap(table_entry);                                                // release snapshot
            
            for (int i = prev_break_count; i < cg->loop_stack.break_count; i++) {  // patch breaks
                PATCH_JUMP(cg, cg->loop_stack.break_jumps[i], exit_addr);
            }
        } else {                                                                    // numeric range loop
            int saved_locals_count = cg->locals.count;                              // capture for-scope boundary
            int saved_next_register = cg->next_register;
            cg->for_scope_depth++;                                                  // enter for-scope

            int var_reg = add_local(cg, node->for_stmt.var_name);                   // allocate loop variable first
            codegen_expression_into(cg, node->for_stmt.start, var_reg);             // write start directly into var_reg
            int end_reg = codegen_expression(cg, node->for_stmt.end);               // evaluate end (fresh temp)
            int step_reg;                                                           // step register
            if (node->for_stmt.step) {                                              // custom step
                step_reg = codegen_expression(cg, node->for_stmt.step);             // evaluate step
            } else {                                                                // default step = 1
                step_reg = alloc_register(cg);                                      // allocate register
                emit(cg, INST(OP_LOAD_NUM_IMM, step_reg, 1, 0), node->line);        // load 1 immediate
            }

            // save current hoist scope and install a fresh one for this loop
            bool prev_hoist_active = cg->hoist.active;
            double* prev_hoist_values = cg->hoist.values;
            int* prev_hoist_regs = cg->hoist.regs;
            int prev_hoist_count = cg->hoist.count;
            int prev_hoist_capacity = cg->hoist.capacity;
            const char** prev_get_names    = cg->hoist.get_names;
            double*      prev_get_indices  = cg->hoist.get_indices;
            int*         prev_get_regs     = cg->hoist.get_regs;
            int          prev_get_count    = cg->hoist.get_count;
            int          prev_get_capacity = cg->hoist.get_capacity;

            cg->hoist.active = false;                                               // fresh scope
            cg->hoist.values = NULL;
            cg->hoist.regs = NULL;
            cg->hoist.count = 0;
            cg->hoist.capacity = 0;
            cg->hoist.get_names    = NULL;
            cg->hoist.get_indices  = NULL;
            cg->hoist.get_regs     = NULL;
            cg->hoist.get_count    = 0;
            cg->hoist.get_capacity = 0;

            collect_hoistable_numbers(cg, node->for_stmt.body);                     // scan body for constants

            for (int i = 0; i < cg->hoist.count; i++) {                             // allocate a register per constant
                int reg = alloc_register(cg);
                cg->hoist.regs[i] = reg;
                double v = cg->hoist.values[i];
                if (v == (int)v && v >= 0 && v <= 65535) {                          // fits in immediate
                    emit(cg, INST(OP_LOAD_NUM_IMM, reg, (int)v, 0), node->line);    // load immediate once
                } else {                                                            // full double
                    int ci = bytecode_add_number_constant(cg->chunk, v);            // add to constant pool
                    emit(cg, INST(OP_LOAD_NUM, reg, ci, 0), node->line);            // load once
                }
            }
            if (cg->hoist.count > 0) cg->hoist.active = true;                       // enable reuse in the body

            // licm: hoist loop-invariant t[CONST] reads into their own registers
            collect_hoistable_table_gets(cg, node->for_stmt.body);
            for (int i = 0; i < cg->hoist.get_count; i++) {
                int tslot = find_local_slot(cg, cg->hoist.get_names[i]);
                int t_reg = cg->locals.registers[tslot];
                int idx   = (int)cg->hoist.get_indices[i];
                int dst   = alloc_register(cg);
                emit(cg, INST(OP_TABLE_GET_INT, dst, t_reg, idx), node->line);
                cg->hoist.get_regs[i] = dst;
            }
            if (cg->hoist.get_count > 0) cg->hoist.active = true;

            emit(cg, INST(OP_FOR_INIT, var_reg, end_reg, step_reg), node->line);    // init for (no MOVE needed)

            int loop_var_slot = find_local_slot(cg, node->for_stmt.var_name);       // slot of loop variable
            if (loop_var_slot >= 0) {
                cg->locals.is_number[loop_var_slot] = true;                        // FOR_NEXT keeps it numeric
                bool int_start = node->for_stmt.start                              // start whole-number?
                                 ? is_integer_expression(cg, node->for_stmt.start) // uses tracked flags
                                 : false;
                bool int_step = node->for_stmt.step                                // custom step?
                                ? is_integer_expression(cg, node->for_stmt.step)   // same predicate
                                : true;                                            // default step 1 is whole
                if (int_start && int_step) {
                    cg->locals.is_integer[loop_var_slot] = true;                   // whole-number loop var
                }
            }

            LocalNumSnap loop_entry = snap_numbers(cg);                             // snapshot before body

            int loop_start = bytecode_current_offset(cg->chunk);                    // loop start
            cg->loop_stack.continue_addr = loop_start;                              // continue re-enters the test

            int for_next_instr = bytecode_current_offset(cg->chunk);                // first test pc
            emit(cg, INST(OP_FOR_NEXT, var_reg, 0, 0), node->line);                 // first test; fail -> exit

            int body_start = bytecode_current_offset(cg->chunk);                    // body start pc

            int prev_loop_floor = cg->register_floor;                               // save floor
            if (cg->hoist.count > 0 || cg->hoist.get_count > 0) {                   // pin hoisted regs
                int highest = 0;                                                    // highest hoisted reg
                for (int i = 0; i < cg->hoist.count; i++) {
                    if (cg->hoist.regs[i] > highest) highest = cg->hoist.regs[i];
                }
                for (int i = 0; i < cg->hoist.get_count; i++) {
                    if (cg->hoist.get_regs[i] > highest) highest = cg->hoist.get_regs[i];
                }
                cg->register_floor = highest + 1;                                   // body temps start above
            }

            codegen_block(cg, node->for_stmt.body);                                 // emit body
            cg->register_floor = prev_loop_floor;                                   // restore floor after body

            cg->for_scope_depth--;                                                  // exit for-scope
            for (int i = saved_locals_count; i < cg->locals.count; i++) {           // drop loop var + body locals
                free(cg->locals.names[i]);
            }
            cg->locals.count = saved_locals_count;
            cg->next_register = saved_next_register;                                // reclaim var/end/step/hoisted at once

            LocalNumSnap loop_exit = snap_numbers(cg);                              // snapshot after body

            int merge_n = loop_entry.count < loop_exit.count ? loop_entry.count : loop_exit.count;
            for (int i = 0; i < merge_n; i++) {                                     // intersect entry and exit
                cg->locals.is_number[i] = loop_entry.flags[i] && loop_exit.flags[i];
            }
            free_snap(loop_entry);                                                  // release snapshots
            free_snap(loop_exit);

            // loop-inverted back edge: jumps to body_start on success, falls through on exit
            emit(cg, INST(OP_FOR_NEXT_LOOP, var_reg, body_start, 0), node->line);

            int exit_addr = bytecode_current_offset(cg->chunk);                     // exit address
            cg->chunk->code[for_next_instr].operands[1] = exit_addr;                // patch first test's exit
            mark_jump_target(cg, exit_addr);                                        // record in bitset

            free(cg->hoist.values);                                                 // release hoist arrays
            free(cg->hoist.regs);
            free(cg->hoist.get_names);
            free(cg->hoist.get_indices);
            free(cg->hoist.get_regs);

            cg->hoist.active = prev_hoist_active;                                   // restore previous scope
            cg->hoist.values = prev_hoist_values;
            cg->hoist.regs = prev_hoist_regs;
            cg->hoist.count = prev_hoist_count;
            cg->hoist.capacity = prev_hoist_capacity;
            cg->hoist.get_names    = prev_get_names;
            cg->hoist.get_indices  = prev_get_indices;
            cg->hoist.get_regs     = prev_get_regs;
            cg->hoist.get_count    = prev_get_count;
            cg->hoist.get_capacity = prev_get_capacity;
        }
        
        int exit_addr = bytecode_current_offset(cg->chunk);                         // exit address
        for (int i = prev_break_count; i < cg->loop_stack.break_count; i++) {       // patch all breaks
            PATCH_JUMP(cg, cg->loop_stack.break_jumps[i], exit_addr);
        }
    } else {                                                                        // no variable, condition loop
        int saved_locals_count = cg->locals.count;                                  // capture for-scope boundary
        int saved_next_register = cg->next_register;
        cg->for_scope_depth++;                                                      // enter for-scope

        ASTNode* condition = node->for_stmt.condition;                              // condition
        int left_reg = -1;                                                          // left operand reg
        int right_reg = -1;                                                         // right operand reg
        bool optimized = false;                                                     // optimized flag
        bool has_imm = false;                                                       // true when an IMM variant exists
        bool right_hoisted = false;                                                 // hoisted right
        bool imm_jump_ready = false;                                                // right folds to small int
        double imm_val_for_cond = 0;                                                // folded immediate value
        Opcode jump_op = OP_JUMP;                                                   // jump opcode
        Opcode jump_op_imm = OP_JUMP;                                               // jump opcode with immediate

        LocalNumSnap cond_entry = snap_numbers(cg);                                 // snapshot before condition

        if (condition) {                                                            // has condition
            if (condition->type == AST_BINARY) {                                    // binary condition
                ApexTokenType op = condition->binary.op;                            // operator
                bool both_numbers = is_number_expression(cg, condition->binary.left) &&  // check both operands known numeric
                                    is_number_expression(cg, condition->binary.right);
                
                switch (op) {                                                       // map to jump op
                    case TOKEN_LESS:          jump_op = OP_JUMP_IF_GTE; jump_op_imm = OP_JUMP_IF_GTE_IMM; has_imm = true; optimized = true; break;
                    case TOKEN_LESS_EQUAL:    jump_op = OP_JUMP_IF_GT;  jump_op_imm = OP_JUMP_IF_GT_IMM;  has_imm = true; optimized = true; break;
                    case TOKEN_GREATER:       jump_op = OP_JUMP_IF_LTE; jump_op_imm = OP_JUMP_IF_LTE_IMM; has_imm = true; optimized = true; break;
                    case TOKEN_GREATER_EQUAL: jump_op = OP_JUMP_IF_LT;  jump_op_imm = OP_JUMP_IF_LT_IMM;  has_imm = true; optimized = true; break;
                    case TOKEN_EQUAL_EQUAL:   jump_op = both_numbers ? OP_JUMP_IF_NEQ_NUM : OP_JUMP_IF_NEQ; jump_op_imm = OP_JUMP_IF_NEQ_IMM; has_imm = true; optimized = true; break;
                    case TOKEN_NOT_EQUAL:     jump_op = both_numbers ? OP_JUMP_IF_EQ_NUM : OP_JUMP_IF_EQ;   jump_op_imm = OP_JUMP_IF_EQ_IMM;   has_imm = true; optimized = true; break;
                    default: break;
                }
                if (has_imm && try_fold_number(condition->binary.right, &imm_val_for_cond) &&
                    imm_val_for_cond == (int)imm_val_for_cond &&
                    imm_val_for_cond >= 0 && imm_val_for_cond <= 65535) {
                    imm_jump_ready = true;
                }
                if (optimized && !imm_jump_ready) {                                 // can optimize and not already folded
                    ASTNode* right_node = condition->binary.right;                  // right side
                    if (right_node->type == AST_LITERAL_NUMBER ||                   // constant right
                        right_node->type == AST_LITERAL_STRING ||
                        right_node->type == AST_LITERAL_BOOL) {
                        right_reg = codegen_expression(cg, right_node);             // evaluate
                        right_hoisted = true;                                       // mark hoisted
                    }
                }
            }
        }

        int loop_start = bytecode_current_offset(cg->chunk);                        // loop start
        cg->loop_stack.continue_addr = loop_start;                                  // set continue
        
        int jump_to_end = -1;                                                       // jump to end instr
        if (condition) {                                                            // has condition
            if (optimized) {                                                        // optimized condition
                left_reg = codegen_expression(cg, condition->binary.left);          // evaluate left
                if (imm_jump_ready) {                                               // right folds to a small int
                    jump_to_end = emit(cg, INST(jump_op_imm, 0, left_reg, (int)imm_val_for_cond), node->line);
                } else {                                                            // register-register compare
                    if (!right_hoisted) {                                           // not hoisted
                        right_reg = codegen_expression(cg, condition->binary.right);  // evaluate right
                    }
                    jump_to_end = emit(cg, INST(jump_op, 0, left_reg, right_reg), node->line);  // emit jump
                    if (!right_hoisted) free_register(cg, right_reg);               // free right
                }
                free_register(cg, left_reg);                                        // free left
            } else {                                                                // normal condition
                int cond_reg = codegen_expression(cg, condition);                   // evaluate condition
                jump_to_end = bytecode_current_offset(cg->chunk);                   // jump address
                emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);       // jump if false
                free_register(cg, cond_reg);                                        // free condition
            }
        }

        LocalNumSnap loop_entry = snap_numbers(cg);                                 // snapshot before body
        int prev_floor = cg->register_floor;                                        // save floor
        if (right_hoisted) {                                                        // right_reg must survive
            int new_floor = right_reg + 1;                                          // pin it above body temps
            if (new_floor < prev_floor) new_floor = prev_floor;                     // never lower an outer floor
            cg->register_floor = new_floor;
        }
        codegen_block(cg, node->for_stmt.body);                                     // emit body
        LocalNumSnap loop_exit = snap_numbers(cg);                                  // snapshot after body

        cg->for_scope_depth--;                                                      // exit for-scope
        for (int i = saved_locals_count; i < cg->locals.count; i++) {               // drop body locals
            free(cg->locals.names[i]);
        }
        cg->locals.count = saved_locals_count;
        cg->next_register = saved_next_register;
        
        int merge_n = loop_entry.count < loop_exit.count ? loop_entry.count : loop_exit.count;
        for (int i = 0; i < merge_n; i++) {                                         // intersect entry and exit
            cg->locals.is_number[i] = loop_entry.flags[i] && loop_exit.flags[i];
        }
        free_snap(loop_entry);                                                      // release snapshots
        free_snap(loop_exit);
        free_snap(cond_entry);
        
        cg->register_floor = prev_floor;                                            // restore floor
        emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);                      // jump back
        
        int end_addr = bytecode_current_offset(cg->chunk);                          // end address
        if (jump_to_end >= 0)                                                       // patch jump
            PATCH_JUMP(cg, jump_to_end, end_addr);
            
        for (int i = 0; i < cg->loop_stack.break_count; i++)                        // patch breaks
            PATCH_JUMP(cg, cg->loop_stack.break_jumps[i], end_addr);
    }

    cg->loop_stack.break_count = prev_break_count;                                  // restore break count
    cg->loop_stack.continue_addr = prev_continue_addr;                              // restore continue
    cg->loop_stack.is_fast = prev_is_fast;                                          // restore fast flag
    cg->loop_depth--;                                                               // re-enable copy propagation
}

// checks if an expression is known to produce a number at this point in emission
static bool is_number_expression(CodeGenerator* cg, ASTNode* node) {
    if (!node) return false;                                                   // null guard
    switch (node->type) {
        case AST_LITERAL_NUMBER:
            return true;                                                       // literal is intrinsically a number
        case AST_BINARY:
            return is_arithmetic_op(node->binary.op);                          // arithmetic yields number by definition
        case AST_UNARY:
            return node->unary.op == TOKEN_MINUS;                              // unary minus is always a number
        case AST_IDENTIFIER: {
            int slot = find_local_slot(cg, node->identifier.name);             // look up local slot by name
            if (slot < 0) return false;                                        // globals/unknown: cannot prove locally
            return cg->locals.is_number[slot];                                 // use tracked numeric-ness
        }
        case AST_ASSIGN:
        case AST_VAR_DECL:
            return is_number_expression(cg, node->var_assign.value);           // type of assignment is type of RHS
        case AST_TERNARY:
            return is_number_expression(cg, node->ternary.true_expr) &&        // both branches must be numbers
                   is_number_expression(cg, node->ternary.false_expr);
        default:
            return false;                                                      // strings, tables, calls, index: unknown
    }
}

// checks if an expression is provably a whole number at this point in emission
static bool is_integer_expression(CodeGenerator* cg, ASTNode* node) {
    if (!node) return false;                                                   // null guard
    switch (node->type) {
        case AST_LITERAL_NUMBER: {
            double v = node->literal_number.number_value;                      // literal value
            return v == (double)(long long)v;                                  // true when whole
        }
        case AST_UNARY:
            return node->unary.op == TOKEN_MINUS &&                            // -x preserves integerness
                   is_integer_expression(cg, node->unary.operand);
        case AST_BINARY: {
            ApexTokenType op = node->binary.op;                                // operator
            if (op == TOKEN_PLUS || op == TOKEN_MINUS ||                       // int op int = int
                op == TOKEN_STAR || op == TOKEN_PERCENT) {                     // (division may yield fraction)
                return is_integer_expression(cg, node->binary.left) &&
                       is_integer_expression(cg, node->binary.right);
            }
            return false;                                                      // division, comparisons: unknown
        }
        case AST_IDENTIFIER: {
            int slot = find_local_slot(cg, node->identifier.name);             // look up local slot
            if (slot < 0) return false;                                        // globals/unknown
            return cg->locals.is_integer[slot];                                // use tracked integer-ness
        }
        case AST_ASSIGN:
        case AST_VAR_DECL:
            return is_integer_expression(cg, node->var_assign.value);          // propagate RHS
        default:
            return false;                                                      // calls, tables, etc.
    }
}

// syntactic equality for the subset of nodes that appear in identities
static bool ast_same_expr(ASTNode* a, ASTNode* b) {
    if (a == b) return true;                                    // same pointer
    if (!a || !b) return false;                                 // one is null
    if (a->type != b->type) return false;                       // different kinds
    switch (a->type) {
        case AST_IDENTIFIER:
            return strcmp(a->identifier.name, b->identifier.name) == 0;
        case AST_LITERAL_NUMBER:
            return a->literal_number.number_value == b->literal_number.number_value;
        case AST_LITERAL_STRING:
            return strcmp(a->literal_string.string_value,
                          b->literal_string.string_value) == 0;
        case AST_LITERAL_BOOL:
            return a->literal_bool.bool_value == b->literal_bool.bool_value;
        case AST_LITERAL_NONE:
            return true;
        default:
            return false;                                       // deeper shapes: give up
    }
}

// checks whether `node` is a two-part interpolation "literal{expr}";
// returns the constant pool index of the literal prefix, or -1.
// does not emit any code and does not allocate registers.
static int key_str_prefix_idx(CodeGenerator* cg, ASTNode* node) {
    if (!node || node->type != AST_STRING_INTERP) return -1;                 // not an interpolation
    if (node->string_interp.parts->count != 2) return -1;                    // need exactly two parts
    if (node->string_interp.parts->nodes[0]->type != AST_LITERAL_STRING)
        return -1;                                                           // left must be a literal
    const char* prefix = node->string_interp.parts->nodes[0]
                             ->literal_string.string_value;                  // prefix text
    int idx = bytecode_add_string_constant(cg->chunk, prefix);               // pool index
    if (idx < 0 || idx > 0xFFFF) return -1;                                  // must fit packed op2
    return idx;                                                              // ready to fuse
}

// main expression dispatcher with dest_hint contract
static int codegen_expression_into(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    if (!node) {                                                                     // null node
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                   // use hint or fresh
        emit(cg, INST(OP_LOAD_NONE, reg, 0, 0), 0);                                  // load none
        return reg;                                                                  // return register
    }

    double nv;                                                                       // folded numeric value
    if (try_fold_number(node, &nv)) {                                                // constant subtree?
        if (cg->hoist.active) {                                                      // inside a loop with hoisted constants
            for (int i = 0; i < cg->hoist.count; i++) {                              // look up folded value
                if (cg->hoist.values[i] == nv) {                                     // matches a hoisted constant
                    int hreg = cg->hoist.regs[i];                                    // hoisted register
                    if (dest_hint < 0 || dest_hint == hreg) return hreg;             // reuse directly
                    emit(cg, INST(OP_MOVE, dest_hint, hreg, 0), node->line);         // copy into hint
                    return dest_hint;
                }
            }
        }
        int cached = num_cache_lookup(cg, nv);                                       // check per-function cache
        if (cached >= 0) {                                                           // already materialized earlier
            if (dest_hint < 0 || dest_hint == cached) return cached;                 // reuse cached register
            emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);               // copy into hint
            return dest_hint;
        }
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                   // use hint or fresh
        if (nv == (int)nv && nv >= 0 && nv <= 65535) {                               // fits in immediate
            emit(cg, INST(OP_LOAD_NUM_IMM, reg, (int)nv, 0), node->line);            // load immediate
        } else {                                                                     // large or fractional
            int const_idx = bytecode_add_number_constant(cg->chunk, nv);             // add to constant pool
            emit(cg, INST(OP_LOAD_NUM, reg, const_idx, 0), node->line);              // load from pool
        }
        if (dest_hint < 0) num_cache_add(cg, nv, reg);                               // cache self-allocated reg
        return reg;                                                                  // single load replaces subtree
    }

    bool bv;                                                                         // folded boolean value
    if (try_fold_bool(node, &bv)) {                                                  // constant boolean?
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                   // use hint or fresh
        emit(cg, INST(OP_LOAD_BOOL, reg, bv ? 1 : 0, 0), node->line);                // load bool directly
        return reg;                                                                  // single load replaces subtree
    }

    switch (node->type) {                                                            // dispatch by type
        case AST_ASSIGN:                                                             // assignment
            return codegen_assign_expr(cg, node, dest_hint);

        case AST_LITERAL_NUMBER:                                                     // number literal
            return codegen_literal_number(cg, node, dest_hint);

        case AST_LITERAL_STRING:                                                     // string literal
            return codegen_literal_string(cg, node, dest_hint);

        case AST_LITERAL_NONE:                                                       // none literal
            return codegen_literal_none(cg, node, dest_hint);

        case AST_LITERAL_BOOL:                                                       // boolean literal
            return codegen_literal_bool(cg, node, dest_hint);

        case AST_IDENTIFIER:                                                         // identifier
            return codegen_identifier(cg, node, dest_hint);

        case AST_BINARY: {                                                           // binary operation
            if (node->binary.op == TOKEN_AND || node->binary.op == TOKEN_OR) {       // logical and/or
                int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);    // result dest

                int left_reg = codegen_expression_into(cg, node->binary.left, result_reg);  // evaluate left into dest
                if (left_reg != result_reg) {                                        // left ignored hint
                    emit(cg, INST(OP_MOVE, result_reg, left_reg, -1), node->line);
                    free_register(cg, left_reg);                                     // free left temp
                }

                int jump_idx;                                                        // jump to skip right
                if (node->binary.op == TOKEN_AND) {                                  // AND
                    jump_idx = emit(cg, INST(OP_JUMP_IF_FALSE, 0, result_reg, -1), node->line);  // if false skip
                } else {                                                             // OR
                    int not_reg = alloc_register(cg);                                // inverted left register
                    emit(cg, INST(OP_NOT, not_reg, result_reg, 0), node->line);      // not left
                    jump_idx = emit(cg, INST(OP_JUMP_IF_FALSE, 0, not_reg, -1), node->line);  // if truthy skip
                    free_register(cg, not_reg);                                      // free inverted
                }

                int right_reg = codegen_expression_into(cg, node->binary.right, result_reg);  // evaluate right into dest
                if (right_reg != result_reg) {                                       // right ignored hint
                    emit(cg, INST(OP_MOVE, result_reg, right_reg, -1), node->line);
                    free_register(cg, right_reg);                                    // free right temp
                }

                PATCH_JUMP(cg, jump_idx, bytecode_current_offset(cg->chunk));                // patch skip

                return result_reg;                                                   // return result
            }

            // algebraic identities (run AFTER constant folding)
            {
                ApexTokenType bop = node->binary.op;
                double lv, rv;
                bool lconst = try_fold_number(node->binary.left,  &lv);
                bool rconst = try_fold_number(node->binary.right, &rv);

                switch (bop) {
                    // x + 0 = x, 0 + x = x  (holds for ALL values incl. none)
                    case TOKEN_PLUS:
                        if (rconst && rv == 0.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        if (lconst && lv == 0.0)
                            return codegen_expression_into(cg, node->binary.right, dest_hint);
                        break;

                    // x - 0 = x  (holds for ALL values incl. none)
                    case TOKEN_MINUS:
                        if (rconst && rv == 0.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        // 0 - x = -x  (safe only when x is a number)
                        if (lconst && lv == 0.0 &&
                            is_number_expression(cg, node->binary.right)) {
                            int op_reg = codegen_expression(cg, node->binary.right);
                            int res    = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_NEG, res, op_reg, 0), node->line);
                            free_register(cg, op_reg);
                            return res;
                        }
                        // x - x = 0  (safe only when x is a number)
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_NUM_IMM, res, 0, 0), node->line);
                            return res;
                        }
                        break;

                    // x * 1 = x, 1 * x = x  (holds for ALL values incl. none)
                    case TOKEN_STAR:
                        if (rconst && rv == 1.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        if (lconst && lv == 1.0)
                            return codegen_expression_into(cg, node->binary.right, dest_hint);
                        // x * 0 = 0, 0 * x = 0  (safe only when x is a number)
                        if (rconst && rv == 0.0 &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_NUM_IMM, res, 0, 0), node->line);
                            return res;
                        }
                        if (lconst && lv == 0.0 &&
                            is_number_expression(cg, node->binary.right)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_NUM_IMM, res, 0, 0), node->line);
                            return res;
                        }
                        break;

                    // x / 1 = x  (holds for ALL values incl. none)
                    case TOKEN_SLASH:
                        if (rconst && rv == 1.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        break;

                    // x == x = true, x != x = false, x <= x = true, x >= x = true,
                    // x < x = false, x > x = false   (safe only when x is a number)
                    case TOKEN_EQUAL_EQUAL:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 1, 0), node->line);
                            return res;
                        }
                        break;
                    case TOKEN_NOT_EQUAL:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 0, 0), node->line);
                            return res;
                        }
                        break;
                    case TOKEN_LESS_EQUAL:
                    case TOKEN_GREATER_EQUAL:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 1, 0), node->line);
                            return res;
                        }
                        break;
                    case TOKEN_LESS:
                    case TOKEN_GREATER:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 0, 0), node->line);
                            return res;
                        }
                        break;

                    default: break;
                }
            }
            
            // try IMM-optimized arithmetic when the right operand folds to a numeric constant
            // that fits the immediate field (integer 0..65535); the fold covers literals,
            // negative literals via unary minus, and nested constant arithmetic like (1 + 1)
            Opcode imm_op = OP_MOVE;                                             // sentinel; overwritten on match
            bool has_imm = false;                                                // true when op maps to an IMM variant
            switch (node->binary.op) {                                           // map arithmetic op to IMM variant
                case TOKEN_PLUS:    imm_op = OP_ADD_IMM; has_imm = true; break;
                case TOKEN_MINUS:   imm_op = OP_SUB_IMM; has_imm = true; break;
                case TOKEN_STAR:    imm_op = OP_MUL_IMM; has_imm = true; break;
                case TOKEN_SLASH:   imm_op = OP_DIV_IMM; has_imm = true; break;
                case TOKEN_PERCENT: imm_op = OP_MOD_IMM; has_imm = true; break;
                default: break;                                              // not an arithmetic op
            }
            double imm_val;                                                      // folded immediate value
            if (has_imm && try_fold_number(node->binary.right, &imm_val) &&
                imm_val == (int)imm_val && imm_val >= 0 && imm_val <= 65535) {
                int left_reg = codegen_expression(cg, node->binary.left);        // evaluate left
                int cached = imm_lvn_lookup(cg, imm_op, left_reg, -1, (int)imm_val); // reuse if identical
                if (cached >= 0) {
                    free_register(cg, left_reg);                                 // release left temp
                    if (dest_hint < 0 || dest_hint == cached) return cached;     // return cached reg
                    emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);   // copy into hint
                    return dest_hint;
                }
                int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);  // result destination
                emit(cg, INST(imm_op, result_reg, left_reg, (int)imm_val), node->line);  // fused op
                if (dest_hint < 0) imm_lvn_add(cg, imm_op, left_reg, -1, (int)imm_val, result_reg);  // cache self-allocated
                free_register(cg, left_reg);                                     // free left
                return result_reg;                                               // return result
            }
            // commutative IMM: when the left operand is a small non-negative constant and the
            // operator is + or *, swap the operands so the constant lands in the immediate
            // field; x + 1 / 1 + x and x * 2 / 2 * x generate identical bytecode
            if (has_imm && (node->binary.op == TOKEN_PLUS || node->binary.op == TOKEN_STAR) &&
                try_fold_number(node->binary.left, &imm_val) &&
                imm_val == (int)imm_val && imm_val >= 0 && imm_val <= 65535) {
                int right_reg = codegen_expression(cg, node->binary.right);      // evaluate right
                int cached = imm_lvn_lookup(cg, imm_op, right_reg, -1, (int)imm_val);  // reuse if identical
                if (cached >= 0) {
                    free_register(cg, right_reg);                                // release right temp
                    if (dest_hint < 0 || dest_hint == cached) return cached;     // return cached reg
                    emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);   // copy into hint
                    return dest_hint;
                }
                int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);  // result destination
                emit(cg, INST(imm_op, result_reg, right_reg, (int)imm_val), node->line);  // fused op
                if (dest_hint < 0) imm_lvn_add(cg, imm_op, right_reg, -1, (int)imm_val, result_reg);  // cache self-allocated
                free_register(cg, right_reg);                                    // free right
                return result_reg;                                               // return result
            }
            
            int left_reg = codegen_expression(cg, node->binary.left);               // evaluate left
            int right_reg = codegen_expression(cg, node->binary.right);             // evaluate right
            
            bool both_numbers = is_number_expression(cg, node->binary.left) &&      // check both operands known numeric
                                is_number_expression(cg, node->binary.right);
            
            Opcode op;                                                              // opcode
            switch (node->binary.op) {                                              // map operator
                case TOKEN_PLUS:           op = OP_ADD; break;
                case TOKEN_MINUS:          op = OP_SUB; break;
                case TOKEN_STAR:           op = OP_MUL; break;
                case TOKEN_SLASH:          op = OP_DIV; break;
                case TOKEN_PERCENT:        op = OP_MOD; break;
                case TOKEN_EQUAL_EQUAL:    op = both_numbers ? OP_CMP_EQ_NUM : OP_CMP_EQ; break;    // specialized if both numbers
                case TOKEN_NOT_EQUAL:      op = both_numbers ? OP_CMP_NEQ_NUM : OP_CMP_NEQ; break;  // specialized if both numbers
                case TOKEN_LESS:           op = OP_CMP_LT; break;
                case TOKEN_GREATER:        op = OP_CMP_GT; break;
                case TOKEN_LESS_EQUAL:     op = OP_CMP_LTE; break;
                case TOKEN_GREATER_EQUAL:  op = OP_CMP_GTE; break;
                default:                                                             // unknown
                    free_register(cg, left_reg);                                     // free left
                    free_register(cg, right_reg);                                    // free right
                    return dest_hint >= 0 ? dest_hint : alloc_register(cg);          // return uninitialized
            }

            // LVN for register-register arithmetic only; comparisons still need
            // their own register per result because the flag bit is not preserved
            bool lvn_ok = (op == OP_ADD || op == OP_SUB || op == OP_MUL ||
                           op == OP_DIV || op == OP_MOD);
            if (lvn_ok) {
                int cached = imm_lvn_lookup(cg, op, left_reg, right_reg, 0);        // reuse if identical
                if (cached >= 0) {
                    free_register(cg, left_reg);                                     // release left temp
                    free_register(cg, right_reg);                                    // release right temp
                    if (dest_hint < 0 || dest_hint == cached) return cached;         // return cached reg
                    emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);       // copy into hint
                    return dest_hint;
                }
            }

            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);       // result destination
            emit(cg, INST(op, result_reg, left_reg, right_reg), node->line);         // emit operation
            if (lvn_ok && dest_hint < 0) {
                imm_lvn_add(cg, op, left_reg, right_reg, 0, result_reg);             // cache self-allocated
            }
            free_register(cg, left_reg);                                             // free left
            free_register(cg, right_reg);                                            // free right
            return result_reg;                                                       // return result
        }
        case AST_UNARY: {                                                            // unary operation
            int operand_reg = codegen_expression(cg, node->unary.operand);           // evaluate operand
            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);        // result destination
            switch (node->unary.op) {                                                // map operator
                case TOKEN_MINUS:                                                    // negation
                    emit(cg, INST(OP_NEG, result_reg, operand_reg, 0), node->line);
                    break;
                case TOKEN_NOT:                                                      // logical not
                    emit(cg, INST(OP_NOT, result_reg, operand_reg, 0), node->line);
                    break;
                default: break;                                                      // unknown
            }
            free_register(cg, operand_reg);                                          // free operand
            return result_reg;                                                       // return result
        }

        case AST_CALL:                                                               // function call
            return codegen_call(cg, node, dest_hint);

        case AST_INDEX_ACCESS: {                                                     // table index access
            // licm: if the enclosing loop hoisted this exact access, return the pre-loaded register
            if (cg->hoist.get_count > 0 &&
                node->access.object->type == AST_IDENTIFIER &&
                node->access.member->type == AST_LITERAL_NUMBER) {
                const char* nm = node->access.object->identifier.name;
                double idx = node->access.member->literal_number.number_value;
                for (int i = 0; i < cg->hoist.get_count; i++) {
                    if (strcmp(cg->hoist.get_names[i], nm) == 0 &&
                        cg->hoist.get_indices[i] == idx) {
                        int r = cg->hoist.get_regs[i];
                        if (dest_hint < 0 || dest_hint == r) return r;
                        emit(cg, INST(OP_MOVE, dest_hint, r, 0), node->line);
                        return dest_hint;
                    }
                }
            }

            bool is_module_access = false;                                           // module flag
            char full_name[512] = "";                                                // qualified name

            if (node->access.object->type == AST_IDENTIFIER) {                       // object is identifier
                const char* obj_name = node->access.object->identifier.name;         // object name
                const char* member_name = NULL;                                      // member name

                if (node->access.member->type == AST_IDENTIFIER) {                   // member identifier
                    member_name = node->access.member->identifier.name;              // get name
                }
                else if (node->access.member->type == AST_LITERAL_STRING) {          // member string
                    member_name = node->access.member->literal_string.string_value;  // get string
                }

                if (member_name) {                                                   // valid member
                    for (int i = 0; i < cg->module_count; i++) {                     // check imported modules
                        if (strcmp(cg->imported_modules[i], obj_name) == 0) {        // matches import
                            is_module_access = true;                                 // mark as module
                            break;                                                   // exit loop
                        }
                    }
                    if (!is_module_access && is_known_builtin_module(obj_name)) {    // builtin module
                        is_module_access = true;                                     // mark as module
                    }

                    if (is_module_access) {                                                      // module access
                        snprintf(full_name, sizeof(full_name), "%s.%s", obj_name, member_name);  // build name
                    }
                }
            }

            if (is_module_access) {                                                              // module access
                int global_idx = bytecode_get_global(cg->chunk, full_name);                      // lookup global
                if (global_idx < 0) {                                                            // not found
                    global_idx = bytecode_add_global(cg->chunk, full_name);                      // add global
                }
                int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                       // use hint or fresh
                emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);                  // load global
                return reg;                                                                      // return register
            }

            int obj_reg = codegen_expression(cg, node->access.object);                           // evaluate object
            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                    // result dest
            
            if (node->access.member->type == AST_LITERAL_NUMBER) {                               // member is number literal
                double key_val = node->access.member->literal_number.number_value;               // get key value
                
                if (key_val == (int)key_val && key_val >= 1 && key_val <= 65535) {               // fits in immediate
                    emit(cg, INST(OP_TABLE_GET_INT, result_reg, obj_reg, (int)key_val), node->line);  // direct array access
                    free_register(cg, obj_reg);                                                  // free object
                    return result_reg;                                                           // return result
                }
            }
            
            if (node->access.member->type == AST_IDENTIFIER) {                                   // member identifier
                int local_reg = find_local(cg, node->access.member->identifier.name);            // check local
                if (local_reg >= 0) {                                                            // local variable
                    Opcode get_op = is_integer_expression(cg, node->access.member)               // whole-number key?
                                    ? OP_TABLE_GET_NUM : OP_TABLE_GET;
                    emit(cg, INST(get_op, result_reg, obj_reg, local_reg), node->line);          // get by local
                } else {                                                                         // not local
                    int global_idx = bytecode_get_global(cg->chunk, node->access.member->identifier.name);  // check global
                    if (global_idx >= 0) {                                                       // global exists
                        int key_reg = alloc_register(cg);                                        // allocate key reg
                        emit(cg, INST(OP_LOAD_GLOBAL, key_reg, global_idx, 0), node->line);      // load global
                        emit(cg, INST(OP_TABLE_GET, result_reg, obj_reg, key_reg), node->line);  // get by global
                        free_register(cg, key_reg);                                              // free key
                    } else {                                                                     // constant string
                        int key_idx = bytecode_add_string_constant(cg->chunk,                    // add string constant
                            node->access.member->identifier.name);
                        emit(cg, INST(OP_TABLE_GET_CONST, result_reg, obj_reg, key_idx), node->line); // get by const
                    }
                }
            } else if (node->access.member->type == AST_STRING_INTERP) {                 // "prefix{expr}" key?
                int prefix_idx = key_str_prefix_idx(cg, node->access.member);            // check without emitting
                if (prefix_idx >= 0) {                                                   // fuse-able pattern
                    int num_reg = codegen_expression(cg,                                  // evaluate the number
                                     node->access.member->string_interp.parts->nodes[1]);
                    int packed = (prefix_idx << 16) | (num_reg & 0xFFFF);                // pack op2
                    emit(cg, INST(OP_TABLE_GET_KEY_STR, result_reg, obj_reg, packed),    // fused get
                         node->line);
                    free_register(cg, num_reg);                                          // free number temp
                    free_register(cg, obj_reg);                                          // free object
                    return result_reg;                                                   // return result
                }
                int key_reg = codegen_expression(cg, node->access.member);               // fallback: full interp
                Opcode get_op = is_integer_expression(cg, node->access.member)           // whole-number key?
                                ? OP_TABLE_GET_NUM : OP_TABLE_GET;
                emit(cg, INST(get_op, result_reg, obj_reg, key_reg), node->line);        // get by key
                free_register(cg, key_reg);                                              // free key
                free_register(cg, obj_reg);                                              // free object
                return result_reg;                                                       // return result
            } else {                                                                     // plain member expression
                int key_reg = codegen_expression(cg, node->access.member);               // evaluate key
                Opcode get_op = is_integer_expression(cg, node->access.member)           // whole-number key?
                                ? OP_TABLE_GET_NUM : OP_TABLE_GET;
                emit(cg, INST(get_op, result_reg, obj_reg, key_reg), node->line);        // get by key
                free_register(cg, key_reg);                                              // free key
            }
            free_register(cg, obj_reg);                                                  // free object
            return result_reg;                                                           // return result
        }

        case AST_TABLE_LITERAL: {                                                        // table literal
            int table_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);             // table dest
            emit(cg, INST(OP_NEW_TABLE, table_reg, 0, 0), node->line);                   // create table

            for (int i = 0; i < node->table_literal.items->count; i++) {                      // sequential items
                int value_reg = codegen_expression(cg, node->table_literal.items->nodes[i]);  // evaluate
                emit(cg, INST(OP_TABLE_APPEND, table_reg, value_reg, 0), node->line);         // append
                free_register(cg, value_reg);                                                 // free value
            }

            for (int i = 0; i < node->table_literal.key_values->count; i++) {        // key-value pairs
                ASTNode* kv = node->table_literal.key_values->nodes[i];              // key-value node
                ASTNode* key = kv->binary.left;                                      // key expression
                int value_reg = codegen_expression(cg, kv->binary.right);            // evaluate value

                if (key->type == AST_LITERAL_NUMBER) {                               // numeric key
                    double key_val = key->literal_number.number_value;               // key value

                    if (key_val == (int)key_val && key_val >= 1 && key_val <= 65535) {  // fits immediate
                        emit(cg, INST(OP_TABLE_SET_INT, table_reg, (int)key_val,     // direct array set
                                      value_reg), node->line);
                    } else {                                                         // large or fractional
                        int key_reg = codegen_expression(cg, key);                   // materialize key
                        emit(cg, INST(OP_TABLE_SET, table_reg, key_reg,              // set by numeric key
                                      value_reg), node->line);
                        free_register(cg, key_reg);                                  // free key
                    }
                } else if (key->type == AST_LITERAL_STRING) {                        // string key
                    const char* key_str = key->literal_string.string_value;          // key string
                    int key_idx = bytecode_add_string_constant(cg->chunk, key_str);  // add key constant
                    emit(cg, INST(OP_TABLE_SET_CONST, table_reg, key_idx,            // set by string key
                                  value_reg), node->line);
                } else {                                                             // dynamic key expression
                    int key_reg = codegen_expression(cg, key);                       // evaluate key
                    emit(cg, INST(OP_TABLE_SET, table_reg, key_reg,                  // set by dynamic key
                                  value_reg), node->line);
                    free_register(cg, key_reg);                                      // free key
                }
                free_register(cg, value_reg);                                        // free value
            }
            return table_reg;                                                        // return table
        }

        case AST_STRING_INTERP:                                                   // string interpolation
            return codegen_string_interp(cg, node, dest_hint);

        case AST_TERNARY: {                                                       // ternary expression
            ASTNode* condition = node->ternary.condition;                         // condition
            ASTNode* true_expr = node->ternary.true_expr;                         // true branch
            ASTNode* false_expr = node->ternary.false_expr;                       // false branch
            
            int dest_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);       // result destination
            int cond_reg = codegen_expression(cg, condition);                     // evaluate condition
            
            LocalNumSnap pre = snap_numbers(cg);                                  // snapshot at branch point
            
            int jump_to_false = bytecode_current_offset(cg->chunk);               // jump to false
            emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);         // jump if false
            free_register(cg, cond_reg);                                          // free condition
            
            codegen_expression_into(cg, true_expr, dest_reg);                     // write true into dest
            LocalNumSnap true_snap = snap_numbers(cg);                            // snapshot after true
            
            int jump_to_end = bytecode_current_offset(cg->chunk);                 // jump to end
            emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);                         // emit jump
            
            restore_numbers(cg, pre);                                             // reset to branch point for false
            
            int false_addr = bytecode_current_offset(cg->chunk);                  // false address
            PATCH_JUMP(cg, jump_to_false, false_addr);                            // patch jump
            
            codegen_expression_into(cg, false_expr, dest_reg);                    // write false into dest
            LocalNumSnap false_snap = snap_numbers(cg);                           // snapshot after false
            
            merge_numbers(cg, true_snap, false_snap);                             // intersect both branches
            free_snap(true_snap);                                                 // release temporaries
            free_snap(false_snap);
            free_snap(pre);
            
            int end_addr = bytecode_current_offset(cg->chunk);                    // end address
            PATCH_JUMP(cg, jump_to_end, end_addr);                                // patch jump
            
            return dest_reg;                                                      // return result
        }

        case AST_AWAIT: {                                                          // await expression
            bool saved_flag = cg->current_call_is_awaited;
            cg->current_call_is_awaited = true;
            int inner_reg = codegen_expression(cg, node->await_expr.expression);   // evaluate inner
            cg->current_call_is_awaited = saved_flag;

            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);      // result destination
            emit(cg, INST(OP_AWAIT, result_reg, inner_reg, 0), node->line);        // unwrap future
            free_register(cg, inner_reg);                                          // free inner temp
            return result_reg;                                                     // return result
        }

        default: {                                                                // unknown node
            int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);            // use hint or fresh
            emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), node->line);                  // load false
            return reg;                                                           // return false
        }
    }
}

// registers a module-scoped global variable for name resolution
static void add_module_global(CodeGenerator* cg, const char* full_name) {
    for (int i = 0; i < cg->module_globals_count; i++) {                          // check existing
        if (strcmp(cg->module_globals[i], full_name) == 0) return;                // already exists
    }
    if (cg->module_globals_count >= cg->module_globals_capacity) {                // need more space
        cg->module_globals_capacity = cg->module_globals_capacity == 0 ? 16 : cg->module_globals_capacity * 2;  // double
        cg->module_globals = (char**)realloc(cg->module_globals, sizeof(char*) * cg->module_globals_capacity);  // reallocate
    }
    cg->module_globals[cg->module_globals_count++] = strdup(full_name);           // add name
}

// emits a fused match check that jumps to a target when the subject matches
static int emit_match_check(CodeGenerator* cg, int subject_reg, ASTNode* pattern, int line) {
    if (pattern->type == AST_LITERAL_NUMBER) {
        int idx = bytecode_add_number_constant(cg->chunk,
                                               pattern->literal_number.number_value);
        return emit(cg, INST(OP_JUMP_MATCH_NUM, 0, subject_reg, idx), line);
    }
    if (pattern->type == AST_LITERAL_STRING) {
        int idx = bytecode_add_string_constant(cg->chunk,
                                               pattern->literal_string.string_value);
        return emit(cg, INST(OP_JUMP_MATCH_STR, 0, subject_reg, idx), line);
    }
    if (pattern->type == AST_LITERAL_BOOL) {
        return emit(cg, INST(OP_JUMP_MATCH_BOOL, 0, subject_reg,
                             pattern->literal_bool.bool_value ? 1 : 0), line);
    }
    if (pattern->type == AST_LITERAL_NONE) {
        return emit(cg, INST(OP_JUMP_MATCH_NONE, 0, subject_reg, 0), line);
    }
    if (pattern->type == AST_UNARY && pattern->unary.op == TOKEN_MINUS &&
        pattern->unary.operand->type == AST_LITERAL_NUMBER) {
        double val = -pattern->unary.operand->literal_number.number_value;
        int idx = bytecode_add_number_constant(cg->chunk, val);
        return emit(cg, INST(OP_JUMP_MATCH_NUM, 0, subject_reg, idx), line);
    }
    return -1;  // parser already reported invalid pattern
}

// emits a match statement as a chain of fused constant checks and jumps
static void codegen_match_statement(CodeGenerator* cg, ASTNode* node) {
    LocalNumSnap match_before = snap_numbers(cg);                        // snapshot before any case body

    int subject_reg = codegen_expression(cg, node->match_stmt.subject);  // evaluate subject
    ASTNodeList* cases = node->match_stmt.cases;                         // non-default cases
    ASTNode* default_case = node->match_stmt.default_case;               // optional default
    int case_count = cases->count;                                       // number of non-default cases
    int line = node->line;                                               // source line for debug info

    int* match_jumps = (int*)malloc(sizeof(int) * case_count);           // jump offsets for each case
    int* end_jumps = (int*)malloc(sizeof(int) * (case_count + 2));       // jumps to match end
    int end_jump_count = 0;                                              // number of end jumps emitted

    for (int i = 0; i < case_count; i++) {                               // emit all case checks first
        ASTNode* case_node = cases->nodes[i];
        match_jumps[i] = emit_match_check(cg, subject_reg,
                                          case_node->case_stmt.pattern, line);
    }

    int no_match_jump = emit(cg, INST(OP_JUMP, 0, 0, 0), line);          // jump when no case matched

    int prev_floor = cg->register_floor;                                 // save floor
    int new_floor = subject_reg + 1;                                     // pin subject across cases
    if (new_floor < prev_floor) new_floor = prev_floor;                  // never lower an outer floor
    cg->register_floor = new_floor;
    for (int i = 0; i < case_count; i++) {                               // emit each case body
        ASTNode* case_node = cases->nodes[i];
        restore_numbers(cg, match_before);                               // reset flags before each case body
        int body_start = bytecode_current_offset(cg->chunk);             // body start address
        PATCH_JUMP(cg, match_jumps[i], body_start);                      // patch match jump
        codegen_block(cg, case_node->case_stmt.body);                    // emit body block
        end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);  // jump to end
    }

    if (default_case) {                                                  // emit default body
        restore_numbers(cg, match_before);                               // reset flags before default body
        int default_start = bytecode_current_offset(cg->chunk);          // default body start
        PATCH_JUMP(cg, no_match_jump, default_start);                    // patch no-match jump
        codegen_block(cg, default_case->case_stmt.body);                 // emit default body
        end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);  // jump to end
    }
    cg->register_floor = prev_floor;                                     // restore floor after match

    restore_numbers(cg, match_before);                                   // conservative: unknown state after match

    int end_addr = bytecode_current_offset(cg->chunk);                   // match end address
    if (!default_case) {
        PATCH_JUMP(cg, no_match_jump, end_addr);                         // no default: go to end
    }
    for (int i = 0; i < end_jump_count; i++) {                           // patch all end jumps
        PATCH_JUMP(cg, end_jumps[i], end_addr);
    }

    free(match_jumps);
    free(end_jumps);
    free_snap(match_before);                                             // release snapshot
    free_register(cg, subject_reg);                                      // release subject register
}

// emits a variable declaration, writing the value directly into the local slot
static void codegen_var_decl(CodeGenerator* cg, ASTNode* node) {
    // copy propagation: `y = x` where x is a local and x is dead after this statement
    if (node->var_assign.name && node->var_assign.value &&
        node->var_assign.value->type == AST_IDENTIFIER) {
        const char* rhs_name = node->var_assign.value->identifier.name;
        int rhs_reg = find_local(cg, rhs_name);
        if (rhs_reg >= 0 &&
            strcmp(node->var_assign.name, rhs_name) != 0 &&
            is_local_dead_after_current_stmt(cg, rhs_name)) {
            int y_reg   = add_local(cg, node->var_assign.name);
            int y_slot  = find_local_slot(cg, node->var_assign.name);
            int x_slot  = find_local_slot(cg, rhs_name);

            cg->locals.registers[y_slot] = rhs_reg;                          // y lives in x's register

            if (y_reg == cg->next_register - 1) {                            // freshly allocated temp
                cg->next_register--;                                         // reclaim it
            }

            if (x_slot >= 0) {                                               // inherit type flags
                cg->locals.is_number[y_slot]  = cg->locals.is_number[x_slot];
                cg->locals.is_integer[y_slot] = cg->locals.is_integer[x_slot];
            } else {
                cg->locals.is_number[y_slot]  = false;
                cg->locals.is_integer[y_slot] = false;
            }

            bool need_global = (cg->current_module != NULL) ||               // mirror the normal path
                               (cg->for_scope_depth == 0 &&
                                ((cg->current_function == 0) ||
                                 cg->current_function_has_nested));
            if (need_global) {
                const char* var_name = node->var_assign.name;
                char global_name[512];
                if (cg->current_module) {
                    snprintf(global_name, sizeof(global_name), "%s.%s",
                             cg->current_module, var_name);
                    var_name = global_name;
                    add_module_global(cg, global_name);
                }
                int global_idx = bytecode_add_global(cg->chunk, var_name);
                emit(cg, INST(OP_STORE_GLOBAL, rhs_reg, global_idx, 0), node->line);
            }
            return;
        }
    }

    // dce: dead store with a side-effect-free rhs
    bool can_dce = (cg->current_function != 0) &&
                   (!cg->current_function_has_nested) &&
                   is_local_dead_after_current_stmt(cg, node->var_assign.name) &&
                   !codegen_expr_has_side_effect(node->var_assign.value);
    if (can_dce) {
        return;                                                              // nothing observable happens
    }

    int local_reg = add_local(cg, node->var_assign.name);                    // allocate local first
    int slot = find_local_slot(cg, node->var_assign.name);                   // slot index for flag update

    int hint = local_reg;                                                    // default: write directly
    if (ast_unsafe_direct_assign(node->var_assign.value, node->var_assign.name)) {
        hint = -1;                                                           // RHS observes partial write: unsafe
    }
    int result = codegen_expression_into(cg, node->var_assign.value, hint);  // evaluate
    if (result != local_reg) {                                               // arrived in a fresh temp
        emit(cg, INST(OP_MOVE, local_reg, result, 0), node->line);           // copy into local slot
        free_register(cg, result);                                           // release temp
    }

    if (slot >= 0) {                                                         // update tracked numeric-ness
        cg->locals.is_number[slot] = is_number_expression(cg, node->var_assign.value);
        cg->locals.is_integer[slot] = is_integer_expression(cg, node->var_assign.value);
    }

    bool need_global = (cg->current_module != NULL) ||                       // module scope: global
                       (cg->for_scope_depth == 0 &&                          // not inside a for-scope
                        ((cg->current_function == 0) ||                      // top-level: global
                         cg->current_function_has_nested));                  // nested fn may capture

    if (need_global) {                                                       // register global slot
        const char* var_name = node->var_assign.name;                        // variable name
        char global_name[512];                                               // qualified buffer

        if (cg->current_module) {                                            // inside module
            snprintf(global_name, sizeof(global_name), "%s.%s",              // qualify with module
                     cg->current_module, var_name);
            var_name = global_name;                                          // use qualified name
            add_module_global(cg, global_name);                              // register module name
        }

        int global_idx = bytecode_add_global(cg->chunk, var_name);           // add or reuse global
        emit(cg, INST(OP_STORE_GLOBAL, local_reg, global_idx, 0),            // store local into global
             node->line);
    }
}

// emits assignment, writing directly into the destination when known
static int codegen_assign_expr(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    if (node->var_assign.access_path) {                                      // indexed assignment
        return codegen_index_assign(cg, node, dest_hint);
    }

    if (!node->var_assign.name) {                                            // no name (expression)
        return codegen_expression_into(cg, node->var_assign.value, dest_hint);  // forward hint
    }

    int local_reg = find_local(cg, node->var_assign.name);                   // find existing local
    if (local_reg >= 0) {                                                    // local variable
        int slot = find_local_slot(cg, node->var_assign.name);               // slot index for flag update
        if (node->var_assign.value->type == AST_BINARY) {                    // binary operation
            ASTNode* bin = node->var_assign.value;                           // binary node
            if (bin->binary.left->type == AST_IDENTIFIER &&                  // x = x op y
                strcmp(bin->binary.left->identifier.name,                    // left is same var
                       node->var_assign.name) == 0) {
                
                if (bin->binary.op == TOKEN_PLUS &&                          // x = x + 1
                    bin->binary.right->type == AST_LITERAL_NUMBER &&         // right is number
                    bin->binary.right->literal_number.number_value == 1.0) {
                    emit(cg, INST(OP_INC, local_reg, 0, 0), node->line);     // in-place increment
                    if (slot >= 0) cg->locals.is_number[slot] = true;        // still a number
                    return local_reg;                                        // return local
                }
                
                if (bin->binary.op == TOKEN_MINUS &&                         // x = x - 1
                    bin->binary.right->type == AST_LITERAL_NUMBER &&         // right is number
                    bin->binary.right->literal_number.number_value == 1.0) {
                    emit(cg, INST(OP_DEC, local_reg, 0, 0), node->line);     // in-place decrement
                    if (slot >= 0) cg->locals.is_number[slot] = true;        // still a number
                    return local_reg;                                        // return local
                }
            }
        }

        int hint = local_reg;                                                // default: write directly
        if (ast_unsafe_direct_assign(node->var_assign.value, node->var_assign.name)) {
            hint = -1;                                                       // rhs observes partial write: unsafe
        }
        int result = codegen_expression_into(cg, node->var_assign.value, hint);  // evaluate
        if (result != local_reg) {                                           // arrived in a fresh temp
            emit(cg, INST(OP_MOVE, local_reg, result, 0), node->line);       // copy into local slot
            free_register(cg, result);                                       // release temp
        }
        if (slot >= 0) {                                                     // update tracked numeric-ness
            cg->locals.is_number[slot] = is_number_expression(cg, node->var_assign.value);
            cg->locals.is_integer[slot] = is_integer_expression(cg, node->var_assign.value);
        }
        return local_reg;                                                    // return local
    }

    int value_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);         // value destination
    codegen_expression_into(cg, node->var_assign.value, value_reg);          // evaluate into it

    const char* var_name = node->var_assign.name;                            // variable name
    char global_name[512];                                                   // qualified name

    if (cg->current_module) {                                                // inside module
        snprintf(global_name, sizeof(global_name), "%s.%s",                  // qualify with module
                 cg->current_module, var_name);
        bool is_known = false;                                               // known flag
        for (int i = 0; i < cg->module_globals_count; i++) {                 // check known
            if (strcmp(cg->module_globals[i], global_name) == 0) {           // found
                is_known = true;                                             // mark known
                break;                                                       // exit loop
            }
        }
        if (is_known) {                                                      // known module global
            var_name = global_name;                                          // use qualified
        }
    }

    int global_idx = bytecode_get_global(cg->chunk, var_name);               // lookup global
    if (global_idx < 0) {                                                    // not found
        global_idx = bytecode_add_global(cg->chunk, var_name);               // add global
    }
    emit(cg, INST(OP_STORE_GLOBAL, value_reg, global_idx, 0), node->line);   // store global
    return value_reg;                                                        // return value
}

// wrapper for assignments that discards the result register
static void codegen_assign(CodeGenerator* cg, ASTNode* node) {
    // dce: bare assignment statement whose local is dead and whose rhs is pure
    if (node->var_assign.name && !node->var_assign.access_path &&
        cg->current_function != 0 &&
        !cg->current_function_has_nested &&
        is_local_dead_after_current_stmt(cg, node->var_assign.name) &&
        !codegen_expr_has_side_effect(node->var_assign.value)) {
        return;
    }

    int reg = codegen_assign_expr(cg, node, -1);                             // fresh temp, no hint
    free_register(cg, reg);                                                  // discard result
}

// emits if/else if/else chain with optimized condition evaluation
static void codegen_if_statement(CodeGenerator* cg, ASTNode* node) {
    // constant condition: emit only the reachable branch, drop the dead one
    bool cv;
    if (try_fold_bool(node->if_stmt.condition, &cv)) {
        if (cv) {
            codegen_block(cg, node->if_stmt.then_branch);                    // if true: only then
            return;
        }
        if (node->if_stmt.elif_chain) {                                      // if false: try elif chain
            codegen_if_statement(cg, node->if_stmt.elif_chain);              // recurse: it may also fold
            return;
        }
        ASTNode* cb_else = node->if_stmt.else_branch;                        // if false: else, if any
        if (cb_else) {
            codegen_block(cg, cb_else);
        }
        return;
    }

    LocalNumSnap before = snap_numbers(cg);                                  // snapshot before any branch

    int jump_to_else = codegen_optimized_condition(cg, node->if_stmt.condition, node->line);  // try to fuse cond+jump
    if (jump_to_else < 0) {                                                  // not optimized
        int cond_reg = codegen_expression(cg, node->if_stmt.condition);      // evaluate condition
        jump_to_else = bytecode_current_offset(cg->chunk);                   // jump address
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);        // jump if false
        free_register(cg, cond_reg);                                         // free condition
    }

    LocalNumSnap entry = snap_numbers(cg);                                   // state at branch entry
    
    codegen_block(cg, node->if_stmt.then_branch);                            // emit then branch
    LocalNumSnap after_then = snap_numbers(cg);                              // state at then exit
    
    ASTNode* else_branch = node->if_stmt.else_branch;                        // direct else branch
    if (!else_branch && node->if_stmt.elif_chain) {                          // no direct else
        ASTNode* last = node->if_stmt.elif_chain;                            // walk elif chain
        while (last->if_stmt.elif_chain) last = last->if_stmt.elif_chain;    // find last elif
        else_branch = last->if_stmt.else_branch;                             // its else is the one
    }
    
    int end_jumps[64];                                                       // end jump array
    int end_jump_count = 0;                                                  // end jump count
    
    bool then_is_last = (node->if_stmt.elif_chain == NULL) && (else_branch == NULL);
    if (!then_is_last) {                                                     // not the trailing branch
        end_jumps[end_jump_count++] = bytecode_current_offset(cg->chunk);    // save position
        emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);                        // jump to end
    }
    
    int else_addr = bytecode_current_offset(cg->chunk);                      // else address
    PATCH_JUMP(cg, jump_to_else, else_addr);                                 // patch jump

    bool has_elif = (node->if_stmt.elif_chain != NULL);                      // elif chain present

    // running intersection of every branch exit; starts as then-branch's exit
    LocalNumSnap merged = after_then;

    if (has_elif) {
        ASTNode* elif = node->if_stmt.elif_chain;                            // else if chain
        while (elif) {
            restore_numbers(cg, entry);                                      // reset for this elif body

            int elif_cond_reg = codegen_expression(cg, elif->if_stmt.condition);
            int jump_to_next = bytecode_current_offset(cg->chunk);
            emit(cg, INST(OP_JUMP_IF_FALSE, 0, elif_cond_reg, 0), elif->line);
            free_register(cg, elif_cond_reg);

            codegen_block(cg, elif->if_stmt.then_branch);                    // emit else if body

            LocalNumSnap after_elif = snap_numbers(cg);                      // this branch's exit
            merge_numbers(cg, merged, after_elif);                           // intersect into cg->locals
            free_snap(merged);                                               // release previous merged
            free_snap(after_elif);
            merged = snap_numbers(cg);                                       // capture the intersection

            bool elif_is_last = (elif->if_stmt.elif_chain == NULL) && (else_branch == NULL);
            if (!elif_is_last) {                                             // not the trailing branch
                end_jumps[end_jump_count++] = bytecode_current_offset(cg->chunk);
                emit(cg, INST(OP_JUMP, 0, 0, 0), elif->line);
            }

            int next_addr = bytecode_current_offset(cg->chunk);
            PATCH_JUMP(cg, jump_to_next, next_addr);

            elif = elif->if_stmt.elif_chain;                                 // next else if
        }
    }

    if (else_branch) {                                                       // has else
        restore_numbers(cg, entry);                                          // reset before else
        codegen_block(cg, else_branch);                                      // emit else
        LocalNumSnap after_else = snap_numbers(cg);
        merge_numbers(cg, merged, after_else);                               // intersect else into merged
        free_snap(merged);
        free_snap(after_else);
        merged = snap_numbers(cg);
    } else {
        // no else: the fall-through (pre-branch `entry`) is another exit
        merge_numbers(cg, merged, entry);
        free_snap(merged);
        merged = snap_numbers(cg);
    }

    free_snap(merged);                                                       // final merged state
    free_snap(entry);                                                        // release entry snapshot
    free_snap(before);                                                       // release outer snapshot

    int end_addr = bytecode_current_offset(cg->chunk);                       // end address
    for (int i = 0; i < end_jump_count; i++) {                               // patch all jumps
        PATCH_JUMP(cg, end_jumps[i], end_addr);
    }
}

// tries to optimize comparison conditions into direct jump instructions
static int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line) {
    if (condition->type != AST_BINARY) return -1;                            // not a binary condition
    
    ApexTokenType op = condition->binary.op;                                 // operator
    ASTNode* left  = condition->binary.left;                                 // left operand
    ASTNode* right = condition->binary.right;                                // right operand
    
    if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL) {                  // only eq/neq are eligible
        bool want_truthy = (op == TOKEN_EQUAL_EQUAL);                        // == true / != false test truthiness
        ASTNode* value_node = NULL;                                          // operand to test for truthiness
        
        if (right->type == AST_LITERAL_BOOL &&                               // right is bool literal
            right->literal_bool.bool_value == want_truthy) {
            value_node = left;                                               // x == true / x != false
        } else if (left->type == AST_LITERAL_BOOL &&                         // left is bool literal
                   left->literal_bool.bool_value == want_truthy) {
            value_node = right;                                              // true == x / false != x
        }
        
        if (value_node) {                                                    // pattern matched
            int reg = codegen_expression(cg, value_node);                    // evaluate tested operand
            int jump_offset = emit(cg, INST(OP_JUMP_IF_FALSE, 0, reg, 0), line);  // jump when falsy
            free_register(cg, reg);                                          // free operand
            return jump_offset;                                              // return jump offset
        }
    }
    
    Opcode jump_op;                                                          // jump opcode for register-register
    Opcode jump_op_imm;                                                      // jump opcode for register-immediate
    bool has_imm = false;                                                    // true when an IMM variant exists
    
    bool both_numbers = is_number_expression(cg, left) &&                    // check both operands known numeric
                        is_number_expression(cg, right);
    
    switch (op) {                                                            // map to jump
        case TOKEN_LESS:          jump_op = OP_JUMP_IF_GTE; jump_op_imm = OP_JUMP_IF_GTE_IMM; has_imm = true; break;
        case TOKEN_LESS_EQUAL:    jump_op = OP_JUMP_IF_GT;  jump_op_imm = OP_JUMP_IF_GT_IMM;  has_imm = true; break;
        case TOKEN_GREATER:       jump_op = OP_JUMP_IF_LTE; jump_op_imm = OP_JUMP_IF_LTE_IMM; has_imm = true; break;
        case TOKEN_GREATER_EQUAL: jump_op = OP_JUMP_IF_LT;  jump_op_imm = OP_JUMP_IF_LT_IMM;  has_imm = true; break;
        case TOKEN_EQUAL_EQUAL:   jump_op = both_numbers ? OP_JUMP_IF_NEQ_NUM : OP_JUMP_IF_NEQ; jump_op_imm = OP_JUMP_IF_NEQ_IMM; has_imm = true; break;  // specialized
        case TOKEN_NOT_EQUAL:     jump_op = both_numbers ? OP_JUMP_IF_EQ_NUM : OP_JUMP_IF_EQ;   jump_op_imm = OP_JUMP_IF_EQ_IMM;   has_imm = true; break;  // specialized
        default: return -1;                                                  // not optimizable
    }
    
    double imm_val;                                                          // folded immediate
    if (has_imm && try_fold_number(right, &imm_val) &&                       // right folds to a small int
        imm_val == (int)imm_val && imm_val >= 0 && imm_val <= 65535) {
        int left_reg = codegen_expression(cg, left);                         // evaluate left
        int jump_offset = emit(cg, INST(jump_op_imm, 0, left_reg, (int)imm_val), line);  // fused jump
        free_register(cg, left_reg);                                         // free left
        return jump_offset;                                                  // return jump offset
    }
    
    int left_reg = codegen_expression(cg, left);                             // evaluate left
    int right_reg = codegen_expression(cg, right);                           // evaluate right
    
    int jump_offset = emit(cg, INST(jump_op, 0, left_reg, right_reg), line); // emit jump
    
    free_register(cg, right_reg);                                            // free right
    free_register(cg, left_reg);                                             // free left
    return jump_offset;                                                      // return jump offset
}

// emits indexed assignment like table[index] = value
static int codegen_index_assign(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    ASTNode* access = node->var_assign.access_path;                          // access path
    
    if (access->type == AST_INDEX_ACCESS &&                                  // simple module assignment
        access->access.object->type == AST_IDENTIFIER &&
        access->access.member->type == AST_IDENTIFIER) {
        const char* obj_name = access->access.object->identifier.name;       // object name
        bool is_module = false;                                              // module flag
        for (int i = 0; i < cg->module_count; i++) {                         // check imports
            if (strcmp(cg->imported_modules[i], obj_name) == 0) { is_module = true; break; }
        }
        if (!is_module && is_known_builtin_module(obj_name)) is_module = true;  // builtin module
        
        if (is_module) {                                                     // module assignment
            char full_name[512];                                             // qualified name
            snprintf(full_name, sizeof(full_name), "%s.%s", obj_name,        // build name
                     access->access.member->identifier.name);
            int val_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);   // value destination
            codegen_expression_into(cg, node->var_assign.value, val_reg);    // evaluate into it
            int global_idx = bytecode_get_global(cg->chunk, full_name);      // lookup global
            if (global_idx < 0) global_idx = bytecode_add_global(cg->chunk, full_name);  // add global
            emit(cg, INST(OP_STORE_GLOBAL, val_reg, global_idx, 0), node->line);  // store global
            return val_reg;                                                  // return value
        }
    }

    ASTNode* value_node = node->var_assign.value;                            // value to assign
    ASTNode* chain[256];                                                     // access chain
    int chain_len = 0;                                                       // chain length
    ASTNode* curr = access;                                                  // current node
    while (curr->type == AST_INDEX_ACCESS) {                                 // traverse chain
        if (chain_len >= 256) break;                                         // limit
        chain[chain_len++] = curr;                                           // add to chain
        curr = curr->access.object;                                          // move to object
    }
    
    int current_obj_reg = codegen_expression(cg, curr);                      // evaluate base object
    for (int i = chain_len - 1; i > 0; i--) {                                // traverse from end
        ASTNode* acc = chain[i];                                             // current access
        int next_obj_reg = alloc_register(cg);                               // allocate next reg
        
        if (acc->access.member->type == AST_LITERAL_NUMBER) {                // member is number literal
            double key_val = acc->access.member->literal_number.number_value;  // get key value
            
            if (key_val == (int)key_val && key_val >= 1 && key_val <= 65535) {  // fits in immediate
                emit(cg, INST(OP_TABLE_GET_INT, next_obj_reg,                // direct array access
                              current_obj_reg, (int)key_val), acc->line);
                free_register(cg, current_obj_reg);                          // free old object
                current_obj_reg = next_obj_reg;                              // update object
                continue;                                                    // next chain element
            }
        }
        
        int key_reg = codegen_expression(cg, acc->access.member);            // evaluate key
        emit(cg, INST(OP_TABLE_GET, next_obj_reg, current_obj_reg, key_reg), acc->line);  // get
        free_register(cg, key_reg);                                          // free key
        free_register(cg, current_obj_reg);                                  // free old object
        current_obj_reg = next_obj_reg;                                      // update object
    }
    
    int val_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);           // value destination
    codegen_expression_into(cg, value_node, val_reg);                        // evaluate into it
    ASTNode* final_acc = chain[0];                                           // final access
    
    if (final_acc->access.member->type == AST_LITERAL_NUMBER) {              // member is number literal
        double key_val = final_acc->access.member->literal_number.number_value;  // get key value
        
        if (key_val == (int)key_val && key_val >= 1 && key_val <= 65535) {   // fits in immediate
            emit(cg, INST(OP_TABLE_SET_INT, current_obj_reg, (int)key_val,   // direct array set
                          val_reg), final_acc->line);
            free_register(cg, current_obj_reg);                              // free object
            return val_reg;                                                  // return value
        }
    }
    
    if (final_acc->access.member->type == AST_STRING_INTERP) {               // "prefix{expr}" key?
        int prefix_idx = key_str_prefix_idx(cg, final_acc->access.member);   // check without emitting
        if (prefix_idx >= 0) {                                               // fuse-able pattern
            int num_reg = codegen_expression(cg,                            // evaluate the number
                             final_acc->access.member->string_interp.parts->nodes[1]);
            int packed = (prefix_idx << 16) | (num_reg & 0xFFFF);            // pack op2
            emit(cg, INST(OP_TABLE_SET_KEY_STR, current_obj_reg, val_reg,    // fused set
                          packed), final_acc->line);
            free_register(cg, num_reg);                                      // free number temp
            free_register(cg, current_obj_reg);                              // free object
            return val_reg;                                                  // return value
        }
        // fallback: full string-interp as the key
        int key_reg = codegen_expression(cg, final_acc->access.member);      // evaluate key
        emit(cg, INST(OP_TABLE_SET, current_obj_reg, key_reg, val_reg),      // set by key
             final_acc->line);
        free_register(cg, key_reg);                                          // free key
        free_register(cg, current_obj_reg);                                  // free object
        return val_reg;                                                      // return value
    }

    int key_reg = codegen_expression(cg, final_acc->access.member);          // evaluate key
    Opcode set_op = is_integer_expression(cg, final_acc->access.member)      // whole-number key?
                    ? OP_TABLE_SET_NUM : OP_TABLE_SET;
    emit(cg, INST(set_op, current_obj_reg, key_reg, val_reg), final_acc->line);  // set
    free_register(cg, key_reg);                                              // free key
    free_register(cg, current_obj_reg);                                      // free object
    
    return val_reg;                                                          // return value
}

// emits a break statement, patching jumps to loop exit later
static void codegen_break(CodeGenerator* cg, ASTNode* node) {
    if (cg->loop_stack.break_count < cg->loop_stack.break_capacity) {        // space available
        if (cg->loop_stack.is_fast) {                                        // fast loop
            emit(cg, INST(OP_POP_ITER, 0, 0, 0), node->line);                // pop iterator
        }
        
        int jump_offset = bytecode_current_offset(cg->chunk);                // jump position
        emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);                        // jump to exit
        
        cg->loop_stack.break_jumps[cg->loop_stack.break_count++] = jump_offset;  // store jump
    }
}

// emits a continue statement jumping to the loop's continue address
static void codegen_continue(CodeGenerator* cg, ASTNode* node) {
    emit(cg, INST(OP_JUMP, cg->loop_stack.continue_addr, 0, 0), node->line);  // jump to continue
}

// emits a function declaration with proper body compilation and state isolation
static void codegen_function_decl(CodeGenerator* cg, ASTNode* node) {
    const char* func_name = node->function_decl.name;                        // function name
    char global_name[512];                                                   // qualified name for global
    char chunk_func_name[512];                                               // qualified name for chunk function table
    
    if (cg->current_module) {                                                // inside a module
        snprintf(chunk_func_name, sizeof(chunk_func_name), "%s.%s",          // qualify with module name
                 cg->current_module, func_name);
    } else {                                                                 // top-level code
        strncpy(chunk_func_name, func_name, sizeof(chunk_func_name) - 1);    // use bare name
        chunk_func_name[sizeof(chunk_func_name) - 1] = '\0';                 // ensure null termination
    }

    int param_count = node->function_decl.params->count;                     // parameter count
    int func_idx = bytecode_add_function(cg->chunk, chunk_func_name, param_count);  // add function
    cg->chunk->functions[func_idx].is_async = node->function_decl.is_async;         // propagate async flag

    int jump_over = bytecode_current_offset(cg->chunk);                      // jump over function body
    emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);                            // emit jump
    
    int func_addr = bytecode_current_offset(cg->chunk);                      // function address
    cg->chunk->functions[func_idx].address = func_addr;                      // set address

    int prev_function = cg->current_function;                                // save current function
    int prev_next_register = cg->next_register;                              // save next register
    int prev_max_registers = cg->max_registers;                              // save max registers
    bool prev_has_nested = cg->current_function_has_nested;                  // save nested flag
    int prev_register_floor = cg->register_floor;                            // save register floor
    int prev_cache_floor = cg->cache_floor;                                  // save cache floor
    double* prev_num_values = cg->num_cache.values;                          // save outer numeric cache
    int* prev_num_regs = cg->num_cache.regs;                                 // save outer numeric cache
    int prev_num_count = cg->num_cache.count;                                // save outer numeric cache
    int prev_num_capacity = cg->num_cache.capacity;                          // save outer numeric cache
    char** prev_str_values = cg->str_cache.values;                           // save outer string cache
    int*   prev_str_regs   = cg->str_cache.regs;
    int    prev_str_count  = cg->str_cache.count;
    int    prev_str_capacity = cg->str_cache.capacity;
    int saved_count = cg->locals.count;                                      // save local count
    char** saved_names = NULL;                                               // saved names
    int* saved_regs = NULL;                                                  // saved registers
    bool* saved_is_number = NULL;                                            // saved numeric-ness flags
    bool* saved_is_integer = NULL;                                           // saved integer-ness flags

    if (saved_count > 0) {                                                   // have locals
        saved_names = (char**)malloc(sizeof(char*) * saved_count);           // allocate names
        saved_regs = (int*)malloc(sizeof(int) * saved_count);                // allocate regs
        saved_is_number = (bool*)malloc(sizeof(bool) * saved_count);         // allocate flags
        saved_is_integer = (bool*)malloc(sizeof(bool) * saved_count);        // allocate int flags
        for (int i = 0; i < saved_count; i++) {                              // copy locals
            saved_names[i] = strdup(cg->locals.names[i]);                    // copy name
            saved_regs[i] = cg->locals.registers[i];                         // copy reg
            saved_is_number[i] = cg->locals.is_number[i];                    // copy numeric flag
            saved_is_integer[i] = cg->locals.is_integer[i];                  // copy integer flag
        }
    }

    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);    // free local names
    free(cg->locals.names);                                                  // free names array
    free(cg->locals.registers);                                              // free registers array
    free(cg->locals.is_number);                                              // free numeric flags array
    free(cg->locals.is_integer);                                             // free integer flags array
    cg->locals.names = NULL;                                                 // clear names
    cg->locals.registers = NULL;                                             // clear regs
    cg->locals.is_number = NULL;                                             // clear flags
    cg->locals.is_integer = NULL;                                            // clear int flags
    cg->locals.count = 0;                                                    // reset count
    cg->locals.capacity = 0;                                                 // reset capacity
    cg->next_register = 0;                                                   // reset next reg
    cg->max_registers = 0;                                                   // reset max regs for new function
    cg->current_function = func_idx;                                         // set current function
    cg->current_function_has_nested =                                        // detect nested function decls
        block_has_function_decl(node->function_decl.body);
    cg->register_floor = 0;                                                  // reset register floor
    cg->cache_floor = 0;                                                     // fresh cache floor
    cg->num_cache.values = NULL;                                             // fresh numeric cache
    cg->num_cache.regs = NULL;
    cg->num_cache.count = 0;
    cg->num_cache.capacity = 0;
    cg->str_cache.values = NULL;                                             // fresh string cache
    cg->str_cache.regs = NULL;
    cg->str_cache.count = 0;
    cg->str_cache.capacity = 0;

    for (int i = 0; i < param_count; i++) {                                 // parameters
        ASTNode* param = node->function_decl.params->nodes[i];              // param node
        add_local(cg, param->param.name);                                   // add as local
    }

    // pre-declare locals so assignments inside match/if/for bind to locals, not globals
    int first_body_local = cg->locals.count;                                // first body-local slot
    collect_local_names(cg, node->function_decl.body);                      // scan body for assigned names

    // compact register assignment: linear scan reuses dead slots
    if (cg->locals.count > 0) {
        int n_loc = cg->locals.count;
        int* lf = (int*)malloc(sizeof(int) * n_loc);
        int* ll = (int*)malloc(sizeof(int) * n_loc);
        for (int i = 0; i < n_loc; i++) { lf[i] = -1; ll[i] = -1; }
        int sidx = 0;
        liveness_walk(cg, node->function_decl.body, &sidx, lf, ll);
        assign_registers_linear_scan(cg, param_count, lf, ll);
        free(lf);
        free(ll);
    }

    for (int i = first_body_local; i < cg->locals.count; i++) {
        int r = cg->locals.registers[i];
        bool dup = false;
        for (int j = first_body_local; j < i; j++) {
            if (cg->locals.registers[j] == r) { dup = true; break; }
        }
        if (!dup) emit(cg, INST(OP_LOAD_NONE, r, 0, 0), node->line);
    }

    codegen_block(cg, node->function_decl.body);                            // emit body

    bool ends_with_return = false;                                           // return flag
    if (cg->chunk->code_count > 0) {                                         // has code
        Instruction* last = &cg->chunk->code[cg->chunk->code_count - 1];     // last instruction
        if (last->opcode == OP_RETURN || last->opcode == OP_RETURN_NONE ||   // return type
            last->opcode == OP_RETURN_NUM || last->opcode == OP_RETURN_NUM_IMM ||
            last->opcode == OP_RETURN_BOOL) {
            ends_with_return = true;                                         // has return
        }
    }

    if (!ends_with_return) {
        emit(cg, INST(OP_RETURN_NONE, 0, 0, 0), node->line);                // implicit return none
    }

    cg->chunk->functions[func_idx].local_count = cg->locals.count;           // store local count
    int fn_max = cg->max_registers;                                          // computed max regs
    if (fn_max < REGISTER_INITIAL_SIZE) fn_max = REGISTER_INITIAL_SIZE;      // enforce pool minimum
    cg->chunk->functions[func_idx].max_registers = fn_max;                   // store padded max regs
    if (cg->locals.count > 0) {                                              // has locals
        cg->chunk->functions[func_idx].local_names = (char**)malloc(sizeof(char*) * cg->locals.count);  // allocate
        for (int i = 0; i < cg->locals.count; i++) {                         // copy names
            cg->chunk->functions[func_idx].local_names[i] = strdup(cg->locals.names[i]);
        }
    } else {
        cg->chunk->functions[func_idx].local_names = NULL;                   // no locals
    }

    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);    // free local names
    free(cg->locals.names);                                                  // free names array
    free(cg->locals.registers);                                              // free registers array
    free(cg->locals.is_number);                                              // free numeric flags array
    free(cg->locals.is_integer);                                             // free integer flags array

    cg->locals.names = saved_names;                                          // restore names
    cg->locals.registers = saved_regs;                                       // restore regs
    cg->locals.is_number = saved_is_number;                                  // restore numeric flags
    cg->locals.is_integer = saved_is_integer;                                // restore integer flags
    cg->locals.count = saved_count;                                          // restore count
    cg->locals.capacity = saved_count;                                       // restore capacity
    cg->next_register = prev_next_register;                                  // restore next reg
    cg->max_registers = prev_max_registers;                                  // restore max regs
    cg->current_function = prev_function;                                    // restore function
    cg->current_function_has_nested = prev_has_nested;                       // restore nested flag
    cg->register_floor = prev_register_floor;                                // restore register floor
    cg->cache_floor = prev_cache_floor;                                      // restore cache floor
    free(cg->num_cache.values);                                              // release fn-local numeric cache
    free(cg->num_cache.regs);
    cg->num_cache.values = prev_num_values;                                  // restore outer cache
    cg->num_cache.regs = prev_num_regs;
    cg->num_cache.count = prev_num_count;
    cg->num_cache.capacity = prev_num_capacity;

    for (int i = 0; i < cg->str_cache.count; i++) {                          // release fn-local string cache
        free(cg->str_cache.values[i]);
    }
    free(cg->str_cache.values);
    free(cg->str_cache.regs);
    cg->str_cache.values = prev_str_values;                                  // restore outer string cache
    cg->str_cache.regs = prev_str_regs;
    cg->str_cache.count = prev_str_count;
    cg->str_cache.capacity = prev_str_capacity;

    cg->current_function_has_nested = prev_has_nested;                       // restore nested flag

    PATCH_JUMP(cg, jump_over, bytecode_current_offset(cg->chunk));           // patch jump

    int func_const_idx = bytecode_add_constant(cg->chunk,                    // add function constant
        (Constant){.type = CONST_FUNCTION, .function_index = func_idx});
    
    const char* gname = node->function_decl.name;                            // bare name by default
    if (cg->current_module) {                                                // inside a module
        snprintf(global_name, sizeof(global_name), "%s.%s",                  // qualify with module
                 cg->current_module, node->function_decl.name);
        gname = global_name;                                                 // use qualified name
        add_module_global(cg, global_name);                                  // register module global
    }

    int global_idx = bytecode_get_global(cg->chunk, gname);                  // lookup global slot
    if (global_idx < 0) global_idx = bytecode_add_global(cg->chunk, gname);  // create if missing

    int temp_reg = alloc_register(cg);                                       // allocate temp
    emit(cg, INST(OP_LOAD_CONST, temp_reg, func_const_idx, 0), node->line);  // load function value
    emit(cg, INST(OP_STORE_GLOBAL, temp_reg, global_idx, 0), node->line);    // expose via global slot
    free_register(cg, temp_reg);                                             // free temp
}

// emits a return statement with optional value
static void codegen_return(CodeGenerator* cg, ASTNode* node) {
    if (node->return_stmt.value) {                                           // has return value
        double folded;                                                       // folded numeric value
        if (try_fold_number(node->return_stmt.value, &folded) &&             // folds to a compile-time constant
            folded == (int)folded && folded >= 0 && folded <= 65535) {       // fits in the immediate field
            emit(cg, INST(OP_RETURN_NUM_IMM, 0, (int)folded, 0), node->line);  // return the immediate directly
            return;                                                          // done, no register needed
        }
        int value_reg = codegen_expression(cg, node->return_stmt.value);     // evaluate value
        
        ASTNode* val = node->return_stmt.value;                              // value node
        bool is_number = false;                                              // guaranteed number flag
        bool is_bool = false;                                                // guaranteed boolean flag

        if (val->type == AST_LITERAL_NUMBER) {                               // number literal
            is_number = true;
        } else if (val->type == AST_LITERAL_BOOL) {                          // boolean literal
            is_bool = true;
        } else if (val->type == AST_BINARY) {                                // binary op
            ApexTokenType op = val->binary.op;                               // operator
            if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR || // arithmetic → number
                op == TOKEN_SLASH || op == TOKEN_PERCENT) {
                is_number = true;
            } else if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL ||   // comparisons → bool
                       op == TOKEN_LESS || op == TOKEN_GREATER ||
                       op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
                is_bool = true;
            }
        } else if (val->type == AST_UNARY) {                                 // unary op
            if (val->unary.op == TOKEN_MINUS) is_number = true;              // unary minus → number
            else if (val->unary.op == TOKEN_NOT) is_bool = true;             // not → bool
        } else if (val->type == AST_IDENTIFIER && !is_bool) {                // tracked local?
            int slot = find_local_slot(cg, val->identifier.name);
            if (slot >= 0) {
                if (cg->locals.is_number[slot])  is_number = true;           // proven number
                // is_bool is not tracked in the local table; leave it false
            }
        }
        
        if (is_bool) {                                                       // guaranteed boolean
            emit(cg, INST(OP_RETURN_BOOL, value_reg, 0, 0), node->line);
        } else if (is_number) {                                              // guaranteed number
            emit(cg, INST(OP_RETURN_NUM, value_reg, 0, 0), node->line);
        } else {                                                             // may be any type
            emit(cg, INST(OP_RETURN, value_reg, 0, 0), node->line);
        }
    } else {
        emit(cg, INST(OP_RETURN_NONE, 0, 0, 0), node->line);                 // return none directly
    }
}

// emits an expression statement, discarding the result
static void codegen_expr_statement(CodeGenerator* cg, ASTNode* node) {
    int result_reg = codegen_expression(cg, node->expr_stmt.expression);     // evaluate expression
    free_register(cg, result_reg);                                           // discard result
}

// statement dispatcher that routes each ast node type to its codegen function
static void codegen_statement(CodeGenerator* cg, ASTNode* node) {
    if (!node) return;                                                       // guard against null
    cg->imm_lvn.count = 0;                                                   // cache is statement-scoped
    switch (node->type) {                                                    // dispatch by type
        case AST_VAR_DECL:        codegen_var_decl(cg, node); break;         // variable decl
        case AST_ASSIGN:          codegen_assign(cg, node); break;           // assignment
        case AST_IF_STMT:         codegen_if_statement(cg, node); break;     // if statement
        case AST_FOR_STMT:        codegen_for_statement(cg, node); break;    // for loop
        case AST_FUNCTION_DECL:   codegen_function_decl(cg, node); break;    // function decl
        case AST_RETURN_STMT:     codegen_return(cg, node); break;           // return
        case AST_BREAK_STMT:      codegen_break(cg, node); break;            // break
        case AST_CONTINUE_STMT:   codegen_continue(cg, node); break;         // continue
        case AST_MATCH_STMT:      codegen_match_statement(cg, node); break;  // match statement
        case AST_IMPORT_STMT:     break;                                     // import (handled elsewhere)
        case AST_EXPR_STMT:       codegen_expr_statement(cg, node); break;   // expr stmt
        case AST_BLOCK:           codegen_block(cg, node); break;            // block
        case AST_MODULE_BLOCK: {                                             // module block
            if (cg->module_count >= cg->module_capacity) {                   // need space
                cg->module_capacity = cg->module_capacity == 0 ? 8 : cg->module_capacity * 2;
                cg->imported_modules = (char**)realloc(cg->imported_modules,
                                                       sizeof(char*) * cg->module_capacity);
            }
            cg->imported_modules[cg->module_count++] =                       // add module
                strdup(node->module_block.module_name);
            
            char* prev_module = cg->current_module;                          // save current module
            cg->current_module = node->module_block.module_name;             // set current module
            codegen_block(cg, node->module_block.body);                      // emit module body
            cg->current_module = prev_module;                                // restore module
            break;
        }
        default: break;                                                      // ignore
    }
}

// emits a block of statements sequentially, resetting temps between them
static void codegen_block(CodeGenerator* cg, ASTNode* node) {
    if (!node || (node->type != AST_BLOCK && node->type != AST_PROGRAM)) return;  // validate
    int frame = -1;                                                          // block_stack slot used
    if (cg->block_depth < 32) {
        frame = cg->block_depth;
        cg->block_stack[frame].stmts = node->block.statements;
        cg->block_stack[frame].index = 0;
    }
    cg->block_depth++;
    for (int i = 0; i < node->block.statements->count; i++) {                // iterate statements
        if (frame >= 0) cg->block_stack[frame].index = i;                    // record position for lookahead
        ASTNode* stmt = node->block.statements->nodes[i];                    // current statement
        codegen_statement(cg, stmt);                                         // emit statement
        int reset_to = locals_high_water(cg);                                // keep locals + pinned
        if (reset_to < cg->register_floor) reset_to = cg->register_floor;    // respect floor
        if (reset_to < cg->cache_floor) reset_to = cg->cache_floor;          // respect cache pins
        if (cg->next_register > reset_to) {                                  // drop temps only
            cg->next_register = reset_to;                                    // reclaim for next stmt
        }
    }
    cg->block_depth--;
}

// public entry point that generates bytecode from an ast
bool codegen_generate(CodeGenerator* cg, ASTNode* ast) {
    if (!cg || !ast) return false;                                           // validate
    
    bytecode_add_function(cg->chunk, "__entry-apex__", 0);                   // add entry function
    cg->current_function = 0;                                                // set current function
    
    if (ast->type == AST_PROGRAM || ast->type == AST_BLOCK) {                // program or block
        codegen_block(cg, ast);                                              // emit block
    } else {                                                                 // single statement
        codegen_statement(cg, ast);                                          // emit statement
        cg->next_register = locals_high_water(cg);                           // drop temps
    }
    
    int entry_max = cg->max_registers;                                       // computed entry max regs
    if (entry_max < REGISTER_INITIAL_SIZE) entry_max = REGISTER_INITIAL_SIZE; // enforce pool minimum
    cg->chunk->functions[0].max_registers = entry_max;                       // store padded entry max regs
    
    emit(cg, INST(OP_HALT, 0, 0, 0), 0);                                     // halt instruction
    return true;                                                             // success
}

#undef PATCH_JUMP
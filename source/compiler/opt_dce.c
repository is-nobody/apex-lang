// source/compiler/opt_dce.c
// Dead-store elimination and side-effect classification
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <string.h>

// returns true if an ast subtree reads a local variable by name
bool ast_references_local(ASTNode* node, const char* name) {
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
            if (node->var_assign.name && strcmp(node->var_assign.name, name) == 0) return true;
            if (node->var_assign.access_path &&
                ast_references_local(node->var_assign.access_path, name)) return true;
            return ast_references_local(node->var_assign.value, name);
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++) {       // any statement
                if (ast_references_local(node->block.statements->nodes[i], name)) return true;
            }
            return false;
        case AST_EXPR_STMT:
            return ast_references_local(node->expr_stmt.expression, name);  // expr statement
        case AST_RETURN_STMT:
            return node->return_stmt.value &&
                   ast_references_local(node->return_stmt.value, name);     // return value
        default:
            return false;                                                    // conservative
    }
}

// statement-level variant of ast_references_local
bool stmt_references_local(ASTNode* node, const char* name) {
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

// true if `name` is never referenced (read or written) in any statement after the current position
bool is_local_dead_after_current_stmt(CodeGenerator* cg, const char* name) {
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
bool ast_unsafe_direct_assign(ASTNode* node, const char* name) {
    if (!node || !name) return false;                                              // null guard
    switch (node->type) {
        case AST_STRING_INTERP:                                                    // check later parts only
            for (int i = 1; i < node->string_interp.parts->count; i++) {
                if (ast_references_local(node->string_interp.parts->nodes[i], name))
                    return true;                                                   // later part reads name
            }
            return false;                                                          // safe to write directly

        case AST_TABLE_LITERAL:                                                    // check items and key-values
            for (int i = 0; i < node->table_literal.items->count; i++) {
                if (ast_references_local(node->table_literal.items->nodes[i], name))
                    return true;                                                   // sequential item reads name
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

// true when evaluating `node` can have an observable effect (call, await, nested assignment)
bool codegen_expr_has_side_effect(ASTNode* node) {
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
            return false;  // identifiers, literals: pure
    }
}
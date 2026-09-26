// source/compiler/opt_licm.c
// Loop-invariant code motion: hoisted constants and t[CONST] reads
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// builtins safe to hoist out of a loop
bool builtin_is_pure(const char* name) {
    if (!name) return false;

    if (strcmp(name, "number") == 0) return true;                  // number(x) -> number
    if (strcmp(name, "string") == 0) return true;                  // string(x) -> string
    if (strcmp(name, "type") == 0) return true;                    // type(x)   -> string

    if (strcmp(name, "string.length") == 0) return true;           // -> number
    if (strcmp(name, "string.lower") == 0) return true;            // -> string
    if (strcmp(name, "string.upper") == 0) return true;            // -> string
    if (strcmp(name, "string.slice") == 0) return true;            // -> string
    if (strcmp(name, "string.join") == 0) return true;             // -> string
    if (strcmp(name, "string.trim") == 0) return true;             // -> string
    if (strcmp(name, "string.find") == 0) return true;             // -> number
    if (strcmp(name, "string.replace") == 0) return true;          // -> string
    if (strcmp(name, "string.repeat") == 0) return true;           // -> string
    // string.split: returns a fresh table, excluded

    if (strncmp(name, "math.", 5) == 0) return true;               // math.*: number, string, or table?
                                                                   // reference lists only number results
    if (strncmp(name, "base.encode_", 12) == 0) return true;       // -> string
    if (strncmp(name, "base.decode_", 12) == 0) return true;       // -> string

    if (strcmp(name, "regex.replace") == 0) return true;           // -> string
    // regex.find_all / split / search: return tables, excluded

    if (strncmp(name, "crypto.random_", 14) == 0) return false;    // not deterministic
    if (strcmp(name, "crypto.md5") == 0) return true;              // -> string
    if (strcmp(name, "crypto.sha1") == 0) return true;             // -> string
    if (strcmp(name, "crypto.sha256") == 0) return true;           // -> string
    if (strcmp(name, "crypto.sha384") == 0) return true;           // -> string
    if (strcmp(name, "crypto.sha512") == 0) return true;           // -> string
    if (strcmp(name, "crypto.hmac_md5") == 0) return true;         // -> string
    if (strcmp(name, "crypto.hmac_sha1") == 0) return true;        // -> string
    if (strcmp(name, "crypto.hmac_sha256") == 0) return true;      // -> string
    if (strcmp(name, "crypto.hmac_sha384") == 0) return true;      // -> string
    if (strcmp(name, "crypto.hmac_sha512") == 0) return true;      // -> string
    if (strcmp(name, "crypto.pbkdf2_md5") == 0) return true;       // -> string
    if (strcmp(name, "crypto.pbkdf2_sha1") == 0) return true;      // -> string
    if (strcmp(name, "crypto.pbkdf2_sha256") == 0) return true;    // -> string
    if (strcmp(name, "crypto.pbkdf2_sha384") == 0) return true;    // -> string
    if (strcmp(name, "crypto.pbkdf2_sha512") == 0) return true;    // -> string
    if (strcmp(name, "crypto.aes128_encrypt") == 0) return true;   // -> string
    if (strcmp(name, "crypto.aes128_decrypt") == 0) return true;   // -> string
    if (strcmp(name, "crypto.aes192_encrypt") == 0) return true;   // -> string
    if (strcmp(name, "crypto.aes192_decrypt") == 0) return true;   // -> string
    if (strcmp(name, "crypto.aes256_encrypt") == 0) return true;   // -> string
    if (strcmp(name, "crypto.aes256_decrypt") == 0) return true;   // -> string
    if (strcmp(name, "crypto.compare_strings") == 0) return true;  // -> bool

    if (strcmp(name, "json.encode") == 0) return true;             // -> string
    // json.decode: returns a table, excluded
    if (strcmp(name, "xml.encode") == 0) return true;              // -> string
    // xml.decode: returns a table, excluded
    if (strcmp(name, "csv.encode") == 0) return true;              // -> string
    // csv.decode: returns a table, excluded

    return false;                                                  // not on the pure whitelist
}

// body may mutate an outer-scope table (call/await/nested fn/indexed assign)
bool body_unsafe_for_licm(ASTNode* node) {
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
bool body_assigns_name(ASTNode* node, const char* name) {
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

// recursively collects foldable numeric constants in a loop body for hoisting
void collect_hoistable_numbers(CodeGenerator* cg, ASTNode* node) {
    if (!node || cg->hoist.count >= 8) return;                    // null guard / cap hoisted registers

    double v;
    if (try_fold_number(cg, node, &v)) {                          // pure numeric subtree?
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

    if (node->type == AST_INDEX_ACCESS &&
        node->access.object->type == AST_IDENTIFIER) {
        const char* obj = node->access.object->identifier.name;
        bool is_module = false;
        for (int i = 0; i < cg->module_count; i++) {
            if (strcmp(cg->imported_modules[i], obj) == 0) { is_module = true; break; }
        }
        if (!is_module && is_known_builtin_module(obj)) is_module = true;
        if (is_module) return;
    }

    if (node->type == AST_IDENTIFIER) {
        if (cg->current_module) return;
        const char* nm = node->identifier.name;
        if (find_local_slot(cg, nm) >= 0) return;
        if (body_assigns_name(body, nm)) return;
        if (strchr(nm, '.')) return;
        for (int i = 0; i < cg->module_count; i++) {
            char q[512];
            snprintf(q, sizeof(q), "%s.%s", cg->imported_modules[i], nm);
            if (bytecode_get_global(cg->chunk, q) >= 0) return;
        }
        add_hoistable_get(cg, nm, -1.0);
        return;
    }

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
            if (node->call.callee->type != AST_IDENTIFIER) {
                scan_hoistable_gets(cg, node->call.callee, body);
            }
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
void collect_hoistable_table_gets(CodeGenerator* cg, ASTNode* body) {
    if (!body) return;
    if (body_unsafe_for_licm(body)) return;   // calls, awaits, nested fns, indexed assigns
    scan_hoistable_gets(cg, body, body);
}

// clears const-known flags for every local whose value can change across loop iterations
void invalidate_loop_consts(CodeGenerator* cg, ASTNode* body,
                            const char* loop_var) {
    for (int i = 0; i < cg->locals.count; i++) {
        if (loop_var && strcmp(cg->locals.names[i], loop_var) == 0) {
            cg->locals.const_known[i] = false;
        } else if (body && body_assigns_name(body, cg->locals.names[i])) {
            cg->locals.const_known[i] = false;
        }
    }
}

// true when `for i = start, end[, step]` has all three bounds
bool for_is_zero_trip(CodeGenerator* cg, ASTNode* node) {
    if (!node || node->type != AST_FOR_STMT) return false;
    if (!node->for_stmt.var_name || !node->for_stmt.end) return false;   // not a range loop
    if (node->for_stmt.condition) return false;                          // not a range loop

    double start_v, end_v, step_v = 1.0;
    if (!try_fold_number(cg, node->for_stmt.start, &start_v)) return false;
    if (!try_fold_number(cg, node->for_stmt.end,   &end_v))   return false;
    if (node->for_stmt.step &&
        !try_fold_number(cg, node->for_stmt.step, &step_v))   return false;

    if (step_v == 0.0)                 return true;   // interpreter exits immediately
    if (step_v > 0 && start_v > end_v) return true;   // ascending out of range
    if (step_v < 0 && start_v < end_v) return true;   // descending out of range
    return false;
}

// deep structural equality for hoistable nodes (identifiers, literals,
// binary, unary, index_access); unknown types compare unequal
bool expr_struct_eq(ASTNode* a, ASTNode* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) return false;
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
        case AST_BINARY:
            return a->binary.op == b->binary.op &&
                   expr_struct_eq(a->binary.left, b->binary.left) &&
                   expr_struct_eq(a->binary.right, b->binary.right);
        case AST_UNARY:
            return a->unary.op == b->unary.op &&
                   expr_struct_eq(a->unary.operand, b->unary.operand);
        case AST_INDEX_ACCESS:
            return expr_struct_eq(a->access.object, b->access.object) &&
                   expr_struct_eq(a->access.member, b->access.member);
        case AST_CALL:
            if (!expr_struct_eq(a->call.callee, b->call.callee)) return false;
            if (a->call.arguments->count != b->call.arguments->count) return false;
            for (int i = 0; i < a->call.arguments->count; i++) {
                if (!expr_struct_eq(a->call.arguments->nodes[i],
                                    b->call.arguments->nodes[i])) return false;
            }
            return true;
        default:
            return false;
    }
}
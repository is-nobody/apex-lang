// source/compiler/opt_const_fold.c
// Compile-time constant folding and numeric-typing predicates
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <math.h>
#include <string.h>

// checks if a binary operator always produces a number result
static bool is_arithmetic_op(ApexTokenType op) {
    return op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR ||
           op == TOKEN_SLASH || op == TOKEN_PERCENT;
}

// tries to fold an expression into a compile-time numeric constant
bool try_fold_number(CodeGenerator* cg, ASTNode* node, double* out) {
    if (!node || !out) return false;                                   // null guard
    switch (node->type) {
        case AST_LITERAL_NUMBER:
            *out = node->literal_number.number_value;                  // literal value
            return true;
        case AST_IDENTIFIER: {
            if (!cg) return false;                                     // no context: cannot prove const
            int slot = find_local_slot(cg, node->identifier.name);
            if (slot < 0 || !cg->locals.const_known[slot]) return false;
            *out = cg->locals.const_value[slot];
            return true;
        }
        case AST_UNARY:
            if (node->unary.op == TOKEN_MINUS) {                       // unary minus
                double v;
                if (!try_fold_number(cg, node->unary.operand, &v)) return false;
                *out = -v;                                             // negate operand
                return true;
            }
            return false;                                              // not: operand may be non-number
        case AST_BINARY: {
            double l, r;
            if (!try_fold_number(cg, node->binary.left,  &l)) return false;  // left must fold
            if (!try_fold_number(cg, node->binary.right, &r)) return false;  // right must fold
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
        case AST_CALL:
            if (!cg) return false;
            if (recursive_call_is_bool(cg, node)) return false;            // handled by try_fold_bool
            if (try_fold_recursive_call(cg, node, out)) return true;       // pure recursive memo
            return try_fold_inline_call(cg, node, out);
        default:
            return false;                                              // calls, index, etc.
    }
}

// tries to fold an expression into a compile-time boolean constant only bool literals
bool try_fold_bool(CodeGenerator* cg, ASTNode* node, bool* out) {
    if (!node || !out) return false;                                   // null guard
    switch (node->type) {
        case AST_LITERAL_BOOL:
            *out = node->literal_bool.bool_value;                      // literal value
            return true;
        case AST_UNARY:
            if (node->unary.op == TOKEN_NOT) {                         // logical not
                bool v;
                if (!try_fold_bool(cg, node->unary.operand, &v)) return false;
                *out = !v;                                             // invert operand
                return true;
            }
            return false;                                              // unary minus is not a bool
        case AST_BINARY: {
            ApexTokenType op = node->binary.op;
            if (op == TOKEN_AND || op == TOKEN_OR) {                   // logical and/or
                bool l, r;
                if (!try_fold_bool(cg, node->binary.left,  &l)) return false;
                if (!try_fold_bool(cg, node->binary.right, &r)) return false;
                *out = (op == TOKEN_AND) ? (l && r) : (l || r);        // evaluate logical op
                return true;
            }
            if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL ||    // numeric comparisons
                op == TOKEN_LESS || op == TOKEN_GREATER ||
                op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
                double l, r;
                if (!try_fold_number(cg, node->binary.left,  &l)) return false;
                if (!try_fold_number(cg, node->binary.right, &r)) return false;
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
        case AST_CALL:
            if (!cg) return false;
            return try_fold_recursive_bool(cg, node, out);
        default:
            return false;                                              // identifiers, calls, etc.
    }
}

// checks if an expression is known to produce a number at this point in emission
bool is_number_expression(CodeGenerator* cg, ASTNode* node) {
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
bool is_integer_expression(CodeGenerator* cg, ASTNode* node) {
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
bool ast_same_expr(ASTNode* a, ASTNode* b) {
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
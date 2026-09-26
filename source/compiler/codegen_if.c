// source/compiler/codegen_if.c
// If/else-if/else codegen and fused comparison conditions
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>

// tries to optimize comparison conditions into direct jump instructions
int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line) {
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
    if (has_imm && try_fold_number(cg, right, &imm_val) &&                   // right folds to a small int
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

// emits if/else if/else chain with optimized condition evaluation
void codegen_if_statement(CodeGenerator* cg, ASTNode* node) {
    // constant condition: emit only the reachable branch, drop the dead one
    bool cv;
    if (try_fold_bool(cg, node->if_stmt.condition, &cv)) {
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

    // num_cache / str_cache entries added while emitting one branch are only valid on that branch's execution path
    int saved_num_count = cg->num_cache.count;
    int saved_str_count = cg->str_cache.count;

    int jump_to_else = codegen_optimized_condition(cg, node->if_stmt.condition, node->line);  // try to fuse cond+jump
    if (jump_to_else < 0) {                                                  // not optimized
        int cond_reg = codegen_expression(cg, node->if_stmt.condition);      // evaluate condition
        jump_to_else = emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);
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
            cg->num_cache.count = saved_num_count;                           // discard previous branch's numeric cache
            str_cache_truncate(cg, saved_str_count);                         // discard previous branch's string cache

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
        cg->num_cache.count = saved_num_count;                               // discard previous branch's numeric cache
        str_cache_truncate(cg, saved_str_count);                             // discard previous branch's string cache
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

    // when every branch exits and there is no else
    bool guard_only = !has_elif && !else_branch &&
                      stmt_always_exits(node->if_stmt.then_branch);
    if (!guard_only) {
        cg->num_cache.count = saved_num_count;
        str_cache_truncate(cg, saved_str_count);
    }

    int end_addr = bytecode_current_offset(cg->chunk);                       // end address
    for (int i = 0; i < end_jump_count; i++) {                               // patch all jumps
        PATCH_JUMP(cg, end_jumps[i], end_addr);
    }

    if (!guard_only) {
        cg->imm_lvn.count = 0;                                               // end is a merge point
    }
}
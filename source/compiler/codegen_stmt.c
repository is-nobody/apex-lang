// source/compiler/codegen_stmt.c
// Statement codegen: declarations, assignments, returns, dispatch
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

            if (x_slot >= 0) {                                               // inherit all tracked flags
                cg->locals.is_number[y_slot]   = cg->locals.is_number[x_slot];
                cg->locals.is_integer[y_slot]  = cg->locals.is_integer[x_slot];
                cg->locals.const_known[y_slot] = cg->locals.const_known[x_slot];
                cg->locals.const_value[y_slot] = cg->locals.const_value[x_slot];
            } else {
                cg->locals.is_number[y_slot]   = false;
                cg->locals.is_integer[y_slot]  = false;
                cg->locals.const_known[y_slot] = false;
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
        double cv;
        if (try_fold_number(cg, node->var_assign.value, &cv)) {              // RHS is a known constant
            cg->locals.const_known[slot] = true;
            cg->locals.const_value[slot] = cv;
        } else {
            cg->locals.const_known[slot] = false;
        }
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
int codegen_assign_expr(CodeGenerator* cg, ASTNode* node, int dest_hint) {
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
                    if (slot >= 0) {
                        cg->locals.is_number[slot] = true;
                        if (cg->locals.const_known[slot])                    // carry the constant forward
                            cg->locals.const_value[slot] += 1.0;
                    }
                    return local_reg;                                        // return local
                }

                if (bin->binary.op == TOKEN_MINUS &&                         // x = x - 1
                    bin->binary.right->type == AST_LITERAL_NUMBER &&         // right is number
                    bin->binary.right->literal_number.number_value == 1.0) {
                    emit(cg, INST(OP_DEC, local_reg, 0, 0), node->line);     // in-place decrement
                    if (slot >= 0) {
                        cg->locals.is_number[slot] = true;
                        if (cg->locals.const_known[slot])
                            cg->locals.const_value[slot] -= 1.0;
                    }
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
            double cv;
            if (try_fold_number(cg, node->var_assign.value, &cv)) {
                cg->locals.const_known[slot] = true;
                cg->locals.const_value[slot] = cv;
            } else {
                cg->locals.const_known[slot] = false;
            }
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

// recognizes `return f(args)` where f resolves to the function currently being compiled
static bool try_tail_call(CodeGenerator* cg, ASTNode* call) {
    if (cg->current_function < 0) return false;
    if (cg->chunk->functions[cg->current_function].is_async) return false;

    ASTNode* callee = call->call.callee;
    if (!callee || callee->type != AST_IDENTIFIER) return false;
    const char* callee_name = callee->identifier.name;

    int target_idx = -1;
    if (cg->current_module) {                                  // qualified first
        char qualified[512];
        snprintf(qualified, sizeof(qualified), "%s.%s",
                 cg->current_module, callee_name);
        for (int i = 0; i < cg->chunk->func_count; i++) {
            if (strcmp(cg->chunk->functions[i].name, qualified) == 0) {
                target_idx = i;
                break;
            }
        }
    }
    if (target_idx < 0) {                                      // then bare
        for (int i = 0; i < cg->chunk->func_count; i++) {
            if (strcmp(cg->chunk->functions[i].name, callee_name) == 0) {
                target_idx = i;
                break;
            }
        }
    }
    if (target_idx != cg->current_function) return false;

    int param_count = cg->chunk->functions[target_idx].arity;
    if (call->call.arguments->count != param_count) return false;
    if (param_count > 64) return false;

    // force every argument into its own fresh temp
    int arg_temps[64];
    for (int i = 0; i < param_count; i++) {
        int t = alloc_register(cg);
        codegen_expression_into(cg, call->call.arguments->nodes[i], t);
        arg_temps[i] = t;
    }

    for (int i = 0; i < param_count; i++) {                    // into param slots
        if (arg_temps[i] != i) {
            emit(cg, INST(OP_MOVE, i, arg_temps[i], 0), call->line);
        }
    }
    for (int i = 0; i < param_count; i++) {
        free_register(cg, arg_temps[i]);
    }

    int entry = cg->chunk->functions[target_idx].address;
    emit(cg, INST(OP_JUMP, entry, 0, 0), call->line);
    mark_jump_target(cg, entry);
    return true;
}

// emits a return statement with optional value
static void codegen_return(CodeGenerator* cg, ASTNode* node) {
    // inlined body: route the return into the inliner's destination register
    // and jump to the inline exit instead of emitting OP_RETURN
    if (cg->inline_result_reg >= 0) {
        if (node->return_stmt.value) {
            int v = codegen_expression_into(cg, node->return_stmt.value,
                                            cg->inline_result_reg);
            if (v != cg->inline_result_reg) {
                emit(cg, INST(OP_MOVE, cg->inline_result_reg, v, 0), node->line);
                free_register(cg, v);
            }
        } else {
            emit(cg, INST(OP_LOAD_NONE, cg->inline_result_reg, 0, 0), node->line);
        }
        int jump_idx = bytecode_emit_line(cg->chunk, INST(OP_JUMP, 0, 0, 0), node->line);
        if (cg->inline_exit_count >= cg->inline_exit_capacity) {
            cg->inline_exit_capacity = cg->inline_exit_capacity == 0
                                       ? 8 : cg->inline_exit_capacity * 2;
            cg->inline_exits = (int*)realloc(cg->inline_exits,
                                             sizeof(int) * cg->inline_exit_capacity);
        }
        cg->inline_exits[cg->inline_exit_count++] = jump_idx;
        return;
    }

    if (node->return_stmt.value) {                                           // has return value
        // tail-call: `return f(args)` where f is the current function
        if (node->return_stmt.value->type == AST_CALL &&
            try_tail_call(cg, node->return_stmt.value)) {
            return;
        }

        double folded;                                                       // folded numeric value
        if (try_fold_number(cg, node->return_stmt.value, &folded) &&         // folds to a compile-time constant
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

// emits an expression statement, discarding the result
static void codegen_expr_statement(CodeGenerator* cg, ASTNode* node) {
    int result_reg = codegen_expression(cg, node->expr_stmt.expression);     // evaluate expression
    free_register(cg, result_reg);                                           // discard result
}

// statement dispatcher that routes each ast node type to its codegen function
void codegen_statement(CodeGenerator* cg, ASTNode* node) {
    if (!node) return;                                                       // guard against null
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

// true when control does not fall through past this statement: a direct return/break/continue
bool stmt_always_exits(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_RETURN_STMT:
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            return true;
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++) {
                if (stmt_always_exits(node->block.statements->nodes[i])) return true;
            }
            return false;
        case AST_IF_STMT: {
            ASTNode* cur = node;
            while (cur) {
                if (!stmt_always_exits(cur->if_stmt.then_branch)) return false;
                if (cur->if_stmt.elif_chain) {
                    cur = cur->if_stmt.elif_chain;
                } else {
                    return stmt_always_exits(cur->if_stmt.else_branch);
                }
            }
            return false;
        }
        default:
            return false;
    }
}

// emits a block of statements sequentially, resetting temps between them
void codegen_block(CodeGenerator* cg, ASTNode* node) {
    if (!node || (node->type != AST_BLOCK && node->type != AST_PROGRAM)) return;  // validate
    int start_pc = bytecode_current_offset(cg->chunk);                       // block start for dce
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
        if (stmt_always_exits(stmt)) break;                                  // rest of block is dead
        int reset_to = locals_high_water(cg);                                // keep locals + pinned
        if (reset_to < cg->register_floor) reset_to = cg->register_floor;    // respect floor
        if (reset_to < cg->cache_floor) reset_to = cg->cache_floor;          // respect cache pins
        if (cg->next_register > reset_to) {                                  // drop temps only
            cg->next_register = reset_to;                                    // reclaim for next stmt
        }
    }
    cg->block_depth--;
    // bytecode-level local dce: only runs when control cannot fall through
    if (cg->loop_depth == 0) {                                               // loops need liveness
        dce_local_range(cg, start_pc, cg->chunk->code_count);                // kill dead stores
    }
}
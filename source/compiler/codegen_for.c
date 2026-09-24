// source/compiler/codegen_for.c
// For-loop codegen: numeric range and table iteration
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// true when `cond` reads any name that `body` assigns; conservative on calls
static bool cond_reads_assigned(ASTNode* cond, ASTNode* body) {
    if (!cond || !body) return true;                            // conservative
    switch (cond->type) {
        case AST_LITERAL_NUMBER:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
            return false;
        case AST_IDENTIFIER:
            return body_assigns_name(body, cond->identifier.name);
        case AST_BINARY:
            return cond_reads_assigned(cond->binary.left, body) ||
                   cond_reads_assigned(cond->binary.right, body);
        case AST_UNARY:
            return cond_reads_assigned(cond->unary.operand, body);
        case AST_INDEX_ACCESS:
            return cond_reads_assigned(cond->access.object, body) ||
                   cond_reads_assigned(cond->access.member, body);
        case AST_TERNARY:
            return cond_reads_assigned(cond->ternary.condition, body) ||
                   cond_reads_assigned(cond->ternary.true_expr, body) ||
                   cond_reads_assigned(cond->ternary.false_expr, body);
        case AST_STRING_INTERP:
            for (int i = 0; i < cond->string_interp.parts->count; i++) {
                if (cond_reads_assigned(cond->string_interp.parts->nodes[i], body)) return true;
            }
            return false;
        default:
            return true;                                        // calls, awaits, unknown: reject
    }
}

// true when `cond` is loop-invariant: reads neither the loop variable nor anything the body assigns
static bool cond_is_loop_invariant(ASTNode* cond, ASTNode* body, const char* loop_var) {
    if (!cond) return false;
    if (loop_var && ast_references_local(cond, loop_var)) return false;
    if (cond_reads_assigned(cond, body)) return false;
    return true;
}

// finds the first top-level if whose condition is loop-invariant; returns index or -1
static int find_unswitchable_if(ASTNode* body, const char* loop_var) {
    if (!body || (body->type != AST_BLOCK && body->type != AST_PROGRAM)) return -1;
    if (body_unsafe_for_licm(body)) return -1;                  // calls, awaits, indexed assigns
    if (body->block.statements->count - 1 > 6) return -1;       // code-bloat cap on pre+post
    for (int i = 0; i < body->block.statements->count; i++) {
        ASTNode* stmt = body->block.statements->nodes[i];
        if (!stmt || stmt->type != AST_IF_STMT) continue;
        if (stmt->if_stmt.elif_chain) continue;                 // elif chains not supported
        if (!cond_is_loop_invariant(stmt->if_stmt.condition, body, loop_var)) continue;
        return i;
    }
    return -1;
}

// builds a statement list = pre + branch content + post, dropping the if itself
static ASTNodeList* build_split_stmts(ASTNode* body, int if_idx, ASTNode* branch) {
    ASTNodeList* result = ast_list_create();
    ASTNodeList* src = body->block.statements;
    for (int i = 0; i < src->count; i++) {
        if (i == if_idx) {
            if (branch && (branch->type == AST_BLOCK || branch->type == AST_PROGRAM)) {
                ASTNodeList* bl = branch->block.statements;
                for (int j = 0; j < bl->count; j++) ast_list_add(result, bl->nodes[j]);
            }
        } else {
            ast_list_add(result, src->nodes[i]);
        }
    }
    return result;
}

// emits two duplicated loops guarded by the invariant condition
static void emit_unswitched_for(CodeGenerator* cg, ASTNode* node, int if_idx) {
    ASTNode* body = node->for_stmt.body;
    ASTNodeList* orig = body->block.statements;
    ASTNode* if_node = orig->nodes[if_idx];
    ASTNode* cond = if_node->if_stmt.condition;
    ASTNode* then_br = if_node->if_stmt.then_branch;
    ASTNode* else_br = if_node->if_stmt.else_branch;

    ASTNodeList* then_stmts = build_split_stmts(body, if_idx, then_br);

    bool else_is_empty = (else_br == NULL) && (if_idx == 0) && (orig->count == 1);

    LocalNumSnap pre = snap_numbers(cg);
    int saved_num = cg->num_cache.count;
    int saved_str = cg->str_cache.count;

    int jump_false = codegen_optimized_condition(cg, cond, if_node->line);   // invariant: evaluate once
    if (jump_false < 0) {                                                    // fallback: non-comparison condition
        int cond_reg = codegen_expression(cg, cond);
        jump_false = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), if_node->line);
        free_register(cg, cond_reg);
    }

    body->block.statements = then_stmts;                        // temporary body swap
    cg->unswitch_depth++;
    codegen_for_statement(cg, node);
    cg->unswitch_depth--;
    body->block.statements = orig;                              // restore

    if (else_is_empty) {                                        // no else: fall through
        PATCH_JUMP(cg, jump_false, bytecode_current_offset(cg->chunk));
        free_snap(pre);
        return;
    }

    ASTNodeList* else_stmts = build_split_stmts(body, if_idx, else_br);

    int jump_end = bytecode_current_offset(cg->chunk);
    emit(cg, INST(OP_JUMP, 0, 0, 0), if_node->line);            // skip else-loop

    PATCH_JUMP(cg, jump_false, bytecode_current_offset(cg->chunk));

    cg->num_cache.count = saved_num;                            // else path: drop then-side caches
    cg->str_cache.count = saved_str;
    restore_numbers(cg, pre);

    body->block.statements = else_stmts;                        // temporary body swap
    cg->unswitch_depth++;
    codegen_for_statement(cg, node);
    cg->unswitch_depth--;
    body->block.statements = orig;

    PATCH_JUMP(cg, jump_end, bytecode_current_offset(cg->chunk));

    free_snap(pre);
}

// true when any statement in `node` is a `continue` that targets the current loop
static bool body_has_continue(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_CONTINUE_STMT: return true;
        case AST_FOR_STMT:      return false;                   // nested loop's continue targets itself
        case AST_FUNCTION_DECL: return false;                   // nested function has own control flow
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++) {
                if (body_has_continue(node->block.statements->nodes[i])) return true;
            }
            return false;
        case AST_IF_STMT:
            return body_has_continue(node->if_stmt.then_branch) ||
                   body_has_continue(node->if_stmt.elif_chain) ||
                   body_has_continue(node->if_stmt.else_branch);
        case AST_MATCH_STMT:
            if (node->match_stmt.cases) {
                for (int i = 0; i < node->match_stmt.cases->count; i++) {
                    if (body_has_continue(node->match_stmt.cases->nodes[i])) return true;
                }
            }
            return body_has_continue(node->match_stmt.default_case);
        case AST_CASE:
            return body_has_continue(node->case_stmt.body);
        default:
            return false;
    }
}

// collects unique `loop_var OP c` candidates; deduped by (op, value)
static void collect_iv_candidates(CodeGenerator* cg, ASTNode* node, const char* loop_var,
                                   Opcode* ops, double* vals, int* count, int max) {
    if (!node || *count >= max) return;
    if (node->type == AST_BINARY) {
        ApexTokenType bop = node->binary.op;
        Opcode red = (bop == TOKEN_STAR) ? OP_MUL :
                     (bop == TOKEN_PLUS) ? OP_ADD : OP_MOVE;
        if (red != OP_MOVE) {
            ASTNode* cn = NULL;
            if (node->binary.left->type == AST_IDENTIFIER &&
                strcmp(node->binary.left->identifier.name, loop_var) == 0) {
                cn = node->binary.right;
            } else if (node->binary.right->type == AST_IDENTIFIER &&
                       strcmp(node->binary.right->identifier.name, loop_var) == 0) {
                cn = node->binary.left;
            }
            if (cn) {
                double cv;
                if (try_fold_number(cg, cn, &cv)) {
                    bool dup = false;
                    for (int k = 0; k < *count; k++) {
                        if (ops[k] == red && vals[k] == cv) { dup = true; break; }
                    }
                    if (!dup) {
                        ops[*count] = red;
                        vals[*count] = cv;
                        (*count)++;
                    }
                }
            }
        }
        collect_iv_candidates(cg, node->binary.left,  loop_var, ops, vals, count, max);
        collect_iv_candidates(cg, node->binary.right, loop_var, ops, vals, count, max);
        return;
    }
    switch (node->type) {
        case AST_UNARY:
            collect_iv_candidates(cg, node->unary.operand, loop_var, ops, vals, count, max);
            break;
        case AST_CALL:
            for (int i = 0; i < node->call.arguments->count; i++)
                collect_iv_candidates(cg, node->call.arguments->nodes[i], loop_var, ops, vals, count, max);
            break;
        case AST_INDEX_ACCESS:
            collect_iv_candidates(cg, node->access.object, loop_var, ops, vals, count, max);
            collect_iv_candidates(cg, node->access.member, loop_var, ops, vals, count, max);
            break;
        case AST_TERNARY:
            collect_iv_candidates(cg, node->ternary.condition, loop_var, ops, vals, count, max);
            collect_iv_candidates(cg, node->ternary.true_expr, loop_var, ops, vals, count, max);
            collect_iv_candidates(cg, node->ternary.false_expr, loop_var, ops, vals, count, max);
            break;
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                collect_iv_candidates(cg, node->string_interp.parts->nodes[i], loop_var, ops, vals, count, max);
            break;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                collect_iv_candidates(cg, node->table_literal.items->nodes[i], loop_var, ops, vals, count, max);
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                collect_iv_candidates(cg, kv->binary.left,  loop_var, ops, vals, count, max);
                collect_iv_candidates(cg, kv->binary.right, loop_var, ops, vals, count, max);
            }
            break;
        case AST_ASSIGN:
        case AST_VAR_DECL:
            collect_iv_candidates(cg, node->var_assign.value, loop_var, ops, vals, count, max);
            break;
        case AST_EXPR_STMT:
            collect_iv_candidates(cg, node->expr_stmt.expression, loop_var, ops, vals, count, max);
            break;
        case AST_RETURN_STMT:
            collect_iv_candidates(cg, node->return_stmt.value, loop_var, ops, vals, count, max);
            break;
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->block.statements->count; i++)
                collect_iv_candidates(cg, node->block.statements->nodes[i], loop_var, ops, vals, count, max);
            break;
        case AST_IF_STMT:
            collect_iv_candidates(cg, node->if_stmt.condition, loop_var, ops, vals, count, max);
            collect_iv_candidates(cg, node->if_stmt.then_branch, loop_var, ops, vals, count, max);
            collect_iv_candidates(cg, node->if_stmt.elif_chain, loop_var, ops, vals, count, max);
            collect_iv_candidates(cg, node->if_stmt.else_branch, loop_var, ops, vals, count, max);
            break;
        case AST_FOR_STMT:
            break;                                              // nested loop has own induction var context
        case AST_MATCH_STMT:
            collect_iv_candidates(cg, node->match_stmt.subject, loop_var, ops, vals, count, max);
            if (node->match_stmt.cases) {
                for (int i = 0; i < node->match_stmt.cases->count; i++)
                    collect_iv_candidates(cg, node->match_stmt.cases->nodes[i], loop_var, ops, vals, count, max);
            }
            collect_iv_candidates(cg, node->match_stmt.default_case, loop_var, ops, vals, count, max);
            break;
        case AST_CASE:
            collect_iv_candidates(cg, node->case_stmt.body, loop_var, ops, vals, count, max);
            break;
        default:
            break;
    }
}

// emits for loops, supporting both range loops and table iteration
void codegen_for_statement(CodeGenerator* cg, ASTNode* node) {
    // zero-trip range loop: body never runs, so drop the whole statement
    if (for_is_zero_trip(cg, node)) return;

    // loop unswitching: hoist an invariant `if` out of the loop body
    if (cg->unswitch_depth == 0 && node->for_stmt.var_name) {
        int if_idx = find_unswitchable_if(node->for_stmt.body, node->for_stmt.var_name);
        if (if_idx >= 0) {
            emit_unswitched_for(cg, node, if_idx);
            return;
        }
    }

    // the body and the (re-evaluated) condition see the loop's per-iteration value
    invalidate_loop_consts(cg, node->for_stmt.body, node->for_stmt.var_name);

    // save outer iv_reduce state (nested loops may shadow the loop var)
    const char* prev_iv_var = cg->iv_reduce.loop_var;
    int prev_iv_count = cg->iv_reduce.count;
    struct { Opcode op; double value; int reg; } prev_iv_entries[8];
    memcpy(prev_iv_entries, cg->iv_reduce.entries, sizeof(prev_iv_entries));
    cg->iv_reduce.loop_var = NULL;
    cg->iv_reduce.count = 0;

    int prev_break_count = cg->loop_stack.break_count;
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

            cg->imm_lvn.count = 0;                                                 // body: back-edge merge point
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

            // iv strength reduction: analyze body FIRST so constants consumed by
            // IV accumulators aren't re-hoisted as dead registers below
            int iv_count = 0;
            double iv_step = 1.0;
            Opcode iv_ops[8];
            double iv_vals[8];
            if (!body_assigns_name(node->for_stmt.body, node->for_stmt.var_name) &&
                !body_has_continue(node->for_stmt.body)) {
                bool step_const = true;
                if (node->for_stmt.step) {
                    if (!try_fold_number(cg, node->for_stmt.step, &iv_step)) step_const = false;
                }
                if (step_const) {
                    collect_iv_candidates(cg, node->for_stmt.body, node->for_stmt.var_name,
                                          iv_ops, iv_vals, &iv_count, 8);
                }
            }

            collect_hoistable_numbers(cg, node->for_stmt.body);                     // scan body for constants

            // drop hoist entries whose value went into an IV accumulator
            if (iv_count > 0 && cg->hoist.count > 0) {
                int w = 0;
                for (int r = 0; r < cg->hoist.count; r++) {
                    bool consumed = false;
                    for (int k = 0; k < iv_count; k++) {
                        if (cg->hoist.values[r] == iv_vals[k]) { consumed = true; break; }
                    }
                    if (!consumed) {
                        if (w != r) cg->hoist.values[w] = cg->hoist.values[r];
                        w++;
                    }
                }
                cg->hoist.count = w;
            }

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

            // emit IV accumulator initializers (analysis done above)
            for (int i = 0; i < iv_count; i++) {
                int acc = alloc_register(cg);
                Opcode red = iv_ops[i];
                double c = iv_vals[i];
                if (c == (int)c && c >= 0 && c <= 65535) {                          // fits in immediate
                    Opcode iop = (red == OP_MUL) ? OP_MUL_IMM : OP_ADD_IMM;
                    emit(cg, INST(iop, acc, var_reg, (int)c), node->line);
                } else {                                                            // full double constant
                    int cidx = bytecode_add_number_constant(cg->chunk, c);
                    int creg = alloc_register(cg);
                    emit(cg, INST(OP_LOAD_NUM, creg, cidx, 0), node->line);
                    emit(cg, INST(red, acc, var_reg, creg), node->line);
                    free_register(cg, creg);
                }
                cg->iv_reduce.entries[i].op    = red;
                cg->iv_reduce.entries[i].value = c;
                cg->iv_reduce.entries[i].reg   = acc;
            }
            if (iv_count > 0) {
                cg->iv_reduce.loop_var = node->for_stmt.var_name;
                cg->iv_reduce.count    = iv_count;
            }

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

            cg->imm_lvn.count = 0;                                                  // body: back-edge merge point

            int prev_loop_floor = cg->register_floor;                               // save floor
            if (cg->hoist.count > 0 || cg->hoist.get_count > 0 || iv_count > 0) {   // pin hoisted + iv regs
                int highest = 0;                                                    // highest hoisted reg
                for (int i = 0; i < cg->hoist.count; i++) {
                    if (cg->hoist.regs[i] > highest) highest = cg->hoist.regs[i];
                }
                for (int i = 0; i < cg->hoist.get_count; i++) {
                    if (cg->hoist.get_regs[i] > highest) highest = cg->hoist.get_regs[i];
                }
                for (int i = 0; i < iv_count; i++) {
                    if (cg->iv_reduce.entries[i].reg > highest) highest = cg->iv_reduce.entries[i].reg;
                }
                cg->register_floor = highest + 1;                                   // body temps start above
            }

            codegen_block(cg, node->for_stmt.body);                                 // emit body
            cg->register_floor = prev_loop_floor;                                   // restore floor after body

            if (iv_count > 0) {                                                     // iv strength reduction: advance accumulators
                for (int i = 0; i < iv_count; i++) {
                    Opcode red = cg->iv_reduce.entries[i].op;
                    double c   = cg->iv_reduce.entries[i].value;
                    int acc    = cg->iv_reduce.entries[i].reg;
                    double inc = (red == OP_MUL) ? c * iv_step : iv_step;
                    if (inc == (int)inc && inc >= 0 && inc <= 65535) {              // fits in immediate
                        emit(cg, INST(OP_ADD_IMM, acc, acc, (int)inc), node->line);
                    } else {                                                        // full double constant
                        int cidx = bytecode_add_number_constant(cg->chunk, inc);
                        int creg = alloc_register(cg);
                        emit(cg, INST(OP_LOAD_NUM, creg, cidx, 0), node->line);
                        emit(cg, INST(OP_ADD, acc, acc, creg), node->line);
                        free_register(cg, creg);
                    }
                }
                cg->iv_reduce.loop_var = NULL;                                      // deactivate before register reset
                cg->iv_reduce.count    = 0;
            }

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
                if (has_imm && try_fold_number(cg, condition->binary.right, &imm_val_for_cond) &&
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

        cg->imm_lvn.count = 0;                                                      // loop top: back-edge merge point

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

    cg->imm_lvn.count = 0;                                                          // loop exit is a merge point

    cg->iv_reduce.loop_var = prev_iv_var;                                           // restore outer iv_reduce
    cg->iv_reduce.count    = prev_iv_count;
    memcpy(cg->iv_reduce.entries, prev_iv_entries, sizeof(prev_iv_entries));
}
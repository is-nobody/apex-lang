// source/compiler/codegen_fn.c
// Function declaration codegen: frame setup, locals pre-declaration, prologue
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// emits a function declaration with proper body compilation and state isolation
void codegen_function_decl(CodeGenerator* cg, ASTNode* node) {
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

    if (func_idx >= cg->fn_decls_cap) {                                      // grow AST table on demand
        int new_cap = cg->fn_decls_cap == 0 ? 16 : cg->fn_decls_cap;         // start / keep current
        while (new_cap <= func_idx) new_cap *= 2;                            // double until fits
        cg->fn_decls = (ASTNode**)realloc(cg->fn_decls,
                                          sizeof(ASTNode*) * new_cap);
        for (int i = cg->fn_decls_cap; i < new_cap; i++) cg->fn_decls[i] = NULL;  // zero new slots
        cg->fn_decls_cap = new_cap;
    }
    cg->fn_decls[func_idx] = node;                                           // keep AST for inlining

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
    bool* saved_const_known = NULL;                                          // saved const-known flags
    double* saved_const_value = NULL;                                        // saved const values

    if (saved_count > 0) {                                                   // have locals
        saved_names = (char**)malloc(sizeof(char*) * saved_count);           // allocate names
        saved_regs = (int*)malloc(sizeof(int) * saved_count);                // allocate regs
        saved_is_number = (bool*)malloc(sizeof(bool) * saved_count);         // allocate flags
        saved_is_integer = (bool*)malloc(sizeof(bool) * saved_count);        // allocate int flags
        saved_const_known = (bool*)malloc(sizeof(bool) * saved_count);       // allocate const flags
        saved_const_value = (double*)malloc(sizeof(double) * saved_count);   // allocate const values
        for (int i = 0; i < saved_count; i++) {                              // copy locals
            saved_names[i] = strdup(cg->locals.names[i]);                    // copy name
            saved_regs[i] = cg->locals.registers[i];                         // copy reg
            saved_is_number[i] = cg->locals.is_number[i];                    // copy numeric flag
            saved_is_integer[i] = cg->locals.is_integer[i];                  // copy integer flag
            saved_const_known[i] = cg->locals.const_known[i];                // copy const flag
            saved_const_value[i] = cg->locals.const_value[i];                // copy const value
        }
    }

    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);    // free local names
    free(cg->locals.names);                                                  // free names array
    free(cg->locals.registers);                                              // free registers array
    free(cg->locals.is_number);                                              // free numeric flags array
    free(cg->locals.is_integer);                                             // free integer flags array
    free(cg->locals.const_known);                                            // free const-known array
    free(cg->locals.const_value);                                            // free const-value array
    cg->locals.names = NULL;                                                 // clear names
    cg->locals.registers = NULL;                                             // clear regs
    cg->locals.is_number = NULL;                                             // clear flags
    cg->locals.is_integer = NULL;                                            // clear int flags
    cg->locals.const_known = NULL;                                           // clear const flags
    cg->locals.const_value = NULL;                                           // clear const values
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
    cg->imm_lvn.count = 0;                                                   // fresh LVN cache

    for (int i = 0; i < param_count; i++) {                                 // parameters
        ASTNode* param = node->function_decl.params->nodes[i];              // param node
        add_local(cg, param->param.name);                                   // add as local
    }

    // pre-declare locals so assignments inside match/if/for bind to locals, not globals
    // parser guarantees every local is assigned before its first read
    collect_local_names(cg, node->function_decl.body);                      // scan body for assigned names

    // linear-scan register allocation: reuse registers for disjoint live ranges
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

    codegen_block(cg, node->function_decl.body);

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
    free(cg->locals.const_known);                                            // free const-known array
    free(cg->locals.const_value);                                            // free const-value array

    cg->locals.names = saved_names;                                          // restore names
    cg->locals.registers = saved_regs;                                       // restore regs
    cg->locals.is_number = saved_is_number;                                  // restore numeric flags
    cg->locals.is_integer = saved_is_integer;                                // restore integer flags
    cg->locals.const_known = saved_const_known;                              // restore const flags
    cg->locals.const_value = saved_const_value;                              // restore const values
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

    // the nested function's body sits behind an op_jump in the instruction stream
    cg->imm_lvn.count = 0;

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

    int global_idx = bytecode_get_global(cg->chunk, gname);
    if (global_idx < 0) global_idx = bytecode_add_global(cg->chunk, gname);

    int temp_reg = alloc_register(cg);
    emit(cg, INST(OP_LOAD_CONST, temp_reg, func_const_idx, 0), node->line);
    emit(cg, INST(OP_STORE_GLOBAL, temp_reg, global_idx, 0), node->line);
    free_register(cg, temp_reg);
}
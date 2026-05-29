#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== Forward Declarations ==========
static int codegen_expression(CodeGenerator* cg, ASTNode* node);
static void codegen_statement(CodeGenerator* cg, ASTNode* node);
static void codegen_block(CodeGenerator* cg, ASTNode* node);
static int codegen_string_interp(CodeGenerator* cg, ASTNode* node);
static int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line);

// ========== Helpers ==========

static int alloc_register(CodeGenerator* cg) {
    int reg = cg->next_register++;
    if (reg >= cg->max_registers) {
        cg->max_registers = reg + 1;
    }
    return reg;
}

static void free_register(CodeGenerator* cg, int reg) {
    // PROTECTION: Never free registers occupied by local variables!
    for (int i = 0; i < cg->locals.count; i++) {
        if (cg->locals.registers[i] == reg) {
            return; // This is a local variable — don't touch it
        }
    }
    
    // Simplified: only decrement counter if this is the last temp register
    if (reg == cg->next_register - 1) {
        cg->next_register--;
    }
}

static int make_label(CodeGenerator* cg) {
    return cg->label_counter++;
}

// Local variable management

static int find_local(CodeGenerator* cg, const char* name) {
    for (int i = 0; i < cg->locals.count; i++) {
        if (strcmp(cg->locals.names[i], name) == 0) {
            return cg->locals.registers[i];
        }
    }
    return -1;
}

static int add_local(CodeGenerator* cg, const char* name) {
    if (cg->locals.count >= cg->locals.capacity) {
        cg->locals.capacity = cg->locals.capacity == 0 ? 16 : cg->locals.capacity * 2;
        cg->locals.names = (char**)realloc(cg->locals.names, 
                                            sizeof(char*) * cg->locals.capacity);
        cg->locals.registers = (int*)realloc(cg->locals.registers,
                                              sizeof(int) * cg->locals.capacity);
    }
    
    int reg = alloc_register(cg);
    cg->locals.names[cg->locals.count] = strdup(name);
    cg->locals.registers[cg->locals.count] = reg;
    cg->locals.count++;
    
    return reg;
}

// Emit with line number info
static int emit(CodeGenerator* cg, Instruction inst, int line) {
    return bytecode_emit_line(cg->chunk, inst, line);
}

// ========== Code Generator ==========

CodeGenerator* codegen_create(BytecodeChunk* chunk, SemAnalyzer* sema) {
    CodeGenerator* cg = (CodeGenerator*)calloc(1, sizeof(CodeGenerator));
    cg->chunk = chunk;
    cg->sema = sema;
    cg->next_register = 0;
    cg->max_registers = 0;
    cg->current_function = -1;
    cg->label_counter = 0;
    cg->try_depth = 0;
    
    // Preload frequently used constants
    cg->cache.zero_reg = alloc_register(cg);
    int zero_idx = bytecode_add_number_constant(chunk, 0.0);
    emit(cg, INST(OP_LOAD_CONST, cg->cache.zero_reg, zero_idx, 0), 0);
    
    cg->cache.one_reg = alloc_register(cg);
    int one_idx = bytecode_add_number_constant(chunk, 1.0);
    emit(cg, INST(OP_LOAD_CONST, cg->cache.one_reg, one_idx, 0), 0);
    
    cg->cache.empty_str = alloc_register(cg);
    int empty_idx = bytecode_add_string_constant(chunk, "");
    emit(cg, INST(OP_LOAD_CONST, cg->cache.empty_str, empty_idx, 0), 0);
    
    return cg;
}

void codegen_destroy(CodeGenerator* cg) {
    if (!cg) return;
    
    for (int i = 0; i < cg->locals.count; i++) {
        free(cg->locals.names[i]);
    }
    free(cg->locals.names);
    free(cg->locals.registers);
    
    free(cg->loop_stack.break_addrs);
    free(cg->loop_stack.continue_addrs);
    
    free(cg);
}

// ========== Expression Code Generation ==========

static int codegen_literal_number(CodeGenerator* cg, ASTNode* node) {
    double val = node->literal_number.number_value;
    
    // Small integers stored as immediate in the instruction
    if (val == (int)val && val >= 0 && val <= 255) {
        int reg = alloc_register(cg);
        emit(cg, INST(OP_LOAD_CONST_NUM, reg, (int)val, 0), node->line);
        return reg;
    }
    
    int reg = alloc_register(cg);
    int const_idx = bytecode_add_number_constant(cg->chunk, val);
    emit(cg, INST(OP_LOAD_CONST, reg, const_idx, 0), node->line);
    return reg;
}

static int codegen_literal_string(CodeGenerator* cg, ASTNode* node) {
    int reg = alloc_register(cg);
    int const_idx = bytecode_add_string_constant(cg->chunk, 
                                                  node->literal_string.string_value);
    emit(cg, INST(OP_LOAD_CONST, reg, const_idx, 0), node->line);
    return reg;
}

static int codegen_literal_bool(CodeGenerator* cg, ASTNode* node) {
    int reg = alloc_register(cg);
    if (node->literal_bool.bool_value) {
        emit(cg, INST(OP_LOAD_BOOL, reg, 1, 0), node->line);
    } else {
        emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), node->line);
    }
    return reg;
}

static int codegen_literal_none(CodeGenerator* cg, ASTNode* node) {
    int reg = alloc_register(cg);
    emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), node->line);
    return reg;
}

static int codegen_identifier(CodeGenerator* cg, ASTNode* node) {
    const char* name = node->identifier.name;
    int local_reg = find_local(cg, name);
    
    if (local_reg >= 0) {
        // === CRITICAL OPTIMIZATION ===
        // Simply return the local register index.
        // No MOVE or extra allocation needed.
        // Safe: free_register() will ignore this register during cleanup
        // since it's not at the top of the temp register stack.
        return local_reg; 
    }
    
    // For globals we must read into a new register
    int global_idx = bytecode_get_global(cg->chunk, name);
    if (global_idx < 0) {
        global_idx = bytecode_add_global(cg->chunk, name);
    }
    int reg = alloc_register(cg);
    emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
    return reg;
}

static int codegen_call(CodeGenerator* cg, ASTNode* node) {
    // Get argument list (ASTNodeList*, not ASTNode*)
    ASTNodeList* args_list = node->call.arguments;
    int arg_count = args_list->count;
    int* arg_regs = NULL;

    // Allocate and generate code for each argument
    if (arg_count > 0) {
        arg_regs = (int*)malloc(sizeof(int) * arg_count);
        for (int i = 0; i < arg_count; i++) {
            // Access nodes via the nodes array
            arg_regs[i] = codegen_expression(cg, args_list->nodes[i]);
        }
    }

    // Get function name
    char func_name[256] = "";
    ASTNode* callee = node->call.callee;
    if (callee->type == AST_IDENTIFIER) {
        strcpy(func_name, callee->identifier.name);
    } else if (callee->type == AST_MEMBER_ACCESS) {
        ASTNode* obj = callee;
        char full_name[1024] = "";
        char parts[32][256];
        int part_count = 0;
        while (obj->type == AST_MEMBER_ACCESS) {
            if (obj->access.member->type == AST_IDENTIFIER) {
                strcpy(parts[part_count++], obj->access.member->identifier.name);
            }
            obj = obj->access.object;
        }
        if (obj->type == AST_IDENTIFIER) {
            strcpy(parts[part_count++], obj->identifier.name);
        }
        for (int i = part_count - 1; i >= 0; i--) {
            strcat(full_name, parts[i]);
            if (i > 0) strcat(full_name, ".");
        }
        strcpy(func_name, full_name);
    }

    int result_reg = alloc_register(cg);

    // Push arguments onto the stack
    for (int i = 0; i < arg_count; i++) {
        emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
    }

    // Check for built-in functions
    static const char* builtins[] = {
        "os.output", "os.input", "number", "string",
        "string.len", "math.abs", "math.sqrt", "table.size",
        NULL
    };
    bool is_builtin = false;
    for (int i = 0; builtins[i] != NULL; i++) {
        if (strcmp(func_name, builtins[i]) == 0) {
            is_builtin = true;
            break;
        }
    }

    if (is_builtin) {
        int name_idx = bytecode_add_string_constant(cg->chunk, func_name);
        emit(cg, INST(OP_CALL_BUILTIN, result_reg, name_idx, arg_count), node->line);
    } else {
        // Look up user-defined function
        int func_addr = -1;
        for (int i = 0; i < cg->chunk->func_count; i++) {
            if (strcmp(cg->chunk->functions[i].name, func_name) == 0) {
                func_addr = cg->chunk->functions[i].address;
                break;
            }
        }
        if (func_addr >= 0) {
            emit(cg, INST(OP_CALL, result_reg, func_addr, arg_count), node->line);
        } else {
            // Fallback to builtin call
            int name_idx = bytecode_add_string_constant(cg->chunk, func_name);
            emit(cg, INST(OP_CALL_BUILTIN, result_reg, name_idx, arg_count), node->line);
        }
    }

    // Cleanup
    if (arg_regs) {
        for (int i = 0; i < arg_count; i++) {
            free_register(cg, arg_regs[i]);
        }
        free(arg_regs);
    }
    return result_reg;
}

static int codegen_string_interp(CodeGenerator* cg, ASTNode* node) {
    if (node->string_interp.parts->count == 0) {
        int reg = alloc_register(cg);
        int empty_idx = bytecode_add_string_constant(cg->chunk, "");
        emit(cg, INST(OP_LOAD_CONST, reg, empty_idx, 0), node->line);
        return reg;
    }
    int result_reg = -1;
    for (int i = 0; i < node->string_interp.parts->count; i++) {
        ASTNode* part = node->string_interp.parts->nodes[i];
        int part_reg;
        if (part->type == AST_LITERAL_STRING) {
            part_reg = codegen_literal_string(cg, part);
        } else {
            int expr_reg = codegen_expression(cg, part);
            part_reg = alloc_register(cg);
            emit(cg, INST(OP_TO_STRING, part_reg, expr_reg, 0), node->line);
            free_register(cg, expr_reg);
        }
        if (result_reg < 0) {
            result_reg = part_reg;
        } else {
            int new_result = alloc_register(cg);
            emit(cg, INST(OP_CONCAT, new_result, result_reg, part_reg), node->line);
            free_register(cg, result_reg);
            free_register(cg, part_reg);
            result_reg = new_result;
        }
    }
    return result_reg;
}

static int codegen_range(CodeGenerator* cg, ASTNode* node) {
    int start_reg = codegen_expression(cg, node->range.start);
    int end_reg = codegen_expression(cg, node->range.end);
    int step_reg = -1;
    
    if (node->range.step) {
        step_reg = codegen_expression(cg, node->range.step);
    } else {
        // Default step = 1
        step_reg = alloc_register(cg);
        int one_idx = bytecode_add_number_constant(cg->chunk, 1.0);
        emit(cg, INST(OP_LOAD_CONST, step_reg, one_idx, 0), node->line);
    }
    
    // Create iterator: table with __start, __end, __step, __current
    int iter_reg = alloc_register(cg);
    emit(cg, INST(OP_NEW_TABLE, iter_reg, 0, 0), node->line);
    
    int start_key = bytecode_add_string_constant(cg->chunk, "__start");
    int end_key = bytecode_add_string_constant(cg->chunk, "__end");
    int step_key = bytecode_add_string_constant(cg->chunk, "__step");
    
    emit(cg, INST(OP_TABLE_SET_CONST, iter_reg, start_key, start_reg), node->line);
    emit(cg, INST(OP_TABLE_SET_CONST, iter_reg, end_key, end_reg), node->line);
    emit(cg, INST(OP_TABLE_SET_CONST, iter_reg, step_key, step_reg), node->line);
    
    free_register(cg, start_reg);
    free_register(cg, end_reg);
    free_register(cg, step_reg);
    
    return iter_reg;
}

static void codegen_for_statement(CodeGenerator* cg, ASTNode* node) {
    // Grow loop stack if needed
    if (cg->loop_stack.count >= cg->loop_stack.capacity) {
        cg->loop_stack.capacity = cg->loop_stack.capacity == 0 ? 16 : cg->loop_stack.capacity * 2;
        cg->loop_stack.break_addrs = (int*)realloc(cg->loop_stack.break_addrs, sizeof(int) * cg->loop_stack.capacity);
        cg->loop_stack.continue_addrs = (int*)realloc(cg->loop_stack.continue_addrs, sizeof(int) * cg->loop_stack.capacity);
        // If you added is_fast to the struct, don't forget to realloc it too
    }

    bool is_fast_loop = false;
    
    // Check if the iterable is a range() call
    if (node->for_stmt.iterable->type == AST_CALL) {
        ASTNode* callee = node->for_stmt.iterable->call.callee;
        if (callee->type == AST_IDENTIFIER && strcmp(callee->identifier.name, "range") == 0) {
            is_fast_loop = true;
        }
    } else if (node->for_stmt.iterable->type == AST_RANGE) {
        is_fast_loop = true;
    }

    if (is_fast_loop) {
        // === FAST NUMERIC LOOP ===
        int start_reg, end_reg, step_reg;
        ASTNodeList* args = NULL;

        if (node->for_stmt.iterable->type == AST_CALL) {
            args = node->for_stmt.iterable->call.arguments;
            if (args->count == 1) {
                start_reg = alloc_register(cg);
                int zero_idx = bytecode_add_number_constant(cg->chunk, 0.0);
                emit(cg, INST(OP_LOAD_CONST, start_reg, zero_idx, 0), node->line);
                end_reg = codegen_expression(cg, args->nodes[0]);
            } else {
                start_reg = codegen_expression(cg, args->nodes[0]);
                end_reg = codegen_expression(cg, args->nodes[1]);
            }
            if (args->count >= 3) {
                step_reg = codegen_expression(cg, args->nodes[2]);
            } else {
                step_reg = alloc_register(cg);
                int one_idx = bytecode_add_number_constant(cg->chunk, 1.0);
                emit(cg, INST(OP_LOAD_CONST, step_reg, one_idx, 0), node->line);
            }
        } else {
            // AST_RANGE (1..10 syntax)
            start_reg = codegen_expression(cg, node->for_stmt.iterable->range.start);
            end_reg = codegen_expression(cg, node->for_stmt.iterable->range.end);
            if (node->for_stmt.iterable->range.step) {
                step_reg = codegen_expression(cg, node->for_stmt.iterable->range.step);
            } else {
                step_reg = alloc_register(cg);
                int one_idx = bytecode_add_number_constant(cg->chunk, 1.0);
                emit(cg, INST(OP_LOAD_CONST, step_reg, one_idx, 0), node->line);
            }
        }

        int var_reg = add_local(cg, node->for_stmt.var_name);
        emit(cg, INST(OP_MOVE, var_reg, start_reg, 0), node->line);
        emit(cg, INST(OP_FOR_INIT, var_reg, end_reg, step_reg), node->line);

        int loop_start = bytecode_current_offset(cg->chunk);
        
        cg->loop_stack.break_addrs[cg->loop_stack.count] = 0;
        cg->loop_stack.continue_addrs[cg->loop_stack.count] = loop_start;
        cg->loop_stack.count++;

        int for_next_instr = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_FOR_NEXT, var_reg, 0, 0), node->line); // op2=0 marks fast loop

        codegen_block(cg, node->for_stmt.body);
        emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);

        int exit_addr = bytecode_current_offset(cg->chunk);
        cg->chunk->code[for_next_instr].operands[1] = exit_addr;
        cg->loop_stack.break_addrs[cg->loop_stack.count - 1] = exit_addr;
        cg->loop_stack.count--;

        free_register(cg, start_reg);
        free_register(cg, end_reg);
        free_register(cg, step_reg);

    } else {
        // === SLOW LOOP (tables) ===
        int iter_reg = codegen_expression(cg, node->for_stmt.iterable);
        int var_reg = add_local(cg, node->for_stmt.var_name);

        emit(cg, INST(OP_FOR_PREP, iter_reg, 0, 0), node->line);

        int loop_start = bytecode_current_offset(cg->chunk);
        
        cg->loop_stack.break_addrs[cg->loop_stack.count] = 0;
        cg->loop_stack.continue_addrs[cg->loop_stack.count] = loop_start;
        cg->loop_stack.count++;

        int for_next_instr = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_FOR_NEXT, var_reg, iter_reg, 0), node->line);

        codegen_block(cg, node->for_stmt.body);
        emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);

        int exit_addr = bytecode_current_offset(cg->chunk);
        cg->chunk->code[for_next_instr].operands[2] = exit_addr; // op2!=0 marks slow loop
        cg->loop_stack.break_addrs[cg->loop_stack.count - 1] = exit_addr;
        cg->loop_stack.count--;

        free_register(cg, iter_reg);
    }
}

static int codegen_expression(CodeGenerator* cg, ASTNode* node) {
    if (!node) {
        int reg = alloc_register(cg);
        emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), node->line);
        return reg;
    }
    
    switch (node->type) {
        case AST_LITERAL_NUMBER: {
            int reg = codegen_literal_number(cg, node);
            return reg;
        }
        case AST_LITERAL_STRING: {
            int reg = codegen_literal_string(cg, node);
            return reg;
        }
        case AST_LITERAL_BOOL: {
            int reg = codegen_literal_bool(cg, node);
            return reg;
        }
        case AST_IDENTIFIER: {
            int reg = codegen_identifier(cg, node);
            return reg;
        }
        
        case AST_BINARY: {
            // Generate left and right operands
            int left_reg = codegen_expression(cg, node->binary.left);
            int right_reg = codegen_expression(cg, node->binary.right);
            int result_reg = alloc_register(cg);
            
            Opcode op;
            switch (node->binary.op) {
                // Arithmetic
                case TOKEN_PLUS:      op = OP_ADD; break;
                case TOKEN_MINUS:     op = OP_SUB; break;
                case TOKEN_STAR:      op = OP_MUL; break;
                case TOKEN_SLASH:     op = OP_DIV; break;
                case TOKEN_PERCENT:   op = OP_MOD; break;
                
                // Comparisons
                case TOKEN_EQUAL_EQUAL:    op = OP_CMP_EQ; break;
                case TOKEN_NOT_EQUAL:      op = OP_CMP_NEQ; break;
                case TOKEN_LESS:           op = OP_CMP_LT; break;
                case TOKEN_GREATER:        op = OP_CMP_GT; break;
                case TOKEN_LESS_EQUAL:     op = OP_CMP_LTE; break;
                case TOKEN_GREATER_EQUAL:  op = OP_CMP_GTE; break;
                
                // Logical
                case TOKEN_AND:       op = OP_AND; break;
                case TOKEN_OR:        op = OP_OR; break;
                
                default:
                    fprintf(stderr, "Error: Unknown binary operator %d\n", node->binary.op);
                    op = OP_ADD; // Fallback
                    break;
            }
            
            emit(cg, INST(op, result_reg, left_reg, right_reg), node->line);
            
            free_register(cg, left_reg);
            free_register(cg, right_reg);
            return result_reg;
        }

        case AST_UNARY: {
            int operand_reg = codegen_expression(cg, node->unary.operand);
            int result_reg = alloc_register(cg);
            
            switch (node->unary.op) {
                case TOKEN_MINUS:
                    emit(cg, INST(OP_NEG, result_reg, operand_reg, 0), node->line);
                    break;
                case TOKEN_NOT:
                    emit(cg, INST(OP_NOT, result_reg, operand_reg, 0), node->line);
                    break;
                default:
                    break;
            }
            
            free_register(cg, operand_reg);
            return result_reg;
        }
        case AST_CALL: {
            int reg = codegen_call(cg, node);
            return reg;
        }
        case AST_MEMBER_ACCESS:
        case AST_INDEX_ACCESS: {
            int obj_reg = codegen_expression(cg, node->access.object);
            int result_reg = alloc_register(cg);
            
            if (node->access.member->type == AST_IDENTIFIER) {
                int key_idx = bytecode_add_string_constant(cg->chunk, 
                                                             node->access.member->identifier.name);
                emit(cg, INST(OP_TABLE_GET_CONST, result_reg, obj_reg, key_idx), node->line);
            } else {
                int key_reg = codegen_expression(cg, node->access.member);
                emit(cg, INST(OP_TABLE_GET, result_reg, obj_reg, key_reg), node->line);
                free_register(cg, key_reg);
            }
            
            free_register(cg, obj_reg);
            return result_reg;
        }
        case AST_TABLE_LITERAL: {
            int table_reg = alloc_register(cg);
            emit(cg, INST(OP_NEW_TABLE, table_reg, 0, 0), node->line);
            
            for (int i = 0; i < node->table_literal.items->count; i++) {
                int value_reg = codegen_expression(cg, node->table_literal.items->nodes[i]);
                emit(cg, INST(OP_TABLE_APPEND, table_reg, value_reg, 0), node->line);
                free_register(cg, value_reg);
            }
            
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                int key_reg = codegen_expression(cg, kv->binary.left);
                int value_reg = codegen_expression(cg, kv->binary.right);
                emit(cg, INST(OP_TABLE_SET, table_reg, key_reg, value_reg), node->line);
                free_register(cg, key_reg);
                free_register(cg, value_reg);
            }
            
            return table_reg;
        }
        case AST_STRING_INTERP: {
            int reg = codegen_string_interp(cg, node);
            return reg;
        }
        case AST_RANGE: {
            int reg = codegen_range(cg, node);
            return reg;
        }
        default: {
            int reg = alloc_register(cg);
            emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), node->line);
            return reg;
        }
    }
}

// ========== Statement Code Generation ==========

static void codegen_var_decl(CodeGenerator* cg, ASTNode* node) {
    int value_reg = codegen_expression(cg, node->var_assign.value);
    
    // Inside a function -> use fast registers
    if (cg->current_function != -1) {
        int local_reg = add_local(cg, node->var_assign.name);
        emit(cg, INST(OP_MOVE, local_reg, value_reg, 0), node->line);
    } else {
        // Otherwise use global memory
        int global_idx = bytecode_add_global(cg->chunk, node->var_assign.name);
        emit(cg, INST(OP_STORE_GLOBAL, value_reg, global_idx, 0), node->line);
    }
    free_register(cg, value_reg);
}

static void codegen_assign(CodeGenerator* cg, ASTNode* node) {
    int local_reg = find_local(cg, node->var_assign.name);
    
    if (local_reg >= 0) {
        // === OPTIMIZATION 1: i = i + 1 -> OP_ADD_IMM ===
        if (node->var_assign.value->type == AST_BINARY &&
            node->var_assign.value->binary.op == TOKEN_PLUS) {
            ASTNode* left = node->var_assign.value->binary.left;
            ASTNode* right = node->var_assign.value->binary.right;
            
            // Pattern: x = x + "literal"
            if (left->type == AST_IDENTIFIER &&
                strcmp(left->identifier.name, node->var_assign.name) == 0 &&
                right->type == AST_LITERAL_STRING) {
                
                int local_reg = find_local(cg, node->var_assign.name);
                if (local_reg >= 0) {
                    int right_reg = codegen_literal_string(cg, right);
                    emit(cg, INST(OP_STRING_APPEND, local_reg, right_reg, 0), node->line);
                    free_register(cg, right_reg);
                    return;
                }
            }
        }

        // === OPTIMIZATION 2: x = x + y -> OP_ADD ===
        if (node->var_assign.value->type == AST_BINARY) {
            ASTNode* bin = node->var_assign.value;
            if (bin->binary.left->type == AST_IDENTIFIER &&
                strcmp(bin->binary.left->identifier.name, node->var_assign.name) == 0) {
                
                int right_reg = codegen_expression(cg, bin->binary.right);
                Opcode op = OP_NOP;
                switch (bin->binary.op) {
                    case TOKEN_PLUS: op = OP_ADD; break;
                    case TOKEN_MINUS: op = OP_SUB; break;
                    case TOKEN_STAR: op = OP_MUL; break;
                    case TOKEN_SLASH: op = OP_DIV; break;
                    case TOKEN_PERCENT: op = OP_MOD; break;
                    default: break;
                }
                if (op != OP_NOP) {
                    emit(cg, INST(op, local_reg, local_reg, right_reg), node->line);
                    free_register(cg, right_reg);
                    return;
                }
                free_register(cg, right_reg);
            }
        }
        
        int value_reg = codegen_expression(cg, node->var_assign.value);
        emit(cg, INST(OP_MOVE, local_reg, value_reg, 0), node->line);
        free_register(cg, value_reg);
        return;
    }

    // === GLOBAL VARIABLES (Fallback) ===
    int value_reg = codegen_expression(cg, node->var_assign.value);
    int global_idx = bytecode_get_global(cg->chunk, node->var_assign.name);
    if (global_idx < 0) {
        global_idx = bytecode_add_global(cg->chunk, node->var_assign.name);
    }
    emit(cg, INST(OP_STORE_GLOBAL, value_reg, global_idx, 0), node->line);
    free_register(cg, value_reg);
}

static void codegen_if_statement(CodeGenerator* cg, ASTNode* node) {
    // Try to generate an optimized jump
    int jump_to_else = codegen_optimized_condition(cg, node->if_stmt.condition, node->line);
    
    if (jump_to_else < 0) {
        // Fallback for complex conditions (function calls, not, etc.)
        int cond_reg = codegen_expression(cg, node->if_stmt.condition);
        jump_to_else = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);
        free_register(cg, cond_reg);
    }
    
    // Then branch
    codegen_block(cg, node->if_stmt.then_branch);
    
    // List of jumps to end
    int end_jumps[64];
    int end_jump_count = 0;
    
    end_jumps[end_jump_count++] = bytecode_current_offset(cg->chunk);
    emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);
    
    // Patch jump to else/elif
    int else_addr = bytecode_current_offset(cg->chunk);
    bytecode_patch_jump(cg->chunk, jump_to_else, else_addr);
    
    // Process elif chain
    ASTNode* elif = node->if_stmt.elif_chain;
    while (elif) {
        int elif_cond_reg = codegen_expression(cg, elif->if_stmt.condition);
        
        int jump_to_next = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, elif_cond_reg, 0), elif->line);
        free_register(cg, elif_cond_reg);
        
        // Elif body
        codegen_block(cg, elif->if_stmt.then_branch);
        
        // Jump to end
        end_jumps[end_jump_count++] = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP, 0, 0, 0), elif->line);
        
        // Patch jump to next elif/else
        int next_addr = bytecode_current_offset(cg->chunk);
        bytecode_patch_jump(cg->chunk, jump_to_next, next_addr);
        
        elif = elif->if_stmt.elif_chain;
    }
    
    // Else branch
    if (node->if_stmt.else_branch) {
        codegen_block(cg, node->if_stmt.else_branch);
    }
    
    // Patch ALL jumps to end
    int end_addr = bytecode_current_offset(cg->chunk);
    for (int i = 0; i < end_jump_count; i++) {
        bytecode_patch_jump(cg->chunk, end_jumps[i], end_addr);
    }
}

static void codegen_while_statement(CodeGenerator* cg, ASTNode* node) {
    ASTNode* condition = node->while_stmt.condition;
    int left_reg = -1;
    int right_reg = -1;
    Opcode jump_op = OP_NOP;
    bool optimized = false;
    bool right_hoisted = false;

    // 1. Attempt to optimize the condition and hoist loop invariants (LICM)
    if (condition->type == AST_BINARY) {
        TokenType op = condition->binary.op;
        switch (op) {
            case TOKEN_LESS:          jump_op = OP_JUMP_IF_GTE; break;
            case TOKEN_LESS_EQUAL:    jump_op = OP_JUMP_IF_GT;  break;
            case TOKEN_GREATER:       jump_op = OP_JUMP_IF_LTE; break;
            case TOKEN_GREATER_EQUAL: jump_op = OP_JUMP_IF_LT;  break;
            case TOKEN_EQUAL_EQUAL:   jump_op = OP_JUMP_IF_NEQ; break;
            case TOKEN_NOT_EQUAL:     jump_op = OP_JUMP_IF_EQ;  break;
            default: jump_op = OP_NOP; break;
        }
        
        if (jump_op != OP_NOP) {
            optimized = true;
            ASTNode* right_node = condition->binary.right;
            // LICM: Hoist literals OUT of the loop
            if (right_node->type == AST_LITERAL_NUMBER || 
                right_node->type == AST_LITERAL_STRING ||
                right_node->type == AST_LITERAL_BOOL) {
                right_reg = codegen_expression(cg, right_node);
                right_hoisted = true;
            }
        }
    }

    // 2. Loop start
    int loop_start = bytecode_current_offset(cg->chunk);
    
    if (cg->loop_stack.count >= cg->loop_stack.capacity) {
        cg->loop_stack.capacity = cg->loop_stack.capacity == 0 ? 16 : cg->loop_stack.capacity * 2;
        cg->loop_stack.break_addrs = (int*)realloc(cg->loop_stack.break_addrs, sizeof(int) * cg->loop_stack.capacity);
        cg->loop_stack.continue_addrs = (int*)realloc(cg->loop_stack.continue_addrs, sizeof(int) * cg->loop_stack.capacity);
    }
    
    cg->loop_stack.break_addrs[cg->loop_stack.count] = 0;
    cg->loop_stack.continue_addrs[cg->loop_stack.count] = loop_start;
    cg->loop_stack.count++;

    int jump_to_end = -1;

    // 3. Generate condition check
    if (optimized) {
        left_reg = codegen_expression(cg, condition->binary.left);
        if (!right_hoisted) {
            right_reg = codegen_expression(cg, condition->binary.right);
        }
        jump_to_end = emit(cg, INST(jump_op, 0, left_reg, right_reg), node->line);
        
        if (!right_hoisted) free_register(cg, right_reg);
        free_register(cg, left_reg);
    } else {
        int cond_reg = codegen_expression(cg, condition);
        jump_to_end = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);
        free_register(cg, cond_reg);
    }
    
    // 4. Loop body
    codegen_block(cg, node->while_stmt.body);
    emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);
    
    // 5. Loop exit
    int end_addr = bytecode_current_offset(cg->chunk);
    bytecode_patch_jump(cg->chunk, jump_to_end, end_addr);
    
    cg->loop_stack.break_addrs[cg->loop_stack.count - 1] = end_addr;
    cg->loop_stack.count--;
}

static int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line) {
    if (condition->type == AST_BINARY) {
        TokenType op = condition->binary.op;
        Opcode jump_op = OP_NOP;
        
        // Invert the condition for exit (jump if FALSE)
        switch (op) {
            case TOKEN_LESS:          jump_op = OP_JUMP_IF_GTE; break;
            case TOKEN_LESS_EQUAL:    jump_op = OP_JUMP_IF_GT;  break;
            case TOKEN_GREATER:       jump_op = OP_JUMP_IF_LTE; break;
            case TOKEN_GREATER_EQUAL: jump_op = OP_JUMP_IF_LT;  break;
            case TOKEN_EQUAL_EQUAL:   jump_op = OP_JUMP_IF_NEQ; break;
            case TOKEN_NOT_EQUAL:     jump_op = OP_JUMP_IF_EQ;  break;
            default: return -1;
        }
        
        int left_reg = codegen_expression(cg, condition->binary.left);
        int right_reg = codegen_expression(cg, condition->binary.right);
        
        // Generate jump and REMEMBER its exact offset
        int jump_offset = emit(cg, INST(jump_op, 0, left_reg, right_reg), line);
        
        free_register(cg, right_reg);
        free_register(cg, left_reg);
        return jump_offset;
    }
    return -1;
}

static void codegen_break(CodeGenerator* cg, ASTNode* node) {
    if (cg->loop_stack.count > 0) {
        if (cg->loop_stack.is_fast[cg->loop_stack.count - 1]) {
            emit(cg, INST(OP_POP_ITER, 0, 0, 0), node->line); // Clean up stack
        }
        int break_addr = cg->loop_stack.break_addrs[cg->loop_stack.count - 1];
        emit(cg, INST(OP_JUMP, break_addr, 0, 0), node->line);
    }
}

static void codegen_continue(CodeGenerator* cg, ASTNode* node) {
    if (cg->loop_stack.count > 0) {
        int continue_addr = cg->loop_stack.continue_addrs[cg->loop_stack.count - 1];
        emit(cg, INST(OP_JUMP, continue_addr, 0, 0), node->line);
    }
}

static void codegen_function_decl(CodeGenerator* cg, ASTNode* node) {
    // Register the function BEFORE generating the body
    int func_idx = bytecode_add_function(cg->chunk, 
                                          node->function_decl.name,
                                          node->function_decl.params->count);
    
    // Jump over the function body
    int jump_over = bytecode_current_offset(cg->chunk);
    emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);
    
    // Address of function start
    int func_addr = bytecode_current_offset(cg->chunk);
    cg->chunk->functions[func_idx].address = func_addr;
    
    // Save ALL codegen state
    int prev_function = cg->current_function;
    int prev_next_register = cg->next_register;
    int prev_max_registers = cg->max_registers;
    
    // Save local variables
    int saved_count = cg->locals.count;
    char** saved_names = NULL;
    int* saved_regs = NULL;
    
    if (saved_count > 0) {
        saved_names = (char**)malloc(sizeof(char*) * saved_count);
        saved_regs = (int*)malloc(sizeof(int) * saved_count);
        for (int i = 0; i < saved_count; i++) {
            saved_names[i] = strdup(cg->locals.names[i]);
            saved_regs[i] = cg->locals.registers[i];
        }
    }
    
    // Clear locals (but don't free the saved strings!)
    for (int i = 0; i < cg->locals.count; i++) {
        free(cg->locals.names[i]);
    }
    free(cg->locals.names);
    free(cg->locals.registers);
    
    // Reset for new function
    cg->locals.names = NULL;
    cg->locals.registers = NULL;
    cg->locals.count = 0;
    cg->locals.capacity = 0;
    cg->next_register = 0;
    cg->max_registers = 0;
    cg->current_function = func_idx;
    
    // Add parameters as locals
    for (int i = 0; i < node->function_decl.params->count; i++) {
        ASTNode* param = node->function_decl.params->nodes[i];
        add_local(cg, param->param.name); // Will automatically occupy R0, R1, R2...
    }
    
    // Generate function body
    codegen_block(cg, node->function_decl.body);
    
    // Implicit return false at end
    int false_idx = bytecode_add_bool_constant(cg->chunk, false);
    int false_reg = alloc_register(cg);
    emit(cg, INST(OP_LOAD_CONST, false_reg, false_idx, 0), node->line);
    emit(cg, INST(OP_RETURN, false_reg, 0, 0), node->line);
    free_register(cg, false_reg);
    
    // Update function info
    cg->chunk->functions[func_idx].local_count = cg->locals.count;
    
    // Free function's local variables
    for (int i = 0; i < cg->locals.count; i++) {
        free(cg->locals.names[i]);
    }
    free(cg->locals.names);
    free(cg->locals.registers);
    
    // Restore parent's local variables
    cg->locals.names = saved_names;
    cg->locals.registers = saved_regs;
    cg->locals.count = saved_count;
    cg->locals.capacity = saved_count;
    
    // Restore register counters
    cg->next_register = prev_next_register;
    cg->max_registers = prev_max_registers;
    cg->current_function = prev_function;
    
    // Patch jump over function body
    bytecode_patch_jump(cg->chunk, jump_over, bytecode_current_offset(cg->chunk));
    
    // Store function as a global variable
    int global_idx = bytecode_add_global(cg->chunk, node->function_decl.name);
    int func_const_idx = bytecode_add_constant(cg->chunk, 
        (Constant){.type = CONST_FUNCTION, .function_index = func_idx});
    
    int temp_reg = alloc_register(cg);
    emit(cg, INST(OP_LOAD_CONST, temp_reg, func_const_idx, 0), node->line);
    emit(cg, INST(OP_STORE_GLOBAL, temp_reg, global_idx, 0), node->line);
    free_register(cg, temp_reg);
}

static void codegen_return(CodeGenerator* cg, ASTNode* node) {
    if (node->return_stmt.value) {
        int value_reg = codegen_expression(cg, node->return_stmt.value);
        emit(cg, INST(OP_RETURN, value_reg, 0, 0), node->line);
    } else {
        emit(cg, INST(OP_RETURN_VOID, 0, 0, 0), node->line);
    }
}

static void codegen_try_statement(CodeGenerator* cg, ASTNode* node) {
    int catch_label = make_label(cg);
    
    // TRY begin
    int try_start = bytecode_current_offset(cg->chunk);
    emit(cg, INST(OP_TRY, catch_label, 0, 0), node->line);
    
    // Try body
    codegen_block(cg, node->try_stmt.try_body);
    
    // Jump to finally
    int jump_to_finally = bytecode_current_offset(cg->chunk);
    emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);
    
    // CATCH block
    bytecode_patch_jump(cg->chunk, try_start + 1, bytecode_current_offset(cg->chunk));
    
    if (node->try_stmt.failure_body) {
        // Create error variable
        int error_reg = add_local(cg, "error");
        emit(cg, INST(OP_CATCH, error_reg, 0, 0), node->line);
        codegen_block(cg, node->try_stmt.failure_body);
    }
    
    // FINALLY block
    bytecode_patch_jump(cg->chunk, jump_to_finally, bytecode_current_offset(cg->chunk));
    
    if (node->try_stmt.always_body) {
        emit(cg, INST(OP_END_TRY, 0, 0, 0), node->line);
        codegen_block(cg, node->try_stmt.always_body);
    } else {
        emit(cg, INST(OP_END_TRY, 0, 0, 0), node->line);
    }
}

static void codegen_import(CodeGenerator* cg, ASTNode* node) {
    const char* full_path = node->import_stmt.module_path;
    
    // Split path into parts
    char path_copy[1024];
    strcpy(path_copy, full_path);
    
    char* parts[64];
    int part_count = 0;
    
    char* token = strtok(path_copy, ".");
    while (token && part_count < 64) {
        parts[part_count++] = token;
        token = strtok(NULL, ".");
    }
    
    if (part_count == 0) return;
    
    // Create nested tables for each path segment
    int parent_reg = -1;
    
    for (int i = 0; i < part_count; i++) {
        int current_reg = alloc_register(cg);
        emit(cg, INST(OP_NEW_TABLE, current_reg, 0, 0), node->line);
        
        if (i == 0) {
            // First part — global variable
            int global_idx = bytecode_add_global(cg->chunk, parts[i]);
            emit(cg, INST(OP_STORE_GLOBAL, current_reg, global_idx, 0), node->line);
        } else {
            // Nested part — add to parent table
            int key_idx = bytecode_add_string_constant(cg->chunk, parts[i]);
            emit(cg, INST(OP_TABLE_SET_CONST, parent_reg, key_idx, current_reg), node->line);
        }
        
        if (parent_reg >= 0) {
            free_register(cg, parent_reg);
        }
        parent_reg = current_reg;
    }
    
    // Load module file if it exists
    // In a full implementation this would load and execute the module
    
    if (parent_reg >= 0) {
        free_register(cg, parent_reg);
    }
}

static void codegen_expr_statement(CodeGenerator* cg, ASTNode* node) {
    int result_reg = codegen_expression(cg, node->expr_stmt.expression);
    free_register(cg, result_reg);
}

static void codegen_statement(CodeGenerator* cg, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_VAR_DECL:        codegen_var_decl(cg, node); break;
        case AST_ASSIGN:          codegen_assign(cg, node); break;
        case AST_IF_STMT:         codegen_if_statement(cg, node); break;
        case AST_WHILE_STMT:      codegen_while_statement(cg, node); break;
        case AST_FOR_STMT:        codegen_for_statement(cg, node); break;
        case AST_FUNCTION_DECL:   codegen_function_decl(cg, node); break;
        case AST_RETURN_STMT:     codegen_return(cg, node); break;
        case AST_BREAK_STMT:      codegen_break(cg, node); break;
        case AST_CONTINUE_STMT:   codegen_continue(cg, node); break;
        case AST_TRY_STMT:        codegen_try_statement(cg, node); break;
        case AST_IMPORT_STMT:     codegen_import(cg, node); break;
        case AST_EXPR_STMT:       codegen_expr_statement(cg, node); break;
        case AST_BLOCK:           codegen_block(cg, node); break;
        default: break;
    }
}

static void codegen_block(CodeGenerator* cg, ASTNode* node) {
    if (!node || (node->type != AST_BLOCK && node->type != AST_PROGRAM)) return;

    for (int i = 0; i < node->block.statements->count; i++) {
        ASTNode* stmt = node->block.statements->nodes[i];
        codegen_statement(cg, stmt);
    }
}

// ========== Public API ==========

bool codegen_generate(CodeGenerator* cg, ASTNode* ast) {
    if (!cg || !ast) return false;

    bytecode_add_function(cg->chunk, "main", 0);
    cg->current_function = 0;
    
    if (ast->type == AST_PROGRAM || ast->type == AST_BLOCK) {
        codegen_block(cg, ast);
    } else {
        codegen_statement(cg, ast);
    }
    
    emit(cg, INST(OP_HALT, 0, 0, 0), 0);
    
    return true;
}

void codegen_print_locals(CodeGenerator* cg) {
    printf("\n=== Local Variables ===\n");
    for (int i = 0; i < cg->locals.count; i++) {
        printf("  %s -> R%d\n", cg->locals.names[i], cg->locals.registers[i]);
    }
}
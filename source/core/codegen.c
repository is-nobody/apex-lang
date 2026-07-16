#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// checks if a module name is a built-in system module
static bool is_known_builtin_module(const char* name) {
    return strcmp(name, "os") == 0 ||
           strcmp(name, "sys") == 0 || strcmp(name, "math") == 0 ||
           strcmp(name, "string") == 0 || strcmp(name, "table") == 0 ||
           strcmp(name, "ffi") == 0 || strcmp(name, "random") == 0 ||
           strcmp(name, "codecs") == 0;
}

static int codegen_expression(CodeGenerator* cg, ASTNode* node);
static void codegen_statement(CodeGenerator* cg, ASTNode* node);
static void codegen_block(CodeGenerator* cg, ASTNode* node);
static int codegen_string_interp(CodeGenerator* cg, ASTNode* node);
static int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line);
static int codegen_index_assign(CodeGenerator* cg, ASTNode* node);
static int codegen_assign_expr(CodeGenerator* cg, ASTNode* node);

// allocates a new virtual register for temporary values
static int alloc_register(CodeGenerator* cg) {
    int reg = cg->next_register++;
    if (reg >= cg->max_registers) {
        cg->max_registers = reg + 1;
    }
    return reg;
}

// releases a register if it's not a local variable and is the last allocated temp
static void free_register(CodeGenerator* cg, int reg) {
    for (int i = 0; i < cg->locals.count; i++) {
        if (cg->locals.registers[i] == reg) {
            return;
        }
    }
    if (reg == cg->next_register - 1) {
        cg->next_register--;
    }
}

// finds the register holding a local variable by name, returns -1 if not found
static int find_local(CodeGenerator* cg, const char* name) {
    if (!name) return -1;
    for (int i = 0; i < cg->locals.count; i++) {
        if (strcmp(cg->locals.names[i], name) == 0) {
            return cg->locals.registers[i];
        }
    }
    return -1;
}

// adds a local variable and assigns it a register, returns the register
static int add_local(CodeGenerator* cg, const char* name) {
    int existing = find_local(cg, name);
    if (existing >= 0) return existing;
    
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

// emits an instruction with source line info for debugging
static int emit(CodeGenerator* cg, Instruction inst, int line) {
    return bytecode_emit_line(cg->chunk, inst, line);
}

// creates a new code generator with preloaded constants in dedicated registers
CodeGenerator* codegen_create(BytecodeChunk* chunk) {
    CodeGenerator* cg = (CodeGenerator*)calloc(1, sizeof(CodeGenerator));
    cg->chunk = chunk;
    cg->next_register = 0;
    cg->max_registers = 0;
    cg->current_function = -1;
    cg->label_counter = 0;
    
    cg->loop_stack.break_capacity = 16;
    cg->loop_stack.break_jumps = (int*)malloc(sizeof(int) * cg->loop_stack.break_capacity);
    
    cg->current_module = NULL;
    cg->imported_modules = NULL;
    cg->module_count = 0;
    cg->module_capacity = 0;
    cg->module_globals = NULL;
    cg->module_globals_count = 0;
    cg->module_globals_capacity = 0;

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

// frees all code generator resources including locals and module tracking arrays
void codegen_destroy(CodeGenerator* cg) {
    if (!cg) return;
    
    for (int i = 0; i < cg->locals.count; i++) {
        free(cg->locals.names[i]);
    }
    free(cg->locals.names);
    free(cg->locals.registers);
    
    free(cg->loop_stack.break_jumps);
    
    for (int i = 0; i < cg->module_count; i++) {
        free(cg->imported_modules[i]);
    }
    free(cg->imported_modules);
    
    for (int i = 0; i < cg->module_globals_count; i++) {
        free(cg->module_globals[i]);
    }
    free(cg->module_globals);
    
    free(cg);
}

// emits a number literal, using immediate form for small integers
static int codegen_literal_number(CodeGenerator* cg, ASTNode* node) {
    double val = node->literal_number.number_value;
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

// emits a string literal by adding it to the constant pool
static int codegen_literal_string(CodeGenerator* cg, ASTNode* node) {
    int reg = alloc_register(cg);
    int const_idx = bytecode_add_string_constant(cg->chunk,
                                                 node->literal_string.string_value);
    emit(cg, INST(OP_LOAD_CONST, reg, const_idx, 0), node->line);
    return reg;
}

// emits a none/null literal by adding it to the constant pool
static int codegen_literal_none(CodeGenerator* cg, ASTNode* node) {
    int reg = alloc_register(cg);
    int none_idx = bytecode_add_none_constant(cg->chunk);
    emit(cg, INST(OP_LOAD_CONST, reg, none_idx, 0), node->line);
    return reg;
}

// emits a boolean literal using the dedicated load-bool instruction
static int codegen_literal_bool(CodeGenerator* cg, ASTNode* node) {
    int reg = alloc_register(cg);
    emit(cg, INST(OP_LOAD_BOOL, reg, node->literal_bool.bool_value ? 1 : 0, 0), node->line);
    return reg;
}

// emits an identifier, preferring local variables over globals
static int codegen_identifier(CodeGenerator* cg, ASTNode* node) {
    const char* name = node->identifier.name;
    
    int local_reg = find_local(cg, name);
    if (local_reg >= 0) {
        return local_reg;
    }
    
    for (int i = 0; i < cg->module_count; i++) {
        char full_name[512];
        snprintf(full_name, sizeof(full_name), "%s.%s", cg->imported_modules[i], name);
        
        int global_idx = bytecode_get_global(cg->chunk, full_name);
        if (global_idx >= 0) {
            int reg = alloc_register(cg);
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
            return reg;
        }
    }
    
    int global_idx = bytecode_get_global(cg->chunk, name);
    if (global_idx >= 0) {
        if (cg->current_function == 0) {
            int reg = add_local(cg, name);
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
            return reg;
        }
        int reg = alloc_register(cg);
        emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
        return reg;
    }
    
    global_idx = bytecode_add_global(cg->chunk, name);
    if (cg->current_function == 0) {
        int reg = add_local(cg, name);
        emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
        return reg;
    }
    int reg = alloc_register(cg);
    emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
    return reg;
}

// emits a function call, resolving builtins and user functions by name
static int codegen_call(CodeGenerator* cg, ASTNode* node) {
    ASTNodeList* args_list = node->call.arguments;
    int arg_count = args_list->count;
    int* arg_regs = NULL;
    
    if (arg_count > 0) {
        arg_regs = (int*)malloc(sizeof(int) * arg_count);
        for (int i = 0; i < arg_count; i++) {
            arg_regs[i] = codegen_expression(cg, args_list->nodes[i]);
        }
    }
    
    char func_name[256] = "";
    ASTNode* callee = node->call.callee;
    if (callee->type == AST_IDENTIFIER) {
        strcpy(func_name, callee->identifier.name);
    } else if (callee->type == AST_INDEX_ACCESS &&
               callee->access.object->type == AST_IDENTIFIER &&
               callee->access.member->type == AST_IDENTIFIER) {
        snprintf(func_name, sizeof(func_name), "%s.%s",
                 callee->access.object->identifier.name,
                 callee->access.member->identifier.name);
    }
    
    int result_reg = alloc_register(cg);
    for (int i = 0; i < arg_count; i++) {
        emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
    }
    
    static const char* builtins[] = {
        "os.output", "os.input",
        "os.time", "os.wait", "os.exit",
        "os.get_current_folder", "os.set_current_folder",
        "os.terminate_process", "os.execute",
        "os.read", "os.write", "os.append",
        "os.exists", "os.isfile", "os.isfolder", "os.size",
        "os.filetype", "os.stat",
        "os.create_file", "os.create_folder",
        "os.delete", "os.rename", 
        "os.move", "os.copy",
        "os.items", "os.parentfolder",
        "os.access",
        "sys.platform", "sys.architecture", "sys.hostname", "sys.user",
        "sys.homedir", "sys.apex_version", "sys.executable", "sys.environment",
        "sys.disksize", "sys.tempdir", "sys.isterminal", "sys.process_id", 
        "math.abs", "math.floor", "math.ceil", "math.round",
        "math.sqrt", "math.exp", "math.log",
        "math.sin", "math.cos", "math.tan",
        "math.asin", "math.acos", "math.atan",
        "math.pi", "math.e", "math.inf",
        "math.isnan", "math.isinf", "math.trunc",
        "math.pow", "math.atan2",
        "math.radians", "math.degrees", "math.hypot",
        "math.gcd", "math.factorial",
        "string.isletter", "string.isnumber",
        "string.len", "string.lower", "string.upper",
        "string.sub", "string.split", "string.join",
        "string.trim", "string.find", "string.replace",
        "table.remove", "table.has", "table.size",
        "table.keys", "table.values", "table.clear",
        "table.copy", "table.merge",
        "ffi.open", "ffi.call", "ffi.errno", "ffi.strerror",
        "ffi.malloc", "ffi.free",
        "random.random", "random.randint", "random.choice", "random.shuffle",
        "random.sample", "random.gauss", "random.seed",
        "random.triangular", "random.expovariate", "random.betavariate",
        "random.secure_token_hex", "random.secure_randint", "random.compare_digest",
        "codecs.json_read", "codecs.json_write",
        "codecs.csv_read", "codecs.csv_write",
        "codecs.xml_read", "codecs.xml_write",
        "codecs.base_read", "codecs.base_write",
        "codecs.baseurl_write", "codecs.baseurl_read",
        "number", "string", "type",
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
            int name_idx = bytecode_add_string_constant(cg->chunk, func_name);
            emit(cg, INST(OP_CALL_BUILTIN, result_reg, name_idx, arg_count), node->line);
        }
    }
    
    if (arg_regs) {
        for (int i = 0; i < arg_count; i++) {
            free_register(cg, arg_regs[i]);
        }
        free(arg_regs);
    }

    if (cg->current_function == 0) {
        for (int i = 0; i < cg->chunk->global_count; i++) {
            int local_reg = find_local(cg, cg->chunk->globals[i].name);
            if (local_reg >= 0) {
                emit(cg, INST(OP_LOAD_GLOBAL, local_reg, i, 0), node->line);
            }
        }
    }

    return result_reg;
}

// emits string interpolation by concatenating parts with to-string conversions
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
        int part_reg = codegen_expression(cg, part);

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

// emits for loops, supporting both range loops and table iteration
static void codegen_for_statement(CodeGenerator* cg, ASTNode* node) {
    int prev_break_count = cg->loop_stack.break_count;
    int prev_continue_addr = cg->loop_stack.continue_addr;
    bool prev_is_fast = cg->loop_stack.is_fast;
    
    cg->loop_stack.is_fast = (node->for_stmt.var_name != NULL);

    if (node->for_stmt.var_name) {
        if (node->for_stmt.end == NULL && !node->for_stmt.condition) {
            int table_reg = codegen_expression(cg, node->for_stmt.start);
            int keys_table_reg = alloc_register(cg);
            int name_idx = bytecode_add_string_constant(cg->chunk, "table.keys");
            emit(cg, INST(OP_PUSH_ARG, table_reg, 0, 0), node->line);
            emit(cg, INST(OP_CALL_BUILTIN, keys_table_reg, name_idx, 1), node->line);

            int size_reg = alloc_register(cg);
            name_idx = bytecode_add_string_constant(cg->chunk, "table.size");
            emit(cg, INST(OP_PUSH_ARG, keys_table_reg, 0, 0), node->line);
            emit(cg, INST(OP_CALL_BUILTIN, size_reg, name_idx, 1), node->line);

            int var_reg = add_local(cg, node->for_stmt.var_name);
            int index_reg = alloc_register(cg);
            int zero_idx = bytecode_add_number_constant(cg->chunk, 0.0);
            emit(cg, INST(OP_LOAD_CONST, index_reg, zero_idx, 0), node->line);

            emit(cg, INST(OP_FOR_INIT, index_reg, size_reg, cg->cache.one_reg), node->line);

            int loop_start = bytecode_current_offset(cg->chunk);
            cg->loop_stack.continue_addr = loop_start;

            int for_next_instr = bytecode_current_offset(cg->chunk);
            emit(cg, INST(OP_FOR_NEXT, index_reg, size_reg, 0), node->line);

            int key_reg = alloc_register(cg);
            emit(cg, INST(OP_TABLE_GET, key_reg, keys_table_reg, index_reg), node->line);

            emit(cg, INST(OP_MOVE, var_reg, key_reg, 0), node->line);

            free_register(cg, key_reg);

            codegen_block(cg, node->for_stmt.body);

            emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);

            int exit_addr = bytecode_current_offset(cg->chunk);
            cg->chunk->code[for_next_instr].operands[2] = exit_addr;

            free_register(cg, table_reg);
            free_register(cg, keys_table_reg);
            free_register(cg, size_reg);
            free_register(cg, index_reg);
            
            for (int i = 0; i < cg->loop_stack.break_count; i++) {
                bytecode_patch_jump(cg->chunk, cg->loop_stack.break_jumps[i], exit_addr);
            }
        } else {
            int start_reg = codegen_expression(cg, node->for_stmt.start);
            int end_reg = codegen_expression(cg, node->for_stmt.end);
            int step_reg;
            
            if (node->for_stmt.step) {
                step_reg = codegen_expression(cg, node->for_stmt.step);
            } else {
                step_reg = alloc_register(cg);
                int one_idx = bytecode_add_number_constant(cg->chunk, 1.0);
                emit(cg, INST(OP_LOAD_CONST, step_reg, one_idx, 0), node->line);
            }

            int var_reg = add_local(cg, node->for_stmt.var_name);
            emit(cg, INST(OP_MOVE, var_reg, start_reg, 0), node->line);
            emit(cg, INST(OP_FOR_INIT, var_reg, end_reg, step_reg), node->line);
            
            int loop_start = bytecode_current_offset(cg->chunk);
            cg->loop_stack.continue_addr = loop_start;
            
            int for_next_instr = bytecode_current_offset(cg->chunk);
            emit(cg, INST(OP_FOR_NEXT, var_reg, 0, 0), node->line);
            
            codegen_block(cg, node->for_stmt.body);
            
            emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);
            
            int exit_addr = bytecode_current_offset(cg->chunk);
            cg->chunk->code[for_next_instr].operands[1] = exit_addr;
            
            free_register(cg, start_reg);
            free_register(cg, end_reg);
            if (!node->for_stmt.step) free_register(cg, step_reg);
        }
        
        int exit_addr = bytecode_current_offset(cg->chunk);
        for (int i = 0; i < cg->loop_stack.break_count; i++) {
            bytecode_patch_jump(cg->chunk, cg->loop_stack.break_jumps[i], exit_addr);
        }

    } else {
        ASTNode* condition = node->for_stmt.condition;
        int left_reg = -1;
        int right_reg = -1;
        bool optimized = false;
        bool right_hoisted = false;
        Opcode jump_op = OP_JUMP;

        if (condition) {
            if (condition->type == AST_BINARY) {
                TokenType op = condition->binary.op;
                switch (op) {
                    case TOKEN_LESS:          jump_op = OP_JUMP_IF_GTE; optimized = true; break;
                    case TOKEN_LESS_EQUAL:    jump_op = OP_JUMP_IF_GT;  optimized = true; break;
                    case TOKEN_GREATER:       jump_op = OP_JUMP_IF_LTE; optimized = true; break;
                    case TOKEN_GREATER_EQUAL: jump_op = OP_JUMP_IF_LT;  optimized = true; break;
                    case TOKEN_EQUAL_EQUAL:   jump_op = OP_JUMP_IF_NEQ; optimized = true; break;
                    case TOKEN_NOT_EQUAL:     jump_op = OP_JUMP_IF_EQ;  optimized = true; break;
                    default: break;
                }
                if (optimized) {
                    ASTNode* right_node = condition->binary.right;
                    if (right_node->type == AST_LITERAL_NUMBER ||
                        right_node->type == AST_LITERAL_STRING ||
                        right_node->type == AST_LITERAL_BOOL) {
                        right_reg = codegen_expression(cg, right_node);
                        right_hoisted = true;
                    }
                }
            }
        }

        int loop_start = bytecode_current_offset(cg->chunk);
        cg->loop_stack.continue_addr = loop_start;
        
        int jump_to_end = -1;
        if (condition) {
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
        }

        codegen_block(cg, node->for_stmt.body);
        emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);
        
        int end_addr = bytecode_current_offset(cg->chunk);
        if (jump_to_end >= 0)
            bytecode_patch_jump(cg->chunk, jump_to_end, end_addr);
            
        for (int i = 0; i < cg->loop_stack.break_count; i++)
            bytecode_patch_jump(cg->chunk, cg->loop_stack.break_jumps[i], end_addr);
    }

    cg->loop_stack.break_count = prev_break_count;
    cg->loop_stack.continue_addr = prev_continue_addr;
    cg->loop_stack.is_fast = prev_is_fast;
}

// main expression dispatcher that handles all expression node types
static int codegen_expression(CodeGenerator* cg, ASTNode* node) {
    if (!node) {
        int reg = alloc_register(cg);
        emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), 0);
        return reg;
    }

    switch (node->type) {
        case AST_ASSIGN:
            return codegen_assign_expr(cg, node);

        case AST_LITERAL_NUMBER:
            return codegen_literal_number(cg, node);

        case AST_LITERAL_STRING:
            return codegen_literal_string(cg, node);

        case AST_LITERAL_NONE:
            return codegen_literal_none(cg, node);

        case AST_LITERAL_BOOL:
            return codegen_literal_bool(cg, node);

        case AST_IDENTIFIER:
            return codegen_identifier(cg, node);

        case AST_BINARY: {
            int left_reg = codegen_expression(cg, node->binary.left);
            int right_reg = codegen_expression(cg, node->binary.right);
            int result_reg = alloc_register(cg);
            Opcode op;
            switch (node->binary.op) {
                case TOKEN_PLUS:           op = OP_ADD; break;
                case TOKEN_MINUS:          op = OP_SUB; break;
                case TOKEN_STAR:           op = OP_MUL; break;
                case TOKEN_SLASH:          op = OP_DIV; break;
                case TOKEN_PERCENT:        op = OP_MOD; break;
                case TOKEN_EQUAL_EQUAL:    op = OP_CMP_EQ; break;
                case TOKEN_NOT_EQUAL:      op = OP_CMP_NEQ; break;
                case TOKEN_LESS:           op = OP_CMP_LT; break;
                case TOKEN_GREATER:        op = OP_CMP_GT; break;
                case TOKEN_LESS_EQUAL:     op = OP_CMP_LTE; break;
                case TOKEN_GREATER_EQUAL:  op = OP_CMP_GTE; break;
                case TOKEN_AND:            op = OP_AND; break;
                case TOKEN_OR:             op = OP_OR; break;
                default:
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
                default: break;
            }
            free_register(cg, operand_reg);
            return result_reg;
        }

        case AST_CALL:
            return codegen_call(cg, node);

        case AST_INDEX_ACCESS: {
            bool is_module_access = false;
            char full_name[512] = "";

            if (node->access.object->type == AST_IDENTIFIER) {
                const char* obj_name = node->access.object->identifier.name;
                const char* member_name = NULL;

                if (node->access.member->type == AST_IDENTIFIER) {
                    member_name = node->access.member->identifier.name;
                }
                else if (node->access.member->type == AST_LITERAL_STRING) {
                    member_name = node->access.member->literal_string.string_value;
                }

                if (member_name) {
                    for (int i = 0; i < cg->module_count; i++) {
                        if (strcmp(cg->imported_modules[i], obj_name) == 0) {
                            is_module_access = true;
                            break;
                        }
                    }
                    if (!is_module_access && is_known_builtin_module(obj_name)) {
                        is_module_access = true;
                    }

                    if (is_module_access) {
                        snprintf(full_name, sizeof(full_name), "%s.%s", obj_name, member_name);
                    }
                }
            }

            if (is_module_access) {
                int global_idx = bytecode_get_global(cg->chunk, full_name);
                if (global_idx < 0) {
                    global_idx = bytecode_add_global(cg->chunk, full_name);
                }
                int reg = alloc_register(cg);
                emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
                return reg;
            }

            int obj_reg = codegen_expression(cg, node->access.object);
            int result_reg = alloc_register(cg);
            
            if (node->access.member->type == AST_IDENTIFIER) {
                int local_reg = find_local(cg, node->access.member->identifier.name);
                if (local_reg >= 0) {
                    emit(cg, INST(OP_TABLE_GET, result_reg, obj_reg, local_reg), node->line);
                } else {
                    int global_idx = bytecode_get_global(cg->chunk, node->access.member->identifier.name);
                    if (global_idx >= 0) {
                        int key_reg = alloc_register(cg);
                        emit(cg, INST(OP_LOAD_GLOBAL, key_reg, global_idx, 0), node->line);
                        emit(cg, INST(OP_TABLE_GET, result_reg, obj_reg, key_reg), node->line);
                        free_register(cg, key_reg);
                    } else {
                        int key_idx = bytecode_add_string_constant(cg->chunk,
                            node->access.member->identifier.name);
                        emit(cg, INST(OP_TABLE_GET_CONST, result_reg, obj_reg, key_idx), node->line);
                    }
                }
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
                const char* key_str = kv->binary.left->literal_string.string_value;
                int key_idx = bytecode_add_string_constant(cg->chunk, key_str);
                int value_reg = codegen_expression(cg, kv->binary.right);
                emit(cg, INST(OP_TABLE_SET_CONST, table_reg, key_idx, value_reg), node->line);
                free_register(cg, value_reg);
            }
            return table_reg;
        }

        case AST_STRING_INTERP:
            return codegen_string_interp(cg, node);

        case AST_TERNARY: {
            ASTNode* condition = node->ternary.condition;
            ASTNode* true_expr = node->ternary.true_expr;
            ASTNode* false_expr = node->ternary.false_expr;
            
            int dest_reg = alloc_register(cg);
            
            int cond_reg = codegen_expression(cg, condition);
            
            int jump_to_false = bytecode_current_offset(cg->chunk);
            emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);
            free_register(cg, cond_reg);
            
            int true_reg = codegen_expression(cg, true_expr);
            
            emit(cg, INST(OP_MOVE, dest_reg, true_reg, 0), node->line);
            free_register(cg, true_reg);
            
            int jump_to_end = bytecode_current_offset(cg->chunk);
            emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);
            
            int false_addr = bytecode_current_offset(cg->chunk);
            bytecode_patch_jump(cg->chunk, jump_to_false, false_addr);
            
            int false_reg = codegen_expression(cg, false_expr);
            
            emit(cg, INST(OP_MOVE, dest_reg, false_reg, 0), node->line);
            free_register(cg, false_reg);
            
            int end_addr = bytecode_current_offset(cg->chunk);
            bytecode_patch_jump(cg->chunk, jump_to_end, end_addr);
            
            return dest_reg;
        }

        default: {
            int reg = alloc_register(cg);
            emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), node->line);
            return reg;
        }
    }
}

// registers a module-scoped global variable for name resolution
static void add_module_global(CodeGenerator* cg, const char* full_name) {
    for (int i = 0; i < cg->module_globals_count; i++) {
        if (strcmp(cg->module_globals[i], full_name) == 0) return;
    }
    if (cg->module_globals_count >= cg->module_globals_capacity) {
        cg->module_globals_capacity = cg->module_globals_capacity == 0 ? 16 : cg->module_globals_capacity * 2;
        cg->module_globals = (char**)realloc(cg->module_globals, sizeof(char*) * cg->module_globals_capacity);
    }
    cg->module_globals[cg->module_globals_count++] = strdup(full_name);
}

// emits a variable declaration, storing in locals for functions or globals at top level
static void codegen_var_decl(CodeGenerator* cg, ASTNode* node) {
    int value_reg = codegen_expression(cg, node->var_assign.value);
    
    const char* var_name = node->var_assign.name;
    char global_name[512];
    
    if (cg->current_module) {
        snprintf(global_name, sizeof(global_name), "%s.%s", cg->current_module, var_name);
        var_name = global_name;
        add_module_global(cg, global_name);
    }
    
    int global_idx = bytecode_add_global(cg->chunk, var_name);
    emit(cg, INST(OP_STORE_GLOBAL, value_reg, global_idx, 0), node->line);
    
    int local_reg = add_local(cg, node->var_assign.name);
    emit(cg, INST(OP_MOVE, local_reg, value_reg, 0), node->line);
    
    free_register(cg, value_reg);
}

// emits assignment, optimizing numeric self-assignment patterns
static int codegen_assign_expr(CodeGenerator* cg, ASTNode* node) {
    if (node->var_assign.access_path) {
        return codegen_index_assign(cg, node);
    }

    if (!node->var_assign.name) {
        return codegen_expression(cg, node->var_assign.value);
    }

    int local_reg = find_local(cg, node->var_assign.name);
    if (local_reg >= 0) {
        if (node->var_assign.value->type == AST_BINARY) {
            ASTNode* bin = node->var_assign.value;
            if (bin->binary.left->type == AST_IDENTIFIER &&
                strcmp(bin->binary.left->identifier.name, node->var_assign.name) == 0) {
                
                int right_reg = codegen_expression(cg, bin->binary.right);
                bool can_optimize = true;
                Opcode op;
                
                switch (bin->binary.op) {
                    case TOKEN_PLUS:    op = OP_ADD; break;
                    case TOKEN_MINUS:   op = OP_SUB; break;
                    case TOKEN_STAR:    op = OP_MUL; break;
                    case TOKEN_SLASH:   op = OP_DIV; break;
                    case TOKEN_PERCENT: op = OP_MOD; break;
                    default: can_optimize = false; break;
                }
                
                if (can_optimize) {
                    emit(cg, INST(op, local_reg, local_reg, right_reg), node->line);
                    free_register(cg, right_reg);
                    return local_reg;
                }
                free_register(cg, right_reg);
            }
        }

        int value_reg = codegen_expression(cg, node->var_assign.value);
        emit(cg, INST(OP_MOVE, local_reg, value_reg, 0), node->line);
        free_register(cg, value_reg);
        return local_reg;
    }

    int value_reg = codegen_expression(cg, node->var_assign.value);
    const char* var_name = node->var_assign.name;
    char global_name[512];

    if (cg->current_module) {
        snprintf(global_name, sizeof(global_name), "%s.%s", cg->current_module, var_name);
        bool is_known = false;
        for (int i = 0; i < cg->module_globals_count; i++) {
            if (strcmp(cg->module_globals[i], global_name) == 0) {
                is_known = true;
                break;
            }
        }
        if (is_known) {
            var_name = global_name;
        }
    }

    int global_idx = bytecode_get_global(cg->chunk, var_name);
    if (global_idx < 0) {
        global_idx = bytecode_add_global(cg->chunk, var_name);
    }
    emit(cg, INST(OP_STORE_GLOBAL, value_reg, global_idx, 0), node->line);
    free_register(cg, value_reg);
    return value_reg;
}

// wrapper for assignments that discards the result register
static void codegen_assign(CodeGenerator* cg, ASTNode* node) {
    int reg = codegen_assign_expr(cg, node);
    free_register(cg, reg);
}

// emits if/elif/else chain with optimized condition evaluation
static void codegen_if_statement(CodeGenerator* cg, ASTNode* node) {
    int jump_to_else = codegen_optimized_condition(cg, node->if_stmt.condition, node->line);
    if (jump_to_else < 0) {
        int cond_reg = codegen_expression(cg, node->if_stmt.condition);
        jump_to_else = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);
        free_register(cg, cond_reg);
    }
    
    codegen_block(cg, node->if_stmt.then_branch);
    
    int end_jumps[64];
    int end_jump_count = 0;
    end_jumps[end_jump_count++] = bytecode_current_offset(cg->chunk);
    emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);
    
    int else_addr = bytecode_current_offset(cg->chunk);
    bytecode_patch_jump(cg->chunk, jump_to_else, else_addr);
    
    ASTNode* elif = node->if_stmt.elif_chain;
    while (elif) {
        int elif_cond_reg = codegen_expression(cg, elif->if_stmt.condition);
        int jump_to_next = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, elif_cond_reg, 0), elif->line);
        free_register(cg, elif_cond_reg);
        
        codegen_block(cg, elif->if_stmt.then_branch);
        
        end_jumps[end_jump_count++] = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP, 0, 0, 0), elif->line);
        
        int next_addr = bytecode_current_offset(cg->chunk);
        bytecode_patch_jump(cg->chunk, jump_to_next, next_addr);
        
        elif = elif->if_stmt.elif_chain;
    }
    
    ASTNode* else_branch = node->if_stmt.else_branch;
    if (!else_branch && node->if_stmt.elif_chain) {
        ASTNode* last = node->if_stmt.elif_chain;
        while (last->if_stmt.elif_chain) {
            last = last->if_stmt.elif_chain;
        }
        else_branch = last->if_stmt.else_branch;
    }

    if (else_branch) {
        codegen_block(cg, else_branch);
    }
    
    int end_addr = bytecode_current_offset(cg->chunk);
    for (int i = 0; i < end_jump_count; i++) {
        bytecode_patch_jump(cg->chunk, end_jumps[i], end_addr);
    }
}

// tries to optimize comparison conditions into direct jump instructions
static int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line) {
    if (condition->type == AST_BINARY) {
        TokenType op = condition->binary.op;
        Opcode jump_op;
        
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
        
        int jump_offset = emit(cg, INST(jump_op, 0, left_reg, right_reg), line);
        
        free_register(cg, right_reg);
        free_register(cg, left_reg);
        return jump_offset;
    }
    return -1;
}

// emits indexed assignment like table[index] = value with nested access support
static int codegen_index_assign(CodeGenerator* cg, ASTNode* node) {
    ASTNode* access = node->var_assign.access_path;
    
    if (access->type == AST_INDEX_ACCESS &&
        access->access.object->type == AST_IDENTIFIER &&
        access->access.member->type == AST_IDENTIFIER) {
        const char* obj_name = access->access.object->identifier.name;
        bool is_module = false;
        for (int i = 0; i < cg->module_count; i++) {
            if (strcmp(cg->imported_modules[i], obj_name) == 0) { is_module = true; break; }
        }
        if (!is_module && is_known_builtin_module(obj_name)) is_module = true;
        
        if (is_module) {
            char full_name[512];
            snprintf(full_name, sizeof(full_name), "%s.%s", obj_name, access->access.member->identifier.name);
            int val_reg = codegen_expression(cg, node->var_assign.value);
            int global_idx = bytecode_get_global(cg->chunk, full_name);
            if (global_idx < 0) global_idx = bytecode_add_global(cg->chunk, full_name);
            emit(cg, INST(OP_STORE_GLOBAL, val_reg, global_idx, 0), node->line);
            return val_reg;
        }
    }

    ASTNode* value_node = node->var_assign.value;
    ASTNode* chain[256];
    int chain_len = 0;
    ASTNode* curr = access;
    while (curr->type == AST_INDEX_ACCESS) {
        if (chain_len >= 256) break;
        chain[chain_len++] = curr;
        curr = curr->access.object;
    }
    
    int current_obj_reg = codegen_expression(cg, curr);
    for (int i = chain_len - 1; i > 0; i--) {
        ASTNode* acc = chain[i];
        int next_obj_reg = alloc_register(cg);
        int key_reg = codegen_expression(cg, acc->access.member);
        emit(cg, INST(OP_TABLE_GET, next_obj_reg, current_obj_reg, key_reg), acc->line);
        free_register(cg, key_reg);
        free_register(cg, current_obj_reg);
        current_obj_reg = next_obj_reg;
    }
    
    int val_reg = codegen_expression(cg, value_node);
    ASTNode* final_acc = chain[0];
    int key_reg = codegen_expression(cg, final_acc->access.member);
    emit(cg, INST(OP_TABLE_SET, current_obj_reg, key_reg, val_reg), final_acc->line);
    free_register(cg, key_reg);
    free_register(cg, current_obj_reg);
    
    return val_reg;
}

// emits a break statement, patching jumps to loop exit later
static void codegen_break(CodeGenerator* cg, ASTNode* node) {
    if (cg->loop_stack.break_count < cg->loop_stack.break_capacity) {
        if (cg->loop_stack.is_fast) {
            emit(cg, INST(OP_POP_ITER, 0, 0, 0), node->line);
        }
        
        int jump_offset = bytecode_current_offset(cg->chunk);
        emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);
        
        cg->loop_stack.break_jumps[cg->loop_stack.break_count++] = jump_offset;
    }
}

// emits a continue statement jumping to the loop's continue address
static void codegen_continue(CodeGenerator* cg, ASTNode* node) {
    emit(cg, INST(OP_JUMP, cg->loop_stack.continue_addr, 0, 0), node->line);
}

// emits a function declaration with proper body compilation and state isolation
static void codegen_function_decl(CodeGenerator* cg, ASTNode* node) {
    const char* func_name = node->function_decl.name;
    char global_name[512];
    
    if (cg->current_module) {
        snprintf(global_name, sizeof(global_name), "%s.%s", cg->current_module, func_name);
        func_name = global_name;
        add_module_global(cg, global_name);
    }

    int func_idx = bytecode_add_function(cg->chunk, func_name, node->function_decl.params->count);

    if (cg->current_module) {
        int global_idx = bytecode_add_global(cg->chunk, func_name);
        int func_const_idx = bytecode_add_constant(cg->chunk,
            (Constant){.type = CONST_FUNCTION, .function_index = func_idx});
        
        int temp_reg = alloc_register(cg);
        emit(cg, INST(OP_LOAD_CONST, temp_reg, func_const_idx, 0), node->line);
        emit(cg, INST(OP_STORE_GLOBAL, temp_reg, global_idx, 0), node->line);
        free_register(cg, temp_reg);
    }

    int jump_over = bytecode_current_offset(cg->chunk);
    emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);
    
    int func_addr = bytecode_current_offset(cg->chunk);
    cg->chunk->functions[func_idx].address = func_addr;

    int prev_function = cg->current_function;
    int prev_next_register = cg->next_register;
    int prev_max_registers = cg->max_registers;
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

    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);
    free(cg->locals.names);
    free(cg->locals.registers);
    cg->locals.names = NULL;
    cg->locals.registers = NULL;
    cg->locals.count = 0;
    cg->locals.capacity = 0;
    cg->next_register = 0;
    cg->max_registers = 0;
    cg->current_function = func_idx;

    for (int i = 0; i < node->function_decl.params->count; i++) {
        ASTNode* param = node->function_decl.params->nodes[i];
        add_local(cg, param->param.name);
    }

    codegen_block(cg, node->function_decl.body);

    bool ends_with_return = false;
    if (cg->chunk->code_count > 0) {
        Opcode last_op = cg->chunk->code[cg->chunk->code_count - 1].opcode;
        if (last_op == OP_RETURN || last_op == OP_RETURN_VOID) {
            ends_with_return = true;
        }
    }

    if (!ends_with_return) {
        int none_idx = bytecode_add_none_constant(cg->chunk);
        int none_reg = alloc_register(cg);
        emit(cg, INST(OP_LOAD_CONST, none_reg, none_idx, 0), node->line);
        emit(cg, INST(OP_RETURN, none_reg, 0, 0), node->line);
    }

    cg->chunk->functions[func_idx].local_count = cg->locals.count;
    if (cg->locals.count > 0) {
        cg->chunk->functions[func_idx].local_names = (char**)malloc(sizeof(char*) * cg->locals.count);
        for (int i = 0; i < cg->locals.count; i++) {
            cg->chunk->functions[func_idx].local_names[i] = strdup(cg->locals.names[i]);
        }
    } else {
        cg->chunk->functions[func_idx].local_names = NULL;
    }

    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);
    free(cg->locals.names);
    free(cg->locals.registers);
    
    cg->locals.names = saved_names;
    cg->locals.registers = saved_regs;
    cg->locals.count = saved_count;
    cg->locals.capacity = saved_count;
    cg->next_register = prev_next_register;
    cg->max_registers = prev_max_registers;
    cg->current_function = prev_function;

    bytecode_patch_jump(cg->chunk, jump_over, bytecode_current_offset(cg->chunk));
}

// emits a return statement with optional value
static void codegen_return(CodeGenerator* cg, ASTNode* node) {
    if (node->return_stmt.value) {
        int value_reg = codegen_expression(cg, node->return_stmt.value);
        emit(cg, INST(OP_RETURN, value_reg, 0, 0), node->line);
    } else {
        emit(cg, INST(OP_RETURN_VOID, 0, 0, 0), node->line);
    }
}

// emits an expression statement, discarding the result
static void codegen_expr_statement(CodeGenerator* cg, ASTNode* node) {
    int result_reg = codegen_expression(cg, node->expr_stmt.expression);
    free_register(cg, result_reg);
}

// statement dispatcher that routes each ast node type to its codegen function
static void codegen_statement(CodeGenerator* cg, ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case AST_VAR_DECL:        codegen_var_decl(cg, node); break;
        case AST_ASSIGN:          codegen_assign(cg, node); break;
        case AST_IF_STMT:         codegen_if_statement(cg, node); break;
        case AST_FOR_STMT:        codegen_for_statement(cg, node); break;
        case AST_FUNCTION_DECL:   codegen_function_decl(cg, node); break;
        case AST_RETURN_STMT:     codegen_return(cg, node); break;
        case AST_BREAK_STMT:      codegen_break(cg, node); break;
        case AST_CONTINUE_STMT:   codegen_continue(cg, node); break;
        case AST_IMPORT_STMT:     
            break; 
        case AST_EXPR_STMT:       codegen_expr_statement(cg, node); break;
        case AST_BLOCK:           codegen_block(cg, node); break;
        case AST_MODULE_BLOCK: {
            if (cg->module_count >= cg->module_capacity) {
                cg->module_capacity = cg->module_capacity == 0 ? 8 : cg->module_capacity * 2;
                cg->imported_modules = (char**)realloc(cg->imported_modules, sizeof(char*) * cg->module_capacity);
            }
            cg->imported_modules[cg->module_count++] = strdup(node->module_block.module_name);
            
            char* prev_module = cg->current_module;
            cg->current_module = node->module_block.module_name;
            codegen_block(cg, node->module_block.body);
            cg->current_module = prev_module;
            break;
        }
        default: break;
    }
}

// emits a block of statements sequentially
static void codegen_block(CodeGenerator* cg, ASTNode* node) {
    if (!node || (node->type != AST_BLOCK && node->type != AST_PROGRAM)) return;
    for (int i = 0; i < node->block.statements->count; i++) {
        ASTNode* stmt = node->block.statements->nodes[i];
        codegen_statement(cg, stmt);
    }
}

// public entry point that generates bytecode from an ast
bool codegen_generate(CodeGenerator* cg, ASTNode* ast) {
    if (!cg || !ast) return false;
    
    bytecode_add_function(cg->chunk, "__entry__", 0);
    cg->current_function = 0;
    
    if (ast->type == AST_PROGRAM || ast->type == AST_BLOCK) {
        codegen_block(cg, ast);
    } else {
        codegen_statement(cg, ast);
    }
    
    emit(cg, INST(OP_HALT, 0, 0, 0), 0);
    return true;
}
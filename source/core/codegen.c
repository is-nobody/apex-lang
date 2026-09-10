// source/core/codegen.c
// Implementation of Bytecode Code Generation for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// checks if a name is a known built-in module root using first-char switch
static bool is_known_builtin_module(const char* name) {
    switch (name[0]) {
        case 'o':
            return strcmp(name, "os") == 0;        // only os starts with 'o'
        case 's':
            return strcmp(name, "sys") == 0 ||     // sys module
                   strcmp(name, "string") == 0;    // string module
        case 'm':
            return strcmp(name, "math") == 0;      // only math starts with 'm'
        case 't':
            return strcmp(name, "table") == 0;     // only table starts with 't'
        case 'f':
            return strcmp(name, "ffi") == 0;       // only ffi starts with 'f'
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
        case 'h':
            return strcmp(name, "hex") == 0;       // hex module
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

// checks if a binary operator always produces a number result
static bool is_arithmetic_op(ApexTokenType op) {
    return op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR ||
           op == TOKEN_SLASH || op == TOKEN_PERCENT;
}

// checks if an expression is guaranteed to produce a number
static bool is_number_expression(ASTNode* node) {
    if (!node) return false;
    if (node->type == AST_LITERAL_NUMBER) return true;
    if (node->type == AST_BINARY && is_arithmetic_op(node->binary.op)) return true;
    if (node->type == AST_UNARY && node->unary.op == TOKEN_MINUS) return true;
    return false;
}

// forward declarations for the dispatcher and helpers with dest_hint contract
static int codegen_expression_into(CodeGenerator* cg, ASTNode* node, int dest_hint);
static void codegen_block(CodeGenerator* cg, ASTNode* node);
static int codegen_optimized_condition(CodeGenerator* cg, ASTNode* condition, int line);
static int codegen_index_assign(CodeGenerator* cg, ASTNode* node, int dest_hint);
static int codegen_assign_expr(CodeGenerator* cg, ASTNode* node, int dest_hint);

// default entry: no destination hint, caller owns the fresh temp
static int codegen_expression(CodeGenerator* cg, ASTNode* node) {
    return codegen_expression_into(cg, node, -1);                  // -1 means "fresh temp please"
}

// allocates a new virtual register for temporary values
static int alloc_register(CodeGenerator* cg) {
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
    }
    int reg = alloc_register(cg);                                          // allocate new register
    cg->locals.names[cg->locals.count] = strdup(name);                     // copy name
    cg->locals.registers[cg->locals.count] = reg;                          // store register
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

// emits an instruction with source line info for debugging
static int emit(CodeGenerator* cg, Instruction inst, int line) {
    return bytecode_emit_line(cg->chunk, inst, line);                      // emit with line info
}

// creates a new code generator
CodeGenerator* codegen_create(BytecodeChunk* chunk) {
    CodeGenerator* cg = (CodeGenerator*)calloc(1, sizeof(CodeGenerator));  // allocate and zero
    cg->chunk = chunk;                                                     // store bytecode chunk
    cg->next_register = 0;                                                 // start at 0
    cg->max_registers = 0;                                                 // no registers yet
    cg->current_function = -1;                                             // no active function
    cg->label_counter = 0;                                                 // label counter
    
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
    
    free(cg->loop_stack.break_jumps);                                      // free break jumps
    
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

// emits a string literal into dest (or fresh temp)
static int codegen_literal_string(CodeGenerator* cg, ASTNode* node, int dest) {
    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    int const_idx = bytecode_add_string_constant(cg->chunk,                // add string constant
                                                 node->literal_string.string_value);
    emit(cg, INST(OP_LOAD_CONST, dest, const_idx, 0), node->line);         // load constant
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
             strncmp(func_name, "ffi.", 4) == 0 ||
             strncmp(func_name, "random.", 7) == 0 ||
             strncmp(func_name, "codecs.", 7) == 0 ||
             strncmp(func_name, "regex.", 6) == 0 ||
             strncmp(func_name, "crypto.", 7) == 0) {
        is_builtin = true;
    }

    if (is_builtin) {                                                         // built-in function
        for (int i = 0; i < arg_count; i++) {                                 // push args
            emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
        }
        int name_idx = bytecode_add_string_constant(cg->chunk, func_name);             // add name constant
        emit(cg, INST(OP_CALL_BUILTIN, result_reg, name_idx, arg_count), node->line);  // call builtin
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
        result_reg = codegen_expression(cg, node->string_interp.parts->nodes[0]);
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
    
    cg->loop_stack.is_fast = (node->for_stmt.var_name != NULL);                    // set fast flag

    if (node->for_stmt.var_name) {                                                 // named variable loop
        if (node->for_stmt.end == NULL && !node->for_stmt.condition) {             // table iteration
            int table_reg = codegen_expression(cg, node->for_stmt.start);          // evaluate table
            int var_reg = add_local(cg, node->for_stmt.var_name);                  // add loop variable
            
            emit(cg, INST(OP_TABLE_ITER_INIT, table_reg, 0, 0), node->line);       // init iterator
            
            int loop_start = bytecode_current_offset(cg->chunk);                   // loop start
            cg->loop_stack.continue_addr = loop_start;                             // set continue
            
            int iter_next_instr = bytecode_current_offset(cg->chunk);              // iter instruction
            emit(cg, INST(OP_TABLE_ITER_NEXT, var_reg, 0, 0), node->line);         // get next item
            
            codegen_block(cg, node->for_stmt.body);                                // emit body
            
            emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);                 // jump back
            
            int exit_addr = bytecode_current_offset(cg->chunk);                    // exit address
            cg->chunk->code[iter_next_instr].operands[2] = exit_addr;              // patch exit
            
            free_register(cg, table_reg);                                          // free table
            
            for (int i = prev_break_count; i < cg->loop_stack.break_count; i++) {  // patch breaks
                bytecode_patch_jump(cg->chunk, cg->loop_stack.break_jumps[i], exit_addr);
            }
        } else {                                                                    // numeric range loop
            int start_reg = codegen_expression(cg, node->for_stmt.start);           // evaluate start
            int end_reg = codegen_expression(cg, node->for_stmt.end);               // evaluate end
            int step_reg;                                                           // step register
            
            if (node->for_stmt.step) {                                              // custom step
                step_reg = codegen_expression(cg, node->for_stmt.step);             // evaluate step
            } else {                                                                // default step = 1
                step_reg = alloc_register(cg);                                      // allocate register
                emit(cg, INST(OP_LOAD_NUM_IMM, step_reg, 1, 0), node->line);        // load 1 immediate
            }

            int var_reg = add_local(cg, node->for_stmt.var_name);                   // add loop variable
            emit(cg, INST(OP_MOVE, var_reg, start_reg, 0), node->line);             // initialize
            emit(cg, INST(OP_FOR_INIT, var_reg, end_reg, step_reg), node->line);    // init for
            
            int loop_start = bytecode_current_offset(cg->chunk);                    // loop start
            cg->loop_stack.continue_addr = loop_start;                              // set continue
            
            int for_next_instr = bytecode_current_offset(cg->chunk);                // for next instr
            emit(cg, INST(OP_FOR_NEXT, var_reg, 0, 0), node->line);                 // check condition
            
            codegen_block(cg, node->for_stmt.body);                                 // emit body
            
            emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);                  // jump back
            
            int exit_addr = bytecode_current_offset(cg->chunk);                     // exit address
            cg->chunk->code[for_next_instr].operands[1] = exit_addr;                // patch exit
            
            free_register(cg, start_reg);                                           // free start
            free_register(cg, end_reg);                                             // free end
            if (!node->for_stmt.step) free_register(cg, step_reg);                  // free step if default
        }
        
        int exit_addr = bytecode_current_offset(cg->chunk);                         // exit address
        for (int i = 0; i < cg->loop_stack.break_count; i++) {                      // patch all breaks
            bytecode_patch_jump(cg->chunk, cg->loop_stack.break_jumps[i], exit_addr);
        }

    } else {                                                                        // no variable, condition loop
        ASTNode* condition = node->for_stmt.condition;                              // condition
        int left_reg = -1;                                                          // left operand reg
        int right_reg = -1;                                                         // right operand reg
        bool optimized = false;                                                     // optimized flag
        bool right_hoisted = false;                                                 // hoisted right
        Opcode jump_op = OP_JUMP;                                                   // jump opcode

        if (condition) {                                                            // has condition
            if (condition->type == AST_BINARY) {                                    // binary condition
                ApexTokenType op = condition->binary.op;                            // operator
                bool both_numbers = is_number_expression(condition->binary.left) && // check if both operands are numbers
                                    is_number_expression(condition->binary.right);
                
                switch (op) {                                                       // map to jump op
                    case TOKEN_LESS:          jump_op = OP_JUMP_IF_GTE; optimized = true; break;
                    case TOKEN_LESS_EQUAL:    jump_op = OP_JUMP_IF_GT;  optimized = true; break;
                    case TOKEN_GREATER:       jump_op = OP_JUMP_IF_LTE; optimized = true; break;
                    case TOKEN_GREATER_EQUAL: jump_op = OP_JUMP_IF_LT;  optimized = true; break;
                    case TOKEN_EQUAL_EQUAL:   jump_op = both_numbers ? OP_JUMP_IF_NEQ_NUM : OP_JUMP_IF_NEQ; optimized = true; break;  // specialized if both numbers
                    case TOKEN_NOT_EQUAL:     jump_op = both_numbers ? OP_JUMP_IF_EQ_NUM : OP_JUMP_IF_EQ; optimized = true; break;     // specialized if both numbers
                    default: break;                                                 // not optimizable
                }
                if (optimized) {                                                    // can optimize
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
                if (!right_hoisted) {                                               // not hoisted
                    right_reg = codegen_expression(cg, condition->binary.right);    // evaluate right
                }
                jump_to_end = emit(cg, INST(jump_op, 0, left_reg, right_reg), node->line);  // emit jump
                if (!right_hoisted) free_register(cg, right_reg);                   // free right
                free_register(cg, left_reg);                                        // free left
            } else {                                                                // normal condition
                int cond_reg = codegen_expression(cg, condition);                   // evaluate condition
                jump_to_end = bytecode_current_offset(cg->chunk);                   // jump address
                emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);       // jump if false
                free_register(cg, cond_reg);                                        // free condition
            }
        }

        int prev_floor = cg->register_floor;                                        // save floor
        if (right_hoisted) {                                                        // right_reg must survive
            cg->register_floor = right_reg + 1;                                     // pin it above body temps
        }
        codegen_block(cg, node->for_stmt.body);                                     // emit body
        cg->register_floor = prev_floor;                                            // restore floor
        emit(cg, INST(OP_JUMP, loop_start, 0, 0), node->line);                      // jump back
        
        int end_addr = bytecode_current_offset(cg->chunk);                          // end address
        if (jump_to_end >= 0)                                                       // patch jump
            bytecode_patch_jump(cg->chunk, jump_to_end, end_addr);
            
        for (int i = 0; i < cg->loop_stack.break_count; i++)                        // patch breaks
            bytecode_patch_jump(cg->chunk, cg->loop_stack.break_jumps[i], end_addr);
    }

    cg->loop_stack.break_count = prev_break_count;                                  // restore break count
    cg->loop_stack.continue_addr = prev_continue_addr;                              // restore continue
    cg->loop_stack.is_fast = prev_is_fast;                                          // restore fast flag
}

// main expression dispatcher with dest_hint contract
static int codegen_expression_into(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    if (!node) {                                                                     // null node
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                   // use hint or fresh
        emit(cg, INST(OP_LOAD_NONE, reg, 0, 0), 0);                                  // load none
        return reg;                                                                  // return register
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
                if (node->binary.op == TOKEN_AND) {                                  // AND
                    int left_reg = codegen_expression(cg, node->binary.left);        // evaluate left
                    int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);  // result dest
                    
                    if (result_reg != left_reg) {                                    // need to seed dest
                        emit(cg, INST(OP_MOVE, result_reg, left_reg, -1), node->line);
                    }
                    
                    int jump_idx = emit(cg, INST(OP_JUMP_IF_FALSE, 0, result_reg, -1), node->line);  // if false skip
                    
                    int right_reg = codegen_expression(cg, node->binary.right);      // evaluate right
                    if (result_reg != right_reg) {                                   // need to copy right
                        emit(cg, INST(OP_MOVE, result_reg, right_reg, -1), node->line);
                    }
                    free_register(cg, right_reg);                                    // free right
                    
                    bytecode_patch_jump(cg->chunk, jump_idx, bytecode_current_offset(cg->chunk));  // patch skip
                    
                    free_register(cg, left_reg);                                     // free left
                    return result_reg;                                               // return result
                } else {                                                             // OR
                    int left_reg = codegen_expression(cg, node->binary.left);        // evaluate left
                    int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);  // result dest
                    
                    if (result_reg != left_reg) {                                    // need to seed dest
                        emit(cg, INST(OP_MOVE, result_reg, left_reg, -1), node->line);
                    }
                    
                    int false_reg = alloc_register(cg);                              // false register
                    emit(cg, INST2(OP_LOAD_BOOL, false_reg, 0), node->line);         // load false
                    
                    int cmp_reg = alloc_register(cg);                                // compare register
                    emit(cg, INST(OP_CMP_EQ, cmp_reg, result_reg, false_reg), node->line);  // compare
                    
                    int jump_idx = emit(cg, INST(OP_JUMP_IF_FALSE, 0, cmp_reg, -1), node->line);  // if false skip
                    
                    free_register(cg, false_reg);                                    // free false
                    free_register(cg, cmp_reg);                                      // free compare
                    
                    int right_reg = codegen_expression(cg, node->binary.right);      // evaluate right
                    if (result_reg != right_reg) {                                   // need to copy right
                        emit(cg, INST(OP_MOVE, result_reg, right_reg, -1), node->line);
                    }
                    free_register(cg, right_reg);                                    // free right
                    
                    bytecode_patch_jump(cg->chunk, jump_idx, bytecode_current_offset(cg->chunk));  // patch skip
                    
                    free_register(cg, left_reg);                                     // free left
                    return result_reg;                                               // return result
                }
            }
            
            int left_reg = codegen_expression(cg, node->binary.left);               // evaluate left
            int right_reg = codegen_expression(cg, node->binary.right);             // evaluate right
            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);       // result destination
            
            bool both_numbers = is_number_expression(node->binary.left) &&          // check if both operands are numbers
                                is_number_expression(node->binary.right);
            
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
                    return result_reg;                                               // return uninitialized
            }
            emit(cg, INST(op, result_reg, left_reg, right_reg), node->line);         // emit operation
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
                    emit(cg, INST(OP_TABLE_GET, result_reg, obj_reg, local_reg), node->line);    // get by local
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
            } else {                                                                     // member expression
                int key_reg = codegen_expression(cg, node->access.member);               // evaluate key
                emit(cg, INST(OP_TABLE_GET, result_reg, obj_reg, key_reg), node->line);  // get by key
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
            
            int jump_to_false = bytecode_current_offset(cg->chunk);               // jump to false
            emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);         // jump if false
            free_register(cg, cond_reg);                                          // free condition
            
            codegen_expression_into(cg, true_expr, dest_reg);                     // write true into dest
            
            int jump_to_end = bytecode_current_offset(cg->chunk);                 // jump to end
            emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);                         // emit jump
            
            int false_addr = bytecode_current_offset(cg->chunk);                  // false address
            bytecode_patch_jump(cg->chunk, jump_to_false, false_addr);            // patch jump
            
            codegen_expression_into(cg, false_expr, dest_reg);                    // write false into dest
            
            int end_addr = bytecode_current_offset(cg->chunk);                    // end address
            bytecode_patch_jump(cg->chunk, jump_to_end, end_addr);                // patch jump
            
            return dest_reg;                                                      // return result
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
    cg->register_floor = subject_reg + 1;                                // pin subject across cases
    for (int i = 0; i < case_count; i++) {                               // emit each case body
        ASTNode* case_node = cases->nodes[i];
        int body_start = bytecode_current_offset(cg->chunk);             // body start address
        bytecode_patch_jump(cg->chunk, match_jumps[i], body_start);      // patch match jump
        codegen_block(cg, case_node->case_stmt.body);                    // emit body block
        end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);  // jump to end
    }

    if (default_case) {                                                  // emit default body
        int default_start = bytecode_current_offset(cg->chunk);          // default body start
        bytecode_patch_jump(cg->chunk, no_match_jump, default_start);    // patch no-match jump
        codegen_block(cg, default_case->case_stmt.body);                 // emit default body
        end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);  // jump to end
    }
    cg->register_floor = prev_floor;                                     // restore floor after match

    int end_addr = bytecode_current_offset(cg->chunk);                   // match end address
    if (!default_case) {
        bytecode_patch_jump(cg->chunk, no_match_jump, end_addr);         // no default: go to end
    }
    for (int i = 0; i < end_jump_count; i++) {                           // patch all end jumps
        bytecode_patch_jump(cg->chunk, end_jumps[i], end_addr);
    }

    free(match_jumps);
    free(end_jumps);
    free_register(cg, subject_reg);                                      // release subject register
}

// emits a variable declaration, writing the value directly into the local slot
static void codegen_var_decl(CodeGenerator* cg, ASTNode* node) {
    int local_reg = add_local(cg, node->var_assign.name);                    // allocate local first
    codegen_expression_into(cg, node->var_assign.value, local_reg);          // write directly

    bool need_global = (cg->current_module != NULL) ||                       // module scope: global
                       (cg->current_function == 0) ||                        // top-level: global
                       cg->current_function_has_nested;                      // nested fn may capture

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
        if (node->var_assign.value->type == AST_BINARY) {                    // binary operation
            ASTNode* bin = node->var_assign.value;                           // binary node
            if (bin->binary.left->type == AST_IDENTIFIER &&                  // x = x op y
                strcmp(bin->binary.left->identifier.name,                    // left is same var
                       node->var_assign.name) == 0) {
                
                if (bin->binary.op == TOKEN_PLUS &&                          // x = x + 1
                    bin->binary.right->type == AST_LITERAL_NUMBER &&         // right is number
                    bin->binary.right->literal_number.number_value == 1.0) {
                    emit(cg, INST(OP_INC, local_reg, 0, 0), node->line);     // in-place increment
                    return local_reg;                                        // return local
                }
                
                if (bin->binary.op == TOKEN_MINUS &&                         // x = x - 1
                    bin->binary.right->type == AST_LITERAL_NUMBER &&         // right is number
                    bin->binary.right->literal_number.number_value == 1.0) {
                    emit(cg, INST(OP_DEC, local_reg, 0, 0), node->line);     // in-place decrement
                    return local_reg;                                        // return local
                }
            }
        }

        codegen_expression_into(cg, node->var_assign.value, local_reg);      // write directly
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
    int reg = codegen_assign_expr(cg, node, -1);                             // fresh temp, no hint
    free_register(cg, reg);                                                  // discard result
}

// emits if/else if/else chain with optimized condition evaluation
static void codegen_if_statement(CodeGenerator* cg, ASTNode* node) {
    int jump_to_else = codegen_optimized_condition(cg, node->if_stmt.condition, node->line);  // try to fuse cond+jump
    if (jump_to_else < 0) {                                                  // not optimized
        int cond_reg = codegen_expression(cg, node->if_stmt.condition);      // evaluate condition
        jump_to_else = bytecode_current_offset(cg->chunk);                   // jump address
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);        // jump if false
        free_register(cg, cond_reg);                                         // free condition
    }
    
    codegen_block(cg, node->if_stmt.then_branch);                            // emit then branch
    
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
    bytecode_patch_jump(cg->chunk, jump_to_else, else_addr);                 // patch jump
    
    ASTNode* elif = node->if_stmt.elif_chain;                                // else if chain
    while (elif) {                                                           // iterate else if
        int elif_cond_reg = codegen_expression(cg, elif->if_stmt.condition); // evaluate condition
        int jump_to_next = bytecode_current_offset(cg->chunk);               // jump to next
        emit(cg, INST(OP_JUMP_IF_FALSE, 0, elif_cond_reg, 0), elif->line);   // jump if false
        free_register(cg, elif_cond_reg);                                    // free condition
        
        codegen_block(cg, elif->if_stmt.then_branch);                        // emit else if body
        
        bool elif_is_last = (elif->if_stmt.elif_chain == NULL) && (else_branch == NULL);
        if (!elif_is_last) {                                                 // not the trailing branch
            end_jumps[end_jump_count++] = bytecode_current_offset(cg->chunk);  // save position
            emit(cg, INST(OP_JUMP, 0, 0, 0), elif->line);                    // jump to end
        }
        
        int next_addr = bytecode_current_offset(cg->chunk);                  // next address
        bytecode_patch_jump(cg->chunk, jump_to_next, next_addr);             // patch jump
        
        elif = elif->if_stmt.elif_chain;                                     // next else if
    }
    
    if (else_branch) {                                                       // has else
        codegen_block(cg, else_branch);                                      // emit else
    }
    
    int end_addr = bytecode_current_offset(cg->chunk);                       // end address
    for (int i = 0; i < end_jump_count; i++) {                               // patch all jumps
        bytecode_patch_jump(cg->chunk, end_jumps[i], end_addr);
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
    
    Opcode jump_op;                                                          // jump opcode for general case
    
    bool both_numbers = is_number_expression(left) &&                        // check if both operands are numbers
                        is_number_expression(right);
    
    switch (op) {                                                            // map to jump
        case TOKEN_LESS:          jump_op = OP_JUMP_IF_GTE; break;
        case TOKEN_LESS_EQUAL:    jump_op = OP_JUMP_IF_GT;  break;
        case TOKEN_GREATER:       jump_op = OP_JUMP_IF_LTE; break;
        case TOKEN_GREATER_EQUAL: jump_op = OP_JUMP_IF_LT;  break;
        case TOKEN_EQUAL_EQUAL:   jump_op = both_numbers ? OP_JUMP_IF_NEQ_NUM : OP_JUMP_IF_NEQ; break;  // specialized
        case TOKEN_NOT_EQUAL:     jump_op = both_numbers ? OP_JUMP_IF_EQ_NUM : OP_JUMP_IF_EQ; break;    // specialized
        default: return -1;                                                  // not optimizable
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
    
    int key_reg = codegen_expression(cg, final_acc->access.member);          // evaluate key
    emit(cg, INST(OP_TABLE_SET, current_obj_reg, key_reg, val_reg), final_acc->line);  // set
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

    int jump_over = bytecode_current_offset(cg->chunk);                      // jump over function body
    emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);                            // emit jump
    
    int func_addr = bytecode_current_offset(cg->chunk);                      // function address
    cg->chunk->functions[func_idx].address = func_addr;                      // set address

    int prev_function = cg->current_function;                                // save current function
    int prev_next_register = cg->next_register;                              // save next register
    int prev_max_registers = cg->max_registers;                              // save max registers
    bool prev_has_nested = cg->current_function_has_nested;                  // save nested flag
    int saved_count = cg->locals.count;                                      // save local count
    char** saved_names = NULL;                                               // saved names
    int* saved_regs = NULL;                                                  // saved registers

    if (saved_count > 0) {                                                   // have locals
        saved_names = (char**)malloc(sizeof(char*) * saved_count);           // allocate names
        saved_regs = (int*)malloc(sizeof(int) * saved_count);                // allocate regs
        for (int i = 0; i < saved_count; i++) {                              // copy locals
            saved_names[i] = strdup(cg->locals.names[i]);                    // copy name
            saved_regs[i] = cg->locals.registers[i];                         // copy reg
        }
    }

    for (int i = 0; i < cg->locals.count; i++) free(cg->locals.names[i]);    // free local names
    free(cg->locals.names);                                                  // free names array
    free(cg->locals.registers);                                              // free registers array
    cg->locals.names = NULL;                                                 // clear names
    cg->locals.registers = NULL;                                             // clear regs
    cg->locals.count = 0;                                                    // reset count
    cg->locals.capacity = 0;                                                 // reset capacity
    cg->next_register = 0;                                                   // reset next reg
    cg->max_registers = 0;                                                   // reset max regs for new function
    cg->current_function = func_idx;                                         // set current function
    cg->current_function_has_nested =                                        // detect nested function decls
        block_has_function_decl(node->function_decl.body);

    for (int i = 0; i < param_count; i++) {                                  // parameters
        ASTNode* param = node->function_decl.params->nodes[i];               // param node
        add_local(cg, param->param.name);                                    // add as local
    }

    codegen_block(cg, node->function_decl.body);                             // emit body

    bool ends_with_return = false;                                           // return flag
    if (cg->chunk->code_count > 0) {                                         // has code
        Instruction* last = &cg->chunk->code[cg->chunk->code_count - 1];     // last instruction
        if (last->opcode == OP_RETURN || last->opcode == OP_RETURN_NONE ||   // return type
            last->opcode == OP_RETURN_NUM) {
            ends_with_return = true;                                         // has return
        }
    }

    if (!ends_with_return) {
        emit(cg, INST(OP_RETURN_NONE, 0, 0, 0), node->line);                 // implicit return none
    }

    cg->chunk->functions[func_idx].local_count = cg->locals.count;           // store local count
    cg->chunk->functions[func_idx].max_registers = cg->max_registers;        // store max regs
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
    
    cg->locals.names = saved_names;                                          // restore names
    cg->locals.registers = saved_regs;                                       // restore regs
    cg->locals.count = saved_count;                                          // restore count
    cg->locals.capacity = saved_count;                                       // restore capacity
    cg->next_register = prev_next_register;                                  // restore next reg
    cg->max_registers = prev_max_registers;                                  // restore max regs
    cg->current_function = prev_function;                                    // restore function
    cg->current_function_has_nested = prev_has_nested;                       // restore nested flag

    bytecode_patch_jump(cg->chunk, jump_over, bytecode_current_offset(cg->chunk));  // patch jump

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
        int value_reg = codegen_expression(cg, node->return_stmt.value);     // evaluate value
        
        ASTNode* val = node->return_stmt.value;                              // value node
        bool is_number = false;                                              // guaranteed number flag
        
        if (val->type == AST_LITERAL_NUMBER) {                               // number literal
            is_number = true;
        } else if (val->type == AST_BINARY) {                                // binary op
            ApexTokenType op = val->binary.op;                               // operator
            if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR || // arithmetic
                op == TOKEN_SLASH || op == TOKEN_PERCENT) {
                is_number = true;
            }
        } else if (val->type == AST_UNARY && val->unary.op == TOKEN_MINUS) { // unary minus
            is_number = true;
        }
        
        if (is_number) {                                                     // guaranteed number
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
    for (int i = 0; i < node->block.statements->count; i++) {                // iterate statements
        ASTNode* stmt = node->block.statements->nodes[i];                    // current statement
        codegen_statement(cg, stmt);                                         // emit statement
        int reset_to = locals_high_water(cg);                                // keep locals + pinned
        if (reset_to < cg->register_floor) reset_to = cg->register_floor;    // respect floor
        if (cg->next_register > reset_to) {                                  // drop temps only
            cg->next_register = reset_to;                                    // reclaim for next stmt
        }
    }
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
    
    cg->chunk->functions[0].max_registers = cg->max_registers;               // store the max registers
    
    emit(cg, INST(OP_HALT, 0, 0, 0), 0);                                     // halt instruction
    return true;                                                             // success
}
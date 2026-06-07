#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "bytecode.h"
#include <stdbool.h>

typedef struct {
    BytecodeChunk* chunk;
    
    // Variable-to-register-slot mapping
    struct {
        char** names;
        int* registers;
        int count;
        int capacity;
    } locals;

    // Loop control stack
    struct {
        int* break_jumps;
        int break_count;
        int break_capacity;
        int continue_addr;
        bool is_fast;
    } loop_stack;

    // Register allocator state
    int next_register;
    int max_registers;

    // Currently compiling function index
    int current_function;

    // Unique label counter
    int label_counter;

    // Optimization: cached registers for common operations
    struct {
        int zero_reg;
        int one_reg;
        int empty_str;
    } cache;

    // Module tracking
    char* current_module;
    char** imported_modules;
    int module_count;
    int module_capacity;
    char** module_globals;
    int module_globals_count;
    int module_globals_capacity;

} CodeGenerator;

// API
CodeGenerator* codegen_create(BytecodeChunk* chunk);
void codegen_destroy(CodeGenerator* cg);
bool codegen_generate(CodeGenerator* cg, ASTNode* ast);

// Debug utility
void codegen_print_locals(CodeGenerator* cg);

#endif // CODEGEN_H
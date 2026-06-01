#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "bytecode.h"

typedef struct {
    BytecodeChunk* chunk;

    // Variable-to-register-slot mapping
    struct {
        char** names;
        int* registers;
        int count;
        int capacity;
    } locals;
    
    // Break/continue address stack
    struct {
        int* break_addrs;
        int* continue_addrs;
        bool* is_fast;
        int count;
        int capacity;
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
        int zero_reg;    // register holding constant 0
        int one_reg;     // register holding constant 1
        int empty_str;   // register holding empty string
    } cache;
    
} CodeGenerator;

// API
CodeGenerator* codegen_create(BytecodeChunk* chunk);
void codegen_destroy(CodeGenerator* cg);
bool codegen_generate(CodeGenerator* cg, ASTNode* ast);

// Debug utility
void codegen_print_locals(CodeGenerator* cg);

#endif // CODEGEN_H
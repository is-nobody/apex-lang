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
        // Dynamic array of instruction offsets for 'break' jumps that need patching
        int* break_jumps;      
        int break_count;       // Number of pending breaks in the current loop scope
        int break_capacity;
        
        int continue_addr;     // Target address for 'continue' (usually loop start)
        
        bool is_fast;          // True if using fast iterator (numeric for-loop)
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
#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "bytecode.h"
#include <stdbool.h>

// code generator context holding all state needed during bytecode emission
typedef struct {
    BytecodeChunk* chunk;          // target bytecode chunk being populated
    
    struct {
        char** names;              // local variable names for debug info
        int* registers;            // register slot assigned to each local
        int count;
        int capacity;
    } locals;                      // maps local variable names to register slots

    struct {
        int* break_jumps;          // list of jump instructions to patch on loop exit
        int break_count;
        int break_capacity;
        int continue_addr;         // target address for continue statements
        bool is_fast;              // whether the loop uses fast range optimization
    } loop_stack;                  // stack of active loops for break/continue resolution

    int next_register;             // next free register for allocation
    int max_registers;             // highest register used so far

    int current_function;          // index of the function currently being compiled

    int label_counter;             // unique id generator for synthetic labels

    struct {
        int zero_reg;              // cached register holding 0
        int one_reg;               // cached register holding 1
        int empty_str;             // cached register holding empty string
    } cache;                       // commonly used constants kept in registers

    char* current_module;          // name of the module being compiled
    char** imported_modules;       // list of imported module names
    int module_count;
    int module_capacity;
    char** module_globals;         // global variables specific to the current module
    int module_globals_count;
    int module_globals_capacity;

} CodeGenerator;

// creates a new code generator attached to a bytecode chunk
CodeGenerator* codegen_create(BytecodeChunk* chunk);

// frees all resources used by the code generator
void codegen_destroy(CodeGenerator* cg);

// generates bytecode from an ast, returns true on success
bool codegen_generate(CodeGenerator* cg, ASTNode* ast);

#endif // CODEGEN_H
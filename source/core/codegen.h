#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "bytecode.h"
#include <stdbool.h>

// code generator context holding all state needed during bytecode emission
typedef struct {
    BytecodeChunk* chunk;          // target bytecode chunk being populated with instructions

    struct {
        char** names;              // local variable names for debug information
        int* registers;            // register slot assigned to each local variable
        int count;                 // number of locals in the current scope
        int capacity;              // allocated capacity of the local arrays
    } locals;                      // maps local variable names to their register slots

    struct {
        int* break_jumps;          // list of jump instruction offsets to patch on loop exit
        int break_count;           // number of pending break jumps
        int break_capacity;        // allocated capacity of break_jumps array
        int continue_addr;         // instruction offset for continue statements to jump to
        bool is_fast;              // whether the current loop uses fast range optimization
    } loop_stack;                  // stack of active loops for break/continue resolution

    int next_register;             // next free register index for allocation
    int max_registers;             // highest register index used so far (for frame sizing)

    int current_function;          // index of the function currently being compiled

    int label_counter;             // unique identifier generator for synthetic labels

    struct {
        int zero_reg;              // cached register holding the constant 0
        int one_reg;               // cached register holding the constant 1
        int empty_str;             // cached register holding the empty string ""
    } cache;                       // commonly used constants kept in registers for efficiency

    char* current_module;          // name of the module currently being compiled
    char** imported_modules;       // list of imported module names for name resolution
    int module_count;              // number of imported modules
    int module_capacity;           // allocated capacity of imported_modules array
    char** module_globals;         // global variables specific to the current module
    int module_globals_count;      // number of module-specific globals
    int module_globals_capacity;   // allocated capacity of module_globals array
} CodeGenerator;

// creates a new code generator attached to a bytecode chunk
CodeGenerator* codegen_create(BytecodeChunk* chunk);

// frees all resources used by the code generator
void codegen_destroy(CodeGenerator* cg);

// generates bytecode from an ast, returns true on success
bool codegen_generate(CodeGenerator* cg, ASTNode* ast);

#endif // CODEGEN_H
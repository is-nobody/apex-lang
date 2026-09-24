// source/compiler/codegen.h
// Implementation of Bytecode Code Generation for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "bytecode.h"
#include <stdbool.h>
#include <stdint.h>

// code generator context holding all state needed during bytecode emission
typedef struct {
    BytecodeChunk* chunk;          // target bytecode chunk being populated with instructions

    struct {
        char** names;              // local variable names for debug information
        int* registers;            // register slot assigned to each local variable
        bool* is_number;           // per-slot: true when the register is known to hold a number right now
        bool* is_integer;          // per-slot: true when the register is known to hold a whole number
        bool* const_known;         // per-slot: true when the value is a known numeric constant
        double* const_value;       // the constant value when const_known is true
        int count;                 // number of locals in the current scope
        int capacity;              // allocated capacity of the local arrays
    } locals;                      // maps local variable names to their register slots

    struct {
        bool    active;      // true while emitting a numeric-for body with hoisted constants
        double* values;      // hoisted constant values (deduped)
        int*    regs;        // register holding each hoisted constant
        int     count;       // number of hoisted constants
        int     capacity;    // allocated capacity of values/regs

        const char** get_names;    // table identifier
        double*      get_indices;  // constant index value
        int*         get_regs;     // register holding the loaded value
        int          get_count;
        int          get_capacity;
    } hoist;

    struct {
        double* values;      // deduped numeric constants reused across statements
        int*    regs;        // register holding each cached constant
        int     count;       // number of cached constants
        int     capacity;    // allocated capacity of values/regs
    } num_cache;

    struct {
        char**  values;      // deduped string literals (owned copies), reused across statements
        int*    regs;        // register holding each cached string
        int     count;       // number of cached strings
        int     capacity;    // allocated capacity of values/regs
    } str_cache;

    struct {
        struct {
            Opcode op;        // arithmetic opcode that produced this value
            int left_reg;     // left operand register
            int right_reg;    // right operand register, -1 for *_IMM
            int imm;          // immediate operand, ignored when right_reg >= 0
            int result_reg;   // register currently holding the result
        } entries[16];        // small fixed cache, capped to bound pressure
        int count;            // number of live entries
    } imm_lvn;                // per-statement value numbering for arithmetic results

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
    bool current_function_has_nested;  // true if current function's body contains a nested function declaration
    int register_floor;            // minimum next_register preserved by codegen_block resets
    int cache_floor;               // persistent floor pinned by the numeric-constant cache
    int for_scope_depth;           // nesting depth of for-scopes; used to decide local vs global
    int unswitch_depth;            // recursion depth of loop unswitching; caps body duplication
    int loop_depth;                // nesting depth of active for-loops (used to gate copy propagation)
    uint8_t* jump_targets;         // jump_targets[pc] == 1 when some jump in the chunk targets pc
    int      jump_targets_cap;     // allocated size of jump_targets

    struct {
        ASTNodeList* stmts;        // statements of the enclosing block
        int          index;        // index of the statement being emitted in that block
    } block_stack[32];             // stack of enclosing blocks for lookahead liveness
    int block_depth;               // number of frames in block_stack

    int label_counter;             // unique identifier generator for synthetic labels

    bool current_call_is_awaited;  // set by AST_AWAIT handling before codegen_call
    char* current_module;          // name of the module currently being compiled
    char** imported_modules;       // list of imported module names for name resolution
    int module_count;              // number of imported modules
    int module_capacity;           // allocated capacity of imported_modules array
    char** module_globals;         // global variables specific to the current module
    int module_globals_count;      // number of module-specific globals
    int module_globals_capacity;   // allocated capacity of module_globals array

    ASTNode** fn_decls;            // AST_FUNCTION_DECL for each compiled function (indexed by func_idx)
    int       fn_decls_cap;        // allocated size of fn_decls
} CodeGenerator;

// creates a new code generator attached to a bytecode chunk
CodeGenerator* codegen_create(BytecodeChunk* chunk);

// frees all resources used by the code generator
void codegen_destroy(CodeGenerator* cg);

// generates bytecode from an ast, returns true on success
bool codegen_generate(CodeGenerator* cg, ASTNode* ast);

#endif // CODEGEN_H
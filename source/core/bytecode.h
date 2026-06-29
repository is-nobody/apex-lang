#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include <stdbool.h>

// all supported vm opcodes, grouped by functionality for clarity
typedef enum {
    OP_NOP,              // no operation, used as a placeholder or padding
    OP_LOAD_CONST,       // loads a constant from the pool into a register
    OP_MOVE,             // copies a value from one register to another
    
    OP_ADD,              // arithmetic addition: rdst = rleft + rright
    OP_SUB,              // subtraction: rdst = rleft - rright
    OP_MUL,              // multiplication: rdst = rleft * rright
    OP_DIV,              // division: rdst = rleft / rright
    OP_MOD,              // modulo: rdst = rleft % rright
    OP_NEG,              // unary negation: rdst = -rsrc
    
    OP_CMP_EQ,           // equality comparison: rdst = (rleft == rright)
    OP_CMP_NEQ,          // inequality: rdst = (rleft != rright)
    OP_CMP_LT,           // less-than: rdst = (rleft < rright)
    OP_CMP_GT,           // greater-than: rdst = (rleft > rright)
    OP_CMP_LTE,          // less-or-equal: rdst = (rleft <= rright)
    OP_CMP_GTE,          // greater-or-equal: rdst = (rleft >= rright)
    
    OP_AND,              // logical and: rdst = rleft && rright
    OP_OR,               // logical or: rdst = rleft || rright
    OP_NOT,              // logical not: rdst = !rsrc
    
    OP_TO_NUMBER,        // converts rsrc to a number, storing in rdst
    OP_TO_STRING,        // converts rsrc to a string, storing in rdst
    OP_TO_BOOL,          // converts rsrc to a boolean, storing in rdst
    
    OP_JUMP,             // unconditional branch to address
    OP_JUMP_IF_TRUE,     // branch if rcond is truthy
    OP_JUMP_IF_FALSE,    // branch if rcond is falsy
    
    OP_CALL,             // calls a function at address, result goes to rdst
    OP_CALL_BUILTIN,     // calls a built-in by index, result to rdst
    OP_RETURN,           // returns a value from the current function
    OP_RETURN_VOID,      // returns without a value
    OP_PUSH_ARG,         // pushes an argument onto the call stack

    OP_LOAD_GLOBAL,      // loads a global variable into a register
    OP_STORE_GLOBAL,     // stores a register value into a global variable
    
    OP_NEW_TABLE,        // creates a new empty table in rdst
    OP_TABLE_SET,        // table[key_reg] = value_reg
    OP_TABLE_SET_CONST,  // table[constant_key] = value_reg
    OP_TABLE_GET,        // rdst = table[key_reg]
    OP_TABLE_GET_CONST,  // rdst = table[constant_key]
    OP_TABLE_APPEND,     // appends a value to a table as a positional item
    
    OP_CONCAT,           // string concatenation: rdst = rleft + rright

    OP_POP_ITER,         // cleans up iterator state when leaving a loop
    OP_JUMP_IF_LT,       // branch if r[op1] < r[op2] (for range loops)
    OP_JUMP_IF_LTE,      // branch if r[op1] <= r[op2]
    OP_JUMP_IF_GT,       // branch if r[op1] > r[op2]
    OP_JUMP_IF_GTE,      // branch if r[op1] >= r[op2]
    OP_JUMP_IF_EQ,       // branch if r[op1] == r[op2]
    OP_JUMP_IF_NEQ,      // branch if r[op1] != r[op2]
    OP_FOR_INIT,         // initializes a numeric for-loop state
    OP_FOR_NEXT,         // advances loop and branches if the end is reached
    
    OP_HALT,             // stops vm execution
    
    OP_LOAD_BOOL,        // loads true/false directly with an immediate operand
    OP_LOAD_CONST_NUM,   // loads an immediate number into rdst (optimized path)

    OP_COUNT,            // total number of opcodes, used for bounds checking
} Opcode;

// fixed-size 16-byte instruction (opcode + three 32-bit operands) for fast decoding
typedef struct {
    Opcode opcode;
    int32_t operands[3]; // usually: dst, src, extra / jump target / constant index
} Instruction;

// convenience macros for constructing instructions with varying operand counts
#define INST(op, a, b, c) ((Instruction){op, {a, b, c}})
#define INST2(op, a, b)   ((Instruction){op, {a, b, 0}})
#define INST1(op, a)      ((Instruction){op, {a, 0, 0}})
#define INST0(op)         ((Instruction){op, {0, 0, 0}})

// limits for vm resource pools to prevent unbounded growth
#define MAX_REGISTERS 256
#define MAX_CONSTANTS 65536
#define MAX_GLOBALS   65536
#define MAX_LOCALS    256

// types of values that can be stored in the constant pool
typedef enum {
    CONST_NUMBER,        // double-precision floating point
    CONST_STRING,        // interned string pointer
    CONST_BOOL,          // boolean true/false
    CONST_FUNCTION,      // function index into the function table
} ConstantType;

// a constant pool entry with a type and a type-specific value
typedef struct {
    ConstantType type;
    union {
        double number_value;
        char* string_value;
        bool bool_value;
        int function_index;
    };
} Constant;

// global variable entry with a name and its index in the globals table
typedef struct {
    char* name;
    int index;
} GlobalVar;

// local variable entry with name, slot, and scope depth for debug info
typedef struct {
    char* name;
    int slot;
    int scope_level;
} LocalVar;

// function metadata: name, entry address, arity, and local variable info
typedef struct {
    char* name;
    int address;
    int arity;
    int local_count;
    char** local_names;
} FunctionInfo;

// bytecode chunk holds all code, constants, globals, functions, and debug data
typedef struct {
    Instruction* code;
    int code_capacity;
    int code_count;
    
    Constant* constants;
    int const_capacity;
    int const_count;
    
    GlobalVar* globals;
    int global_capacity;
    int global_count;
    
    FunctionInfo* functions;
    int func_capacity;
    int func_count;
    
    int current_function;
    
    int* line_info;
    int line_capacity;
    int line_count;
    
    struct {
        char** strings;
        int count;
        int capacity;
    } string_pool;
    
} BytecodeChunk;

// creates a new bytecode chunk with default capacities
BytecodeChunk* bytecode_create();

// frees all memory used by a bytecode chunk
void bytecode_destroy(BytecodeChunk* chunk);

// emits an instruction, returning its offset in the code buffer
int bytecode_emit(BytecodeChunk* chunk, Instruction inst);

// emits an instruction with source line info for debugging
int bytecode_emit_line(BytecodeChunk* chunk, Instruction inst, int line);

// adds a constant to the pool, deduplicating identical values
int bytecode_add_constant(BytecodeChunk* chunk, Constant constant);

// convenience wrappers for adding typed constants
int bytecode_add_number_constant(BytecodeChunk* chunk, double value);
int bytecode_add_string_constant(BytecodeChunk* chunk, const char* value);
int bytecode_add_bool_constant(BytecodeChunk* chunk, bool value);

// adds or retrieves a global variable by name
int bytecode_add_global(BytecodeChunk* chunk, const char* name);
int bytecode_get_global(BytecodeChunk* chunk, const char* name);

// registers a function with its name and arity
int bytecode_add_function(BytecodeChunk* chunk, const char* name, int arity);

// interns a string in the pool to share identical string instances
const char* bytecode_intern_string(BytecodeChunk* chunk, const char* str);

// patches a jump instruction to a resolved target address
void bytecode_patch_jump(BytecodeChunk* chunk, int jump_instruction, int target_address);

// returns the current code offset (useful for jump targets)
int bytecode_current_offset(BytecodeChunk* chunk);

// returns the human-readable name of an opcode
const char* opcode_name(Opcode op);

#endif // BYTECODE_H
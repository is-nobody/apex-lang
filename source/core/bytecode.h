#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include <stdbool.h>

// all supported vm opcodes, grouped by functionality for clarity
typedef enum {
    OP_MOVE,             // copies a value from one register to another
    OP_LOAD_CONST,       // loads a constant from the pool into a register
    OP_LOAD_CONST_NUM,   // loads an immediate number into rdst (optimized path)
    OP_LOAD_BOOL,        // loads true/false directly with an immediate operand

    OP_ADD,              // arithmetic addition: rdst = rleft + rright
    OP_SUB,              // subtraction: rdst = rleft - rright
    OP_MUL,              // multiplication: rdst = rleft * rright
    OP_DIV,              // division: rdst = rleft / rright
    OP_MOD,              // modulo: rdst = rleft % rright
    OP_NEG,              // unary negation: rdst = -rsrc

    OP_JUMP,             // unconditional branch to address
    OP_JUMP_IF_FALSE,    // branch if rcond is falsy
    OP_JUMP_IF_EQ,       // branch if r[op1] == r[op2]
    OP_JUMP_IF_NEQ,      // branch if r[op1] != r[op2]
    OP_JUMP_IF_LT,       // branch if r[op1] < r[op2] (for range loops)
    OP_JUMP_IF_GT,       // branch if r[op1] > r[op2]
    OP_JUMP_IF_LTE,      // branch if r[op1] <= r[op2]
    OP_JUMP_IF_GTE,      // branch if r[op1] >= r[op2]

    OP_CMP_EQ,           // equality comparison: rdst = (rleft == rright)
    OP_CMP_NEQ,          // inequality: rdst = (rleft != rright)
    OP_CMP_LT,           // less-than: rdst = (rleft < rright)
    OP_CMP_GT,           // greater-than: rdst = (rleft > rright)
    OP_CMP_LTE,          // less-or-equal: rdst = (rleft <= rright)
    OP_CMP_GTE,          // greater-or-equal: rdst = (rleft >= rright)

    OP_FOR_INIT,         // initializes a numeric for-loop state
    OP_FOR_NEXT,         // advances loop and branches if the end is reached
    OP_TABLE_ITER_INIT,  // initialize table iterator for "for key = table" loops
    OP_TABLE_ITER_NEXT,  // advance table iterator, yield next key into register
    OP_POP_ITER,         // cleans up iterator state when leaving a loop

    OP_TABLE_GET,        // rdst = table[key_reg]
    OP_TABLE_GET_CONST,  // rdst = table[constant_key]
    OP_TABLE_SET,        // table[key_reg] = value_reg
    OP_TABLE_SET_CONST,  // table[constant_key] = value_reg
    OP_TABLE_APPEND,     // appends a value to a table as a positional item
    OP_NEW_TABLE,        // creates a new empty table in rdst

    OP_CONCAT,           // string concatenation: rdst = rleft + rright

    OP_AND,              // logical and: rdst = rleft && rright
    OP_OR,               // logical or: rdst = rleft || rright
    OP_NOT,              // logical not: rdst = !rsrc

    OP_PUSH_ARG,         // pushes an argument onto the call stack
    OP_CALL,             // calls a function at address, result goes to rdst
    OP_CALL_BUILTIN,     // calls a built-in by index, result to rdst
    OP_RETURN,           // returns a value from the current function
    OP_RETURN_VOID,      // returns without a value

    OP_CALL_0,           // calling a function with 0 arguments (fast way)
    OP_CALL_1,           // calling a function with 1 arguments (fast way)
    OP_CALL_2,           // calling a function with 2 arguments (fast way)
    OP_RETURN_NUM,       // return a number (without refcounting)

    OP_LOAD_GLOBAL,      // loads a global variable into a register
    OP_STORE_GLOBAL,     // stores a register value into a global variable

    OP_HALT,             // stops vm execution

    OP_COUNT,            // total number of opcodes, used for bounds checking
} Opcode;

// fixed-size 16-byte instruction (opcode + three 32-bit operands) for fast decoding
typedef struct {
    Opcode opcode;               // instruction type (ADD, LOAD_CONST, CALL, etc.)
    int32_t operands[3];         // operands: dst register, src register, and extra (jump target, constant index, etc.)
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
    CONST_NONE,          // none/null constant
    CONST_BOOL,          // boolean true/false
    CONST_FUNCTION,      // function index into the function table
} ConstantType;

// a constant pool entry with a type and a type-specific value
typedef struct {
    ConstantType type;           // discriminator for the union below
    union {
        double number_value;     // numeric constant (integers and floats)
        char* string_value;      // string constant (interned, pointer to string pool)
        bool bool_value;         // boolean constant (true/false)
        int function_index;      // function constant (index into functions table)
    };
} Constant;

// global variable entry with a name and its index in the globals table
typedef struct {
    char* name;                  // global variable name for lookup
    int index;                   // unique index assigned to this global
} GlobalVar;

// local variable entry with name, slot, and scope depth for debug info
typedef struct {
    char* name;                  // local variable name
    int slot;                    // register slot where the variable resides
    int scope_level;             // lexical scope depth for debug information
} LocalVar;

// function metadata: name, entry address, arity, and local variable info
typedef struct {
    char* name;                  // function name for debugging and call resolution
    int address;                 // bytecode offset where the function begins
    int arity;                   // number of parameters the function expects
    int local_count;             // number of local variables in the function
    char** local_names;          // local variable names for debugging (debug info)
} FunctionInfo;

// bytecode chunk holds all code, constants, globals, functions, and debug data
typedef struct {
    Instruction* code;           // dynamically growing array of bytecode instructions
    int code_capacity;           // allocated capacity of the code array
    int code_count;              // number of instructions currently emitted

    Constant* constants;         // constant pool (numbers, strings, functions, etc.)
    int const_capacity;          // allocated capacity of the constants array
    int const_count;             // number of constants stored

    GlobalVar* globals;          // global variable table with names and indices
    int global_capacity;         // allocated capacity of the globals array
    int global_count;            // number of global variables registered

    FunctionInfo* functions;     // function metadata (name, address, arity, locals)
    int func_capacity;           // allocated capacity of the functions array
    int func_count;              // number of functions defined in this chunk

    int current_function;        // index of the function currently being compiled

    int* line_info;              // source line numbers for each instruction (debugging)
    int line_capacity;           // allocated capacity of the line_info array
    int line_count;              // number of line entries (matches code_count)

    struct {
        char** strings;          // string interning pool for deduplication
        int count;               // number of interned strings
        int capacity;            // allocated capacity of the string pool
    } string_pool;               // shared string storage to reduce memory duplication
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
int bytecode_add_none_constant(BytecodeChunk* chunk);
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
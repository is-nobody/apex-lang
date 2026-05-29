#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include <stdbool.h>

// ========== Opcode Definitions ==========
typedef enum {
    // === Stack & Register Management ===
    OP_NOP,              // no operation
    OP_LOAD_CONST,       // Rdest = constant[index]
    OP_MOVE,             // Rdest = Rsrc
    
    // === Arithmetic (3-address, register-based) ===
    OP_ADD,              // Rdest = Rleft + Rright
    OP_SUB,              // Rdest = Rleft - Rright
    OP_MUL,              // Rdest = Rleft * Rright
    OP_DIV,              // Rdest = Rleft / Rright
    OP_MOD,              // Rdest = Rleft % Rright
    OP_NEG,              // Rdest = -Rsrc (unary minus)
    
    // === Comparison (returns boolean in Rdest) ===
    OP_CMP_EQ,           // Rdest = (Rleft == Rright)
    OP_CMP_NEQ,          // Rdest = (Rleft != Rright)
    OP_CMP_LT,           // Rdest = (Rleft < Rright)
    OP_CMP_GT,           // Rdest = (Rleft > Rright)
    OP_CMP_LTE,          // Rdest = (Rleft <= Rright)
    OP_CMP_GTE,          // Rdest = (Rleft >= Rright)
    
    // === Logical ===
    OP_AND,              // Rdest = Rleft AND Rright
    OP_OR,               // Rdest = Rleft OR Rright
    OP_NOT,              // Rdest = NOT Rsrc
    
    // === Type Conversion ===
    OP_TO_NUMBER,        // Rdest = number(Rsrc)
    OP_TO_STRING,        // Rdest = string(Rsrc)
    OP_TO_BOOL,          // Rdest = bool(Rsrc)
    
    // === Control Flow ===
    OP_JUMP,             // PC = address (unconditional jump)
    OP_JUMP_IF_TRUE,     // if Rcond then PC = address
    OP_JUMP_IF_FALSE,    // if !Rcond then PC = address
    
    // === Functions ===
    OP_CALL,             // call function at address, store result in Rdest
    OP_CALL_BUILTIN,     // call builtin by index, result in Rdest
    OP_RETURN,           // return Rvalue
    OP_RETURN_VOID,      // return without value
    OP_PUSH_ARG,         // push argument onto call stack

    // === Memory Operations ===
    OP_LOAD_GLOBAL,      // Rdest = global[name_index]
    OP_STORE_GLOBAL,     // global[name_index] = Rsrc
    OP_LOAD_LOCAL,       // Rdest = local[slot]
    OP_STORE_LOCAL,      // local[slot] = Rsrc
    OP_LOAD_UPVALUE,     // Rdest = upvalue[slot] (closure support)
    OP_STORE_UPVALUE,    // upvalue[slot] = Rsrc
    
    // === Table Operations ===
    OP_NEW_TABLE,        // Rdest = new table()
    OP_TABLE_SET,        // table[key_reg] = value_reg
    OP_TABLE_SET_CONST,  // table[const_idx] = value (constant key)
    OP_TABLE_GET,        // Rdest = table[key_reg]
    OP_TABLE_GET_CONST,  // Rdest = table[const_idx] (constant key)
    OP_TABLE_APPEND,     // table.append(value_reg)
    OP_TABLE_LEN,        // Rdest = length(table)
    
    // === String Operations ===
    OP_CONCAT,           // Rdest = Rleft + Rright (string concatenation)
    OP_STRING_INTERP,    // Rdest = interpolate template with values
    
    // === Loop Optimizations ===
    OP_FOR_PREP,         // prepare for-in loop iteration
    OP_POP_ITER,         // clean up loop state
    OP_JUMP_IF_LT,       // if R[op1] <  R[op2] then jump op0
    OP_JUMP_IF_LTE,      // if R[op1] <= R[op2] then jump op0
    OP_JUMP_IF_GT,       // if R[op1] >  R[op2] then jump op0
    OP_JUMP_IF_GTE,      // if R[op1] >= R[op2] then jump op0
    OP_JUMP_IF_EQ,       // if R[op1] == R[op2] then jump op0
    OP_JUMP_IF_NEQ,      // if R[op1] != R[op2] then jump op0
    OP_FOR_INIT,         // initialize for-loop (faster than CALL range())
    OP_FOR_NEXT,         // get next element, jump if end reached
    OP_BREAK,            // exit loop
    OP_CONTINUE,         // next iteration
    
    // === Error Handling ===
    OP_TRY,              // begin try block (install handler)
    OP_CATCH,            // begin catch block
    OP_THROW,            // throw exception
    OP_END_TRY,          // end of try/catch block
    
    // === Misc ===
    OP_PRINT,            // print Rvalue to stdout
    OP_HALT,             // stop VM execution
    
    // === Extended (optimizations) ===
    OP_ADD_IMM,          // Rdest = Rleft + immediate
    OP_LOAD_BOOL,        // Rdest = true/false (fast bool load)
    OP_DUP,              // Rdest = Rsrc (duplicate register)
    OP_SWAP,             // swap(Ra, Rb)
    
    OP_INC_GLOBAL,       // global[op1] += R[op2]
    OP_ADD_GLOBAL,       // global[op1] = R[op2] + R[op3]
    OP_LOAD_CONST_NUM,   // R[op0] = op1 (immediate number)
    OP_JUMP_IF_NUM,      // if R[op1] < op2 then jump op0

    OP_COUNT,
} Opcode;

// ========== Instruction Format ==========
// Fixed-size instructions for fast decoding and cache-friendly access.
// Each instruction is exactly 16 bytes (4 x int32_t).

typedef struct {
    Opcode opcode;       // 4 bytes
    int32_t operands[3]; // 12 bytes (3 operands, 4 bytes each)
    // operands[0] = destination register / jump offset
    // operands[1] = source register / constant index
    // operands[2] = extra operand / flags
} Instruction;

// Convenience macros for creating instructions
#define INST(op, a, b, c) ((Instruction){op, {a, b, c}})
#define INST2(op, a, b)   ((Instruction){op, {a, b, 0}})
#define INST1(op, a)      ((Instruction){op, {a, 0, 0}})
#define INST0(op)         ((Instruction){op, {0, 0, 0}})

// ========== Bytecode Chunk ==========
#define MAX_REGISTERS 256
#define MAX_CONSTANTS 65536
#define MAX_GLOBALS   65536
#define MAX_LOCALS    256

typedef enum {
    CONST_NUMBER,
    CONST_STRING,
    CONST_BOOL,
    CONST_FUNCTION,    // index into function table
} ConstantType;

typedef struct {
    ConstantType type;
    union {
        double number_value;
        char* string_value;
        bool bool_value;
        int function_index;
    };
} Constant;

typedef struct {
    char* name;
    int index;
} GlobalVar;

typedef struct {
    char* name;
    int slot;           // register/stack slot index
    int scope_level;
} LocalVar;

typedef struct {
    char* name;
    int address;        // bytecode address
    int arity;          // number of parameters
    int local_count;    // number of local variables
} FunctionInfo;

typedef struct {
    // Bytecode buffer
    Instruction* code;
    int code_capacity;
    int code_count;
    
    // Constant pool
    Constant* constants;
    int const_capacity;
    int const_count;
    
    // Global variable table
    GlobalVar* globals;
    int global_capacity;
    int global_count;
    
    // Function table
    FunctionInfo* functions;
    int func_capacity;
    int func_count;
    
    // Currently compiling function index
    int current_function;
    
    // Debug: instruction -> source line mapping
    int* line_info;
    int line_capacity;
    int line_count;
    
    // Optimization: string interning pool
    struct {
        char** strings;
        int count;
        int capacity;
    } string_pool;
    
} BytecodeChunk;

// ========== Bytecode API ==========
BytecodeChunk* bytecode_create();
void bytecode_destroy(BytecodeChunk* chunk);

// Emit instructions
int bytecode_emit(BytecodeChunk* chunk, Instruction inst);
int bytecode_emit_line(BytecodeChunk* chunk, Instruction inst, int line);

// Constant management
int bytecode_add_constant(BytecodeChunk* chunk, Constant constant);
int bytecode_add_number_constant(BytecodeChunk* chunk, double value);
int bytecode_add_string_constant(BytecodeChunk* chunk, const char* value);
int bytecode_add_bool_constant(BytecodeChunk* chunk, bool value);

// Global variable management
int bytecode_add_global(BytecodeChunk* chunk, const char* name);
int bytecode_get_global(BytecodeChunk* chunk, const char* name);

// Function management
int bytecode_add_function(BytecodeChunk* chunk, const char* name, int arity);

// String interning (saves memory by deduplication)
const char* bytecode_intern_string(BytecodeChunk* chunk, const char* str);

// Jump patching for forward references
void bytecode_patch_jump(BytecodeChunk* chunk, int jump_instruction, int target_address);
int bytecode_current_offset(BytecodeChunk* chunk);

// Debug disassembly
void bytecode_disassemble(BytecodeChunk* chunk);
const char* opcode_name(Opcode op);

#endif // BYTECODE_H
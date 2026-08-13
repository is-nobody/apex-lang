// source/core/bytecode.c
// Implementation of Bytecode for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "bytecode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// human-readable names for each opcode, used in disassembly and debugging
static const char* opcode_names[] = {
    [OP_MOVE]             = "MOVE",
    [OP_LOAD_CONST]       = "LOAD_CONST",
    [OP_LOAD_NUM]         = "LOAD_NUM",
    [OP_LOAD_BOOL]        = "LOAD_BOOL",

    [OP_ADD]              = "ADD",
    [OP_SUB]              = "SUB",
    [OP_MUL]              = "MUL",
    [OP_DIV]              = "DIV",
    [OP_MOD]              = "MOD",
    [OP_NEG]              = "NEG",

    [OP_JUMP]             = "JUMP",
    [OP_JUMP_IF_FALSE]    = "JUMP_IF_FALSE",
    [OP_JUMP_IF_EQ]       = "JUMP_IF_EQ",
    [OP_JUMP_IF_NEQ]      = "JUMP_IF_NEQ",
    [OP_JUMP_IF_LT]       = "JUMP_IF_LT",
    [OP_JUMP_IF_GT]       = "JUMP_IF_GT",
    [OP_JUMP_IF_LTE]      = "JUMP_IF_LTE",
    [OP_JUMP_IF_GTE]      = "JUMP_IF_GTE",

    [OP_CMP_EQ]           = "CMP_EQ",
    [OP_CMP_NEQ]          = "CMP_NEQ",
    [OP_CMP_LT]           = "CMP_LT",
    [OP_CMP_GT]           = "CMP_GT",
    [OP_CMP_LTE]          = "CMP_LTE",
    [OP_CMP_GTE]          = "CMP_GTE",

    [OP_FOR_INIT]         = "FOR_INIT",
    [OP_FOR_NEXT]         = "FOR_NEXT",
    [OP_TABLE_ITER_INIT]  = "TABLE_ITER_INIT",
    [OP_TABLE_ITER_NEXT]  = "TABLE_ITER_NEXT",
    [OP_POP_ITER]         = "POP_ITER",

    [OP_TABLE_GET]        = "TABLE_GET",
    [OP_TABLE_GET_CONST]  = "TABLE_GET_CONST",
    [OP_TABLE_SET]        = "TABLE_SET",
    [OP_TABLE_SET_CONST]  = "TABLE_SET_CONST",
    [OP_TABLE_APPEND]     = "TABLE_APPEND",
    [OP_NEW_TABLE]        = "NEW_TABLE",

    [OP_CONCAT]           = "CONCAT",

    [OP_AND]              = "AND",
    [OP_OR]               = "OR",
    [OP_NOT]              = "NOT",

    [OP_PUSH_ARG]         = "PUSH_ARG",
    [OP_CALL]             = "CALL",
    [OP_CALL_BUILTIN]     = "CALL_BUILTIN",
    [OP_CALL_0]           = "CALL_0",
    [OP_CALL_1]           = "CALL_1",
    [OP_CALL_2]           = "CALL_2",
    [OP_RETURN]           = "RETURN",
    [OP_RETURN_VOID]      = "RETURN_VOID",
    [OP_RETURN_NUM]       = "RETURN_NUM",

    [OP_LOAD_GLOBAL]      = "LOAD_GLOBAL",
    [OP_STORE_GLOBAL]     = "STORE_GLOBAL",

    [OP_HALT]             = "HALT",

    [OP_COUNT]            = "COUNT",
};

// returns the name of an opcode, or "unknown" if out of range
const char* opcode_name(Opcode op) {
    if (op >= 0 && op < OP_COUNT && opcode_names[op]) {  // validate opcode range and check name exists
        return opcode_names[op];                         // return human-readable name
    }
    return "UNKNOWN";                                    // fallback for invalid opcode
}

// creates a new bytecode chunk with initial capacity for code, constants, globals, and debug info
BytecodeChunk* bytecode_create() {
    BytecodeChunk* chunk = (BytecodeChunk*)calloc(1, sizeof(BytecodeChunk));                   // allocate and zero chunk struct
    
    chunk->code_capacity = 1024;       // initial code buffer capacity
    chunk->code = (Instruction*)malloc(sizeof(Instruction) * chunk->code_capacity);            // allocate code array
    chunk->code_count = 0;             // no instructions yet
    
    chunk->const_capacity = 256;       // initial constant pool capacity
    chunk->constants = (Constant*)malloc(sizeof(Constant) * chunk->const_capacity);            // allocate constant array
    chunk->const_count = 0;            // no constants yet
    
    chunk->global_capacity = 64;       // initial global variable capacity
    chunk->globals = (GlobalVar*)malloc(sizeof(GlobalVar) * chunk->global_capacity);           // allocate global array
    chunk->global_count = 0;           // no globals yet
    
    chunk->func_capacity = 16;         // initial function info capacity
    chunk->functions = (FunctionInfo*)malloc(sizeof(FunctionInfo) * chunk->func_capacity);     // allocate function array
    chunk->func_count = 0;             // no functions yet
    
    chunk->line_capacity = 1024;       // initial line info capacity
    chunk->line_info = (int*)malloc(sizeof(int) * chunk->line_capacity);                       // allocate line info array
    chunk->line_count = 0;             // no line info yet
    
    chunk->string_pool.capacity = 64;  // initial string pool capacity
    chunk->string_pool.strings = (char**)malloc(sizeof(char*) * chunk->string_pool.capacity);  // allocate string pool
    chunk->string_pool.count = 0;      // no strings yet
    
    return chunk;                      // return new chunk
}

// frees all memory associated with a bytecode chunk, including its pools and nested structures
void bytecode_destroy(BytecodeChunk* chunk) {
    if (!chunk) return;                                               // guard against null
    
    free(chunk->code);                                                // free instruction array
    
    for (int i = 0; i < chunk->const_count; i++) {                    // iterate over constants
        if (chunk->constants[i].type == CONST_STRING) {               // check if constant is a string
            if (chunk->constants[i].string_value) {                   // check if string value exists
                bool in_pool = false;                                 // flag to track if string is pooled
                for (int j = 0; j < chunk->string_pool.count; j++) {  // iterate over string pool
                    if (chunk->string_pool.strings[j] == chunk->constants[i].string_value) {  // compare pointers
                        in_pool = true;                               // string is in pool, don't free separately
                        break;                                        // exit pool check loop
                    }
                }
                if (!in_pool) {                                    // string not in pool, owned by constant
                    free(chunk->constants[i].string_value);        // free constant-owned string
                }
            }
        }
    }
    free(chunk->constants);                                        // free constant array
    
    for (int i = 0; i < chunk->global_count; i++) {                // iterate over global variables
        free(chunk->globals[i].name);                              // free global name string
    }
    free(chunk->globals);                                          // free global array
    
    for (int i = 0; i < chunk->func_count; i++) {                  // iterate over function infos
        free(chunk->functions[i].name);                            // free function name
        if (chunk->functions[i].local_names) {                     // check if local names exist
            for (int j = 0; j < chunk->functions[i].local_count; j++) {  // iterate over local names
                free(chunk->functions[i].local_names[j]);          // free each local name string
            }
            free(chunk->functions[i].local_names);                 // free local names array
        }
    }
    free(chunk->functions);                                        // free function array
    
    free(chunk->line_info);                                        // free line info array
    
    for (int i = 0; i < chunk->string_pool.count; i++) {           // iterate over string pool
        free(chunk->string_pool.strings[i]);                       // free each pooled string
    }
    free(chunk->string_pool.strings);                              // free string pool array
    
    free(chunk);                                                   // free chunk struct
}

// appends an instruction to the code buffer, resizing if necessary
int bytecode_emit(BytecodeChunk* chunk, Instruction inst) {
    if (chunk->code_count >= chunk->code_capacity) {               // check if code buffer is full
        chunk->code_capacity *= 2;                                 // double capacity
        chunk->code = (Instruction*)realloc(chunk->code,           // resize code array
                                             sizeof(Instruction) * chunk->code_capacity);
    }
    
    int offset = chunk->code_count;                                // store current position
    chunk->code[chunk->code_count++] = inst;                       // append instruction and increment count
    return offset;                                                 // return offset of emitted instruction
}

// emits an instruction with line number debug info for source correlation
int bytecode_emit_line(BytecodeChunk* chunk, Instruction inst, int line) {
    int offset = bytecode_emit(chunk, inst);                       // emit instruction and get offset
    
    if (chunk->line_count >= chunk->line_capacity) {               // check if line info buffer is full
        chunk->line_capacity *= 2;                                 // double capacity
        chunk->line_info = (int*)realloc(chunk->line_info,         // resize line info array
                                         sizeof(int) * chunk->line_capacity);
    }
    
    while (chunk->line_count < chunk->code_count - 1) {            // fill any gaps in line info
        chunk->line_info[chunk->line_count++] = line;              // store line number for each instruction
    }
    chunk->line_info[chunk->line_count++] = line;                  // store line for current instruction
    
    return offset;                                                 // return offset of emitted instruction
}

// adds a constant to the pool, deduplicating identical values to save space
int bytecode_add_constant(BytecodeChunk* chunk, Constant constant) {
    for (int i = 0; i < chunk->const_count; i++) {                  // iterate over existing constants
        if (chunk->constants[i].type == constant.type) {            // check if types match
            switch (constant.type) {                                // dispatch based on constant type
                case CONST_NUMBER:
                    if (chunk->constants[i].number_value == constant.number_value) {  // compare values
                        return i;                                   // found duplicate, return existing index
                    }
                    break;
                case CONST_STRING:
                    if (strcmp(chunk->constants[i].string_value,    // compare string content
                              constant.string_value) == 0) {
                        return i;                                   // found duplicate, return existing index
                    }
                    break;
                case CONST_BOOL:
                    if (chunk->constants[i].bool_value == constant.bool_value) {  // compare values
                        return i;                                                 // found duplicate, return existing index
                    }
                    break;
                case CONST_NONE:
                    return i;                                       // none is always duplicate, return first index
                default:                                            // unknown constant type
                    break;                                          // skip dedup
            }
        }
    }
    
    if (chunk->const_count >= chunk->const_capacity) {              // check if constant pool is full
        chunk->const_capacity *= 2;                                 // double capacity
        chunk->constants = (Constant*)realloc(chunk->constants,     // resize constant array
                                              sizeof(Constant) * chunk->const_capacity);
    }
    
    int index = chunk->const_count;                                 // store current position
    chunk->constants[chunk->const_count++] = constant;              // add constant and increment count
    return index;                                                   // return index of new constant
}

// convenience wrapper for adding a number constant
int bytecode_add_number_constant(BytecodeChunk* chunk, double value) {
    Constant c = {.type = CONST_NUMBER, .number_value = value};     // create number constant
    return bytecode_add_constant(chunk, c);                         // delegate to generic adder
}

// convenience wrapper for adding a string constant with interning
int bytecode_add_string_constant(BytecodeChunk* chunk, const char* value) {
    const char* interned = bytecode_intern_string(chunk, value);           // intern string in pool
    Constant c = {.type = CONST_STRING, .string_value = (char*)interned};  // create string constant
    return bytecode_add_constant(chunk, c);                                // delegate to generic adder
}

// convenience wrapper for adding a none/null constant
int bytecode_add_none_constant(BytecodeChunk* chunk) {
    Constant c = {.type = CONST_NONE};                              // create none constant
    return bytecode_add_constant(chunk, c);                         // delegate to generic adder
}

// convenience wrapper for adding a boolean constant
int bytecode_add_bool_constant(BytecodeChunk* chunk, bool value) {
    Constant c = {.type = CONST_BOOL, .bool_value = value};         // create boolean constant
    return bytecode_add_constant(chunk, c);                         // delegate to generic adder
}

// registers a global variable by name, returning its index or creating a new entry
int bytecode_add_global(BytecodeChunk* chunk, const char* name) {
    int existing = bytecode_get_global(chunk, name);                // check if global already exists
    if (existing >= 0) return existing;                             // return existing index if found
    
    if (chunk->global_count >= chunk->global_capacity) {            // check if global array is full
        chunk->global_capacity *= 2;                                // double capacity
        chunk->globals = (GlobalVar*)realloc(chunk->globals,        // resize global array
                                             sizeof(GlobalVar) * chunk->global_capacity);
    }
    
    int index = chunk->global_count;                                // store current position
    chunk->globals[chunk->global_count].name = strdup(name);        // duplicate name string
    chunk->globals[chunk->global_count].index = index;              // store index
    chunk->global_count++;                                          // increment global count
    
    return index;                                                   // return index of new global
}

// looks up a global variable by name, returning -1 if not found
int bytecode_get_global(BytecodeChunk* chunk, const char* name) {
    for (int i = 0; i < chunk->global_count; i++) {                 // iterate over global variables
        if (strcmp(chunk->globals[i].name, name) == 0) {            // compare names
            return i;                                               // return index if found
        }
    }
    return -1;                                                      // not found, return -1
}

// adds a function definition with its name, arity, and current code address
int bytecode_add_function(BytecodeChunk* chunk, const char* name, int arity) {
    if (chunk->func_count >= chunk->func_capacity) {                 // check if function array is full
        chunk->func_capacity *= 2;                                   // double capacity
        chunk->functions = (FunctionInfo*)realloc(chunk->functions,  // resize function array
                                                   sizeof(FunctionInfo) * chunk->func_capacity);
    }
    
    int index = chunk->func_count;                                    // store current position
    chunk->functions[chunk->func_count].name = strdup(name);          // duplicate function name
    chunk->functions[chunk->func_count].address = chunk->code_count;  // store current code position
    chunk->functions[chunk->func_count].arity = arity;                // store function arity
    chunk->functions[chunk->func_count].local_count = 0;              // no locals yet
    chunk->functions[chunk->func_count].local_names = NULL;           // local names not allocated yet
    chunk->func_count++;                                              // increment function count
    
    return index;                                                     // return index of new function
}

// interns a string in the chunk's pool, returning a persistent pointer
const char* bytecode_intern_string(BytecodeChunk* chunk, const char* str) {
    for (int i = 0; i < chunk->string_pool.count; i++) {            // iterate over string pool
        if (strcmp(chunk->string_pool.strings[i], str) == 0) {      // compare strings
            return chunk->string_pool.strings[i];                   // return existing pooled string
        }
    }
    
    if (chunk->string_pool.count >= chunk->string_pool.capacity) {  // check if pool is full
        chunk->string_pool.capacity *= 2;                           // double capacity
        chunk->string_pool.strings = (char**)realloc(               // resize string pool
            chunk->string_pool.strings,
            sizeof(char*) * chunk->string_pool.capacity);
    }
    
    char* copy = strdup(str);                                       // duplicate string
    chunk->string_pool.strings[chunk->string_pool.count++] = copy;  // add to pool and increment count
    return copy;                                                    // return pooled string pointer
}

// patches a jump instruction to point to a new target address
void bytecode_patch_jump(BytecodeChunk* chunk, int jump_instruction, int target_address) {
    chunk->code[jump_instruction].operands[0] = target_address;     // update jump target operand
}

// returns the current code size, used for jump targets and offset calculations
int bytecode_current_offset(BytecodeChunk* chunk) {
    return chunk->code_count;                                       // return current instruction count
}
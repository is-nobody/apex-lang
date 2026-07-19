#include "bytecode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// human-readable names for each opcode, used in disassembly and debugging
static const char* opcode_names[] = {
    [OP_MOVE]             = "MOVE",
    [OP_LOAD_CONST]       = "LOAD_CONST",
    [OP_LOAD_CONST_NUM]   = "LOAD_CONST_NUM",
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
    [OP_RETURN]           = "RETURN",
    [OP_RETURN_VOID]      = "RETURN_VOID",

    [OP_CALL_0]           = "CALL_0",
    [OP_CALL_1]           = "CALL_1",
    [OP_CALL_2]           = "CALL_2",
    [OP_RETURN_NUM]       = "RETURN_NUM",

    [OP_LOAD_GLOBAL]      = "LOAD_GLOBAL",
    [OP_STORE_GLOBAL]     = "STORE_GLOBAL",

    [OP_HALT]             = "HALT",

    [OP_COUNT]            = "OP_COUNT",
};

// returns the name of an opcode, or "unknown" if out of range
const char* opcode_name(Opcode op) {
    if (op >= 0 && op < OP_COUNT && opcode_names[op]) {
        return opcode_names[op];
    }
    return "UNKNOWN";
}

// creates a new bytecode chunk with initial capacity for code, constants, globals, and debug info
BytecodeChunk* bytecode_create() {
    BytecodeChunk* chunk = (BytecodeChunk*)calloc(1, sizeof(BytecodeChunk));
    
    chunk->code_capacity = 1024;
    chunk->code = (Instruction*)malloc(sizeof(Instruction) * chunk->code_capacity);
    chunk->code_count = 0;
    
    chunk->const_capacity = 256;
    chunk->constants = (Constant*)malloc(sizeof(Constant) * chunk->const_capacity);
    chunk->const_count = 0;
    
    chunk->global_capacity = 64;
    chunk->globals = (GlobalVar*)malloc(sizeof(GlobalVar) * chunk->global_capacity);
    chunk->global_count = 0;
    
    chunk->func_capacity = 16;
    chunk->functions = (FunctionInfo*)malloc(sizeof(FunctionInfo) * chunk->func_capacity);
    chunk->func_count = 0;
    
    chunk->line_capacity = 1024;
    chunk->line_info = (int*)malloc(sizeof(int) * chunk->line_capacity);
    chunk->line_count = 0;
    
    chunk->string_pool.capacity = 64;
    chunk->string_pool.strings = (char**)malloc(sizeof(char*) * chunk->string_pool.capacity);
    chunk->string_pool.count = 0;
    
    return chunk;
}

// frees all memory associated with a bytecode chunk, including its pools and nested structures
void bytecode_destroy(BytecodeChunk* chunk) {
    if (!chunk) return;
    
    free(chunk->code);
    
    for (int i = 0; i < chunk->const_count; i++) {
        if (chunk->constants[i].type == CONST_STRING) {
            if (chunk->constants[i].string_value) {
                bool in_pool = false;
                for (int j = 0; j < chunk->string_pool.count; j++) {
                    if (chunk->string_pool.strings[j] == chunk->constants[i].string_value) {
                        in_pool = true;
                        break;
                    }
                }
                if (!in_pool) {
                    free(chunk->constants[i].string_value);
                }
            }
        }
    }
    free(chunk->constants);
    
    for (int i = 0; i < chunk->global_count; i++) {
        free(chunk->globals[i].name);
    }
    free(chunk->globals);
    
    for (int i = 0; i < chunk->func_count; i++) {
        free(chunk->functions[i].name);
        if (chunk->functions[i].local_names) {
            for (int j = 0; j < chunk->functions[i].local_count; j++) {
                free(chunk->functions[i].local_names[j]);
            }
            free(chunk->functions[i].local_names);
        }
    }
    free(chunk->functions);
    
    free(chunk->line_info);
    
    for (int i = 0; i < chunk->string_pool.count; i++) {
        free(chunk->string_pool.strings[i]);
    }
    free(chunk->string_pool.strings);
    
    free(chunk);
}

// appends an instruction to the code buffer, resizing if necessary
int bytecode_emit(BytecodeChunk* chunk, Instruction inst) {
    if (chunk->code_count >= chunk->code_capacity) {
        chunk->code_capacity *= 2;
        chunk->code = (Instruction*)realloc(chunk->code, 
                                             sizeof(Instruction) * chunk->code_capacity);
    }
    
    int offset = chunk->code_count;
    chunk->code[chunk->code_count++] = inst;
    return offset;
}

// emits an instruction with line number debug info for source correlation
int bytecode_emit_line(BytecodeChunk* chunk, Instruction inst, int line) {
    int offset = bytecode_emit(chunk, inst);
    
    if (chunk->line_count >= chunk->line_capacity) {
        chunk->line_capacity *= 2;
        chunk->line_info = (int*)realloc(chunk->line_info,
                                         sizeof(int) * chunk->line_capacity);
    }
    
    while (chunk->line_count < chunk->code_count - 1) {
        chunk->line_info[chunk->line_count++] = line;
    }
    chunk->line_info[chunk->line_count++] = line;
    
    return offset;
}

// adds a constant to the pool, deduplicating identical values to save space
int bytecode_add_constant(BytecodeChunk* chunk, Constant constant) {
    for (int i = 0; i < chunk->const_count; i++) {
        if (chunk->constants[i].type == constant.type) {
            switch (constant.type) {
                case CONST_NUMBER:
                    if (chunk->constants[i].number_value == constant.number_value) {
                        return i;
                    }
                    break;
                case CONST_STRING:
                    if (strcmp(chunk->constants[i].string_value, 
                              constant.string_value) == 0) {
                        return i;
                    }
                    break;
                case CONST_BOOL:
                    if (chunk->constants[i].bool_value == constant.bool_value) {
                        return i;
                    }
                    break;
                case CONST_NONE:
                    return i;
                default:
                    break;
            }
        }
    }
    
    if (chunk->const_count >= chunk->const_capacity) {
        chunk->const_capacity *= 2;
        chunk->constants = (Constant*)realloc(chunk->constants,
                                              sizeof(Constant) * chunk->const_capacity);
    }
    
    int index = chunk->const_count;
    chunk->constants[chunk->const_count++] = constant;
    return index;
}

// convenience wrapper for adding a number constant
int bytecode_add_number_constant(BytecodeChunk* chunk, double value) {
    Constant c = {.type = CONST_NUMBER, .number_value = value};
    return bytecode_add_constant(chunk, c);
}

// convenience wrapper for adding a string constant with interning
int bytecode_add_string_constant(BytecodeChunk* chunk, const char* value) {
    const char* interned = bytecode_intern_string(chunk, value);
    Constant c = {.type = CONST_STRING, .string_value = (char*)interned};
    return bytecode_add_constant(chunk, c);
}

// convenience wrapper for adding a none/null constant
int bytecode_add_none_constant(BytecodeChunk* chunk) {
    Constant c = {.type = CONST_NONE};
    return bytecode_add_constant(chunk, c);
}

// convenience wrapper for adding a boolean constant
int bytecode_add_bool_constant(BytecodeChunk* chunk, bool value) {
    Constant c = {.type = CONST_BOOL, .bool_value = value};
    return bytecode_add_constant(chunk, c);
}

// registers a global variable by name, returning its index or creating a new entry
int bytecode_add_global(BytecodeChunk* chunk, const char* name) {
    int existing = bytecode_get_global(chunk, name);
    if (existing >= 0) return existing;
    
    if (chunk->global_count >= chunk->global_capacity) {
        chunk->global_capacity *= 2;
        chunk->globals = (GlobalVar*)realloc(chunk->globals,
                                             sizeof(GlobalVar) * chunk->global_capacity);
    }
    
    int index = chunk->global_count;
    chunk->globals[chunk->global_count].name = strdup(name);
    chunk->globals[chunk->global_count].index = index;
    chunk->global_count++;
    
    return index;
}

// looks up a global variable by name, returning -1 if not found
int bytecode_get_global(BytecodeChunk* chunk, const char* name) {
    for (int i = 0; i < chunk->global_count; i++) {
        if (strcmp(chunk->globals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// adds a function definition with its name, arity, and current code address
int bytecode_add_function(BytecodeChunk* chunk, const char* name, int arity) {
    if (chunk->func_count >= chunk->func_capacity) {
        chunk->func_capacity *= 2;
        chunk->functions = (FunctionInfo*)realloc(chunk->functions,
                                                   sizeof(FunctionInfo) * chunk->func_capacity);
    }
    
    int index = chunk->func_count;
    chunk->functions[chunk->func_count].name = strdup(name);
    chunk->functions[chunk->func_count].address = chunk->code_count;
    chunk->functions[chunk->func_count].arity = arity;
    chunk->functions[chunk->func_count].local_count = 0;
    chunk->functions[chunk->func_count].local_names = NULL;
    chunk->func_count++;
    
    return index;
}

// interns a string in the chunk's pool, returning a persistent pointer
const char* bytecode_intern_string(BytecodeChunk* chunk, const char* str) {
    for (int i = 0; i < chunk->string_pool.count; i++) {
        if (strcmp(chunk->string_pool.strings[i], str) == 0) {
            return chunk->string_pool.strings[i];
        }
    }
    
    if (chunk->string_pool.count >= chunk->string_pool.capacity) {
        chunk->string_pool.capacity *= 2;
        chunk->string_pool.strings = (char**)realloc(
            chunk->string_pool.strings,
            sizeof(char*) * chunk->string_pool.capacity);
    }
    
    char* copy = strdup(str);
    chunk->string_pool.strings[chunk->string_pool.count++] = copy;
    return copy;
}

// patches a jump instruction to point to a new target address
void bytecode_patch_jump(BytecodeChunk* chunk, int jump_instruction, int target_address) {
    chunk->code[jump_instruction].operands[0] = target_address;
}

// returns the current code size, used for jump targets and offset calculations
int bytecode_current_offset(BytecodeChunk* chunk) {
    return chunk->code_count;
}
#include "bytecode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== Opcode Names ==========
static const char* opcode_names[] = {
    [OP_NOP]              = "NOP",
    [OP_LOAD_CONST]       = "LOAD_CONST",
    [OP_MOVE]             = "MOVE",
    [OP_ADD]              = "ADD",
    [OP_SUB]              = "SUB",
    [OP_MUL]              = "MUL",
    [OP_DIV]              = "DIV",
    [OP_MOD]              = "MOD",
    [OP_NEG]              = "NEG",
    [OP_CMP_EQ]           = "CMP_EQ",
    [OP_CMP_NEQ]          = "CMP_NEQ",
    [OP_CMP_LT]           = "CMP_LT",
    [OP_CMP_GT]           = "CMP_GT",
    [OP_CMP_LTE]          = "CMP_LTE",
    [OP_CMP_GTE]          = "CMP_GTE",
    [OP_AND]              = "AND",
    [OP_OR]               = "OR",
    [OP_NOT]              = "NOT",
    [OP_TO_NUMBER]        = "TO_NUMBER",
    [OP_TO_STRING]        = "TO_STRING",
    [OP_TO_BOOL]          = "TO_BOOL",
    [OP_JUMP]             = "JUMP",
    [OP_JUMP_IF_TRUE]     = "JUMP_IF_TRUE",
    [OP_JUMP_IF_FALSE]    = "JUMP_IF_FALSE",
    [OP_CALL]             = "CALL",
    [OP_CALL_BUILTIN]     = "CALL_BUILTIN",
    [OP_RETURN]           = "RETURN",
    [OP_RETURN_VOID]      = "RETURN_VOID",
    [OP_LOAD_GLOBAL]      = "LOAD_GLOBAL",
    [OP_STORE_GLOBAL]     = "STORE_GLOBAL",
    [OP_NEW_TABLE]        = "NEW_TABLE",
    [OP_TABLE_SET]        = "TABLE_SET",
    [OP_TABLE_GET]        = "TABLE_GET",
    [OP_TABLE_APPEND]     = "TABLE_APPEND",
    [OP_CONCAT]           = "CONCAT",
    [OP_STRING_APPEND]    = "STRING_APPEND",
    [OP_STRING_INTERP]    = "STRING_INTERP",
    [OP_FOR_PREP]         = "FOR_PREP",
    [OP_POP_ITER]         = "POP_ITER",
    [OP_FOR_INIT]         = "FOR_INIT",
    [OP_FOR_NEXT]         = "FOR_NEXT",
    [OP_JUMP_IF_LT]       = "JUMP_IF_LT",
    [OP_JUMP_IF_LTE]      = "JUMP_IF_LTE",
    [OP_JUMP_IF_GT]       = "JUMP_IF_GT",
    [OP_JUMP_IF_GTE]      = "JUMP_IF_GTE",
    [OP_JUMP_IF_EQ]       = "JUMP_IF_EQ",
    [OP_JUMP_IF_NEQ]      = "JUMP_IF_NEQ",
    [OP_PUSH_ARG]         = "PUSH_ARG",
    [OP_HALT]             = "HALT",
    [OP_ADD_IMM]          = "ADD_IMM",
    [OP_LOAD_BOOL]        = "LOAD_BOOL",
    [OP_LOAD_CONST_NUM]   = "LOAD_CONST_NUM",
};

const char* opcode_name(Opcode op) {
    if (op >= 0 && op < OP_COUNT && opcode_names[op]) {
        return opcode_names[op];
    }
    return "UNKNOWN";
}

// ========== Bytecode Chunk Implementation ==========

BytecodeChunk* bytecode_create() {
    BytecodeChunk* chunk = (BytecodeChunk*)calloc(1, sizeof(BytecodeChunk));
    
    // Instruction buffer
    chunk->code_capacity = 1024;
    chunk->code = (Instruction*)malloc(sizeof(Instruction) * chunk->code_capacity);
    chunk->code_count = 0;
    
    // Constant pool
    chunk->const_capacity = 256;
    chunk->constants = (Constant*)malloc(sizeof(Constant) * chunk->const_capacity);
    chunk->const_count = 0;
    
    // Global variable table
    chunk->global_capacity = 64;
    chunk->globals = (GlobalVar*)malloc(sizeof(GlobalVar) * chunk->global_capacity);
    chunk->global_count = 0;
    
    // Function table
    chunk->func_capacity = 16;
    chunk->functions = (FunctionInfo*)malloc(sizeof(FunctionInfo) * chunk->func_capacity);
    chunk->func_count = 0;
    
    // Line number debug info
    chunk->line_capacity = 1024;
    chunk->line_info = (int*)malloc(sizeof(int) * chunk->line_capacity);
    chunk->line_count = 0;
    
    // String pool for interning
    chunk->string_pool.capacity = 64;
    chunk->string_pool.strings = (char**)malloc(sizeof(char*) * chunk->string_pool.capacity);
    chunk->string_pool.count = 0;
    
    return chunk;
}

void bytecode_destroy(BytecodeChunk* chunk) {
    if (!chunk) return;
    
    free(chunk->code);
    
    // Free constants (strings in the pool are handled separately below)
    for (int i = 0; i < chunk->const_count; i++) {
        if (chunk->constants[i].type == CONST_STRING) {
            if (chunk->constants[i].string_value) {
                // Only free if not in the string pool
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
    
    // Free global variables
    for (int i = 0; i < chunk->global_count; i++) {
        free(chunk->globals[i].name);
    }
    free(chunk->globals);
    
    // Free functions
    for (int i = 0; i < chunk->func_count; i++) {
        free(chunk->functions[i].name);
    }
    free(chunk->functions);
    
    free(chunk->line_info);
    
    // Free string pool
    for (int i = 0; i < chunk->string_pool.count; i++) {
        free(chunk->string_pool.strings[i]);
    }
    free(chunk->string_pool.strings);
    
    free(chunk);
}

// ========== Emit Instructions ==========

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

int bytecode_emit_line(BytecodeChunk* chunk, Instruction inst, int line) {
    int offset = bytecode_emit(chunk, inst);
    
    if (chunk->line_count >= chunk->line_capacity) {
        chunk->line_capacity *= 2;
        chunk->line_info = (int*)realloc(chunk->line_info,
                                         sizeof(int) * chunk->line_capacity);
    }
    
    // Fill any gaps
    while (chunk->line_count < chunk->code_count - 1) {
        chunk->line_info[chunk->line_count++] = line;
    }
    chunk->line_info[chunk->line_count++] = line;
    
    return offset;
}

// ========== Constants Management ==========

int bytecode_add_constant(BytecodeChunk* chunk, Constant constant) {
    // Check for duplicates (interning)
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

int bytecode_add_number_constant(BytecodeChunk* chunk, double value) {
    Constant c = {.type = CONST_NUMBER, .number_value = value};
    return bytecode_add_constant(chunk, c);
}

int bytecode_add_string_constant(BytecodeChunk* chunk, const char* value) {
    const char* interned = bytecode_intern_string(chunk, value);
    Constant c = {.type = CONST_STRING, .string_value = (char*)interned};
    return bytecode_add_constant(chunk, c);
}

int bytecode_add_bool_constant(BytecodeChunk* chunk, bool value) {
    Constant c = {.type = CONST_BOOL, .bool_value = value};
    return bytecode_add_constant(chunk, c);
}

// ========== Globals ==========

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

int bytecode_get_global(BytecodeChunk* chunk, const char* name) {
    for (int i = 0; i < chunk->global_count; i++) {
        if (strcmp(chunk->globals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// ========== Functions ==========

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
    chunk->func_count++;
    
    return index;
}

// ========== String Interning ==========

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

// ========== Jump Patching ==========

void bytecode_patch_jump(BytecodeChunk* chunk, int jump_instruction, int target_address) {
    chunk->code[jump_instruction].operands[0] = target_address;
}

int bytecode_current_offset(BytecodeChunk* chunk) {
    return chunk->code_count;
}

// ========== Disassembler ==========

void bytecode_disassemble(BytecodeChunk* chunk) {
    printf("\n=== Bytecode Disassembly (%d instructions) ===\n", chunk->code_count);
    printf("Functions: %d, Globals: %d, Constants: %d\n\n",
           chunk->func_count, chunk->global_count, chunk->const_count);
    
    printf("%-6s %-20s %-10s %-10s %-10s %s\n", 
           "PC", "Opcode", "Op0", "Op1", "Op2", "Comment");
    printf("%-6s %-20s %-10s %-10s %-10s %s\n",
           "------", "------", "----", "----", "----", "-------");
    
    for (int i = 0; i < chunk->code_count; i++) {
        Instruction* inst = &chunk->code[i];
        int line = (i < chunk->line_count) ? chunk->line_info[i] : 0;
        
        printf("%-6d %-20s %-10d %-10d %-10d ",
               i,
               opcode_name(inst->opcode),
               inst->operands[0],
               inst->operands[1],
               inst->operands[2]);
        
        // Annotate with constant/global/function info depending on opcode
        switch (inst->opcode) {
            case OP_LOAD_CONST:
                if (inst->operands[1] < chunk->const_count) {
                    Constant* c = &chunk->constants[inst->operands[1]];
                    switch (c->type) {
                        case CONST_NUMBER:
                            printf("; R%d = %g", inst->operands[0], c->number_value);
                            break;
                        case CONST_STRING:
                            printf("; R%d = \"%s\"", inst->operands[0], c->string_value);
                            break;
                        case CONST_BOOL:
                            printf("; R%d = %s", inst->operands[0], 
                                   c->bool_value ? "true" : "false");
                            break;
                        default: break;
                    }
                }
                break;
            case OP_LOAD_GLOBAL:
                if (inst->operands[1] < chunk->global_count) {
                    printf("; R%d = global[%s]", 
                           inst->operands[0], 
                           chunk->globals[inst->operands[1]].name);
                }
                break;
            case OP_STORE_GLOBAL:
                if (inst->operands[1] < chunk->global_count) {
                    printf("; global[%s] = R%d",
                           chunk->globals[inst->operands[1]].name,
                           inst->operands[0]);
                }
                break;
            case OP_JUMP:
            case OP_JUMP_IF_TRUE:
            case OP_JUMP_IF_FALSE:
                printf("; -> %d", inst->operands[0]);
                break;
            case OP_CALL:
                printf("; R%d = call addr %d", inst->operands[0], inst->operands[1]);
                break;
            default:
                if (line > 0) printf("; line %d", line);
                break;
        }
        printf("\n");
    }
    
    // Print constant pool
    printf("\n=== Constant Pool ===\n");
    for (int i = 0; i < chunk->const_count; i++) {
        Constant* c = &chunk->constants[i];
        printf("[%d] ", i);
        switch (c->type) {
            case CONST_NUMBER: printf("number: %g\n", c->number_value); break;
            case CONST_STRING: printf("string: \"%s\"\n", c->string_value); break;
            case CONST_BOOL:   printf("bool: %s\n", c->bool_value ? "true" : "false"); break;
            default:           printf("unknown\n"); break;
        }
    }
    
    // Print globals table
    printf("\n=== Globals ===\n");
    for (int i = 0; i < chunk->global_count; i++) {
        printf("[%d] %s\n", i, chunk->globals[i].name);
    }
}
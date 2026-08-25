// source/core/bytecode.c
// Implementation of Bytecode for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "bytecode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// djb2 hash function for fast string lookup in string pool
static unsigned int hash_string_pool(const char* str) {
    unsigned int hash = 5381;             // initial hash value (djb2 standard)
    int c;                                // current character from string
    while ((c = *str++)) {                // iterate over each character
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c (djb2 formula)
    }
    return hash;                          // return computed hash value
}

// human-readable names for each opcode, used in disassembly and debugging
static const char* opcode_names[] = {
    [OP_MOVE]             = "MOVE",
    [OP_LOAD_CONST]       = "LOAD_CONST",
    [OP_LOAD_NUM_IMM]     = "LOAD_NUM_IMM",
    [OP_LOAD_NUM]         = "LOAD_NUM",
    [OP_LOAD_BOOL]        = "LOAD_BOOL",
    [OP_LOAD_NONE]        = "LOAD_NONE",

    [OP_ADD]              = "ADD",
    [OP_SUB]              = "SUB",
    [OP_MUL]              = "MUL",
    [OP_DIV]              = "DIV",
    [OP_MOD]              = "MOD",
    [OP_NEG]              = "NEG",
    [OP_INC]              = "INC",
    [OP_DEC]              = "DEC",

    [OP_JUMP]             = "JUMP",
    [OP_JUMP_IF_FALSE]    = "JUMP_IF_FALSE",
    [OP_JUMP_IF_EQ]       = "JUMP_IF_EQ",
    [OP_JUMP_IF_NEQ]      = "JUMP_IF_NEQ",
    [OP_JUMP_IF_EQ_NUM]   = "JUMP_IF_EQ_NUM",
    [OP_JUMP_IF_NEQ_NUM]  = "JUMP_IF_NEQ_NUM",
    [OP_JUMP_IF_LT]       = "JUMP_IF_LT",
    [OP_JUMP_IF_GT]       = "JUMP_IF_GT",
    [OP_JUMP_IF_LTE]      = "JUMP_IF_LTE",
    [OP_JUMP_IF_GTE]      = "JUMP_IF_GTE",

    [OP_CMP_EQ]           = "CMP_EQ",
    [OP_CMP_NEQ]          = "CMP_NEQ",
    [OP_CMP_EQ_NUM]       = "CMP_EQ_NUM",
    [OP_CMP_NEQ_NUM]      = "CMP_NEQ_NUM",
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
    [OP_TABLE_GET_INT]    = "TABLE_GET_INT",
    [OP_TABLE_SET]        = "TABLE_SET",
    [OP_TABLE_SET_CONST]  = "TABLE_SET_CONST",
    [OP_TABLE_SET_INT]    = "TABLE_SET_INT",
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
    [OP_RETURN_NUM]       = "RETURN_NUM",
    [OP_RETURN_NONE]      = "RETURN_NONE",

    [OP_LOAD_GLOBAL]      = "LOAD_GLOBAL",
    [OP_STORE_GLOBAL]     = "STORE_GLOBAL",

    [OP_HALT]             = "HALT",

    [OP_COUNT]            = "COUNT",
};

// returns the name of an opcode, or "unknown" if out of range
const char* opcode_name(Opcode op) {
    if (op >= 0 && op < OP_COUNT && opcode_names[op]) {  // validate opcode range and check name exists
        return opcode_names[op];                         // return human-readable name from lookup table
    }
    return "UNKNOWN";                                    // fallback for invalid opcode value
}

// creates a new bytecode chunk with initial capacity for code, constants, globals, and debug info
BytecodeChunk* bytecode_create() {
    BytecodeChunk* chunk = (BytecodeChunk*)calloc(1, sizeof(BytecodeChunk));                   // allocate and zero chunk struct
    
    chunk->code_capacity = 1024;       // initial code buffer capacity
    chunk->code = (Instruction*)malloc(sizeof(Instruction) * chunk->code_capacity);            // allocate instruction array
    chunk->code_count = 0;             // no instructions emitted yet
    
    chunk->const_capacity = 256;       // initial constant pool capacity
    chunk->constants = (Constant*)malloc(sizeof(Constant) * chunk->const_capacity);            // allocate constant array
    chunk->const_count = 0;            // no constants stored yet
    
    chunk->global_capacity = 64;       // initial global variable capacity
    chunk->globals = (GlobalVar*)malloc(sizeof(GlobalVar) * chunk->global_capacity);           // allocate global variable array
    chunk->global_count = 0;           // no globals registered yet
    
    chunk->func_capacity = 16;         // initial function info capacity
    chunk->functions = (FunctionInfo*)malloc(sizeof(FunctionInfo) * chunk->func_capacity);     // allocate function info array
    chunk->func_count = 0;             // no functions defined yet
    
    chunk->line_capacity = 1024;       // initial line info capacity
    chunk->line_info = (int*)malloc(sizeof(int) * chunk->line_capacity);                       // allocate line info array
    chunk->line_count = 0;             // no line info stored yet
    
    chunk->string_pool.capacity = 64;                                                          // initial string pool capacity
    chunk->string_pool.strings = (char**)malloc(sizeof(char*) * chunk->string_pool.capacity);  // allocate string pool array
    chunk->string_pool.count = 0;                                                              // no strings interned yet
    
    chunk->string_pool.hash_size = 64;                                                         // hash table size (power of 2 for fast modulo)
    chunk->string_pool.hash_table = (StringHashEntry**)calloc(chunk->string_pool.hash_size, sizeof(StringHashEntry*));  // allocate hash table buckets
    
    return chunk;                                                                              // return initialized chunk
}

// frees all memory associated with a bytecode chunk, including its pools and nested structures
void bytecode_destroy(BytecodeChunk* chunk) {
    if (!chunk) return;                                               // guard against null pointer
    
    free(chunk->code);                                                // free instruction array
    
    for (int i = 0; i < chunk->const_count; i++) {                    // iterate over constants
        if (chunk->constants[i].type == CONST_STRING) {               // check if constant is a string
            if (chunk->constants[i].string_value) {                   // check if string value exists
                bool in_pool = false;                                 // flag to track if string is in pool
                for (int j = 0; j < chunk->string_pool.count; j++) {  // iterate over string pool
                    if (chunk->string_pool.strings[j] == chunk->constants[i].string_value) {  // compare pointers for identity
                        in_pool = true;                               // string is in pool, will be freed separately
                        break;                                        // exit pool check loop early
                    }
                }
                if (!in_pool) {                                       // string not in pool, owned by constant
                    free(chunk->constants[i].string_value);           // free constant-owned string directly
                }
            }
        }
    }
    free(chunk->constants);                                        // free constant array
    
    for (int i = 0; i < chunk->global_count; i++) {                // iterate over global variables
        free(chunk->globals[i].name);                              // free global variable name string
    }
    free(chunk->globals);                                          // free global variable array
    
    for (int i = 0; i < chunk->func_count; i++) {                  // iterate over function infos
        free(chunk->functions[i].name);                            // free function name string
        if (chunk->functions[i].local_names) {                     // check if local names array exists
            for (int j = 0; j < chunk->functions[i].local_count; j++) {  // iterate over local names
                free(chunk->functions[i].local_names[j]);          // free each local variable name
            }
            free(chunk->functions[i].local_names);                 // free local names array itself
        }
    }
    free(chunk->functions);                                        // free function info array
    
    free(chunk->line_info);                                        // free line info array
    
    for (int i = 0; i < chunk->string_pool.count; i++) {           // iterate over string pool
        free(chunk->string_pool.strings[i]);                       // free each interned string
    }
    free(chunk->string_pool.strings);                              // free string pool array
    
    for (int i = 0; i < chunk->string_pool.hash_size; i++) {       // iterate over hash table buckets
        StringHashEntry* entry = chunk->string_pool.hash_table[i]; // get first entry in bucket
        while (entry) {                                            // iterate over collision chain
            StringHashEntry* next = entry->next;                   // save next entry before freeing
            free(entry);                                           // free hash table entry (string is freed above)
            entry = next;                                          // move to next entry
        }
    }
    free(chunk->string_pool.hash_table);                           // free hash table bucket array
    
    free(chunk);                                                   // free chunk struct itself
}

// appends an instruction to the code buffer, resizing if necessary
int bytecode_emit(BytecodeChunk* chunk, Instruction inst) {
    if (chunk->code_count >= chunk->code_capacity) {               // check if code buffer is full
        chunk->code_capacity *= 2;                                 // double code capacity
        chunk->code = (Instruction*)realloc(chunk->code,           // resize code array
                                             sizeof(Instruction) * chunk->code_capacity);
    }
    
    int offset = chunk->code_count;                                // store current instruction position
    chunk->code[chunk->code_count++] = inst;                       // append instruction and increment count
    return offset;                                                 // return offset of emitted instruction
}

// emits an instruction with line number debug info for source correlation
int bytecode_emit_line(BytecodeChunk* chunk, Instruction inst, int line) {
    int offset = bytecode_emit(chunk, inst);                       // emit instruction and get its offset
    
    if (chunk->line_count >= chunk->line_capacity) {               // check if line info buffer is full
        chunk->line_capacity *= 2;                                 // double line info capacity
        chunk->line_info = (int*)realloc(chunk->line_info,         // resize line info array
                                         sizeof(int) * chunk->line_capacity);
    }
    
    while (chunk->line_count < chunk->code_count - 1) {            // fill any gaps in line info from previous emissions
        chunk->line_info[chunk->line_count++] = line;              // store line number for each skipped instruction
    }
    chunk->line_info[chunk->line_count++] = line;                  // store line number for current instruction
    
    return offset;                                                 // return offset of emitted instruction
}

// adds a constant to the pool, deduplicating identical values to save space
int bytecode_add_constant(BytecodeChunk* chunk, Constant constant) {
    if (constant.type == CONST_STRING) {                           // fast path for string constants
        for (int i = 0; i < chunk->const_count; i++) {             // iterate over existing constants
            if (chunk->constants[i].type == CONST_STRING &&
                chunk->constants[i].string_value == constant.string_value) {  // compare pointers (strings are interned)
                return i;                                          // found duplicate string constant, return existing index
            }
        }
    } else {                                                       // slow path for non-string constants
        for (int i = 0; i < chunk->const_count; i++) {             // iterate over existing constants
            if (chunk->constants[i].type == constant.type) {       // check if types match
                switch (constant.type) {                           // dispatch by constant type
                    case CONST_NUMBER:                             // numeric constant
                        if (chunk->constants[i].number_value == constant.number_value) {  // compare numeric values
                            return i;                              // found duplicate number, return existing index
                        }
                        break;
                    case CONST_BOOL:                               // boolean constant
                        if (chunk->constants[i].bool_value == constant.bool_value) {      // compare boolean values
                            return i;                              // found duplicate bool, return existing index
                        }
                        break;
                    case CONST_NONE:                               // none constant
                        return i;                                  // none is always duplicate, return first index
                    default:                                       // unknown constant type
                        break;                                     // skip deduplication for unknown types
                }
            }
        }
    }
    
    if (chunk->const_count >= chunk->const_capacity) {             // check if constant pool is full
        chunk->const_capacity *= 2;                                // double constant capacity
        chunk->constants = (Constant*)realloc(chunk->constants,    // resize constant array
                                              sizeof(Constant) * chunk->const_capacity);
    }
    
    int index = chunk->const_count;                                // store current constant position
    chunk->constants[chunk->const_count++] = constant;             // add constant and increment count
    return index;                                                  // return index of new constant
}

// convenience wrapper for adding a number constant
int bytecode_add_number_constant(BytecodeChunk* chunk, double value) {
    Constant c = {.type = CONST_NUMBER, .number_value = value};  // create number constant struct
    return bytecode_add_constant(chunk, c);                      // delegate to generic constant adder
}

// convenience wrapper for adding a string constant with interning
int bytecode_add_string_constant(BytecodeChunk* chunk, const char* value) {
    unsigned int h = hash_string_pool(value) & (chunk->string_pool.hash_size - 1);  // compute hash bucket for string
    
    for (StringHashEntry* entry = chunk->string_pool.hash_table[h]; entry; entry = entry->next) {  // search hash bucket
        if (strcmp(entry->str, value) == 0) {                      // compare strings for match
            if (entry->const_index >= 0) {                         // check if constant index is cached
                return entry->const_index;                         // return cached constant index (fast path)
            }
            
            // string is interned but not yet added to constant pool
            Constant c = {.type = CONST_STRING, .string_value = (char*)entry->str};  // create string constant
            
            if (chunk->const_count >= chunk->const_capacity) {           // check if constant pool is full
                chunk->const_capacity *= 2;                              // double constant capacity
                chunk->constants = (Constant*)realloc(chunk->constants,  // resize constant array
                                                      sizeof(Constant) * chunk->const_capacity);
            }
            
            int index = chunk->const_count;                       // store current constant position
            chunk->constants[chunk->const_count++] = c;           // add constant and increment count
            entry->const_index = index;                           // cache index for future lookups
            return index;                                         // return index of new constant
        }
    }
    
    const char* interned = bytecode_intern_string(chunk, value);  // add string to pool and get persistent pointer
    
    for (StringHashEntry* entry = chunk->string_pool.hash_table[h]; entry; entry = entry->next) {  // find the new hash entry
        if (entry->str == interned) {                                              // match by pointer identity
            Constant c = {.type = CONST_STRING, .string_value = (char*)interned};  // create string constant
            
            if (chunk->const_count >= chunk->const_capacity) {           // check if constant pool is full
                chunk->const_capacity *= 2;                              // double constant capacity
                chunk->constants = (Constant*)realloc(chunk->constants,  // resize constant array
                                                      sizeof(Constant) * chunk->const_capacity);
            }
            
            int index = chunk->const_count;                        // store current constant position
            chunk->constants[chunk->const_count++] = c;            // add constant and increment count
            entry->const_index = index;                            // cache index for future lookups
            return index;                                          // return index of new constant
        }
    }
    
    Constant c = {.type = CONST_STRING, .string_value = (char*)interned};  // create string constant
    return bytecode_add_constant(chunk, c);                                // delegate to generic constant adder
}

// convenience wrapper for adding a none/null constant
int bytecode_add_none_constant(BytecodeChunk* chunk) {
    Constant c = {.type = CONST_NONE};                              // create none constant struct
    return bytecode_add_constant(chunk, c);                         // delegate to generic constant adder
}

// convenience wrapper for adding a boolean constant
int bytecode_add_bool_constant(BytecodeChunk* chunk, bool value) {
    Constant c = {.type = CONST_BOOL, .bool_value = value};         // create boolean constant struct
    return bytecode_add_constant(chunk, c);                         // delegate to generic constant adder
}

// registers a global variable by name, returning its index or creating a new entry
int bytecode_add_global(BytecodeChunk* chunk, const char* name) {
    int existing = bytecode_get_global(chunk, name);                // check if global already exists
    if (existing >= 0) return existing;                             // return existing index if found
    
    if (chunk->global_count >= chunk->global_capacity) {            // check if global array is full
        chunk->global_capacity *= 2;                                // double global capacity
        chunk->globals = (GlobalVar*)realloc(chunk->globals,        // resize global array
                                             sizeof(GlobalVar) * chunk->global_capacity);
    }
    
    int index = chunk->global_count;                                // store current global position
    chunk->globals[chunk->global_count].name = strdup(name);        // duplicate global name string
    chunk->globals[chunk->global_count].index = index;              // store global index
    chunk->global_count++;                                          // increment global count
    
    return index;                                                   // return index of new global
}

// cached last global lookup for fast repeated access to the same global
static int last_global_index = -1;           // cached index of last found global
static const char* last_global_name = NULL;  // cached name pointer of last found global

// looks up a global variable by name, using a one-entry cache for repeated lookups
int bytecode_get_global(BytecodeChunk* chunk, const char* name) {
    if (last_global_name == name && last_global_index >= 0 &&      // fast path: check cached entry first
        last_global_index < chunk->global_count &&
        strcmp(chunk->globals[last_global_index].name, name) == 0) {
        return last_global_index;                                  // return cached index if name matches
    }
    
    for (int i = 0; i < chunk->global_count; i++) {                // linear scan of global variables
        if (strcmp(chunk->globals[i].name, name) == 0) {           // compare global names
            last_global_index = i;                                 // cache index for next lookup
            last_global_name = name;                               // cache name pointer for next lookup
            return i;                                              // return found global index
        }
    }
    return -1;                                                     // global not found
}

// adds a function definition with its name, arity, and current code address
int bytecode_add_function(BytecodeChunk* chunk, const char* name, int arity) {
    if (chunk->func_count >= chunk->func_capacity) {                 // check if function array is full
        chunk->func_capacity *= 2;                                   // double function capacity
        chunk->functions = (FunctionInfo*)realloc(chunk->functions,  // resize function array
                                                   sizeof(FunctionInfo) * chunk->func_capacity);
    }
    
    int index = chunk->func_count;                                    // store current function position
    chunk->functions[chunk->func_count].name = strdup(name);          // duplicate function name string
    chunk->functions[chunk->func_count].address = chunk->code_count;  // store current code position as function entry
    chunk->functions[chunk->func_count].arity = arity;                // store function parameter count
    chunk->functions[chunk->func_count].local_count = 0;              // no local variables yet
    chunk->functions[chunk->func_count].local_names = NULL;           // local names array not allocated yet
    chunk->func_count++;                                              // increment function count
    
    return index;                                                     // return index of new function
}

// interns a string in the chunk's pool, returning a persistent pointer for deduplication
const char* bytecode_intern_string(BytecodeChunk* chunk, const char* str) {
    unsigned int h = hash_string_pool(str) & (chunk->string_pool.hash_size - 1);  // compute hash bucket for string
    
    for (StringHashEntry* entry = chunk->string_pool.hash_table[h]; entry; entry = entry->next) {  // search hash bucket
        if (strcmp(entry->str, str) == 0) {                        // compare strings for match
            return entry->str;                                     // return existing pooled string pointer
        }
    }
    
    if (chunk->string_pool.count >= chunk->string_pool.capacity) {  // check if string pool is full
        chunk->string_pool.capacity *= 2;                           // double string pool capacity
        chunk->string_pool.strings = (char**)realloc(               // resize string pool array
            chunk->string_pool.strings,
            sizeof(char*) * chunk->string_pool.capacity);
    }
    
    char* copy = strdup(str);                                       // duplicate the input string
    chunk->string_pool.strings[chunk->string_pool.count++] = copy;  // add copy to pool and increment count
    
    StringHashEntry* entry = (StringHashEntry*)malloc(sizeof(StringHashEntry));  // allocate hash table entry
    entry->str = copy;                                              // store pointer to pooled string
    entry->const_index = -1;                                        // mark as not yet in constant pool
    entry->next = chunk->string_pool.hash_table[h];                 // link new entry at head of bucket
    chunk->string_pool.hash_table[h] = entry;                       // update bucket head to new entry
    
    return copy;                                                    // return pooled string pointer
}

// patches a jump instruction to point to a new target address
void bytecode_patch_jump(BytecodeChunk* chunk, int jump_instruction, int target_address) {
    chunk->code[jump_instruction].operands[0] = target_address;     // update jump target operand with resolved address
}

// returns the current code size, used for jump targets and offset calculations
int bytecode_current_offset(BytecodeChunk* chunk) {
    return chunk->code_count;                                       // return current instruction count
}
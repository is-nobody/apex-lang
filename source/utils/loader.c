// source/utils/loader.c
// Implementation of Bytecode Loader for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "loader.h"
#include "execute.h"
#include "bytecode.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// magic number for .apexc files ("APEX" in hex)
#define APEXC_MAGIC 0x41504558

// union for type-punning double to uint64_t without strict-aliasing violations
typedef union {
    double d;
    uint64_t u;
} DoubleUnion;

// reads a uint32_t in little-endian order
static uint32_t read_u32(FILE* f) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) return 0;  // read 4 bytes or fail
    return (uint32_t)buf[0] |                // reconstruct little-endian
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

// reads a uint64_t in little-endian order
static uint64_t read_u64(FILE* f) {
    uint8_t buf[8];
    if (fread(buf, 1, 8, f) != 8) return 0;  // read 8 bytes or fail
    return (uint64_t)buf[0] |                // reconstruct little-endian
           ((uint64_t)buf[1] << 8) |
           ((uint64_t)buf[2] << 16) |
           ((uint64_t)buf[3] << 24) |
           ((uint64_t)buf[4] << 32) |
           ((uint64_t)buf[5] << 40) |
           ((uint64_t)buf[6] << 48) |
           ((uint64_t)buf[7] << 56);
}

// reads a string prefixed with its length, returns empty string for length 0
static char* read_string(FILE* f) {
    uint32_t len = read_u32(f);          // read length prefix
    if (len == 0) {                      // empty string
        char* str = (char*)malloc(1);    // allocate empty string
        if (str) str[0] = '\0';
        return str;
    }
    if (len > 65536) {                   // sanity check
        return NULL;
    }
    char* str = (char*)malloc(len + 1);  // allocate buffer
    if (!str) return NULL;
    if (fread(str, 1, len, f) != len) {  // read string data
        free(str);
        return NULL;
    }
    str[len] = '\0';                     // null terminate
    return str;
}

// reads a single instruction from file
static Instruction read_instruction(FILE* f) {
    Instruction inst;
    inst.opcode = (Opcode)read_u32(f);        // read opcode
    inst.operands[0] = (int32_t)read_u32(f);  // read operand 1
    inst.operands[1] = (int32_t)read_u32(f);  // read operand 2
    inst.operands[2] = (int32_t)read_u32(f);  // read operand 3
    return inst;
}

// reads a constant pool entry from file
static Constant read_constant(FILE* f) {
    Constant c;
    c.type = (ConstantType)read_u32(f);           // read type discriminator
    switch (c.type) {
        case CONST_NUMBER: {
            DoubleUnion du;
            du.u = read_u64(f);                   // read uint64 bits
            c.number_value = du.d;                // convert to double
            break;
        }
        case CONST_STRING: {
            c.string_value = read_string(f);      // read string with prefix
            break;
        }
        case CONST_BOOL: {
            c.bool_value = read_u32(f) != 0;      // read bool as 0/1
            break;
        }
        case CONST_FUNCTION: {
            c.function_index = (int)read_u32(f);  // read function index
            break;
        }
        case CONST_NONE: {
            break;                                // none has no data
        }
        default: {
            c.type = CONST_NONE;                  // fallback for unknown
            break;
        }
    }
    return c;
}

// loads a bytecode chunk from a .apexc file, restoring all sections
BytecodeChunk* bytecode_load(const char* path) {
    FILE* f = fopen(path, "rb");                                // open bytecode file
    if (!f) {
        print_error("Cannot open bytecode file '%s'", path);
        return NULL;
    }
    
    // verify header: magic number must match
    uint32_t magic = read_u32(f);                               // read magic number
    if (magic != APEXC_MAGIC) {                                 // verify "APEX"
        print_error("Invalid bytecode file: bad magic number (expected APEX)");
        fclose(f);
        return NULL;
    }
    
    BytecodeChunk* chunk = bytecode_create();                   // create empty chunk
    if (!chunk) {
        fclose(f);
        return NULL;
    }
    
    // load code section: all instructions
    uint32_t code_count = read_u32(f);                          // read instruction count
    for (uint32_t i = 0; i < code_count; i++) {
        Instruction inst = read_instruction(f);                 // read each instruction
        bytecode_emit(chunk, inst);                             // add to chunk
    }
    
    // load constants section: all constants from the pool
    uint32_t const_count = read_u32(f);                         // read constant count
    for (uint32_t i = 0; i < const_count; i++) {
        Constant c = read_constant(f);                          // read each constant
        bytecode_add_constant(chunk, c);                        // add to chunk
    }
    
    // load globals section: all global variables
    uint32_t global_count = read_u32(f);                        // read global count
    for (uint32_t i = 0; i < global_count; i++) {
        char* name = read_string(f);                            // read global name
        if (name) {
            bytecode_add_global(chunk, name);                   // add to chunk
            free(name);                                         // free temporary name
        }
        read_u32(f);                                            // skip stored index (recalculated)
    }
    
    // load functions section: all function metadata
    uint32_t func_count = read_u32(f);                          // read function count
    for (uint32_t i = 0; i < func_count; i++) {
        char* name = read_string(f);                            // read function name
        uint32_t address = read_u32(f);                         // read entry address
        uint32_t arity = read_u32(f);                           // read parameter count
        uint32_t local_count = read_u32(f);                     // read local count
        uint32_t max_registers = read_u32(f);                   // read max registers
        
        int func_idx = bytecode_add_function(chunk, name ? name : "(anonymous)", (int)arity);  // add function
        if (func_idx >= 0 && func_idx < chunk->func_count) {    // success
            chunk->functions[func_idx].address = (int)address;  // restore address
            chunk->functions[func_idx].local_count = (int)local_count;      // restore local count
            chunk->functions[func_idx].max_registers = (int)max_registers;  // restore max regs
        }
        
        free(name);                                             // free temporary name
    }
    
    // load string pool: interned strings for deduplication
    uint32_t string_count = read_u32(f);                        // read string pool count
    for (uint32_t i = 0; i < string_count; i++) {
        char* str = read_string(f);                             // read pooled string
        if (str) {
            bytecode_intern_string(chunk, str);                 // intern into chunk
            free(str);                                          // free temporary string
        }
    }
    
    fclose(f);                                                  // close file
    return chunk;                                               // return loaded chunk
}

// executes a bytecode file directly, bypassing tokenization and parsing
bool execute_bytecode_file(const char* filepath, int argc, char** argv, bool skip_script_name) {
    if (!filepath) return false;                                // validate path
    
    BytecodeChunk* chunk = bytecode_load(filepath);
    if (!chunk) {                                               // load failed
        print_error("Failed to load bytecode from '%s'", filepath);
        return false;
    }
    
    if (chunk->func_count == 0) {                               // no functions found
        print_error("Invalid bytecode: no functions found");
        bytecode_destroy(chunk);
        return false;
    }
    
    VM* vm = vm_create("");                                     // empty source for bytecode
    if (!vm) {                                                  // vm creation failed
        bytecode_destroy(chunk);
        print_error("Failed to create VM");
        return false;
    }
    
    vm_set_args(vm, argc, argv, skip_script_name);              // set cli args
    
    if (chunk->functions[0].max_registers == 0) {               // not set
        chunk->functions[0].max_registers = 16;                 // set default minimum
    }
    
    bool ok = vm_execute(vm, chunk);                            // run vm
    vm_destroy(vm);                                             // free vm
    bytecode_destroy(chunk);                                    // free chunk
    
    return ok;                                                  // return execution result
}
// source/utils/compile.c
// Implementation of Compiler for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "compile.h"
#include "execute.h"
#include "tokenizer.h"
#include "parser.h"
#include "ast.h"
#include "bytecode.h"
#include "codegen.h"
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

// writes a uint32_t in little-endian order
static void write_u32(FILE* f, uint32_t value) {
    uint8_t buf[4];
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = (value >> 16) & 0xFF;
    buf[3] = (value >> 24) & 0xFF;
    fwrite(buf, 1, 4, f);
}

// writes a uint64_t in little-endian order
static void write_u64(FILE* f, uint64_t value) {
    uint8_t buf[8];
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = (value >> 16) & 0xFF;
    buf[3] = (value >> 24) & 0xFF;
    buf[4] = (value >> 32) & 0xFF;
    buf[5] = (value >> 40) & 0xFF;
    buf[6] = (value >> 48) & 0xFF;
    buf[7] = (value >> 56) & 0xFF;
    fwrite(buf, 1, 8, f);
}

// writes a string prefixed with its length, handles NULL as empty string
static void write_string(FILE* f, const char* str) {
    if (!str) {                               // check for null string
        write_u32(f, 0);                      // write empty string length
        return;
    }
    uint32_t len = (uint32_t)strlen(str);     // get string length
    write_u32(f, len);                        // write length prefix
    if (len > 0) {
        fwrite(str, 1, len, f);               // write string data
    }
}

// serializes a single instruction to file
static void write_instruction(FILE* f, Instruction* inst) {
    write_u32(f, (uint32_t)inst->opcode);       // opcode
    write_u32(f, (uint32_t)inst->operands[0]);  // operand 1
    write_u32(f, (uint32_t)inst->operands[1]);  // operand 2
    write_u32(f, (uint32_t)inst->operands[2]);  // operand 3
}

// serializes a constant pool entry to file
static void write_constant(FILE* f, Constant* c) {
    write_u32(f, (uint32_t)c->type);  // constant type discriminator
    switch (c->type) {
        case CONST_NUMBER: {
            DoubleUnion du;
            du.d = c->number_value;  // store double in union
            write_u64(f, du.u);      // write as uint64
            break;
        }
        case CONST_STRING:
            write_string(f, c->string_value);     // write string with length prefix
            break;
        case CONST_BOOL:
            write_u32(f, c->bool_value ? 1 : 0);  // write bool as 0 or 1
            break;
        case CONST_FUNCTION:
            write_u32(f, (uint32_t)c->function_index);  // function index
            break;
        case CONST_NONE:
            break;  // none has no data
        default:
            break;
    }
}

// serializes a global variable entry to file
static void write_global(FILE* f, GlobalVar* g) {
    write_string(f, g->name);          // global name
    write_u32(f, (uint32_t)g->index);  // global index
}

// serializes function metadata to file
static void write_function(FILE* f, FunctionInfo* func) {
    write_string(f, func->name);                  // function name
    write_u32(f, (uint32_t)func->address);        // entry address
    write_u32(f, (uint32_t)func->arity);          // parameter count
    write_u32(f, (uint32_t)func->local_count);    // local variables count
    write_u32(f, (uint32_t)func->max_registers);  // max registers used
}

// serializes the entire bytecode chunk to a .apexc file
static bool serialize_chunk(BytecodeChunk* chunk, const char* output_path) {
    FILE* f = fopen(output_path, "wb");                  // open output file in binary mode
    if (!f) {                                            // check file open
        print_error("Cannot create output file '%s'", output_path);
        return false;
    }
    
    write_u32(f, APEXC_MAGIC);                           // write "APEX" magic marker
    
    write_u32(f, (uint32_t)chunk->code_count);           // write instruction count
    for (int i = 0; i < chunk->code_count; i++) {
        write_instruction(f, &chunk->code[i]);           // write each instruction
    }
    
    write_u32(f, (uint32_t)chunk->const_count);          // write constant count
    for (int i = 0; i < chunk->const_count; i++) {
        write_constant(f, &chunk->constants[i]);         // write each constant
    }
    
    write_u32(f, (uint32_t)chunk->global_count);         // write global count
    for (int i = 0; i < chunk->global_count; i++) {
        write_global(f, &chunk->globals[i]);             // write each global
    }
    
    write_u32(f, (uint32_t)chunk->func_count);           // write function count
    for (int i = 0; i < chunk->func_count; i++) {
        write_function(f, &chunk->functions[i]);         // write each function
    }
    
    write_u32(f, (uint32_t)chunk->string_pool.count);    // write string pool count
    for (int i = 0; i < chunk->string_pool.count; i++) {
        write_string(f, chunk->string_pool.strings[i]);  // write each pooled string
    }
    
    fclose(f);                                           // close output file
    return true;                                         // serialization successful
}

// main compile command handler
int compile_command(int argc, char** argv) {
    if (argc < 3) {                           // check arguments
        fprintf(stderr, "\033[31mError: Missing filename.\nUsage: apex compile <filename.apex>\033[0m\n");
        return 1;
    }
    
    const char* filename = argv[2];
    
    FILE* f_check = fopen(filename, "rb");    // verify source file exists
    if (!f_check) {                           // file not found
        fprintf(stderr, "\033[31mError: Source file '%s' does not exist.\033[0m\n", filename);
        return 1;
    }
    fclose(f_check);
    
    FILE* f = fopen(filename, "rb");          // read entire source file into memory
    if (!f) {                                 // cannot open
        fprintf(stderr, "\033[31mError: Cannot open source file '%s'.\033[0m\n", filename);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);                    // seek to end
    long size = ftell(f);                     // get file size
    fseek(f, 0, SEEK_SET);                    // seek to start
    
    char* source = (char*)malloc(size + 1);   // allocate buffer
    if (!source) {                            // allocation failed
        print_error("Memory allocation failed");
        fclose(f);
        return 1;
    }
    
    if (fread(source, 1, size, f) != (size_t)size) {  // read file
        print_error("Cannot read file '%s'", filename);
        fclose(f);
        free(source);
        return 1;
    }
    source[size] = '\0';  // null terminate
    fclose(f);
    
    Tokenizer* tokenizer = tokenizer_create(source, filename);
    int token_count;
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);
    
    if (!tokens || tokenizer_has_error(tokenizer)) {
        fprintf(stderr, "\033[31mError: Tokenization failed for '%s'\033[0m\n", filename);
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, source);
        return 1;
    }
    
    Parser* parser = parser_create(tokens, token_count, filename, source);
    ASTNode* ast = parse_program(parser);
    
    if (!ast || parser_had_errors(parser)) {
        fprintf(stderr, "\033[31mError: Parsing failed for '%s'\033[0m\n", filename);
        cleanup_all(tokenizer, parser, ast, NULL, NULL, NULL, source);
        return 1;
    }
    
    BytecodeChunk* chunk = bytecode_create();
    CodeGenerator* cg = codegen_create(chunk);
    
    if (!codegen_generate(cg, ast)) {
        fprintf(stderr, "\033[31mError: Code generation failed for '%s'\033[0m\n", filename);
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);
        return 1;
    }
    
    if (chunk->func_count > 0 && chunk->functions[0].max_registers == 0) {
        chunk->functions[0].max_registers = 16;   // default minimum
    }
    
    char output_path[4096];
    strncpy(output_path, filename, sizeof(output_path) - 1);
    output_path[sizeof(output_path) - 1] = '\0';
    
    char* dot = strrchr(output_path, '.');    // find last dot
    if (dot) {
        *dot = '\0';                          // strip extension
    }
    strcat(output_path, ".apexc");            // add .apexc extension
    
    printf("\033[36mCompiling %s...\033[0m\n", filename);
    
    if (!serialize_chunk(chunk, output_path)) {
        fprintf(stderr, "\033[31mError: Failed to write bytecode to '%s'\033[0m\n", output_path);
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);
        return 1;
    }
    
    printf("\033[36mInstructions: %d\033[0m\n", chunk->code_count);
    printf("\033[32mCompilation successful! Bytecode saved to: %s\033[0m\n", output_path);

    cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);
    return 0;
}
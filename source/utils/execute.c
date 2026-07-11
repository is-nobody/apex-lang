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
#include <setjmp.h>

#define ANSI_RED    "\033[31m"
#define ANSI_RESET  "\033[0m"

// prints a formatted error message in red
void print_error(const char* format, ...) {
    fprintf(stderr, "%s[Error] ", ANSI_RED);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "%s\n", ANSI_RESET);
}

// cleans up all allocated resources in reverse order
static void cleanup_all(Tokenizer* tok, Parser* par, ASTNode* ast,
                        CodeGenerator* cg, BytecodeChunk* chunk, VM* vm,
                        char* source) {
    if (vm) vm_destroy(vm);
    if (cg) codegen_destroy(cg);
    if (chunk) bytecode_destroy(chunk);
    if (ast) ast_free(ast);
    if (par) parser_destroy(par);
    if (tok) tokenizer_destroy(tok);
    if (source) free(source);
}

// executes apex source code from a string
bool execute_source_string(const char* source_code, const char* filename) {
    if (!source_code || !filename) return false;
    
    Tokenizer* tokenizer = NULL;
    Parser* parser = NULL;
    ASTNode* ast = NULL;
    CodeGenerator* cg = NULL;
    BytecodeChunk* chunk = NULL;
    VM* vm = NULL;

    tokenizer = tokenizer_create(source_code, filename);
    int token_count;
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);

    if (tokenizer_has_error(tokenizer) || !tokens) {
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, NULL);
        return false;
    }

    parser = parser_create(tokens, token_count, filename, source_code);
    ast = parser_parse(parser);
    if (!ast || parser_had_errors(parser)) {
        cleanup_all(tokenizer, parser, ast, NULL, NULL, NULL, NULL);
        return false;
    }

    chunk = bytecode_create();
    cg = codegen_create(chunk);
    if (!codegen_generate(cg, ast)) {
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, NULL);
        return false;
    }

    vm = vm_create(source_code);
    bool ok = vm_execute(vm, chunk);
    cleanup_all(tokenizer, parser, ast, cg, chunk, vm, NULL);
    return ok;
}

// executes apex source code from a file path
bool execute_source(const char* filepath, const char* filename) {
    if (!filepath || !filename) return false;
    
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        print_error("Cannot open file '%s'", filepath);
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = (char*)malloc(size + 1);
    if (!source) {
        print_error("Memory allocation failed");
        fclose(f);
        return false;
    }
    
    if (fread(source, 1, size, f) != (size_t)size) {
        print_error("Cannot read file '%s'", filepath);
        fclose(f);
        free(source);
        return false;
    }
    source[size] = '\0';
    fclose(f);
    
    Tokenizer* tokenizer = NULL;
    Parser* parser = NULL;
    ASTNode* ast = NULL;
    CodeGenerator* cg = NULL;
    BytecodeChunk* chunk = NULL;
    VM* vm = NULL;
    
    tokenizer = tokenizer_create(source, filename);
    int token_count;
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);

    if (tokenizer_has_error(tokenizer) || !tokens) {
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, NULL);
        return false;
    }
    
    parser = parser_create(tokens, token_count, filename, source);
    ast = parser_parse(parser);
    if (!ast || parser_had_errors(parser)) {
        cleanup_all(tokenizer, parser, ast, NULL, NULL, NULL, source);
        return false;
    }
    
    chunk = bytecode_create();
    cg = codegen_create(chunk);
    if (!codegen_generate(cg, ast)) {
        print_error("Code generation failed for '%s'", filename);
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);
        return false;
    }
    
    vm = vm_create(source);
    bool ok = vm_execute(vm, chunk);
    
    cleanup_all(tokenizer, parser, ast, cg, chunk, vm, source);
    return ok;
}
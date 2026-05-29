#include "execute.h"
#include "tokenizer.h"
#include "parser.h"
#include "ast.h"
#include "sema.h"
#include "bytecode.h"
#include "codegen.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

static jmp_buf error_env;
static int is_repl_mode = 0;

void set_repl_mode(int active) { is_repl_mode = active; }

void throw_repl_error(void) {
    if (is_repl_mode) {
        longjmp(error_env, 1);
    } else {
        exit(1);
    }
}

static void cleanup_all(Tokenizer* tok, Parser* par, ASTNode* ast, 
                        SemAnalyzer* sema, CodeGenerator* cg, 
                        BytecodeChunk* chunk, VM* vm, char* source) {
    if (vm) vm_destroy(vm);
    if (cg) codegen_destroy(cg);
    if (chunk) bytecode_destroy(chunk);
    if (sema) sema_destroy(sema);
    if (ast) ast_free(ast);
    if (par) parser_destroy(par);
    if (tok) tokenizer_destroy(tok);
    if (source) free(source);
}

bool execute_source(const char* filepath, const char* filename) {
    if (!filepath || !filename) return false;

    // Read source file
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = (char*)malloc(size + 1);
    if (!source) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return false;
    }

    if (fread(source, 1, size, f) != (size_t)size) {
        fprintf(stderr, "Error: Cannot read file '%s'\n", filepath);
        fclose(f);
        free(source);
        return false;
    }
    source[size] = '\0';
    fclose(f);

    Tokenizer* tokenizer = NULL;
    Parser* parser = NULL;
    ASTNode* ast = NULL;
    SemAnalyzer* sema = NULL;
    CodeGenerator* cg = NULL;
    BytecodeChunk* chunk = NULL;
    VM* vm = NULL;

    // Recovery point for REPL errors
    if (is_repl_mode && setjmp(error_env) != 0) {
        fprintf(stderr, "[REPL] Error recovered. Ready for next input.\n");
        cleanup_all(tokenizer, parser, ast, sema, cg, chunk, vm, source);
        return false;
    }

    // Tokenization
    tokenizer = tokenizer_create(source, filename);
    int token_count;
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);
    if (!tokens) {
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, NULL, source);
        return false;
    }

    // Parsing
    parser = parser_create(tokens, token_count, filename);
    ast = parser_parse(parser);
    if (!ast) {
        cleanup_all(tokenizer, parser, NULL, NULL, NULL, NULL, NULL, source);
        return false;
    }

    // Semantic analysis
    sema = sema_create(filename);
    if (!sema_analyze(sema, ast)) {
        fprintf(stderr, "Semantic analysis failed for '%s'\n", filename);
        cleanup_all(tokenizer, parser, ast, sema, NULL, NULL, NULL, source);
        return false;
    }

    // Bytecode generation
    chunk = bytecode_create();
    cg = codegen_create(chunk, sema);
    if (!codegen_generate(cg, ast)) {
        fprintf(stderr, "Code generation failed for '%s'\n", filename);
        cleanup_all(tokenizer, parser, ast, sema, cg, chunk, NULL, source);
        return false;
    }

    // VM execution
    vm = vm_create();
    bool ok = vm_execute(vm, chunk);

    cleanup_all(tokenizer, parser, ast, sema, cg, chunk, vm, source);
    return ok;
}
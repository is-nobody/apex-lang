#include "apex_api.h"
#include "execute.h"
#include "platform.h"
#include "tokenizer.h"
#include "parser.h"
#include "ast.h"
#include "bytecode.h"
#include "codegen.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <process.h>
  #define getpid _getpid
#else
  #include <unistd.h>
#endif

static bool is_initialized = false;

// initializes the apex runtime with platform-specific setup
void apex_init(void) {
    if (is_initialized) return;
    
    platform_init();
    
    is_initialized = true;
}

// shuts down the apex runtime and releases global resources
void apex_shutdown(void) {
    if (!is_initialized) return;
    
    is_initialized = false;
}

// executes apex code from a file
bool apex_execute_file(const char* filepath) {
    if (!is_initialized) {
        apex_init();
    }

    if (!filepath) {
        print_error("Invalid filepath provided to apex_execute_file");
        return false;
    }

    return execute_source(filepath, filepath);
}

// executes apex code from a source string with the given filename for error context
bool apex_execute_string(const char* source_code, const char* filename) {
    if (!is_initialized) {
        apex_init();
    }

    if (!source_code) {
        print_error("Invalid source_code provided to apex_execute_string");
        return false;
    }

    const char* error_filename = filename ? filename : "string_script.apex";
    
    size_t source_len = strlen(source_code);
    char* source = (char*)malloc(source_len + 1);
    if (!source) {
        print_error("Memory allocation failed");
        return false;
    }
    memcpy(source, source_code, source_len + 1);
    
    Tokenizer* tokenizer = NULL;
    Parser* parser = NULL;
    ASTNode* ast = NULL;
    CodeGenerator* cg = NULL;
    BytecodeChunk* chunk = NULL;
    VM* vm = NULL;
    
    tokenizer = tokenizer_create(source, error_filename);
    int token_count;
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);

    if (!tokens || tokenizer_has_error(tokenizer)) {
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, source);
        return false;
    }
    
    parser = parser_create(tokens, token_count, error_filename, source);
    ast = parser_parse(parser);
    if (!ast || parser_had_errors(parser)) {
        cleanup_all(tokenizer, parser, ast, NULL, NULL, NULL, source);
        return false;
    }
    
    chunk = bytecode_create();
    cg = codegen_create(chunk);
    if (!codegen_generate(cg, ast)) {
        print_error("Code generation failed for '%s'", error_filename);
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);
        return false;
    }
    
    vm = vm_create(source);
    bool ok = vm_execute(vm, chunk);

    cleanup_all(tokenizer, parser, ast, cg, chunk, vm, source);
    return ok;
}
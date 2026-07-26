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

static bool is_initialized = false;  // runtime initialization flag

// initializes the apex runtime with platform-specific setup
void apex_init(void) {
    if (is_initialized) return;   // already initialized
    
    platform_init();              // initialize platform layer
    
    is_initialized = true;        // mark as initialized
}

// shuts down the apex runtime and releases global resources
void apex_shutdown(void) {
    if (!is_initialized) return;  // not initialized
    
    is_initialized = false;       // mark as shutdown
}

// executes apex code from a file
bool apex_execute_file(const char* filepath) {
    if (!is_initialized) {        // check if runtime is initialized
        apex_init();              // initialize if needed
    }

    if (!filepath) {                                                    // validate filepath
        print_error("Invalid filepath provided to apex_execute_file");  // print error
        return false;                                                   // execution failed
    }

    return execute_source(filepath, filepath);                          // delegate to execute_source
}

// executes apex code from a source string with the given filename for error context
bool apex_execute_string(const char* source_code, const char* filename) {
    if (!is_initialized) {        // check if runtime is initialized
        apex_init();              // initialize if needed
    }

    if (!source_code) {                                                       // validate source code
        print_error("Invalid source_code provided to apex_execute_string");   // print error
        return false;                                                         // execution failed
    }

    const char* error_filename = filename ? filename : "string_script.apex";  // use provided filename or default
    
    size_t source_len = strlen(source_code);       // get source length
    char* source = (char*)malloc(source_len + 1);  // allocate source copy
    if (!source) {                                 // check allocation
        print_error("Memory allocation failed");   // print error
        return false;                              // execution failed
    }
    memcpy(source, source_code, source_len + 1);   // copy source string
    
    Tokenizer* tokenizer = NULL;                   // tokenizer instance
    Parser* parser = NULL;                         // parser instance
    ASTNode* ast = NULL;                           // ast root
    CodeGenerator* cg = NULL;                      // code generator instance
    BytecodeChunk* chunk = NULL;                   // bytecode chunk
    VM* vm = NULL;                                 // virtual machine instance
    
    tokenizer = tokenizer_create(source, error_filename);                 // create tokenizer
    int token_count;                                                      // token count storage
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);          // tokenize source

    if (!tokens || tokenizer_has_error(tokenizer)) {                      // check for tokenization errors
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, source);     // cleanup tokenizer only
        return false;                                                     // execution failed
    }
    
    parser = parser_create(tokens, token_count, error_filename, source);  // create parser
    ast = parser_parse(parser);                                           // parse ast
    if (!ast || parser_had_errors(parser)) {                              // check for parsing errors
        cleanup_all(tokenizer, parser, ast, NULL, NULL, NULL, source);    // cleanup tokenizer and parser
        return false;                                                     // execution failed
    }
    
    chunk = bytecode_create();                                            // create bytecode chunk
    cg = codegen_create(chunk);                                           // create code generator
    if (!codegen_generate(cg, ast)) {                                     // generate bytecode
        print_error("Code generation failed for '%s'", error_filename);   // print error
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);     // cleanup everything except vm
        return false;                                                     // execution failed
    }
    
    vm = vm_create(source);                                               // create virtual machine
    bool ok = vm_execute(vm, chunk);                                      // execute bytecode

    cleanup_all(tokenizer, parser, ast, cg, chunk, vm, source);           // cleanup all resources
    return ok;                                                            // return execution result
}
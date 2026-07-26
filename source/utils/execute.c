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

#define ANSI_RED    "\033[31m"                           // red color for errors
#define ANSI_RESET  "\033[0m"                            // reset terminal color

// prints a formatted error message in red
void print_error(const char* format, ...) {
    fprintf(stderr, "%s[Error] ", ANSI_RED);             // print error prefix in red
    va_list args;                                        // variadic arguments list
    va_start(args, format);                              // start variadic processing
    vfprintf(stderr, format, args);                      // print formatted message
    va_end(args);                                        // end variadic processing
    fprintf(stderr, "%s\n", ANSI_RESET);                 // reset color and newline
}

// cleans up all allocated resources in reverse order
void cleanup_all(Tokenizer* tok, Parser* par, ASTNode* ast,
                        CodeGenerator* cg, BytecodeChunk* chunk, VM* vm,
                        char* source) {
    if (vm) vm_destroy(vm);                              // destroy virtual machine
    if (cg) codegen_destroy(cg);                         // destroy code generator
    if (chunk) bytecode_destroy(chunk);                  // destroy bytecode chunk
    if (ast) ast_free(ast);                              // free ast tree
    if (par) parser_destroy(par);                        // destroy parser
    if (tok) tokenizer_destroy(tok);                     // destroy tokenizer
    if (source) free(source);                            // free source copy
}

// executes apex source code from a file path
bool execute_source(const char* filepath, const char* filename) {
    if (!filepath || !filename) return false;            // validate arguments
    
    FILE* f = fopen(filepath, "rb");                     // open file in binary mode
    if (!f) {                                            // check file open
        print_error("Cannot open file '%s'", filepath);  // print error
        return false;                                    // execution failed
    }
    
    fseek(f, 0, SEEK_END);                               // seek to end
    long size = ftell(f);                                // get file size
    fseek(f, 0, SEEK_SET);                               // seek to start
    
    char* source = (char*)malloc(size + 1);              // allocate source buffer
    if (!source) {                                       // check allocation
        print_error("Memory allocation failed");         // print error
        fclose(f);                                       // close file
        return false;                                    // execution failed
    }
    
    if (fread(source, 1, size, f) != (size_t)size) {     // read file content
        print_error("Cannot read file '%s'", filepath);  // print error
        fclose(f);                                       // close file
        free(source);                                    // free source
        return false;                                    // execution failed
    }
    source[size] = '\0';                                 // null terminate
    fclose(f);                                           // close file
    
    Tokenizer* tokenizer = NULL;                         // tokenizer instance
    Parser* parser = NULL;                               // parser instance
    ASTNode* ast = NULL;                                 // ast root
    CodeGenerator* cg = NULL;                            // code generator instance
    BytecodeChunk* chunk = NULL;                         // bytecode chunk
    VM* vm = NULL;                                       // virtual machine instance
    
    tokenizer = tokenizer_create(source, filename);                     // create tokenizer
    int token_count;                                                    // token count storage
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);        // tokenize source

    if (!tokens || tokenizer_has_error(tokenizer)) {                    // check for tokenization errors
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, source);   // cleanup tokenizer only
        return false;                                                   // execution failed
    }
    
    parser = parser_create(tokens, token_count, filename, source);      // create parser
    ast = parser_parse(parser);                                         // parse ast
    if (!ast || parser_had_errors(parser)) {                            // check for parsing errors
        cleanup_all(tokenizer, parser, ast, NULL, NULL, NULL, source);  // cleanup tokenizer and parser
        return false;                                                   // execution failed
    }
    
    chunk = bytecode_create();                                          // create bytecode chunk
    cg = codegen_create(chunk);                                         // create code generator
    if (!codegen_generate(cg, ast)) {                                   // generate bytecode
        print_error("Code generation failed for '%s'", filename);       // print error
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);   // cleanup everything except vm
        return false;                                                   // execution failed
    }
    
    vm = vm_create(source);                                             // create virtual machine
    bool ok = vm_execute(vm, chunk);                                    // execute bytecode
    
    cleanup_all(tokenizer, parser, ast, cg, chunk, vm, source);         // cleanup all resources
    return ok;                                                          // return execution result
}
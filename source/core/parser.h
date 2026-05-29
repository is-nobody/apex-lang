#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

typedef struct {
    Token* tokens;
    int count;
    int current;
    char* filename;
    
    // Symbol table for tracking declared variables
    // Key = variable name, Value = scope level
    struct {
        char** names;
        int* scope_levels;
        int count;
        int capacity;
        int current_scope;
    } symbols;
} Parser;

Parser* parser_create(Token* tokens, int count, const char* filename);
void parser_destroy(Parser* parser);
ASTNode* parser_parse(Parser* parser);
void parser_error(Parser* parser, const char* message);

// Symbol scope management
void parser_enter_scope(Parser* parser);
void parser_exit_scope(Parser* parser);
void parser_add_symbol(Parser* parser, const char* name);
bool parser_is_declared(Parser* parser, const char* name);

#endif // PARSER_H
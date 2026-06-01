#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include <stdbool.h>

typedef enum {
    TYPE_UNKNOWN,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_BOOLEAN,
    TYPE_TABLE,
    TYPE_FUNCTION,
    TYPE_ERROR,
    TYPE_ANY,
} ValueType;

typedef enum {
    PARSER_SYM_VARIABLE,
    PARSER_SYM_FUNCTION,
    PARSER_SYM_PARAMETER,
    PARSER_SYM_MODULE,
} ParserSymbolKind;

typedef struct {
    char** names;
    int* scope_levels;
    ParserSymbolKind* kinds;
    ValueType* types;
    int* param_counts;
    bool* const_known;
    double* const_values;
    int count;
    int capacity;
    int current_scope;
} ParserSymbolTable;

typedef struct Parser Parser;

struct Parser {
    Token* tokens;
    int count;
    int current;
    char* filename;
    char* source_dir;
    const char* source;

    ParserSymbolTable symbols;
    int error_count;
    int loop_depth;
    int function_depth;
    bool semantic_checks;
};

Parser* parser_create(Token* tokens, int count, const char* filename, const char* source);
void parser_destroy(Parser* parser);
ASTNode* parser_parse(Parser* parser);
bool parser_had_errors(const Parser* parser);

void parser_error(Parser* parser, const char* message);
void parser_error_at(Parser* parser, int line, int column, int len,
                     const char* format, ...);

void parser_enter_scope(Parser* parser);
void parser_exit_scope(Parser* parser);
bool parser_declare_symbol(Parser* parser, const char* name, ParserSymbolKind kind,
                           ValueType type, int param_count, int line, int column);
bool parser_is_declared(Parser* parser, const char* name);
ValueType parser_check_expression(Parser* parser, ASTNode* node);

#endif // PARSER_H

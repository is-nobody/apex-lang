#ifndef PARSER_H
#define PARSER_H
#define ERROR_HISTORY_SIZE 16
#include "ast.h"
#include <stdbool.h>

// type annotations for semantic checking and type inference
typedef enum {
    TYPE_UNKNOWN,        // type not yet determined
    TYPE_NUMBER,         // numeric values (integers and floats)
    TYPE_STRING,         // string values
    TYPE_NONE,           // none/null type
    TYPE_BOOLEAN,        // true/false
    TYPE_TABLE,          // table/array/dictionary
    TYPE_FUNCTION,       // callable function
    TYPE_ERROR,          // type error marker for recovery
    TYPE_ANY,            // any type, used for generic contexts
} ValueType;

// kinds of symbols tracked in the parser's symbol table
typedef enum {
    PARSER_SYM_VARIABLE,     // local or global variable
    PARSER_SYM_FUNCTION,     // function declaration
    PARSER_SYM_PARAMETER,    // function parameter
    PARSER_SYM_MODULE,       // imported module
} ParserSymbolKind;

// symbol table entry with name, scope, kind, type, and constant folding data
typedef struct {
    char** names;
    int* scope_levels;
    ParserSymbolKind* kinds;
    ValueType* types;
    int* param_counts;       // for functions: number of parameters
    bool* const_known;       // whether the symbol has a known constant value
    double* const_values;    // constant value if const_known is true
    int count;
    int capacity;
    int current_scope;
} ParserSymbolTable;

// forward declaration so the struct can reference itself in function signatures
typedef struct Parser Parser;

// main parser context tracking tokens, symbols, errors, and nesting levels
struct Parser {
    Token* tokens;
    int count;
    int current;
    char* filename;
    char* source_dir;
    const char* source;

    ParserSymbolTable symbols;
    int error_count;
    int loop_depth;          // nesting depth of loops (for break/continue checks)
    int function_depth;      // nesting depth of functions (for return checks)
    bool semantic_checks;    // whether to perform type checking and constant folding

    int last_error_line;
    int last_error_column;

    int last_error_lines[ERROR_HISTORY_SIZE];
    int last_error_columns[ERROR_HISTORY_SIZE];
    int last_error_idx;
};

// creates a parser instance for the given token stream and source info
Parser* parser_create(Token* tokens, int count, const char* filename, const char* source);

// frees all parser resources
void parser_destroy(Parser* parser);

// parses the token stream into an ast, returns the root program node
ASTNode* parser_parse(Parser* parser);

// returns true if any syntax or semantic error was encountered
bool parser_had_errors(const Parser* parser);

// reports an error at the current token position
void parser_error(Parser* parser, const char* message);

// reports an error at a specific source location with formatting
void parser_error_at(Parser* parser, int line, int column, int len,
                     const char* format, ...);

// enters a new scope for symbol tracking (e.g., function or block)
void parser_enter_scope(Parser* parser);

// exits the current scope, discarding symbols declared inside it
void parser_exit_scope(Parser* parser);

// declares a new symbol in the current scope, returns false if already declared
bool parser_declare_symbol(Parser* parser, const char* name, ParserSymbolKind kind,
                           ValueType type, int param_count, int line, int column);

// checks whether a symbol is declared in any accessible scope
bool parser_is_declared(Parser* parser, const char* name);

// performs type checking on an expression node, returns its inferred type
ValueType parser_check_expression(Parser* parser, ASTNode* node);

#endif // PARSER_H
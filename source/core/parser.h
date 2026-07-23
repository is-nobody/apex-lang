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
    char** names;            // symbol names (dynamically allocated)
    int* scope_levels;       // lexical scope depth where the symbol was declared
    ParserSymbolKind* kinds; // symbol kind (variable, function, parameter, module)
    ValueType* types;        // inferred or declared type of the symbol
    int* param_counts;       // for functions: number of parameters (0 for non-functions)
    bool* const_known;       // whether the symbol has a known compile-time constant value
    double* const_values;    // constant value if const_known is true (for numeric folding)
    int count;               // number of symbols currently stored
    int capacity;            // allocated capacity of the symbol arrays
    int current_scope;       // current lexical scope depth for symbol lookup
} ParserSymbolTable;

// forward declaration so the struct can reference itself in function signatures
typedef struct Parser Parser;

// main parser context tracking tokens, symbols, errors, and nesting levels
struct Parser {
    Token* tokens;                  // array of tokens from the tokenizer
    int count;                      // total number of tokens
    int current;                    // current token index being parsed

    char* filename;                 // source file name for error reporting
    char* source_dir;               // directory of the source file for module resolution
    const char* source;             // raw source code for error context display

    ParserSymbolTable symbols;      // symbol table for scope and type tracking
    int error_count;                // total number of errors encountered during parsing
    int loop_depth;                 // nesting depth of loops (for break/continue validation)
    int function_depth;             // nesting depth of functions (for return validation)
    bool semantic_checks;           // whether to perform type checking and constant folding
    bool expecting_indented_block;  // true if previous statement opened a block (if/for/fn/elif/else)

    int last_error_line;            // most recent error position for duplicate detection
    int last_error_column;          // most recent error column for duplicate detection

    int last_error_lines[ERROR_HISTORY_SIZE];    // circular buffer of recent error lines
    int last_error_columns[ERROR_HISTORY_SIZE];  // circular buffer of recent error columns
    int last_error_idx;             // current index into the error history buffer
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
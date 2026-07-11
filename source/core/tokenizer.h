#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdbool.h>

// all token types recognized by the lexer, including keywords, literals, and delimiters
typedef enum {
    TOKEN_FUNCTION,      // 'function' keyword for function declarations
    TOKEN_IF,            // 'if' keyword for conditionals
    TOKEN_ELIF,          // 'elif' keyword for else-if chains
    TOKEN_ELSE,          // 'else' keyword for fallback branches
    TOKEN_FOR,           // 'for' keyword for loops
    TOKEN_BREAK,         // 'break' to exit a loop early
    TOKEN_CONTINUE,      // 'continue' to skip to next loop iteration
    TOKEN_RETURN,        // 'return' to exit a function with an optional value
    TOKEN_IMPORT,        // 'import' for loading external modules
    TOKEN_AND,           // logical 'and' operator
    TOKEN_OR,            // logical 'or' operator
    TOKEN_NOT,           // logical 'not' operator
    
    TOKEN_NONE,          // 'none' value
    TOKEN_TRUE,          // boolean literal 'true'
    TOKEN_FALSE,         // boolean literal 'false'
    TOKEN_NUMBER,        // numeric literal (integer or float)
    TOKEN_STRING,        // string literal enclosed in quotes
    TOKEN_IDENTIFIER,    // variable or function name
    
    TOKEN_PLUS,          // addition
    TOKEN_MINUS,         // subtraction
    TOKEN_STAR,          // multiplication
    TOKEN_SLASH,         // division
    TOKEN_PERCENT,       // modulo
    TOKEN_EQUAL,         // assignment '='
    TOKEN_EQUAL_EQUAL,   // equality comparison '=='
    TOKEN_NOT_EQUAL,     // inequality '!='
    TOKEN_LESS,          // less-than '<'
    TOKEN_GREATER,       // greater-than '>'
    TOKEN_LESS_EQUAL,    // less-or-equal '<='
    TOKEN_GREATER_EQUAL, // greater-or-equal '>='
    
    TOKEN_LPAREN,        // '(' for grouping or function calls
    TOKEN_RPAREN,        // ')' closing parenthesis
    TOKEN_LBRACKET,      // '[' for index access
    TOKEN_RBRACKET,      // ']' closing bracket
    TOKEN_COMMA,         // ',' for separating arguments or elements
    TOKEN_DOT,           // '.' for member access
    TOKEN_NEWLINE,       // end of line, significant for statement boundaries
    
    TOKEN_EOF,           // end of file marker

    TOKEN_INDENT,        // indentation increase (block start)
    TOKEN_DEDENT,        // indentation decrease (block end)
} TokenType;

// token structure with type, lexeme, and source position
typedef struct {
    TokenType type;
    char* value;         // dynamically allocated string for the lexeme
    int line;            // 1-based line number
    int column;          // 0-based column within the line
} Token;

// tokenizer state tracking source scan position, indentation, and output buffer
typedef struct {
    char* source;            // the entire source code string
    char* filename;          // source file name for error reporting
    int pos;                 // current position in the source
    int line;                // current line number
    int column;              // current column number
    Token* tokens;           // dynamically growing array of tokens
    int token_count;         // number of tokens collected so far
    int token_capacity;      // allocated capacity of the tokens array
    int indent_stack[256];   // stack for tracking indentation levels
    int indent_depth;        // current indentation depth index
    int pending_newline;     // flag to defer newline emission after indentation
    int paren_depth;         // depth of parentheses to ignore newline significance
    bool has_error;          // whether an error occurred during tokenization
} Tokenizer;

// creates a tokenizer instance for the given source string and filename
Tokenizer* tokenizer_create(const char* source, const char* filename);

// frees all resources used by the tokenizer
void tokenizer_destroy(Tokenizer* tokenizer);

// tokenizes the entire source, returns the token array and sets token_count
Token* tokenizer_tokenize(Tokenizer* tokenizer, int* token_count);

// returns a human-readable name for a token type
const char* token_type_name(TokenType type);

// returns whether an error occurred during tokenization
bool tokenizer_has_error(Tokenizer* tokenizer);

#endif // TOKENIZER_H
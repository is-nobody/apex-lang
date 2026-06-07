// tokenizer.h
#ifndef TOKENIZER_H
#define TOKENIZER_H

typedef enum {
    // Keywords
    TOKEN_FUNCTION,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_RETURN,
    TOKEN_IMPORT,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    
    // Literals
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    
    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    
    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_NEWLINE,
    
    // Special
    TOKEN_EOF,

    TOKEN_INDENT,
    TOKEN_DEDENT, 
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int line;
    int column;
} Token;

typedef struct {
    char* source;
    char* filename;
    int pos;
    int line;
    int column;
    Token* tokens;
    int token_count;
    int token_capacity;
    int indent_stack[256];
    int indent_depth;
    int pending_newline; 
    int paren_depth;
} Tokenizer;

// Tokenizer functions
Tokenizer* tokenizer_create(const char* source, const char* filename);
void tokenizer_destroy(Tokenizer* tokenizer);
Token* tokenizer_tokenize(Tokenizer* tokenizer, int* token_count);
const char* token_type_name(TokenType type);
void token_print(Token* token);

#endif // TOKENIZER_H
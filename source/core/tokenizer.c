// tokenizer.c
#include "tokenizer.h"
#include "execute.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// human-readable names for all token types, used in debug output
static const char* token_type_names[] = {
    "FUNCTION", "IF", "ELIF", "ELSE", "FOR",
    "BREAK", "CONTINUE", "RETURN", "IMPORT",
    "AND", "OR", "NOT",
    "TRUE", "FALSE", "NUMBER", "STRING", "IDENTIFIER",
    "PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "EQUAL",
    "EQUAL_EQUAL", "NOT_EQUAL", "LESS", "GREATER", "LESS_EQUAL",
    "GREATER_EQUAL",
    "LPAREN", "RPAREN", "LBRACKET ", "RBRACKET ", "COMMA", "DOT",
    "NEWLINE", "EOF", "INDENT", "DEDENT",
};

// keyword lookup table for case-sensitive language keywords
typedef struct {
    const char* keyword;
    TokenType type;
} KeywordEntry;

static KeywordEntry keywords[] = {
    {"function", TOKEN_FUNCTION},
    {"if", TOKEN_IF},
    {"elif", TOKEN_ELIF},
    {"else", TOKEN_ELSE},
    {"for", TOKEN_FOR},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"return", TOKEN_RETURN},
    {"import", TOKEN_IMPORT},
    {"and", TOKEN_AND},
    {"or", TOKEN_OR},
    {"not", TOKEN_NOT},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {NULL, TOKEN_EOF}
};

// returns the string name for a token type, used for debugging
const char* token_type_name(TokenType type) {
    if (type >= 0 && type < (int)(sizeof(token_type_names) / sizeof(token_type_names[0]))) {
        return token_type_names[type];
    }
    return "UNKNOWN";
}

// prints a token's details including escaped special characters for readability
void token_print(Token* token) {
    if (token->type == TOKEN_INDENT) {
        printf("%-20s %-25s %-8d %-8d\n", "INDENT", "", token->line, token->column);
        return;
    }
    if (token->type == TOKEN_DEDENT) {
        printf("%-20s %-25s %-8d %-8d\n", "DEDENT", "", token->line, token->column);
        return;
    }
    
    char display_value[512];
    int j = 0;
    for (int i = 0; token->value[i] != '\0' && j < 510; i++) {
        if (token->value[i] == '\n') {
            display_value[j++] = '\\';
            display_value[j++] = 'n';
        } else if (token->value[i] == '\t') {
            display_value[j++] = '\\';
            display_value[j++] = 't';
        } else if (token->value[i] == '\r') {
            display_value[j++] = '\\';
            display_value[j++] = 'r';
        } else {
            display_value[j++] = token->value[i];
        }
    }
    display_value[j] = '\0';
    
    char quoted_value[520];
    snprintf(quoted_value, sizeof(quoted_value), "'%s'", display_value);
    
    printf("%-20s %-25s %-8d %-8d\n", 
           token_type_name(token->type), 
           quoted_value, 
           token->line, 
           token->column);
}

// creates a tokenizer instance with initial capacity and source tracking
Tokenizer* tokenizer_create(const char* source, const char* filename) {
    Tokenizer* tokenizer = (Tokenizer*)malloc(sizeof(Tokenizer));
    tokenizer->source = strdup(source);
    tokenizer->filename = strdup(filename ? filename : "<unknown>");
    tokenizer->pos = 0;
    tokenizer->line = 1;
    tokenizer->column = 1;
    tokenizer->token_capacity = 256;
    tokenizer->tokens = (Token*)malloc(sizeof(Token) * tokenizer->token_capacity);
    tokenizer->token_count = 0;
    tokenizer->indent_stack[0] = 0;
    tokenizer->indent_depth = 1;
    tokenizer->pending_newline = 0;
    tokenizer->paren_depth = 0;
    return tokenizer;
}

// frees all tokenizer resources including tokens and source strings
void tokenizer_destroy(Tokenizer* tokenizer) {
    if (tokenizer) {
        free(tokenizer->source);
        free(tokenizer->filename);
        for (int i = 0; i < tokenizer->token_count; i++) {
            free(tokenizer->tokens[i].value);
        }
        free(tokenizer->tokens);
        free(tokenizer);
    }
}

// reports a tokenizer error with source context and throws a repl error
static void tokenizer_error(Tokenizer* tokenizer, int len, const char* message) {
    print_error_with_context(tokenizer->filename, tokenizer->source,
                             tokenizer->line, tokenizer->column, len,
                             "Tokenizer Error", message);
    throw_repl_error();
}

// peeks at a character ahead in the source without consuming it
static char peek(Tokenizer* tokenizer, int offset) {
    int index = tokenizer->pos + offset;
    if (index >= (int)strlen(tokenizer->source)) {
        return '\0';
    }
    return tokenizer->source[index];
}

// consumes and returns the next character, updating line and column positions
static char advance(Tokenizer* tokenizer) {
    if (tokenizer->pos >= (int)strlen(tokenizer->source)) {
        return '\0';
    }
    char c = tokenizer->source[tokenizer->pos++];
    if (c == '\n') {
        tokenizer->line++;
        tokenizer->column = 1;
    } else {
        tokenizer->column++;
    }
    return c;
}

// adds a new token to the dynamic array, resizing if necessary
static void add_token(Tokenizer* tokenizer, TokenType type, const char* value, int line, int column) {
    if (tokenizer->token_count >= tokenizer->token_capacity) {
        tokenizer->token_capacity *= 2;
        tokenizer->tokens = (Token*)realloc(tokenizer->tokens, 
                                           sizeof(Token) * tokenizer->token_capacity);
    }
    tokenizer->tokens[tokenizer->token_count].type = type;
    tokenizer->tokens[tokenizer->token_count].value = strdup(value);
    tokenizer->tokens[tokenizer->token_count].line = line;
    tokenizer->tokens[tokenizer->token_count].column = column;
    tokenizer->token_count++;
}

// skips spaces, tabs, and carriage returns without producing tokens
static void skip_whitespace(Tokenizer* tokenizer) {
    char c = peek(tokenizer, 0);
    while (c == ' ' || c == '\t' || c == '\r') {
        advance(tokenizer);
        c = peek(tokenizer, 0);
    }
}

// consumes a line comment starting with '//' until the end of the line
static void skip_comment(Tokenizer* tokenizer) {
    advance(tokenizer); // skip first '/'
    advance(tokenizer); // skip second '/'
    
    char c = peek(tokenizer, 0);
    while (c != '\0' && c != '\n' && c != '\r') {
        advance(tokenizer);
        c = peek(tokenizer, 0);
    }
}

// reads a quoted string literal with escape sequence handling
static char* read_string(Tokenizer* tokenizer) {
    advance(tokenizer); // skip opening quote
    char* buffer = (char*)malloc(256);
    if (!buffer) return NULL;
    int buf_size = 256;
    int buf_pos = 0;
    
    while (1) {
        char c = peek(tokenizer, 0);
        
        if (c == '\0') {
            tokenizer_error(tokenizer, 1, "Unterminated string");
            break;
        }
        
        if (c == '\\') {
            advance(tokenizer); // skip backslash
            char next_c = peek(tokenizer, 0);
            char char_to_add = next_c;
            
            switch (next_c) {
                case 'n': char_to_add = '\n'; break;
                case 't': char_to_add = '\t'; break;
                case 'r': char_to_add = '\r'; break;
                case '"': char_to_add = '"'; break;
                case '\\': char_to_add = '\\'; break;
                default: 
                    if (next_c == '{' || next_c == '}') {
                         if (buf_pos + 2 >= buf_size) {
                            buf_size *= 2;
                            buffer = (char*)realloc(buffer, buf_size);
                        }
                        buffer[buf_pos++] = '\\';
                        buffer[buf_pos++] = next_c;
                        advance(tokenizer);
                        continue;
                    }
                    break;
            }
            
            if (buf_pos + 1 >= buf_size) {
                buf_size *= 2;
                buffer = (char*)realloc(buffer, buf_size);
            }
            buffer[buf_pos++] = char_to_add;
            advance(tokenizer); // skip the escaped character
            continue;
        }
        
        if (c == '"') {
            advance(tokenizer); 
            break;
        }
        
        if (buf_pos + 1 >= buf_size) {
            buf_size *= 2;
            buffer = (char*)realloc(buffer, buf_size);
        }
        buffer[buf_pos++] = advance(tokenizer);
    }

    buffer[buf_pos] = '\0';
    return buffer;
}

// reads a numeric literal including integer, float, and scientific notation
static char* read_number(Tokenizer* tokenizer) {
    char* buffer = (char*)malloc(64);
    int buf_pos = 0;
    char c = peek(tokenizer, 0);
    
    while (isdigit(c)) {
        buffer[buf_pos++] = advance(tokenizer);
        c = peek(tokenizer, 0);
    }

    if (c == '.' && isdigit(peek(tokenizer, 1))) {
        buffer[buf_pos++] = advance(tokenizer);
        c = peek(tokenizer, 0);
        while (isdigit(c)) {
            buffer[buf_pos++] = advance(tokenizer);
            c = peek(tokenizer, 0);
        }
    }

    if (c == 'e' || c == 'E') {
        char next = peek(tokenizer, 1);
        if (isdigit(next) || ((next == '+' || next == '-') && isdigit(peek(tokenizer, 2)))) {
            buffer[buf_pos++] = advance(tokenizer);
            c = peek(tokenizer, 0);
            if (c == '+' || c == '-') {
                buffer[buf_pos++] = advance(tokenizer);
                c = peek(tokenizer, 0);
            }
            while (isdigit(c)) {
                buffer[buf_pos++] = advance(tokenizer);
                c = peek(tokenizer, 0);
            }
        }
    }

    buffer[buf_pos] = '\0';
    return buffer;
}

// reads an identifier or keyword starting with a letter or underscore
static char* read_identifier(Tokenizer* tokenizer) {
    char* buffer = (char*)malloc(256);
    int buf_pos = 0;
    
    char c = peek(tokenizer, 0);
    while (c != '\0' && (isalnum(c) || c == '_')) {
        buffer[buf_pos++] = advance(tokenizer);
        c = peek(tokenizer, 0);
    }
    
    buffer[buf_pos] = '\0';
    return buffer;
}

// checks if an identifier is a keyword, returns the appropriate token type
static TokenType lookup_keyword(const char* identifier) {
    if (strcasecmp(identifier, "true") == 0) return TOKEN_TRUE;
    if (strcasecmp(identifier, "false") == 0) return TOKEN_FALSE;
    
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strcmp(identifier, keywords[i].keyword) == 0) {
            return keywords[i].type;
        }
    }
    
    return TOKEN_IDENTIFIER;
}

// main tokenization loop that processes the entire source and returns tokens
Token* tokenizer_tokenize(Tokenizer* tokenizer, int* out_count) {
    while (tokenizer->pos < (int)strlen(tokenizer->source)) {
        skip_whitespace(tokenizer);
        
        if (tokenizer->pos >= (int)strlen(tokenizer->source)) {
            break;
        }
        
        char c = peek(tokenizer, 0);
        int line = tokenizer->line;
        int col = tokenizer->column;
        
    if (c == '\n') {
        advance(tokenizer);
        add_token(tokenizer, TOKEN_NEWLINE, "\n", line, col);
        
        while (tokenizer->pos  < (int)strlen(tokenizer->source)) {
            char nc = peek(tokenizer, 0);
            if (nc == ' ' || nc == '\t' || nc == '\r') {
                advance(tokenizer);
            } else if (nc == '\n') {
                advance(tokenizer);
                add_token(tokenizer, TOKEN_NEWLINE,  "\n", tokenizer->line, tokenizer->column);
            } else if (nc == '/'  && peek(tokenizer, 1) == '/') {
                skip_comment(tokenizer);
            } else {
                break;
            }
        }
        
        if (tokenizer->paren_depth == 0) {
            int current_indent = 0;
            if (tokenizer->pos < (int)strlen(tokenizer->source)) {
                current_indent = tokenizer->column - 1;
            }
            int prev_indent = tokenizer->indent_stack[tokenizer->indent_depth - 1];
            
            if (current_indent > prev_indent) {
                int expected_indent = prev_indent + 4;
                if (current_indent != expected_indent) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Expected indentation of %d spaces, got %d", expected_indent, current_indent);
                    tokenizer_error(tokenizer, current_indent > 0 ? current_indent : 1, msg);
                }
                add_token(tokenizer, TOKEN_INDENT, " ", tokenizer->line, tokenizer->column);
                tokenizer->indent_stack[tokenizer->indent_depth++] = current_indent;
            } else if (current_indent < prev_indent) {
                while (tokenizer->indent_depth > 1 &&
                    current_indent < tokenizer->indent_stack[tokenizer->indent_depth - 1]) {
                    add_token(tokenizer, TOKEN_DEDENT, " ", tokenizer->line, tokenizer->column);
                    tokenizer->indent_depth--;
                }
            }
        }
        continue;
    }
        
        if (c == '/' && peek(tokenizer, 1) == '/') {
            skip_comment(tokenizer);
            continue;
        }
        
        if (c == '"') {
            char* string_value = read_string(tokenizer);
            add_token(tokenizer, TOKEN_STRING, string_value, line, col);
            free(string_value);
            continue;
        }
        
        if (isdigit(c)) {
            char* number_value = read_number(tokenizer);
            add_token(tokenizer, TOKEN_NUMBER, number_value, line, col);
            free(number_value);
            continue;
        }
        
        if (isalpha(c) || c == '_') {
            char* identifier = read_identifier(tokenizer);
            TokenType type = lookup_keyword(identifier);
            add_token(tokenizer, type, identifier, line, col);
            free(identifier);
            continue;
        }
        
        if (c == '=' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);
            advance(tokenizer);
            add_token(tokenizer, TOKEN_EQUAL_EQUAL, "==", line, col);
            continue;
        }
        
        if (c == '!' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);
            advance(tokenizer);
            add_token(tokenizer, TOKEN_NOT_EQUAL, "!=", line, col);
            continue;
        }
        
        if (c == '<' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);
            advance(tokenizer);
            add_token(tokenizer, TOKEN_LESS_EQUAL, "<=", line, col);
            continue;
        }
        
        if (c == '>' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);
            advance(tokenizer);
            add_token(tokenizer, TOKEN_GREATER_EQUAL, ">=", line, col);
            continue;
        }
        
        switch (c) {
            case '+': advance(tokenizer); add_token(tokenizer, TOKEN_PLUS, "+", line, col); continue;
            case '-': advance(tokenizer); add_token(tokenizer, TOKEN_MINUS, "-", line, col); continue;
            case '*': advance(tokenizer); add_token(tokenizer, TOKEN_STAR, "*", line, col); continue;
            case '/': advance(tokenizer); add_token(tokenizer, TOKEN_SLASH, "/", line, col); continue;
            case '%': advance(tokenizer); add_token(tokenizer, TOKEN_PERCENT, "%", line, col); continue;
            case '=': advance(tokenizer); add_token(tokenizer, TOKEN_EQUAL, "=", line, col); continue;
            case '<': advance(tokenizer); add_token(tokenizer, TOKEN_LESS, "<", line, col); continue;
            case '>': advance(tokenizer); add_token(tokenizer, TOKEN_GREATER, ">", line, col); continue;
            case '(':
                advance(tokenizer);
                tokenizer->paren_depth++;
                add_token(tokenizer, TOKEN_LPAREN, "(", line, col);
                continue;
            case ')':
                advance(tokenizer);
                if (tokenizer->paren_depth > 0) tokenizer->paren_depth--;
                add_token(tokenizer, TOKEN_RPAREN, ")", line, col);
                continue;
            case '[': 
                advance(tokenizer); 
                tokenizer->paren_depth++;
                add_token(tokenizer, TOKEN_LBRACKET, "[", line, col); 
                continue;
            case ']': 
                advance(tokenizer); 
                if (tokenizer->paren_depth > 0) tokenizer->paren_depth--;
                add_token(tokenizer, TOKEN_RBRACKET, "]", line, col); 
                continue;
            case ',': advance(tokenizer); add_token(tokenizer, TOKEN_COMMA, ",", line, col); continue;
            case '.':
                advance(tokenizer);
                add_token(tokenizer, TOKEN_DOT, ".", line, col);
                continue;
        }
        
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Unexpected character: '%c'", c);
        tokenizer_error(tokenizer, 1, error_msg);
    }
    
    while (tokenizer->indent_depth > 1) {
        add_token(tokenizer, TOKEN_DEDENT, " ", tokenizer->line, tokenizer->column);
        tokenizer->indent_depth--;
    }

    if (tokenizer->token_count > 0) {
        Token* last = &tokenizer->tokens[tokenizer->token_count - 1];
        if (last->type != TOKEN_NEWLINE && last->type != TOKEN_EOF) {
            add_token(tokenizer, TOKEN_NEWLINE, "\n", tokenizer->line, tokenizer->column);
        }
    } else if (tokenizer->token_count == 0) {
        add_token(tokenizer, TOKEN_NEWLINE, "\n", tokenizer->line, tokenizer->column);
    }
    
    add_token(tokenizer, TOKEN_EOF, "", tokenizer->line, tokenizer->column);
    
    if (out_count) {
        *out_count = tokenizer->token_count;
    }
    
    return tokenizer->tokens;
}
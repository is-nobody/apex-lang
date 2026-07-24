// tokenizer.c
#include "tokenizer.h"
#include "execute.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #include <string.h>
  #define strcasecmp _stricmp
#else
  #include <strings.h>
#endif

// human-readable names for all token types, used in debug output
static const char* token_type_names[] = {
    "FUNCTION", "IF", "ELIF", "ELSE", "FOR",
    "BREAK", "CONTINUE", "RETURN", "IMPORT",
    "AND", "OR", "NOT",
    "NONE", "TRUE", "FALSE", "NUMBER", "STRING", "IDENTIFIER",
    "PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "EQUAL",
    "EQUAL_EQUAL", "NOT_EQUAL", "LESS", "GREATER", "LESS_EQUAL",
    "GREATER_EQUAL",
    "LPAREN", "RPAREN", "LBRACKET ", "RBRACKET ", "COMMA", "DOT",
    "NEWLINE", "EOF", "INDENT", "DEDENT",
};

// keyword lookup table for case-sensitive language keywords
typedef struct {
    const char* keyword;  // keyword string
    TokenType type;       // corresponding token type
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
    {"none", TOKEN_NONE},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {NULL, TOKEN_EOF}
};

// returns the string name for a token type, used for debugging
const char* token_type_name(TokenType type) {
    if (type >= 0 && type < (int)(sizeof(token_type_names) / sizeof(token_type_names[0]))) {
        return token_type_names[type];                     // return name from lookup table
    }
    return "UNKNOWN";                                      // fallback for invalid token types
}

// creates a tokenizer instance with initial capacity and source tracking
Tokenizer* tokenizer_create(const char* source, const char* filename) {
    Tokenizer* tokenizer = (Tokenizer*)malloc(sizeof(Tokenizer));     // allocate tokenizer struct
    tokenizer->source = strdup(source);                               // duplicate source string for ownership
    tokenizer->filename = strdup(filename ? filename : "<unknown>");  // duplicate filename or use default
    tokenizer->pos = 0;                                     // start at beginning of source
    tokenizer->line = 1;                                    // start at line 1
    tokenizer->column = 1;                                  // start at column 1
    tokenizer->token_capacity = 256;                        // initial token array capacity
    tokenizer->tokens = (Token*)malloc(sizeof(Token) * tokenizer->token_capacity);  // allocate token array
    tokenizer->token_count = 0;                             // no tokens produced yet
    tokenizer->indent_stack[0] = 0;                         // base indentation level is zero
    tokenizer->indent_depth = 1;                            // stack starts with one entry
    tokenizer->pending_newline = 0;                         // no pending newlines
    tokenizer->paren_depth = 0;                             // not inside any parentheses/brackets
    tokenizer->has_error = false;                           // no errors yet
    return tokenizer;                                       // return initialized tokenizer
}

// frees all tokenizer resources including tokens and source strings
void tokenizer_destroy(Tokenizer* tokenizer) {
    if (tokenizer) {
        free(tokenizer->source);                            // free duplicated source string
        free(tokenizer->filename);                          // free duplicated filename
        for (int i = 0; i < tokenizer->token_count; i++) {
            free(tokenizer->tokens[i].value);               // free each token's value string
        }
        free(tokenizer->tokens);                            // free token array
        free(tokenizer);                                    // free tokenizer struct itself
    }
}

// reports a tokenizer error with source context and throws a repl error
static void tokenizer_error(Tokenizer* tokenizer, int len, const char* message) {
    tokenizer->has_error = true;                           // mark that an error occurred
    print_error_with_context(tokenizer->filename, tokenizer->source,
                             tokenizer->line, tokenizer->column, len,
                             "Tokenizer Error", message);  // print formatted error with source context
}

// peeks at a character ahead in the source without consuming it
static char peek(Tokenizer* tokenizer, int offset) {
    int index = tokenizer->pos + offset;            // compute absolute position in source
    if (index >= (int)strlen(tokenizer->source)) {
        return '\0';                                // return null terminator at end of source
    }
    return tokenizer->source[index];                // return character at offset
}

// consumes and returns the next character, updating line and column positions
static char advance(Tokenizer* tokenizer) {
    if (tokenizer->pos >= (int)strlen(tokenizer->source)) {
        return '\0';                               // return null terminator at end of source
    }
    char c = tokenizer->source[tokenizer->pos++];  // read and advance position
    if (c == '\n') {
        tokenizer->line++;                         // increment line counter on newline
        tokenizer->column = 1;                     // reset column to 1 on new line
    } else {
        unsigned char uc = (unsigned char)c;
        if (uc < 0x80 || uc >= 0xC0) {
            tokenizer->column++;                   // increment column for single-byte or start byte
        }
    }
    return c;                                      // return consumed character
}

// adds a new token to the dynamic array, resizing if necessary
static void add_token(Tokenizer* tokenizer, TokenType type, const char* value, int line, int column) {
    if (tokenizer->token_count >= tokenizer->token_capacity) {
        tokenizer->token_capacity *= 2;                         // double capacity when full
        tokenizer->tokens = (Token*)realloc(tokenizer->tokens, 
                                           sizeof(Token) * tokenizer->token_capacity); // resize token array
    }
    tokenizer->tokens[tokenizer->token_count].type = type;      // set token type
    tokenizer->tokens[tokenizer->token_count].value = strdup(value);  // duplicate token value string
    tokenizer->tokens[tokenizer->token_count].line = line;      // record source line
    tokenizer->tokens[tokenizer->token_count].column = column;  // record source column
    tokenizer->token_count++;                                   // increment token count
}

// skips spaces, tabs, and carriage returns without producing tokens
static void skip_whitespace(Tokenizer* tokenizer) {
    char c = peek(tokenizer, 0);                          // look at current character
    while (c == ' ' || c == '\t' || c == '\r') {
        advance(tokenizer);                               // consume whitespace character
        c = peek(tokenizer, 0);                           // check next character
    }
}

// consumes a line comment starting with '//' until the end of the line
static void skip_comment(Tokenizer* tokenizer) {
    advance(tokenizer);                                   // skip first '/'
    advance(tokenizer);                                   // skip second '/'
    
    char c = peek(tokenizer, 0);                          // look at current character
    while (c != '\0' && c != '\n' && c != '\r') {
        advance(tokenizer);                               // consume comment characters until end of line
        c = peek(tokenizer, 0);                           // check next character
    }
}

// consumes a shebang line starting with '#!' until the end of the line
static void skip_shebang(Tokenizer* tokenizer) {
    advance(tokenizer);                                   // skip '#'
    advance(tokenizer);                                   // skip '!'
    
    char c = peek(tokenizer, 0);                          // look at current character
    while (c != '\0' && c != '\n' && c != '\r') {
        advance(tokenizer);                               // consume shebang characters until end of line
        c = peek(tokenizer, 0);                           // check next character
    }
}

// reads a quoted string literal with escape sequence handling and interpolation awareness
static char* read_string(Tokenizer* tokenizer) {
    char quote_char = peek(tokenizer, 0);                // get the opening quote character (' or ")
    int quote_line = tokenizer->line;                    // save line for error reporting
    int quote_column = tokenizer->column;                // save column for error reporting
    int quote_pos = tokenizer->pos;                      // save position for error recovery
    advance(tokenizer);                                  // skip opening quote
    
    char* buffer = (char*)malloc(256);                   // allocate initial buffer for string content
    if (!buffer) return NULL;                            // allocation failed
    int buf_size = 256;                                  // track buffer capacity
    int buf_pos = 0;                                     // current write position in buffer
    
    while (1) {
        char c = peek(tokenizer, 0);                     // peek at next character
        
        if (c == '\0') {
            tokenizer->pos = quote_pos;                  // restore position for error context
            tokenizer->line = quote_line;                // restore line for error context
            tokenizer->column = quote_column;            // restore column for error context
            tokenizer_error(tokenizer, 1, "Unterminated string");  // report unterminated string error
            tokenizer->pos = strlen(tokenizer->source);  // advance to end to stop further tokenizing
            free(buffer);                                // free partial buffer
            return NULL;                                 // return null on error
        }
        
        if (c == '\\') {
            advance(tokenizer);                              // consume backslash
            char next_c = peek(tokenizer, 0);                // peek at escaped character
            
            if (next_c == '\0') {
                tokenizer->pos = quote_pos;                  // restore position for error context
                tokenizer->line = quote_line;                // restore line for error context
                tokenizer->column = quote_column;            // restore column for error context
                tokenizer_error(tokenizer, 1, "Unterminated string");  // report unterminated string error
                tokenizer->pos = strlen(tokenizer->source);  // advance to end to stop further tokenizing
                free(buffer);                                // free partial buffer
                return NULL;                                 // return null on error
            }
            
            if (next_c == '{' || next_c == '}') {
                if (buf_pos + 2 >= buf_size) {
                    buf_size *= 2;                              // double buffer capacity
                    buffer = (char*)realloc(buffer, buf_size);  // resize buffer
                }
                buffer[buf_pos++] = '\\';                   // preserve backslash in output
                buffer[buf_pos++] = next_c;                 // preserve brace character in output
                advance(tokenizer);                         // consume the brace character
                continue;                                   // continue reading string
            }
            
            char char_to_add = next_c;                      // default: use character as-is
            switch (next_c) {
                case 'n': char_to_add = '\n'; break;        // newline escape
                case 't': char_to_add = '\t'; break;        // tab escape
                case 'r': char_to_add = '\r'; break;        // carriage return escape
                case '"': char_to_add = '"'; break;         // double quote escape
                case '\'': char_to_add = '\''; break;       // single quote escape
                case '\\': char_to_add = '\\'; break;       // backslash escape
                default: break;                             // unknown escape, use literal character
            }
            
            if (buf_pos + 1 >= buf_size) {
                buf_size *= 2;                              // double buffer capacity
                buffer = (char*)realloc(buffer, buf_size);  // resize buffer
            }
            buffer[buf_pos++] = char_to_add;                // store interpreted escape character
            advance(tokenizer);                             // consume the escaped character
            continue;                                       // continue reading string
        }
        
        if (c == quote_char) {
            advance(tokenizer);                             // consume closing quote
            break;                                          // end of string literal
        }
        
        if (buf_pos + 1 >= buf_size) {
            buf_size *= 2;                                  // double buffer capacity
            buffer = (char*)realloc(buffer, buf_size);      // resize buffer
        }
        buffer[buf_pos++] = advance(tokenizer);             // store regular character
    }

    buffer[buf_pos] = '\0';                                 // null-terminate the string
    return buffer;                                          // return allocated string content
}

// reads a numeric literal including integer, float, and scientific notation
static char* read_number(Tokenizer* tokenizer) {
    char* buffer = (char*)malloc(64);                       // allocate buffer for number string (max 64 chars)
    int buf_pos = 0;                                        // current write position in buffer
    char c = peek(tokenizer, 0);                            // peek at first character
    
    while (isdigit(c)) {
        buffer[buf_pos++] = advance(tokenizer);             // consume integer digits
        c = peek(tokenizer, 0);                             // peek at next character
    }

    if (c == '.' && isdigit(peek(tokenizer, 1))) {
        buffer[buf_pos++] = advance(tokenizer);             // consume decimal point
        c = peek(tokenizer, 0);                             // peek at next character
        while (isdigit(c)) {
            buffer[buf_pos++] = advance(tokenizer);         // consume fractional digits
            c = peek(tokenizer, 0);                         // peek at next character
        }
    }

    if (c == 'e' || c == 'E') {
        char next = peek(tokenizer, 1);                     // peek at character after 'e'
        if (isdigit(next) || ((next == '+' || next == '-') && isdigit(peek(tokenizer, 2)))) {
            buffer[buf_pos++] = advance(tokenizer);         // consume 'e' or 'E'
            c = peek(tokenizer, 0);                         // peek at next character
            if (c == '+' || c == '-') {
                buffer[buf_pos++] = advance(tokenizer);     // consume exponent sign
                c = peek(tokenizer, 0);                     // peek at next character
            }
            while (isdigit(c)) {
                buffer[buf_pos++] = advance(tokenizer);     // consume exponent digits
                c = peek(tokenizer, 0);                     // peek at next character
            }
        }
    }

    buffer[buf_pos] = '\0';                                 // null-terminate the number string
    return buffer;                                          // return allocated number string
}

// reads an identifier or keyword starting with a letter or underscore
static char* read_identifier(Tokenizer* tokenizer) {
    char* buffer = (char*)malloc(256);                      // allocate buffer for identifier
    int buf_pos = 0;                                        // current write position in buffer
    int buf_size = 256;                                     // track buffer capacity
    
    char c = peek(tokenizer, 0);                            // peek at first character
    while (c != '\0') {
        unsigned char uc = (unsigned char)c;
        if (isalnum(c) || c == '_' || uc >= 0x80) {
            if (buf_pos + 4 >= buf_size) {
                buf_size *= 2;                              // double buffer capacity
                buffer = (char*)realloc(buffer, buf_size);  // resize buffer
            }
            buffer[buf_pos++] = advance(tokenizer);         // consume identifier character
            c = peek(tokenizer, 0);                         // peek at next character
        } else {
            break;                                          // stop at non-identifier character
        }
    }
    
    buffer[buf_pos] = '\0';                                       // null-terminate the identifier
    return buffer;                                                // return allocated identifier string
}

// checks if an identifier is a keyword, returns the appropriate token type
static TokenType lookup_keyword(const char* identifier) {
    if (strcasecmp(identifier, "true") == 0) return TOKEN_TRUE;    // case-insensitive check for true
    if (strcasecmp(identifier, "false") == 0) return TOKEN_FALSE;  // case-insensitive check for false
    
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strcmp(identifier, keywords[i].keyword) == 0) {
            return keywords[i].type;                           // return keyword token type on match
        }
    }
    
    return TOKEN_IDENTIFIER;                                   // not a keyword, it's a user identifier
}

// returns whether an error occurred during tokenization
bool tokenizer_has_error(Tokenizer* tokenizer) {
    return tokenizer->has_error;                               // return error flag state
}

// main tokenization loop that processes the entire source and returns tokens
Token* tokenizer_tokenize(Tokenizer* tokenizer, int* out_count) {
    while (tokenizer->pos < (int)strlen(tokenizer->source)) {
        skip_whitespace(tokenizer);                            // skip spaces and tabs before each token
        
        if (tokenizer->pos >= (int)strlen(tokenizer->source)) {
            break;                                             // reached end of source after skipping whitespace
        }
        
        char c = peek(tokenizer, 0);                           // peek at next non-whitespace character
        int line = tokenizer->line;                            // save line for token position
        int col = tokenizer->column;                           // save column for token position
        
    if (c == '\n') {
        advance(tokenizer);                                    // consume newline character
        add_token(tokenizer, TOKEN_NEWLINE, "\n", line, col);  // emit newline token
        
        while (tokenizer->pos  < (int)strlen(tokenizer->source)) {
            char nc = peek(tokenizer, 0);                // peek at character after newline
            if (nc == ' ' || nc == '\t' || nc == '\r') {
                advance(tokenizer);                      // skip inline whitespace after newline
            } else if (nc == '\n') {
                advance(tokenizer);                      // consume consecutive blank line
                add_token(tokenizer, TOKEN_NEWLINE,  "\n", tokenizer->line, tokenizer->column);  // emit newline for blank line
            } else if (nc == '/'  && peek(tokenizer, 1) == '/') {
                skip_comment(tokenizer);                 // skip comment on new line
            } else {
                break;                                   // non-whitespace found, stop consuming newlines
            }
        }
        
        if (tokenizer->paren_depth == 0) {
            int current_indent = 0;                      // default indent level
            if (tokenizer->pos < (int)strlen(tokenizer->source)) {
                current_indent = tokenizer->column - 1;  // measure indentation from column position
            }
            int prev_indent = tokenizer->indent_stack[tokenizer->indent_depth - 1];  // get previous indentation level
            
            if (current_indent > prev_indent) {
                int expected_indent = prev_indent + 4;  // expect exactly 4-space indent increase
                if (current_indent != expected_indent) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Expected indentation of %d spaces, got %d", expected_indent, current_indent);
                    tokenizer_error(tokenizer, current_indent > 0 ? current_indent : 1, msg);     // report wrong indentation
                }
                add_token(tokenizer, TOKEN_INDENT, " ", tokenizer->line, tokenizer->column);      // emit indent token
                tokenizer->indent_stack[tokenizer->indent_depth++] = current_indent;              // push new indentation level
            } else if (current_indent < prev_indent) {
                while (tokenizer->indent_depth > 1 &&
                    current_indent < tokenizer->indent_stack[tokenizer->indent_depth - 1]) {
                    add_token(tokenizer, TOKEN_DEDENT, " ", tokenizer->line, tokenizer->column);  // emit dedent token
                    tokenizer->indent_depth--;  // pop indentation level from stack
                }
            }
        }
        continue;                         // move to next iteration
    }
        
        if (c == '/' && peek(tokenizer, 1) == '/') {
            skip_comment(tokenizer);      // skip line comment
            continue;                     // move to next iteration
        }
        
        // ignore shebang only on the first line, otherwise throw an error
        if (c == '#' && peek(tokenizer, 1) == '!') {
            if (tokenizer->line == 1 && tokenizer->pos == 0) {
                skip_shebang(tokenizer);  // shebang allowed only at start of file
                continue;                 // move to next iteration
            } else {
                tokenizer_error(tokenizer, 2, "Shebang (#!) is only allowed on the first line");  // error for misplaced shebang
            }
        }
        
        if (c == '"' || c == '\'') {
            int saved_line = line;                        // save line before reading string
            int saved_col = col;                          // save column before reading string
            char* string_value = read_string(tokenizer);  // read string literal content
            if (string_value != NULL) {
                add_token(tokenizer, TOKEN_STRING, string_value, saved_line, saved_col);         // emit string token
                free(string_value);                       // free temporary string buffer
            }
            continue;                                     // move to next iteration
        }
        
        if (isdigit(c)) {
            char* number_value = read_number(tokenizer);                  // read numeric literal
            add_token(tokenizer, TOKEN_NUMBER, number_value, line, col);  // emit number token
            free(number_value);                                           // free temporary number buffer
            continue;                                                     // move to next iteration
        }
        
        if (isalpha(c) || c == '_' || (unsigned char)c >= 0x80) {
            char* identifier = read_identifier(tokenizer);      // read identifier or keyword
            TokenType type = lookup_keyword(identifier);        // determine if it's a keyword
            add_token(tokenizer, type, identifier, line, col);  // emit identifier or keyword token
            free(identifier);                                   // free temporary identifier buffer
            continue;                                           // move to next iteration
        }
        
        if (c == '=' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);                                        // consume first '='
            advance(tokenizer);                                        // consume second '='
            add_token(tokenizer, TOKEN_EQUAL_EQUAL, "==", line, col);  // emit equality comparison token
            continue;                                                  // move to next iteration
        }
        
        if (c == '!' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);                                      // consume '!'
            advance(tokenizer);                                      // consume '='
            add_token(tokenizer, TOKEN_NOT_EQUAL, "!=", line, col);  // emit not-equal comparison token
            continue;                                                // move to next iteration
        }
        
        if (c == '<' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);                                       // consume '<'
            advance(tokenizer);                                       // consume '='
            add_token(tokenizer, TOKEN_LESS_EQUAL, "<=", line, col);  // emit less-or-equal token
            continue;                                                 // move to next iteration
        }
        
        if (c == '>' && peek(tokenizer, 1) == '=') {
            advance(tokenizer);                                          // consume '>'
            advance(tokenizer);                                          // consume '='
            add_token(tokenizer, TOKEN_GREATER_EQUAL, ">=", line, col);  // emit greater-or-equal token
            continue;                                                    // move to next iteration
        }
        
        switch (c) {
            case '+': advance(tokenizer); add_token(tokenizer, TOKEN_PLUS, "+", line, col); continue;      // plus operator
            case '-': advance(tokenizer); add_token(tokenizer, TOKEN_MINUS, "-", line, col); continue;     // minus operator
            case '*': advance(tokenizer); add_token(tokenizer, TOKEN_STAR, "*", line, col); continue;      // multiplication operator
            case '/': advance(tokenizer); add_token(tokenizer, TOKEN_SLASH, "/", line, col); continue;     // division operator
            case '%': advance(tokenizer); add_token(tokenizer, TOKEN_PERCENT, "%", line, col); continue;   // modulo operator
            case '=': advance(tokenizer); add_token(tokenizer, TOKEN_EQUAL, "=", line, col); continue;     // assignment operator
            case '<': advance(tokenizer); add_token(tokenizer, TOKEN_LESS, "<", line, col); continue;      // less-than operator
            case '>': advance(tokenizer); add_token(tokenizer, TOKEN_GREATER, ">", line, col); continue;   // greater-than operator
            case '(':
                advance(tokenizer);                                        // consume '('
                tokenizer->paren_depth++;                                  // track nesting depth for indent handling
                add_token(tokenizer, TOKEN_LPAREN, "(", line, col);        // emit left paren token
                continue;
            case ')':
                advance(tokenizer);                                        // consume ')'
                if (tokenizer->paren_depth > 0) tokenizer->paren_depth--;  // decrement nesting depth, never below zero
                add_token(tokenizer, TOKEN_RPAREN, ")", line, col);        // emit right paren token
                continue;
            case '[': 
                advance(tokenizer);                                        // consume '['
                tokenizer->paren_depth++;                                  // brackets also suppress indent/dedent
                add_token(tokenizer, TOKEN_LBRACKET, "[", line, col);      // emit left bracket token
                continue;
            case ']': 
                advance(tokenizer);                                        // consume ']'
                if (tokenizer->paren_depth > 0) tokenizer->paren_depth--;  // decrement nesting depth
                add_token(tokenizer, TOKEN_RBRACKET, "]", line, col);      // emit right bracket token
                continue;
            case ',': advance(tokenizer); add_token(tokenizer, TOKEN_COMMA, ",", line, col); continue;  // comma separator
            case '.':
                advance(tokenizer);                                        // consume '.'
                add_token(tokenizer, TOKEN_DOT, ".", line, col);           // emit dot token (field access)
                continue;
        }
        
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Unexpected character: '%c'", c);            // format error message with the unexpected char
        tokenizer_error(tokenizer, 1, error_msg);  // report unexpected character error
        advance(tokenizer);                        // consume the problematic character to avoid infinite loop
    }
    
    while (tokenizer->indent_depth > 1) {
        add_token(tokenizer, TOKEN_DEDENT, " ", tokenizer->line, tokenizer->column);        // emit remaining dedent tokens at end of file
        tokenizer->indent_depth--;                 // pop indentation level
    }

    if (tokenizer->token_count > 0) {
        Token* last = &tokenizer->tokens[tokenizer->token_count - 1];                       // get last emitted token
        if (last->type != TOKEN_NEWLINE && last->type != TOKEN_EOF) {
            add_token(tokenizer, TOKEN_NEWLINE, "\n", tokenizer->line, tokenizer->column);  // ensure file ends with newline token
        }
    } else if (tokenizer->token_count == 0) {
        add_token(tokenizer, TOKEN_NEWLINE, "\n", tokenizer->line, tokenizer->column);      // empty file gets a newline token
    }
    
    add_token(tokenizer, TOKEN_EOF, "", tokenizer->line, tokenizer->column);                // emit end-of-file sentinel token
    
    if (out_count) {
        *out_count = tokenizer->token_count;                                                // return token count to caller
    }
    
    return tokenizer->tokens;                                                               // return token array
}
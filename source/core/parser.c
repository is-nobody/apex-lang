#include "parser.h"
#include "execute.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ========== Forward Declarations ==========
static ASTNode* parse_program(Parser* parser);
static ASTNode* parse_statement(Parser* parser);
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_block(Parser* parser, bool expect_newlines);
static ASTNode* parse_string_expression(Parser* parser, const char* expr_str, int line, int column);

// ========== Symbol Management ==========

void parser_enter_scope(Parser* parser) {
    parser->symbols.current_scope++;
}

void parser_exit_scope(Parser* parser) {
    // Remove all symbols at the current scope level
    int i = 0;
    while (i < parser->symbols.count) {
        if (parser->symbols.scope_levels[i] == parser->symbols.current_scope) {
            // Remove symbol
            free(parser->symbols.names[i]);
            for (int j = i; j < parser->symbols.count - 1; j++) {
                parser->symbols.names[j] = parser->symbols.names[j + 1];
                parser->symbols.scope_levels[j] = parser->symbols.scope_levels[j + 1];
            }
            parser->symbols.count--;
        } else {
            i++;
        }
    }
    parser->symbols.current_scope--;
}

void parser_add_symbol(Parser* parser, const char* name) {
    // Check if the symbol already exists in the current scope
    for (int i = 0; i < parser->symbols.count; i++) {
        if (parser->symbols.scope_levels[i] == parser->symbols.current_scope &&
            strcmp(parser->symbols.names[i], name) == 0) {
            return; // already declared
        }
    }
    
    // Grow arrays if needed
    if (parser->symbols.count >= parser->symbols.capacity) {
        parser->symbols.capacity = parser->symbols.capacity == 0 ? 16 : parser->symbols.capacity * 2;
        parser->symbols.names = (char**)realloc(parser->symbols.names, 
                                                  sizeof(char*) * parser->symbols.capacity);
        parser->symbols.scope_levels = (int*)realloc(parser->symbols.scope_levels,
                                                       sizeof(int) * parser->symbols.capacity);
    }
    
    parser->symbols.names[parser->symbols.count] = strdup(name);
    parser->symbols.scope_levels[parser->symbols.count] = parser->symbols.current_scope;
    parser->symbols.count++;
}

bool parser_is_declared(Parser* parser, const char* name) {
    // Search across all scopes
    for (int i = 0; i < parser->symbols.count; i++) {
        if (strcmp(parser->symbols.names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

// ========== Parser Utilities ==========

Parser* parser_create(Token* tokens, int count, const char* filename, const char* source) {
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    parser->tokens = tokens;
    parser->count = count;
    parser->current = 0;
    parser->filename = strdup(filename);
    
    // Initialize symbol table
    parser->symbols.names = NULL;
    parser->symbols.scope_levels = NULL;
    parser->symbols.count = 0;
    parser->symbols.capacity = 0;
    parser->symbols.current_scope = 0;
    parser->source = source;

    return parser;
}

void parser_destroy(Parser* parser) {
    if (parser) {
        free(parser->filename);
        
        // Free symbol table
        for (int i = 0; i < parser->symbols.count; i++) {
            free(parser->symbols.names[i]);
        }
        free(parser->symbols.names);
        free(parser->symbols.scope_levels);
        
        free(parser);
    }
}

static Token* peek(Parser* parser, int offset) {
    int idx = parser->current + offset;
    if (idx >= parser->count) return &parser->tokens[parser->count - 1];
    return &parser->tokens[idx];
}

static Token* current_token(Parser* parser) {
    return peek(parser, 0);
}

static Token* advance(Parser* parser) {
    if (parser->current >= parser->count) return &parser->tokens[parser->count - 1];
    return &parser->tokens[parser->current++];
}

static bool check(Parser* parser, TokenType type) {
    if (current_token(parser)->type == TOKEN_EOF) return false;
    return current_token(parser)->type == type;
}

static bool check_next(Parser* parser, TokenType type) {
    if (peek(parser, 1)->type == TOKEN_EOF) return false;
    return peek(parser, 1)->type == type;
}

static bool match(Parser* parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}

static Token* consume(Parser* parser, TokenType type, const char* message) {
    if (check(parser, type)) {
        return advance(parser);
    }
    parser_error(parser, message);
    return NULL;
}

static void skip_newlines(Parser* parser) {
    while (check(parser, TOKEN_NEWLINE)) {
        advance(parser);
    }
}

void parser_error(Parser* parser, const char* message) {
    Token* token = current_token(parser);
    int len = token->value ? (int)strlen(token->value) : 1;
    
    print_error_with_context(parser->filename, parser->source,
                             token->line, token->column, len,
                             "ParseError", message);
    throw_repl_error();
}
// ========== Expression Parsing (Pratt Parser) ==========

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,   // =
    PREC_OR,           // or
    PREC_AND,          // and
    PREC_EQUALITY,     // == !=
    PREC_COMPARISON,   // < > <= >=
    PREC_TERM,         // + -
    PREC_FACTOR,       // * / %
    PREC_UNARY,        // - not
    PREC_CALL,         // . () []
    PREC_PRIMARY
} Precedence;

static Precedence get_precedence(TokenType type) {
    switch (type) {
        case TOKEN_EQUAL: return PREC_ASSIGNMENT;
        case TOKEN_OR: return PREC_OR;
        case TOKEN_AND: return PREC_AND;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL: return PREC_EQUALITY;
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL: return PREC_COMPARISON;
        case TOKEN_PLUS:
        case TOKEN_MINUS: return PREC_TERM;
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT: return PREC_FACTOR;
        default: return PREC_NONE;
    }
}

// Forward declarations for Pratt parser
static ASTNode* parse_prefix(Parser* parser);
static ASTNode* parse_infix(Parser* parser, ASTNode* left);
static ASTNode* parse_precedence(Parser* parser, Precedence precedence);

// ========== Primary Expressions ==========

static ASTNode* parse_number(Parser* parser) {
    Token* token = advance(parser);
    double value = atof(token->value);
    return ast_create_literal_number(value, token->line, token->column);
}

static ASTNode* parse_string_expression(Parser* parser, const char* expr_str, int line, int column) {
    // Create a temporary tokenizer for the expression inside {}
    Tokenizer* temp_tokenizer = tokenizer_create(expr_str, "<interpolation>");
    int temp_count;
    Token* temp_tokens = tokenizer_tokenize(temp_tokenizer, &temp_count);
    
    // Create a temporary parser
    Parser* temp_parser = parser_create(temp_tokens, temp_count, "<interpolation>", expr_str);
    ASTNode* expr = parse_expression(temp_parser);
    
    // Cleanup
    parser_destroy(temp_parser);
    tokenizer_destroy(temp_tokenizer);
    
    return expr;
}

static ASTNode* parse_string(Parser* parser) {
    Token* token = advance(parser);
    const char* value = token->value;
    
    // Check for interpolation
    if (!strchr(value, '{')) {
        return ast_create_literal_string(value, token->line, token->column);
    }
    
    // String interpolation
    ASTNodeList* parts = ast_list_create();
    const char* p = value;
    const char* start = value;
    
    while (*p) {
        if (*p == '{') {
            // Add preceding text
            if (p > start) {
                int len = p - start;
                char* lit = (char*)malloc(len + 1);
                strncpy(lit, start, len);
                lit[len] = '\0';
                
                if (len > 0) {
                    ASTNode* str_node = ast_create_literal_string(lit, token->line, token->column);
                    ast_list_add(parts, str_node);
                }
                free(lit);
            }
            
            // Find closing brace
            const char* expr_start = p + 1;
            const char* expr_end = strchr(expr_start, '}');
            
            if (expr_end) {
                int expr_len = expr_end - expr_start;
                char* expr_str = (char*)malloc(expr_len + 1);
                strncpy(expr_str, expr_start, expr_len);
                expr_str[expr_len] = '\0';
                
                // Parse the expression inside {}
                ASTNode* expr_node = parse_string_expression(parser, expr_str, 
                                                              token->line, 
                                                              token->column + (expr_start - value));
                if (expr_node) {
                    ast_list_add(parts, expr_node);
                }
                free(expr_str);
                
                p = expr_end + 1;
                start = p;
            } else {
                p++;
            }
        } else {
            p++;
        }
    }
    
    // Add remaining text
    if (*start) {
        ASTNode* str_node = ast_create_literal_string(start, token->line, token->column);
        ast_list_add(parts, str_node);
    }
    
    return ast_create_string_interp(parts);
}

static ASTNode* parse_bool(Parser* parser) {
    Token* token = advance(parser);
    return ast_create_literal_bool(
        token->type == TOKEN_TRUE, token->line, token->column);
}

static ASTNode* parse_identifier(Parser* parser) {
    Token* token = advance(parser);
    return ast_create_identifier(token->value, token->line, token->column);
}

static ASTNode* parse_table_literal(Parser* parser) {
    advance(parser); // consume '('
    ASTNodeList* items = ast_list_create();
    ASTNodeList* key_values = ast_list_create();
    
    bool has_key_values = false;
    
    if (!check(parser, TOKEN_RPAREN)) {
        while (true) {
            // Check for key = value pattern
            if (check(parser, TOKEN_IDENTIFIER) && check_next(parser, TOKEN_EQUAL)) {
                has_key_values = true;
                Token* key = advance(parser);
                advance(parser); // consume '='
                ASTNode* value = parse_expression(parser);
                
                // Create a key-value node (stored as binary with key on left)
                ASTNode* kv_node = ast_create_binary(
                    TOKEN_EQUAL,
                    ast_create_identifier(key->value, key->line, key->column),
                    value
                );
                ast_list_add(key_values, kv_node);
            } else {
                if (has_key_values) {
                    parser_error(parser, "Cannot mix ordered items after key-value pairs");
                }
                ASTNode* item = parse_expression(parser);
                ast_list_add(items, item);
            }
            
            if (!match(parser, TOKEN_COMMA)) break;
            skip_newlines(parser);
        }
    }
    
    consume(parser, TOKEN_RPAREN, "Expected ')' after table literal");
    return ast_create_table_literal(items, key_values);
}

static ASTNode* parse_group_or_table(Parser* parser) {
    // Determine if this is a table literal by looking for commas or key=value
    bool is_table = false;
    int lookahead = 0;
    
    if (check(parser, TOKEN_LPAREN)) {
        int paren_count = 1;
        lookahead = 1;
        
        while (paren_count > 0 && peek(parser, lookahead)->type != TOKEN_EOF) {
            TokenType t = peek(parser, lookahead)->type;
            if (t == TOKEN_LPAREN) paren_count++;
            else if (t == TOKEN_RPAREN) paren_count--;
            else if (t == TOKEN_COMMA && paren_count == 1) {
                is_table = true;
                break;
            } else if (t == TOKEN_EQUAL && paren_count == 1) {
                is_table = true;
                break;
            }
            lookahead++;
        }
    }
    
    if (is_table) {
        return parse_table_literal(parser);
    }
    
    // Regular grouped expression
    advance(parser); // consume '('
    ASTNode* expr = parse_expression(parser);
    consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
    return expr;
}

// ========== Prefix Parsers ==========

static ASTNode* parse_prefix(Parser* parser) {
    Token* token = current_token(parser);
    
    switch (token->type) {
        case TOKEN_NUMBER:
            return parse_number(parser);
        case TOKEN_STRING:
            return parse_string(parser);
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parse_bool(parser);
        case TOKEN_IDENTIFIER:
            return parse_identifier(parser);
        case TOKEN_LPAREN:
            return parse_group_or_table(parser);
        case TOKEN_MINUS: {
            advance(parser);
            ASTNode* operand = parse_precedence(parser, PREC_UNARY);
            if (!operand) {
                parser_error(parser, "Expected expression after unary minus");
                return NULL;
            }
            return ast_create_unary(TOKEN_MINUS, operand);
        }
        case TOKEN_NOT: {
            advance(parser);
            ASTNode* operand = parse_precedence(parser, PREC_UNARY);
            if (!operand) {
                parser_error(parser, "Expected expression after 'not'");
                return NULL;
            }
            return ast_create_unary(TOKEN_NOT, operand);
        }
        case TOKEN_EOF:
        case TOKEN_NEWLINE:
            return NULL;
        default:
            parser_error(parser, "Expected expression");
            return NULL;
    }
}

// ========== Infix Parsers ==========

static ASTNode* parse_call(Parser* parser, ASTNode* callee) {
    advance(parser); // consume '('
    ASTNodeList* arguments = ast_list_create();
    
    if (!check(parser, TOKEN_RPAREN)) {
        while (true) {
            ast_list_add(arguments, parse_expression(parser));
            if (!match(parser, TOKEN_COMMA)) break;
            skip_newlines(parser);
        }
    }
    
    consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
    return ast_create_call(callee, arguments);
}

static ASTNode* parse_member_access(Parser* parser, ASTNode* object) {
    advance(parser); // consume '.'
    
    Token* token = current_token(parser);
    ASTNode* member_node;
    
    // After dot, allow: identifiers, numbers, and KEYWORDS as field names
    if (token->type == TOKEN_IDENTIFIER || 
        token->type == TOKEN_NUMBER ||
        token->type == TOKEN_FUNCTION ||
        token->type == TOKEN_IF ||
        token->type == TOKEN_ELIF ||
        token->type == TOKEN_ELSE ||
        token->type == TOKEN_WHILE ||
        token->type == TOKEN_FOR ||
        token->type == TOKEN_BREAK ||
        token->type == TOKEN_CONTINUE ||
        token->type == TOKEN_RETURN ||
        token->type == TOKEN_IMPORT ||
        token->type == TOKEN_AND ||
        token->type == TOKEN_OR ||
        token->type == TOKEN_NOT ||
        token->type == TOKEN_TRUE ||
        token->type == TOKEN_FALSE) {
        
        advance(parser);
        
        if (token->type == TOKEN_NUMBER) {
            member_node = ast_create_literal_string(token->value, token->line, token->column);
        } else {
            member_node = ast_create_identifier(token->value, token->line, token->column);
        }
        
        return ast_create_member_access(object, member_node);
    }
    
    parser_error(parser, "Expected identifier after '.'");
    return NULL;
}

static ASTNode* parse_infix(Parser* parser, ASTNode* left) {
    Token* token = current_token(parser);
    
    switch (token->type) {
        case TOKEN_LPAREN:
            return parse_call(parser, left);
        case TOKEN_DOT:
            return parse_member_access(parser, left);
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_AND:
        case TOKEN_OR: {
            advance(parser);
            Precedence prec = get_precedence(token->type);
            ASTNode* right = parse_precedence(parser, prec);
            return ast_create_binary(token->type, left, right);
        }
        case TOKEN_EQUAL: {
            // Assignment
            advance(parser);
            ASTNode* right = parse_precedence(parser, PREC_ASSIGNMENT - 1);
            
            // Extract the variable name from the left side
            if (left->type == AST_IDENTIFIER) {
                return ast_create_var_assign(
                    left->identifier.name, right, false, NULL,
                    token->line, token->column);
            } else if (left->type == AST_MEMBER_ACCESS) {
                // Compound assignment like x.field = value
                char* name = NULL;
                if (left->access.object->type == AST_IDENTIFIER) {
                    name = left->access.object->identifier.name;
                }
                return ast_create_var_assign(
                    name, right, false, left,
                    token->line, token->column);
            }
            parser_error(parser, "Invalid assignment target");
            return NULL;
        }
        default:
            return NULL;
    }
}

// ========== Pratt Parser Core ==========

static ASTNode* parse_precedence(Parser* parser, Precedence precedence) {
    ASTNode* left = parse_prefix(parser);
    
    while (true) {
        Token* token = current_token(parser);
        Precedence current_prec = get_precedence(token->type);
        
        // Also check for call and member access (higher precedence)
        if (token->type == TOKEN_LPAREN || token->type == TOKEN_DOT) {
            current_prec = PREC_CALL;
        }
        
        if (current_prec <= precedence) break;
        
        left = parse_infix(parser, left);
    }
    
    return left;
}

static ASTNode* parse_expression(Parser* parser) {
    skip_newlines(parser);
    
    if (check(parser, TOKEN_EOF)) {
        return NULL;
    }
    
    return parse_precedence(parser, PREC_ASSIGNMENT);
}

// ========== Statement Parsing ==========

static ASTNode* parse_var_decl_or_assign(Parser* parser) {
    Token* name = current_token(parser);
    advance(parser); // consume identifier
    
    consume(parser, TOKEN_EQUAL, "Expected '=' in assignment");
    ASTNode* value = parse_expression(parser);
    
    // Check if variable already exists
    bool is_declaration = !parser_is_declared(parser, name->value);

    if (is_declaration) {
        parser_add_symbol(parser, name->value);
    }
    
    return ast_create_var_assign(name->value, value, is_declaration, NULL,
                                  name->line, name->column);
}

static ASTNode* parse_function(Parser* parser) {
    advance(parser); // consume 'function'
    Token* name = consume(parser, TOKEN_IDENTIFIER, "Expected function name");
    
    parser_add_symbol(parser, name->value);
    
    consume(parser, TOKEN_LPAREN, "Expected '(' after function name");
    
    parser_enter_scope(parser);
    
    ASTNodeList* params = ast_list_create();
    
    if (!check(parser, TOKEN_RPAREN)) {
        while (true) {
            Token* param_name = consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
            parser_add_symbol(parser, param_name->value);
            
            ASTNode* param = ast_create_param(param_name->value, param_name->line, param_name->column);
            ast_list_add(params, param);
            
            if (!match(parser, TOKEN_COMMA)) break;
        }
    }
    
    consume(parser, TOKEN_RPAREN, "Expected ')' after parameters");
    
    ASTNode* body = parse_block(parser, true);
    
    parser_exit_scope(parser);
    
    return ast_create_function(name->value, params, body, name->line, name->column);
}

static ASTNode* parse_if_statement(Parser* parser) {
    advance(parser); // consume 'if'
    ASTNode* condition = parse_expression(parser);
    
    ASTNode* then_branch = parse_block(parser, true);
    
    ASTNode* elif_chain = NULL;
    ASTNode* else_branch = NULL;
    
    skip_newlines(parser);
    
    while (check(parser, TOKEN_ELIF)) {
        Token* elif_kw = advance(parser);
        ASTNode* elif_cond = parse_expression(parser);
        ASTNode* elif_body = parse_block(parser, true);
        
        ASTNode* elif_node = (ASTNode*)calloc(1, sizeof(ASTNode));
        elif_node->type = AST_IF_STMT;
        elif_node->line = elif_kw->line;
        elif_node->column = elif_kw->column;
        elif_node->if_stmt.condition = elif_cond;
        elif_node->if_stmt.then_branch = elif_body;
        elif_node->if_stmt.elif_chain = NULL;
        elif_node->if_stmt.else_branch = NULL;
        
        if (!elif_chain) {
            elif_chain = elif_node;
        } else {
            ASTNode* last = elif_chain;
            while (last->if_stmt.elif_chain) {
                last = last->if_stmt.elif_chain;
            }
            last->if_stmt.elif_chain = elif_node;
        }
        
        skip_newlines(parser);
    }
    
    if (match(parser, TOKEN_ELSE)) {
        else_branch = parse_block(parser, true);
        if (elif_chain) {
            ASTNode* last = elif_chain;
            while (last->if_stmt.elif_chain) {
                last = last->if_stmt.elif_chain;
            }
            last->if_stmt.else_branch = else_branch;
        }
    }
    
    return ast_create_if(condition, then_branch, elif_chain, elif_chain ? NULL : else_branch);
}

static ASTNode* parse_while_statement(Parser* parser) {
    advance(parser); // consume 'while'
    ASTNode* condition = parse_expression(parser);
    ASTNode* body = parse_block(parser, true);
    return ast_create_while(condition, body);
}

static ASTNode* parse_for_statement(Parser* parser) {
    Token* for_kw = advance(parser);
    Token* var_name = consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
    consume(parser, TOKEN_EQUAL, "Expected '=' after for variable");
    
    ASTNode* start = parse_expression(parser);
    consume(parser, TOKEN_COMMA, "Expected ',' after start value");
    ASTNode* end = parse_expression(parser);
    
    ASTNode* step = NULL;
    if (match(parser, TOKEN_COMMA)) {
        step = parse_expression(parser);
    }
    
    ASTNode* body = parse_block(parser, true);
    return ast_create_for(var_name->value, start, end, step, body, for_kw->line, for_kw->column);
}

static bool is_valid_import_segment(TokenType type) {
    if (type == TOKEN_IDENTIFIER) return true;
    if (type >= TOKEN_FUNCTION && type <= TOKEN_FALSE) return true;
    if (type == TOKEN_NUMBER || type == TOKEN_STRING) return true;
    return false;
}

static ASTNode* parse_import_statement(Parser* parser) {
    Token* import_kw = advance(parser);
    
    char* module_path = (char*)malloc(128);
    int path_len = 0, path_cap = 128;
    module_path[0] = '\0';
    
    #define APPEND_PATH(s) do { \
        int slen = (int)strlen(s); \
        if (path_len + slen + 2 >= path_cap) { \
            path_cap = (path_len + slen + 2) * 2; \
            module_path = (char*)realloc(module_path, path_cap); \
        } \
        strcat(module_path, s); \
        path_len += slen; \
    } while(0)
    
    Token* first = current_token(parser);
    if (!is_valid_import_segment(first->type)) {
        free(module_path);
        parser_error(parser, "Expected module name");
    }
    APPEND_PATH(first->value);
    advance(parser);
    
    while (match(parser, TOKEN_DOT)) {
        APPEND_PATH(".");
        Token* next = current_token(parser);
        
        if (is_valid_import_segment(next->type)) {
            APPEND_PATH(next->value);
            advance(parser);
        } else {
            free(module_path);
            parser_error(parser, "Expected identifier after '.'");
        }
    }
    
    ASTNode* node = ast_create_import(module_path, import_kw->line, import_kw->column);
    free(module_path);
    #undef APPEND_PATH
    return node;
}

static ASTNode* parse_return_statement(Parser* parser) {
    Token* return_kw = advance(parser); // consume 'return'
    
    ASTNode* value = NULL;
    // Check if there's an expression on the same line
    if (!check(parser, TOKEN_NEWLINE) && !check(parser, TOKEN_EOF)) {
        value = parse_expression(parser);
    }
    
    return ast_create_return(value, return_kw->line, return_kw->column);
}

static ASTNode* parse_break_statement(Parser* parser) {
    Token* break_kw = advance(parser);
    return ast_create_node(AST_BREAK_STMT, break_kw->line, break_kw->column);
}

static ASTNode* parse_continue_statement(Parser* parser) {
    Token* cont_kw = advance(parser);
    return ast_create_node(AST_CONTINUE_STMT, cont_kw->line, cont_kw->column);
}

static ASTNode* parse_statement(Parser* parser) {
    skip_newlines(parser);
    
    if (check(parser, TOKEN_EOF)) {
        return NULL;
    }
    
    Token* token = current_token(parser);
    
    switch (token->type) {
        case TOKEN_EOF:
            return NULL;
            
        case TOKEN_NEWLINE:
            advance(parser);
            return NULL;
            
        case TOKEN_IMPORT:
            return parse_import_statement(parser);
            
        case TOKEN_FUNCTION:
            return parse_function(parser);
            
        case TOKEN_IF:
            return parse_if_statement(parser);
            
        case TOKEN_WHILE:
            return parse_while_statement(parser);
            
        case TOKEN_FOR:
            return parse_for_statement(parser);
            
        case TOKEN_RETURN:
            return parse_return_statement(parser);
            
        case TOKEN_BREAK:
            return parse_break_statement(parser);
            
        case TOKEN_CONTINUE:
            return parse_continue_statement(parser);
            
        case TOKEN_IDENTIFIER: {
            // Check if this is an assignment
            int lookahead = 1;
            bool is_assignment = false;
            
            // Skip dot access chains (module.function)
            while (peek(parser, lookahead)->type == TOKEN_DOT) {
                lookahead += 2;
                if (peek(parser, lookahead - 1)->type == TOKEN_EOF) break;
            }
            
            TokenType next = peek(parser, lookahead)->type;
            if (next == TOKEN_EQUAL) {
                is_assignment = true;
            }
            
            if (is_assignment) {
                ASTNode* node = parse_var_decl_or_assign(parser);
                if (node) return node;
            }
            
            // Parse as expression (function call or module access)
            ASTNode* expr = parse_expression(parser);
            if (expr) {
                return ast_create_expr_stmt(expr);
            }
            return NULL;
        }
        
        default: {
            ASTNode* expr = parse_expression(parser);
            if (expr) {
                return ast_create_expr_stmt(expr);
            }
            advance(parser);
            return NULL;
        }
    }
}

static ASTNode* parse_block(Parser* parser, bool expect_newlines) {
    ASTNodeList* statements = ast_list_create();
    
    // Expect INDENT after function/if/while/for declarations
    if (expect_newlines) {
        skip_newlines(parser);
        
        if (!match(parser, TOKEN_INDENT)) {
            // No INDENT — parse a single statement
            ASTNode* stmt = parse_statement(parser);
            if (stmt) {
                ast_list_add(statements, stmt);
            }
            return ast_create_block(statements);
        }
    }
    
    // Parse statements until DEDENT or EOF
    while (!check(parser, TOKEN_EOF) && !check(parser, TOKEN_DEDENT)) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            ast_list_add(statements, stmt);
        }
        skip_newlines(parser);
    }
    
    // Consume DEDENT
    match(parser, TOKEN_DEDENT);
    
    return ast_create_block(statements);
}

static ASTNode* parse_program(Parser* parser) {
    ASTNodeList* statements = ast_list_create();
    
    int prev_pos = -1;
    
    while (!check(parser, TOKEN_EOF)) {
        // Infinite loop guard
        if (parser->current == prev_pos) {
            break;
        }
        prev_pos = parser->current;
        
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            ast_list_add(statements, stmt);
        }
        
        // Consume NEWLINE
        skip_newlines(parser);
    }
    
    ASTNode* program = ast_create_block(statements);
    program->type = AST_PROGRAM;
    return program;
}


// ========== Main Parse Function ==========

ASTNode* parser_parse(Parser* parser) {
    return parse_program(parser);
}
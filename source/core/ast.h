#ifndef AST_H
#define AST_H

#include "tokenizer.h"
#include <stdbool.h>

// ========== AST Node Types ==========
typedef enum {
    // Statements
    AST_PROGRAM,           // root node — top-level program
    AST_IMPORT_STMT,       // import "module"
    AST_VAR_DECL,          // x = 5 (new variable declaration)
    AST_ASSIGN,            // x = 5 (existing variable) or x.field = 5
    AST_FUNCTION_DECL,     // function name(params) { body }
    AST_RETURN_STMT,       // return value
    AST_IF_STMT,           // if / elif / else chain
    AST_WHILE_STMT,        // while (condition) { body }
    AST_FOR_STMT,          // for var in iterable { body }
    AST_BREAK_STMT,        // break
    AST_CONTINUE_STMT,     // continue
    AST_EXPR_STMT,         // standalone expression used as a statement
    
    // Expressions
    AST_BINARY,            // a + b, a == b, etc.
    AST_UNARY,             // -a, not a
    AST_LITERAL_NUMBER,    // 42, 3.14
    AST_LITERAL_STRING,    // "hello"
    AST_LITERAL_BOOL,      // true, false
    AST_IDENTIFIER,        // variable name reference
    AST_CALL,              // function(args)
    AST_INDEX_ACCESS,      // table[index] (bracket notation)
    AST_MEMBER_ACCESS,     // table.key (dot notation)
    AST_TABLE_LITERAL,     // (1, 2, 3) or (key = value, ...)
    AST_STRING_INTERP,     // "Hello {name}"
    
    AST_MODULE_BLOCK,

    // Internal
    AST_BLOCK,             // { statement; statement; ... }
    AST_PARAM,             // function parameter with optional type annotation
} ASTNodeType;

// ========== Forward Declarations ==========
typedef struct ASTNode ASTNode;
typedef struct ASTNodeList ASTNodeList;

// ========== AST Node ==========
struct ASTNode {
    ASTNodeType type;
    int line;
    int column;
    
    union {
        // String literal value
        struct {
            char* string_value;
        } literal_string;
        
        // Numeric literal value
        struct {
            double number_value;
        } literal_number;
        
        // Boolean literal value
        struct {
            bool bool_value;
        } literal_bool;
        
        // Variable name reference
        struct {
            char* name;
        } identifier;
        
        // Binary operation (arithmetic, comparison, logical)
        struct {
            TokenType op;
            ASTNode* left;
            ASTNode* right;
        } binary;
        
        // Unary operation (negation, logical not)
        struct {
            TokenType op;
            ASTNode* operand;
        } unary;
        
        // Function or method call
        struct {
            ASTNode* callee;
            ASTNodeList* arguments;
        } call;
        
        // Table member/index access (shared by both access types)
        struct {
            ASTNode* object;
            ASTNode* member;     // identifier for dot, expression for bracket
        } access;
        
        // Table literal constructor
        struct {
            ASTNodeList* items;       // positional items (array-like)
            ASTNodeList* key_values;  // key = value pairs (dict-like)
        } table_literal;
        
        // Variable assignment or declaration
        struct {
            char* name;
            ASTNode* value;
            bool is_declaration;      // true = new variable, false = reassign
            ASTNode* access_path;     // non-NULL for compound assignments (x.field = val)
        } var_assign;
        
        // Function definition
        struct {
            char* name;
            ASTNodeList* params;
            ASTNode* body;            // block node
        } function_decl;
        
        // If/elif/else conditional
        struct {
            ASTNode* condition;
            ASTNode* then_branch;
            ASTNode* elif_chain;      // linked list of elif branches
            ASTNode* else_branch;
        } if_stmt;
        
        // Single elif branch (linked into if_stmt)
        struct {
            ASTNode* condition;
            ASTNode* body;
            ASTNode* next_elif;
        } elif_branch;
        
        // While loop
        struct {
            ASTNode* condition;
            ASTNode* body;
        } while_stmt;
        
        // For-in loop
        struct {
            char* var_name;
            ASTNode* start;
            ASTNode* end;
            ASTNode* step;
            ASTNode* body;
        } for_stmt;
        
        // Module import
        struct {
            char* module_path;
        } import_stmt;
        
        struct {
            char* module_name;
            ASTNode* body;
        } module_block;

        // Function return
        struct {
            ASTNode* value;           // NULL if bare return (no value)
        } return_stmt;
        
        // String interpolation ("Hello {name}")
        struct {
            ASTNodeList* parts;       // alternating string literals and expressions
        } string_interp;
        
        // Block of statements
        struct {
            ASTNodeList* statements;
        } block;
        
        // Function parameter definition
        struct {
            char* name;
        } param;
        
        // Wraps an expression as a statement
        struct {
            ASTNode* expression;
        } expr_stmt;
    };
};

// ========== Dynamic Array for Node Lists ==========
struct ASTNodeList {
    ASTNode** nodes;
    int count;
    int capacity;
};

// ========== Node Creation ==========
ASTNode* ast_create_node(ASTNodeType type, int line, int column);
ASTNode* ast_create_literal_number(double value, int line, int column);
ASTNode* ast_create_literal_string(const char* value, int line, int column);
ASTNode* ast_create_literal_bool(bool value, int line, int column);
ASTNode* ast_create_identifier(const char* name, int line, int column);
ASTNode* ast_create_binary(TokenType op, ASTNode* left, ASTNode* right);
ASTNode* ast_create_unary(TokenType op, ASTNode* operand);
ASTNode* ast_create_call(ASTNode* callee, ASTNodeList* arguments);
ASTNode* ast_create_member_access(ASTNode* object, ASTNode* member);
ASTNode* ast_create_table_literal(ASTNodeList* items, ASTNodeList* key_values);
ASTNode* ast_create_var_assign(const char* name, ASTNode* value, bool is_decl, 
                                ASTNode* access_path, int line, int column);
ASTNode* ast_create_function(const char* name, ASTNodeList* params, ASTNode* body, 
                              int line, int column);
ASTNode* ast_create_if(ASTNode* condition, ASTNode* then_branch, 
                       ASTNode* elif_chain, ASTNode* else_branch);
ASTNode* ast_create_while(ASTNode* condition, ASTNode* body);
ASTNode* ast_create_for(const char* var_name, ASTNode* start, ASTNode* end,
                        ASTNode* step, ASTNode* body, int line, int column);
ASTNode* ast_create_try(ASTNode* try_body, ASTNode* failure_body, ASTNode* always_body);
ASTNode* ast_create_import(const char* module_path, int line, int column);
ASTNode* ast_create_return(ASTNode* value, int line, int column);
ASTNode* ast_create_string_interp(ASTNodeList* parts);
ASTNode* ast_create_type_check(const char* param_name, const char* type_name, 
                                int line, int column);
ASTNode* ast_create_block(ASTNodeList* statements);
ASTNode* ast_create_expr_stmt(ASTNode* expression);
ASTNode* ast_create_param(const char* name, int line, int column);
ASTNode* ast_create_module_block(const char* name, ASTNode* body, int line, int column);

// ========== List Operations ==========
ASTNodeList* ast_list_create();
void ast_list_add(ASTNodeList* list, ASTNode* node);
void ast_list_free(ASTNodeList* list);

// ========== Free AST ==========
void ast_free_node(ASTNode* node);
void ast_free(ASTNode* program);

// ========== Debug ==========
void ast_print(ASTNode* node, int indent);

#endif // AST_H
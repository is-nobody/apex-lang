#ifndef AST_H
#define AST_H

#include "tokenizer.h"
#include <stdbool.h>

// all ast node kinds representing either statements or expressions
typedef enum {
    AST_PROGRAM,           // root node holding the whole module's top-level statements
    AST_IMPORT_STMT,       // import "module" — loads another module by path
    AST_VAR_DECL,          // x = 5 where x is being introduced as a new variable
    AST_ASSIGN,            // x = 5 for existing variable, or x.field = 5 for field update
    AST_FUNCTION_DECL,     // function name(params) { body } — defines a callable
    AST_RETURN_STMT,       // return value — exits the current function
    AST_IF_STMT,           // if condition { ... } with optional elif/else chains
    AST_FOR_STMT,          // for var in iterable { body } — loop construct
    AST_BREAK_STMT,        // break — exits the nearest enclosing loop
    AST_CONTINUE_STMT,     // continue — jumps to the next loop iteration
    AST_EXPR_STMT,         // expression used as a statement, e.g. a function call on its own
    
    AST_BINARY,            // a + b, a == b, etc. — combines two expressions with an operator
    AST_UNARY,             // -a, not a — applies a unary operator to a single expression
    AST_LITERAL_NUMBER,    // 42, 3.14 — numeric constant
    AST_LITERAL_STRING,    // "hello" — string constant
    AST_LITERAL_NONE,      // none literal — represents null/nil value
    AST_LITERAL_BOOL,      // true, false — boolean constant
    AST_IDENTIFIER,        // variable name reference, resolved in scope
    AST_CALL,              // function(args) — invokes a callee with arguments
    AST_INDEX_ACCESS,      // table[index] — bracket notation for array or table access
    AST_TABLE_LITERAL,     // (1, 2, 3) or (key = value, ...) — table constructor
    AST_STRING_INTERP,     // "Hello {name}" — interpolated string with embedded expressions
    AST_TERNARY,           // condition ? true_expr : false_expr — ternary conditional operator
    AST_MODULE_BLOCK,      // container for a separate module's top-level body

    AST_BLOCK,             // { statement; statement; ... } — groups multiple statements
    AST_PARAM,             // function parameter, holds its name for lookup
} ASTNodeType;

// forward declarations for recursive structures
typedef struct ASTNode ASTNode;
typedef struct ASTNodeList ASTNodeList;

// main ast node with a type discriminator, source location, and a type-specific payload
struct ASTNode {
    ASTNodeType type;
    int line;
    int column;
    bool in_interpolation;
    
    union {
        // string literal value, duplicated so the node owns its memory
        struct {
            char* string_value;
        } literal_string;
        
        // numeric literal value stored as a double for simplicity
        struct {
            double number_value;
        } literal_number;
        
        // boolean literal value, used directly in conditions
        struct {
            bool bool_value;
        } literal_bool;
        
        // identifier reference, duplicated string for ownership
        struct {
            char* name;
        } identifier;
        
        // binary operation with a token operator and two child expressions
        struct {
            TokenType op;
            ASTNode* left;
            ASTNode* right;
        } binary;
        
        // unary operation with an operator and a single operand
        struct {
            TokenType op;
            ASTNode* operand;
        } unary;
        
        // call node with a callee expression and a list of argument nodes
        struct {
            ASTNode* callee;
            ASTNodeList* arguments;
        } call;
        
        // index access storing the object and the index expression (bracket notation)
        struct {
            ASTNode* object;
            ASTNode* member;
        } access;
        
        // table literal with separate lists for positional items and key-value pairs
        struct {
            ASTNodeList* items;       // sequential array-like elements
            ASTNodeList* key_values;  // named field assignments
        } table_literal;
        
        // variable assignment or declaration, with optional access path for fields
        struct {
            char* name;
            ASTNode* value;
            bool is_declaration;      // distinguishes declaration from reassignment
            ASTNode* access_path;     // non-null for x.field = val to track the target
        } var_assign;
        
        // function declaration with name, parameters, and body block
        struct {
            char* name;
            ASTNodeList* params;
            ASTNode* body;
        } function_decl;
        
        // if statement with condition, then branch, optional elif chain, and else branch
        struct {
            ASTNode* condition;
            ASTNode* then_branch;
            ASTNode* elif_chain;      // linked list of elif branches
            ASTNode* else_branch;
        } if_stmt;
        
        // single elif branch linked into the elif chain of an if statement
        struct {
            ASTNode* condition;
            ASTNode* body;
            ASTNode* next_elif;
        } elif_branch;
        
        // for loop with optional variable, condition, range bounds, and step
        struct {
            char* var_name;      // null when not a numeric range loop
            ASTNode* condition;  // null for range or infinite loops
            ASTNode* start;
            ASTNode* end;
            ASTNode* step;
            ASTNode* body;
        } for_stmt;
        
        // import statement storing the module path as a duplicated string
        struct {
            char* module_path;
        } import_stmt;
        
        // module block with its own name and body for nested modules
        struct {
            char* module_name;
            ASTNode* body;
        } module_block;

        // return statement with an optional return value expression
        struct {
            ASTNode* value;           // null for bare return with no value
        } return_stmt;
        
        // string interpolation with a list of alternating string literals and expressions
        struct {
            ASTNodeList* parts;
        } string_interp;
        
        // block node holding a list of statements executed sequentially
        struct {
            ASTNodeList* statements;
        } block;
        
        // parameter definition holding the parameter name for function signatures
        struct {
            char* name;
        } param;
        
        // ternary expression
        struct {
            ASTNode* condition;
            ASTNode* true_expr;
            ASTNode* false_expr;
        } ternary;

        // expression statement wrapper to treat any expression as a statement
        struct {
            ASTNode* expression;
        } expr_stmt;
    };
};

// dynamic array used for lists of ast nodes (statements, arguments, etc.)
struct ASTNodeList {
    ASTNode** nodes;
    int count;
    int capacity;
};

// creation functions for each ast node type, all taking source location info
ASTNode* ast_create_node(ASTNodeType type, int line, int column);
ASTNode* ast_create_literal_number(double value, int line, int column);
ASTNode* ast_create_literal_string(const char* value, int line, int column);
ASTNode* ast_create_literal_none(int line, int column);
ASTNode* ast_create_literal_bool(bool value, int line, int column);
ASTNode* ast_create_identifier(const char* name, int line, int column);
ASTNode* ast_create_binary(TokenType op, ASTNode* left, ASTNode* right);
ASTNode* ast_create_unary(TokenType op, ASTNode* operand);
ASTNode* ast_create_call(ASTNode* callee, ASTNodeList* arguments);
ASTNode* ast_create_index_access(ASTNode* object, ASTNode* index);
ASTNode* ast_create_table_literal(ASTNodeList* items, ASTNodeList* key_values, int line, int column);
ASTNode* ast_create_var_assign(const char* name, ASTNode* value, bool is_decl, 
                                ASTNode* access_path, int line, int column);
ASTNode* ast_create_function(const char* name, ASTNodeList* params, ASTNode* body, 
                              int line, int column);
ASTNode* ast_create_if(ASTNode* condition, ASTNode* then_branch, 
                       ASTNode* elif_chain, ASTNode* else_branch);
ASTNode* ast_create_for(const char* var_name, ASTNode* condition, ASTNode* start, 
                        ASTNode* end, ASTNode* step, ASTNode* body, int line, int column);
ASTNode* ast_create_import(const char* module_path, int line, int column);
ASTNode* ast_create_return(ASTNode* value, int line, int column);
ASTNode* ast_create_string_interp(ASTNodeList* parts);
ASTNode* ast_create_ternary(ASTNode* condition, ASTNode* true_expr, ASTNode* false_expr, int line, int column);
ASTNode* ast_create_type_check(const char* param_name, const char* type_name, 
                                int line, int column);
ASTNode* ast_create_block(ASTNodeList* statements);
ASTNode* ast_create_expr_stmt(ASTNode* expression);
ASTNode* ast_create_param(const char* name, int line, int column);
ASTNode* ast_create_module_block(const char* name, ASTNode* body, int line, int column);

// list management functions for dynamic arrays of ast nodes
ASTNodeList* ast_list_create();
void ast_list_add(ASTNodeList* list, ASTNode* node);
void ast_list_free(ASTNodeList* list);

// memory deallocation for the entire ast tree
void ast_free_node(ASTNode* node);
void ast_free(ASTNode* program);

#endif // AST_H
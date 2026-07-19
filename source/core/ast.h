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

// forward declaration for self-referential AST nodes
typedef struct ASTNode ASTNode;

// forward declaration for dynamic node lists
typedef struct ASTNodeList ASTNodeList;

// main AST node with a type discriminator, source location, and a type-specific payload
struct ASTNode {
    ASTNodeType type;            // discriminator indicating which union member is active
    int line;                    // source line number for error reporting
    int column;                  // source column position for error reporting
    bool in_interpolation;       // whether this node appears inside a string interpolation
    
    union {
        // string literal value, duplicated so the node owns its memory
        struct {
            char* string_value;  // dynamically allocated string content
        } literal_string;
        
        // numeric literal value stored as a double for simplicity
        struct {
            double number_value; // numeric value (integer or float)
        } literal_number;
        
        // boolean literal value, used directly in conditions
        struct {
            bool bool_value;     // true or false
        } literal_bool;
        
        // identifier reference, duplicated string for ownership
        struct {
            char* name;          // variable or function name
        } identifier;
        
        // binary operation with a token operator and two child expressions
        struct {
            TokenType op;        // operator token (PLUS, MINUS, STAR, etc.)
            ASTNode* left;       // left operand expression
            ASTNode* right;      // right operand expression
        } binary;
        
        // unary operation with an operator and a single operand
        struct {
            TokenType op;        // operator token (MINUS, NOT)
            ASTNode* operand;    // operand expression
        } unary;
        
        // call node with a callee expression and a list of argument nodes
        struct {
            ASTNode* callee;          // expression being called (function or method)
            ASTNodeList* arguments;   // list of argument expressions
        } call;
        
        // index access storing the object and the index expression (bracket notation)
        struct {
            ASTNode* object;     // table or array being accessed
            ASTNode* member;     // index expression (key or position)
        } access;
        
        // table literal with separate lists for positional items and key-value pairs
        struct {
            ASTNodeList* items;       // sequential array-like elements (positional)
            ASTNodeList* key_values;  // named field assignments (key = value)
        } table_literal;
        
        // variable assignment or declaration, with optional access path for fields
        struct {
            char* name;               // variable name being assigned
            ASTNode* value;           // expression being assigned
            bool is_declaration;      // true for 'var x = val', false for 'x = val'
            ASTNode* access_path;     // non-null for x.field = val to track the target
        } var_assign;
        
        // function declaration with name, parameters, and body block
        struct {
            char* name;               // function name (NULL for anonymous functions)
            ASTNodeList* params;      // list of parameter nodes
            ASTNode* body;            // block node containing function body
        } function_decl;
        
        // if statement with condition, then branch, optional elif chain, and else branch
        struct {
            ASTNode* condition;       // condition expression
            ASTNode* then_branch;     // block executed when condition is true
            ASTNode* elif_chain;      // linked list of elif branches
            ASTNode* else_branch;     // block executed when all conditions are false
        } if_stmt;
        
        // single elif branch linked into the elif chain of an if statement
        struct {
            ASTNode* condition;       // elif condition expression
            ASTNode* body;            // block executed when elif condition is true
            ASTNode* next_elif;       // next elif node in the chain
        } elif_branch;
        
        // for loop with optional variable, condition, range bounds, and step
        struct {
            char* var_name;           // loop variable name (NULL for conditional loops)
            ASTNode* condition;       // loop condition (NULL for range loops)
            ASTNode* start;           // start value for numeric range loops
            ASTNode* end;             // end value for numeric range loops
            ASTNode* step;            // step value for numeric range loops (NULL for default 1)
            ASTNode* body;            // block node containing loop body
        } for_stmt;
        
        // import statement storing the module path as a duplicated string
        struct {
            char* module_path;        // dotted module path (e.g., "os", "math.sqrt")
        } import_stmt;
        
        // module block with its own name and body for nested modules
        struct {
            char* module_name;        // name of the imported module
            ASTNode* body;            // body of the imported module
        } module_block;

        // return statement with an optional return value expression
        struct {
            ASTNode* value;           // return value expression (NULL for bare return)
        } return_stmt;
        
        // string interpolation with a list of alternating string literals and expressions
        struct {
            ASTNodeList* parts;       // list of string literal and expression nodes
        } string_interp;
        
        // block node holding a list of statements executed sequentially
        struct {
            ASTNodeList* statements;  // list of statement nodes
        } block;
        
        // parameter definition holding the parameter name for function signatures
        struct {
            char* name;               // parameter name
        } param;
        
        // ternary expression (condition ? true_expr : false_expr)
        struct {
            ASTNode* condition;       // condition expression
            ASTNode* true_expr;       // expression evaluated when condition is true
            ASTNode* false_expr;      // expression evaluated when condition is false
        } ternary;

        // expression statement wrapper to treat any expression as a statement
        struct {
            ASTNode* expression;      // expression being treated as a statement
        } expr_stmt;
    };
};

// dynamic array used for lists of ast nodes (statements, arguments, etc.)
struct ASTNodeList {
    ASTNode** nodes;             // dynamically growing array of AST node pointers
    int count;                   // number of nodes currently stored
    int capacity;                // allocated capacity of the nodes array
};

// allocates a zero-initialized AST node with the given type and source position
ASTNode* ast_create_node(ASTNodeType type, int line, int column);

// wraps a numeric literal into an AST node, storing the raw double value
ASTNode* ast_create_literal_number(double value, int line, int column);

// duplicates the string so the AST owns its memory independently
ASTNode* ast_create_literal_string(const char* value, int line, int column);

// creates a none/null literal node (no associated value)
ASTNode* ast_create_literal_none(int line, int column);

// stores a boolean literal, used for conditions and direct values
ASTNode* ast_create_literal_bool(bool value, int line, int column);

// identifier nodes hold variable or function names, duplicating for ownership
ASTNode* ast_create_identifier(const char* name, int line, int column);

// binary operation combines two subexpressions with a token operator
ASTNode* ast_create_binary(TokenType op, ASTNode* left, ASTNode* right);

// unary operation applies to a single operand (negation, logical not)
ASTNode* ast_create_unary(TokenType op, ASTNode* operand);

// function call with a callee expression and a list of argument nodes
ASTNode* ast_create_call(ASTNode* callee, ASTNodeList* arguments);

// indexed access like array[key] or table[field], storing both object and index
ASTNode* ast_create_index_access(ASTNode* object, ASTNode* index);

// table constructor with separate sequential items and key-value pairs
ASTNode* ast_create_table_literal(ASTNodeList* items, ASTNodeList* key_values, int line, int column);

// handles both variable declaration and assignment, with optional access path for indexed targets
ASTNode* ast_create_var_assign(const char* name, ASTNode* value, bool is_decl, 
                                ASTNode* access_path, int line, int column);

// function declaration with optional name, parameter list, and body block
ASTNode* ast_create_function(const char* name, ASTNodeList* params, ASTNode* body, 
                              int line, int column);

// if statement with condition, then branch, optional elif chain, and optional else branch
ASTNode* ast_create_if(ASTNode* condition, ASTNode* then_branch, 
                       ASTNode* elif_chain, ASTNode* else_branch);

// for loop with optional variable name, condition, start/end/step for numeric range, and body
ASTNode* ast_create_for(const char* var_name, ASTNode* condition, ASTNode* start, 
                        ASTNode* end, ASTNode* step, ASTNode* body, int line, int column);

// import statement stores the module path as a duplicated string
ASTNode* ast_create_import(const char* module_path, int line, int column);

// return statement may carry an optional expression value
ASTNode* ast_create_return(ASTNode* value, int line, int column);

// string interpolation node holds a list of parts (strings and expressions)
ASTNode* ast_create_string_interp(ASTNodeList* parts);

// ternary node: condition ? true_expr : false_expr
ASTNode* ast_create_ternary(ASTNode* condition, ASTNode* true_expr, ASTNode* false_expr, int line, int column);

// creates a type check node for parameter validation (reserved for future use)
ASTNode* ast_create_type_check(const char* param_name, const char* type_name, 
                                int line, int column);

// block node groups a list of statements, using first statement's location as fallback
ASTNode* ast_create_block(ASTNodeList* statements);

// expression statement wraps a standalone expression as a statement
ASTNode* ast_create_expr_stmt(ASTNode* expression);

// parameter node holds a parameter name for function definitions
ASTNode* ast_create_param(const char* name, int line, int column);

// module block represents a separate module with its own name and body
ASTNode* ast_create_module_block(const char* name, ASTNode* body, int line, int column);

// creates a dynamic array for AST nodes with an initial capacity of 8
ASTNodeList* ast_list_create();

// appends a node to the list, doubling capacity if needed
void ast_list_add(ASTNodeList* list, ASTNode* node);

// frees only the list container, not the nodes themselves (tree owns them)
void ast_list_free(ASTNodeList* list);

// recursively frees an AST node and all its children, handling each type specifically
void ast_free_node(ASTNode* node);

// public entry point to free the whole AST program tree
void ast_free(ASTNode* program);

#endif // AST_H
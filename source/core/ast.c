#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// node factory with zero-initialized memory and source position tracking
ASTNode* ast_create_node(ASTNodeType type, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));  // allocate and zero out node memory
    node->type = type;                                     // set node type
    node->line = line;                                     // store source line number
    node->column = column;                                 // store source column position
    return node;                                           // return new node
}

// wraps a numeric literal into an ast node, storing the raw double value
ASTNode* ast_create_literal_number(double value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_NUMBER, line, column);  // create base node
    node->literal_number.number_value = value;                          // store numeric value
    return node;                                                        // return new literal node
}

// duplicates the string so the ast owns its memory independently
ASTNode* ast_create_literal_string(const char* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_STRING, line, column);  // create base node
    node->literal_string.string_value = strdup(value);                  // duplicate string for ownership
    return node;                                                        // return new string literal
}

// none literal has no associated value since it represents a single null/nil value
ASTNode* ast_create_literal_none(int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_NONE, line, column);  // create base node, no extra data
    return node;                                                      // return none literal node
}

// stores a boolean literal, used for conditions and direct values
ASTNode* ast_create_literal_bool(bool value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_BOOL, line, column);  // create base node
    node->literal_bool.bool_value = value;                            // store boolean value
    return node;                                                      // return boolean literal node
}

// identifier nodes hold variable or function names, duplicating for ownership
ASTNode* ast_create_identifier(const char* name, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));  // allocate and zero out node
    node->type = AST_IDENTIFIER;                           // set node type to identifier
    node->line = line;                                     // store source line
    node->column = column;                                 // store source column
    node->in_interpolation = false;                        // not inside interpolation by default
    node->identifier.name = strdup(name);                  // duplicate name string
    return node;                                           // return identifier node
}

// binary operation combines two subexpressions with a token operator
ASTNode* ast_create_binary(TokenType op, ASTNode* left, ASTNode* right) {
    ASTNode* node = ast_create_node(AST_BINARY, left->line, left->column);  // create binary node with left location
    node->binary.op = op;                                                   // store operator type
    node->binary.left = left;                                               // store left operand
    node->binary.right = right;                                             // store right operand
    return node;                                                            // return binary operation node
}

// unary operation applies to a single operand, e.g. negation or not
ASTNode* ast_create_unary(TokenType op, ASTNode* operand) {
    ASTNode* node = ast_create_node(AST_UNARY, operand->line, operand->column);  // create unary node with operand location
    node->unary.op = op;                                                         // store operator type
    node->unary.operand = operand;                                               // store operand
    return node;                                                                 // return unary operation node
}

// function call with a callee expression and a list of argument nodes
ASTNode* ast_create_call(ASTNode* callee, ASTNodeList* arguments) {
    ASTNode* node = ast_create_node(AST_CALL, callee->line, callee->column);  // create call node with callee location
    node->call.callee = callee;                                               // store callee expression
    node->call.arguments = arguments ? arguments : ast_list_create();         // store args list, create if null
    return node;                                                              // return call node
}

// indexed access like array[key] or table[field], storing both object and index
ASTNode* ast_create_index_access(ASTNode* object, ASTNode* index) {
    ASTNode* node = ast_create_node(AST_INDEX_ACCESS, object->line, object->column);  // create access node with object location
    node->access.object = object;                                                     // store object being accessed
    node->access.member = index;                                                      // store index expression
    return node;                                                                      // return index access node
}

// table constructor with separate sequential items and key-value pairs
ASTNode* ast_create_table_literal(ASTNodeList* items, ASTNodeList* key_values, int line, int column) {
    ASTNode* node = ast_create_node(AST_TABLE_LITERAL, line, column);              // create table node with source location
    node->table_literal.items = items ? items : ast_list_create();                 // store items list, create if null
    node->table_literal.key_values = key_values ? key_values : ast_list_create();  // store key-value pairs, create if null
    return node;                                                                   // return table literal node
}

// handles both variable declaration and assignment, with optional access path for indexed targets
ASTNode* ast_create_var_assign(const char* name, ASTNode* value, bool is_decl,
                                ASTNode* access_path, int line, int column) {
    ASTNode* node = ast_create_node(is_decl ? AST_VAR_DECL : AST_ASSIGN, line, column);  // choose node type based on declaration flag
    node->var_assign.name = name ? strdup(name) : NULL;                                  // duplicate name if provided
    node->var_assign.value = value;                                                      // store value expression
    node->var_assign.is_declaration = is_decl;                                           // store declaration flag
    node->var_assign.access_path = access_path;                                          // store optional access path
    return node;                                                                         // return var/assign node
}

// function declaration with optional name, parameter list, and body block
ASTNode* ast_create_function(const char* name, ASTNodeList* params, ASTNode* body,
                              int line, int column) {
    ASTNode* node = ast_create_node(AST_FUNCTION_DECL, line, column);  // create function node with source location
    node->function_decl.name = name ? strdup(name) : NULL;             // duplicate function name if provided
    node->function_decl.params = params ? params : ast_list_create();  // store params list, create if null
    node->function_decl.body = body;                                   // store function body block
    return node;                                                       // return function declaration node
}

// if statement with condition, then branch, optional elif chain, and optional else branch
ASTNode* ast_create_if(ASTNode* condition, ASTNode* then_branch,
                       ASTNode* elif_chain, ASTNode* else_branch) {
    ASTNode* node = ast_create_node(AST_IF_STMT, condition->line, condition->column);  // create if node with condition location
    node->if_stmt.condition = condition;                                               // store condition expression
    node->if_stmt.then_branch = then_branch;                                           // store then branch block
    node->if_stmt.elif_chain = elif_chain;                                             // store elif chain (list of if nodes)
    node->if_stmt.else_branch = else_branch;                                           // store optional else branch
    return node;                                                                       // return if statement node
}

// for loop with optional variable name, condition, start/end/step for numeric range, and body
ASTNode* ast_create_for(const char* var_name, ASTNode* condition, ASTNode* start, ASTNode* end, ASTNode* step, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_FOR_STMT, line, column);   // create for node with source location
    node->for_stmt.var_name = var_name ? strdup(var_name) : NULL;  // duplicate loop variable name if provided
    node->for_stmt.condition = condition;                          // store condition expression (for generic for)
    node->for_stmt.start = start;                                  // store start value (for numeric for)
    node->for_stmt.end = end;                                      // store end value (for numeric for)
    node->for_stmt.step = step;                                    // store step value (for numeric for, optional)
    node->for_stmt.body = body;                                    // store loop body block
    return node;                                                   // return for statement node
}

// import statement stores the module path as a duplicated string
ASTNode* ast_create_import(const char* module_path, int line, int column) {
    ASTNode* node = ast_create_node(AST_IMPORT_STMT, line, column);  // create import node with source location
    node->import_stmt.module_path = strdup(module_path);             // duplicate module path string
    return node;                                                     // return import statement node
}

// return statement may carry an optional expression value
ASTNode* ast_create_return(ASTNode* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_RETURN_STMT, line, column);  // create return node with source location
    node->return_stmt.value = value;                                 // store optional return value
    return node;                                                     // return return statement node
}

// string interpolation node holds a list of parts (strings and expressions)
ASTNode* ast_create_string_interp(ASTNodeList* parts) {
    ASTNode* node = ast_create_node(AST_STRING_INTERP, 0, 0);  // create interpolation node (location not tracked)
    node->string_interp.parts = parts;                         // store list of parts
    return node;                                               // return interpolation node
}

// ternary node: condition ? true_expr : false_expr
ASTNode* ast_create_ternary(ASTNode* condition, ASTNode* true_expr, ASTNode* false_expr, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));  // allocate and zero out node
    node->type = AST_TERNARY;                              // set node type to ternary
    node->line = line;                                     // store source line
    node->column = column;                                 // store source column
    node->ternary.condition = condition;                   // store condition expression
    node->ternary.true_expr = true_expr;                   // store true branch expression
    node->ternary.false_expr = false_expr;                 // store false branch expression
    return node;                                           // return ternary node
}

// block node groups a list of statements, using first statement's location as fallback
ASTNode* ast_create_block(ASTNodeList* statements) {
    ASTNode* node = ast_create_node(AST_BLOCK, 
        statements->count > 0 ? statements->nodes[0]->line : 0,     // use first statement's line or 0 if empty
        statements->count > 0 ? statements->nodes[0]->column : 0);  // use first statement's column or 0 if empty
    node->block.statements = statements;                            // store statements list
    return node;                                                    // return block node
}

// expression statement wraps a standalone expression as a statement
ASTNode* ast_create_expr_stmt(ASTNode* expression) {
    ASTNode* node = ast_create_node(AST_EXPR_STMT, expression->line, expression->column);  // create expr stmt with expression location
    node->expr_stmt.expression = expression;                                               // store wrapped expression
    return node;                                                                           // return expression statement node
}

// parameter node holds a parameter name for function definitions
ASTNode* ast_create_param(const char* name, int line, int column) {
    ASTNode* node = ast_create_node(AST_PARAM, line, column);  // create parameter node with source location
    node->param.name = strdup(name);                           // duplicate parameter name
    return node;                                               // return parameter node
}

// module block represents a separate module with its own name and body
ASTNode* ast_create_module_block(const char* name, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_MODULE_BLOCK, line, column);  // create module block with source location
    node->module_block.module_name = strdup(name);                    // duplicate module name
    node->module_block.body = body;                                   // store module body
    return node;                                                      // return module block node
}

// creates a dynamic array for ast nodes with an initial capacity of 8
ASTNodeList* ast_list_create() {
    ASTNodeList* list = (ASTNodeList*)malloc(sizeof(ASTNodeList));       // allocate list struct
    list->capacity = 8;                                                  // set initial capacity
    list->count = 0;                                                     // no nodes yet
    list->nodes = (ASTNode**)malloc(sizeof(ASTNode*) * list->capacity);  // allocate node pointer array
    return list;                                                         // return new list
}

// appends a node to the list, doubling capacity if needed
void ast_list_add(ASTNodeList* list, ASTNode* node) {
    if (list->count >= list->capacity) {  // check if list is full
        list->capacity *= 2;              // double capacity
        list->nodes = (ASTNode**)realloc(list->nodes, sizeof(ASTNode*) * list->capacity);  // resize array
    }
    list->nodes[list->count++] = node;    // insert node and increment count
}

// frees only the list container, not the nodes themselves (tree owns them)
void ast_list_free(ASTNodeList* list) {
    if (!list) return;                    // guard against null
    free(list->nodes);                    // free node pointer array
    free(list);                           // free list struct
}

// recursively frees an ast node and all its children, handling each type specifically
void ast_free_node(ASTNode* node) {
    if (!node) return;                                                   // guard against null
    
    switch (node->type) {                                                // dispatch based on node type
        case AST_LITERAL_STRING:                                         // string literal node
            free(node->literal_string.string_value);                     // free the duplicated string
            break;
        case AST_IDENTIFIER:                                             // identifier node
            free(node->identifier.name);                                 // free the duplicated name
            break;
        case AST_BINARY:                                                 // binary operation node
            ast_free_node(node->binary.left);                            // recursively free left operand
            ast_free_node(node->binary.right);                           // recursively free right operand
            break;
        case AST_UNARY:                                                  // unary operation node
            ast_free_node(node->unary.operand);                          // recursively free operand
            break;
        case AST_CALL:                                                   // function call node
            ast_free_node(node->call.callee);                            // recursively free callee
            for (int i = 0; i < node->call.arguments->count; i++) {      // iterate over arguments
                ast_free_node(node->call.arguments->nodes[i]);           // recursively free each argument
            }
            ast_list_free(node->call.arguments);                         // free arguments list container
            break;
        case AST_INDEX_ACCESS:                                           // index access node
            ast_free_node(node->access.object);                          // recursively free object
            ast_free_node(node->access.member);                          // recursively free index
            break;
        case AST_TABLE_LITERAL:                                              // table literal node
            for (int i = 0; i < node->table_literal.items->count; i++)       // iterate over sequential items
                ast_free_node(node->table_literal.items->nodes[i]);          // recursively free each item
            ast_list_free(node->table_literal.items);                        // free items list container
            for (int i = 0; i < node->table_literal.key_values->count; i++)  // iterate over key-value pairs
                ast_free_node(node->table_literal.key_values->nodes[i]);     // recursively free each pair
            ast_list_free(node->table_literal.key_values);                   // free key-values list container
            break;
        case AST_VAR_DECL:                                               // variable declaration node
        case AST_ASSIGN:                                                 // assignment node
            free(node->var_assign.name);                                 // free variable name
            ast_free_node(node->var_assign.value);                       // recursively free value expression
            if (node->var_assign.access_path)                            // check if access path exists
                ast_free_node(node->var_assign.access_path);             // recursively free access path
            break;
        case AST_FUNCTION_DECL:                                          // function declaration node
            free(node->function_decl.name);                              // free function name
            for (int i = 0; i < node->function_decl.params->count; i++)  // iterate over parameters
                ast_free_node(node->function_decl.params->nodes[i]);     // recursively free each parameter
            ast_list_free(node->function_decl.params);                   // free params list container
            ast_free_node(node->function_decl.body);                     // recursively free body
            break;
        case AST_IF_STMT:                                                // if statement node
            ast_free_node(node->if_stmt.condition);                      // recursively free condition
            ast_free_node(node->if_stmt.then_branch);                    // recursively free then branch
            ast_free_node(node->if_stmt.elif_chain);                     // recursively free elif chain
            ast_free_node(node->if_stmt.else_branch);                    // recursively free else branch
            break;
        case AST_FOR_STMT:                                               // for loop node
            free(node->for_stmt.var_name);                               // free loop variable name
            if (node->for_stmt.condition) ast_free_node(node->for_stmt.condition);  // free condition if present
            if (node->for_stmt.start) ast_free_node(node->for_stmt.start);          // free start if present
            if (node->for_stmt.end) ast_free_node(node->for_stmt.end);              // free end if present
            if (node->for_stmt.step) ast_free_node(node->for_stmt.step);            // free step if present
            ast_free_node(node->for_stmt.body);                          // recursively free body
            break;
        case AST_IMPORT_STMT:                                            // import statement node
            free(node->import_stmt.module_path);                         // free module path string
            break;
        case AST_MODULE_BLOCK:                                           // module block node
            free(node->module_block.module_name);                        // free module name
            ast_free_node(node->module_block.body);                      // recursively free body
            break;
        case AST_RETURN_STMT:                                            // return statement node
            ast_free_node(node->return_stmt.value);                      // recursively free return value
            break;
        case AST_STRING_INTERP:                                          // string interpolation node
            for (int i = 0; i < node->string_interp.parts->count; i++)   // iterate over parts
                ast_free_node(node->string_interp.parts->nodes[i]);      // recursively free each part
            ast_list_free(node->string_interp.parts);                    // free parts list container
            break;
        case AST_PROGRAM:                                                // program node (same as block)
        case AST_BLOCK:                                                  // block node
            for (int i = 0; i < node->block.statements->count; i++)      // iterate over statements
                ast_free_node(node->block.statements->nodes[i]);         // recursively free each statement
            ast_list_free(node->block.statements);                       // free statements list container
            break;
        case AST_EXPR_STMT:                                              // expression statement node
            ast_free_node(node->expr_stmt.expression);                   // recursively free expression
            break;
        case AST_PARAM:                                                  // parameter node
            free(node->param.name);                                      // free parameter name
            break;
        case AST_TERNARY:                                                // ternary expression node
            ast_free_node(node->ternary.condition);                      // recursively free condition
            ast_free_node(node->ternary.true_expr);                      // recursively free true branch
            ast_free_node(node->ternary.false_expr);                     // recursively free false branch
            break;
        default:                   // unknown node type
            break;                 // nothing to free
    }
    free(node);                    // free the node itself
}

// public entry point to free the whole ast program tree
void ast_free(ASTNode* program) {
    ast_free_node(program);        // recursively free entire tree
}
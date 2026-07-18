#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// node factory with zero-initialized memory and source position tracking
ASTNode* ast_create_node(ASTNodeType type, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

// wraps a numeric literal into an ast node, storing the raw double value
ASTNode* ast_create_literal_number(double value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_NUMBER, line, column);
    node->literal_number.number_value = value;
    return node;
}

// duplicates the string so the ast owns its memory independently
ASTNode* ast_create_literal_string(const char* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_STRING, line, column);
    node->literal_string.string_value = strdup(value);
    return node;
}

// none literal has no associated value since it represents a single null/nil value
ASTNode* ast_create_literal_none(int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_NONE, line, column);
    return node;
}

// stores a boolean literal, used for conditions and direct values
ASTNode* ast_create_literal_bool(bool value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_BOOL, line, column);
    node->literal_bool.bool_value = value;
    return node;
}

// identifier nodes hold variable or function names, duplicating for ownership
ASTNode* ast_create_identifier(const char* name, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = AST_IDENTIFIER;
    node->line = line;
    node->column = column;
    node->in_interpolation = false;
    node->identifier.name = strdup(name);
    return node;
}

// binary operation combines two subexpressions with a token operator
ASTNode* ast_create_binary(TokenType op, ASTNode* left, ASTNode* right) {
    ASTNode* node = ast_create_node(AST_BINARY, left->line, left->column);
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    return node;
}

// unary operation applies to a single operand, e.g. negation or not
ASTNode* ast_create_unary(TokenType op, ASTNode* operand) {
    ASTNode* node = ast_create_node(AST_UNARY, operand->line, operand->column);
    node->unary.op = op;
    node->unary.operand = operand;
    return node;
}

// function call with a callee expression and a list of argument nodes
ASTNode* ast_create_call(ASTNode* callee, ASTNodeList* arguments) {
    ASTNode* node = ast_create_node(AST_CALL, callee->line, callee->column);
    node->call.callee = callee;
    node->call.arguments = arguments ? arguments : ast_list_create();
    return node;
}

// indexed access like array[key] or table[field], storing both object and index
ASTNode* ast_create_index_access(ASTNode* object, ASTNode* index) {
    ASTNode* node = ast_create_node(AST_INDEX_ACCESS, object->line, object->column);
    node->access.object = object;
    node->access.member = index;
    return node;
}

// table constructor with separate sequential items and key-value pairs
ASTNode* ast_create_table_literal(ASTNodeList* items, ASTNodeList* key_values, int line, int column) {
    ASTNode* node = ast_create_node(AST_TABLE_LITERAL, line, column);
    node->table_literal.items = items ? items : ast_list_create();
    node->table_literal.key_values = key_values ? key_values : ast_list_create();
    return node;
}

// handles both variable declaration and assignment, with optional access path for indexed targets
ASTNode* ast_create_var_assign(const char* name, ASTNode* value, bool is_decl,
                                ASTNode* access_path, int line, int column) {
    ASTNode* node = ast_create_node(is_decl ? AST_VAR_DECL : AST_ASSIGN, line, column);
    node->var_assign.name = name ? strdup(name) : NULL;
    node->var_assign.value = value;
    node->var_assign.is_declaration = is_decl;
    node->var_assign.access_path = access_path;
    return node;
}

// function declaration with optional name, parameter list, and body block
ASTNode* ast_create_function(const char* name, ASTNodeList* params, ASTNode* body,
                              int line, int column) {
    ASTNode* node = ast_create_node(AST_FUNCTION_DECL, line, column);
    node->function_decl.name = name ? strdup(name) : NULL;
    node->function_decl.params = params ? params : ast_list_create();
    node->function_decl.body = body;
    return node;
}

// if statement with condition, then branch, optional elif chain, and optional else branch
ASTNode* ast_create_if(ASTNode* condition, ASTNode* then_branch,
                       ASTNode* elif_chain, ASTNode* else_branch) {
    ASTNode* node = ast_create_node(AST_IF_STMT, condition->line, condition->column);
    node->if_stmt.condition = condition;
    node->if_stmt.then_branch = then_branch;
    node->if_stmt.elif_chain = elif_chain;
    node->if_stmt.else_branch = else_branch;
    return node;
}

// for loop with optional variable name, condition, start/end/step for numeric range, and body
ASTNode* ast_create_for(const char* var_name, ASTNode* condition, ASTNode* start, ASTNode* end, ASTNode* step, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_FOR_STMT, line, column);
    node->for_stmt.var_name = var_name ? strdup(var_name) : NULL;
    node->for_stmt.condition = condition;
    node->for_stmt.start = start;
    node->for_stmt.end = end;
    node->for_stmt.step = step;
    node->for_stmt.body = body;
    return node;
}

// import statement stores the module path as a duplicated string
ASTNode* ast_create_import(const char* module_path, int line, int column) {
    ASTNode* node = ast_create_node(AST_IMPORT_STMT, line, column);
    node->import_stmt.module_path = strdup(module_path);
    return node;
}

// return statement may carry an optional expression value
ASTNode* ast_create_return(ASTNode* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_RETURN_STMT, line, column);
    node->return_stmt.value = value;
    return node;
}

// string interpolation node holds a list of parts (strings and expressions)
ASTNode* ast_create_string_interp(ASTNodeList* parts) {
    ASTNode* node = ast_create_node(AST_STRING_INTERP, 0, 0);
    node->string_interp.parts = parts;
    return node;
}

// ternary node: condition ? true_expr : false_expr
ASTNode* ast_create_ternary(ASTNode* condition, ASTNode* true_expr, ASTNode* false_expr, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = AST_TERNARY;
    node->line = line;
    node->column = column;
    node->ternary.condition = condition;
    node->ternary.true_expr = true_expr;
    node->ternary.false_expr = false_expr;
    return node;
}

// block node groups a list of statements, using first statement's location as fallback
ASTNode* ast_create_block(ASTNodeList* statements) {
    ASTNode* node = ast_create_node(AST_BLOCK, 
        statements->count > 0 ? statements->nodes[0]->line : 0,
        statements->count > 0 ? statements->nodes[0]->column : 0);
    node->block.statements = statements;
    return node;
}

// expression statement wraps a standalone expression as a statement
ASTNode* ast_create_expr_stmt(ASTNode* expression) {
    ASTNode* node = ast_create_node(AST_EXPR_STMT, expression->line, expression->column);
    node->expr_stmt.expression = expression;
    return node;
}

// parameter node holds a parameter name for function definitions
ASTNode* ast_create_param(const char* name, int line, int column) {
    ASTNode* node = ast_create_node(AST_PARAM, line, column);
    node->param.name = strdup(name);
    return node;
}

// module block represents a separate module with its own name and body
ASTNode* ast_create_module_block(const char* name, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_MODULE_BLOCK, line, column);
    node->module_block.module_name = strdup(name);
    node->module_block.body = body;
    return node;
}

// creates a dynamic array for ast nodes with an initial capacity of 8
ASTNodeList* ast_list_create() {
    ASTNodeList* list = (ASTNodeList*)malloc(sizeof(ASTNodeList));
    list->capacity = 8;
    list->count = 0;
    list->nodes = (ASTNode**)malloc(sizeof(ASTNode*) * list->capacity);
    return list;
}

// appends a node to the list, doubling capacity if needed
void ast_list_add(ASTNodeList* list, ASTNode* node) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->nodes = (ASTNode**)realloc(list->nodes, sizeof(ASTNode*) * list->capacity);
    }
    list->nodes[list->count++] = node;
}

// frees only the list container, not the nodes themselves (tree owns them)
void ast_list_free(ASTNodeList* list) {
    if (!list) return;
    free(list->nodes);
    free(list);
}

// recursively frees an ast node and all its children, handling each type specifically
void ast_free_node(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_LITERAL_STRING:
            free(node->literal_string.string_value);
            break;
        case AST_IDENTIFIER:
            free(node->identifier.name);
            break;
        case AST_BINARY:
            ast_free_node(node->binary.left);
            ast_free_node(node->binary.right);
            break;
        case AST_UNARY:
            ast_free_node(node->unary.operand);
            break;
        case AST_CALL:
            ast_free_node(node->call.callee);
            for (int i = 0; i < node->call.arguments->count; i++) {
                ast_free_node(node->call.arguments->nodes[i]);
            }
            ast_list_free(node->call.arguments);
            break;
        case AST_INDEX_ACCESS:
            ast_free_node(node->access.object);
            ast_free_node(node->access.member);
            break;
        case AST_TABLE_LITERAL:
            for (int i = 0; i < node->table_literal.items->count; i++)
                ast_free_node(node->table_literal.items->nodes[i]);
            ast_list_free(node->table_literal.items);
            for (int i = 0; i < node->table_literal.key_values->count; i++)
                ast_free_node(node->table_literal.key_values->nodes[i]);
            ast_list_free(node->table_literal.key_values);
            break;
        case AST_VAR_DECL:
        case AST_ASSIGN:
            free(node->var_assign.name);
            ast_free_node(node->var_assign.value);
            if (node->var_assign.access_path)
                ast_free_node(node->var_assign.access_path);
            break;
        case AST_FUNCTION_DECL:
            free(node->function_decl.name);
            for (int i = 0; i < node->function_decl.params->count; i++)
                ast_free_node(node->function_decl.params->nodes[i]);
            ast_list_free(node->function_decl.params);
            ast_free_node(node->function_decl.body);
            break;
        case AST_IF_STMT:
            ast_free_node(node->if_stmt.condition);
            ast_free_node(node->if_stmt.then_branch);
            ast_free_node(node->if_stmt.elif_chain);
            ast_free_node(node->if_stmt.else_branch);
            break;
        case AST_FOR_STMT:
            free(node->for_stmt.var_name);
            if (node->for_stmt.condition) ast_free_node(node->for_stmt.condition);
            if (node->for_stmt.start) ast_free_node(node->for_stmt.start);
            if (node->for_stmt.end) ast_free_node(node->for_stmt.end);
            if (node->for_stmt.step) ast_free_node(node->for_stmt.step);
            ast_free_node(node->for_stmt.body);
            break;
        case AST_IMPORT_STMT:
            free(node->import_stmt.module_path);
            break;
        case AST_MODULE_BLOCK:
            free(node->module_block.module_name);
            ast_free_node(node->module_block.body);
            break;
        case AST_RETURN_STMT:
            ast_free_node(node->return_stmt.value);
            break;
        case AST_STRING_INTERP:
            for (int i = 0; i < node->string_interp.parts->count; i++)
                ast_free_node(node->string_interp.parts->nodes[i]);
            ast_list_free(node->string_interp.parts);
            break;
        case AST_PROGRAM:
        case AST_BLOCK:
            for (int i = 0; i < node->block.statements->count; i++)
                ast_free_node(node->block.statements->nodes[i]);
            ast_list_free(node->block.statements);
            break;
        case AST_EXPR_STMT:
            ast_free_node(node->expr_stmt.expression);
            break;
        case AST_PARAM:
            free(node->param.name);
            break;
        case AST_TERNARY:
            ast_free_node(node->ternary.condition);
            ast_free_node(node->ternary.true_expr);
            ast_free_node(node->ternary.false_expr);
            break;
        default:
            break;
    }
    free(node);
}

// public entry point to free the whole ast program tree
void ast_free(ASTNode* program) {
    ast_free_node(program);
}
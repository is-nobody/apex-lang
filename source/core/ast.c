#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== Node Creation ==========

ASTNode* ast_create_node(ASTNodeType type, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

ASTNode* ast_create_literal_number(double value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_NUMBER, line, column);
    node->literal_number.number_value = value;
    return node;
}

ASTNode* ast_create_literal_string(const char* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_STRING, line, column);
    node->literal_string.string_value = strdup(value);
    return node;
}

ASTNode* ast_create_literal_bool(bool value, int line, int column) {
    ASTNode* node = ast_create_node(AST_LITERAL_BOOL, line, column);
    node->literal_bool.bool_value = value;
    return node;
}

ASTNode* ast_create_identifier(const char* name, int line, int column) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER, line, column);
    node->identifier.name = strdup(name);
    return node;
}

ASTNode* ast_create_binary(TokenType op, ASTNode* left, ASTNode* right) {
    ASTNode* node = ast_create_node(AST_BINARY, left->line, left->column);
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    return node;
}

ASTNode* ast_create_unary(TokenType op, ASTNode* operand) {
    ASTNode* node = ast_create_node(AST_UNARY, operand->line, operand->column);
    node->unary.op = op;
    node->unary.operand = operand;
    return node;
}

ASTNode* ast_create_call(ASTNode* callee, ASTNodeList* arguments) {
    ASTNode* node = ast_create_node(AST_CALL, callee->line, callee->column);
    node->call.callee = callee;
    node->call.arguments = arguments ? arguments : ast_list_create();
    return node;
}

ASTNode* ast_create_member_access(ASTNode* object, ASTNode* member) {
    ASTNode* node = ast_create_node(
        member->type == AST_IDENTIFIER ? AST_MEMBER_ACCESS : AST_INDEX_ACCESS,
        object->line, object->column
    );
    node->access.object = object;
    node->access.member = member;
    return node;
}

ASTNode* ast_create_table_literal(ASTNodeList* items, ASTNodeList* key_values) {
    ASTNode* node = ast_create_node(AST_TABLE_LITERAL, 0, 0);
    node->table_literal.items = items ? items : ast_list_create();
    node->table_literal.key_values = key_values ? key_values : ast_list_create();
    return node;
}

ASTNode* ast_create_var_assign(const char* name, ASTNode* value, bool is_decl,
                                ASTNode* access_path, int line, int column) {
    ASTNode* node = ast_create_node(is_decl ? AST_VAR_DECL : AST_ASSIGN, line, column);
    node->var_assign.name = name ? strdup(name) : NULL;
    node->var_assign.value = value;
    node->var_assign.is_declaration = is_decl;
    node->var_assign.access_path = access_path;
    return node;
}

ASTNode* ast_create_function(const char* name, ASTNodeList* params, ASTNode* body,
                              int line, int column) {
    ASTNode* node = ast_create_node(AST_FUNCTION_DECL, line, column);
    node->function_decl.name = name ? strdup(name) : NULL;
    node->function_decl.params = params ? params : ast_list_create();
    node->function_decl.body = body;
    return node;
}

ASTNode* ast_create_if(ASTNode* condition, ASTNode* then_branch,
                       ASTNode* elif_chain, ASTNode* else_branch) {
    ASTNode* node = ast_create_node(AST_IF_STMT, condition->line, condition->column);
    node->if_stmt.condition = condition;
    node->if_stmt.then_branch = then_branch;
    node->if_stmt.elif_chain = elif_chain;
    node->if_stmt.else_branch = else_branch;
    return node;
}

ASTNode* ast_create_while(ASTNode* condition, ASTNode* body) {
    ASTNode* node = ast_create_node(AST_WHILE_STMT, condition->line, condition->column);
    node->while_stmt.condition = condition;
    node->while_stmt.body = body;
    return node;
}

ASTNode* ast_create_for(const char* var_name, ASTNode* start, ASTNode* end,
                        ASTNode* step, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_FOR_STMT, line, column);
    node->for_stmt.var_name = strdup(var_name);
    node->for_stmt.start = start;
    node->for_stmt.end = end;
    node->for_stmt.step = step;
    node->for_stmt.body = body;
    return node;
}
ASTNode* ast_create_import(const char* module_path, int line, int column) {
    ASTNode* node = ast_create_node(AST_IMPORT_STMT, line, column);
    node->import_stmt.module_path = strdup(module_path);
    return node;
}

ASTNode* ast_create_return(ASTNode* value, int line, int column) {
    ASTNode* node = ast_create_node(AST_RETURN_STMT, line, column);
    node->return_stmt.value = value;
    return node;
}

ASTNode* ast_create_string_interp(ASTNodeList* parts) {
    ASTNode* node = ast_create_node(AST_STRING_INTERP, 0, 0);
    node->string_interp.parts = parts;
    return node;
}

ASTNode* ast_create_block(ASTNodeList* statements) {
    ASTNode* node = ast_create_node(AST_BLOCK, 
        statements->count > 0 ? statements->nodes[0]->line : 0,
        statements->count > 0 ? statements->nodes[0]->column : 0);
    node->block.statements = statements;
    return node;
}

ASTNode* ast_create_expr_stmt(ASTNode* expression) {
    ASTNode* node = ast_create_node(AST_EXPR_STMT, expression->line, expression->column);
    node->expr_stmt.expression = expression;
    return node;
}

ASTNode* ast_create_param(const char* name, int line, int column) {
    ASTNode* node = ast_create_node(AST_PARAM, line, column);
    node->param.name = strdup(name);
    return node;
}

ASTNode* ast_create_module_block(const char* name, ASTNode* body, int line, int column) {
    ASTNode* node = ast_create_node(AST_MODULE_BLOCK, line, column);
    node->module_block.module_name = strdup(name);
    node->module_block.body = body;
    return node;
}

// ========== List Operations ==========

ASTNodeList* ast_list_create() {
    ASTNodeList* list = (ASTNodeList*)malloc(sizeof(ASTNodeList));
    list->capacity = 8;
    list->count = 0;
    list->nodes = (ASTNode**)malloc(sizeof(ASTNode*) * list->capacity);
    return list;
}

void ast_list_add(ASTNodeList* list, ASTNode* node) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->nodes = (ASTNode**)realloc(list->nodes, sizeof(ASTNode*) * list->capacity);
    }
    list->nodes[list->count++] = node;
}

void ast_list_free(ASTNodeList* list) {
    if (!list) return;
    // Nodes are owned by the tree, don't free them here
    free(list->nodes);
    free(list);
}

// ========== Free AST ==========

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
        case AST_MEMBER_ACCESS:
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
        case AST_WHILE_STMT:
            ast_free_node(node->while_stmt.condition);
            ast_free_node(node->while_stmt.body);
            break;
        case AST_FOR_STMT:
            free(node->for_stmt.var_name);
            ast_free_node(node->for_stmt.start);
            ast_free_node(node->for_stmt.end);
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
        default:
            break;
    }
    free(node);
}

void ast_free(ASTNode* program) {
    ast_free_node(program);
}

// ========== Debug Print ==========

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void ast_print_impl(ASTNode* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("NULL\n");
        return;
    }
    
    print_indent(indent);
    
    switch (node->type) {
        case AST_PROGRAM:
            printf("Program\n");
            for (int i = 0; i < node->block.statements->count; i++)
                ast_print_impl(node->block.statements->nodes[i], indent + 1);
            break;
        case AST_IMPORT_STMT:
            printf("Import: %s\n", node->import_stmt.module_path);
            break;
        case AST_VAR_DECL:
            printf("VarDecl: %s\n", node->var_assign.name);
            ast_print_impl(node->var_assign.value, indent + 1);
            break;
        case AST_ASSIGN:
            printf("Assign: %s\n", node->var_assign.name);
            ast_print_impl(node->var_assign.value, indent + 1);
            break;
        case AST_FUNCTION_DECL:
            printf("Function: %s\n", node->function_decl.name);
            print_indent(indent + 1);
            printf("Params: %d\n", node->function_decl.params->count);
            ast_print_impl(node->function_decl.body, indent + 1);
            break;
        case AST_RETURN_STMT:
            printf("Return\n");
            ast_print_impl(node->return_stmt.value, indent + 1);
            break;
        case AST_IF_STMT:
            printf("If\n");
            ast_print_impl(node->if_stmt.condition, indent + 1);
            ast_print_impl(node->if_stmt.then_branch, indent + 1);
            if (node->if_stmt.elif_chain) {
                print_indent(indent + 1);
                printf("ElifChain\n");
                ast_print_impl(node->if_stmt.elif_chain, indent + 2);
            }
            if (node->if_stmt.else_branch) {
                print_indent(indent + 1);
                printf("Else\n");
                ast_print_impl(node->if_stmt.else_branch, indent + 2);
            }
            break;
        case AST_WHILE_STMT:
            printf("While\n");
            ast_print_impl(node->while_stmt.condition, indent + 1);
            ast_print_impl(node->while_stmt.body, indent + 1);
            break;
        case AST_FOR_STMT:
            printf("For: %s = ", node->for_stmt.var_name);
            ast_print_impl(node->for_stmt.start, indent);
            print_indent(indent + 1); printf("to ");
            ast_print_impl(node->for_stmt.end, indent + 1);
            if (node->for_stmt.step) {
                print_indent(indent + 1); printf("step ");
                ast_print_impl(node->for_stmt.step, indent + 1);
            }
            ast_print_impl(node->for_stmt.body, indent + 1);
            break;
        case AST_BREAK_STMT:
            printf("Break\n");
            break;
        case AST_CONTINUE_STMT:
            printf("Continue\n");
            break;
        case AST_EXPR_STMT:
            printf("ExprStmt\n");
            ast_print_impl(node->expr_stmt.expression, indent + 1);
            break;
        case AST_BINARY:
            printf("Binary: %s\n", token_type_name(node->binary.op));
            ast_print_impl(node->binary.left, indent + 1);
            ast_print_impl(node->binary.right, indent + 1);
            break;
        case AST_UNARY:
            printf("Unary: %s\n", token_type_name(node->unary.op));
            ast_print_impl(node->unary.operand, indent + 1);
            break;
        case AST_LITERAL_NUMBER:
            printf("Number: %g\n", node->literal_number.number_value);
            break;
        case AST_LITERAL_STRING:
            printf("String: \"%s\"\n", node->literal_string.string_value);
            break;
        case AST_LITERAL_BOOL:
            printf("Bool: %s\n", node->literal_bool.bool_value ? "true" : "false");
            break;
        case AST_IDENTIFIER:
            printf("Identifier: %s\n", node->identifier.name);
            break;
        case AST_MODULE_BLOCK:
            printf("ModuleBlock: %s\n", node->module_block.module_name);
            ast_print_impl(node->module_block.body, indent + 1);
            break;
        case AST_CALL:
            printf("Call\n");
            ast_print_impl(node->call.callee, indent + 1);
            print_indent(indent + 1);
            printf("Args: %d\n", node->call.arguments->count);
            for (int i = 0; i < node->call.arguments->count; i++)
                ast_print_impl(node->call.arguments->nodes[i], indent + 2);
            break;
        case AST_MEMBER_ACCESS:
            printf("MemberAccess\n");
            ast_print_impl(node->access.object, indent + 1);
            ast_print_impl(node->access.member, indent + 1);
            break;
        case AST_INDEX_ACCESS:
            printf("IndexAccess\n");
            ast_print_impl(node->access.object, indent + 1);
            ast_print_impl(node->access.member, indent + 1);
            break;
        case AST_TABLE_LITERAL:
            printf("TableLiteral (%d items, %d kv)\n",
                   node->table_literal.items->count,
                   node->table_literal.key_values->count);
            break;
        case AST_STRING_INTERP:
            printf("StringInterp\n");
            for (int i = 0; i < node->string_interp.parts->count; i++)
                ast_print_impl(node->string_interp.parts->nodes[i], indent + 1);
            break;
        case AST_BLOCK:
            printf("Block (%d stmts)\n", node->block.statements->count);
            for (int i = 0; i < node->block.statements->count; i++)
                ast_print_impl(node->block.statements->nodes[i], indent + 1);
            break;
        case AST_PARAM:
            printf("Param: %s\n", node->param.name);
            break;
        default:
            printf("Unknown node type: %d\n", node->type);
            break;
    }
}

void ast_print(ASTNode* node, int indent) {
    ast_print_impl(node, indent);
}
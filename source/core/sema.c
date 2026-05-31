#include "sema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ========== Error Reporting ==========

typedef struct {
    int line;
    int column;
    char* message;
    bool is_warning;
} SemanticError;

#define MAX_ERRORS 256

static SemanticError errors[MAX_ERRORS];
static int error_idx = 0;

static void sema_error(SemAnalyzer* sema, int line, int column, 
                        bool is_warning, const char* format, ...) {
    if (error_idx >= MAX_ERRORS) return;
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    errors[error_idx].line = line;
    errors[error_idx].column = column;
    errors[error_idx].message = strdup(buffer);
    errors[error_idx].is_warning = is_warning;
    error_idx++;
    
    if (is_warning) {
        sema->warning_count++;
        fprintf(stderr, "Warning: in %s on line %d: %s\n", 
                sema->filename, line, buffer);
    } else {
        sema->error_count++;
        fprintf(stderr, "SemanticError: in %s on line %d: %s\n", 
                sema->filename, line, buffer);
    }
}

void sema_print_errors(SemAnalyzer* sema) {
    printf("Errors: %d, Warnings: %d\n", sema->error_count, sema->warning_count);
    
    for (int i = 0; i < error_idx; i++) {
        printf("  %s in line %d: %s\n",
               errors[i].is_warning ? "Warning" : "Error",
               errors[i].line,
               errors[i].message);
    }
}

// ========== Type Utilities ==========

static const char* type_name(ValueType type) {
    switch (type) {
        case TYPE_NUMBER: return "number";
        case TYPE_STRING: return "string";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_TABLE: return "table";
        case TYPE_FUNCTION: return "function";
        case TYPE_UNKNOWN: return "unknown";
        case TYPE_ERROR: return "error";
        case TYPE_ANY: return "any";
        default: return "???";
    }
}

static ValueType type_from_string(const char* name) {
    if (strcmp(name, "number") == 0) return TYPE_NUMBER;
    if (strcmp(name, "string") == 0) return TYPE_STRING;
    if (strcmp(name, "boolean") == 0) return TYPE_BOOLEAN;
    if (strcmp(name, "table") == 0) return TYPE_TABLE;
    if (strcmp(name, "function") == 0) return TYPE_FUNCTION;
    return TYPE_UNKNOWN;
}

static bool is_numeric_type(ValueType type) {
    return type == TYPE_NUMBER;
}

static bool is_comparable_type(ValueType type) {
    return type == TYPE_NUMBER || type == TYPE_STRING || 
           type == TYPE_BOOLEAN;
}

// ========== Scope Management ==========

#define SCOPE_HASH_SIZE 64

static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % SCOPE_HASH_SIZE;
}

static Scope* scope_create(Scope* parent, int level) {
    Scope* scope = (Scope*)calloc(1, sizeof(Scope));
    scope->symbols = (Symbol**)calloc(SCOPE_HASH_SIZE, sizeof(Symbol*));
    scope->capacity = SCOPE_HASH_SIZE;
    scope->level = level;
    scope->parent = parent;
    return scope;
}

static void scope_destroy(Scope* scope) {
    if (!scope) return;
    
    for (int i = 0; i < scope->capacity; i++) {
        Symbol* sym = scope->symbols[i];
        while (sym) {
            Symbol* next = sym->next;
            free(sym->name);
            if (sym->func_info.param_types) {
                free(sym->func_info.param_types);
            }
            free(sym);
            sym = next;
        }
    }
    free(scope->symbols);
    free(scope);
}

static Symbol* scope_lookup(Scope* scope, const char* name) {
    unsigned int hash = hash_string(name);
    Symbol* sym = scope->symbols[hash];
    
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }
    return NULL;
}

static Symbol* scope_lookup_recursive(Scope* scope, const char* name) {
    while (scope) {
        Symbol* sym = scope_lookup(scope, name);
        if (sym) return sym;
        scope = scope->parent;
    }
    return NULL;
}

static Symbol* scope_insert(Scope* scope, const char* name, SymbolKind kind,
                             ValueType type, int line, int column) {
    // Check for duplicate in the CURRENT scope
    Symbol* existing = scope_lookup(scope, name);
    if (existing) {
        return NULL; // duplicate in same scope
    }
    
    unsigned int hash = hash_string(name);
    Symbol* sym = (Symbol*)calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->kind = kind;
    sym->value_type = type;
    sym->declared_type = type;
    sym->is_mutable = false;  // Apex: types are immutable
    sym->is_initialized = (type != TYPE_UNKNOWN);
    sym->scope_level = scope->level;
    sym->line = line;
    sym->column = column;
    
    sym->next = scope->symbols[hash];
    scope->symbols[hash] = sym;
    
    return sym;
}

// ========== Built-in Functions Registration ==========

static void register_builtin(Scope* scope, const char* name, 
                              ValueType return_type, int param_count, ...) {
    Symbol* sym = (Symbol*)calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->kind = SYMBOL_BUILTIN;
    sym->value_type = TYPE_FUNCTION;
    sym->func_info.return_type = return_type;
    sym->func_info.param_count = param_count;
    sym->func_info.has_return = true;
    
    if (param_count > 0) {
        sym->func_info.param_types = (ValueType*)malloc(
            sizeof(ValueType) * param_count);
        
        va_list args;
        va_start(args, param_count);
        for (int i = 0; i < param_count; i++) {
            sym->func_info.param_types[i] = va_arg(args, ValueType);
        }
        va_end(args);
    }
    
    unsigned int hash = hash_string(name);
    sym->next = scope->symbols[hash];
    scope->symbols[hash] = sym;
}

static void register_all_builtins(Scope* scope) {
    // OS Library
    register_builtin(scope, "os.output", TYPE_BOOLEAN, 1, TYPE_ANY);
    register_builtin(scope, "os.input", TYPE_STRING, 1, TYPE_STRING);
    register_builtin(scope, "os.read", TYPE_STRING, 1, TYPE_STRING);
    register_builtin(scope, "os.write", TYPE_BOOLEAN, 2, TYPE_STRING, TYPE_STRING);
    register_builtin(scope, "os.close", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.exists", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.isfile", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.isdir", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.rename", TYPE_BOOLEAN, 2, TYPE_STRING, TYPE_STRING);
    register_builtin(scope, "os.rmfile", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.mkfile", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.listdir", TYPE_TABLE, 1, TYPE_STRING);
    register_builtin(scope, "os.getcwd", TYPE_STRING, 0);
    register_builtin(scope, "os.chdir", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.mkdir", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.rmdir", TYPE_BOOLEAN, 1, TYPE_STRING);
    register_builtin(scope, "os.stat", TYPE_TABLE, 1, TYPE_STRING);
    register_builtin(scope, "os.exit", TYPE_BOOLEAN, 1, TYPE_NUMBER);
    register_builtin(scope, "os.wait", TYPE_BOOLEAN, 1, TYPE_NUMBER);
    register_builtin(scope, "os.time", TYPE_NUMBER, 0);
    register_builtin(scope, "os.system", TYPE_NUMBER, 1, TYPE_STRING);
    register_builtin(scope, "os.platform", TYPE_STRING, 0);
    
    // Math Library
    register_builtin(scope, "math.abs", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.floor", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.ceil", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.round", TYPE_NUMBER, 2, TYPE_NUMBER, TYPE_NUMBER);
    register_builtin(scope, "math.sqrt", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.exp", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.log", TYPE_NUMBER, 2, TYPE_NUMBER, TYPE_NUMBER);
    register_builtin(scope, "math.sin", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.cos", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.tan", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.asin", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.acos", TYPE_NUMBER, 1, TYPE_NUMBER);
    register_builtin(scope, "math.atan", TYPE_NUMBER, 1, TYPE_NUMBER);
    
    // String Library
    register_builtin(scope, "string.len", TYPE_NUMBER, 1, TYPE_STRING);
    register_builtin(scope, "string.lower", TYPE_STRING, 1, TYPE_STRING);
    register_builtin(scope, "string.upper", TYPE_STRING, 1, TYPE_STRING);
    register_builtin(scope, "string.sub", TYPE_STRING, 3, TYPE_STRING, TYPE_NUMBER, TYPE_NUMBER);
    register_builtin(scope, "string.split", TYPE_TABLE, 2, TYPE_STRING, TYPE_STRING);
    register_builtin(scope, "string.join", TYPE_STRING, 2, TYPE_TABLE, TYPE_STRING);
    register_builtin(scope, "string.trim", TYPE_STRING, 1, TYPE_STRING);
    register_builtin(scope, "string.find", TYPE_NUMBER, 2, TYPE_STRING, TYPE_STRING);
    register_builtin(scope, "string.replace", TYPE_STRING, 3, TYPE_STRING, TYPE_STRING, TYPE_STRING);
    
    // Table Library
    register_builtin(scope, "table.remove", TYPE_BOOLEAN, 2, TYPE_TABLE, TYPE_ANY);
    register_builtin(scope, "table.has", TYPE_BOOLEAN, 2, TYPE_TABLE, TYPE_ANY);
    register_builtin(scope, "table.size", TYPE_NUMBER, 1, TYPE_TABLE);
    register_builtin(scope, "table.keys", TYPE_TABLE, 1, TYPE_TABLE);
    register_builtin(scope, "table.values", TYPE_TABLE, 1, TYPE_TABLE);
    register_builtin(scope, "table.clear", TYPE_BOOLEAN, 1, TYPE_TABLE);
    register_builtin(scope, "table.copy", TYPE_TABLE, 1, TYPE_TABLE);
    register_builtin(scope, "table.merge", TYPE_TABLE, 2, TYPE_TABLE, TYPE_TABLE);
    
    // Conversion functions
    register_builtin(scope, "number", TYPE_NUMBER, 1, TYPE_STRING);
    register_builtin(scope, "string", TYPE_STRING, 1, TYPE_ANY);
}

// ========== Forward Declarations for Recursive Analysis ==========

static ValueType analyze_expression(SemAnalyzer* sema, ASTNode* node);
static void analyze_statement(SemAnalyzer* sema, ASTNode* node);
static void analyze_block(SemAnalyzer* sema, ASTNode* node);

// ========== Expression Analysis ==========

static ValueType analyze_binary_expr(SemAnalyzer* sema, ASTNode* node) {
    ValueType left_type = analyze_expression(sema, node->binary.left);
    ValueType right_type = analyze_expression(sema, node->binary.right);
    
    // If type is unknown (function parameter), skip the check
    if (left_type == TYPE_ANY || right_type == TYPE_ANY) {
        return TYPE_ANY;
    }
    if (left_type == TYPE_UNKNOWN || right_type == TYPE_UNKNOWN) {
        return TYPE_UNKNOWN;
    }
    
    switch (node->binary.op) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                sema_error(sema, node->line, node->column, false,
                    "Arithmetic operator '%s' requires number operands, got %s and %s",
                    token_type_name(node->binary.op),
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_NUMBER;
            
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
            // Equality comparison
            if (!is_comparable_type(left_type) || !is_comparable_type(right_type)) {
                sema_error(sema, node->line, node->column, false,
                    "Cannot compare types %s and %s",
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
            
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
            // Relational comparisons require numbers
            if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                sema_error(sema, node->line, node->column, false,
                    "Comparison operator '%s' requires number operands, got %s and %s",
                    token_type_name(node->binary.op),
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
            
        case TOKEN_AND:
        case TOKEN_OR:
            // Logical operators require boolean
            if (left_type != TYPE_BOOLEAN || right_type != TYPE_BOOLEAN) {
                sema_error(sema, node->line, node->column, false,
                    "Logical operator '%s' requires boolean operands, got %s and %s",
                    token_type_name(node->binary.op),
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
            
        default:
            break;
    }
    return TYPE_ERROR;
}

static ValueType analyze_unary_expr(SemAnalyzer* sema, ASTNode* node) {
    ValueType operand_type = analyze_expression(sema, node->unary.operand);
    
    switch (node->unary.op) {
        case TOKEN_MINUS:
            if (!is_numeric_type(operand_type)) {
                sema_error(sema, node->line, node->column, false,
                    "Unary minus requires number operand, got %s",
                    type_name(operand_type));
                return TYPE_ERROR;
            }
            return TYPE_NUMBER;
            
        case TOKEN_NOT:
            if (operand_type != TYPE_BOOLEAN) {
                sema_error(sema, node->line, node->column, false,
                    "Logical not requires boolean operand, got %s",
                    type_name(operand_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
            
        default:
            return TYPE_ERROR;
    }
}

static ValueType analyze_identifier(SemAnalyzer* sema, ASTNode* node) {
    const char* name = node->identifier.name;
    
    // Search for symbol in all visible scopes
    Symbol* sym = scope_lookup_recursive(sema->current_scope, name);
    
    if (!sym) {
        // Check builtins
        sym = scope_lookup_recursive(sema->builtins, name);
        if (!sym) {
            sema_error(sema, node->line, node->column, false,
                "Undefined variable or function '%s'", name);
            return TYPE_ERROR;
        }
    }
    
    return sym->value_type;
}

static ValueType analyze_call(SemAnalyzer* sema, ASTNode* node) {
    ValueType callee_type = analyze_expression(sema, node->call.callee);
    
    if (callee_type != TYPE_FUNCTION && callee_type != TYPE_UNKNOWN) {
        sema_error(sema, node->line, node->column, false,
            "Cannot call non-function value of type %s", type_name(callee_type));
        return TYPE_ERROR;
    }
    
    // Get function name for argument checking
    const char* func_name = NULL;
    ASTNode* callee = node->call.callee;
    
    // Resolve full name (module.function)
    char full_name[256] = "";
    if (callee->type == AST_IDENTIFIER) {
        func_name = callee->identifier.name;
    } else if (callee->type == AST_MEMBER_ACCESS) {
        // Build module.function string
        if (callee->access.object->type == AST_IDENTIFIER) {
            snprintf(full_name, sizeof(full_name), "%s.%s",
                     callee->access.object->identifier.name,
                     callee->access.member->identifier.name);
            func_name = full_name;
        }
    }
    
    // Check argument count for built-in functions
    if (func_name) {
        Symbol* sym = scope_lookup_recursive(sema->builtins, func_name);
        if (!sym) {
            sym = scope_lookup_recursive(sema->current_scope, func_name);
        }
        
        if (sym && sym->kind == SYMBOL_BUILTIN) {
            int expected = sym->func_info.param_count;
            int actual = node->call.arguments->count;
            
            if (expected != actual) {
                sema_error(sema, node->line, node->column, true,
                    "Function '%s' expects %d arguments, got %d",
                    func_name, expected, actual);
            }
            
            return sym->func_info.return_type;
        }
        
        if (sym && sym->kind == SYMBOL_FUNCTION) {
            int expected = sym->func_info.param_count;
            int actual = node->call.arguments->count;
            
            if (expected != actual) {
                sema_error(sema, node->line, node->column, false,
                    "Function '%s' expects %d arguments, got %d",
                    func_name, expected, actual);
            }
            
            return sym->func_info.return_type;
        }
    }
    
    // Analyze arguments
    for (int i = 0; i < node->call.arguments->count; i++) {
        analyze_expression(sema, node->call.arguments->nodes[i]);
    }
    
    return TYPE_UNKNOWN; // Return type unknown without function info
}

static ValueType analyze_member_access(SemAnalyzer* sema, ASTNode* node) {
    ValueType object_type = analyze_expression(sema, node->access.object);
    
    if (object_type != TYPE_TABLE && object_type != TYPE_UNKNOWN && 
        object_type != TYPE_ANY) {
        sema_error(sema, node->line, node->column, false,
            "Cannot access member of non-table type %s", type_name(object_type));
        return TYPE_ERROR;
    }
    
    // Table member type is unknown without structural information
    return TYPE_UNKNOWN;
}

static ValueType analyze_table_literal(SemAnalyzer* sema, ASTNode* node) {
    // Analyze ordered items
    for (int i = 0; i < node->table_literal.items->count; i++) {
        analyze_expression(sema, node->table_literal.items->nodes[i]);
    }
    
    // Analyze key-value pairs
    for (int i = 0; i < node->table_literal.key_values->count; i++) {
        ASTNode* kv = node->table_literal.key_values->nodes[i];
        // kv is a binary node: key on left, value on right
        analyze_expression(sema, kv->binary.left);   // key
        analyze_expression(sema, kv->binary.right);  // value
    }
    
    return TYPE_TABLE;
}

static ValueType analyze_string_interp(SemAnalyzer* sema, ASTNode* node) {
    for (int i = 0; i < node->string_interp.parts->count; i++) {
        ASTNode* part = node->string_interp.parts->nodes[i];
        if (part->type != AST_LITERAL_STRING) {
            analyze_expression(sema, part);
        }
    }
    return TYPE_STRING;
}

static ValueType analyze_expression(SemAnalyzer* sema, ASTNode* node) {
    if (!node) return TYPE_UNKNOWN;
    
    switch (node->type) {
        case AST_LITERAL_NUMBER:
            return TYPE_NUMBER;
        case AST_LITERAL_STRING:
            return TYPE_STRING;
        case AST_LITERAL_BOOL:
            return TYPE_BOOLEAN;
        case AST_IDENTIFIER:
            return analyze_identifier(sema, node);
        case AST_BINARY:
            return analyze_binary_expr(sema, node);
        case AST_UNARY:
            return analyze_unary_expr(sema, node);
        case AST_CALL:
            return analyze_call(sema, node);
        case AST_MEMBER_ACCESS:
        case AST_INDEX_ACCESS:
            return analyze_member_access(sema, node);
        case AST_TABLE_LITERAL:
            return analyze_table_literal(sema, node);
        case AST_STRING_INTERP:
            return analyze_string_interp(sema, node);
        case AST_FUNCTION_DECL:
            return TYPE_FUNCTION;
        default:
            sema_error(sema, node->line, node->column, false,
                "Unexpected expression type %d", node->type);
            return TYPE_ERROR;
    }
}

// ========== Statement Analysis ==========

static void analyze_var_decl(SemAnalyzer* sema, ASTNode* node) {
    ValueType value_type = analyze_expression(sema, node->var_assign.value);
    
    if (value_type == TYPE_ERROR) return;
    
    // Check if variable is already declared in current scope
    Symbol* existing = scope_lookup(sema->current_scope, node->var_assign.name);
    if (existing) {
        sema_error(sema, node->line, node->column, false,
            "Variable '%s' already declared in this scope", node->var_assign.name);
        return;
    }
    
    // Register the variable
    Symbol* sym = scope_insert(sema->current_scope, node->var_assign.name,
                                SYMBOL_VARIABLE, value_type,
                                node->line, node->column);
    if (!sym) {
        sema_error(sema, node->line, node->column, false,
            "Failed to declare variable '%s'", node->var_assign.name);
    }
}

static void analyze_assign(SemAnalyzer* sema, ASTNode* node) {
    // Look up the variable
    Symbol* sym = scope_lookup_recursive(sema->current_scope, node->var_assign.name);
    
    if (!sym) {
        sema_error(sema, node->line, node->column, false,
            "Assignment to undefined variable '%s'", node->var_assign.name);
        return;
    }
    
    if (sym->kind != SYMBOL_VARIABLE && sym->kind != SYMBOL_PARAMETER) {
        sema_error(sema, node->line, node->column, false,
            "Cannot assign to '%s' (not a variable)", node->var_assign.name);
        return;
    }
    
    ValueType new_type = analyze_expression(sema, node->var_assign.value);
    
    // Type immutability check (Apex rule)
    if (sym->is_initialized && new_type != TYPE_UNKNOWN && 
        sym->value_type != TYPE_UNKNOWN && new_type != sym->value_type) {
        sema_error(sema, node->line, node->column, false,
            "Cannot change type of variable '%s' from %s to %s",
            sym->name, type_name(sym->value_type), type_name(new_type));
        return;
    }
    
    // Update type (only if it was UNKNOWN)
    if (sym->value_type == TYPE_UNKNOWN) {
        sym->value_type = new_type;
    }
    
    sym->is_initialized = true;
}

static void analyze_if_stmt(SemAnalyzer* sema, ASTNode* node) {
    ValueType cond_type = analyze_expression(sema, node->if_stmt.condition);
    
    // TYPE_ANY is acceptable (function parameters)
    if (cond_type != TYPE_BOOLEAN && cond_type != TYPE_ANY && cond_type != TYPE_UNKNOWN) {
        sema_error(sema, node->line, node->column, false,
            "If condition must be boolean, got %s", type_name(cond_type));
    }
    
    analyze_block(sema, node->if_stmt.then_branch);
    
    if (node->if_stmt.elif_chain) {
        analyze_statement(sema, node->if_stmt.elif_chain);
    }
    
    if (node->if_stmt.else_branch) {
        analyze_block(sema, node->if_stmt.else_branch);
    }
}

static void analyze_while_stmt(SemAnalyzer* sema, ASTNode* node) {
    ValueType cond_type = analyze_expression(sema, node->while_stmt.condition);
    
    if (cond_type != TYPE_BOOLEAN && cond_type != TYPE_ANY && cond_type != TYPE_UNKNOWN) {
        sema_error(sema, node->line, node->column, false,
            "While condition must be boolean, got %s", type_name(cond_type));
    }
    
    // Save context
    bool prev_in_loop = sema->in_loop;
    sema->in_loop = true;
    
    Scope* loop_scope = scope_create(sema->current_scope,
                                      sema->current_scope->level + 1);
    loop_scope->is_loop_scope = true;
    sema->current_scope = loop_scope;
    
    analyze_block(sema, node->while_stmt.body);
    
    sema->current_scope = loop_scope->parent;
    scope_destroy(loop_scope);
    sema->in_loop = prev_in_loop;
}

static void analyze_for_stmt(SemAnalyzer* sema, ASTNode* node) {
    ValueType start_type = analyze_expression(sema, node->for_stmt.start);
    if (start_type != TYPE_NUMBER && start_type != TYPE_ANY && start_type != TYPE_UNKNOWN) {
        sema_error(sema, node->line, node->column, false,
                   "For loop start must be a number, got %s", type_name(start_type));
    }

    ValueType end_type = analyze_expression(sema, node->for_stmt.end);
    if (end_type != TYPE_NUMBER && end_type != TYPE_ANY && end_type != TYPE_UNKNOWN) {
        sema_error(sema, node->line, node->column, false,
                   "For loop end must be a number, got %s", type_name(end_type));
    }

    if (node->for_stmt.step) {
        ValueType step_type = analyze_expression(sema, node->for_stmt.step);
        if (step_type != TYPE_NUMBER && step_type != TYPE_ANY && step_type != TYPE_UNKNOWN) {
            sema_error(sema, node->line, node->column, false,
                       "For loop step must be a number, got %s", type_name(step_type));
        }
    }

    bool prev_in_loop = sema->in_loop;
    sema->in_loop = true;
    Scope* loop_scope = scope_create(sema->current_scope, sema->current_scope->level + 1);
    loop_scope->is_loop_scope = true;
    sema->current_scope = loop_scope;

    scope_insert(sema->current_scope, node->for_stmt.var_name,
                 SYMBOL_VARIABLE, TYPE_NUMBER, node->line, node->column);

    analyze_block(sema, node->for_stmt.body);
    sema->current_scope = loop_scope->parent;
    scope_destroy(loop_scope);
    sema->in_loop = prev_in_loop;
}

static void analyze_function_decl(SemAnalyzer* sema, ASTNode* node) {
    Symbol* func_sym = scope_insert(sema->current_scope,
                                    node->function_decl.name,
                                    SYMBOL_FUNCTION, TYPE_FUNCTION,
                                    node->line, node->column);
    if (!func_sym) {
        sema_error(sema, node->line, node->column, false,
                   "Function '%s' already declared in this scope",
                   node->function_decl.name);
        return;
    }
    
    Scope* func_scope = scope_create(sema->current_scope,
                                     sema->current_scope->level + 1);
    func_scope->is_function_scope = true;
    sema->current_scope = func_scope;
    bool prev_in_function = sema->in_function;
    sema->in_function = true;
    
    int param_count = node->function_decl.params->count;
    func_sym->func_info.param_count = param_count;
    func_sym->func_info.param_types = (ValueType*)malloc(sizeof(ValueType) * param_count);

    for (int i = 0; i < param_count; i++) {
        ASTNode* param = node->function_decl.params->nodes[i];
        func_sym->func_info.param_types[i] = TYPE_ANY;

        Symbol* param_sym = scope_insert(sema->current_scope, param->param.name,
                                         SYMBOL_PARAMETER, TYPE_ANY,
                                         param->line, param->column);
        if (param_sym) {
            param_sym->is_initialized = true;
        }
    }
    
    analyze_block(sema, node->function_decl.body);
    func_sym->func_info.has_return = true;

    sema->current_scope = func_scope->parent;
    sema->in_function = prev_in_function;
    scope_destroy(func_scope);
}

static void analyze_return_stmt(SemAnalyzer* sema, ASTNode* node) {
    if (!sema->in_function) {
        sema_error(sema, node->line, node->column, false,
            "Return statement outside of function");
        return;
    }
    
    if (node->return_stmt.value) {
        analyze_expression(sema, node->return_stmt.value);
    }
}

static void analyze_break_continue(SemAnalyzer* sema, ASTNode* node) {
    if (!sema->in_loop) {
        const char* keyword = node->type == AST_BREAK_STMT ? "break" : "continue";
        sema_error(sema, node->line, node->column, false,
            "'%s' statement outside of loop", keyword);
    }
}

static void analyze_import_stmt(SemAnalyzer* sema, ASTNode* node) {
    const char* full_path = node->import_stmt.module_path;
    
    // Split path into parts
    char path_copy[1024];
    strcpy(path_copy, full_path);
    
    char* parts[64];
    int part_count = 0;
    
    char* token = strtok(path_copy, ".");
    while (token && part_count < 64) {
        parts[part_count++] = token;
        token = strtok(NULL, ".");
    }
    
    if (part_count == 0) return;
    
    // Register only the first level as a global
    Symbol* existing = scope_lookup(sema->current_scope, parts[0]);
    if (!existing) {
        scope_insert(sema->current_scope, parts[0],
                     SYMBOL_MODULE, TYPE_TABLE,
                     node->line, node->column);
    }
}

static void analyze_statement(SemAnalyzer* sema, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_VAR_DECL:
            analyze_var_decl(sema, node);
            break;
        case AST_ASSIGN:
            analyze_assign(sema, node);
            break;
        case AST_IF_STMT:
            analyze_if_stmt(sema, node);
            break;
        case AST_WHILE_STMT:
            analyze_while_stmt(sema, node);
            break;
        case AST_FOR_STMT:
            analyze_for_stmt(sema, node);
            break;
        case AST_FUNCTION_DECL:
            analyze_function_decl(sema, node);
            break;
        case AST_RETURN_STMT:
            analyze_return_stmt(sema, node);
            break;
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            analyze_break_continue(sema, node);
            break;
        case AST_IMPORT_STMT:
            analyze_import_stmt(sema, node);
            break;
        case AST_EXPR_STMT:
            analyze_expression(sema, node->expr_stmt.expression);
            break;
        case AST_BLOCK:
            analyze_block(sema, node);
            break;
        default:
            sema_error(sema, node->line, node->column, false,
                "Unexpected statement type %d", node->type);
            break;
    }
}

static void analyze_block(SemAnalyzer* sema, ASTNode* node) {
    if (!node || node->type != AST_BLOCK) return;
    
    for (int i = 0; i < node->block.statements->count; i++) {
        analyze_statement(sema, node->block.statements->nodes[i]);
    }
}

// ========== Public API ==========

SemAnalyzer* sema_create(const char* filename) {
    SemAnalyzer* sema = (SemAnalyzer*)calloc(1, sizeof(SemAnalyzer));
    sema->filename = strdup(filename);
    
    // Create global scope
    sema->global_scope = scope_create(NULL, 0);
    sema->current_scope = sema->global_scope;
    
    // Create built-in function scope
    sema->builtins = scope_create(NULL, -1);
    register_all_builtins(sema->builtins);
    
    return sema;
}

void sema_destroy(SemAnalyzer* sema) {
    if (!sema) return;
    
    scope_destroy(sema->global_scope);
    scope_destroy(sema->builtins);
    free(sema->filename);
    
    // Clear errors
    for (int i = 0; i < error_idx; i++) {
        free(errors[i].message);
    }
    error_idx = 0;
    
    free(sema);
}

bool sema_analyze(SemAnalyzer* sema, ASTNode* ast) {
    if (!sema || !ast) return false;
    
    sema->ast = ast;
    sema->error_count = 0;
    sema->warning_count = 0;
    error_idx = 0;
    
    if (ast->type == AST_PROGRAM) {
        for (int i = 0; i < ast->block.statements->count; i++) {
            analyze_statement(sema, ast->block.statements->nodes[i]);
        }
    }
    
    // Print errors only if present
    if (sema->error_count > 0 || sema->warning_count > 0) {
        sema_print_errors(sema);
    }
    
    return sema->error_count == 0;
}

int sema_get_error_count(SemAnalyzer* sema) {
    return sema ? sema->error_count : -1;
}

void sema_print_symbols(SemAnalyzer* sema) {
    printf("\n=== Symbol Table ===\n");
    
    Scope* scope = sema->global_scope;
    while (scope) {
        printf("\nScope level %d:\n", scope->level);
        for (int i = 0; i < scope->capacity; i++) {
            Symbol* sym = scope->symbols[i];
            while (sym) {
                printf("  %-20s kind=%-12s type=%-10s line=%d\n",
                       sym->name,
                       sym->kind == SYMBOL_VARIABLE ? "variable" :
                       sym->kind == SYMBOL_FUNCTION ? "function" :
                       sym->kind == SYMBOL_PARAMETER ? "parameter" :
                       sym->kind == SYMBOL_MODULE ? "module" : "builtin",
                       type_name(sym->value_type),
                       sym->line);
                sym = sym->next;
            }
        }
        scope = scope->parent;
    }
}
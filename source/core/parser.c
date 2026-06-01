#include "parser.h"
#include "execute.h"
#include "error.h"
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>
#ifdef _WIN32
    #include <io.h>
    #ifndef F_OK
        #define F_OK 0
    #endif
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
#else
    #include <unistd.h>
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
#endif

// ========== Forward Declarations ==========
static Token* current_token(Parser* parser);
static ASTNode* parse_program(Parser* parser);
static ASTNode* parse_statement(Parser* parser);
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_block(Parser* parser, bool expect_newlines);
static ASTNode* parse_string_expression(Parser* parser, const char* expr_str, int line, int column);
static ValueType infer_expression_type(Parser* parser, ASTNode* node);
static int symbol_index_recursive(Parser* parser, const char* name);

// ========== Built-in function signatures ==========

typedef struct {
    const char* name;
    int param_count;
} BuiltinSig;

static const BuiltinSig BUILTINS[] = {
    {"os.output", 1}, {"os.input", 1}, {"os.read", 1}, {"os.write", 2},
    {"os.close", 1}, {"os.exists", 1}, {"os.isfile", 1}, {"os.isdir", 1},
    {"os.rename", 2}, {"os.rmfile", 1}, {"os.mkfile", 1}, {"os.listdir", 1},
    {"os.getcwd", 0}, {"os.chdir", 1}, {"os.mkdir", 1}, {"os.rmdir", 1},
    {"os.stat", 1}, {"os.exit", 1}, {"os.wait", 1}, {"os.time", 0},
    {"os.system", 1}, {"os.platform", 0},
    {"math.abs", 1}, {"math.floor", 1}, {"math.ceil", 1}, {"math.round", 2},
    {"math.sqrt", 1}, {"math.exp", 1}, {"math.log", 2}, {"math.sin", 1},
    {"math.cos", 1}, {"math.tan", 1}, {"math.asin", 1}, {"math.acos", 1},
    {"math.atan", 1},
    {"string.len", 1}, {"string.lower", 1}, {"string.upper", 1},
    {"string.sub", 3}, {"string.split", 2}, {"string.join", 2},
    {"string.trim", 1}, {"string.find", 2}, {"string.replace", 3},
    {"table.remove", 2}, {"table.has", 2}, {"table.size", 1},
    {"table.keys", 1}, {"table.values", 1}, {"table.clear", 1},
    {"table.copy", 1}, {"table.merge", 2},
    {"number", 1}, {"string", 1},
};

static const BuiltinSig* lookup_builtin(const char* name) {
    for (size_t i = 0; i < sizeof(BUILTINS) / sizeof(BUILTINS[0]); i++) {
        if (strcmp(BUILTINS[i].name, name) == 0) {
            return &BUILTINS[i];
        }
    }
    return NULL;
}

// ========== Error reporting ==========

void parser_error_at(Parser* parser, int line, int column, int len,
                     const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    parser->error_count++;
    print_error_with_context(parser->filename, parser->source,
                             line, column, len, "Parse Error", buffer);
    throw_repl_error();
}

void parser_error(Parser* parser, const char* message) {
    Token* token = current_token(parser);
    int len = token->value ? (int)strlen(token->value) : 1;
    parser_error_at(parser, token->line, token->column, len, "%s", message);
}

bool parser_had_errors(const Parser* parser) {
    return parser->error_count > 0;
}

// ========== Type utilities ==========

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

static bool is_numeric_type(ValueType type) {
    return type == TYPE_NUMBER;
}

static bool is_comparable_type(ValueType type) {
    return type == TYPE_NUMBER || type == TYPE_STRING || type == TYPE_BOOLEAN;
}

static const char* binary_op_name(TokenType op) {
    switch (op) {
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EQUAL_EQUAL: return "==";
        case TOKEN_NOT_EQUAL: return "!=";
        case TOKEN_LESS: return "<";
        case TOKEN_GREATER: return ">";
        case TOKEN_LESS_EQUAL: return "<=";
        case TOKEN_GREATER_EQUAL: return ">=";
        case TOKEN_AND: return "and";
        case TOKEN_OR: return "or";
        default: return token_type_name(op);
    }
}

static void parser_set_source_dir(Parser* parser, const char* filename) {
    parser->source_dir = (char*)malloc(PATH_MAX);
    if (!parser->source_dir) return;

    if (!filename || filename[0] == '\0' ||
        strcmp(filename, "stdin") == 0 ||
        strcmp(filename, "<interpolation>") == 0) {
        strncpy(parser->source_dir, ".", PATH_MAX - 1);
        parser->source_dir[PATH_MAX - 1] = '\0';
        return;
    }

    strncpy(parser->source_dir, filename, PATH_MAX - 1);
    parser->source_dir[PATH_MAX - 1] = '\0';
    char* slash = strrchr(parser->source_dir, '/');
    if (slash) {
        *slash = '\0';
    } else {
        strncpy(parser->source_dir, ".", PATH_MAX - 1);
    }
}

static bool parser_is_zero_constant(Parser* parser, ASTNode* node) {
    if (!node) return false;

    if (node->type == AST_LITERAL_NUMBER) {
        return node->literal_number.number_value == 0.0;
    }
    if (node->type == AST_UNARY && node->unary.op == TOKEN_MINUS) {
        ASTNode* operand = node->unary.operand;
        if (operand && operand->type == AST_LITERAL_NUMBER) {
            return operand->literal_number.number_value == 0.0;
        }
    }
    if (node->type == AST_IDENTIFIER) {
        int idx = symbol_index_recursive(parser, node->identifier.name);
        if (idx >= 0 && parser->symbols.const_known[idx]) {
            return parser->symbols.const_values[idx] == 0.0;
        }
    }
    return false;
}

static void parser_check_divisor(Parser* parser, ASTNode* node, ASTNode* divisor) {
    if (!parser->semantic_checks || !divisor) return;
    if (parser_is_zero_constant(parser, divisor)) {
        parser_error_at(parser, node->line, node->column, 0,
                        "Division by zero");
    }
}

static bool expr_has_side_effect(ASTNode* node) {
    if (!node) return false;

    switch (node->type) {
        case AST_CALL:
            return true;
        case AST_BINARY:
            return expr_has_side_effect(node->binary.left) ||
                   expr_has_side_effect(node->binary.right);
        case AST_UNARY:
            return expr_has_side_effect(node->unary.operand);
        case AST_MEMBER_ACCESS:
        case AST_INDEX_ACCESS:
            return expr_has_side_effect(node->access.object);
        case AST_STRING_INTERP: {
            for (int i = 0; i < node->string_interp.parts->count; i++) {
                if (expr_has_side_effect(node->string_interp.parts->nodes[i])) {
                    return true;
                }
            }
            return false;
        }
        case AST_TABLE_LITERAL: {
            for (int i = 0; i < node->table_literal.items->count; i++) {
                if (expr_has_side_effect(node->table_literal.items->nodes[i])) {
                    return true;
                }
            }
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                if (expr_has_side_effect(kv->binary.left) ||
                    expr_has_side_effect(kv->binary.right)) {
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

static void parser_check_expr_statement(Parser* parser, ASTNode* expr) {
    if (!parser->semantic_checks || !expr) return;
    if (!expr_has_side_effect(expr)) {
        parser_error_at(parser, expr->line, expr->column, 0,
                        "Expression statement has no effect");
    }
}

static bool is_builtin_module_root(const char* name) {
    return strcmp(name, "os") == 0 || strcmp(name, "math") == 0 ||
           strcmp(name, "string") == 0 || strcmp(name, "table") == 0;
}

static void parser_validate_import_file(Parser* parser, const char* module_path,
                                        int line, int column) {
    if (!parser->semantic_checks || !parser->source_dir || !module_path) return;
    if (strcmp(parser->filename, "stdin") == 0 ||
        strcmp(parser->filename, "<interpolation>") == 0) {
        return;
    }

    char first_segment[256];
    const char* dot = strchr(module_path, '.');
    size_t first_len = dot ? (size_t)(dot - module_path) : strlen(module_path);
    if (first_len >= sizeof(first_segment)) first_len = sizeof(first_segment) - 1;
    memcpy(first_segment, module_path, first_len);
    first_segment[first_len] = '\0';

    if (is_builtin_module_root(first_segment)) {
        return;
    }

    char relative[1024];
    size_t len = 0;
    relative[0] = '\0';
    char path_copy[1024];
    strncpy(path_copy, module_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* segment = strtok(path_copy, ".");
    while (segment) {
        if (len > 0) {
            if (len + 1 < sizeof(relative)) relative[len++] = '/';
        }
        size_t seg_len = strlen(segment);
        if (len + seg_len >= sizeof(relative)) return;
        memcpy(relative + len, segment, seg_len);
        len += seg_len;
        relative[len] = '\0';
        segment = strtok(NULL, ".");
    }

    if (len + 5 >= sizeof(relative)) return;
    strcat(relative, ".apex");

    char full_path[PATH_MAX];
    if (snprintf(full_path, sizeof(full_path), "%s/%s", parser->source_dir, relative) >=
        (int)sizeof(full_path)) {
        return;
    }
    
    #ifdef _WIN32
    if (_access(full_path, F_OK) != 0) {
    #else
    if (access(full_path, F_OK) != 0) {
    #endif
        parser_error_at(parser, line, column, (int)strlen(module_path),
                "Module '%s' not found (expected '%s')",
                module_path, relative);
    }
}

static void parser_symbol_set_const(Parser* parser, int idx, bool known, double value) {
    if (idx < 0) return;
    parser->symbols.const_known[idx] = known;
    parser->symbols.const_values[idx] = value;
}

static void parser_symbol_clear_const(Parser* parser, int idx) {
    parser_symbol_set_const(parser, idx, false, 0.0);
}

// ========== Symbol Management ==========

static void symbols_grow(Parser* parser) {
    if (parser->symbols.count < parser->symbols.capacity) return;
    parser->symbols.capacity = parser->symbols.capacity == 0 ? 16 : parser->symbols.capacity * 2;
    ParserSymbolTable* s = &parser->symbols;
    s->names = (char**)realloc(s->names, sizeof(char*) * s->capacity);
    s->scope_levels = (int*)realloc(s->scope_levels, sizeof(int) * s->capacity);
    s->kinds = (ParserSymbolKind*)realloc(s->kinds, sizeof(ParserSymbolKind) * s->capacity);
    s->types = (ValueType*)realloc(s->types, sizeof(ValueType) * s->capacity);
    s->param_counts = (int*)realloc(s->param_counts, sizeof(int) * s->capacity);
    s->const_known = (bool*)realloc(s->const_known, sizeof(bool) * s->capacity);
    s->const_values = (double*)realloc(s->const_values, sizeof(double) * s->capacity);
}

static int symbol_index_in_scope(Parser* parser, const char* name, int scope) {
    for (int i = 0; i < parser->symbols.count; i++) {
        if (parser->symbols.scope_levels[i] == scope &&
            strcmp(parser->symbols.names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int symbol_index_recursive(Parser* parser, const char* name) {
    for (int scope = parser->symbols.current_scope; scope >= 0; scope--) {
        int idx = symbol_index_in_scope(parser, name, scope);
        if (idx >= 0) return idx;
    }
    return -1;
}

void parser_enter_scope(Parser* parser) {
    parser->symbols.current_scope++;
}

void parser_exit_scope(Parser* parser) {
    int i = 0;
    while (i < parser->symbols.count) {
        if (parser->symbols.scope_levels[i] == parser->symbols.current_scope) {
            free(parser->symbols.names[i]);
            for (int j = i; j < parser->symbols.count - 1; j++) {
                parser->symbols.names[j] = parser->symbols.names[j + 1];
                parser->symbols.scope_levels[j] = parser->symbols.scope_levels[j + 1];
                parser->symbols.kinds[j] = parser->symbols.kinds[j + 1];
                parser->symbols.types[j] = parser->symbols.types[j + 1];
                parser->symbols.param_counts[j] = parser->symbols.param_counts[j + 1];
                parser->symbols.const_known[j] = parser->symbols.const_known[j + 1];
                parser->symbols.const_values[j] = parser->symbols.const_values[j + 1];
            }
            parser->symbols.count--;
        } else {
            i++;
        }
    }
    parser->symbols.current_scope--;
}

bool parser_declare_symbol(Parser* parser, const char* name, ParserSymbolKind kind,
                           ValueType type, int param_count, int line, int column) {
    if (symbol_index_in_scope(parser, name, parser->symbols.current_scope) >= 0) {
        const char* what = kind == PARSER_SYM_FUNCTION ? "Function" : "Variable";
        parser_error_at(parser, line, column, (int)strlen(name),
                        "%s '%s' already declared in this scope", what, name);
        return false;
    }

    symbols_grow(parser);
    int i = parser->symbols.count++;
    parser->symbols.names[i] = strdup(name);
    parser->symbols.scope_levels[i] = parser->symbols.current_scope;
    parser->symbols.kinds[i] = kind;
    parser->symbols.types[i] = type;
    parser->symbols.param_counts[i] = param_count;
    parser->symbols.const_known[i] = false;
    parser->symbols.const_values[i] = 0.0;
    return true;
}

bool parser_is_declared(Parser* parser, const char* name) {
    return symbol_index_recursive(parser, name) >= 0;
}

static void parser_register_import(Parser* parser, const char* module_path, int line, int column) {
    char path_copy[1024];
    strncpy(path_copy, module_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    char* token = strtok(path_copy, ".");
    if (!token) return;
    if (symbol_index_recursive(parser, token) < 0) {
        parser_declare_symbol(parser, token, PARSER_SYM_MODULE, TYPE_TABLE, 0, line, column);
    }
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
    parser->symbols.kinds = NULL;
    parser->symbols.types = NULL;
    parser->symbols.param_counts = NULL;
    parser->symbols.const_known = NULL;
    parser->symbols.const_values = NULL;
    parser->symbols.count = 0;
    parser->symbols.capacity = 0;
    parser->symbols.current_scope = 0;
    parser->error_count = 0;
    parser->loop_depth = 0;
    parser->function_depth = 0;
    parser->semantic_checks = true;
    parser->source = source;
    parser->source_dir = NULL;
    parser_set_source_dir(parser, filename);

    return parser;
}

void parser_destroy(Parser* parser) {
    if (parser) {
        free(parser->filename);
        free(parser->source_dir);

        // Free symbol table
        for (int i = 0; i < parser->symbols.count; i++) {
            free(parser->symbols.names[i]);
        }
        free(parser->symbols.names);
        free(parser->symbols.scope_levels);
        free(parser->symbols.kinds);
        free(parser->symbols.types);
        free(parser->symbols.param_counts);
        free(parser->symbols.const_known);
        free(parser->symbols.const_values);

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

// ========== Semantic checks during parse ==========

static ValueType infer_binary_type(Parser* parser, ASTNode* node) {
    ValueType left_type = infer_expression_type(parser, node->binary.left);
    ValueType right_type = infer_expression_type(parser, node->binary.right);

    if (left_type == TYPE_ANY || right_type == TYPE_ANY) return TYPE_ANY;
    if (left_type == TYPE_UNKNOWN || right_type == TYPE_UNKNOWN) return TYPE_UNKNOWN;

    switch (node->binary.op) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                parser_error_at(parser, node->line, node->column, 0,
                    "Arithmetic operator '%s' requires number operands, got %s and %s",
                    binary_op_name(node->binary.op),
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            if (node->binary.op == TOKEN_SLASH || node->binary.op == TOKEN_PERCENT) {
                parser_check_divisor(parser, node, node->binary.right);
            }
            return TYPE_NUMBER;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
            if (!is_comparable_type(left_type) || !is_comparable_type(right_type)) {
                parser_error_at(parser, node->line, node->column, 0,
                    "Cannot compare types %s and %s",
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
            if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                parser_error_at(parser, node->line, node->column, 0,
                    "Comparison operator '%s' requires number operands, got %s and %s",
                    binary_op_name(node->binary.op),
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
        case TOKEN_AND:
        case TOKEN_OR:
            if (left_type != TYPE_BOOLEAN || right_type != TYPE_BOOLEAN) {
                parser_error_at(parser, node->line, node->column, 0,
                    "Logical operator '%s' requires boolean operands, got %s and %s",
                    binary_op_name(node->binary.op),
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
        default:
            parser_error_at(parser, node->line, node->column, 0,
                            "Unknown binary operator");
            return TYPE_ERROR;
    }
}

static ValueType infer_unary_type(Parser* parser, ASTNode* node) {
    ValueType operand_type = infer_expression_type(parser, node->unary.operand);
    switch (node->unary.op) {
        case TOKEN_MINUS:
            if (!is_numeric_type(operand_type)) {
                parser_error_at(parser, node->line, node->column, 0,
                    "Unary minus requires number operand, got %s",
                    type_name(operand_type));
                return TYPE_ERROR;
            }
            return TYPE_NUMBER;
        case TOKEN_NOT:
            if (operand_type != TYPE_BOOLEAN) {
                parser_error_at(parser, node->line, node->column, 0,
                    "Logical not requires boolean operand, got %s",
                    type_name(operand_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
        default:
            return TYPE_ERROR;
    }
}

static const char* resolve_call_name(ASTNode* callee, char* buffer, size_t buflen) {
    if (callee->type == AST_IDENTIFIER) {
        return callee->identifier.name;
    }
    if (callee->type == AST_MEMBER_ACCESS &&
        callee->access.object->type == AST_IDENTIFIER) {
        snprintf(buffer, buflen, "%s.%s",
                 callee->access.object->identifier.name,
                 callee->access.member->identifier.name);
        return buffer;
    }
    return NULL;
}

static ValueType infer_call_type(Parser* parser, ASTNode* node) {
    ValueType callee_type = infer_expression_type(parser, node->call.callee);
    if (callee_type != TYPE_FUNCTION && callee_type != TYPE_UNKNOWN &&
        callee_type != TYPE_ANY) {
        int callee_len = (node->call.callee->type == AST_IDENTIFIER) ?
            (int)strlen(node->call.callee->identifier.name) : 0;
        parser_error_at(parser, node->line, node->column, callee_len,
            "Cannot call non-function value of type %s", type_name(callee_type));
        return TYPE_ERROR;
    }

    char full_name[256] = "";
    const char* func_name = resolve_call_name(node->call.callee, full_name, sizeof(full_name));
    if (func_name) {
        const BuiltinSig* builtin = lookup_builtin(func_name);
        if (builtin) {
            int actual = node->call.arguments->count;
            if (builtin->param_count != actual) {
                parser_error_at(parser, node->line, node->column, 1,
                    "Function '%s' expects %d arguments, got %d",
                    func_name, builtin->param_count, actual);
            }
            return TYPE_ANY;
        }
        int sym_idx = symbol_index_recursive(parser, func_name);
        if (sym_idx >= 0 && parser->symbols.kinds[sym_idx] == PARSER_SYM_FUNCTION) {
            int expected = parser->symbols.param_counts[sym_idx];
            int actual = node->call.arguments->count;
            if (expected != actual) {
                parser_error_at(parser, node->line, node->column, 0,
                    "Function '%s' expects %d arguments, got %d",
                    func_name, expected, actual);
            }
            return parser->symbols.types[sym_idx];
        }
    }

    for (int i = 0; i < node->call.arguments->count; i++) {
        infer_expression_type(parser, node->call.arguments->nodes[i]);
    }
    return TYPE_UNKNOWN;
}

static ValueType infer_member_access_type(Parser* parser, ASTNode* node) {
    ValueType object_type = infer_expression_type(parser, node->access.object);
    if (object_type == TYPE_ERROR) return TYPE_ERROR;
    if (object_type != TYPE_TABLE && object_type != TYPE_UNKNOWN && object_type != TYPE_ANY) {
        int obj_len = 0;
        if (node->access.object->type == AST_IDENTIFIER) {
            obj_len = (int)strlen(node->access.object->identifier.name);
        }
        parser_error_at(parser, node->line, node->column, obj_len,
            "Cannot access member of non-table type %s", type_name(object_type));
        return TYPE_ERROR;
    }
    return TYPE_UNKNOWN;
}

static ValueType infer_expression_type(Parser* parser, ASTNode* node) {
    if (!node) return TYPE_UNKNOWN;

    switch (node->type) {
        case AST_LITERAL_NUMBER: return TYPE_NUMBER;
        case AST_LITERAL_STRING: return TYPE_STRING;
        case AST_LITERAL_BOOL: return TYPE_BOOLEAN;
        case AST_IDENTIFIER: {
            const char* name = node->identifier.name;
            int idx = symbol_index_recursive(parser, name);
            if (idx < 0 && !lookup_builtin(name)) {
                parser_error_at(parser, node->line, node->column, (int)strlen(name),
                    "Undefined variable or function '%s'", name);
                return TYPE_ERROR;
            }
            if (idx >= 0) return parser->symbols.types[idx];
            return TYPE_ANY;
        }
        case AST_BINARY: return infer_binary_type(parser, node);
        case AST_UNARY: return infer_unary_type(parser, node);
        case AST_CALL: return infer_call_type(parser, node);
        case AST_MEMBER_ACCESS:
        case AST_INDEX_ACCESS: return infer_member_access_type(parser, node);
        case AST_TABLE_LITERAL: {
            for (int i = 0; i < node->table_literal.items->count; i++) {
                infer_expression_type(parser, node->table_literal.items->nodes[i]);
            }
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                infer_expression_type(parser, kv->binary.left);
                infer_expression_type(parser, kv->binary.right);
            }
            return TYPE_TABLE;
        }
        case AST_STRING_INTERP: {
            for (int i = 0; i < node->string_interp.parts->count; i++) {
                ASTNode* part = node->string_interp.parts->nodes[i];
                if (part->type != AST_LITERAL_STRING) {
                    infer_expression_type(parser, part);
                }
            }
            return TYPE_STRING;
        }
        case AST_FUNCTION_DECL: return TYPE_FUNCTION;
        default:
            parser_error_at(parser, node->line, node->column, 0,
                "Unexpected expression type %d", node->type);
            return TYPE_ERROR;
    }
}

ValueType parser_check_expression(Parser* parser, ASTNode* node) {
    if (!parser->semantic_checks) return TYPE_UNKNOWN;
    return infer_expression_type(parser, node);
}

static void parser_check_condition(Parser* parser, ASTNode* condition, const char* context) {
    if (!parser->semantic_checks) return;
    ValueType cond_type = infer_expression_type(parser, condition);
    if (cond_type != TYPE_BOOLEAN && cond_type != TYPE_ANY && cond_type != TYPE_UNKNOWN) {
        parser_error_at(parser, condition->line, condition->column, 0,
            "%s condition must be boolean, got %s", context, type_name(cond_type));
    }
}

static void parser_check_number_expr(Parser* parser, ASTNode* expr, const char* context) {
    if (!parser->semantic_checks) return;
    ValueType t = infer_expression_type(parser, expr);
    if (t != TYPE_NUMBER && t != TYPE_ANY && t != TYPE_UNKNOWN) {
        parser_error_at(parser, expr->line, expr->column, 0,
            "%s must be a number, got %s", context, type_name(t));
    }
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
    temp_parser->semantic_checks = false;
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

    ASTNode* expr = parse_precedence(parser, PREC_ASSIGNMENT);
    if (expr) {
        parser_check_expression(parser, expr);
    }
    return expr;
}

// ========== Statement Parsing ==========

static ASTNode* parse_var_decl_or_assign(Parser* parser) {
    Token* name = current_token(parser);
    advance(parser);

    consume(parser, TOKEN_EQUAL, "Expected '=' in assignment");
    ASTNode* value = parse_expression(parser);

    bool is_declaration = !parser_is_declared(parser, name->value);
    int current_idx = symbol_index_in_scope(parser, name->value, parser->symbols.current_scope);

    if (!parser->semantic_checks) {
        return ast_create_var_assign(name->value, value, is_declaration, NULL,
                                    name->line, name->column);
    }

    if (is_declaration) {
        if (current_idx >= 0) {
            parser_error_at(parser, name->line, name->column, (int)strlen(name->value),
                "Variable '%s' already declared in this scope", name->value);
        } else if (value) {
            ValueType value_type = infer_expression_type(parser, value);
            if (value_type != TYPE_ERROR) {
                parser_declare_symbol(parser, name->value, PARSER_SYM_VARIABLE,
                                      value_type, 0, name->line, name->column);
                int idx = symbol_index_recursive(parser, name->value);
                if (idx >= 0 && value->type == AST_LITERAL_NUMBER) {
                    parser_symbol_set_const(parser, idx, true,
                                            value->literal_number.number_value);
                }
            }
        }
    } else {
        int idx = symbol_index_recursive(parser, name->value);
        if (idx < 0) {
            parser_error_at(parser, name->line, name->column, (int)strlen(name->value),
                "Assignment to undefined variable '%s'", name->value);
        } else if (parser->symbols.kinds[idx] != PARSER_SYM_VARIABLE &&
                   parser->symbols.kinds[idx] != PARSER_SYM_PARAMETER) {
            parser_error_at(parser, name->line, name->column, (int)strlen(name->value),
                "Cannot assign to '%s' (not a variable)", name->value);
        } else if (value) {
            ValueType new_type = infer_expression_type(parser, value);
            ValueType old_type = parser->symbols.types[idx];
            if (old_type != TYPE_UNKNOWN && new_type != TYPE_UNKNOWN &&
                new_type != TYPE_ERROR && old_type != new_type) {
                parser_error_at(parser, name->line, name->column, 0,
                    "Cannot change type of variable '%s' from %s to %s",
                    name->value, type_name(old_type), type_name(new_type));
            } else if (old_type == TYPE_UNKNOWN && new_type != TYPE_ERROR) {
                parser->symbols.types[idx] = new_type;
            }
            parser_symbol_clear_const(parser, idx);
            if (value && value->type == AST_LITERAL_NUMBER) {
                parser_symbol_set_const(parser, idx, true,
                                        value->literal_number.number_value);
            }
        }
    }

    return ast_create_var_assign(name->value, value, is_declaration, NULL,
                                 name->line, name->column);
}

static ASTNode* parse_function(Parser* parser) {
    advance(parser);
    Token* name = consume(parser, TOKEN_IDENTIFIER, "Expected function name");

    consume(parser, TOKEN_LPAREN, "Expected '(' after function name");

    ASTNodeList* params = ast_list_create();

    if (!check(parser, TOKEN_RPAREN)) {
        while (true) {
            Token* param_name = consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
            ASTNode* param = ast_create_param(param_name->value, param_name->line, param_name->column);
            ast_list_add(params, param);
            if (!match(parser, TOKEN_COMMA)) break;
        }
    }

    consume(parser, TOKEN_RPAREN, "Expected ')' after parameters");

    parser_declare_symbol(parser, name->value, PARSER_SYM_FUNCTION,
                          TYPE_FUNCTION, params->count, name->line, name->column);

    parser->function_depth++;
    parser_enter_scope(parser);

    for (int i = 0; i < params->count; i++) {
        ASTNode* param = params->nodes[i];
        parser_declare_symbol(parser, param->param.name, PARSER_SYM_PARAMETER,
                              TYPE_ANY, 0, param->line, param->column);
    }

    ASTNode* body = parse_block(parser, true);

    parser_exit_scope(parser);
    parser->function_depth--;

    return ast_create_function(name->value, params, body, name->line, name->column);
}

static ASTNode* parse_if_statement(Parser* parser) {
    advance(parser);
    ASTNode* condition = parse_expression(parser);
    parser_check_condition(parser, condition, "If");

    ASTNode* then_branch = parse_block(parser, true);
    
    ASTNode* elif_chain = NULL;
    ASTNode* else_branch = NULL;
    
    skip_newlines(parser);
    
    while (check(parser, TOKEN_ELIF)) {
        Token* elif_kw = advance(parser);
        ASTNode* elif_cond = parse_expression(parser);
        parser_check_condition(parser, elif_cond, "If");
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
    advance(parser);
    ASTNode* condition = parse_expression(parser);
    parser_check_condition(parser, condition, "While");

    parser->loop_depth++;
    parser_enter_scope(parser);
    ASTNode* body = parse_block(parser, true);
    parser_exit_scope(parser);
    parser->loop_depth--;

    return ast_create_while(condition, body);
}

static ASTNode* parse_for_statement(Parser* parser) {
    Token* for_kw = advance(parser);
    Token* var_name = consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
    consume(parser, TOKEN_EQUAL, "Expected '=' after for variable");

    ASTNode* start = parse_expression(parser);
    parser_check_number_expr(parser, start, "For loop start");
    consume(parser, TOKEN_COMMA, "Expected ',' after start value");
    ASTNode* end = parse_expression(parser);
    parser_check_number_expr(parser, end, "For loop end");

    ASTNode* step = NULL;
    if (match(parser, TOKEN_COMMA)) {
        step = parse_expression(parser);
        parser_check_number_expr(parser, step, "For loop step");
    }

    parser->loop_depth++;
    parser_enter_scope(parser);
    parser_declare_symbol(parser, var_name->value, PARSER_SYM_VARIABLE,
                          TYPE_NUMBER, 0, var_name->line, var_name->column);
    ASTNode* body = parse_block(parser, true);
    parser_exit_scope(parser);
    parser->loop_depth--;

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
    parser_validate_import_file(parser, node->import_stmt.module_path,
                                import_kw->line, import_kw->column);
    parser_register_import(parser, node->import_stmt.module_path,
                           import_kw->line, import_kw->column);
    free(module_path);
    #undef APPEND_PATH
    return node;
}

static ASTNode* parse_return_statement(Parser* parser) {
    Token* return_kw = advance(parser);

    if (parser->semantic_checks && parser->function_depth == 0) {
        parser_error_at(parser, return_kw->line, return_kw->column, 6,
            "Return statement outside of function");
    }

    ASTNode* value = NULL;
    // Check if there's an expression on the same line
    if (!check(parser, TOKEN_NEWLINE) && !check(parser, TOKEN_EOF)) {
        value = parse_expression(parser);
    }
    
    return ast_create_return(value, return_kw->line, return_kw->column);
}

static ASTNode* parse_break_statement(Parser* parser) {
    Token* break_kw = advance(parser);
    if (parser->semantic_checks && parser->loop_depth == 0) {
        parser_error_at(parser, break_kw->line, break_kw->column, 5,
            "'break' statement outside of loop");
    }
    return ast_create_node(AST_BREAK_STMT, break_kw->line, break_kw->column);
}

static ASTNode* parse_continue_statement(Parser* parser) {
    Token* cont_kw = advance(parser);
    if (parser->semantic_checks && parser->loop_depth == 0) {
        parser_error_at(parser, cont_kw->line, cont_kw->column, 8,
            "'continue' statement outside of loop");
    }
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
                parser_check_expr_statement(parser, expr);
                return ast_create_expr_stmt(expr);
            }
            return NULL;
        }
        
        default: {
            ASTNode* expr = parse_expression(parser);
            if (expr) {
                parser_check_expr_statement(parser, expr);
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
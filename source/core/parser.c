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

#define APEX_MAX_CALL_DEPTH 512
#define APEX_MAX_LOOP_DEPTH 512
#define APEX_MAX_CALL_ARGS 64

static Token* current_token(Parser* parser);
static ASTNode* parse_program(Parser* parser);
static ASTNode* parse_statement(Parser* parser);
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_block(Parser* parser, bool require_indent, const char* after_keyword);
static ASTNode* parse_string_expression(Parser* parser, const char* expr_str, int line, int column);
static ValueType infer_expression_type(Parser* parser, ASTNode* node);
static int symbol_index_recursive(Parser* parser, const char* name);
static const char* binary_op_name(TokenType op);
static ASTNode* parse_call(Parser* parser, ASTNode* callee);

// estimates the source length of a node for error reporting
static int get_node_len(ASTNode* node) {
    if (!node) return 1;
    switch (node->type) {
        case AST_LITERAL_STRING:
            return (int)strlen(node->literal_string.string_value) + 2;
        case AST_LITERAL_NONE:
            return 4;
        case AST_IDENTIFIER:
            return (int)strlen(node->identifier.name);
        case AST_LITERAL_BOOL:
            return node->literal_bool.bool_value ? 4 : 5;
        case AST_LITERAL_NUMBER: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", node->literal_number.number_value);
            return (int)strlen(buf);
        }
        case AST_BINARY: {
            int left_len = get_node_len(node->binary.left);
            int right_len = get_node_len(node->binary.right);
            const char* op_str = binary_op_name(node->binary.op);
            return left_len + (int)strlen(op_str) + right_len + 2;
        }
        case AST_UNARY:
            return get_node_len(node->unary.operand) + (node->unary.op == TOKEN_NOT ? 4 : 1);
        case AST_CALL:
            return get_node_len(node->call.callee);
        case AST_INDEX_ACCESS: {
            if (node->access.member->type == AST_IDENTIFIER) {
                return get_node_len(node->access.object) + 1 + get_node_len(node->access.member);
            }
            return get_node_len(node->access.object) + get_node_len(node->access.member) + 2;
        }
        case AST_TABLE_LITERAL: {
            int len = 2;
            for (int i = 0; i < node->table_literal.items->count; i++) {
                if (i > 0) len += 2;
                len += get_node_len(node->table_literal.items->nodes[i]);
            }
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                if (i > 0 || node->table_literal.items->count > 0) len += 2;
                ASTNode* kv = node->table_literal.key_values->nodes[i];
                len += get_node_len(kv->binary.left) + 3 + get_node_len(kv->binary.right);
            }
            return len;
        }
        case AST_STRING_INTERP: {
            int len = 2;
            for (int i = 0; i < node->string_interp.parts->count; i++) {
                len += get_node_len(node->string_interp.parts->nodes[i]);
            }
            return len;
        }
        default:
            return 1;
    }
}

// built-in function signatures for argument count and type validation
typedef struct {
    const char* name;
    int min_args;
    int max_args;
    ValueType arg_type;
} BuiltinSig;

static const BuiltinSig BUILTINS[] = {
    {"os.output", 0, 1, TYPE_ANY},
    {"os.input", 0, 1, TYPE_ANY},
    {"os.time", 0, 0, TYPE_ANY},
    {"os.wait", 1, 1, TYPE_NUMBER},
    {"os.exit", 1, 1, TYPE_NUMBER},
    {"os.get_current_folder", 0, 0, TYPE_ANY},
    {"os.set_current_folder", 1, 1, TYPE_STRING},
    {"os.terminate_process", 1, 1, TYPE_NUMBER},
    {"os.execute", 1, 1, TYPE_STRING},
    {"os.read", 1, 1, TYPE_STRING},
    {"os.write", 2, 2, TYPE_STRING},
    {"os.append", 2, 2, TYPE_STRING},
    {"os.exists", 1, 1, TYPE_STRING},
    {"os.isfile", 1, 1, TYPE_STRING},
    {"os.isfolder", 1, 1, TYPE_STRING},
    {"os.size", 1, 1, TYPE_STRING},
    {"os.filetype", 1, 1, TYPE_STRING},
    {"os.stat", 1, 1, TYPE_STRING},
    {"os.create_file", 1, 1, TYPE_STRING},
    {"os.create_folder", 1, 1, TYPE_STRING},
    {"os.delete", 1, 1, TYPE_STRING},
    {"os.rename", 2, 2, TYPE_STRING},
    {"os.move", 2, 2, TYPE_STRING},
    {"os.copy", 2, 2, TYPE_STRING},
    {"os.items", 0, 1, TYPE_STRING},
    {"os.parentfolder", 1, 1, TYPE_STRING},
    {"os.access", 2, 2, TYPE_STRING},
    {"sys.platform", 0, 0, TYPE_ANY},
    {"sys.architecture", 0, 0, TYPE_ANY},
    {"sys.hostname", 0, 0, TYPE_ANY},
    {"sys.user", 0, 0, TYPE_ANY},
    {"sys.homedir", 0, 0, TYPE_ANY},
    {"sys.apex_version", 0, 0, TYPE_ANY},
    {"sys.executable", 0, 0, TYPE_ANY},
    {"sys.disksize", 0, 0, TYPE_STRING},
    {"sys.tempdir", 0, 0, TYPE_ANY},
    {"sys.isterminal", 0, 0, TYPE_ANY},
    {"sys.process_id", 0, 0, TYPE_ANY},
    {"sys.environment", 0, 0, TYPE_ANY},
    {"math.abs", 1, 1, TYPE_NUMBER},
    {"math.floor", 1, 1, TYPE_NUMBER},
    {"math.ceil", 1, 1, TYPE_NUMBER},
    {"math.round", 1, 2, TYPE_NUMBER},
    {"math.sqrt", 1, 1, TYPE_NUMBER},
    {"math.exp", 1, 1, TYPE_NUMBER},
    {"math.log", 1, 2, TYPE_NUMBER},
    {"math.sin", 1, 1, TYPE_NUMBER},
    {"math.cos", 1, 1, TYPE_NUMBER},
    {"math.tan", 1, 1, TYPE_NUMBER},
    {"math.asin", 1, 1, TYPE_NUMBER},
    {"math.acos", 1, 1, TYPE_NUMBER},
    {"math.atan", 1, 1, TYPE_NUMBER},
    {"math.pi", 0, 0, TYPE_ANY},
    {"math.e", 0, 0, TYPE_ANY},
    {"math.inf", 0, 0, TYPE_ANY},
    {"math.isnan", 1, 1, TYPE_NUMBER},
    {"math.isinf", 1, 1, TYPE_NUMBER},
    {"math.trunc", 1, 1, TYPE_NUMBER},
    {"math.pow", 2, 2, TYPE_NUMBER},
    {"math.atan2", 2, 2, TYPE_NUMBER},
    {"math.radians", 1, 1, TYPE_NUMBER},
    {"math.degrees", 1, 1, TYPE_NUMBER},
    {"math.gcd", 2, 2, TYPE_NUMBER},
    {"math.hypot", 2, 2, TYPE_NUMBER},
    {"math.factorial", 1, 1, TYPE_NUMBER},
    {"string.len", 1, 1, TYPE_STRING},
    {"string.lower", 1, 1, TYPE_STRING},
    {"string.upper", 1, 1, TYPE_STRING},
    {"string.sub", 3, 3, TYPE_STRING},
    {"string.split", 2, 2, TYPE_STRING},
    {"string.join", 2, 2, TYPE_STRING},
    {"string.trim", 1, 1, TYPE_STRING},
    {"string.find", 2, 2, TYPE_STRING},
    {"string.replace", 3, 3, TYPE_STRING},
    {"table.remove", 2, 2, TYPE_TABLE},
    {"table.has", 2, 2, TYPE_TABLE},
    {"table.size", 1, 1, TYPE_TABLE},
    {"table.keys", 1, 1, TYPE_TABLE},
    {"table.values", 1, 1, TYPE_TABLE},
    {"table.clear", 1, 1, TYPE_TABLE},
    {"table.copy", 1, 1, TYPE_TABLE},
    {"table.merge", 2, 2, TYPE_TABLE},
    {"ffi.open", 1, 1, TYPE_STRING},
    {"ffi.call", 2, 64, TYPE_ANY},
    {"ffi.errno", 0, 0, TYPE_ANY},
    {"ffi.strerror", 0, 1, TYPE_NUMBER},
    {"ffi.malloc", 1, 1, TYPE_NUMBER},
    {"ffi.free", 1, 1, TYPE_NUMBER},
    {"random.random", 0, 0, TYPE_ANY},
    {"random.randint", 2, 2, TYPE_NUMBER},
    {"random.choice", 1, 1, TYPE_TABLE},
    {"random.shuffle", 1, 1, TYPE_TABLE},
    {"random.sample", 2, 2, TYPE_TABLE},
    {"random.gauss", 2, 2, TYPE_NUMBER},
    {"random.seed", 0, 1, TYPE_NUMBER},
    {"random.triangular", 0, 3, TYPE_NUMBER},
    {"random.expovariate", 1, 1, TYPE_NUMBER},
    {"random.betavariate", 2, 2, TYPE_NUMBER},
    {"random.secure_token_hex", 0, 1, TYPE_NUMBER},
    {"random.secure_token_bytes", 1, 1, TYPE_NUMBER},
    {"random.secure_randint", 1, 1, TYPE_NUMBER},
    {"random.compare_digest", 2, 2, TYPE_STRING},
    {"codecs.json_read", 1, 1, TYPE_STRING},
    {"codecs.json_write", 1, 1, TYPE_ANY},
    {"codecs.csv_read", 1, 3, TYPE_STRING},
    {"codecs.csv_write", 1, 3, TYPE_TABLE},
    {"codecs.xml_read", 1, 1, TYPE_STRING},
    {"codecs.xml_write", 1, 1, TYPE_TABLE},
    {"codecs.base_write", 1, 1, TYPE_STRING},
    {"codecs.base_read", 1, 1, TYPE_STRING},
    {"codecs.baseurl_write", 1, 1, TYPE_STRING},
    {"codecs.baseurl_read", 1, 1, TYPE_STRING},
    {"number", 1, 1, TYPE_ANY},
    {"string", 1, 1, TYPE_ANY},
    {"type", 1, 1, TYPE_ANY}
};

// looks up a built-in function signature by name
static const BuiltinSig* lookup_builtin(const char* name) {
    for (size_t i = 0; i < sizeof(BUILTINS) / sizeof(BUILTINS[0]); i++) {
        if (strcmp(BUILTINS[i].name, name) == 0) {
            return &BUILTINS[i];
        }
    }
    return NULL;
}

// reports a parse error at a specific source position with formatting
void parser_error_at(Parser* parser, int line, int column, int len,
                     const char* format, ...) {
    if (parser->last_error_line == line && parser->last_error_column == column) {
        return;
    }
    
    parser->last_error_line = line;
    parser->last_error_column = column;
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    parser->error_count++;
    print_error_with_context(parser->filename, parser->source,
                             line, column, len, "Parse Error", buffer);
}

// reports a parse error at the current token position
void parser_error(Parser* parser, const char* message) {
    Token* token = current_token(parser);
    int len = token->value ? (int)strlen(token->value) : 1;
    parser_error_at(parser, token->line, token->column, len, "%s", message);
}

// returns true if any error was encountered during parsing
bool parser_had_errors(const Parser* parser) {
    return parser->error_count > 0;
}

// returns a string name for a value type
static const char* type_name(ValueType type) {
    switch (type) {
        case TYPE_NUMBER: return "number";
        case TYPE_STRING: return "string";
        case TYPE_NONE: return "none";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_TABLE: return "table";
        case TYPE_FUNCTION: return "function";
        case TYPE_UNKNOWN: return "unknown";
        case TYPE_ERROR: return "error";
        case TYPE_ANY: return "any";
        default: return "???";
    }
}

// checks if a type is numeric (for arithmetic operations)
static bool is_numeric_type(ValueType type) {
    return type == TYPE_NUMBER;
}

// checks if a type supports comparison operations
static bool is_comparable_type(ValueType type) {
    return type == TYPE_NUMBER || type == TYPE_STRING || type == TYPE_BOOLEAN || type == TYPE_NONE;
}

// returns the string representation of a binary operator token
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

// sets the source directory for module resolution
static void parser_set_source_dir(Parser* parser, const char* filename) {
    parser->source_dir = (char*)malloc(PATH_MAX);
    if (!parser->source_dir) return;
    
    if (!filename || filename[0] == '\0' ||
        strcmp(filename, "stdin") == 0 || strcmp(filename, "<interpolation>") == 0) {
        strcpy(parser->source_dir, ".");
        return;
    }
    
    strncpy(parser->source_dir, filename, PATH_MAX - 1);
    parser->source_dir[PATH_MAX - 1] = '\0';
    
    char* last_sep = strrchr(parser->source_dir, '/');
    char* last_sep_win = strrchr(parser->source_dir, '\\');
    
    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }
    
    if (last_sep) {
        *last_sep = '\0';
    } else {
        strcpy(parser->source_dir, ".");
    }
}

// evaluates a numeric constant expression, returns true if successful
static bool evaluate_numeric_constant(Parser* parser, ASTNode* node, double* out_value) {
    if (!node) return false;
    switch (node->type) {
        case AST_LITERAL_NUMBER:
            *out_value = node->literal_number.number_value;
            return true;
        case AST_IDENTIFIER: {
            int idx = symbol_index_recursive(parser, node->identifier.name);
            if (idx >= 0 && parser->symbols.const_known[idx]) {
                *out_value = parser->symbols.const_values[idx];
                return true;
            }
            return false;
        }
        case AST_UNARY:
            if (node->unary.op == TOKEN_MINUS) {
                double val;
                if (evaluate_numeric_constant(parser, node->unary.operand, &val)) {
                    *out_value = -val;
                    return true;
                }
            }
            return false;
        case AST_BINARY: {
            double lval, rval;
            if (evaluate_numeric_constant(parser, node->binary.left, &lval) &&
                evaluate_numeric_constant(parser, node->binary.right, &rval)) {
                switch (node->binary.op) {
                    case TOKEN_PLUS: *out_value = lval + rval; return true;
                    case TOKEN_MINUS: *out_value = lval - rval; return true;
                    case TOKEN_STAR: *out_value = lval * rval; return true;
                    case TOKEN_SLASH: 
                        if (rval == 0.0) { *out_value = 0.0; }
                        else { *out_value = lval / rval; }
                        return true;
                    case TOKEN_PERCENT:
                        if (rval == 0.0) { *out_value = 0.0; }
                        else { *out_value = fmod(lval, rval); }
                        return true;
                    default: return false;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

// checks if an expression has side effects (calls, assignments)
static bool expr_has_side_effect(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_CALL:
        case AST_ASSIGN:
        case AST_VAR_DECL:
            return true;
        case AST_BINARY:
            return expr_has_side_effect(node->binary.left) ||
                   expr_has_side_effect(node->binary.right);
        case AST_UNARY:
            return expr_has_side_effect(node->unary.operand);
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
                if (expr_has_side_effect(kv->binary.left) || expr_has_side_effect(kv->binary.right)) {
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

// warns about expression statements that have no effect
static void parser_check_expr_statement(Parser* parser, ASTNode* expr) {
    if (!parser->semantic_checks || !expr) return;
    
    if (expr->type == AST_BINARY) {
        TokenType op = expr->binary.op;
        if (op == TOKEN_AND || op == TOKEN_OR || 
            op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL ||
            op == TOKEN_LESS || op == TOKEN_GREATER || 
            op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
            return;
        }
    }
    
    if (!expr_has_side_effect(expr)) {
        parser_error_at(parser, expr->line, expr->column, get_node_len(expr),
                        "Expression statement has no effect");
    }
}

// checks if a module name is a built-in system module
static bool is_builtin_module_root(const char* name) {
    return strcmp(name, "os") == 0 || 
           strcmp(name, "sys") == 0 ||
           strcmp(name, "math") == 0 ||
           strcmp(name, "string") == 0 || 
           strcmp(name, "table") == 0 ||
           strcmp(name, "ffi") == 0 ||
           strcmp(name, "random") == 0 ||
           strcmp(name, "codecs") == 0;
}

// builds a filesystem path for a module from its dotted name
static bool build_module_path(Parser* parser, const char* module_path, char* out_path, int out_size) {
    char relative[1024];
    size_t len = 0;
    relative[0] = '\0';
    
    char path_copy[1024];
    strncpy(path_copy, module_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char* segment = strtok(path_copy, ".");
    while (segment) {
        if (len > 0) {
            if (len + 1 < sizeof(relative)) {
#ifdef _WIN32
                relative[len++] = '\\';
#else
                relative[len++] = '/';
#endif
            }
        }
        size_t seg_len = strlen(segment);
        if (len + seg_len >= sizeof(relative)) return false;
        memcpy(relative + len, segment, seg_len);
        len += seg_len;
        relative[len] = '\0';
        segment = strtok(NULL, ".");
    }
    
    if (len + 6 >= sizeof(relative)) return false;
    strcat(relative, ".apex");
    
#ifdef _WIN32
    if (snprintf(out_path, out_size, "%s\\%s", parser->source_dir, relative) >= out_size) return false;
#else
    if (snprintf(out_path, out_size, "%s/%s", parser->source_dir, relative) >= out_size) return false;
#endif
    return true;
}

// validates that an imported module file exists on disk
static void parser_validate_import_file(Parser* parser, const char* module_path, int line, int column) {
    if (!parser->semantic_checks || !parser->source_dir || !module_path) return;
    if (strcmp(parser->filename, "stdin") == 0 || strcmp(parser->filename, "<interpolation>") == 0) return;
    
    char first_segment[256];
    const char* dot = strchr(module_path, '.');
    size_t first_len = dot ? (size_t)(dot - module_path) : strlen(module_path);
    if (first_len >= sizeof(first_segment)) first_len = sizeof(first_segment) - 1;
    memcpy(first_segment, module_path, first_len);
    first_segment[first_len] = '\0';
    
    if (is_builtin_module_root(first_segment)) return;
    
    char full_path[PATH_MAX];
    if (!build_module_path(parser, module_path, full_path, sizeof(full_path))) return;
    
#ifdef _WIN32
    if (_access(full_path, F_OK) != 0) {
#else
    if (access(full_path, F_OK) != 0) {
#endif
        parser_error_at(parser, line, column, (int)strlen(module_path),
                        "Module '%s' not found (expected '%s')", module_path, full_path);
    }
}

// sets or clears a symbol's constant value
static void parser_symbol_set_const(Parser* parser, int idx, bool known, double value) {
    if (idx < 0) return;
    parser->symbols.const_known[idx] = known;
    parser->symbols.const_values[idx] = value;
}

static void parser_symbol_clear_const(Parser* parser, int idx) {
    parser_symbol_set_const(parser, idx, false, 0.0);
}

// grows the symbol table when capacity is reached
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

// finds a symbol index in a specific scope
static int symbol_index_in_scope(Parser* parser, const char* name, int scope) {
    for (int i = 0; i < parser->symbols.count; i++) {
        if (parser->symbols.scope_levels[i] == scope &&
            strcmp(parser->symbols.names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

// finds a symbol by name searching from innermost to outermost scope
static int symbol_index_recursive(Parser* parser, const char* name) {
    for (int scope = parser->symbols.current_scope; scope >= 0; scope--) {
        int idx = symbol_index_in_scope(parser, name, scope);
        if (idx >= 0) return idx;
    }
    return -1;
}

// enters a new lexical scope
void parser_enter_scope(Parser* parser) {
    parser->symbols.current_scope++;
}

// exits the current lexical scope, removing all symbols declared there
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

// declares a new symbol in the current scope
bool parser_declare_symbol(Parser* parser, const char* name, ParserSymbolKind kind,
                           ValueType type, int param_count, int line, int column) {
    (void)line;
    (void)column;
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

// checks if a symbol is declared in any accessible scope
bool parser_is_declared(Parser* parser, const char* name) {
    return symbol_index_recursive(parser, name) >= 0;
}

// registers an imported module as a symbol in the current scope
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

// creates a new parser instance for a token stream
Parser* parser_create(Token* tokens, int count, const char* filename, const char* source) {
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    parser->tokens = tokens;
    parser->count = count;
    parser->current = 0;
    parser->filename = strdup(filename);
    
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

    parser->last_error_line = -1;
    parser->last_error_column = -1;

    return parser;
}

// destroys a parser and frees all associated resources
void parser_destroy(Parser* parser) {
    if (parser) {
        free(parser->filename);
        free(parser->source_dir);

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

// peeks at a token ahead without consuming it
static Token* peek(Parser* parser, int offset) {
    int idx = parser->current + offset;
    if (idx >= parser->count) return &parser->tokens[parser->count - 1];
    return &parser->tokens[idx];
}

// returns the current token without consuming it
static Token* current_token(Parser* parser) {
    return peek(parser, 0);
}

// advances to the next token and returns the previous one
static Token* advance(Parser* parser) {
    if (parser->current >= parser->count) return &parser->tokens[parser->count - 1];
    return &parser->tokens[parser->current++];
}

// checks if the current token is of the given type
static bool check(Parser* parser, TokenType type) {
    if (current_token(parser)->type == TOKEN_EOF) {
        return type == TOKEN_EOF; 
    }
    return current_token(parser)->type == type;
}

// checks if the next token is of the given type
static bool check_next(Parser* parser, TokenType type) {
    if (peek(parser, 1)->type == TOKEN_EOF) return false;
    return peek(parser, 1)->type == type;
}

// consumes a token if it matches the expected type
static bool match(Parser* parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}

// consumes a token or reports an error if the type doesn't match
static Token* consume(Parser* parser, TokenType type, const char* message) {
    if (check(parser, type)) {
        return advance(parser);
    }
    parser_error(parser, message);
    return NULL;
}

// skips over newline tokens
static void skip_newlines(Parser* parser) {
    while (check(parser, TOKEN_NEWLINE)) {
        advance(parser);
    }
}

// infers the type of a binary operation with type checking
static ValueType infer_binary_type(Parser* parser, ASTNode* node) {
    ValueType left_type = infer_expression_type(parser, node->binary.left);
    ValueType right_type = infer_expression_type(parser, node->binary.right);

    if (left_type == TYPE_ANY || right_type == TYPE_ANY) {
        switch (node->binary.op) {
            case TOKEN_PLUS:
            case TOKEN_MINUS:
            case TOKEN_STAR:
            case TOKEN_SLASH:
            case TOKEN_PERCENT:
                return TYPE_NUMBER;
            case TOKEN_EQUAL_EQUAL:
            case TOKEN_NOT_EQUAL:
            case TOKEN_LESS:
            case TOKEN_GREATER:
            case TOKEN_LESS_EQUAL:
            case TOKEN_GREATER_EQUAL:
                return TYPE_BOOLEAN;
            case TOKEN_AND:
            case TOKEN_OR:
                return TYPE_BOOLEAN;
            default:
                return TYPE_ANY;
        }
    }

    if (left_type == TYPE_UNKNOWN || right_type == TYPE_UNKNOWN) return TYPE_UNKNOWN;

    switch (node->binary.op) {
        case TOKEN_PLUS:
            if (left_type == TYPE_STRING || right_type == TYPE_STRING) {
                parser_error_at(parser, node->line, node->column, get_node_len(node),
                    "Arithmetic '+' requires numbers. For strings, use interpolation.");
                return TYPE_ERROR;
            }
            if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                parser_error_at(parser, node->line, node->column, get_node_len(node),
                    "Arithmetic operator '+' requires number operands, got %s and %s",
                    type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_NUMBER;
            
        case TOKEN_MINUS: case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT:
            if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                parser_error_at(parser, node->line, node->column, get_node_len(node),
                    "Arithmetic operator '%s' requires number operands, got %s and %s",
                    binary_op_name(node->binary.op), type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_NUMBER;

        case TOKEN_EQUAL_EQUAL: case TOKEN_NOT_EQUAL:
            if (left_type == TYPE_NONE || right_type == TYPE_NONE) {
                return TYPE_BOOLEAN;
            }
            if (!is_comparable_type(left_type) || !is_comparable_type(right_type)) {
                parser_error_at(parser, node->binary.left->line, node->binary.left->column, get_node_len(node->binary.left),
                                "Cannot compare types %s and %s", type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            if (left_type != right_type) {
                parser_error_at(parser, node->binary.left->line, node->binary.left->column, get_node_len(node->binary.left),
                                "Cannot compare %s with %s", type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;

        case TOKEN_LESS: case TOKEN_GREATER: case TOKEN_LESS_EQUAL: case TOKEN_GREATER_EQUAL:
            if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                parser_error_at(parser, node->line, node->column, get_node_len(node),
                    "Comparison operator '%s' requires number operands, got %s and %s",
                    binary_op_name(node->binary.op), type_name(left_type), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
            
        case TOKEN_AND: case TOKEN_OR:
            if (left_type != TYPE_BOOLEAN && left_type != TYPE_ANY && left_type != TYPE_UNKNOWN) {
                parser_error_at(parser, node->binary.left->line, node->binary.left->column, 
                            get_node_len(node->binary.left),
                            "Logical operator '%s' requires boolean operands, got %s",
                            binary_op_name(node->binary.op), type_name(left_type));
                return TYPE_ERROR;
            }
            if (right_type != TYPE_BOOLEAN && right_type != TYPE_ANY && right_type != TYPE_UNKNOWN) {
                parser_error_at(parser, node->binary.right->line, node->binary.right->column,
                            get_node_len(node->binary.right),
                            "Logical operator '%s' requires boolean operands, got %s",
                            binary_op_name(node->binary.op), type_name(right_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
        default:
            parser_error_at(parser, node->line, node->column, 0, "Unknown binary operator");
            return TYPE_ERROR;
    }
}

// infers the type of a unary operation with type checking
static ValueType infer_unary_type(Parser* parser, ASTNode* node) {
    ValueType operand_type = infer_expression_type(parser, node->unary.operand);
    if (operand_type == TYPE_ANY) {
        if (node->unary.op == TOKEN_MINUS) return TYPE_NUMBER;
        if (node->unary.op == TOKEN_NOT) return TYPE_BOOLEAN;
    }
    if (operand_type == TYPE_UNKNOWN) return TYPE_UNKNOWN;

    switch (node->unary.op) {
        case TOKEN_MINUS:
            if (!is_numeric_type(operand_type)) {
                parser_error_at(parser, node->unary.operand->line, node->unary.operand->column, get_node_len(node->unary.operand),
                                "Unary minus requires number operand, got %s", type_name(operand_type));
                return TYPE_ERROR;
            }
            return TYPE_NUMBER;
        case TOKEN_NOT:
            if (operand_type != TYPE_BOOLEAN) {
                parser_error_at(parser, node->unary.operand->line, node->unary.operand->column, get_node_len(node->unary.operand),
                                "Logical not requires boolean operand, got %s", type_name(operand_type));
                return TYPE_ERROR;
            }
            return TYPE_BOOLEAN;
        default:
            return TYPE_ERROR;
    }
}

// resolves the name of a callable expression
static const char* resolve_call_name(ASTNode* callee, char* buffer, size_t buflen) {
    if (callee->type == AST_IDENTIFIER) {
        return callee->identifier.name;
    }
    if (callee->type == AST_INDEX_ACCESS &&
        callee->access.object->type == AST_IDENTIFIER &&
        callee->access.member->type == AST_IDENTIFIER) {
        snprintf(buffer, buflen, "%s.%s",
                 callee->access.object->identifier.name,
                 callee->access.member->identifier.name);
        return buffer;
    }
    return NULL;
}

// checks if a name is a known built-in module root
static bool is_known_builtin_module(const char* name) {
    return strcmp(name, "os") == 0 ||
           strcmp(name, "sys") == 0 ||
           strcmp(name, "math") == 0 ||
           strcmp(name, "string") == 0 ||
           strcmp(name, "table") == 0 ||
           strcmp(name, "ffi") == 0 ||
           strcmp(name, "random") == 0 ||
           strcmp(name, "codecs") == 0;
}

// infers the return type of a function call with signature validation
static ValueType infer_call_type(Parser* parser, ASTNode* node) {
    ValueType callee_type = infer_expression_type(parser, node->call.callee);
    
    if (callee_type == TYPE_ERROR) {
        return TYPE_ERROR;
    }

    char full_name[256] = "";
    const char* func_name = resolve_call_name(node->call.callee, full_name, sizeof(full_name));

    if (func_name && (strcmp(func_name, "number") == 0 || 
                      strcmp(func_name, "string") == 0 || 
                      strcmp(func_name, "type") == 0)) {
        if (strcmp(func_name, "number") == 0) return TYPE_NUMBER;
        if (strcmp(func_name, "string") == 0) return TYPE_STRING;
        if (strcmp(func_name, "type") == 0) return TYPE_STRING;
    }

    if (callee_type != TYPE_FUNCTION && callee_type != TYPE_ANY && 
        callee_type != TYPE_UNKNOWN) {
        int err_len = get_node_len(node->call.callee);
        parser_error_at(parser, node->call.callee->line, node->call.callee->column,
            err_len > 0 ? err_len : 1,
            "Cannot call non-function (type: %s)", type_name(callee_type));
        return TYPE_ERROR;
    }

    if (func_name) {
        const BuiltinSig* builtin = lookup_builtin(func_name);
        if (builtin) {
            int actual = node->call.arguments->count;
            if (actual < builtin->min_args || actual > builtin->max_args) {
                int err_len = get_node_len(node->call.callee);
                parser_error_at(parser, node->call.callee->line, node->call.callee->column, err_len > 0 ? err_len : 1,
                    "Function '%s' expects %d to %d arguments, got %d",
                    func_name, builtin->min_args, builtin->max_args, actual);
            }
            if (actual > APEX_MAX_CALL_ARGS) {
                parser_error_at(parser, node->line, node->column, 0, "Too many arguments (%d), maximum is %d", actual, APEX_MAX_CALL_ARGS);
            }
            
            if (actual >= 1 && builtin->arg_type != TYPE_ANY) {
                ASTNode* arg = node->call.arguments->nodes[0];
                ValueType arg_t = infer_expression_type(parser, arg);
                if (arg_t != builtin->arg_type && arg_t != TYPE_ANY && arg_t != TYPE_UNKNOWN) {
                     parser_error_at(parser, arg->line, arg->column, get_node_len(arg),
                        "%s argument must be %s, got %s", func_name, type_name(builtin->arg_type), type_name(arg_t));
                }
            }
            return TYPE_ANY;
        }

        int sym_idx = symbol_index_recursive(parser, func_name);
        if (sym_idx >= 0 && parser->symbols.kinds[sym_idx] == PARSER_SYM_FUNCTION) {
            int expected = parser->symbols.param_counts[sym_idx];
            int actual = node->call.arguments->count;
            if (expected != actual) {
                int err_len = get_node_len(node->call.callee);
                parser_error_at(parser, node->call.callee->line, node->call.callee->column, err_len > 0 ? err_len : 1,
                    "Function '%s' expects %d arguments, got %d", func_name, expected, actual);
            }
            return TYPE_ANY;
        }

        char root_module[64] = {0};
        const char* dot_pos = strchr(func_name, '.');
        if (dot_pos) {
            size_t root_len = dot_pos - func_name;
            if (root_len < sizeof(root_module)) {
                strncpy(root_module, func_name, root_len);
                root_module[root_len] = '\0';
                
                if (is_known_builtin_module(root_module)) {
                    parser_error_at(parser, node->call.callee->line, node->call.callee->column, get_node_len(node->call.callee),
                        "Undefined function '%s' in module '%s'", dot_pos + 1, root_module);
                    return TYPE_ERROR;
                }
            }
        }
    }

    for (int i = 0; i < node->call.arguments->count; i++) {
        infer_expression_type(parser, node->call.arguments->nodes[i]);
    }
    
    return TYPE_UNKNOWN;
}

// infers the type of an index access expression
static ValueType infer_index_access_type(Parser* parser, ASTNode* node) {
    if (node->access.object->type == AST_IDENTIFIER && 
        node->access.member->type == AST_IDENTIFIER) {
        return TYPE_ANY; 
    }
    
    ValueType object_type = infer_expression_type(parser, node->access.object);
    if (object_type == TYPE_ERROR) return TYPE_ERROR;
    if (object_type != TYPE_TABLE && object_type != TYPE_UNKNOWN && object_type != TYPE_ANY) {
        int obj_len = (node->access.object->type == AST_IDENTIFIER) ? (int)strlen(node->access.object->identifier.name) : 0;
        parser_error_at(parser, node->line, node->column, obj_len,
            "Cannot access element of non-table type %s", type_name(object_type));
        return TYPE_ERROR;
    }
    infer_expression_type(parser, node->access.member);
    return TYPE_UNKNOWN;
}

// main type inference dispatcher for all expression types
static ValueType infer_expression_type(Parser* parser, ASTNode* node) {
    if (!node) return TYPE_UNKNOWN;

    switch (node->type) {
        case AST_LITERAL_NUMBER: return TYPE_NUMBER;
        case AST_LITERAL_STRING: return TYPE_STRING;
        case AST_LITERAL_NONE: return TYPE_NONE;
        case AST_LITERAL_BOOL: return TYPE_BOOLEAN;
        case AST_ASSIGN:
        case AST_VAR_DECL:
            return infer_expression_type(parser, node->var_assign.value);
        case AST_IDENTIFIER: {
            const char* name = node->identifier.name;
            int idx = symbol_index_recursive(parser, name);
            
            if (idx >= 0) {
                return parser->symbols.types[idx];
            }

            if (lookup_builtin(name)) {
                return TYPE_FUNCTION;
            }

            if (is_known_builtin_module(name)) {
                return TYPE_UNKNOWN;
            }

            parser_error_at(parser, node->line, node->column, (int)strlen(name),
                            "Undefined variable or function '%s'", name);
            return TYPE_ERROR;
        }
        case AST_BINARY: return infer_binary_type(parser, node);
        case AST_UNARY: return infer_unary_type(parser, node);
        case AST_CALL: return infer_call_type(parser, node);
        case AST_INDEX_ACCESS: return infer_index_access_type(parser, node);
        case AST_TABLE_LITERAL: {
            for (int i = 0; i < node->table_literal.items->count; i++) {
                infer_expression_type(parser, node->table_literal.items->nodes[i]);
            }
            for (int i = 0; i < node->table_literal.key_values->count; i++) {
                ASTNode* kv = node->table_literal.key_values->nodes[i];
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

// public API for type checking an expression
ValueType parser_check_expression(Parser* parser, ASTNode* node) {
    if (!parser->semantic_checks) return TYPE_UNKNOWN;
    return infer_expression_type(parser, node);
}

// validates that a condition expression is boolean and explicit
static void parser_check_condition(Parser* parser, ASTNode* condition, const char* context) {
    if (!parser->semantic_checks) return;
    ValueType cond_type = infer_expression_type(parser, condition);
    
    if (cond_type != TYPE_BOOLEAN && cond_type != TYPE_ANY && cond_type != TYPE_UNKNOWN) {
        parser_error_at(parser, condition->line, condition->column, get_node_len(condition),
            "%s condition must be boolean, got %s", context, type_name(cond_type));
        return;
    }

    bool is_explicit_condition = false;
    
    if (condition->type == AST_BINARY) {
        TokenType op = condition->binary.op;
        if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL || 
            op == TOKEN_LESS || op == TOKEN_GREATER || 
            op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL ||
            op == TOKEN_AND || op == TOKEN_OR) {
            is_explicit_condition = true;
        }
    } else if (condition->type == AST_UNARY && condition->unary.op == TOKEN_NOT) {
        is_explicit_condition = true;
    }

    if (!is_explicit_condition) {
        parser_error_at(parser, condition->line, condition->column, get_node_len(condition),
            "%s requires explicit condition (e.g., 'var == true' or 'var != false')", context);
    }
}

// validates that an expression is a number
static void parser_check_number_expr(Parser* parser, ASTNode* expr, const char* context) {
    if (!parser->semantic_checks) return;
    ValueType t = infer_expression_type(parser, expr);
    if (t != TYPE_NUMBER && t != TYPE_ANY && t != TYPE_UNKNOWN) {
        parser_error_at(parser, expr->line, expr->column, get_node_len(expr),
                        "%s must be a number, got %s", context, type_name(t));
    }
}

// returns the length of a source line for error reporting
static int get_line_length(const char* source, int line_num) {
    if (!source) return 0;
    int current_line = 1;
    int i = 0;
    while (source[i] != '\0') {
        if (current_line == line_num) {
            int start = i;
            while (source[i] != '\0' && source[i] != '\n' && source[i] != '\r') {
                i++;
            }
            return i - start;
        }
        if (source[i] == '\n') {
            current_line++;
        }
        i++;
    }
    return 0;
}

// Pratt parser precedence levels
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} Precedence;

// returns the precedence of a token type for Pratt parsing
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

static ASTNode* parse_prefix(Parser* parser);
static ASTNode* parse_infix(Parser* parser, ASTNode* left);
static ASTNode* parse_precedence(Parser* parser, Precedence precedence);

// parses a number literal token
static ASTNode* parse_number(Parser* parser) {
    Token* token = advance(parser);
    double value = atof(token->value);
    return ast_create_literal_number(value, token->line, token->column);
}

// parses a string expression with interpolation support
static ASTNode* parse_string_expression(Parser* parser, const char* expr_str, int line, int column) {
    Tokenizer* temp_tokenizer = tokenizer_create(expr_str, parser->filename);
    temp_tokenizer->line = line;
    
    int temp_count;
    Token* temp_tokens = tokenizer_tokenize(temp_tokenizer, &temp_count);
    
    for (int i = 0; i < temp_count; i++) {
        temp_tokens[i].line = line;
        temp_tokens[i].column += column - 1;
    }

    Parser* temp_parser = parser_create(temp_tokens, temp_count, parser->filename, parser->source);
    temp_parser->semantic_checks = true;

    for (int i = 0; i < parser->symbols.count; i++) {
        if (parser->symbols.scope_levels[i] <= parser->symbols.current_scope) {
            parser_declare_symbol(temp_parser, 
                                  parser->symbols.names[i], 
                                  parser->symbols.kinds[i], 
                                  parser->symbols.types[i], 
                                  parser->symbols.param_counts[i],
                                  0,
                                  0);
            
            int new_idx = temp_parser->symbols.count - 1;
            temp_parser->symbols.const_known[new_idx] = parser->symbols.const_known[i];
            temp_parser->symbols.const_values[new_idx] = parser->symbols.const_values[i];
        }
    }
    temp_parser->symbols.current_scope = parser->symbols.current_scope;

    ASTNode* expr = parse_expression(temp_parser);
    
    parser->error_count += temp_parser->error_count;
    
    parser_destroy(temp_parser);
    tokenizer_destroy(temp_tokenizer);
    return expr;
}

// parses a string literal with interpolation detection
static ASTNode* parse_string(Parser* parser) {
    Token* token = advance(parser);
    const char* value = token->value;
    
    if (!strchr(value, '{') && !strchr(value, '\\')) {
        return ast_create_literal_string(value, token->line, token->column);
    }

    ASTNodeList* parts = ast_list_create();
    const char* p = value;
    const char* start = value;
    
    int line_offset = 0; 

    while (*p) {
        if (*p == '\n') {
            line_offset++;
            p++;
            continue;
        }

        if (*p == '\\' && *(p + 1) != '\0') {
            char next_char = *(p + 1);
            
            if (next_char == '{' || next_char == '}') {
                if (p > start) {
                    int len = (int)(p - start);
                    char* lit = (char*)malloc(len + 1);
                    strncpy(lit, start, len);
                    lit[len] = '\0';
                    if (len > 0) {
                        ASTNode* str_node = ast_create_literal_string(lit, token->line + line_offset, token->column);
                        ast_list_add(parts, str_node);
                    }
                    free(lit);
                }
                
                char escaped_char[2] = { next_char, '\0' };
                ASTNode* char_node = ast_create_literal_string(escaped_char, token->line + line_offset, token->column + (int)(p - value));
                ast_list_add(parts, char_node);
                
                p += 2;
                start = p;
                continue;
            }
            
            p++; 
        } else if (*p == '{') {
            if (p > start) {
                int len = (int)(p - start);
                char* lit = (char*)malloc(len + 1);
                strncpy(lit, start, len);
                lit[len] = '\0';
                if (len > 0) {
                    ASTNode* str_node = ast_create_literal_string(lit, token->line + line_offset, token->column);
                    ast_list_add(parts, str_node);
                }
                free(lit);
            }
            
            const char* expr_start = p + 1;
            const char* expr_end = strchr(expr_start, '}');
            
            if (expr_end) {
                int expr_len = (int)(expr_end - expr_start);
                char* expr_str = (char*)malloc(expr_len + 1);
                strncpy(expr_str, expr_start, expr_len);
                expr_str[expr_len] = '\0';
                
                int absolute_col = token->column + 1 + (int)(expr_start - value);

                ASTNode* expr_node = parse_string_expression(parser, expr_str,
                    token->line + line_offset,
                    absolute_col);
                
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
    
    if (p > start) {
        ASTNode* str_node = ast_create_literal_string(start, token->line + line_offset, token->column);
        ast_list_add(parts, str_node);
    }
    
    return ast_create_string_interp(parts);
}

// parses a none/null literal
static ASTNode* parse_none(Parser* parser) {
    Token* token = advance(parser);
    return ast_create_literal_none(token->line, token->column);
}

// parses a boolean literal
static ASTNode* parse_bool(Parser* parser) {
    Token* token = advance(parser);
    return ast_create_literal_bool(
        token->type == TOKEN_TRUE, token->line, token->column);
}

// parses an identifier
static ASTNode* parse_identifier(Parser* parser) {
    Token* token = advance(parser);
    return ast_create_identifier(token->value, token->line, token->column);
}

// parses a table literal with positional items and key-value pairs
static ASTNode* parse_table_literal(Parser* parser) {
    Token* open_bracket = advance(parser);
    int line = open_bracket->line;
    int column = open_bracket->column;

    ASTNodeList* items = ast_list_create();
    ASTNodeList* key_values = ast_list_create();
    bool has_key_values = false;

    skip_newlines(parser);

    if (!check(parser, TOKEN_RBRACKET)) {
        while (true) {
            skip_newlines(parser);

            bool is_key = (check(parser, TOKEN_IDENTIFIER) || 
                           check(parser, TOKEN_STRING) || 
                           check(parser, TOKEN_NUMBER)) &&
                          check_next(parser, TOKEN_EQUAL);

            if (is_key) {
                has_key_values = true;
                Token* key_token = advance(parser);
                advance(parser);

                ASTNode* key_node = ast_create_literal_string(
                    key_token->value, key_token->line, key_token->column);

                ASTNode* value = parse_expression(parser);
                ASTNode* kv_node = ast_create_binary(TOKEN_EQUAL, key_node, value);
                ast_list_add(key_values, kv_node);
            } else {
                if (has_key_values) {
                    Token* bad_token = current_token(parser);
                    int len = bad_token->value ? (int)strlen(bad_token->value) : 1;
                    if (bad_token->type == TOKEN_STRING) len += 2;
                    
                    parser_error_at(parser, bad_token->line, bad_token->column, len,
                                    "Cannot mix ordered items after key-value pairs");
                }
                ASTNode* item = parse_expression(parser);
                ast_list_add(items, item);
            }

            skip_newlines(parser);
            if (!match(parser, TOKEN_COMMA)) break;
            skip_newlines(parser);
        }
    }

    skip_newlines(parser);
    consume(parser, TOKEN_RBRACKET, "Expected ']' after table literal");

    return ast_create_table_literal(items, key_values, line, column);
}

// parses a parenthesized expression
static ASTNode* parse_group(Parser* parser) {
    advance(parser);
    skip_newlines(parser);
    ASTNode* expr = parse_expression(parser);
    skip_newlines(parser);
    consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
    return expr;
}

// parses index access using brackets
static ASTNode* parse_index_access(Parser* parser, ASTNode* object) {
    advance(parser);
    skip_newlines(parser);
    ASTNode* index = parse_expression(parser);
    skip_newlines(parser);
    consume(parser, TOKEN_RBRACKET, "Expected ']' after index");
    return ast_create_index_access(object, index);
}

// parses dot member access with module validation
static ASTNode* parse_member_access(Parser* parser, ASTNode* object) {
    advance(parser);
    Token* token = current_token(parser);
    
    bool is_module = false;
    if (object->type == AST_IDENTIFIER) {
        int idx = symbol_index_recursive(parser, object->identifier.name);
        if (idx >= 0 && parser->symbols.kinds[idx] == PARSER_SYM_MODULE) {
            is_module = true;
        } else if (is_known_builtin_module(object->identifier.name)) {
            parser_error_at(parser, object->line, object->column, 
                          get_node_len(object),
                          "Module '%s' is not imported. Use 'import %s' first", 
                          object->identifier.name, object->identifier.name);
            if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_NUMBER) 
                advance(parser);
            return object;
        }
    }
    
    if (!is_module) {
        parser_error_at(parser, token->line, token->column, 
                       token->value ? (int)strlen(token->value) : 1,
            "Dot access is restricted to imported modules. Use bracket notation '[]' for table access.");
        if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_NUMBER) 
            advance(parser);
        return object; 
    }

    if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_NUMBER || 
        (token->type >= TOKEN_FUNCTION && token->type <= TOKEN_FALSE)) {
        
        int member_line = token->line;
        int member_col = token->column;
        char* member_name = strdup(token->value);
        
        advance(parser);
        ASTNode* member_node = ast_create_identifier(token->value, token->line, token->column);
        ASTNode* access_node = ast_create_index_access(object, member_node);
        
        char full_name[256];
        snprintf(full_name, sizeof(full_name), "%s.%s", 
                 object->identifier.name, member_name);
        
        const BuiltinSig* builtin = lookup_builtin(full_name);
        int sym_idx = symbol_index_recursive(parser, full_name);
        
        bool is_function = (builtin != NULL) || 
                          (sym_idx >= 0 && parser->symbols.kinds[sym_idx] == PARSER_SYM_FUNCTION);
        bool is_variable = (sym_idx >= 0 && 
                           (parser->symbols.kinds[sym_idx] == PARSER_SYM_VARIABLE ||
                            parser->symbols.kinds[sym_idx] == PARSER_SYM_PARAMETER));
        
        if (is_function) {
            if (check(parser, TOKEN_LPAREN)) {
                free(member_name);
                return parse_call(parser, access_node);
            } else {
                parser_error_at(parser, member_line, member_col, 
                              (int)strlen(member_name),
                              "Function '%s' must be called with parentheses '()'",
                              full_name);
                free(member_name);
                return access_node;
            }
        } else if (is_variable) {
            if (check(parser, TOKEN_LPAREN)) {
                parser_error_at(parser, member_line, member_col, 
                              (int)strlen(member_name),
                              "Variable '%s' cannot be called with parentheses",
                              full_name);
                free(member_name);
                return access_node;
            }
            free(member_name);
            return access_node;
        } else {
            if (check(parser, TOKEN_LPAREN)) {
                free(member_name);
                return parse_call(parser, access_node);
            }
            free(member_name);
            return access_node;
        }
    }
    parser_error(parser, "Expected identifier after '.'");
    return object;
}

// parses a prefix expression (primary or unary)
static ASTNode* parse_prefix(Parser* parser) {
    Token* token = current_token(parser);
    switch (token->type) {
        case TOKEN_NUMBER:     return parse_number(parser);
        case TOKEN_STRING:     return parse_string(parser);
        case TOKEN_NONE:       return parse_none(parser);
        case TOKEN_TRUE:
        case TOKEN_FALSE:      return parse_bool(parser);
        case TOKEN_IDENTIFIER: return parse_identifier(parser);
        case TOKEN_LBRACKET:   return parse_table_literal(parser);
        case TOKEN_LPAREN:     return parse_group(parser);
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

// parses a function call
static ASTNode* parse_call(Parser* parser, ASTNode* callee) {
    advance(parser);
    ASTNodeList* arguments = ast_list_create();
    
    skip_newlines(parser);
    if (!check(parser, TOKEN_RPAREN)) {
        while (true) {
            skip_newlines(parser);
            ast_list_add(arguments, parse_expression(parser));
            
            skip_newlines(parser);
            if (!match(parser, TOKEN_COMMA)) break;
            skip_newlines(parser);
        }
    }
    skip_newlines(parser);
    consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
    return ast_create_call(callee, arguments);
}

// parses an infix expression (binary, call, or access)
static ASTNode* parse_infix(Parser* parser, ASTNode* left) {
    Token* token = current_token(parser);
    
    switch (token->type) {
        case TOKEN_LPAREN:
            return parse_call(parser, left);
        case TOKEN_LBRACKET:
            return parse_index_access(parser, left);
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
            advance(parser);
            ASTNode* right = parse_precedence(parser, PREC_ASSIGNMENT - 1);
            if (left->type == AST_IDENTIFIER) {
                return ast_create_var_assign(
                    left->identifier.name, right, false, NULL,
                    token->line, token->column);
            } else if (left->type == AST_INDEX_ACCESS) {
                char* name = NULL;
                if (left->access.object->type == AST_IDENTIFIER) {
                    name = left->access.object->identifier.name;
                }
                return ast_create_var_assign(
                    name, right, false, left,
                    token->line, token->column);
            }
            parser_error_at(parser, left->line, left->column, get_node_len(left),
                            "Invalid assignment target");
            return NULL;
        }
        default:
            return NULL;
    }
}

// core Pratt parser that handles precedence climbing
static ASTNode* parse_precedence(Parser* parser, Precedence precedence) {
    ASTNode* left = parse_prefix(parser);
    while (true) {
        Token* token = current_token(parser);
        Precedence current_prec = get_precedence(token->type);
        if (token->type == TOKEN_LPAREN || token->type == TOKEN_LBRACKET || token->type == TOKEN_DOT) {
            current_prec = PREC_CALL;
        }
        if (current_prec <= precedence) break;
        left = parse_infix(parser, left);
    }
    return left;
}

// parses an expression using Pratt parser
static ASTNode* parse_expression(Parser* parser) {
    skip_newlines(parser);
    if (check(parser, TOKEN_EOF)) {
        return NULL;
    }
    ASTNode* expr = parse_precedence(parser, PREC_NONE);
    if (expr) {
        parser_check_expression(parser, expr);
    }
    return expr;
}

// parses a variable declaration or assignment
static ASTNode* parse_var_decl_or_assign(Parser* parser) {
    Token* name = current_token(parser);
    advance(parser);

    consume(parser, TOKEN_EQUAL, "Expected '=' in assignment");
    
    if (check(parser, TOKEN_NEWLINE) || check(parser, TOKEN_EOF)) {
        parser_error_at(parser, name->line, name->column + (int)strlen(name->value) + 1, 1,
                       "Expected expression after '='");
        return ast_create_var_assign(name->value, NULL, !parser_is_declared(parser, name->value), NULL,
                                    name->line, name->column);
    }
    
    ASTNode* value = parse_expression(parser);
    
    if (!value) {
        parser_error_at(parser, name->line, name->column + (int)strlen(name->value) + 1, 1,
                       "Expected expression after '='");
        return ast_create_var_assign(name->value, NULL, !parser_is_declared(parser, name->value), NULL,
                                    name->line, name->column);
    }

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
            if (old_type == TYPE_UNKNOWN && new_type != TYPE_ERROR) {
                parser->symbols.types[idx] = new_type;
            } else if (new_type != TYPE_ERROR && new_type != TYPE_UNKNOWN) {
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

// parses a function declaration with parameters and body
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
    if (parser->function_depth > APEX_MAX_CALL_DEPTH) {
        parser_error_at(parser, name->line, name->column, (int)strlen(name->value),
            "Function nesting exceeds maximum depth of %d", APEX_MAX_CALL_DEPTH);
    }
    parser_enter_scope(parser);

    for (int i = 0; i < params->count; i++) {
        ASTNode* param = params->nodes[i];
        parser_declare_symbol(parser, param->param.name, PARSER_SYM_PARAMETER,
                              TYPE_ANY, 0, param->line, param->column);
    }

    ASTNode* body = parse_block(parser, true, "function");

    parser_exit_scope(parser);
    parser->function_depth--;

    return ast_create_function(name->value, params, body, name->line, name->column);
}

// parses an if statement with elif and else clauses
static ASTNode* parse_if_statement(Parser* parser) {
    advance(parser);
    ASTNode* condition = parse_expression(parser);
    parser_check_condition(parser, condition, "If");

    ASTNode* then_branch = parse_block(parser, true, "if");
    
    ASTNode* elif_chain = NULL;
    ASTNode* else_branch = NULL;
    
    skip_newlines(parser);
    
    while (check(parser, TOKEN_ELIF)) {
        Token* elif_kw = advance(parser);
        ASTNode* elif_cond = parse_expression(parser);
        parser_check_condition(parser, elif_cond, "If");
        ASTNode* elif_body = parse_block(parser, true, "elif");
        
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
        else_branch = parse_block(parser, true, "else");
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

// checks if a token type is a valid import segment
static bool is_valid_import_segment(TokenType type) {
    if (type == TOKEN_IDENTIFIER) return true;
    if (type >= TOKEN_FUNCTION && type <= TOKEN_FALSE) return true;
    if (type == TOKEN_NUMBER || type == TOKEN_STRING) return true;
    return false;
}

// parses a for loop with range, table, or condition-based iteration
static ASTNode* parse_for_statement(Parser* parser) {
    Token* for_kw = advance(parser);
    skip_newlines(parser);
    ASTNode* condition = NULL;
    char* var_name = NULL;
    ASTNode* start = NULL, *end = NULL, *step = NULL;
    int var_line = for_kw->line;
    int var_col = for_kw->column;
    bool is_table_iter = false;

    if (check(parser, TOKEN_NEWLINE) || check(parser, TOKEN_INDENT)) {
        parser_error_at(parser, for_kw->line, for_kw->column, 3,
            "Use an explicit condition for 'for' (e.g., 'for running == true').");
        if (match(parser, TOKEN_NEWLINE)) {
            while (!check(parser, TOKEN_EOF) && !check(parser, TOKEN_DEDENT)) {
                advance(parser);
            }
            match(parser, TOKEN_DEDENT);
        }
        return NULL;
    }

    if (!check(parser, TOKEN_IDENTIFIER)) {
        Token* bad_token = current_token(parser);
        int len = bad_token->value ? (int)strlen(bad_token->value) : 1;
        
        if (bad_token->type == TOKEN_STRING) {
            len += 2;
        }
        
        const char* got = NULL;
        switch (bad_token->type) {
            case TOKEN_NUMBER: got = "number"; break;
            case TOKEN_STRING: got = "string"; break;
            case TOKEN_TRUE:
            case TOKEN_FALSE: got = "boolean"; break;
            default: got = token_type_name(bad_token->type); break;
        }
        
        parser_error_at(parser, bad_token->line, bad_token->column, len,
                    "Expected variable name, got %s", got);
        
        while (!check(parser, TOKEN_NEWLINE) && !check(parser, TOKEN_EOF)) {
            advance(parser);
        }
        return NULL;
    }

    if (check(parser, TOKEN_IDENTIFIER)) {
        Token* id_tok = advance(parser);
        var_line = id_tok->line;
        var_col = id_tok->column;
        if (match(parser, TOKEN_EQUAL)) {
            bool is_number_next = check(parser, TOKEN_NUMBER);
            if (!is_number_next && check(parser, TOKEN_MINUS) && peek(parser, 1)->type == TOKEN_NUMBER) {
                is_number_next = true;
            }

            if (is_number_next) {
                var_name = strdup(id_tok->value);
                start = parse_expression(parser);
                parser_check_number_expr(parser, start, "For loop start");
                
                if (check(parser, TOKEN_COMMA)) {
                    consume(parser, TOKEN_COMMA, "Expected ',' after start value");
                    end = parse_expression(parser);
                    parser_check_number_expr(parser, end, "For loop end");
                    if (match(parser, TOKEN_COMMA)) {
                        step = parse_expression(parser);
                        parser_check_number_expr(parser, step, "For loop step");
                        double step_val;
                        if (step && evaluate_numeric_constant(parser, step, &step_val) && step_val == 0.0) {
                            parser_error_at(parser, step->line, step->column, 0, "For loop step cannot be zero");
                        }
                    }
                } else {
                    Token* bad = current_token(parser);
                    int len = bad->value ? (int)strlen(bad->value) : 1;
                    if (bad->type == TOKEN_STRING) len += 2;
                    
                    parser_error_at(parser, bad->line, bad->column, len,
                                "Expected ',' after start value");
                    free(var_name); var_name = NULL;
                }
            } else {
                var_name = strdup(id_tok->value);
                start = parse_expression(parser);
                is_table_iter = true;
            }
        } else {
            Token* next = current_token(parser);
            
            if (next->type == TOKEN_EQUAL_EQUAL || next->type == TOKEN_NOT_EQUAL ||
                next->type == TOKEN_LESS || next->type == TOKEN_GREATER ||
                next->type == TOKEN_LESS_EQUAL || next->type == TOKEN_GREATER_EQUAL ||
                next->type == TOKEN_AND || next->type == TOKEN_OR) {
                parser->current--;
                condition = parse_expression(parser);
                parser_check_condition(parser, condition, "For");
            } else {
                int len = next->value ? (int)strlen(next->value) : 1;
                if (next->type == TOKEN_STRING) len += 2;
                
                parser_error_at(parser, next->line, next->column, len,
                              "Expected '=' after variable name in for loop");
                while (!check(parser, TOKEN_NEWLINE) && !check(parser, TOKEN_EOF)) {
                    advance(parser);
                }
                free(id_tok->value);
                return NULL;
            }
        }
    }

    parser->loop_depth++;
    if (parser->loop_depth > APEX_MAX_LOOP_DEPTH) {
        parser_error_at(parser, for_kw->line, for_kw->column, 3,
        "Loop nesting exceeds maximum depth of %d", APEX_MAX_LOOP_DEPTH);
    }

    parser_enter_scope(parser);
    if (var_name) {
        ValueType vtype = is_table_iter ? TYPE_ANY : TYPE_NUMBER;
        parser_declare_symbol(parser, var_name, PARSER_SYM_VARIABLE, vtype, 0, var_line, var_col);
    }

    ASTNode* body = parse_block(parser, true, "for");
    parser_exit_scope(parser);
    parser->loop_depth--;

    return ast_create_for(var_name, condition, start, end, step, body, for_kw->line, for_kw->column);
}

// parses an import statement with module file loading
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

    ASTNode* import_node = ast_create_import(module_path, import_kw->line, import_kw->column);
    parser_validate_import_file(parser, module_path, first->line, first->column);
    parser_register_import(parser, module_path, import_kw->line, import_kw->column);

    char first_segment[256];
    const char* dot = strchr(module_path, '.');
    size_t first_len = dot ? (size_t)(dot - module_path) : strlen(module_path);
    if (first_len >= sizeof(first_segment)) first_len = sizeof(first_segment) - 1;
    memcpy(first_segment, module_path, first_len);
    first_segment[first_len] = '\0';

    if (is_builtin_module_root(first_segment)) {
        free(module_path);
        return import_node;
    }

    char full_path[PATH_MAX];
    if (!build_module_path(parser, module_path, full_path, sizeof(full_path))) {
        parser_error(parser, "Module path too long");
        free(module_path);
        return import_node;
    }

    FILE* f = fopen(full_path, "rb");
    if (!f) {
        parser_error(parser, "Cannot open module file");
        free(module_path);
        return import_node;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* source = (char*)malloc(size + 1);
    if (!source) { 
        fclose(f); 
        free(module_path); 
        return import_node; 
    }
    
    size_t bytes_read = fread(source, 1, size, f);
    if (bytes_read != (size_t)size) {
        fclose(f);
        free(source);
        free(module_path);
        parser_error(parser, "Failed to read module file");
        return import_node;
    }
    source[size] = '\0';
    fclose(f);

    Tokenizer* mod_tok = tokenizer_create(source, full_path);
    int mod_count;
    Token* mod_tokens = tokenizer_tokenize(mod_tok, &mod_count);

    Parser* mod_parser = parser_create(mod_tokens, mod_count, full_path, source);
    mod_parser->semantic_checks = parser->semantic_checks;
    free(mod_parser->source_dir);
    mod_parser->source_dir = strdup(parser->source_dir);

    ASTNode* mod_ast = parser_parse(mod_parser);

    if (parser_had_errors(mod_parser)) {
        parser->error_count += mod_parser->error_count;
    }

    for (int i = 0; i < mod_parser->symbols.count; i++) {
        if (mod_parser->symbols.scope_levels[i] == 0) {
            char full_name[512];
            snprintf(full_name, sizeof(full_name), "%s.%s", 
                     first_segment, mod_parser->symbols.names[i]);
            
            parser_declare_symbol(parser, full_name, 
                                mod_parser->symbols.kinds[i],
                                mod_parser->symbols.types[i],
                                mod_parser->symbols.param_counts[i],
                                import_kw->line, import_kw->column);
        }
    }

    parser_destroy(mod_parser);
    tokenizer_destroy(mod_tok);
    free(source);
    free(module_path);
#undef APPEND_PATH

    if (!mod_ast) return import_node;

    ast_free_node(import_node);
    return ast_create_module_block(first_segment, mod_ast, import_kw->line, import_kw->column);
}

// parses a return statement
static ASTNode* parse_return_statement(Parser* parser) {
    Token* return_kw = advance(parser);

    if (parser->semantic_checks && parser->function_depth == 0) {
        parser_error_at(parser, return_kw->line, return_kw->column, 6,
            "Return statement outside of function");
    }

    ASTNode* value = NULL;
    if (!check(parser, TOKEN_NEWLINE) && !check(parser, TOKEN_EOF)) {
        value = parse_expression(parser);
    }
    
    return ast_create_return(value, return_kw->line, return_kw->column);
}

// parses a break statement
static ASTNode* parse_break_statement(Parser* parser) {
    Token* break_kw = advance(parser);
    if (parser->semantic_checks && parser->loop_depth == 0) {
        parser_error_at(parser, break_kw->line, break_kw->column, 5,
            "'break' statement outside of loop");
    }
    return ast_create_node(AST_BREAK_STMT, break_kw->line, break_kw->column);
}

// parses a continue statement
static ASTNode* parse_continue_statement(Parser* parser) {
    Token* cont_kw = advance(parser);
    if (parser->semantic_checks && parser->loop_depth == 0) {
        parser_error_at(parser, cont_kw->line, cont_kw->column, 8,
            "'continue' statement outside of loop");
    }
    return ast_create_node(AST_CONTINUE_STMT, cont_kw->line, cont_kw->column);
}

// parses a single statement, dispatching by token type
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
            
        case TOKEN_FOR:
            return parse_for_statement(parser);
            
        case TOKEN_RETURN:
            return parse_return_statement(parser);
            
        case TOKEN_BREAK:
            return parse_break_statement(parser);
            
        case TOKEN_CONTINUE:
            return parse_continue_statement(parser);
            
        case TOKEN_IDENTIFIER: {
            if (peek(parser, 1)->type == TOKEN_EQUAL) {
                ASTNode* node = parse_var_decl_or_assign(parser);
                if (node) return node;
            }

            ASTNode* expr = parse_expression(parser);
            if (expr) {
                parser_check_expr_statement(parser, expr);
                return ast_create_expr_stmt(expr);
            }
            return NULL;
        }
        
        case TOKEN_INDENT:
        case TOKEN_DEDENT:
            advance(parser);
            return NULL;

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

// parses a block of statements with indentation
static ASTNode* parse_block(Parser* parser, bool require_indent, const char* after_keyword) {
    ASTNodeList* statements = ast_list_create();

    if (require_indent) {
        skip_newlines(parser);

        if (!match(parser, TOKEN_INDENT)) {
            Token* tok = current_token(parser);
            
            int line_len = get_line_length(parser->source, tok->line);
            int len = line_len - (tok->column - 1);
            if (len < 1) len = 1;

            parser_error_at(parser, tok->line, tok->column, len,
                            "Expected indented block after '%s' (next line must start with 4 spaces)",
                            after_keyword ? after_keyword : "block");
            return ast_create_block(statements);
        }
    }

    while (!check(parser, TOKEN_EOF) && !check(parser, TOKEN_DEDENT)) {
        if (check(parser, TOKEN_INDENT)) {
            advance(parser);
            continue;
        }
        
        int before = parser->current;
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            ast_list_add(statements, stmt);
        }
        if (parser->current == before && !check(parser, TOKEN_EOF) && !check(parser, TOKEN_DEDENT)) {
            Token* tok = current_token(parser);
            int len = tok->value ? (int)strlen(tok->value) : 1;
            parser_error_at(parser, tok->line, tok->column, len,
                "Unexpected token in block");
            advance(parser);
        }
        skip_newlines(parser);
    }

    match(parser, TOKEN_DEDENT);

    return ast_create_block(statements);
}

// parses the entire program as a sequence of statements
static ASTNode* parse_program(Parser* parser) {
    ASTNodeList* statements = ast_list_create();
    
    int prev_pos = -1;
    
    while (!check(parser, TOKEN_EOF)) {
        if (parser->current == prev_pos) {
            break;
        }
        prev_pos = parser->current;
        
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            ast_list_add(statements, stmt);
        }
        
        skip_newlines(parser);
    }
    
    ASTNode* program = ast_create_block(statements);
    program->type = AST_PROGRAM;
    return program;
}

// main public parse function
ASTNode* parser_parse(Parser* parser) {
    return parse_program(parser);
}
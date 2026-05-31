#ifndef SEMA_H
#define SEMA_H

#include "ast.h"
#include <stdbool.h>

// ========== Symbol Types ==========
typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER,
    SYMBOL_MODULE,
    SYMBOL_BUILTIN,      // built-in functions (os.output, math.abs, etc.)
} SymbolKind;

// ========== Type System ==========
typedef enum {
    TYPE_UNKNOWN,        // not yet determined
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_BOOLEAN,
    TYPE_TABLE,
    TYPE_FUNCTION,
    TYPE_ERROR,          // type error marker
    TYPE_ANY,            // for built-in functions with varying return types
} ValueType;

// ========== Symbol Entry ==========
typedef struct Symbol {
    char* name;
    SymbolKind kind;
    ValueType value_type;      // actual value type
    ValueType declared_type;   // declared type (from annotation)
    bool is_mutable;           // whether the type can change (no in Apex)
    bool is_initialized;       // whether a value has been assigned
    int scope_level;
    int line;
    int column;
    
    // For functions
    struct {
        int param_count;
        ValueType* param_types;  // parameter types
        bool has_return;
        ValueType return_type;
    } func_info;
    
    struct Symbol* next;      // hash chain link
} Symbol;

// ========== Scope ==========
typedef struct Scope {
    Symbol** symbols;        // hash table of symbols
    int capacity;
    int level;
    struct Scope* parent;
    bool is_function_scope;  // enables return checking
    bool is_loop_scope;      // enables break/continue checking
} Scope;

// ========== Semantic Analyzer ==========
typedef struct {
    Scope* global_scope;
    Scope* current_scope;
    ASTNode* ast;
    char* filename;
    
    // Error counters
    int error_count;
    int warning_count;
    
    // Context tracking
    bool in_loop;
    bool in_function;
    
    // Built-in function table
    Scope* builtins;
    
    // Imported module info
    // In a full implementation this would be a list of loaded modules
} SemAnalyzer;

// ========== API ==========
SemAnalyzer* sema_create(const char* filename);
void sema_destroy(SemAnalyzer* sema);
bool sema_analyze(SemAnalyzer* sema, ASTNode* ast);
int sema_get_error_count(SemAnalyzer* sema);

// Analysis result output
void sema_print_symbols(SemAnalyzer* sema);
void sema_print_errors(SemAnalyzer* sema);

#endif // SEMA_H
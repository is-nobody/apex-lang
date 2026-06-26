#ifndef VM_H
#define VM_H

#define VM_MAX_CALL_FRAMES 4096
#define VM_MAX_FRAMES VM_MAX_CALL_FRAMES
#define VM_REGS_PER_FRAME 32

#include "bytecode.h"
#include <stdbool.h>

// ========== Value Types ==========
typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_TABLE,
    VAL_FUNCTION,
} ValueType_VM;

// ========== Reference Counted Header ==========
typedef struct {
    int ref_count;
    ValueType_VM type;
} RefCountedObject;

// ========== String Object ==========
typedef struct StringObject {
    RefCountedObject header;
    uint32_t hash;
    bool hash_computed;
    int length;
    char chars[];
} StringObject;

// ========== Value ==========
typedef struct Value {
    ValueType_VM type;
    union {
        double number;
        bool boolean;
        int function_addr;
        StringObject* string;
        struct Table* table;
    };
} Value;

// ========== FastTable with array part ==========
#define TABLE_MAX_LOAD 0.75
#define TABLE_ARRAY_INIT 64
#define TABLE_TOTAL_COUNT(t) ((t)->array_count + (t)->hash_count)

typedef struct TableEntry {
    char* key;
    Value value;
    struct TableEntry* next;
} TableEntry;

typedef struct Table {
    RefCountedObject header;
    // Hash part (string keys)
    TableEntry** entries;
    int capacity;
    int hash_count;
    // Array part (integer keys 1..N) — O(1) direct access!
    Value* array_part;
    int array_capacity;
    int array_count;
    // Total count = hash_count + array_count
} Table;

// ========== String Intern Table ==========
#define INTERN_INITIAL_SIZE 16384
#define INTERN_MAX_LOAD 0.75

typedef struct {
    StringObject** buckets;
    int capacity;
    int count;
} StringInternTable;

// ========== Object Pool ==========
#define POOL_STRING_SIZE 512
#define POOL_MAX_ITEMS 1024

typedef struct {
    StringObject* string_pool[POOL_MAX_ITEMS];
    int string_pool_count;
    Table* table_pool[POOL_MAX_ITEMS / 4];
    int table_pool_count;
} ObjectPool;

// ========== VM ==========
#define VM_MAX_CALL_FRAMES 4096
#define VM_MAX_GLOBALS 512
#define VM_MAX_ARGS_STACK 64

typedef struct {
    // Register frames array
    Value* register_frames;
    Value* registers;
    int current_frame;
    int register_count;
    
    // Global variables
    Value globals[VM_MAX_GLOBALS];
    int global_count;
    
    // Argument passing stack
    Value args_stack[VM_MAX_ARGS_STACK];
    int args_top;
    
    // Call stack
    struct {
        int return_address;
        int base_register;
        int base_iterator_depth;
        int frame_index;
        int dest_reg;
    } call_stack[VM_MAX_CALL_FRAMES];
    int call_depth;
    
    // Current bytecode
    BytecodeChunk* chunk;
    Instruction* code;
    int code_count;
    
    // Program counter
    int pc;
    
    // Flags
    bool running;
    bool had_error;
    
    // Fast numeric loop stack
    struct {
        double index;
        double end;
        double step;
    } iterator_stack[VM_MAX_CALL_FRAMES];
    int iterator_depth;
    
    // String interning
    StringInternTable intern_table;
    
    // Object pool for recycling
    ObjectPool obj_pool;
    
    const char* source;
} VM;

// ========== Value API ==========
Value vm_make_number(double value);
Value vm_make_string(const char* value);
Value vm_make_bool(bool value);
Value vm_make_table();
Value vm_copy_value(Value value);
void vm_free_value(Value* value);
const char* vm_value_type_name(Value* value);
void vm_print_value(Value* value);

// ========== Table API ==========
Table* table_create(int capacity);
void table_destroy(Table* table);
bool table_set(Table* table, const char* key, Value value);
bool table_get(Table* table, const char* key, Value* out_value);
bool table_has(Table* table, const char* key);
void table_remove(Table* table, const char* key);
int table_size(Table* table);
char** table_keys(Table* table, int* out_count);
void table_clear(Table* table);
Table* table_copy(Table* table);

// ========== Fast integer-indexed table operations ==========
bool table_set_int(Table* table, int index, Value value);
bool table_get_int(Table* table, int index, Value* out_value);
void table_append(Table* table, Value value);

// ========== String Intern API ==========
StringObject* string_intern(StringInternTable* it, const char* chars, int length);
void string_intern_table_init(StringInternTable* it);
void string_intern_table_free(StringInternTable* it);

// ========== Object Pool API ==========
StringObject* string_create_pooled(ObjectPool* pool, const char* chars, int length);
void string_destroy_pooled(ObjectPool* pool, StringObject* str);
Table* table_create_pooled(ObjectPool* pool, int capacity);
void table_destroy_pooled(ObjectPool* pool, Table* table);
void object_pool_init(ObjectPool* pool);
void object_pool_free(ObjectPool* pool);

// ========== VM API ==========
VM* vm_create(const char* source);
void vm_destroy(VM* vm);
bool vm_execute(VM* vm, BytecodeChunk* chunk);
void vm_dump_state(VM* vm);
void vm_dump_registers(VM* vm);
void value_incref(Value* v);
void value_decref(Value* v);

#endif // VM_H
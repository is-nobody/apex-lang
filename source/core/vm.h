#ifndef VM_H
#define VM_H

#include "bytecode.h"
#include <stdbool.h>

// ========== VM Configuration Constants ==========

// call stack limits
#define VM_MAX_CALL_FRAMES 8192
#define VM_MAX_FRAMES VM_MAX_CALL_FRAMES
#define VM_REGS_PER_FRAME 32
#define VM_MAX_GLOBALS 512
#define VM_MAX_ARGS_STACK 64

// table load factor for hash part resizing
#define TABLE_MAX_LOAD 0.75
#define TABLE_ARRAY_INIT 64

// string intern table configuration
#define INTERN_INITIAL_SIZE 16384
#define INTERN_MAX_LOAD 0.75

// object pool configuration
#define POOL_STRING_SIZE 512
#define POOL_MAX_ITEMS 1024

// helper macro for total table entry count
#define TABLE_TOTAL_COUNT(t) ((t)->array_count + (t)->hash_count)

// ========== Value Types ==========
typedef enum {
    VAL_NUMBER,          // double-precision floating point
    VAL_STRING,          // interned string object
    VAL_NONE,            // null/nil value
    VAL_BOOL,            // boolean true or false
    VAL_TABLE,           // table/array with both hash and array parts
    VAL_FUNCTION,        // compiled function reference (by address)
} ValueType_VM;

// reference counting header for garbage-collected objects
typedef struct {
    int ref_count;       // number of references to this object
    ValueType_VM type;   // discriminator for the object type
} RefCountedObject;

// string object with interned storage, hash cache, and flexible array for data
typedef struct StringObject {
    RefCountedObject header;
    uint32_t hash;           // cached hash for faster comparisons
    bool hash_computed;      // whether the hash has been calculated
    int length;              // string length in characters
    char chars[];            // flexible array member for the actual string data
} StringObject;

// value union that can hold any runtime type
typedef struct Value {
    ValueType_VM type;
    union {
        double number;
        bool boolean;
        int function_addr;   // bytecode address for function calls
        StringObject* string;
        struct Table* table;
    };
} Value;

// hash table entry with chaining for collisions
typedef struct TableEntry {
    char* key;               // string key (interned)
    Value value;             // associated value
    struct TableEntry* next; // next entry in the chain
} TableEntry;

// fast table with separate array part for integer keys (O(1) direct access)
typedef struct Table {
    RefCountedObject header;
    TableEntry** entries;    // hash buckets for string keys
    int capacity;            // hash table capacity
    int hash_count;          // number of entries in hash part
    Value* array_part;       // dense array for integer keys starting from 1
    int array_capacity;      // allocated size of array_part
    int array_count;         // number of valid entries in array part
} Table;

// string interning table for deduplication and fast equality
typedef struct {
    StringObject** buckets;  // hash buckets for interned strings
    int capacity;
    int count;
} StringInternTable;

// object pool for reusing frequently allocated objects
typedef struct {
    StringObject* string_pool[POOL_MAX_ITEMS];
    int string_pool_count;
    Table* table_pool[POOL_MAX_ITEMS / 4];
    int table_pool_count;
} ObjectPool;

// main virtual machine state with registers, call stack, and execution context
typedef struct {
    Value* register_frames;  // flattened array of all register frames
    Value* registers;        // pointer to current frame's registers
    int current_frame;       // index of the current frame in call_stack
    int register_count;      // total registers allocated for all frames
    
    Value globals[VM_MAX_GLOBALS];
    int global_count;
    
    Value args_stack[VM_MAX_ARGS_STACK]; // stack for passing arguments
    int args_top;
    
    struct {
        int return_address;   // pc to resume after call
        int base_register;    // register base for the frame
        int base_iterator_depth; // saved loop iterator depth
        int frame_index;      // index for debug info
        int dest_reg;         // destination register for return value
    } call_stack[VM_MAX_CALL_FRAMES];
    int call_depth;
    
    BytecodeChunk* chunk;     // currently executing bytecode
    Instruction* code;        // pointer to chunk's code array for fast access
    int code_count;           // total instructions in the chunk
    
    int pc;                   // program counter (next instruction to execute)
    
    bool running;             // whether execution is active
    bool had_error;           // whether an error occurred during execution
    
    struct {
        double index;         // current loop index
        double end;           // loop end bound
        double step;          // loop step value
    } iterator_stack[VM_MAX_CALL_FRAMES];
    int iterator_depth;       // nesting depth of active numeric loops
    
    StringInternTable intern_table; // global string interning table
    ObjectPool obj_pool;      // object recycling pool

    const char* source;       // source code for error reporting
} VM;

// creates a value from a double number
Value vm_make_number(double value);

// creates a value from a string (interns it)
Value vm_make_string(const char* value);

// creates a none/null value (represents absence of a value)
Value vm_make_none(void);

// creates a boolean value
Value vm_make_bool(bool value);

// creates a new empty table value
Value vm_make_table();

// copies a value with proper reference counting
Value vm_copy_value(Value value);

// decrements reference count and frees if zero
void vm_free_value(Value* value);

// returns a human-readable type name for a value
const char* vm_value_type_name(Value* value);

// prints a value to stdout for debugging
void vm_print_value(Value* value);

// creates a new table with the given initial hash capacity
Table* table_create(int capacity);

// frees a table and all its contents
void table_destroy(Table* table);

// sets a string-keyed value in the table, returns true on success
bool table_set(Table* table, const char* key, Value value);

// gets a string-keyed value from the table, returns true if found
bool table_get(Table* table, const char* key, Value* out_value);

// checks if a string key exists in the table
bool table_has(Table* table, const char* key);

// removes a string-keyed entry from the table
void table_remove(Table* table, const char* key);

// returns the total number of entries in the table
int table_size(Table* table);

// returns an array of all string keys in the table
char** table_keys(Table* table, int* out_count);

// removes all entries from the table
void table_clear(Table* table);

// creates a shallow copy of the table
Table* table_copy(Table* table);

// sets a value by integer index (uses array part if possible)
bool table_set_int(Table* table, int index, Value value);

// gets a value by integer index (checks array part first)
bool table_get_int(Table* table, int index, Value* out_value);

// appends a value to the array part of the table
void table_append(Table* table, Value value);

// interns a string, returns a canonical StringObject pointer
StringObject* string_intern(StringInternTable* it, const char* chars, int length);

// initializes the string intern table
void string_intern_table_init(StringInternTable* it);

// frees the string intern table
void string_intern_table_free(StringInternTable* it);

// creates a pooled string object (reuses from pool if available)
StringObject* string_create_pooled(ObjectPool* pool, const char* chars, int length);

// returns a string to the pool for reuse
void string_destroy_pooled(ObjectPool* pool, StringObject* str);

// creates a pooled table (reuses from pool if available)
Table* table_create_pooled(ObjectPool* pool, int capacity);

// returns a table to the pool for reuse
void table_destroy_pooled(ObjectPool* pool, Table* table);

// initializes the object pool
void object_pool_init(ObjectPool* pool);

// frees all objects in the pool
void object_pool_free(ObjectPool* pool);

// creates a new vm instance with the given source code
VM* vm_create(const char* source);

// destroys a vm instance and frees all resources
void vm_destroy(VM* vm);

// executes the given bytecode chunk in the vm
bool vm_execute(VM* vm, BytecodeChunk* chunk);

// increments the reference count of a value
void value_incref(Value* v);

// decrements the reference count of a value
void value_decref(Value* v);

#endif // VM_H
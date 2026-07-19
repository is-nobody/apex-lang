#ifndef VM_H
#define VM_H

#include "bytecode.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

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

// ========== NaN Boxing Value Representation ==========
// uses IEEE 754 double NaN space to encode type tags in the mantissa bits
// quiet NaN has bits 51 set, we use bits 48-50 for type tags

#define SIGN_BIT        ((uint64_t)0x8000000000000000ULL)
#define QNAN            ((uint64_t)0x7FF8000000000000ULL)

// type tags stored in bits 48-50 of the NaN mantissa
#define TAG_NONE        ((uint64_t)0)
#define TAG_NULL        ((uint64_t)1)
#define TAG_BOOL        ((uint64_t)2)
#define TAG_STRING      ((uint64_t)3)
#define TAG_TABLE       ((uint64_t)4)
#define TAG_FUNCTION    ((uint64_t)5)
#define TAG_NAN         ((uint64_t)6)

// mask for extracting type tag (bits 48-50)
#define TAG_MASK        ((uint64_t)0x7)
#define TAG_SHIFT       ((uint64_t)48)

// helper macros for constructing tagged values
#define MAKE_QNAN(tag)       (QNAN | ((uint64_t)(tag) << TAG_SHIFT))
#define MAKE_STRING(p)       (MAKE_QNAN(TAG_STRING) | ((uint64_t)(uintptr_t)(p) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
#define MAKE_TABLE(p)        (MAKE_QNAN(TAG_TABLE)  | ((uint64_t)(uintptr_t)(p) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
#define MAKE_FUNCTION(idx)   (MAKE_QNAN(TAG_FUNCTION) | ((uint64_t)(idx) & ((uint64_t)0x00000000FFFFFFFFULL)))
#define MAKE_NONE()          (MAKE_QNAN(TAG_NONE))
#define MAKE_BOOL(b)         (MAKE_QNAN(TAG_BOOL) | ((b) ? ((uint64_t)1) : ((uint64_t)0)))
#define MAKE_NUMBER(n) ({ \
    double _n = (n); \
    uint64_t _u; \
    if (isnan(_n)) { \
        _u = MAKE_QNAN(TAG_NAN); \
    } else { \
        memcpy(&_u, &_n, 8); \
    } \
    _u; \
})

// extraction macros
#define GET_TYPE(v)          (((v) & QNAN) == QNAN ? (ValueType_VM)(((v) >> TAG_SHIFT) & TAG_MASK) : VAL_NUMBER)
#define AS_NUMBER(v)         (IS_NAN(v) ? NAN : ({ uint64_t _v = (v); double _d; memcpy(&_d, &_v, 8); _d; }))
#define AS_STRING(v)         ((StringObject*)(uintptr_t)((v) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
#define AS_TABLE(v)          ((Table*)(uintptr_t)((v) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
#define AS_FUNCTION(v)       ((int)((v) & ((uint64_t)0x00000000FFFFFFFFULL)))
#define AS_BOOL(v)           (((v) & 1) != 0)

// type check macros
#define IS_NUMBER(v)         (((v) & QNAN) != QNAN)
#define IS_NAN(v)            (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_NAN))
#define IS_NONE(v)           ((v) == MAKE_QNAN(TAG_NONE))
#define IS_BOOL(v)           (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_BOOL))
#define IS_STRING(v)         (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_STRING))
#define IS_TABLE(v)          (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_TABLE))
#define IS_FUNCTION(v)       (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_FUNCTION))

// value type enum kept for compatibility
typedef enum {
    VAL_NUMBER,          // double-precision floating point (unboxed)
    VAL_STRING,          // interned string object (pointer tagged in NaN)
    VAL_NONE,            // null/nil value (tagged NaN)
    VAL_BOOL,            // boolean true or false (tagged NaN)
    VAL_TABLE,           // table/array (pointer tagged in NaN)
    VAL_FUNCTION,        // compiled function reference (tagged NaN)
} ValueType_VM;

// reference counting header for garbage-collected objects
typedef struct {
    int ref_count;       // number of references to this object
    ValueType_VM type;   // discriminator for the object type
} RefCountedObject;

// string object with interned storage, hash cache, and flexible array for data
typedef struct StringObject {
    RefCountedObject header; // reference counting header for memory management
    uint32_t hash;           // cached hash for faster comparisons
    bool hash_computed;      // whether the hash has been calculated
    int length;              // string length in characters
    char chars[];            // flexible array member for the actual string data
} StringObject;

// value is now a single 64-bit integer using NaN boxing
typedef uint64_t Value;

// hash table entry with chaining for collisions
typedef struct TableEntry {
    Value key;               // string key (interned)
    uint32_t hash;           // precomputed hash for fast comparison and bucketing
    Value value;             // associated value
    struct TableEntry* next; // next entry in the chain
} TableEntry;

// fast table with separate array part for integer keys (O(1) direct access)
typedef struct Table {
    RefCountedObject header; // reference counting header for memory management
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
    int capacity;            // total number of buckets
    int count;               // number of interned strings stored
} StringInternTable;

// object pool for reusing frequently allocated objects
typedef struct {
    StringObject* string_pool[POOL_MAX_ITEMS]; // pool of reusable string objects
    int string_pool_count;                     // number of strings currently in pool
    Table* table_pool[POOL_MAX_ITEMS / 4];     // pool of reusable table objects
    int table_pool_count;                      // number of tables currently in pool
} ObjectPool;

// state for "for key = table" iteration, walks array_part then hash buckets
typedef struct {
    Table* table;              // table being iterated
    int array_index;           // current position in array_part
    int bucket_index;          // current bucket in hash entries
    TableEntry* current_entry; // current node in bucket chain
} TableIterState;

// main virtual machine state with registers, call stack, and execution context
typedef struct {
    Value* register_frames;        // flattened array of all register frames (Value = uint64_t NaN-boxed)
    Value* registers;              // pointer to current frame's registers for fast access
    int current_frame;             // index of the currently active frame in call_stack
    int register_count;            // total registers allocated across all frames

    Value globals[VM_MAX_GLOBALS]; // global variable storage (persistent across frames)
    int global_count;              // number of initialized globals

    Value args_stack[VM_MAX_ARGS_STACK]; // stack for passing arguments to functions
    int args_top;                        // top index of the arguments stack

    struct {
        int return_address;        // instruction pointer to resume after call returns
        int base_register;         // register base offset for this frame
        int base_iterator_depth;   // saved loop iterator depth for nested loops
        int frame_index;           // frame index for debugging and stack traces
        int dest_reg;              // destination register for the return value
        int saved_args_top;        // saved args_top to restore after call
    } call_stack[VM_MAX_CALL_FRAMES];
    int call_depth;                // current call stack depth

    BytecodeChunk* chunk;          // currently executing bytecode chunk
    Instruction* code;             // pointer to chunk's code array for fast instruction dispatch
    int code_count;                // total number of instructions in the chunk
    int pc;                        // program counter (index of next instruction to execute)

    bool running;                  // whether the VM is actively executing
    bool had_error;                // whether an error occurred during execution

    struct {
        double index;              // current loop iteration value
        double end;                // loop end bound (inclusive/exclusive based on step)
        double step;               // loop step increment (positive or negative)
    } iterator_stack[VM_MAX_CALL_FRAMES];
    int iterator_depth;            // nesting depth of active numeric for-loops

    TableIterState table_iters[16]; // state for table iteration (for key = table loops)
    int table_iter_depth;           // nesting depth of active table iterators

    StringInternTable intern_table; // global string interning table for deduplication
    ObjectPool obj_pool;            // object recycling pool for performance

    const char* source;             // source code string for error reporting
} VM;

// creates a value from a double number (stored unboxed if not NaN)
Value vm_make_number(double value);

// creates a value from a string (interns it, stores pointer in NaN box)
Value vm_make_string(const char* value);

// creates a none/null value (special NaN tag)
Value vm_make_none(void);

// creates a boolean value (special NaN tag with boolean payload)
Value vm_make_bool(bool value);

// creates a new empty table value (pointer in NaN box)
Value vm_make_table(void);

// copies a value with proper reference counting
Value vm_copy_value(Value value);

// decrements reference count and frees if zero
void vm_free_value(Value value);

// returns a human-readable type name for a value
const char* vm_value_type_name(Value value);

// prints a value to stdout for debugging
void vm_print_value(Value value);

// creates a new table with the given initial hash capacity
Table* table_create(int capacity);

// frees a table and all its contents
void table_destroy(Table* table);

// sets a string-keyed value in the table, returns true on success
bool table_set(Table* table, Value key, Value value);

// gets a string-keyed value from the table, returns true if found
bool table_get(Table* table, Value key, Value* out_value);

// checks if a string key exists in the table
bool table_has(Table* table, Value key);

// removes a string-keyed entry from the table
void table_remove(Table* table, Value key);

// returns the total number of entries in the table
int table_size(Table* table);

// returns an array of all string keys in the table
Value* table_keys(Table* table, int* out_count);

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

// increments the reference count of a value (only for heap-allocated types)
void value_incref(Value v);

// decrements the reference count of a value (only for heap-allocated types)
void value_decref(Value v);

// returns the type of a NaN-boxed value
ValueType_VM value_get_type(Value v);

// creates a new string object (not interned, caller owns the reference)
StringObject* string_create(const char* chars, int length);

#endif // VM_H
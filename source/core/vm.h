// source/core/vm.h
// Implementation of Virtual Machine for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef VM_H
#define VM_H

#include "bytecode.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// global runtime toggle for the JIT; false means "never call jit"
#if APEX_JIT_ENABLED
extern bool apex_jit_runtime_enabled;
#endif

// platform abstractions for background worker coordination
#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION ApexMutex;          // windows critical section
    #define APEX_MUTEX_INIT(m)    InitializeCriticalSection(m)
    #define APEX_MUTEX_LOCK(m)    EnterCriticalSection(m)
    #define APEX_MUTEX_UNLOCK(m)  LeaveCriticalSection(m)
    #define APEX_MUTEX_DESTROY(m) DeleteCriticalSection(m)
#else
    #include <pthread.h>
    typedef pthread_mutex_t ApexMutex;           // posix mutex
    #define APEX_MUTEX_INIT(m)    pthread_mutex_init(m, NULL)
    #define APEX_MUTEX_LOCK(m)    pthread_mutex_lock(m)
    #define APEX_MUTEX_UNLOCK(m)  pthread_mutex_unlock(m)
    #define APEX_MUTEX_DESTROY(m) pthread_mutex_destroy(m)
#endif

// branch prediction hints for compiler optimization
#if defined(__GNUC__) || defined(__clang__)
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// call stack limits
#define VM_MAX_CALL_FRAMES 1024
#define VM_MAX_FRAMES VM_MAX_CALL_FRAMES
#define VM_MAX_GLOBALS 512
#define VM_MAX_ARGS_STACK 64

// register frame configuration
#define REGISTER_INITIAL_SIZE 16    // starting register count per frame
#define REGISTER_MAX_SIZE 4096      // safety cap per frame

// table load factor for hash part resizing
#define TABLE_MAX_LOAD 0.75
#define TABLE_ARRAY_INIT 64

// string intern table configuration
#define INTERN_INITIAL_SIZE 4096
#define INTERN_MAX_LOAD 0.75

// object pool configuration
#define POOL_MAX_ITEMS 1024

// helper macro for total table entry count
#define TABLE_TOTAL_COUNT(t) ((t)->array_count + (t)->hash_count)

// nan boxing value representation
// uses ieee 754 double nan space to encode type tags in the mantissa bits
// quiet nan has bits 51 set, we use bits 48-50 for type tags
#define QNAN            ((uint64_t)0x7FF8000000000000ULL)

// type tags stored in bits 48-50 of the nan mantissa
#define TAG_NONE        ((uint64_t)0)
#define TAG_BOOL        ((uint64_t)2)
#define TAG_STRING      ((uint64_t)3)
#define TAG_TABLE       ((uint64_t)4)
#define TAG_FUNCTION    ((uint64_t)5)
#define TAG_NAN         ((uint64_t)6)
#define TAG_FUTURE      ((uint64_t)7)

// mask for extracting type tag (bits 48-50)
#define TAG_MASK        ((uint64_t)0x7)
#define TAG_SHIFT       ((uint64_t)48)

// helper macros for constructing tagged values
#define MAKE_QNAN(tag)       (QNAN | ((uint64_t)(tag) << TAG_SHIFT))
#define MAKE_STRING(p)       (MAKE_QNAN(TAG_STRING) | ((uint64_t)(uintptr_t)(p) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
#define MAKE_TABLE(p)        (MAKE_QNAN(TAG_TABLE)  | ((uint64_t)(uintptr_t)(p) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
#define MAKE_FUNCTION(idx)   (MAKE_QNAN(TAG_FUNCTION) | ((uint64_t)(idx) & ((uint64_t)0x00000000FFFFFFFFULL)))
#define MAKE_FUTURE(p)       (MAKE_QNAN(TAG_FUTURE)   | ((uint64_t)(uintptr_t)(p) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
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
#define AS_FUTURE(v)         ((FutureObject*)(uintptr_t)((v) & ((uint64_t)0x0000FFFFFFFFFFFFULL)))
#define AS_BOOL(v)           (((v) & 1) != 0)

// type check macros
#define IS_NUMBER(v)         (((v) & QNAN) != QNAN)
#define IS_NAN(v)            (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_NAN))
#define IS_NONE(v)           ((v) == MAKE_QNAN(TAG_NONE))
#define IS_BOOL(v)           (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_BOOL))
#define IS_STRING(v)         (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_STRING))
#define IS_TABLE(v)          (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_TABLE))
#define IS_FUNCTION(v)       (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_FUNCTION))
#define IS_FUTURE(v)         (((v) & (QNAN | (TAG_MASK << TAG_SHIFT))) == MAKE_QNAN(TAG_FUTURE))

// value is a single 64-bit integer using nan boxing
typedef uint64_t Value;

// value type enum
typedef enum {
    VAL_NUMBER,          // double-precision floating point (unboxed)
    VAL_STRING,          // interned string object (pointer tagged in NaN)
    VAL_NONE,            // null/nil value (tagged NaN)
    VAL_BOOL,            // boolean true or false (tagged NaN)
    VAL_TABLE,           // table/array (pointer tagged in NaN)
    VAL_FUNCTION,        // compiled function reference (tagged NaN)
    VAL_FUTURE,          // resolved future object (pointer tagged in NaN)
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

// state for "for key = table" iteration, walks array_part then hash buckets
typedef struct {
    Table* table;              // table being iterated
    int array_index;           // current position in array_part
    int bucket_index;          // current bucket in hash entries
    TableEntry* current_entry; // current node in bucket chain
} TableIterState;

// state for a numeric for-loop, saved across coroutine suspension
typedef struct {
    double index;              // current loop iteration value
    double end;                // loop end bound (inclusive/exclusive based on step)
    double step;               // loop step increment (positive or negative)
} ForIter;

// future produced by an async call; also carries coroutine state while suspended
typedef struct FutureObject {
    RefCountedObject header;   // reference counting header for memory management
    Value result;              // the async body's return value, once resolved
    int state;                 // 0 = pending, 1 = resolved
    int func_idx;              // function table index of the async body
    int arg_count;             // number of captured arguments
    Value* args;               // heap array of captured argument values

    // per-coroutine register pool and frame bookkeeping (each coroutine owns
    // its own so concurrent coroutines never share frame slots)
    Value* register_pool;      // this coroutine's private register pool
    int* frame_offset;         // frame start offsets in register_pool
    int* frame_capacity;       // capacity of each frame in registers
    int* frame_used;           // highest register used per frame
    int pool_capacity;         // total pool capacity in registers
    int current_frame;         // active frame index within this pool
    bool owns_frame;           // true once pool was allocated, freed on completion

    int frame_idx;             // legacy: superseded by register_pool, kept for source compat
    int saved_ip;              // resume offset inside the body
    int saved_dest_reg;        // register that receives the awaited value
    Value awaiting;            // future we are currently waiting on (none if runnable)
    struct FutureObject** waiters;  // futures blocked on this one
    int waiter_count;          // number of waiters
    int waiter_capacity;       // capacity of the waiter array
    ForIter saved_iters[16];   // saved numeric loop iterators across suspension
    int saved_iter_depth;      // depth of the numeric iterator stack
    TableIterState saved_table_iters[4];  // saved table iterators across suspension
    int saved_table_iter_depth;// depth of the table iterator stack
} FutureObject;

// a finished background task waiting to be handed back to the scheduler
typedef struct Completion {
    FutureObject* fut;             // future to resolve (node owns one reference)
    Value result;                  // result value (node owns one reference if heap-tagged)
    struct Completion* next;       // next node in the pending list
} Completion;

// pending sleep that resolves its future when the deadline passes
typedef struct SleepTimer {
    double deadline;           // absolute seconds since epoch
    FutureObject* fut;         // future to resolve
    struct SleepTimer* next;   // linked list
} SleepTimer;

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

#if APEX_JIT_ENABLED
struct JITContext;   // forward declaration, only when JIT is compiled in
#endif

// main virtual machine state with registers, call stack, and execution context
typedef struct {
    Value* register_pool;            // single contiguous array for all frame registers
    int* frame_offset;               // offset of each frame in register_pool
    int* frame_capacity;             // capacity of each frame
    int* frame_used;                 // highest register used in each frame
    int pool_capacity;               // total size of register_pool
    Value* registers;                // hot pointer to current frame's registers
    int current_frame;               // index of the currently active frame

    Value globals[VM_MAX_GLOBALS];   // global variable storage (persistent across frames)
    int global_count;                // number of initialized globals

    Value args_stack[VM_MAX_ARGS_STACK]; // stack for passing arguments to functions
    int args_top;                        // top index of the arguments stack

    struct {
        int return_address;        // instruction pointer to resume after call returns
        int base_iterator_depth;   // saved loop iterator depth for nested loops
        int frame_index;           // frame index for restoring registers
        int dest_reg;              // destination register for the return value
        Value* saved_registers;    // cached pointer to caller's registers
    } call_stack[VM_MAX_CALL_FRAMES];
    
    int call_depth;                // current call stack depth

    BytecodeChunk* chunk;          // currently executing bytecode chunk
    Instruction* code;             // pointer to chunk's code array for fast instruction dispatch
    int code_count;                // total number of instructions in the chunk
    int pc;                        // program counter (index of next instruction to execute)

    bool running;                  // whether the VM is actively executing
    bool had_error;                // whether an error occurred during execution

    ForIter iterator_stack[VM_MAX_CALL_FRAMES];  // active numeric for-loops
    int iterator_depth;            // nesting depth of active numeric for-loops

    TableIterState table_iters[16]; // state for table iteration (for key = table loops)
    int table_iter_depth;           // nesting depth of active table iterators

    StringInternTable intern_table; // global string interning table for deduplication
    ObjectPool obj_pool;            // object recycling pool for performance

    const char* source;             // source code string for error reporting

#if APEX_JIT_ENABLED
    struct JITContext* jit;         // native JIT, NULL if unavailable / disabled
#endif

    Value args_table;               // table of command line arguments (1-indexed)

    Value frame_futures[VM_MAX_FRAMES];  // futures for each call frame
    FutureObject** ready;           // ready coroutine queue
    int ready_count;                // number of ready coroutines
    int ready_capacity;             // allocated capacity of ready queue
    FutureObject* current_task;     // coroutine currently executing, NULL at top level
    SleepTimer* timers;             // pending sleep timers
    ApexMutex completion_mutex;     // protects completions and pending_workers
    Completion* completions;        // queue of finished background tasks
    volatile int pending_workers;   // number of live worker threads
} VM;

// returns a human-readable type name for a value
const char* vm_value_type_name(Value value);

// prints a value to stdout for debugging
void vm_print_value(Value value);

// increments the reference count of a value (only for heap-allocated types)
void value_incref(Value v);

// decrements the reference count of a value (only for heap-allocated types)
void value_decref(Value v);

// creates a new string object (not interned, caller owns the reference)
StringObject* string_create(const char* chars, int length);

// interns a string, returns a canonical StringObject pointer
StringObject* string_intern(StringInternTable* it, const char* chars, int length);

// initializes the string intern table
void string_intern_table_init(StringInternTable* it);

// frees the string intern table
void string_intern_table_free(StringInternTable* it);

// creates a new table with the given initial hash capacity
Table* table_create(int capacity);

// frees a table and all its contents
void table_destroy(Table* table);

// sets a string-keyed value in the table, returns true on success
bool table_set(Table* table, Value key, Value value);

// gets a string-keyed value from the table, returns true if found
bool table_get(Table* table, Value key, Value* out_value);

// removes a string-keyed entry from the table
void table_remove(Table* table, Value key);

// returns the total number of entries in the table
int table_size(Table* table);

// returns an array of all string keys in the table
Value* table_keys(Table* table, int* out_count);

// sets a value by integer index (uses array part if possible)
bool table_set_int(Table* table, int index, Value value);

// creates a new vm instance with the given source code
VM* vm_create(const char* source);

// destroys a vm instance and frees all resources
void vm_destroy(VM* vm);

// executes the given bytecode chunk in the vm
bool vm_execute(VM* vm, BytecodeChunk* chunk);

// populates vm->args_table with user command line arguments (1-indexed)
void vm_set_args(VM* vm, int argc, char** argv, bool skip_script_name);

// returns current wall-clock time in seconds
double apex_now_seconds(void);

// starts a pending future as a coroutine and queues it for execution
void future_start(VM* vm, FutureObject* fut);

// resolves a future, waking its waiters
void future_resolve(VM* vm, FutureObject* fut, Value value);

// registers a timer that resolves the future after the given seconds
void vm_schedule_timer(VM* vm, double seconds, FutureObject* fut);

// drives the scheduler until the target future resolves
bool vm_drive_until(VM* vm, Value target, Value* out_result);

// pushes a completed background task for the scheduler to resolve
void vm_push_completion(VM* vm, FutureObject* fut, Value result);

// drains the completion queue, resolving each future (scheduler thread only)
int vm_drain_completions(VM* vm);

#endif // VM_H
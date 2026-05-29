#ifndef VM_H
#define VM_H

#define VM_MAX_FRAMES 1024       // Maximum recursion depth
#define VM_REGS_PER_FRAME 256    // Registers per call frame

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

// ========== Value ==========
typedef struct Value {
    ValueType_VM type;
    union {
        double number;
        char* string;
        bool boolean;
        int function_addr;
        struct Table* table;
    };
} Value;

// ========== Table (Hash Table) ==========
#define TABLE_MAX_LOAD 0.75

typedef struct TableEntry {
    char* key;
    Value value;
    struct TableEntry* next;
} TableEntry;

typedef struct Table {
    TableEntry** entries;
    int capacity;
    int count;
} Table;

// ========== VM ==========
#define VM_MAX_STACK 1024
#define VM_MAX_CALL_FRAMES 1024    // Increased for deep recursion
#define VM_MAX_GLOBALS 65536
#define VM_MAX_ARGS_STACK 256      // Argument passing stack

typedef struct {
    // Register frames array (each function call gets its own frame)
    Value register_frames[VM_MAX_FRAMES][VM_REGS_PER_FRAME];
    Value* registers;              // Pointer to the CURRENT active frame
    int current_frame;             // Current frame index
    int register_count;            // Max used register in the current frame
    
    // Global variables (shared across all frames)
    Value globals[VM_MAX_GLOBALS];
    int global_count;
    
    // Argument passing stack between calls
    Value args_stack[VM_MAX_ARGS_STACK];
    int args_top;
    
    // Call stack (saves state on CALL)
    struct {
        int return_address;        // Return address (next instruction after CALL)
        int base_register;         // (unused, kept for compatibility)
        int base_iterator_depth;   // Iterator stack depth before call
        int frame_index;           // Caller's frame index
        int dest_reg;              // Register to store the call result
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
    
    // Try/catch error handler address (-1 if not in a try block)
    int try_handler_addr;
    
    // Fast numeric loop stack (FOR_NEXT)
    struct {
        double index;
        double end;
        double step;
    } iterator_stack[VM_MAX_CALL_FRAMES];
    int iterator_depth;
    
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

// ========== VM API ==========
VM* vm_create();
void vm_destroy(VM* vm);
bool vm_execute(VM* vm, BytecodeChunk* chunk);
void vm_error(VM* vm, const char* format, ...);

// Debug utilities
void vm_dump_state(VM* vm);
void vm_dump_registers(VM* vm);

#endif // VM_H
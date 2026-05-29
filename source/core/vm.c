#include "vm.h"
#include "os_module.h"
#include "math_module.h"
#include "string_module.h"
#include "table_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

// ========== Value Functions ==========

Value vm_make_number(double value) {
    Value v;
    v.type = VAL_NUMBER;
    v.number = value;
    return v;
}

Value vm_make_string(const char* value) {
    Value v;
    v.type = VAL_STRING;
    v.string = strdup(value);
    return v;
}

Value vm_make_bool(bool value) {
    Value v;
    v.type = VAL_BOOL;
    v.boolean = value;
    return v;
}

Value vm_make_table() {
    Value v;
    v.type = VAL_TABLE;
    v.table = table_create(8);
    return v;
}

Value vm_copy_value(Value value) {
    Value copy = value;
    if (value.type == VAL_STRING) {
        copy.string = strdup(value.string);
    } else if (value.type == VAL_TABLE && value.table) {
        copy.table = table_copy(value.table);
    }
    return copy;
}

void vm_free_value(Value* value) {
    if (!value) return;
    switch (value->type) {
        case VAL_STRING:
            free(value->string);
            value->string = NULL;
            break;
        case VAL_TABLE:
            if (value->table) {
                table_destroy(value->table);
                value->table = NULL;
            }
            break;
        default:
            break;
    }
    value->type = VAL_BOOL;
    value->boolean = false;
}

const char* vm_value_type_name(Value* value) {
    switch (value->type) {
        case VAL_NUMBER: return "number";
        case VAL_STRING: return "string";
        case VAL_BOOL:   return "boolean";
        case VAL_TABLE:  return "table";
        case VAL_FUNCTION: return "function";
        default: return "unknown";
    }
}

void vm_print_value(Value* value) {
    switch (value->type) {
        case VAL_NUMBER: {
            double num = value->number;
            // Use %.0f for large integers, %.15g otherwise
            if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9) {
                printf("%.0f", num);
            } else {
                printf("%.15g", num);
            }
            break;
        }
        case VAL_STRING:
            printf("%s", value->string);
            break;
        case VAL_BOOL:
            printf("%s", value->boolean ? "true" : "false");
            break;
        case VAL_TABLE:
            printf("<table %p>", (void*)value->table);
            break;
        case VAL_FUNCTION:
            printf("<function>");
            break;
    }
}

// ========== Hash Table Implementation ==========

static unsigned int hash_key(const char* key, int capacity) {
    unsigned int hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % capacity;
}

Table* table_create(int capacity) {
    Table* table = (Table*)malloc(sizeof(Table));
    table->capacity = capacity < 8 ? 8 : capacity;
    table->count = 0;
    table->entries = (TableEntry**)calloc(table->capacity, sizeof(TableEntry*));
    return table;
}

void table_destroy(Table* table) {
    if (!table) return;
    
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            TableEntry* next = entry->next;
            free(entry->key);
            vm_free_value(&entry->value);
            free(entry);
            entry = next;
        }
    }
    free(table->entries);
    free(table);
}

static void table_resize(Table* table, int new_capacity) {
    TableEntry** old_entries = table->entries;
    int old_capacity = table->capacity;
    
    table->capacity = new_capacity;
    table->entries = (TableEntry**)calloc(new_capacity, sizeof(TableEntry*));
    table->count = 0;
    
    for (int i = 0; i < old_capacity; i++) {
        TableEntry* entry = old_entries[i];
        while (entry) {
            TableEntry* next = entry->next;
            unsigned int index = hash_key(entry->key, new_capacity);
            entry->next = table->entries[index];
            table->entries[index] = entry;
            table->count++;
            entry = next;
        }
    }
    free(old_entries);
}

bool table_set(Table* table, const char* key, Value value) {
    if ((double)(table->count + 1) / table->capacity > TABLE_MAX_LOAD) {
        table_resize(table, table->capacity * 2);
    }
    
    unsigned int index = hash_key(key, table->capacity);
    TableEntry* entry = table->entries[index];
    
    // Check if key already exists
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            vm_free_value(&entry->value);
            entry->value = value;
            return true;
        }
        entry = entry->next;
    }
    
    // New key
    entry = (TableEntry*)malloc(sizeof(TableEntry));
    entry->key = strdup(key);
    entry->value = value;
    entry->next = table->entries[index];
    table->entries[index] = entry;
    table->count++;
    return true;
}

bool table_get(Table* table, const char* key, Value* out_value) {
    if (!table || !key) return false;
    
    unsigned int index = hash_key(key, table->capacity);
    TableEntry* entry = table->entries[index];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (out_value) *out_value = entry->value;
            return true;
        }
        entry = entry->next;
    }
    return false;
}

bool table_has(Table* table, const char* key) {
    return table_get(table, key, NULL);
}

void table_remove(Table* table, const char* key) {
    if (!table || !key) return;
    
    unsigned int index = hash_key(key, table->capacity);
    TableEntry* entry = table->entries[index];
    TableEntry* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                table->entries[index] = entry->next;
            }
            free(entry->key);
            vm_free_value(&entry->value);
            free(entry);
            table->count--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

int table_size(Table* table) {
    return table ? table->count : 0;
}

void table_clear(Table* table) {
    if (!table) return;
    
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            TableEntry* next = entry->next;
            free(entry->key);
            vm_free_value(&entry->value);
            free(entry);
            entry = next;
        }
        table->entries[i] = NULL;
    }
    table->count = 0;
}

Table* table_copy(Table* table) {
    if (!table) return NULL;
    
    Table* copy = table_create(table->capacity);
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            table_set(copy, entry->key, vm_copy_value(entry->value));
            entry = entry->next;
        }
    }
    return copy;
}

// ========== VM Implementation ==========

VM* vm_create() {
    VM* vm = (VM*)calloc(1, sizeof(VM));
    vm->register_count = 0;
    vm->global_count = 0;
    vm->call_depth = 0;
    vm->try_handler_addr = -1;
    vm->iterator_depth = -1;
    vm->running = false;
    vm->had_error = false;
    vm->current_frame = 0;
    vm->registers = vm->register_frames[0];
    vm->args_top = 0;
    for (int i = 0; i < VM_REGS_PER_FRAME; i++) {
        vm->registers[i] = vm_make_bool(false);
    }
    return vm;
}

void vm_destroy(VM* vm) {
    if (!vm) return;
    
    // Free registers
    for (int i = 0; i < vm->register_count; i++) {
        vm_free_value(&vm->registers[i]);
    }
    
    // Free global variables
    for (int i = 0; i < vm->global_count; i++) {
        vm_free_value(&vm->globals[i]);
    }
    
    free(vm);
}

void vm_error(VM* vm, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    fprintf(stderr, "VM Error at PC=%d: %s\n", vm->pc, buffer);
    vm->had_error = true;
}

// ========== Built-in Functions ==========

static bool vm_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    // Try each module
    if (strncmp(name, "os.", 3) == 0) {
        return os_call_builtin(vm, name, arg_count, args, result);
    }
    
    if (strncmp(name, "math.", 5) == 0) {
        return math_call_builtin(vm, name, arg_count, args, result);
    }
    
    if (strncmp(name, "string.", 7) == 0) {
        return string_call_builtin(vm, name, arg_count, args, result);
    }
    
    if (strncmp(name, "table.", 6) == 0) {
        return table_call_builtin(vm, name, arg_count, args, result);
    }
    
    // Global conversion functions
    if (strcmp(name, "number") == 0) {
        if (arg_count >= 1) {
            if (args[0].type == VAL_STRING) {
                *result = vm_make_number(atof(args[0].string));
            } else if (args[0].type == VAL_NUMBER) {
                *result = vm_copy_value(args[0]);
            } else {
                *result = vm_make_bool(false);
            }
        }
        return true;
    }
    
    if (strcmp(name, "string") == 0) {
        if (arg_count >= 1) {
            char buffer[256];
            switch (args[0].type) {
                case VAL_NUMBER:
                    snprintf(buffer, sizeof(buffer), "%g", args[0].number);
                    *result = vm_make_string(buffer);
                    break;
                case VAL_BOOL:
                    *result = vm_make_string(args[0].boolean ? "true" : "false");
                    break;
                case VAL_STRING:
                    *result = vm_copy_value(args[0]);
                    break;
                default:
                    *result = vm_make_string("false");
                    break;
            }
        }
        return true;
    }
    
    return false;
}

// ========== Main Execution Loop ==========

bool vm_execute(VM* vm, BytecodeChunk* chunk) {
    if (!vm || !chunk) return false;
    
    vm->chunk = chunk;
    vm->code = chunk->code;
    vm->code_count = chunk->code_count;
    vm->running = true;
    vm->had_error = false;
    
    vm->global_count = chunk->global_count;
    for (int i = 0; i < chunk->global_count; i++) {
        vm->globals[i].type = VAL_BOOL;
        vm->globals[i].boolean = false;
    }
    
    // Computed goto dispatch table
    static void* dispatch_table[] = {
        [OP_NOP]              = &&OP_NOP_LABEL,
        [OP_LOAD_CONST]       = &&OP_LOAD_CONST_LABEL,
        [OP_MOVE]             = &&OP_MOVE_LABEL,
        [OP_ADD]              = &&OP_ADD_LABEL,
        [OP_SUB]              = &&OP_SUB_LABEL,
        [OP_MUL]              = &&OP_MUL_LABEL,
        [OP_DIV]              = &&OP_DIV_LABEL,
        [OP_MOD]              = &&OP_MOD_LABEL,
        [OP_NEG]              = &&OP_NEG_LABEL,
        [OP_CMP_EQ]           = &&OP_CMP_EQ_LABEL,
        [OP_CMP_NEQ]          = &&OP_CMP_NEQ_LABEL,
        [OP_CMP_LT]           = &&OP_CMP_LT_LABEL,
        [OP_CMP_GT]           = &&OP_CMP_GT_LABEL,
        [OP_CMP_LTE]          = &&OP_CMP_LTE_LABEL,
        [OP_CMP_GTE]          = &&OP_CMP_GTE_LABEL,
        [OP_AND]              = &&OP_AND_LABEL,
        [OP_OR]               = &&OP_OR_LABEL,
        [OP_NOT]              = &&OP_NOT_LABEL,
        [OP_TO_NUMBER]        = &&OP_TO_NUMBER_LABEL,
        [OP_TO_STRING]        = &&OP_TO_STRING_LABEL,
        [OP_TO_BOOL]          = &&OP_TO_BOOL_LABEL,
        [OP_JUMP]             = &&OP_JUMP_LABEL,
        [OP_JUMP_IF_TRUE]     = &&OP_JUMP_IF_TRUE_LABEL,
        [OP_JUMP_IF_FALSE]    = &&OP_JUMP_IF_FALSE_LABEL,
        [OP_CALL]             = &&OP_CALL_LABEL,
        [OP_CALL_BUILTIN]     = &&OP_CALL_BUILTIN_LABEL,
        [OP_RETURN]           = &&OP_RETURN_LABEL,
        [OP_RETURN_VOID]      = &&OP_RETURN_VOID_LABEL,
        [OP_LOAD_GLOBAL]      = &&OP_LOAD_GLOBAL_LABEL,
        [OP_STORE_GLOBAL]     = &&OP_STORE_GLOBAL_LABEL,
        [OP_LOAD_LOCAL]       = &&OP_LOAD_LOCAL_LABEL,
        [OP_STORE_LOCAL]      = &&OP_STORE_LOCAL_LABEL,
        [OP_NEW_TABLE]        = &&OP_NEW_TABLE_LABEL,
        [OP_TABLE_SET]        = &&OP_TABLE_SET_LABEL,
        [OP_TABLE_SET_CONST]  = &&OP_TABLE_SET_CONST_LABEL,
        [OP_TABLE_GET]        = &&OP_TABLE_GET_LABEL,
        [OP_TABLE_GET_CONST]  = &&OP_TABLE_GET_CONST_LABEL,
        [OP_TABLE_APPEND]     = &&OP_TABLE_APPEND_LABEL,
        [OP_CONCAT]           = &&OP_CONCAT_LABEL,
        [OP_STRING_INTERP]    = &&OP_STRING_INTERP_LABEL,
        [OP_FOR_PREP]         = &&OP_FOR_PREP_LABEL,
        [OP_POP_ITER]         = &&OP_POP_ITER_LABEL,
        [OP_FOR_INIT]         = &&OP_FOR_INIT_LABEL,
        [OP_FOR_NEXT]         = &&OP_FOR_NEXT_LABEL,
        [OP_BREAK]            = &&OP_BREAK_LABEL,
        [OP_CONTINUE]         = &&OP_CONTINUE_LABEL,
        [OP_JUMP_IF_LT]       = &&OP_JUMP_IF_LT_LABEL,
        [OP_JUMP_IF_LTE]      = &&OP_JUMP_IF_LTE_LABEL,
        [OP_JUMP_IF_GT]       = &&OP_JUMP_IF_GT_LABEL,
        [OP_JUMP_IF_GTE]      = &&OP_JUMP_IF_GTE_LABEL,
        [OP_JUMP_IF_EQ]       = &&OP_JUMP_IF_EQ_LABEL,
        [OP_JUMP_IF_NEQ]      = &&OP_JUMP_IF_NEQ_LABEL,
        [OP_PUSH_ARG]         = &&OP_PUSH_ARG_LABEL,
        [OP_TRY]              = &&OP_TRY_LABEL,
        [OP_CATCH]            = &&OP_CATCH_LABEL,
        [OP_END_TRY]          = &&OP_END_TRY_LABEL,
        [OP_PRINT]            = &&OP_PRINT_LABEL,
        [OP_HALT]             = &&OP_HALT_LABEL,
        [OP_ADD_IMM]          = &&OP_ADD_IMM_LABEL,
        [OP_LOAD_BOOL]        = &&OP_LOAD_BOOL_LABEL,
        [OP_DUP]              = &&OP_DUP_LABEL,
        [OP_SWAP]             = &&OP_SWAP_LABEL,
        [OP_INC_GLOBAL]       = &&OP_INC_GLOBAL_LABEL,
        [OP_ADD_GLOBAL]       = &&OP_ADD_GLOBAL_LABEL,
        [OP_LOAD_CONST_NUM]   = &&OP_LOAD_CONST_NUM_LABEL,
        [OP_JUMP_IF_NUM]      = &&OP_JUMP_IF_NUM_LABEL,
    };
    
    // Pointer to current instruction
    register Instruction* ip = vm->code;
    
    // First dispatch
    goto *dispatch_table[ip->opcode];
    
OP_NOP_LABEL:
    ip++;
    goto *dispatch_table[ip->opcode];
    
OP_INC_GLOBAL_LABEL: {
    int idx = ip->operands[0];
    int src = ip->operands[1];
    
    // Fast global variable increment
    vm->globals[idx].number += vm->registers[src].number;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_ADD_GLOBAL_LABEL: {
    int dest_idx = ip->operands[0];
    int left = ip->operands[1];
    int right = ip->operands[2];
    
    vm->globals[dest_idx].type = VAL_NUMBER;
    vm->globals[dest_idx].number = vm->registers[left].number + vm->registers[right].number;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_LOAD_CONST_NUM_LABEL: {
    int dest = ip->operands[0];
    int value = ip->operands[1];
    
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = (double)value;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_JUMP_IF_NUM_LABEL: {
    int target = ip->operands[0];
    int reg = ip->operands[1];
    double limit = (double)ip->operands[2];
    
    if (vm->registers[reg].type == VAL_NUMBER && vm->registers[reg].number < limit) {
        ip = &vm->code[target];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_LOAD_CONST_LABEL: {
    int dest = ip->operands[0];
    int const_idx = ip->operands[1];
    Constant* c = &chunk->constants[const_idx];
    switch (c->type) {
        case CONST_NUMBER:
            vm->registers[dest].type = VAL_NUMBER;
            vm->registers[dest].number = c->number_value;
            break;
        case CONST_STRING:
            vm->registers[dest].type = VAL_STRING;
            vm->registers[dest].string = strdup(c->string_value);
            break;
        case CONST_BOOL:
            vm->registers[dest].type = VAL_BOOL;
            vm->registers[dest].boolean = c->bool_value;
            break;
        default:
            break;
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}
    
OP_MOVE_LABEL: {
    int dest = ip->operands[0];
    int src = ip->operands[1];
    Value* sv = &vm->registers[src];
    
    if (sv->type == VAL_NUMBER) {
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = sv->number;
    } else {
        vm_free_value(&vm->registers[dest]);
        vm->registers[dest] = vm_copy_value(*sv);
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}
    
OP_ADD_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = vm->registers[ip->operands[1]].number + 
                                 vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_SUB_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = vm->registers[ip->operands[1]].number - 
                                 vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_MUL_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = vm->registers[ip->operands[1]].number * 
                                 vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_DIV_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = vm->registers[ip->operands[1]].number / 
                                 vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_MOD_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = fmod(vm->registers[ip->operands[1]].number, 
                                      vm->registers[ip->operands[2]].number);
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_NEG_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = -vm->registers[ip->operands[1]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

    
OP_CMP_EQ_LABEL: {
    int dest = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    int result = 0;
    if (left->type == right->type) {
        switch (left->type) {
            case VAL_NUMBER: result = (left->number == right->number); break;
            case VAL_STRING: result = (strcmp(left->string, right->string) == 0); break;
            case VAL_BOOL:   result = (left->boolean == right->boolean); break;
            default: break;
        }
    }
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = result;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_CMP_NEQ_LABEL: {
    int dest = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    int result = 1;
    if (left->type == right->type) {
        switch (left->type) {
            case VAL_NUMBER: result = (left->number != right->number); break;
            case VAL_STRING: result = (strcmp(left->string, right->string) != 0); break;
            case VAL_BOOL:   result = (left->boolean != right->boolean); break;
            default: break;
        }
    }
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = result;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_CMP_LT_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = vm->registers[ip->operands[1]].number < 
                                  vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_CMP_GT_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = vm->registers[ip->operands[1]].number > 
                                  vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_CMP_LTE_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = vm->registers[ip->operands[1]].number <= 
                                  vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_CMP_GTE_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = vm->registers[ip->operands[1]].number >= 
                                  vm->registers[ip->operands[2]].number;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_AND_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = vm->registers[ip->operands[1]].boolean && 
                                  vm->registers[ip->operands[2]].boolean;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_OR_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = vm->registers[ip->operands[1]].boolean || 
                                  vm->registers[ip->operands[2]].boolean;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_NOT_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = !vm->registers[ip->operands[1]].boolean;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}
    
OP_TO_NUMBER_LABEL: {
    int dest = ip->operands[0];
    Value* src = &vm->registers[ip->operands[1]];
    if (src->type == VAL_STRING) {
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = atof(src->string);
    } else if (src->type == VAL_NUMBER) {
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = src->number;
    } else {
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = false; // Fallback
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TO_STRING_LABEL: {
    int dest = ip->operands[0];
    Value* src = &vm->registers[ip->operands[1]];
    char buffer[256];
    switch (src->type) {
        case VAL_NUMBER: {
            double num = src->number;
            if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) {
                snprintf(buffer, sizeof(buffer), "%lld", (long long)num);
            } else {
                snprintf(buffer, sizeof(buffer), "%.15g", num);
            }
            vm->registers[dest] = vm_make_string(buffer);
            break;
        }
        case VAL_BOOL:
            vm->registers[dest] = vm_make_string(src->boolean ? "true" : "false");
            break;
        case VAL_STRING:
            vm->registers[dest] = vm_make_string(src->string);
            break;
        default:
            vm->registers[dest] = vm_make_string("false"); // Fallback
            break;
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TO_BOOL_LABEL: {
    int dest = ip->operands[0];
    Value* src = &vm->registers[ip->operands[1]];
    vm->registers[dest].type = VAL_BOOL;
    switch (src->type) {
        case VAL_BOOL: 
            vm->registers[dest].boolean = src->boolean; 
            break;
        case VAL_STRING:
            vm->registers[dest].boolean = (strlen(src->string) > 0);
            break;
        case VAL_NUMBER:
            vm->registers[dest].boolean = (src->number != 0.0);
            break;
        default: 
            vm->registers[dest].boolean = false; 
            break;
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}
    
OP_JUMP_LABEL:
    ip = &vm->code[ip->operands[0]];
    goto *dispatch_table[ip->opcode];

OP_JUMP_IF_TRUE_LABEL: {
    int cond_reg = ip->operands[1];
    if (vm->registers[cond_reg].boolean) {
        ip = &vm->code[ip->operands[0]];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_JUMP_IF_FALSE_LABEL: {
    int cond_reg = ip->operands[1];
    if (!vm->registers[cond_reg].boolean) {
        ip = &vm->code[ip->operands[0]];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}
    
OP_CALL_LABEL: {
    int func_addr = ip->operands[1];
    int arg_count = ip->operands[2];
    int dest_reg = ip->operands[0];
    
    if (vm->call_depth >= VM_MAX_CALL_FRAMES || vm->current_frame >= VM_MAX_FRAMES - 1) {
        vm_error(vm, "Stack overflow (recursion too deep)");
    }
    
    // Save caller state
    vm->call_stack[vm->call_depth].return_address = (ip + 1) - vm->code;
    vm->call_stack[vm->call_depth].dest_reg = dest_reg;
    vm->call_stack[vm->call_depth].frame_index = vm->current_frame;
    vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;
    vm->call_depth++;
    
    // Switch to NEW register frame
    vm->current_frame++;
    vm->registers = vm->register_frames[vm->current_frame];
    
    // Load arguments from arg stack into the first registers (R0, R1...)
    for (int i = 0; i < arg_count; i++) {
        vm->registers[i] = vm->args_stack[vm->args_top - arg_count + i];
    }
    vm->args_top -= arg_count; // Pop arguments
    
    ip = &vm->code[func_addr];
    goto *dispatch_table[ip->opcode];
}

OP_CALL_BUILTIN_LABEL: {
    int dest_reg = ip->operands[0];
    int name_idx = ip->operands[1];
    int arg_count = ip->operands[2];
    
    Value args[16];
    for (int i = 0; i < arg_count && i < 16; i++) {
        args[i] = vm->args_stack[vm->args_top - arg_count + i];
    }
    vm->args_top -= arg_count;
    
    Value result;
    if (vm_call_builtin(vm, chunk->constants[name_idx].string_value, arg_count, args, &result)) {
        vm->registers[dest_reg] = result;
    } else {
        vm->registers[dest_reg] = vm_make_bool(false);
    }
    if (dest_reg >= vm->register_count) vm->register_count = dest_reg + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_RETURN_LABEL: {
    int value_reg = ip->operands[0];
    // Get result from current (departing) frame
    Value ret_val = vm_copy_value(vm->registers[value_reg]);
    
    if (vm->call_depth > 0) {
        vm->call_depth--;
        // Restore OLD frame
        vm->current_frame = vm->call_stack[vm->call_depth].frame_index;
        vm->registers = vm->register_frames[vm->current_frame];
        vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;
        
        int dest_reg = vm->call_stack[vm->call_depth].dest_reg;
        vm_free_value(&vm->registers[dest_reg]);
        vm->registers[dest_reg] = ret_val;
        
        ip = &vm->code[vm->call_stack[vm->call_depth].return_address];
        goto *dispatch_table[ip->opcode];
    }
    vm->running = false;
    goto OP_HALT_LABEL;
}

OP_RETURN_VOID_LABEL: {
    if (vm->call_depth > 0) {
        vm->call_depth--;
        int return_addr = vm->call_stack[vm->call_depth].return_address;
        int dest_reg = vm->code[return_addr - 1].operands[0];
        vm_free_value(&vm->registers[dest_reg]);
        
        // Functions without explicit return now return false
        vm->registers[dest_reg].type = VAL_BOOL;
        vm->registers[dest_reg].boolean = false;
        
        ip = &vm->code[return_addr];
        goto *dispatch_table[ip->opcode];
    }
    vm->running = false;
    goto OP_HALT_LABEL;
}
    
OP_LOAD_GLOBAL_LABEL: {
    int dest = ip->operands[0];
    int idx = ip->operands[1];
    
    if (vm->globals[idx].type == VAL_NUMBER) {
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = vm->globals[idx].number;
    } else {
        vm_free_value(&vm->registers[dest]);
        vm->registers[dest] = vm_copy_value(vm->globals[idx]);
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_STORE_GLOBAL_LABEL: {
    int src = ip->operands[0];
    int idx = ip->operands[1];
    
    Value* sv = &vm->registers[src];
    if (sv->type == VAL_NUMBER) {
        vm_free_value(&vm->globals[idx]);
        vm->globals[idx].type = VAL_NUMBER;
        vm->globals[idx].number = sv->number;
    } else {
        vm_free_value(&vm->globals[idx]);
        vm->globals[idx] = vm_copy_value(*sv);
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_LOAD_LOCAL_LABEL:
OP_STORE_LOCAL_LABEL:
    ip++;
    goto *dispatch_table[ip->opcode];
    
OP_NEW_TABLE_LABEL: {
    int dest = ip->operands[0];
    vm_free_value(&vm->registers[dest]);
    vm->registers[dest] = vm_make_table();
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TABLE_SET_LABEL: {
    int table_reg = ip->operands[0];
    int key_reg = ip->operands[1];
    int val_reg = ip->operands[2];
    Table* table = vm->registers[table_reg].table;
    char key_str[256];
    Value* key = &vm->registers[key_reg];
    if (key->type == VAL_STRING) strcpy(key_str, key->string);
    else if (key->type == VAL_NUMBER) snprintf(key_str, sizeof(key_str), "%g", key->number);
    else key_str[0] = '\0';
    table_set(table, key_str, vm_copy_value(vm->registers[val_reg]));
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TABLE_SET_CONST_LABEL: {
    int table_reg = ip->operands[0];
    int key_idx = ip->operands[1];
    int val_reg = ip->operands[2];
    
    if (vm->registers[table_reg].type == VAL_TABLE && 
        key_idx < chunk->const_count && 
        chunk->constants[key_idx].type == CONST_STRING) {
        table_set(vm->registers[table_reg].table, 
                 chunk->constants[key_idx].string_value,
                 vm_copy_value(vm->registers[val_reg]));
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TABLE_GET_LABEL: {
    int dest = ip->operands[0];
    int table_reg = ip->operands[1];
    int key_reg = ip->operands[2];
    
    if (vm->registers[table_reg].type == VAL_TABLE) {
        char key_str[256];
        Value* key = &vm->registers[key_reg];
        if (key->type == VAL_STRING) strcpy(key_str, key->string);
        else if (key->type == VAL_NUMBER) snprintf(key_str, sizeof(key_str), "%g", key->number);
        else key_str[0] = '\0';
        
        Value val;
        if (table_get(vm->registers[table_reg].table, key_str, &val)) {
            vm_free_value(&vm->registers[dest]);
            vm->registers[dest] = vm_copy_value(val);
        }
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TABLE_GET_CONST_LABEL: {
    int dest = ip->operands[0];
    int table_reg = ip->operands[1];
    int key_idx = ip->operands[2];
    
    if (vm->registers[table_reg].type == VAL_TABLE &&
        key_idx < chunk->const_count &&
        chunk->constants[key_idx].type == CONST_STRING) {
        Value val;
        if (table_get(vm->registers[table_reg].table, chunk->constants[key_idx].string_value, &val)) {
            vm_free_value(&vm->registers[dest]);
            vm->registers[dest] = vm_copy_value(val);
        }
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TABLE_APPEND_LABEL: {
    int table_reg = ip->operands[0];
    int val_reg = ip->operands[1];
    
    if (vm->registers[table_reg].type == VAL_TABLE) {
        char key[32];
        snprintf(key, sizeof(key), "%d", table_size(vm->registers[table_reg].table) + 1);
        table_set(vm->registers[table_reg].table, key, vm_copy_value(vm->registers[val_reg]));
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_CONCAT_LABEL: {
    int dest = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    
    char buffer[2048];
    char left_str[1024] = "";
    char right_str[1024] = "";
    
    if (left->type == VAL_NUMBER) snprintf(left_str, sizeof(left_str), "%.15g", left->number);
    else if (left->type == VAL_STRING) strcpy(left_str, left->string);
    else if (left->type == VAL_BOOL) strcpy(left_str, left->boolean ? "true" : "false");
    
    if (right->type == VAL_NUMBER) snprintf(right_str, sizeof(right_str), "%.15g", right->number);
    else if (right->type == VAL_STRING) strcpy(right_str, right->string);
    else if (right->type == VAL_BOOL) strcpy(right_str, right->boolean ? "true" : "false");
    
    snprintf(buffer, sizeof(buffer), "%s%s", left_str, right_str);
    vm_free_value(&vm->registers[dest]);
    vm->registers[dest] = vm_make_string(buffer);
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_STRING_INTERP_LABEL:
    ip++;
    goto *dispatch_table[ip->opcode];

OP_FOR_PREP_LABEL: {
    int iter_reg = ip->operands[0];
    
    if (vm->registers[iter_reg].type == VAL_TABLE) {
        Table* table = vm->registers[iter_reg].table;
        Value start_val;
        if (table_get(table, "__start", &start_val) && start_val.type == VAL_NUMBER) {
            table_set(table, "__current", vm_make_number(start_val.number));
        }
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_FOR_INIT_LABEL: {
    int var_reg = ip->operands[0];
    int end_reg = ip->operands[1];
    int step_reg = ip->operands[2];
    
    if (vm->iterator_depth < VM_MAX_CALL_FRAMES - 1) {
        vm->iterator_depth++;
        vm->iterator_stack[vm->iterator_depth].index = vm->registers[var_reg].number;
        vm->iterator_stack[vm->iterator_depth].end   = vm->registers[end_reg].number;
        vm->iterator_stack[vm->iterator_depth].step  = vm->registers[step_reg].number;
    } else {
        vm_error(vm, "Iterator stack overflow");
    }
    
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_FOR_NEXT_LABEL: {
    int var_reg = ip->operands[0];
    int op1 = ip->operands[1];
    int op2 = ip->operands[2];
    int exit_addr;

    if (op2 != 0) {
        // === SLOW LOOP (table-based, op2 = exit_addr) ===
        exit_addr = op2;
        int iter_reg = op1;
        if (vm->registers[iter_reg].type == VAL_TABLE) {
            Table* table = vm->registers[iter_reg].table;
            Value curr, end, step;
            
            if (!table_get(table, "__current", &curr) || curr.type != VAL_NUMBER) {
                ip = &vm->code[exit_addr];
                goto *dispatch_table[ip->opcode];
            }
            if (!table_get(table, "__end", &end) || end.type != VAL_NUMBER) {
                ip = &vm->code[exit_addr];
                goto *dispatch_table[ip->opcode];
            }
            if (!table_get(table, "__step", &step) || step.type != VAL_NUMBER) {
                step = vm_make_number(1.0);
            }
            
            double c = curr.number;
            double e = end.number;
            double s = step.number;
            
            // Support both positive and negative steps
            if ((s > 0 && c < e) || (s < 0 && c > e)) {
                vm->registers[var_reg].type = VAL_NUMBER;
                vm->registers[var_reg].number = c;
                table_set(table, "__current", vm_make_number(c + s));
                if (var_reg >= vm->register_count) vm->register_count = var_reg + 1;
                ip++;
                goto *dispatch_table[ip->opcode];
            } else {
                // Loop finished — remove __current from table
                table_remove(table, "__current");
                ip = &vm->code[exit_addr];
                goto *dispatch_table[ip->opcode];
            }
        }
        // Not a table — exit loop
        ip = &vm->code[exit_addr];
        goto *dispatch_table[ip->opcode];
        
    } else {
        // === FAST LOOP (via iterator_stack, op1 = exit_addr) ===
        exit_addr = op1;
        
        if (vm->iterator_depth >= 0) {
            double c = vm->iterator_stack[vm->iterator_depth].index;
            double e = vm->iterator_stack[vm->iterator_depth].end;
            double s = vm->iterator_stack[vm->iterator_depth].step;
            
            // Support both positive and negative steps
            if ((s > 0 && c < e) || (s < 0 && c > e)) {
                // Update loop variable register
                vm->registers[var_reg].type = VAL_NUMBER;
                vm->registers[var_reg].number = c;
                if (var_reg >= vm->register_count) vm->register_count = var_reg + 1;
                
                // Advance iterator for next iteration
                vm->iterator_stack[vm->iterator_depth].index = c + s;
                
                ip++;
                goto *dispatch_table[ip->opcode];
            } else {
                // Loop finished — pop iterator from stack
                vm->iterator_depth--;
                ip = &vm->code[exit_addr];
                goto *dispatch_table[ip->opcode];
            }
        }
        
        // Iterator stack empty (should not happen in normal code)
        ip = &vm->code[exit_addr];
        goto *dispatch_table[ip->opcode];
    }
}

OP_POP_ITER_LABEL:
    if (vm->iterator_depth >= 0) {
        vm->iterator_depth--;
    }
    ip++;
    goto *dispatch_table[ip->opcode];

OP_BREAK_LABEL:
    ip = &vm->code[ip->operands[0]];
    goto *dispatch_table[ip->opcode];

OP_CONTINUE_LABEL:
    ip = &vm->code[ip->operands[0]];
    goto *dispatch_table[ip->opcode];

OP_JUMP_IF_LT_LABEL: {
    int target = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    if (left->type == VAL_NUMBER && right->type == VAL_NUMBER && left->number < right->number) {
        ip = &vm->code[target];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_JUMP_IF_LTE_LABEL: {
    int target = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    if (left->type == VAL_NUMBER && right->type == VAL_NUMBER && left->number <= right->number) {
        ip = &vm->code[target];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_JUMP_IF_GT_LABEL: {
    int target = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    if (left->type == VAL_NUMBER && right->type == VAL_NUMBER && left->number > right->number) {
        ip = &vm->code[target];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_JUMP_IF_GTE_LABEL: {
    int target = ip->operands[0];
    if (vm->registers[ip->operands[1]].number >= vm->registers[ip->operands[2]].number) {
        ip = &vm->code[target];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_JUMP_IF_EQ_LABEL: {
    int target = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    bool jump = false;
    
    if (left->type == right->type) {
        switch (left->type) {
            case VAL_NUMBER: jump = (left->number == right->number); break;
            case VAL_STRING: jump = (strcmp(left->string, right->string) == 0); break;
            case VAL_BOOL:   jump = (left->boolean == right->boolean); break;
            default: break;
        }
    }
    
    if (jump) {
        ip = &vm->code[target];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_JUMP_IF_NEQ_LABEL: {
    int target = ip->operands[0];
    Value* left = &vm->registers[ip->operands[1]];
    Value* right = &vm->registers[ip->operands[2]];
    bool jump = true;
    
    if (left->type == right->type) {
        switch (left->type) {
            case VAL_NUMBER: jump = (left->number != right->number); break;
            case VAL_STRING: jump = (strcmp(left->string, right->string) != 0); break;
            case VAL_BOOL:   jump = (left->boolean != right->boolean); break;
            default: break;
        }
    }
    
    if (jump) {
        ip = &vm->code[target];
        goto *dispatch_table[ip->opcode];
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_TRY_LABEL: {
    vm->try_handler_addr = ip->operands[0];
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_PUSH_ARG_LABEL: {
    int reg = ip->operands[0];
    if (vm->args_top >= VM_MAX_ARGS_STACK) {
        vm_error(vm, "Arguments stack overflow");
    } else {
        Value* src = &vm->registers[reg];
        // Fast path: for numbers and bools just copy bytes, no malloc needed
        if (src->type == VAL_NUMBER || src->type == VAL_BOOL) {
            vm->args_stack[vm->args_top] = *src; 
        } else {
            // Strings and tables need a deep copy (protection against use-after-free)
            vm->args_stack[vm->args_top] = vm_copy_value(*src);
        }
        vm->args_top++;
    }
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_CATCH_LABEL: {
    int err_reg = ip->operands[0];
    vm_free_value(&vm->registers[err_reg]);
    vm->registers[err_reg] = vm_make_string("runtime_error");
    vm->try_handler_addr = -1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_END_TRY_LABEL:
    vm->try_handler_addr = -1;
    ip++;
    goto *dispatch_table[ip->opcode];

OP_PRINT_LABEL:
    vm_print_value(&vm->registers[ip->operands[0]]);
    printf("\n");
    fflush(stdout);
    ip++;
    goto *dispatch_table[ip->opcode];

OP_HALT_LABEL:
    vm->running = false;
    return !vm->had_error;

OP_ADD_IMM_LABEL: {
    int dest = ip->operands[0];
    int src = ip->operands[1];
    double imm = chunk->constants[ip->operands[2]].number_value;
    
    vm->registers[dest].type = VAL_NUMBER;
    vm->registers[dest].number = vm->registers[src].number + imm;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_LOAD_BOOL_LABEL: {
    int dest = ip->operands[0];
    vm->registers[dest].type = VAL_BOOL;
    vm->registers[dest].boolean = ip->operands[1] != 0;
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_DUP_LABEL: {
    int dest = ip->operands[0];
    int src = ip->operands[1];
    Value* sv = &vm->registers[src];
    
    if (sv->type == VAL_NUMBER) {
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = sv->number;
    } else {
        vm_free_value(&vm->registers[dest]);
        vm->registers[dest] = vm_copy_value(*sv);
    }
    if (dest >= vm->register_count) vm->register_count = dest + 1;
    ip++;
    goto *dispatch_table[ip->opcode];
}

OP_SWAP_LABEL: {
    Value tmp = vm->registers[ip->operands[0]];
    vm->registers[ip->operands[0]] = vm->registers[ip->operands[1]];
    vm->registers[ip->operands[1]] = tmp;
    ip++;
    goto *dispatch_table[ip->opcode];
}
    
    return !vm->had_error;
}

// ========== Debug Functions ==========

void vm_dump_registers(VM* vm) {
    printf("\n=== Registers (used: %d) ===\n", vm->register_count);
    for (int i = 0; i < vm->register_count; i++) {
        printf("R%-3d: ", i);
        vm_print_value(&vm->registers[i]);
        printf("\n");
    }
}

void vm_dump_state(VM* vm) {
    printf("\n=== VM State ===\n");
    printf("PC: %d/%d\n", vm->pc, vm->code_count);
    printf("Call depth: %d\n", vm->call_depth);
    printf("Try handler: %d\n", vm->try_handler_addr);
    
    vm_dump_registers(vm);
    
    printf("\n=== Globals (%d) ===\n", vm->global_count);
    for (int i = 0; i < vm->global_count; i++) {
        printf("G%-3d: ", i);
        vm_print_value(&vm->globals[i]);
        printf("\n");
    }
}
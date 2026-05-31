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

// ========== Forward Declarations ==========
static StringObject* string_create(const char* chars, int length);
static void string_destroy(StringObject* str);
static inline void value_incref(Value* v);
static inline void value_decref(Value* v);
static bool string_equal(StringObject* a, StringObject* b);
static const char* value_to_cstr(Value* v, char* buf, int buf_size);

// ========== String Object Implementation ==========
static StringObject* string_create(const char* chars, int length) {
    StringObject* str = (StringObject*)malloc(sizeof(StringObject) + length + 1);
    str->header.ref_count = 1;
    str->header.type = VAL_STRING;
    str->length = length;
    str->hash_computed = false;
    str->hash = 0;
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    return str;
}

static uint32_t string_get_hash(StringObject* str) {
    if (!str->hash_computed) {
        uint32_t h = 5381;
        for (int i = 0; i < str->length; i++) {
            h = ((h << 5) + h) + (uint8_t)str->chars[i];
        }
        str->hash = h;
        str->hash_computed = true;
    }
    return str->hash;
}

static void string_destroy(StringObject* str) {
    if (str) free(str);
}

static bool string_equal(StringObject* a, StringObject* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->length != b->length) return false;
    if (string_get_hash(a) != string_get_hash(b)) return false;
    return memcmp(a->chars, b->chars, a->length) == 0;
}

static const char* value_to_cstr(Value* v, char* buf, int buf_size) {
    switch (v->type) {
        case VAL_STRING: return v->string->chars;
        case VAL_NUMBER:
            snprintf(buf, buf_size, "%.15g", v->number);
            return buf;
        case VAL_BOOL:
            return v->boolean ? "true" : "false";
        default:
            return "";
    }
}

// ========== Reference Counting ==========
static inline void value_incref(Value* v) {
    if (v->type == VAL_STRING && v->string) {
        v->string->header.ref_count++;
    } else if (v->type == VAL_TABLE && v->table) {
        v->table->header.ref_count++;
    }
}

static inline void value_decref(Value* v) {
    if (!v) return;
    switch (v->type) {
        case VAL_STRING:
            if (v->string && --v->string->header.ref_count == 0) {
                string_destroy(v->string);
            }
            v->string = NULL;
            break;
        case VAL_TABLE:
            if (v->table && --v->table->header.ref_count == 0) {
                table_destroy(v->table);
            }
            v->table = NULL;
            break;
        default:
            break;
    }
    v->type = VAL_BOOL;
    v->boolean = false;
}

// ========== Value Functions ==========
Value vm_make_number(double value) {
    Value v; v.type = VAL_NUMBER; v.number = value; return v;
}

Value vm_make_string(const char* value) {
    Value v;
    v.type = VAL_STRING;
    v.string = string_create(value, (int)strlen(value));
    return v;
}

Value vm_make_bool(bool value) {
    Value v; v.type = VAL_BOOL; v.boolean = value; return v;
}

Value vm_make_table() {
    Value v;
    v.type = VAL_TABLE;
    v.table = table_create(8);
    return v;
}

Value vm_copy_value(Value value) {
    value_incref(&value);
    return value;
}

void vm_free_value(Value* value) {
    value_decref(value);
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
            if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9) printf("%.0f", num);
            else printf("%.15g", num);
            break;
        }
        case VAL_STRING: printf("%s", value->string->chars); break;
        case VAL_BOOL: printf("%s", value->boolean ? "true" : "false"); break;
        case VAL_TABLE: printf("<table %p>", (void*)value->table); break;
        case VAL_FUNCTION: printf("<function>"); break;
    }
}

// ========== Hash Table Implementation ==========
static unsigned int hash_key(const char* key, int capacity) {
    unsigned int hash = 5381;
    int c;
    while ((c = *key++)) hash = ((hash << 5) + hash) + c;
    return hash % capacity;
}

Table* table_create(int capacity) {
    Table* table = (Table*)malloc(sizeof(Table));
    table->header.ref_count = 1;
    table->header.type = VAL_TABLE;
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
            value_decref(&entry->value);
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
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            value_decref(&entry->value);
            entry->value = value;
            value_incref(&entry->value);
            return true;
        }
        entry = entry->next;
    }
    entry = (TableEntry*)malloc(sizeof(TableEntry));
    entry->key = strdup(key);
    entry->value = value;
    value_incref(&entry->value);
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
            if (out_value) {
                *out_value = entry->value;
                value_incref(out_value);
            }
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
            if (prev) prev->next = entry->next;
            else table->entries[index] = entry->next;
            free(entry->key);
            value_decref(&entry->value);
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
            value_decref(&entry->value);
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
            table_set(copy, entry->key, entry->value);
            entry = entry->next;
        }
    }
    return copy;
}

// ========== String Builder ==========
typedef struct {
    char* buffer;
    int length;
    int capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb, int initial_capacity) {
    sb->capacity = initial_capacity > 16 ? initial_capacity : 16;
    sb->buffer = (char*)malloc(sb->capacity);
    sb->length = 0;
    sb->buffer[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str, int len) {
    if (sb->length + len + 1 > sb->capacity) {
        sb->capacity = (sb->length + len + 1) * 2;
        sb->buffer = (char*)realloc(sb->buffer, sb->capacity);
    }
    memcpy(sb->buffer + sb->length, str, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0';
}

static StringObject* sb_to_string(StringBuilder* sb) {
    return string_create(sb->buffer, sb->length);
}

static void sb_free(StringBuilder* sb) {
    free(sb->buffer);
}

// ========== VM Implementation ==========
VM* vm_create() {
    VM* vm = (VM*)calloc(1, sizeof(VM));
    vm->register_count = 0;
    vm->global_count = 0;
    vm->call_depth = 0;
    vm->iterator_depth = -1;
    vm->running = false;
    vm->had_error = false;
    vm->current_frame = 0;
    vm->registers = vm->register_frames[0];
    vm->args_top = 0;
    for (int i = 0; i < VM_REGS_PER_FRAME; i++) {
        vm->registers[i].type = VAL_BOOL;
        vm->registers[i].boolean = false;
    }
    return vm;
}

void vm_destroy(VM* vm) {
    if (!vm) return;
    
    for (int f = 0; f < VM_MAX_FRAMES; f++) {
        for (int i = 0; i < VM_REGS_PER_FRAME; i++) {
            value_decref(&vm->register_frames[f][i]);
        }
    }
    
    for (int i = 0; i < vm->global_count; i++) {
        value_decref(&vm->globals[i]);
    }
    
    for (int i = 0; i < vm->args_top; i++) {
        value_decref(&vm->args_stack[i]);
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
    if (strncmp(name, "os.", 3) == 0) return os_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "math.", 5) == 0) return math_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "string.", 7) == 0) return string_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "table.", 6) == 0) return table_call_builtin(vm, name, arg_count, args, result);

    if (strcmp(name, "number") == 0) {
        if (arg_count >= 1) {
            if (args[0].type == VAL_STRING) *result = vm_make_number(atof(args[0].string->chars));
            else if (args[0].type == VAL_NUMBER) *result = vm_copy_value(args[0]);
            else *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "string") == 0) {
        if (arg_count >= 1) {
            char buffer[256];
            switch (args[0].type) {
                case VAL_NUMBER: snprintf(buffer, sizeof(buffer), "%g", args[0].number); *result = vm_make_string(buffer); break;
                case VAL_BOOL: *result = vm_make_string(args[0].boolean ? "true" : "false"); break;
                case VAL_STRING: *result = vm_copy_value(args[0]); break;
                default: *result = vm_make_string("false"); break;
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

    register Instruction* ip = vm->code;
    goto *dispatch_table[ip->opcode];

    OP_NOP_LABEL: ip++; goto *dispatch_table[ip->opcode];
    OP_INC_GLOBAL_LABEL: {
        int idx = ip->operands[0]; int src = ip->operands[1];
        vm->globals[idx].number += vm->registers[src].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_ADD_GLOBAL_LABEL: {
        int dest_idx = ip->operands[0]; int left = ip->operands[1]; int right = ip->operands[2];
        vm->globals[dest_idx].type = VAL_NUMBER;
        vm->globals[dest_idx].number = vm->registers[left].number + vm->registers[right].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_LOAD_CONST_NUM_LABEL: {
        int dest = ip->operands[0]; int value = ip->operands[1];
        vm->registers[dest].type = VAL_NUMBER; vm->registers[dest].number = (double)value;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_NUM_LABEL: {
        int target = ip->operands[0]; int reg = ip->operands[1]; double limit = (double)ip->operands[2];
        if (vm->registers[reg].type == VAL_NUMBER && vm->registers[reg].number < limit) {
            ip = &vm->code[target]; goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_LOAD_CONST_LABEL: {
        int dest = ip->operands[0]; int const_idx = ip->operands[1];
        Constant* c = &chunk->constants[const_idx];
        value_decref(&vm->registers[dest]);
        switch (c->type) {
            case CONST_NUMBER: vm->registers[dest].type = VAL_NUMBER; vm->registers[dest].number = c->number_value; break;
            case CONST_STRING: vm->registers[dest].type = VAL_STRING; vm->registers[dest].string = string_create(c->string_value, strlen(c->string_value)); break;
            case CONST_BOOL: vm->registers[dest].type = VAL_BOOL; vm->registers[dest].boolean = c->bool_value; break;
            default: break;
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MOVE_LABEL: {
        int dest = ip->operands[0]; int src = ip->operands[1];
        Value* sv = &vm->registers[src];
        if (sv->type == VAL_NUMBER || sv->type == VAL_BOOL) {
            vm->registers[dest] = *sv;
        } else {
            value_decref(&vm->registers[dest]);
            vm->registers[dest] = *sv;
            value_incref(&vm->registers[dest]);
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_ADD_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = vm->registers[ip->operands[1]].number + vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_SUB_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = vm->registers[ip->operands[1]].number - vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MUL_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = vm->registers[ip->operands[1]].number * vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_DIV_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = vm->registers[ip->operands[1]].number / vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MOD_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = fmod(vm->registers[ip->operands[1]].number, vm->registers[ip->operands[2]].number);
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NEG_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_NUMBER;
        vm->registers[dest].number = -vm->registers[ip->operands[1]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_EQ_LABEL: {
        int dest = ip->operands[0];
        Value* left = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]];
        int result = 0;
        if (left->type == right->type) {
            switch (left->type) {
                case VAL_NUMBER: result = (left->number == right->number); break;
                case VAL_STRING: result = string_equal(left->string, right->string); break;
                case VAL_BOOL:   result = (left->boolean == right->boolean); break;
                default: break;
            }
        }
        vm->registers[dest].type = VAL_BOOL; vm->registers[dest].boolean = result;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_NEQ_LABEL: {
        int dest = ip->operands[0];
        Value* left = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]];
        int result = 1;
        if (left->type == right->type) {
            switch (left->type) {
                case VAL_NUMBER: result = (left->number != right->number); break;
                case VAL_STRING: result = !string_equal(left->string, right->string); break;
                case VAL_BOOL:   result = (left->boolean != right->boolean); break;
                default: break;
            }
        }
        vm->registers[dest].type = VAL_BOOL; vm->registers[dest].boolean = result;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_LT_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = vm->registers[ip->operands[1]].number < vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_GT_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = vm->registers[ip->operands[1]].number > vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_LTE_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = vm->registers[ip->operands[1]].number <= vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_GTE_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = vm->registers[ip->operands[1]].number >= vm->registers[ip->operands[2]].number;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_AND_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = vm->registers[ip->operands[1]].boolean && vm->registers[ip->operands[2]].boolean;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_OR_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = vm->registers[ip->operands[1]].boolean || vm->registers[ip->operands[2]].boolean;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NOT_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL;
        vm->registers[dest].boolean = !vm->registers[ip->operands[1]].boolean;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TO_NUMBER_LABEL: {
        int dest = ip->operands[0]; Value* src = &vm->registers[ip->operands[1]];
        if (src->type == VAL_STRING) {
            vm->registers[dest].type = VAL_NUMBER; vm->registers[dest].number = atof(src->string->chars);
        } else if (src->type == VAL_NUMBER) {
            vm->registers[dest].type = VAL_NUMBER; vm->registers[dest].number = src->number;
        } else {
            vm->registers[dest].type = VAL_BOOL; vm->registers[dest].boolean = false;
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TO_STRING_LABEL: {
        int dest = ip->operands[0]; Value* src = &vm->registers[ip->operands[1]];
        char buffer[256];
        value_decref(&vm->registers[dest]);
        switch (src->type) {
            case VAL_NUMBER: {
                double num = src->number;
                if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) snprintf(buffer, sizeof(buffer), "%lld", (long long)num);
                else snprintf(buffer, sizeof(buffer), "%.15g", num);
                vm->registers[dest] = vm_make_string(buffer); break;
            }
            case VAL_BOOL: vm->registers[dest] = vm_make_string(src->boolean ? "true" : "false"); break;
            case VAL_STRING: vm->registers[dest] = *src; value_incref(&vm->registers[dest]); break;
            default: vm->registers[dest] = vm_make_string("false"); break;
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TO_BOOL_LABEL: {
        int dest = ip->operands[0]; Value* src = &vm->registers[ip->operands[1]];
        vm->registers[dest].type = VAL_BOOL;
        switch (src->type) {
            case VAL_BOOL: vm->registers[dest].boolean = src->boolean; break;
            case VAL_STRING: vm->registers[dest].boolean = (src->string->length > 0); break;
            case VAL_NUMBER: vm->registers[dest].boolean = (src->number != 0.0); break;
            default: vm->registers[dest].boolean = false; break;
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_LABEL: ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode];
    OP_JUMP_IF_TRUE_LABEL: {
        int cond_reg = ip->operands[1];
        if (vm->registers[cond_reg].boolean) { ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_FALSE_LABEL: {
        int cond_reg = ip->operands[1];
        if (!vm->registers[cond_reg].boolean) { ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CALL_LABEL: {
        int func_addr = ip->operands[1]; int arg_count = ip->operands[2]; int dest_reg = ip->operands[0];
        if (vm->call_depth >= VM_MAX_CALL_FRAMES || vm->current_frame >= VM_MAX_FRAMES - 1) vm_error(vm, "Stack overflow");
        vm->call_stack[vm->call_depth].return_address = (ip + 1) - vm->code;
        vm->call_stack[vm->call_depth].dest_reg = dest_reg;
        vm->call_stack[vm->call_depth].frame_index = vm->current_frame;
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;
        vm->call_depth++;
        vm->current_frame++;
        vm->registers = vm->register_frames[vm->current_frame];
        for (int i = 0; i < arg_count; i++) {
            vm->registers[i] = vm->args_stack[vm->args_top - arg_count + i];
        }
        vm->args_top -= arg_count;
        ip = &vm->code[func_addr]; goto *dispatch_table[ip->opcode];
    }
    OP_CALL_BUILTIN_LABEL: {
        int dest_reg = ip->operands[0]; int name_idx = ip->operands[1]; int arg_count = ip->operands[2];
        Value args[16];
        
        for (int i = 0; i < arg_count && i < 16; i++) {
            args[i] = vm->args_stack[vm->args_top - arg_count + i];
        }
        
        Value result;
        bool ok = vm_call_builtin(vm, chunk->constants[name_idx].string_value, arg_count, args, &result);
        
        for (int i = 0; i < arg_count; i++) {
            value_decref(&vm->args_stack[vm->args_top - arg_count + i]);
        }
        vm->args_top -= arg_count;

        if (ok) {
            value_decref(&vm->registers[dest_reg]);
            vm->registers[dest_reg] = result;
        } else {
            value_decref(&vm->registers[dest_reg]);
            vm->registers[dest_reg] = vm_make_bool(false);
        }
        if (dest_reg >= vm->register_count) vm->register_count = dest_reg + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_RETURN_LABEL: {
        int value_reg = ip->operands[0];
        Value ret_val = vm->registers[value_reg];
        value_incref(&ret_val);
        if (vm->call_depth > 0) {
            vm->call_depth--;
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;
            vm->registers = vm->register_frames[vm->current_frame];
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;
            value_decref(&vm->registers[dest_reg]);
            vm->registers[dest_reg] = ret_val;
            ip = &vm->code[vm->call_stack[vm->call_depth].return_address];
            goto *dispatch_table[ip->opcode];
        }
        vm->running = false;
        value_decref(&ret_val);
        goto OP_HALT_LABEL;
    }
    OP_RETURN_VOID_LABEL: {
        if (vm->call_depth > 0) {
            vm->call_depth--;
            int return_addr = vm->call_stack[vm->call_depth].return_address;
            int dest_reg = vm->code[return_addr - 1].operands[0];
            value_decref(&vm->registers[dest_reg]);
            vm->registers[dest_reg].type = VAL_BOOL; vm->registers[dest_reg].boolean = false;
            ip = &vm->code[return_addr]; goto *dispatch_table[ip->opcode];
        }
        vm->running = false; goto OP_HALT_LABEL;
    }
    OP_LOAD_GLOBAL_LABEL: {
        int dest = ip->operands[0]; int idx = ip->operands[1];
        Value* gv = &vm->globals[idx];
        if (gv->type == VAL_NUMBER || gv->type == VAL_BOOL) {
            vm->registers[dest] = *gv;
        } else {
            value_decref(&vm->registers[dest]);
            vm->registers[dest] = *gv;
            value_incref(&vm->registers[dest]);
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_STORE_GLOBAL_LABEL: {
        int src = ip->operands[0]; int idx = ip->operands[1];
        Value* sv = &vm->registers[src];
        value_decref(&vm->globals[idx]);
        vm->globals[idx] = *sv;
        value_incref(&vm->globals[idx]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_LOAD_LOCAL_LABEL:
    OP_STORE_LOCAL_LABEL: ip++; goto *dispatch_table[ip->opcode];
    OP_NEW_TABLE_LABEL: {
        int dest = ip->operands[0];
        value_decref(&vm->registers[dest]);
        vm->registers[dest] = vm_make_table();
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_SET_LABEL: {
        int table_reg = ip->operands[0]; int key_reg = ip->operands[1]; int val_reg = ip->operands[2];
        Table* table = vm->registers[table_reg].table;
        
        const char* key_cstr = "";
        char num_buf[64];
        Value* key = &vm->registers[key_reg];
        
        if (key->type == VAL_STRING) {
            key_cstr = key->string->chars;
        } else if (key->type == VAL_NUMBER) {
            snprintf(num_buf, sizeof(num_buf), "%g", key->number);
            key_cstr = num_buf;
        }
        table_set(table, key_cstr, vm->registers[val_reg]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_SET_CONST_LABEL: {
        int table_reg = ip->operands[0]; int key_idx = ip->operands[1]; int val_reg = ip->operands[2];
        if (vm->registers[table_reg].type == VAL_TABLE && key_idx < chunk->const_count && chunk->constants[key_idx].type == CONST_STRING) {
            table_set(vm->registers[table_reg].table, chunk->constants[key_idx].string_value, vm->registers[val_reg]);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_LABEL: {
        int dest = ip->operands[0]; int table_reg = ip->operands[1]; int key_reg = ip->operands[2];
        if (vm->registers[table_reg].type == VAL_TABLE) {
            char key_str[256];
            Value* key = &vm->registers[key_reg];
            if (key->type == VAL_STRING) strcpy(key_str, key->string->chars);
            else if (key->type == VAL_NUMBER) snprintf(key_str, sizeof(key_str), "%g", key->number);
            else key_str[0] = '\0';
            Value val; val.type = VAL_BOOL; val.boolean = false;
            if (table_get(vm->registers[table_reg].table, key_str, &val)) {
                value_decref(&vm->registers[dest]);
                vm->registers[dest] = val;
            }
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_CONST_LABEL: {
        int dest = ip->operands[0]; int table_reg = ip->operands[1]; int key_idx = ip->operands[2];
        if (vm->registers[table_reg].type == VAL_TABLE && key_idx < chunk->const_count && chunk->constants[key_idx].type == CONST_STRING) {
            Value val; val.type = VAL_BOOL; val.boolean = false;
            if (table_get(vm->registers[table_reg].table, chunk->constants[key_idx].string_value, &val)) {
                value_decref(&vm->registers[dest]);
                vm->registers[dest] = val;
            }
        }
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_APPEND_LABEL: {
        int table_reg = ip->operands[0]; int val_reg = ip->operands[1];
        if (vm->registers[table_reg].type == VAL_TABLE) {
            char key[32];
            snprintf(key, sizeof(key), "%d", table_size(vm->registers[table_reg].table) + 1);
            table_set(vm->registers[table_reg].table, key, vm->registers[val_reg]);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CONCAT_LABEL: {
        int dest = ip->operands[0];
        Value* left  = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]];
        char lbuf[64], rbuf[64];
        const char* ls = value_to_cstr(left, lbuf, sizeof(lbuf));
        const char* rs = value_to_cstr(right, rbuf, sizeof(rbuf));
        int llen = (left->type == VAL_STRING) ? left->string->length : (int)strlen(ls);
        int rlen = (right->type == VAL_STRING) ? right->string->length : (int)strlen(rs);
        StringBuilder sb;
        sb_init(&sb, llen + rlen + 1);
        sb_append(&sb, ls, llen);
        sb_append(&sb, rs, rlen);
        value_decref(&vm->registers[dest]);
        vm->registers[dest].type = VAL_STRING;
        vm->registers[dest].string = sb_to_string(&sb);
        sb_free(&sb);
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_STRING_INTERP_LABEL: ip++; goto *dispatch_table[ip->opcode];
    OP_FOR_PREP_LABEL: {
        int iter_reg = ip->operands[0];
        if (vm->registers[iter_reg].type == VAL_TABLE) {
            Table* table = vm->registers[iter_reg].table;
            Value start_val;
            if (table_get(table, "__start", &start_val) && start_val.type == VAL_NUMBER) {
                table_set(table, "__current", vm_make_number(start_val.number));
                value_decref(&start_val);
            }
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_FOR_INIT_LABEL: {
        int var_reg = ip->operands[0]; int end_reg = ip->operands[1]; int step_reg = ip->operands[2];
        if (vm->iterator_depth < VM_MAX_CALL_FRAMES - 1) {
            vm->iterator_depth++;
            vm->iterator_stack[vm->iterator_depth].index = vm->registers[var_reg].number;
            vm->iterator_stack[vm->iterator_depth].end   = vm->registers[end_reg].number;
            vm->iterator_stack[vm->iterator_depth].step  = vm->registers[step_reg].number;
        } else vm_error(vm, "Iterator stack overflow");
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_FOR_NEXT_LABEL: {
        int var_reg = ip->operands[0]; int op1 = ip->operands[1]; int op2 = ip->operands[2]; int exit_addr;
        if (op2 != 0) {
            exit_addr = op2; int iter_reg = op1;
            if (vm->registers[iter_reg].type == VAL_TABLE) {
                Table* table = vm->registers[iter_reg].table;
                Value curr, end, step;
                curr.type = VAL_BOOL; end.type = VAL_BOOL; step.type = VAL_BOOL;
                if (!table_get(table, "__current", &curr) || curr.type != VAL_NUMBER) { ip = &vm->code[exit_addr]; goto *dispatch_table[ip->opcode]; }
                if (!table_get(table, "__end", &end) || end.type != VAL_NUMBER) { value_decref(&curr); ip = &vm->code[exit_addr]; goto *dispatch_table[ip->opcode]; }
                if (!table_get(table, "__step", &step) || step.type != VAL_NUMBER) { step = vm_make_number(1.0); }
                double c = curr.number; double e = end.number; double s = step.number;
                value_decref(&curr); value_decref(&end); value_decref(&step);
                if ((s > 0 && c < e) || (s < 0 && c > e)) {
                    vm->registers[var_reg].type = VAL_NUMBER; vm->registers[var_reg].number = c;
                    table_set(table, "__current", vm_make_number(c + s));
                    if (var_reg >= vm->register_count) vm->register_count = var_reg + 1;
                    ip++; goto *dispatch_table[ip->opcode];
                } else {
                    table_remove(table, "__current"); ip = &vm->code[exit_addr]; goto *dispatch_table[ip->opcode];
                }
            }
            ip = &vm->code[exit_addr]; goto *dispatch_table[ip->opcode];
        } else {
            exit_addr = op1;
            if (vm->iterator_depth >= 0) {
                double c = vm->iterator_stack[vm->iterator_depth].index;
                double e = vm->iterator_stack[vm->iterator_depth].end;
                double s = vm->iterator_stack[vm->iterator_depth].step;
                
                if ((s > 0 && c <= e) || (s < 0 && c >= e)) {
                    vm->registers[var_reg].type = VAL_NUMBER;
                    vm->registers[var_reg].number = c;
                    if (var_reg >= vm->register_count) vm->register_count = var_reg + 1;
                    vm->iterator_stack[vm->iterator_depth].index = c + s;
                    ip++; goto *dispatch_table[ip->opcode];
                } else {
                    vm->iterator_depth--;
                    ip = &vm->code[exit_addr];
                    goto *dispatch_table[ip->opcode];
                }
            }
            ip = &vm->code[exit_addr];
            goto *dispatch_table[ip->opcode];
        }
    }
    OP_POP_ITER_LABEL: if (vm->iterator_depth >= 0) vm->iterator_depth--; ip++; goto *dispatch_table[ip->opcode];
    OP_BREAK_LABEL: ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode];
    OP_CONTINUE_LABEL: ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode];
    OP_JUMP_IF_LT_LABEL: {
        int target = ip->operands[0]; Value* left = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]];
        if (left->type == VAL_NUMBER && right->type == VAL_NUMBER && left->number < right->number) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_LTE_LABEL: {
        int target = ip->operands[0]; Value* left = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]];
        if (left->type == VAL_NUMBER && right->type == VAL_NUMBER && left->number <= right->number) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_GT_LABEL: {
        int target = ip->operands[0]; Value* left = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]];
        if (left->type == VAL_NUMBER && right->type == VAL_NUMBER && left->number > right->number) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_GTE_LABEL: {
        int target = ip->operands[0];
        if (vm->registers[ip->operands[1]].number >= vm->registers[ip->operands[2]].number) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_EQ_LABEL: {
        int target = ip->operands[0]; Value* left = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]]; bool jump = false;
        if (left->type == right->type) {
            switch (left->type) {
                case VAL_NUMBER: jump = (left->number == right->number); break;
                case VAL_STRING: jump = string_equal(left->string, right->string); break;
                case VAL_BOOL:   jump = (left->boolean == right->boolean); break;
                default: break;
            }
        }
        if (jump) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_NEQ_LABEL: {
        int target = ip->operands[0]; Value* left = &vm->registers[ip->operands[1]]; Value* right = &vm->registers[ip->operands[2]]; bool jump = true;
        if (left->type == right->type) {
            switch (left->type) {
                case VAL_NUMBER: jump = (left->number != right->number); break;
                case VAL_STRING: jump = !string_equal(left->string, right->string); break;
                case VAL_BOOL:   jump = (left->boolean != right->boolean); break;
                default: break;
            }
        }
        if (jump) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_PUSH_ARG_LABEL: {
        int reg = ip->operands[0];
        if (vm->args_top >= VM_MAX_ARGS_STACK) vm_error(vm, "Arguments stack overflow");
        else {
            Value* src = &vm->registers[reg];
            vm->args_stack[vm->args_top] = *src;
            value_incref(&vm->args_stack[vm->args_top]);
            vm->args_top++;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_PRINT_LABEL: vm_print_value(&vm->registers[ip->operands[0]]); printf("\n"); fflush(stdout); ip++; goto *dispatch_table[ip->opcode];
    OP_HALT_LABEL: vm->running = false; return !vm->had_error;
    OP_ADD_IMM_LABEL: {
        int dest = ip->operands[0]; int src = ip->operands[1]; double imm = chunk->constants[ip->operands[2]].number_value;
        vm->registers[dest].type = VAL_NUMBER; vm->registers[dest].number = vm->registers[src].number + imm;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_LOAD_BOOL_LABEL: {
        int dest = ip->operands[0];
        vm->registers[dest].type = VAL_BOOL; vm->registers[dest].boolean = ip->operands[1] != 0;
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_DUP_LABEL: {
        int dest = ip->operands[0]; int src = ip->operands[1]; Value* sv = &vm->registers[src];
        value_decref(&vm->registers[dest]);
        vm->registers[dest] = *sv;
        value_incref(&vm->registers[dest]);
        if (dest >= vm->register_count) vm->register_count = dest + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_SWAP_LABEL: {
        Value tmp = vm->registers[ip->operands[0]];
        vm->registers[ip->operands[0]] = vm->registers[ip->operands[1]];
        vm->registers[ip->operands[1]] = tmp;
        ip++; goto *dispatch_table[ip->opcode];
    }
    return !vm->had_error;
}

// ========== Debug Functions ==========
void vm_dump_registers(VM* vm) {
    printf("\n=== Registers (used: %d) ===\n", vm->register_count);
    for (int i = 0; i < vm->register_count; i++) {
        printf("R%-3d: ", i); vm_print_value(&vm->registers[i]); printf("\n");
    }
}

void vm_dump_state(VM* vm) {
    printf("\n=== VM State ===\n");
    printf("PC: %d/%d\n", vm->pc, vm->code_count);
    printf("Call depth: %d\n", vm->call_depth);
    vm_dump_registers(vm);
    printf("\n=== Globals (%d) ===\n", vm->global_count);
    for (int i = 0; i < vm->global_count; i++) {
        printf("G%-3d: ", i); vm_print_value(&vm->globals[i]); printf("\n");
    }
}
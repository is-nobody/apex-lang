#include "vm.h"
#include "os_module.h"
#include "sys_module.h"
#include "math_module.h"
#include "string_module.h"
#include "table_module.h"
#include "ffi_module.h"
#include "random_module.h"
#include "codecs_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ========== Forward Declarations ==========
static StringObject* string_create(const char* chars, int length);
static void string_destroy(StringObject* str);
void value_incref(Value* v);
void value_decref(Value* v);
static bool string_equal(StringObject* a, StringObject* b);
static const char* value_to_cstr(Value* v, char* buf, int buf_size);

// ========== Object Pool Implementation ==========
void object_pool_init(ObjectPool* pool) {
    pool->string_pool_count = 0;
    pool->table_pool_count = 0;
    memset(pool->string_pool, 0, sizeof(pool->string_pool));
    memset(pool->table_pool, 0, sizeof(pool->table_pool));
}

void object_pool_free(ObjectPool* pool) {
    // Free remaining strings in pool
    for (int i = 0; i < pool->string_pool_count; i++) {
        free(pool->string_pool[i]);
    }
    pool->string_pool_count = 0;
    // Free remaining tables in pool
    for (int i = 0; i < pool->table_pool_count; i++) {
        table_destroy(pool->table_pool[i]);
    }
    pool->table_pool_count = 0;
}

StringObject* string_create_pooled(ObjectPool* pool, const char* chars, int length) {
    // Try to reuse from pool (for strings shorter than POOL_STRING_SIZE)
    if (length < POOL_STRING_SIZE && pool->string_pool_count > 0) {
        StringObject* str = pool->string_pool[--pool->string_pool_count];
        if (str->length >= length) {
            memcpy(str->chars, chars, length);
            str->chars[length] = '\0';
            str->length = length;
            str->header.ref_count = 1;
            str->hash_computed = false;
            return str;
        } else {
            // Too small, free it and allocate new
            free(str);
        }
    }
    return string_create(chars, length);
}

void string_destroy_pooled(ObjectPool* pool, StringObject* str) {
    if (!str) return;
    if (str->length < POOL_STRING_SIZE && pool->string_pool_count < POOL_MAX_ITEMS) {
        pool->string_pool[pool->string_pool_count++] = str;
    } else {
        string_destroy(str);
    }
}

Table* table_create_pooled(ObjectPool* pool, int capacity) {
    if (pool->table_pool_count > 0) {
        Table* table = pool->table_pool[--pool->table_pool_count];
        table->header.ref_count = 1;
        table->capacity = capacity > 0 ? capacity : TABLE_ARRAY_INIT;
        table->hash_count = 0;
        table->array_count = 0;
        table->entries = calloc(table->capacity, sizeof(TableEntry*));
        table->array_part = calloc(TABLE_ARRAY_INIT, sizeof(Value));
        table->array_capacity = TABLE_ARRAY_INIT;
        for (int i = 0; i < TABLE_ARRAY_INIT; i++) {
            table->array_part[i].type = VAL_BOOL;
            table->array_part[i].boolean = false;
        }
        return table;
    }
    return table_create(capacity);
}

void table_destroy_pooled(ObjectPool* pool, Table* table) {
    if (!table) return;
    if (pool->table_pool_count < POOL_MAX_ITEMS / 4) {
        // Clean and reuse
        table_clear(table);
        pool->table_pool[pool->table_pool_count++] = table;
    } else {
        table_destroy(table);
    }
}

// ========== String Intern Table Implementation ==========
static unsigned int intern_hash(const char* chars, int length) {
    unsigned int hash = 5381;
    for (int i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)chars[i];
    }
    return hash;
}

void string_intern_table_init(StringInternTable* it) {
    it->capacity = INTERN_INITIAL_SIZE;
    it->count = 0;
    it->buckets = calloc(INTERN_INITIAL_SIZE, sizeof(StringObject*));
}

void string_intern_table_free(StringInternTable* it) {
    for (int i = 0; i < it->capacity; i++) {
        StringObject* str = it->buckets[i];
        while (str) {
            StringObject* next = (StringObject*)((uintptr_t)str->hash_computed ? 
                NULL : NULL); // We'll just free all
            string_destroy(str);
            str = next;
        }
    }
    free(it->buckets);
    it->buckets = NULL;
    it->capacity = 0;
    it->count = 0;
}

static void intern_table_resize(StringInternTable* it, int new_capacity) {
    StringObject** old_buckets = it->buckets;
    int old_capacity = it->capacity;
    
    it->buckets = calloc(new_capacity, sizeof(StringObject*));
    it->capacity = new_capacity;
    it->count = 0;
    
    for (int i = 0; i < old_capacity; i++) {
        StringObject* str = old_buckets[i];
        while (str) {
            StringObject* next = NULL; // Simplified - no chain in our simple impl
            unsigned int idx = intern_hash(str->chars, str->length) % new_capacity;
            // Simple insertion (no chaining, just find new slot)
            while (it->buckets[idx] != NULL) {
                idx = (idx + 1) % new_capacity;
            }
            it->buckets[idx] = str;
            it->count++;
            str = next;
        }
    }
    free(old_buckets);
}

StringObject* string_intern(StringInternTable* it, const char* chars, int length) {
    if (!it || !chars) return NULL;
    
    if (it->count > 50000) {
        // Table is too large, just create without interning
        StringObject* new_str = string_create(chars, length);
        new_str->hash = intern_hash(chars, length);
        new_str->hash_computed = true;
        return new_str;
    }

    if ((double)it->count / it->capacity > INTERN_MAX_LOAD) {
        intern_table_resize(it, it->capacity * 2);
    }
    
    unsigned int hash = intern_hash(chars, length);
    unsigned int idx = hash % it->capacity;
    
    // Linear probing
    for (int i = 0; i < it->capacity; i++) {
        unsigned int probe_idx = (idx + i) % it->capacity;
        StringObject* existing = it->buckets[probe_idx];
        
        if (existing == NULL) {
            // Not found, insert
            StringObject* new_str = string_create(chars, length);
            new_str->hash = hash;
            new_str->hash_computed = true;
            it->buckets[probe_idx] = new_str;
            it->count++;
            return new_str;
        }
        
        if (existing->length == length && 
            memcmp(existing->chars, chars, length) == 0) {
            // Found! Return existing
            return existing;
        }
    }
    
    return NULL; // Should never happen if resize works
}

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
void value_incref(Value* v) {
    if (v->type == VAL_STRING && v->string) {
        v->string->header.ref_count++;
    } else if (v->type == VAL_TABLE && v->table) {
        v->table->header.ref_count++;
    }
}

void value_decref(Value* v) {
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
    table->hash_count = 0;
    table->entries = (TableEntry**)calloc(table->capacity, sizeof(TableEntry*));
    
    table->array_capacity = 0;
    table->array_part = NULL;
    table->array_count = 0;
    
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
    
    if (table->array_part) {
        for (int i = 0; i < table->array_count; i++) {
            value_decref(&table->array_part[i]);
        }
        free(table->array_part);
    }
    
    free(table);
}

static void array_part_grow(Table* table, int needed_index) {
    if (table->array_part == NULL) {
        // Lazy init
        table->array_capacity = TABLE_ARRAY_INIT;
        while (table->array_capacity <= needed_index) {
            table->array_capacity *= 2;
        }
        table->array_part = (Value*)calloc(table->array_capacity, sizeof(Value));
        for (int i = 0; i < table->array_capacity; i++) {
            table->array_part[i].type = VAL_BOOL;
            table->array_part[i].boolean = false;
        }
        return;
    }
    
    int new_capacity = table->array_capacity;
    while (new_capacity <= needed_index) {
        new_capacity *= 2;
    }
    
    Value* new_array = (Value*)calloc(new_capacity, sizeof(Value));
    for (int i = 0; i < table->array_count; i++) {
        new_array[i] = table->array_part[i];
    }
    for (int i = table->array_count; i < new_capacity; i++) {
        new_array[i].type = VAL_BOOL;
        new_array[i].boolean = false;
    }
    
    free(table->array_part);
    table->array_part = new_array;
    table->array_capacity = new_capacity;
}

bool table_set_int(Table* table, int index, Value value) {
    if (index < 0) return false;
    
    // Lazy init or grow
    if (table->array_part == NULL || index >= table->array_capacity) {
        array_part_grow(table, index);
    }
    
    value_decref(&table->array_part[index]);
    table->array_part[index] = value;
    value_incref(&table->array_part[index]);
    
    if (index >= table->array_count) {
        table->array_count = index + 1;
    }
    return true;
}

bool table_get_int(Table* table, int index, Value* out_value) {
    if (!table || index < 0 || table->array_part == NULL || index >= table->array_count) 
        return false;
    
    if (out_value) {
        *out_value = table->array_part[index];
        value_incref(out_value);
    }
    return true;
}

void table_append(Table* table, Value value) {
    table_set_int(table, table->array_count, value);
}

bool table_set(Table* table, const char* key, Value value) {
    // FAST CHECK: only try integer parsing if first char is digit
    if (key[0] >= '0' && key[0] <= '9') {
        char* endptr;
        long int_key = strtol(key, &endptr, 10);
        if (*endptr == '\0' && int_key > 0 && int_key <= 1000000000) {
            return table_set_int(table, (int)(int_key - 1), value);
        }
    }
    
    // Hash part for string keys
    if ((double)(table->hash_count + 1) / table->capacity > TABLE_MAX_LOAD) {
        int old_capacity = table->capacity;
        TableEntry** old_entries = table->entries;
        
        table->capacity = old_capacity * 2;
        table->entries = (TableEntry**)calloc(table->capacity, sizeof(TableEntry*));
        table->hash_count = 0;
        
        // Reinsert all old entries directly (no recursion, no parsing)
        for (int i = 0; i < old_capacity; i++) {
            TableEntry* entry = old_entries[i];
            while (entry) {
                TableEntry* next = entry->next;
                
                unsigned int idx = hash_key(entry->key, table->capacity);
                entry->next = table->entries[idx];
                table->entries[idx] = entry;
                table->hash_count++;
                
                entry = next;
            }
        }
        free(old_entries);
    }
    
    // Now insert the new key
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
    table->hash_count++;
    return true;
}

bool table_get(Table* table, const char* key, Value* out_value) {
    if (!table || !key) return false;
    
    // FAST CHECK: only try integer parsing if first char is digit
    if (key[0] >= '0' && key[0] <= '9') {
        char* endptr;
        long int_key = strtol(key, &endptr, 10);
        if (*endptr == '\0' && int_key > 0) {
            return table_get_int(table, (int)(int_key - 1), out_value);
        }
    }
    
    // Hash lookup
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
    
    // FAST CHECK: only try integer parsing if first char is digit
    if (key[0] >= '0' && key[0] <= '9') {
        char* endptr;
        long int_key = strtol(key, &endptr, 10);
        if (*endptr == '\0' && int_key > 0 && table->array_part != NULL && int_key - 1 < table->array_count) {
            int idx = (int)(int_key - 1);
            value_decref(&table->array_part[idx]);
            table->array_part[idx].type = VAL_BOOL;
            table->array_part[idx].boolean = false;
            
            while (table->array_count > 0 && 
                   table->array_part[table->array_count - 1].type == VAL_BOOL &&
                   !table->array_part[table->array_count - 1].boolean) {
                table->array_count--;
            }
            return;
        }
    }
    
    // Remove from hash part
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
            table->hash_count--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

int table_size(Table* table) {
    return table ? table->array_count + table->hash_count : 0;
}

char** table_keys(Table* table, int* out_count) {
    if (!table || !out_count) return NULL;
    *out_count = 0;
    
    int total = table->array_count + table->hash_count;
    if (total == 0) return NULL;
    
    char** keys = (char**)malloc(sizeof(char*) * total);
    if (!keys) return NULL;
    
    int idx = 0;
    
    // Integer keys from array part
    for (int i = 0; i < table->array_count; i++) {
        if (table->array_part[i].type != VAL_BOOL || table->array_part[i].boolean) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", i + 1);
            keys[idx++] = strdup(buf);
        }
    }
    
    // String keys from hash part
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            keys[idx++] = entry->key; // Don't strdup, already owned
            entry = entry->next;
        }
    }
    
    *out_count = idx;
    return keys;
}

void table_clear(Table* table) {
    if (!table) return;
    
    // Clear hash entries
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
    table->hash_count = 0;
    
    // Clear array part
    for (int i = 0; i < table->array_count; i++) {
        value_decref(&table->array_part[i]);
        table->array_part[i].type = VAL_BOOL;
        table->array_part[i].boolean = false;
    }
    table->array_count = 0;
}

Table* table_copy(Table* table) {
    if (!table) return NULL;
    
    Table* copy = table_create(table->capacity);
    
    // Copy hash entries
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            table_set(copy, entry->key, entry->value);
            entry = entry->next;
        }
    }
    
    // Copy array part
    for (int i = 0; i < table->array_count; i++) {
        table_set_int(copy, i, table->array_part[i]);
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
VM* vm_create(const char* source) {
    VM* vm = (VM*)calloc(1, sizeof(VM));
    if (!vm) return NULL;
    
    vm->register_frames = (Value*)calloc(VM_MAX_FRAMES * VM_REGS_PER_FRAME, sizeof(Value));
    if (!vm->register_frames) { free(vm); return NULL; }
    
    vm->registers = vm->register_frames;
    vm->register_count = 0;
    vm->global_count = 0;
    vm->call_depth = 0;
    vm->iterator_depth = -1;
    vm->running = false;
    vm->had_error = false;
    vm->current_frame = 0;
    vm->args_top = 0;
    vm->source = source;

    for (int i = 0; i < VM_REGS_PER_FRAME; i++) {
        vm->registers[i].type = VAL_BOOL;
        vm->registers[i].boolean = false;
    }
    
    string_intern_table_init(&vm->intern_table);
    object_pool_init(&vm->obj_pool);
    
    return vm;
}

void vm_destroy(VM* vm) {
    if (!vm) return;
    
    int frames_to_clean = vm->current_frame + 2;
    if (frames_to_clean > VM_MAX_FRAMES) frames_to_clean = VM_MAX_FRAMES;
    
    for (int f = 0; f < frames_to_clean; f++) {
        for (int i = 0; i < VM_REGS_PER_FRAME; i++) {
            value_decref(&vm->register_frames[f * VM_REGS_PER_FRAME + i]);
        }
    }

    for (int i = 0; i < vm->global_count; i++) {
        value_decref(&vm->globals[i]);
    }
    for (int i = 0; i < vm->args_top; i++) {
        value_decref(&vm->args_stack[i]);
    }
    
    string_intern_table_free(&vm->intern_table);
    object_pool_free(&vm->obj_pool);
    
    free(vm->register_frames);
    free(vm);
}

// ========== Built-in Functions ==========
static bool vm_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strncmp(name, "os.", 3) == 0) return os_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "sys.", 4) == 0) return sys_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "math.", 5) == 0) return math_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "string.", 7) == 0) return string_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "table.", 6) == 0) return table_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "ffi.", 4) == 0) return ffi_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "random.", 7) == 0) return random_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "codecs.", 7) == 0) return codecs_call_builtin(vm, name, arg_count, args, result);

    if (strcmp(name, "number") == 0) {
        if (arg_count >= 1) {
            switch (args[0].type) {
                case VAL_STRING: {
                    char* endptr;
                    double val = strtod(args[0].string->chars, &endptr);
                    
                    if (endptr == args[0].string->chars || *endptr != '\0') {
                        *result = vm_make_bool(false);
                    } else {
                        *result = vm_make_number(val);
                    }
                    break;
                }
                case VAL_NUMBER:
                    *result = vm_copy_value(args[0]);
                    break;
                case VAL_BOOL:
                    *result = vm_make_bool(false);
                    break;
                default:
                    *result = vm_make_bool(false);
                    break;
            }
        } else {
            *result = vm_make_bool(false);
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
                    *result = vm_make_string("");
                    break;
            }
        }
        return true;
    }
    if (strcmp(name, "type") == 0) {
        if (arg_count >= 1) {
            *result = vm_make_string(vm_value_type_name(&args[0]));
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    return false;
}

// ========== Main Execution Loop ==========
bool vm_execute(VM* vm, BytecodeChunk* chunk) {
    if (!vm || !chunk) return false;

    volatile double warmup_dummy = 1.0;
    for (int i = 0; i < 10000000; i++) {
        warmup_dummy = warmup_dummy * 1.0000001 + 1.0;
    }

    vm->chunk = chunk;
    vm->code = chunk->code;
    vm->code_count = chunk->code_count;
    vm->running = true;
    vm->had_error = false;
    vm->global_count = chunk->global_count;
    vm->register_count = chunk->functions[0].local_count + 32;
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
        [OP_JUMP_IF_LT]       = &&OP_JUMP_IF_LT_LABEL,
        [OP_JUMP_IF_LTE]      = &&OP_JUMP_IF_LTE_LABEL,
        [OP_JUMP_IF_GT]       = &&OP_JUMP_IF_GT_LABEL,
        [OP_JUMP_IF_GTE]      = &&OP_JUMP_IF_GTE_LABEL,
        [OP_JUMP_IF_EQ]       = &&OP_JUMP_IF_EQ_LABEL,
        [OP_JUMP_IF_NEQ]      = &&OP_JUMP_IF_NEQ_LABEL,
        [OP_PUSH_ARG]         = &&OP_PUSH_ARG_LABEL,
        [OP_HALT]             = &&OP_HALT_LABEL,
        [OP_ADD_IMM]          = &&OP_ADD_IMM_LABEL,
        [OP_LOAD_BOOL]        = &&OP_LOAD_BOOL_LABEL,
        [OP_LOAD_CONST_NUM]   = &&OP_LOAD_CONST_NUM_LABEL,
    };

    register Instruction* ip = vm->code;
    register Value* regs = vm->registers;
    __builtin_prefetch(ip + 1, 0, 1);
    goto *dispatch_table[ip->opcode];

    OP_NOP_LABEL: ip++; goto *dispatch_table[ip->opcode];
    OP_LOAD_CONST_NUM_LABEL: {
        int dest = ip->operands[0]; int value = ip->operands[1];
        regs[dest].type = VAL_NUMBER; regs[dest].number = (double)value;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_LOAD_CONST_LABEL: {
        int dest = ip->operands[0]; int const_idx = ip->operands[1];
        Constant* c = &chunk->constants[const_idx];
        value_decref(&regs[dest]);
        switch (c->type) {
            case CONST_NUMBER: 
                regs[dest].type = VAL_NUMBER; 
                regs[dest].number = c->number_value; 
                break;
            case CONST_STRING: 
                {
                    int len = (int)strlen(c->string_value);
                    // Only intern medium strings, never short ones
                    if (len >= 16 && len <= 64 && vm->intern_table.count < 50000) {
                        regs[dest].type = VAL_STRING;
                        regs[dest].string = string_intern(&vm->intern_table, c->string_value, len);
                        value_incref(&regs[dest]);
                    } else {
                        regs[dest].type = VAL_STRING;
                        regs[dest].string = string_create(c->string_value, len);
                    }
                }
                break;
            case CONST_BOOL: 
                regs[dest].type = VAL_BOOL; 
                regs[dest].boolean = c->bool_value; 
                break;
            default: 
                break;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MOVE_LABEL: {
        int dest = ip->operands[0]; int src = ip->operands[1];
        Value* sv = &regs[src];
        if (sv->type == VAL_NUMBER || sv->type == VAL_BOOL) {
            regs[dest] = *sv;
        } else {
            value_decref(&regs[dest]);
            regs[dest] = *sv;
            value_incref(&regs[dest]);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_ADD_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_NUMBER;
        regs[dest].number = regs[ip->operands[1]].number + regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_SUB_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_NUMBER;
        regs[dest].number = regs[ip->operands[1]].number - regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MUL_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_NUMBER;
        regs[dest].number = regs[ip->operands[1]].number * regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_DIV_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_NUMBER;
        regs[dest].number = regs[ip->operands[1]].number / regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MOD_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_NUMBER;
        regs[dest].number = fmod(regs[ip->operands[1]].number, regs[ip->operands[2]].number);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NEG_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_NUMBER;
        regs[dest].number = -regs[ip->operands[1]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_EQ_LABEL: {
        int dest = ip->operands[0];
        Value* left = &regs[ip->operands[1]];
        Value* right = &regs[ip->operands[2]];
        int result = 0;
        switch (left->type) {
            case VAL_NUMBER: result = (left->number == right->number); break;
            case VAL_STRING: result = string_equal(left->string, right->string); break;
            case VAL_BOOL:   result = (left->boolean == right->boolean); break;
            default: break;
        }
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = result;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_NEQ_LABEL: {
        int dest = ip->operands[0];
        Value* left = &regs[ip->operands[1]];
        Value* right = &regs[ip->operands[2]];
        int result = 1;
        switch (left->type) {
            case VAL_NUMBER: result = (left->number != right->number); break;
            case VAL_STRING: result = !string_equal(left->string, right->string); break;
            case VAL_BOOL:   result = (left->boolean != right->boolean); break;
            default: break;
        }
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = result;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_LT_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = regs[ip->operands[1]].number < regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_GT_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = regs[ip->operands[1]].number > regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_LTE_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = regs[ip->operands[1]].number <= regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_GTE_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = regs[ip->operands[1]].number >= regs[ip->operands[2]].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_AND_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = regs[ip->operands[1]].boolean && regs[ip->operands[2]].boolean;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_OR_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = regs[ip->operands[1]].boolean || regs[ip->operands[2]].boolean;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NOT_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL;
        regs[dest].boolean = !regs[ip->operands[1]].boolean;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TO_NUMBER_LABEL: {
        int dest = ip->operands[0];
        Value* src = &regs[ip->operands[1]];
        switch (src->type) {
            case VAL_STRING:
                regs[dest].type = VAL_NUMBER;
                regs[dest].number = atof(src->string->chars);
                break;
            case VAL_NUMBER:
                regs[dest].type = VAL_NUMBER;
                regs[dest].number = src->number;
                break;
            case VAL_BOOL:
                regs[dest].type = VAL_NUMBER;
                regs[dest].number = src->boolean ? 1.0 : 0.0;
                break;
            default:
                regs[dest].type = VAL_NUMBER;
                regs[dest].number = 0.0;
                break;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TO_STRING_LABEL: {
        int dest = ip->operands[0]; 
        Value* src = &regs[ip->operands[1]];
        char buffer[256];
        value_decref(&regs[dest]);
        switch (src->type) {
            case VAL_NUMBER: {
                double num = src->number;
                int len;
                if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) {
                    len = snprintf(buffer, sizeof(buffer), "%lld", (long long)num);
                } else {
                    len = snprintf(buffer, sizeof(buffer), "%.15g", num);
                }
                // Only intern medium-length numeric strings
                if (len >= 8 && len <= 32 && vm->intern_table.count < 50000) {
                    regs[dest].type = VAL_STRING;
                    regs[dest].string = string_intern(&vm->intern_table, buffer, len);
                    value_incref(&regs[dest]);
                } else {
                    regs[dest] = vm_make_string(buffer);
                }
                break;
            }
            case VAL_BOOL: 
                regs[dest] = vm_make_string(src->boolean ? "true" : "false"); 
                break;
            case VAL_STRING: 
                regs[dest] = *src; 
                value_incref(&regs[dest]); 
                break;
            default: 
                regs[dest] = vm_make_string("false"); 
                break;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TO_BOOL_LABEL: {
        int dest = ip->operands[0]; Value* src = &regs[ip->operands[1]];
        regs[dest].type = VAL_BOOL;
        switch (src->type) {
            case VAL_BOOL: regs[dest].boolean = src->boolean; break;
            case VAL_STRING: regs[dest].boolean = (src->string->length > 0); break;
            case VAL_NUMBER: regs[dest].boolean = (src->number != 0.0); break;
            default: regs[dest].boolean = false; break;
        }
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
        if (vm->call_depth >= VM_MAX_CALL_FRAMES) {
            vm->had_error = true;
            vm->running = false;
            return false; // Prevent stack overflow
        }
        int func_addr = ip->operands[1];
        int arg_count = ip->operands[2];
        int dest_reg  = ip->operands[0];

        vm->call_stack[vm->call_depth].return_address = (ip + 1) - vm->code;
        vm->call_stack[vm->call_depth].dest_reg       = dest_reg;
        vm->call_stack[vm->call_depth].frame_index    = vm->current_frame;
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;
        vm->call_depth++;
        vm->current_frame++;
        
        vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];
        regs = vm->registers;

        for (int i = 0; i < arg_count; i++) {
            regs[i] = vm->args_stack[vm->args_top - arg_count + i];
        }
        vm->args_top -= arg_count;
        ip = &vm->code[func_addr];
        goto *dispatch_table[ip->opcode];
    }
    OP_CALL_BUILTIN_LABEL: {
        int dest_reg = ip->operands[0]; int name_idx = ip->operands[1]; int arg_count = ip->operands[2];
        Value args[VM_MAX_ARGS_STACK];
        
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
        Value ret_val = regs[value_reg];
        value_incref(&ret_val);
        
        if (vm->call_depth > 0) {
            vm->call_depth--;
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;
            
            vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];
            regs = vm->registers;
            
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;
            value_decref(&regs[dest_reg]);
            regs[dest_reg] = ret_val;
            
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
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;
            
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;
            vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];
            regs = vm->registers;
            value_decref(&regs[dest_reg]);
            regs[dest_reg].type = VAL_BOOL; regs[dest_reg].boolean = false;
            ip = &vm->code[return_addr]; goto *dispatch_table[ip->opcode];
        }
        vm->running = false; goto OP_HALT_LABEL;
    }

    OP_LOAD_GLOBAL_LABEL: {
        int dest = ip->operands[0]; int idx = ip->operands[1];
        Value* gv = &vm->globals[idx];
        if (gv->type == VAL_NUMBER || gv->type == VAL_BOOL) {
            regs[dest] = *gv;
        } else {
            value_decref(&regs[dest]);
            regs[dest] = *gv;
            value_incref(&regs[dest]);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_STORE_GLOBAL_LABEL: {
        int src = ip->operands[0]; int idx = ip->operands[1];
        Value* sv = &regs[src];
        value_decref(&vm->globals[idx]);
        vm->globals[idx] = *sv;
        value_incref(&vm->globals[idx]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NEW_TABLE_LABEL: {
        int dest = ip->operands[0];
        value_decref(&vm->registers[dest]);
        vm->registers[dest] = vm_make_table();
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_SET_LABEL: {
        int table_reg = ip->operands[0]; 
        int key_reg = ip->operands[1]; 
        int val_reg = ip->operands[2];
        
        Table* table = vm->registers[table_reg].table;
        Value* key = &vm->registers[key_reg];
        
        // FAST PATH: numeric integer key -> direct array access O(1)
        if (key->type == VAL_NUMBER) {
            double num = key->number;
            if (num >= 1 && num == (int)num) {
                table_set_int(table, (int)num - 1, vm->registers[val_reg]);
                ip++; goto *dispatch_table[ip->opcode];
            }
        }
        
        // SLOW PATH: convert to string
        const char* key_cstr = "";
        char num_buf[64];
        
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
        int table_reg = ip->operands[0];
        int key_idx = ip->operands[1];
        int val_reg = ip->operands[2];
        table_set(vm->registers[table_reg].table,
                  chunk->constants[key_idx].string_value,
                  vm->registers[val_reg]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_LABEL: {
        int dest = ip->operands[0];
        int table_reg = ip->operands[1];
        int key_reg = ip->operands[2];
        
        Value* table_val = &vm->registers[table_reg];
        Value* key = &vm->registers[key_reg];
        
        if (table_val->type != VAL_TABLE) {
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_BOOL;
            vm->registers[dest].boolean = false;
            ip++; goto *dispatch_table[ip->opcode];
        }
        
        Table* table = table_val->table;
        
        // FAST PATH: numeric integer key -> direct array access O(1)
        if (key->type == VAL_NUMBER) {
            double num = key->number;
            if (num >= 1 && num == (int)num && (int)num - 1 < 1000000000) {
                int idx = (int)num - 1;
                if (table->array_part != NULL && idx < table->array_count) {
                    value_decref(&vm->registers[dest]);
                    vm->registers[dest] = table->array_part[idx];
                    value_incref(&vm->registers[dest]);
                    ip++; goto *dispatch_table[ip->opcode];
                }
            }
        }
        
        // SLOW PATH: convert to string and hash lookup
        char key_str[256];
        if (key->type == VAL_STRING) {
            strcpy(key_str, key->string->chars);
        } else {
            snprintf(key_str, sizeof(key_str), "%g", key->number);
        }
        
        Value val;
        val.type = VAL_BOOL;
        val.boolean = false;
        if (table_get(table, key_str, &val)) {
            value_decref(&vm->registers[dest]);
            vm->registers[dest] = val;
        } else {
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_BOOL;
            vm->registers[dest].boolean = false;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_CONST_LABEL: {
        int dest = ip->operands[0];
        int table_reg = ip->operands[1];
        int key_idx = ip->operands[2];
        
        Value* table_val = &vm->registers[table_reg];
        
        if (table_val->type != VAL_TABLE) {
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_BOOL;
            vm->registers[dest].boolean = false;
            ip++; goto *dispatch_table[ip->opcode];
        }
        
        // FAST PATH: try integer key from constant string
        const char* key_str = chunk->constants[key_idx].string_value;
        char* endptr;
        long int_key = strtol(key_str, &endptr, 10);
        if (*endptr == '\0' && int_key > 0) {
            int idx = (int)(int_key - 1);
            Table* table = table_val->table;
            if (idx < table->array_count) {
                value_decref(&vm->registers[dest]);
                vm->registers[dest] = table->array_part[idx];
                value_incref(&vm->registers[dest]);
                ip++; goto *dispatch_table[ip->opcode];
            }
        }

        // SLOW PATH: hash lookup
        Value val;
        val.type = VAL_BOOL;
        val.boolean = false;
        if (table_get(table_val->table, key_str, &val)) {
            value_decref(&vm->registers[dest]);
            vm->registers[dest] = val;
        } else {
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_BOOL;
            vm->registers[dest].boolean = false;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_APPEND_LABEL: {
        int table_reg = ip->operands[0];
        int val_reg = ip->operands[1];
        
        Table* table = vm->registers[table_reg].table;
        Value* val = &vm->registers[val_reg];
        
        // Ensure array_part exists (lazy init)
        if (table->array_part == NULL) {
            table->array_capacity = TABLE_ARRAY_INIT;
            table->array_part = (Value*)calloc(TABLE_ARRAY_INIT, sizeof(Value));
            for (int i = 0; i < TABLE_ARRAY_INIT; i++) {
                table->array_part[i].type = VAL_BOOL;
                table->array_part[i].boolean = false;
            }
        }
        
        int idx = table->array_count;
        if (idx >= table->array_capacity) {
            array_part_grow(table, idx);
        }
        
        value_decref(&table->array_part[idx]);
        table->array_part[idx] = *val;
        value_incref(&table->array_part[idx]);
        table->array_count++;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CONCAT_LABEL: {
        int dest = ip->operands[0];
        Value* left  = &vm->registers[ip->operands[1]]; 
        Value* right = &vm->registers[ip->operands[2]];
        
        char lbuf[64], rbuf[64];
        const char* ls = value_to_cstr(left, lbuf, sizeof(lbuf));
        const char* rs = value_to_cstr(right, rbuf, sizeof(rbuf));
        int llen = (left->type == VAL_STRING) ? left->string->length : (int)strlen(ls);
        int rlen = (right->type == VAL_STRING) ? right->string->length : (int)strlen(rs);
        int total_len = llen + rlen;
        
        // Smart interning: only for medium strings (16-64 chars)
        // Too short: direct malloc is faster
        // Too long: pollutes intern table
        if (total_len >= 16 && total_len <= 64 && vm->intern_table.count < 50000) {
            char combined[65];
            memcpy(combined, ls, llen);
            memcpy(combined + llen, rs, rlen);
            combined[total_len] = '\0';
            
            StringObject* interned = string_intern(&vm->intern_table, combined, total_len);
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_STRING;
            vm->registers[dest].string = interned;
            value_incref(&vm->registers[dest]);
        } else if (total_len < 16) {
            // For very short strings: direct StringBuilder (faster than interning)
            StringBuilder sb;
            sb_init(&sb, total_len + 1);
            sb_append(&sb, ls, llen);
            sb_append(&sb, rs, rlen);
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_STRING;
            vm->registers[dest].string = sb_to_string(&sb);
            sb_free(&sb);
        } else {
            // For long strings: StringBuilder
            StringBuilder sb;
            sb_init(&sb, total_len + 1);
            sb_append(&sb, ls, llen);
            sb_append(&sb, rs, rlen);
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_STRING;
            vm->registers[dest].string = sb_to_string(&sb);
            sb_free(&sb);
        }
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
        int var_reg = ip->operands[0];
        int end_reg = ip->operands[1];
        int step_reg = ip->operands[2];
        vm->iterator_depth++;
        vm->iterator_stack[vm->iterator_depth].index = vm->registers[var_reg].number;
        vm->iterator_stack[vm->iterator_depth].end   = vm->registers[end_reg].number;
        vm->iterator_stack[vm->iterator_depth].step  = vm->registers[step_reg].number;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_FOR_NEXT_LABEL: {
        int var_reg = ip->operands[0];      
        int end_or_size_reg = ip->operands[1];
        int flag_or_exit = ip->operands[2];
        
        if (flag_or_exit == 0) {
            // Range iteration
            int exit_addr = end_or_size_reg;
            
            if (vm->iterator_depth >= 0) {
                double c = vm->iterator_stack[vm->iterator_depth].index;
                double e = vm->iterator_stack[vm->iterator_depth].end;
                double s = vm->iterator_stack[vm->iterator_depth].step;
                
                if ((s > 0 && c <= e) || (s < 0 && c >= e)) {
                    vm->registers[var_reg].type = VAL_NUMBER;
                    vm->registers[var_reg].number = c;
                    if (var_reg >= vm->register_count) vm->register_count = var_reg + 1;
                    vm->iterator_stack[vm->iterator_depth].index = c + s;
                    ip++;
                    goto *dispatch_table[ip->opcode];
                } else {
                    vm->iterator_depth--;
                    ip = &vm->code[exit_addr];
                    goto *dispatch_table[ip->opcode];
                }
            }
            ip = &vm->code[exit_addr];
            goto *dispatch_table[ip->opcode];
            
        } else {
            // Table iteration
            int exit_addr = flag_or_exit;
            
            double index = vm->registers[var_reg].number;
            double size = vm->registers[end_or_size_reg].number;
            
            // Increment index for table iteration
            index += 1.0;
            vm->registers[var_reg].number = index;
            
            if (index <= size) {
                if (var_reg >= vm->register_count) vm->register_count = var_reg + 1;
                ip++;
                goto *dispatch_table[ip->opcode];
            } else {
                ip = &vm->code[exit_addr];
                goto *dispatch_table[ip->opcode];
            }
        }
    }
    OP_POP_ITER_LABEL: if (vm->iterator_depth >= 0) vm->iterator_depth--; ip++; goto *dispatch_table[ip->opcode];
    OP_JUMP_IF_LT_LABEL: {
        int target = ip->operands[0];
        if (regs[ip->operands[1]].number < regs[ip->operands[2]].number) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_LTE_LABEL: {
        int target = ip->operands[0];
        if (regs[ip->operands[1]].number <= regs[ip->operands[2]].number) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_GT_LABEL: {
        int target = ip->operands[0];
        if (regs[ip->operands[1]].number > regs[ip->operands[2]].number) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_GTE_LABEL: {
        int target = ip->operands[0];
        if (vm->registers[ip->operands[1]].number >= vm->registers[ip->operands[2]].number) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_EQ_LABEL: {
        int target = ip->operands[0];
        Value* left = &regs[ip->operands[1]];
        Value* right = &regs[ip->operands[2]];
        bool jump = false;
        switch (left->type) {
            case VAL_NUMBER: jump = (left->number == right->number); break;
            case VAL_STRING: jump = string_equal(left->string, right->string); break;
            case VAL_BOOL:   jump = (left->boolean == right->boolean); break;
            default: break;
        }
        if (jump) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_NEQ_LABEL: {
        int target = ip->operands[0];
        Value* left = &regs[ip->operands[1]];
        Value* right = &regs[ip->operands[2]];
        bool jump = true;
        switch (left->type) {
            case VAL_NUMBER: jump = (left->number != right->number); break;
            case VAL_STRING: jump = !string_equal(left->string, right->string); break;
            case VAL_BOOL:   jump = (left->boolean != right->boolean); break;
            default: break;
        }
        if (jump) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_PUSH_ARG_LABEL: {
        if (vm->args_top >= VM_MAX_ARGS_STACK) {
            vm->had_error = true;
            vm->running = false;
            return false; // Prevent args_stack overflow
        }
        int reg = ip->operands[0];
        Value* src = &vm->registers[reg];
        vm->args_stack[vm->args_top] = *src;
        value_incref(&vm->args_stack[vm->args_top]);
        vm->args_top++;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_HALT_LABEL: vm->running = false; return !vm->had_error;
    OP_ADD_IMM_LABEL: {
        int dest = ip->operands[0]; int src = ip->operands[1]; 
        double imm = chunk->constants[ip->operands[2]].number_value;
        regs[dest].type = VAL_NUMBER; 
        regs[dest].number = regs[src].number + imm;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_LOAD_BOOL_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL; regs[dest].boolean = ip->operands[1] != 0;
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
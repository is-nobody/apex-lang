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

static StringObject* string_create(const char* chars, int length);
static void string_destroy(StringObject* str);
void value_incref(Value* v);
void value_decref(Value* v);
static bool string_equal(StringObject* a, StringObject* b);
static const char* value_to_cstr(Value* v, char* buf, int buf_size);

// initializes the object pool with empty arrays
void object_pool_init(ObjectPool* pool) {
    pool->string_pool_count = 0;
    pool->table_pool_count = 0;
    memset(pool->string_pool, 0, sizeof(pool->string_pool));
    memset(pool->table_pool, 0, sizeof(pool->table_pool));
}

// frees all objects remaining in the pool
void object_pool_free(ObjectPool* pool) {
    for (int i = 0; i < pool->string_pool_count; i++) {
        free(pool->string_pool[i]);
    }
    pool->string_pool_count = 0;
    for (int i = 0; i < pool->table_pool_count; i++) {
        table_destroy(pool->table_pool[i]);
    }
    pool->table_pool_count = 0;
}

// creates a string from the pool if available, otherwise allocates new
StringObject* string_create_pooled(ObjectPool* pool, const char* chars, int length) {
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
            free(str);
        }
    }
    return string_create(chars, length);
}

// returns a string to the pool for reuse if it's small enough
void string_destroy_pooled(ObjectPool* pool, StringObject* str) {
    if (!str) return;
    if (str->length < POOL_STRING_SIZE && pool->string_pool_count < POOL_MAX_ITEMS) {
        pool->string_pool[pool->string_pool_count++] = str;
    } else {
        string_destroy(str);
    }
}

// creates a table from the pool if available, otherwise allocates new
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

// returns a table to the pool for reuse after clearing it
void table_destroy_pooled(ObjectPool* pool, Table* table) {
    if (!table) return;
    if (pool->table_pool_count < POOL_MAX_ITEMS / 4) {
        table_clear(table);
        pool->table_pool[pool->table_pool_count++] = table;
    } else {
        table_destroy(table);
    }
}

// djb2 hash function for string interning
static unsigned int intern_hash(const char* chars, int length) {
    unsigned int hash = 5381;
    for (int i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)chars[i];
    }
    return hash;
}

// initializes the string intern table with a fixed size
void string_intern_table_init(StringInternTable* it) {
    it->capacity = INTERN_INITIAL_SIZE;
    it->count = 0;
    it->buckets = calloc(INTERN_INITIAL_SIZE, sizeof(StringObject*));
}

// frees all interned strings and the table itself
void string_intern_table_free(StringInternTable* it) {
    for (int i = 0; i < it->capacity; i++) {
        StringObject* str = it->buckets[i];
        while (str) {
            StringObject* next = (StringObject*)((uintptr_t)str->hash_computed ? 
                NULL : NULL);
            string_destroy(str);
            str = next;
        }
    }
    free(it->buckets);
    it->buckets = NULL;
    it->capacity = 0;
    it->count = 0;
}

// resizes the intern table when load factor exceeds the threshold
static void intern_table_resize(StringInternTable* it, int new_capacity) {
    StringObject** old_buckets = it->buckets;
    int old_capacity = it->capacity;
    
    it->buckets = calloc(new_capacity, sizeof(StringObject*));
    it->capacity = new_capacity;
    it->count = 0;
    
    for (int i = 0; i < old_capacity; i++) {
        StringObject* str = old_buckets[i];
        while (str) {
            StringObject* next = NULL;
            unsigned int idx = intern_hash(str->chars, str->length) % new_capacity;
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

// interns a string, returning a canonical object with linear probing
StringObject* string_intern(StringInternTable* it, const char* chars, int length) {
    if (!it || !chars) return NULL;
    
    if (it->count > 50000) {
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
    
    for (int i = 0; i < it->capacity; i++) {
        unsigned int probe_idx = (idx + i) % it->capacity;
        StringObject* existing = it->buckets[probe_idx];
        
        if (existing == NULL) {
            StringObject* new_str = string_create(chars, length);
            new_str->hash = hash;
            new_str->hash_computed = true;
            it->buckets[probe_idx] = new_str;
            it->count++;
            return new_str;
        }
        
        if (existing->length == length && 
            memcmp(existing->chars, chars, length) == 0) {
            return existing;
        }
    }
    
    return NULL;
}

// allocates a new string object with refcount and flexible array
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

// computes the hash of a string lazily
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

// frees a string object
static void string_destroy(StringObject* str) {
    if (str) free(str);
}

// compares two strings by length, hash, and content
static bool string_equal(StringObject* a, StringObject* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->length != b->length) return false;
    if (string_get_hash(a) != string_get_hash(b)) return false;
    return memcmp(a->chars, b->chars, a->length) == 0;
}

// converts a value to a C string for concatenation
static const char* value_to_cstr(Value* v, char* buf, int buf_size) {
    switch (v->type) {
        case VAL_STRING: return v->string->chars;
        case VAL_NUMBER:
            snprintf(buf, buf_size, "%.15g", v->number);
            return buf;
        case VAL_NONE:
            return "none";
        case VAL_BOOL:
            return v->boolean ? "true" : "false";
        default:
            return "";
    }
}

// increments the reference count of a reference-counted value
void value_incref(Value* v) {
    if (v->type == VAL_STRING && v->string) {
        v->string->header.ref_count++;
    } else if (v->type == VAL_TABLE && v->table) {
        v->table->header.ref_count++;
    }
}

// decrements the reference count and frees the object when it reaches zero
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

// constructs a numeric value
Value vm_make_number(double value) {
    Value v; v.type = VAL_NUMBER; v.number = value; return v;
}

// constructs a string value
Value vm_make_string(const char* value) {
    Value v;
    v.type = VAL_STRING;
    v.string = string_create(value, (int)strlen(value));
    return v;
}

// constructs a none/null value (represents absence of a value)
Value vm_make_none() {
    Value v;
    v.type = VAL_NONE;
    return v;
}

// constructs a boolean value
Value vm_make_bool(bool value) {
    Value v; v.type = VAL_BOOL; v.boolean = value; return v;
}

// constructs a new empty table value
Value vm_make_table() {
    Value v;
    v.type = VAL_TABLE;
    v.table = table_create(8);
    return v;
}

// copies a value with proper reference counting
Value vm_copy_value(Value value) {
    value_incref(&value);
    return value;
}

// frees a value and decrements its reference count
void vm_free_value(Value* value) {
    value_decref(value);
}

// returns a type name string for a value
const char* vm_value_type_name(Value* value) {
    switch (value->type) {
        case VAL_NUMBER: return "number";
        case VAL_STRING: return "string";
        case VAL_NONE:   return "none";
        case VAL_BOOL:   return "boolean";
        case VAL_TABLE:  return "table";
        case VAL_FUNCTION: return "function";
        default: return "unknown";
    }
}

// dynamic string builder for efficient concatenation
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

static uint32_t hash_value_key(Value key) {
    if (key.type == VAL_STRING) {
        if (!key.string->hash_computed) {
            uint32_t h = 5381;
            for (int i = 0; i < key.string->length; i++) {
                h = ((h << 5) + h) + (uint8_t)key.string->chars[i];
            }
            key.string->hash = h;
            key.string->hash_computed = true;
        }
        return key.string->hash;
    } else if (key.type == VAL_NUMBER) {
        union { double d; uint64_t u; } u;
        u.d = key.number;
        uint32_t hash = 2166136261u;
        hash ^= (uint32_t)(u.u & 0xFFFFFFFF);
        hash *= 16777619u;
        hash ^= (uint32_t)(u.u >> 32);
        hash *= 16777619u;
        return hash;
    }
    return 0;
}

static bool key_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    if (a.type == VAL_STRING) {
        if (a.string == b.string) return true;
        if (a.string->length != b.string->length) return false;
        return memcmp(a.string->chars, b.string->chars, a.string->length) == 0;
    }
    if (a.type == VAL_NUMBER) return a.number == b.number;
    return false;
}

static void table_to_string_builder(Table* table, StringBuilder* sb, int indent_level);

// converts a table to string with indentation, matching print_table_recursive format
static char* table_to_string(Table* table) {
    StringBuilder sb;
    sb_init(&sb, 4096);
    table_to_string_builder(table, &sb, 0);
    char* result = strdup(sb.buffer);
    sb_free(&sb);
    return result;
}

static void table_to_string_builder(Table* table, StringBuilder* sb, int indent_level) {
    for (int i = 0; i < indent_level; i++) sb_append(sb, "    ", 4);
    sb_append(sb, "[\n", 2);
    
    int key_count;
    Value* keys = table_keys(table, &key_count);
    if (key_count == 0) {
        for (int i = 0; i < indent_level + 1; i++) sb_append(sb, "    ", 4);
        sb_append(sb, "]", 1);
        return;
    }

    for (int i = 0; i < key_count; i++) {
        Value key = keys[i];
        Value val;
        if (table_get(table, key, &val)) {
            for (int j = 0; j < indent_level + 1; j++) sb_append(sb, "    ", 4);
            
            if (key.type == VAL_STRING) {
                sb_append(sb, "\"", 1);
                sb_append(sb, key.string->chars, key.string->length);
                sb_append(sb, "\"", 1);
            } else if (key.type == VAL_NUMBER) {
                char num_buf[64];
                snprintf(num_buf, sizeof(num_buf), "%g", key.number);
                sb_append(sb, num_buf, strlen(num_buf));
            }
            
            sb_append(sb, " = ", 3);
            
            char num_buf[64];
            switch (val.type) {
                case VAL_NUMBER: {
                    double num = val.number;
                    if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9)
                        snprintf(num_buf, sizeof(num_buf), "%.0f", num);
                    else
                        snprintf(num_buf, sizeof(num_buf), "%.15g", num);
                    sb_append(sb, num_buf, strlen(num_buf));
                    break;
                }
                case VAL_STRING:
                    sb_append(sb, "\"", 1);
                    sb_append(sb, val.string->chars, val.string->length);
                    sb_append(sb, "\"", 1);
                    break;
                case VAL_BOOL:
                    sb_append(sb, val.boolean ? "true" : "false", val.boolean ? 4 : 5);
                    break;
                case VAL_TABLE:
                    table_to_string_builder(val.table, sb, indent_level + 1);
                    break;
                case VAL_NONE:
                    sb_append(sb, "none", 4);
                    break;
                default:
                    sb_append(sb, "unknown", 7);
                    break;
            }

            if (i < key_count - 1) sb_append(sb, ",", 1);
            sb_append(sb, "\n", 1);
            value_decref(&val);
        }
        value_decref(&key);
    }
    free(keys);

    for (int i = 0; i < indent_level; i++) sb_append(sb, "    ", 4);
    sb_append(sb, "]", 1);
}

// recursively prints a table with indentation for nested structures
static void print_table_recursive(Table* table, int indent_level) {
    (void)indent_level;
    if (!table) return;
    
    char* str = table_to_string(table);
    printf("%s", str);
    free(str);
}

// prints a value to stdout with formatting
void vm_print_value(Value* value) {
    switch (value->type) {
        case VAL_NUMBER: {
            double num = value->number;
            if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9) printf("%.0f", num);
            else printf("%.15g", num);
            break;
        }
        case VAL_STRING: printf("%s", value->string->chars); break;
        case VAL_NONE: printf("none"); break;
        case VAL_BOOL: printf("%s", value->boolean ? "true" : "false"); break;
        case VAL_TABLE: 
            print_table_recursive(value->table, 0);
            break;
        case VAL_FUNCTION: printf("<function>"); break;
    }
}

// creates a new hash table with separate array part for integer keys
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

// destroys a table and all its entries
void table_destroy(Table* table) {
    if (!table) return;
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            TableEntry* next = entry->next;
            value_decref(&entry->key);
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

// grows the array part to accommodate a needed index
static void array_part_grow(Table* table, int needed_index) {
    if (table->array_part == NULL) {
        table->array_capacity = TABLE_ARRAY_INIT;
        while (table->array_capacity <= needed_index) {
            table->array_capacity *= 2;
        }
        table->array_part = (Value*)malloc(table->array_capacity * sizeof(Value));
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
    
    Value* new_array = (Value*)malloc(new_capacity * sizeof(Value));
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

// sets a value by integer index in the array part
bool table_set_int(Table* table, int index, Value value) {
    if (index < 0) return false;
    
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

// gets a value by integer index from the array part
bool table_get_int(Table* table, int index, Value* out_value) {
    if (!table || index < 0 || table->array_part == NULL || index >= table->array_count) 
        return false;
    
    if (out_value) {
        *out_value = table->array_part[index];
        value_incref(out_value);
    }
    return true;
}

// appends a value to the end of the array part
void table_append(Table* table, Value value) {
    table_set_int(table, table->array_count, value);
}

// sets a string-keyed value, with auto-resizing and duplicate detection
bool table_set(Table* table, Value key, Value value) {
    if (key.type == VAL_NUMBER) {
        double num = key.number;
        if (num >= 1 && num == (int)num) {
            int idx = (int)num - 1;
            if (table->array_part == NULL) {
                if (idx < TABLE_ARRAY_INIT * 2) return table_set_int(table, idx, value);
            } else if (idx < table->array_capacity) {
                return table_set_int(table, idx, value);
            } else {
                int gap = idx - table->array_count;
                if (gap <= table->array_count * 2 && gap <= 1024) return table_set_int(table, idx, value);
            }
        }
    }

    if ((double)(table->hash_count + 1) / table->capacity > TABLE_MAX_LOAD) {
        int old_capacity = table->capacity;
        TableEntry** old_entries = table->entries;
        table->capacity = old_capacity * 2;
        table->entries = (TableEntry**)calloc(table->capacity, sizeof(TableEntry*));
        table->hash_count = 0;
        for (int i = 0; i < old_capacity; i++) {
            TableEntry* entry = old_entries[i];
            while (entry) {
                TableEntry* next = entry->next;
                uint32_t idx = entry->hash % table->capacity;
                entry->next = table->entries[idx];
                table->entries[idx] = entry;
                table->hash_count++;
                entry = next;
            }
        }
        free(old_entries);
    }

    uint32_t hash = hash_value_key(key);
    uint32_t index = hash % table->capacity;

    TableEntry* entry = table->entries[index];
    while (entry) {
        if (entry->hash == hash && key_equal(entry->key, key)) {
            value_decref(&entry->value);
            entry->value = value;
            value_incref(&entry->value);
            return true;
        }
        entry = entry->next;
    }

    entry = (TableEntry*)malloc(sizeof(TableEntry));
    entry->key = key;
    value_incref(&entry->key);
    entry->hash = hash;
    entry->value = value;
    value_incref(&entry->value);
    entry->next = table->entries[index];
    table->entries[index] = entry;
    table->hash_count++;
    return true;
}

// retrieves a string-keyed value from the table
bool table_get(Table* table, Value key, Value* out_value) {
    if (!table) return false;
    
    if (key.type == VAL_NUMBER) {
        double num = key.number;
        if (num >= 1 && num == (int)num) {
            int idx = (int)num - 1;
            if (table->array_part != NULL && idx < table->array_count) {
                if (table->array_part[idx].type != VAL_BOOL || table->array_part[idx].boolean) {
                    if (out_value) {
                        *out_value = table->array_part[idx];
                        value_incref(out_value);
                    }
                    return true;
                }
            }
        }
    }

    uint32_t hash = hash_value_key(key);
    uint32_t index = hash % table->capacity;

    TableEntry* entry = table->entries[index];
    while (entry) {
        if (entry->hash == hash && key_equal(entry->key, key)) {
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

// checks if a key exists in the table
bool table_has(Table* table, Value key) {
    return table_get(table, key, NULL);
}

// removes a key-value pair from the table
void table_remove(Table* table, Value key) {
    if (!table) return;
    
    if (key.type == VAL_NUMBER) {
        double num = key.number;
        if (num >= 1 && num == (int)num && table->array_part != NULL) {
            int idx = (int)num - 1;
            if (idx < table->array_count) {
                if (table->array_part[idx].type != VAL_BOOL || table->array_part[idx].boolean) {
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
        }
    }

    uint32_t hash = hash_value_key(key);
    uint32_t index = hash % table->capacity;

    TableEntry* entry = table->entries[index];
    TableEntry* prev = NULL;
    while (entry) {
        if (entry->hash == hash && key_equal(entry->key, key)) {
            if (prev) prev->next = entry->next;
            else table->entries[index] = entry->next;
            value_decref(&entry->key);
            value_decref(&entry->value);
            free(entry);
            table->hash_count--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

// returns the total number of entries in the table
int table_size(Table* table) {
    if (!table) return 0;
    int count = table->hash_count;
    for (int i = 0; i < table->array_count; i++) {
        if (table->array_part[i].type != VAL_BOOL || table->array_part[i].boolean) count++;
    }
    return count;
}

// returns an array of all keys in the table
Value* table_keys(Table* table, int* out_count) {
    if (!table || !out_count) return NULL;
    int total = 0;
    for (int i = 0; i < table->array_count; i++) {
        if (table->array_part[i].type != VAL_BOOL || table->array_part[i].boolean) total++;
    }
    total += table->hash_count;
    if (total == 0) { *out_count = 0; return NULL; }

    Value* keys = (Value*)malloc(sizeof(Value) * total);
    if (!keys) return NULL;
    int idx = 0;
    
    for (int i = 0; i < table->array_count; i++) {
        if (table->array_part[i].type != VAL_BOOL || table->array_part[i].boolean) {
            keys[idx].type = VAL_NUMBER;
            keys[idx].number = i + 1;
            idx++;
        }
    }
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            keys[idx] = entry->key;
            value_incref(&keys[idx]);
            idx++;
            entry = entry->next;
        }
    }
    *out_count = idx;
    return keys;
}

// clears all entries from the table
void table_clear(Table* table) {
    if (!table) return;
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            TableEntry* next = entry->next;
            value_decref(&entry->key);
            value_decref(&entry->value);
            free(entry);
            entry = next;
        }
        table->entries[i] = NULL;
    }
    table->hash_count = 0;
    if (table->array_part) {
        for (int i = 0; i < table->array_count; i++) value_decref(&table->array_part[i]);
        free(table->array_part);
        table->array_part = NULL;
        table->array_capacity = 0;
        table->array_count = 0;
    }
}

// creates a shallow copy of the table
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
    for (int i = 0; i < table->array_count; i++) {
        if (table->array_part[i].type != VAL_BOOL || table->array_part[i].boolean) {
            Value key;
            key.type = VAL_NUMBER;
            key.number = i + 1;
            table_set(copy, key, table->array_part[i]);
        }
    }
    return copy;
}

// creates a new VM instance with register frames and intern table
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

// destroys a VM and frees all resources
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

// dispatches built-in function calls to module-specific handlers
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
                        *result = vm_make_none();
                    } else {
                        *result = vm_make_number(val);
                    }
                    break;
                }
                case VAL_NUMBER:
                    *result = vm_copy_value(args[0]);
                    break;
                case VAL_BOOL:
                    *result = vm_make_none();
                    break;
                default:
                    *result = vm_make_none();
                    break;
            }
        } else {
            *result = vm_make_none();
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
                case VAL_NONE:
                    *result = vm_make_string("none");
                    break;
                case VAL_BOOL:
                    *result = vm_make_string(args[0].boolean ? "true" : "false");
                    break;
                case VAL_STRING:
                    *result = vm_copy_value(args[0]);
                    break;
                case VAL_TABLE:
                    *result = vm_make_string(table_to_string(args[0].table));
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
            *result = vm_make_none();
        }
        return true;
    }
    return false;
}

// main execution loop with direct threaded dispatch for performance
bool vm_execute(VM* vm, BytecodeChunk* chunk) {
    if (!vm || !chunk) return false;

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
        [OP_TO_STRING]        = &&OP_TO_STRING_LABEL,
        [OP_JUMP]             = &&OP_JUMP_LABEL,
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
        [OP_LOAD_BOOL]        = &&OP_LOAD_BOOL_LABEL,
        [OP_LOAD_CONST_NUM]   = &&OP_LOAD_CONST_NUM_LABEL,
    };

    register Instruction* ip = vm->code;
    register Value* regs = vm->registers;
    __builtin_prefetch(ip + 1, 0, 1);
    goto *dispatch_table[ip->opcode];

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
            case CONST_NONE:
                regs[dest].type = VAL_NONE;
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
        if (regs[ip->operands[1]].type == VAL_NUMBER && regs[ip->operands[2]].type == VAL_NUMBER) {
            regs[dest].type = VAL_NUMBER;
            regs[dest].number = regs[ip->operands[1]].number + regs[ip->operands[2]].number;
        } else {
            value_decref(&regs[dest]);
            regs[dest].type = VAL_NONE;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_SUB_LABEL: {
        int dest = ip->operands[0];
        if (regs[ip->operands[1]].type == VAL_NUMBER && regs[ip->operands[2]].type == VAL_NUMBER) {
            regs[dest].type = VAL_NUMBER;
            regs[dest].number = regs[ip->operands[1]].number - regs[ip->operands[2]].number;
        } else {
            value_decref(&regs[dest]);
            regs[dest].type = VAL_NONE;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MUL_LABEL: {
        int dest = ip->operands[0];
        if (regs[ip->operands[1]].type == VAL_NUMBER && regs[ip->operands[2]].type == VAL_NUMBER) {
            regs[dest].type = VAL_NUMBER;
            regs[dest].number = regs[ip->operands[1]].number * regs[ip->operands[2]].number;
        } else {
            value_decref(&regs[dest]);
            regs[dest].type = VAL_NONE;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_DIV_LABEL: {
        int dest = ip->operands[0];
        if (regs[ip->operands[1]].type == VAL_NUMBER && regs[ip->operands[2]].type == VAL_NUMBER) {
            regs[dest].type = VAL_NUMBER;
            regs[dest].number = regs[ip->operands[1]].number / regs[ip->operands[2]].number;
        } else {
            value_decref(&regs[dest]);
            regs[dest].type = VAL_NONE;
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MOD_LABEL: {
        int dest = ip->operands[0];
        if (regs[ip->operands[1]].type == VAL_NUMBER && regs[ip->operands[2]].type == VAL_NUMBER) {
            regs[dest].type = VAL_NUMBER;
            regs[dest].number = fmod(regs[ip->operands[1]].number, regs[ip->operands[2]].number);
        } else {
            value_decref(&regs[dest]);
            regs[dest].type = VAL_NONE;
        }
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
        
        if (left->type == VAL_NONE && right->type == VAL_NONE) {
            result = 1;
        }
        else if (left->type == VAL_NONE || right->type == VAL_NONE) {
            result = 0;
        }
        else {
            switch (left->type) {
                case VAL_NUMBER: result = (left->number == right->number); break;
                case VAL_STRING: result = string_equal(left->string, right->string); break;
                case VAL_BOOL:   result = (left->boolean == right->boolean); break;
                default: break;
            }
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
        
        if (left->type == VAL_NONE && right->type == VAL_NONE) {
            result = 0;
        }
        else if (left->type == VAL_NONE || right->type == VAL_NONE) {
            result = 1;
        }
        else {
            switch (left->type) {
                case VAL_NUMBER: result = (left->number != right->number); break;
                case VAL_STRING: result = !string_equal(left->string, right->string); break;
                case VAL_BOOL:   result = (left->boolean != right->boolean); break;
                default: break;
            }
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
    OP_TO_STRING_LABEL: {
        int dest = ip->operands[0]; 
        Value* src = &regs[ip->operands[1]];
        char buffer[256];
        value_decref(&regs[dest]);
        switch (src->type) {
            case VAL_NUMBER: {
                double num = src->number;
                
                if (num == (int)num && num >= 0 && num <= 100) {
                    static StringObject* num_cache[101] = {NULL};
                    int idx = (int)num;
                    if (num_cache[idx] == NULL) {
                        char buf[4];
                        int len = snprintf(buf, sizeof(buf), "%d", idx);
                        num_cache[idx] = string_create_pooled(&vm->obj_pool, buf, len);
                        num_cache[idx]->header.ref_count = 999999;
                    }
                    regs[dest].type = VAL_STRING;
                    regs[dest].string = num_cache[idx];
                    value_incref(&regs[dest]);
                    break;
                }
                
                int len;
                if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) {
                    len = snprintf(buffer, sizeof(buffer), "%lld", (long long)num);
                } else {
                    len = snprintf(buffer, sizeof(buffer), "%.15g", num);
                }
                if (len >= 8 && len <= 32 && vm->intern_table.count < 50000) {
                    regs[dest].type = VAL_STRING;
                    regs[dest].string = string_intern(&vm->intern_table, buffer, len);
                    value_incref(&regs[dest]);
                } else {
                    regs[dest] = vm_make_string(buffer);
                }
                break;
            }
            case VAL_NONE:
                regs[dest] = vm_make_string("none");
                break;
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
    OP_JUMP_LABEL: ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode];
    OP_JUMP_IF_FALSE_LABEL: {
        int cond_reg = ip->operands[1];
        if (!vm->registers[cond_reg].boolean) { ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CALL_LABEL: {
        if (vm->call_depth >= VM_MAX_CALL_FRAMES) {
            fprintf(stderr, "Stack overflow - maximum call depth (%d) exceeded. "
                    "Too many nested function calls or infinite recursion detected.\n", 
                    VM_MAX_CALL_FRAMES);
            vm->had_error = true;
            vm->running = false;
            return false;
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
            vm->registers[dest_reg] = vm_make_none();
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
            regs[dest_reg].type = VAL_NONE;
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
        Value key = vm->registers[key_reg];
        Value val = vm->registers[val_reg];
        table_set(table, key, val);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_SET_CONST_LABEL: {
        int table_reg = ip->operands[0];
        int key_idx = ip->operands[1];
        int val_reg = ip->operands[2];
        Table* table = vm->registers[table_reg].table;
        Value key;
        key.type = VAL_STRING;
        key.string = string_intern(&vm->intern_table, chunk->constants[key_idx].string_value, strlen(chunk->constants[key_idx].string_value));
        table_set(table, key, vm->registers[val_reg]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_LABEL: {
        int dest = ip->operands[0];
        int table_reg = ip->operands[1];
        int key_reg = ip->operands[2];
        Value* table_val = &vm->registers[table_reg];
        
        if (table_val->type != VAL_TABLE) {
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_NONE;
            ip++; goto *dispatch_table[ip->opcode];
        }
        
        Table* table = table_val->table;
        Value key = vm->registers[key_reg];
        Value val;
        val.type = VAL_NONE;
        table_get(table, key, &val);
        
        value_decref(&vm->registers[dest]);
        vm->registers[dest] = val;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_CONST_LABEL: {
        int dest = ip->operands[0];
        int table_reg = ip->operands[1];
        int key_idx = ip->operands[2];
        Value* table_val = &vm->registers[table_reg];
        
        if (table_val->type != VAL_TABLE) {
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_NONE;
            ip++; goto *dispatch_table[ip->opcode];
        }
        
        Table* table = table_val->table;
        Value key;
        key.type = VAL_STRING;
        key.string = string_intern(&vm->intern_table, chunk->constants[key_idx].string_value, strlen(chunk->constants[key_idx].string_value));
        
        Value val;
        val.type = VAL_NONE;
        table_get(table, key, &val);
        
        value_decref(&vm->registers[dest]);
        vm->registers[dest] = val;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_APPEND_LABEL: {
        int table_reg = ip->operands[0];
        int val_reg = ip->operands[1];
        
        Table* table = vm->registers[table_reg].table;
        Value* val = &vm->registers[val_reg];
        
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
        
        if (total_len >= 16 && total_len <= 64) {
            char combined[65];
            memcpy(combined, ls, llen);
            memcpy(combined + llen, rs, rlen);
            combined[total_len] = '\0';
            
            StringObject* interned = string_intern(&vm->intern_table, combined, total_len);
            value_decref(&vm->registers[dest]);
            vm->registers[dest].type = VAL_STRING;
            vm->registers[dest].string = interned;
            value_incref(&vm->registers[dest]);
        } else {
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
            int exit_addr = flag_or_exit;
            
            double index = vm->registers[var_reg].number;
            double size = vm->registers[end_or_size_reg].number;
            
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
        
        if (left->type == VAL_NONE && right->type == VAL_NONE) {
            jump = true;
        } else if (left->type == VAL_NONE || right->type == VAL_NONE) {
            jump = false;
        } else {
            switch (left->type) {
                case VAL_NUMBER: jump = (left->number == right->number); break;
                case VAL_STRING: jump = string_equal(left->string, right->string); break;
                case VAL_BOOL:   jump = (left->boolean == right->boolean); break;
                default: break;
            }
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
        bool jump = false;
        
        if (left->type == VAL_NONE && right->type == VAL_NONE) {
            jump = false;
        } else if (left->type == VAL_NONE || right->type == VAL_NONE) {
            jump = true;
        } else {
            switch (left->type) {
                case VAL_NUMBER: jump = (left->number != right->number); break;
                case VAL_STRING: jump = !string_equal(left->string, right->string); break;
                case VAL_BOOL:   jump = (left->boolean != right->boolean); break;
                default: break;
            }
        }
        if (jump) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_PUSH_ARG_LABEL: {
        if (vm->args_top >= VM_MAX_ARGS_STACK) {
            fprintf(stderr, "Argument stack overflow - maximum %d arguments exceeded. "
                    "Too many function arguments being passed.\n",
                    VM_MAX_ARGS_STACK);
            vm->had_error = true;
            vm->running = false;
            return false;
        }
        int reg = ip->operands[0];
        Value* src = &vm->registers[reg];
        vm->args_stack[vm->args_top] = *src;
        value_incref(&vm->args_stack[vm->args_top]);
        vm->args_top++;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_HALT_LABEL: vm->running = false; return !vm->had_error;
    OP_LOAD_BOOL_LABEL: {
        int dest = ip->operands[0];
        regs[dest].type = VAL_BOOL; regs[dest].boolean = ip->operands[1] != 0;
        ip++; goto *dispatch_table[ip->opcode];
    }
    return !vm->had_error;
}
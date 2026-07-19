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
#include <limits.h>

typedef union { uint64_t u; double d; } du64;

StringObject* string_create(const char* chars, int length);
static void string_destroy(StringObject* str);
static bool string_equal(StringObject* a, StringObject* b);
static const char* value_to_cstr(Value v, char* buf, int buf_size);

// returns the type of a nan-boxed value
ValueType_VM value_get_type(Value v) {
    return (ValueType_VM)GET_TYPE(v);
}

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
    (void)pool;
    return string_create(chars, length);
}

// returns a string to the pool for reuse if it's small enough
void string_destroy_pooled(ObjectPool* pool, StringObject* str) {
    (void)pool;
    if (!str) return;
    string_destroy(str);
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
            table->array_part[i] = MAKE_BOOL(false);
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
        
        table->capacity = 8;
        table->entries = NULL;
        table->array_part = NULL;
        table->array_capacity = 0;
        table->array_count = 0;
        table->hash_count = 0;
        
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
        if (it->buckets[i]) {
            free(it->buckets[i]);
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
        if (str) {
            unsigned int idx = intern_hash(str->chars, str->length) % new_capacity;
            while (it->buckets[idx] != NULL) {
                idx = (idx + 1) % new_capacity;
            }
            it->buckets[idx] = str;
            it->count++;
        }
    }
    free(old_buckets);
}

// interns a string, returning a canonical object with linear probing
StringObject* string_intern(StringInternTable* it, const char* chars, int length) {
    if (!it || !chars) return NULL;
    
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
            new_str->header.ref_count = INT_MAX;
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
StringObject* string_create(const char* chars, int length) {
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
static const char* value_to_cstr(Value v, char* buf, int buf_size) {
    if (IS_NUMBER(v)) {
        snprintf(buf, buf_size, "%.15g", AS_NUMBER(v));
        return buf;
    } else if (IS_STRING(v)) {
        return AS_STRING(v)->chars;
    } else if (IS_NONE(v)) {
        return "none";
    } else if (IS_BOOL(v)) {
        return AS_BOOL(v) ? "true" : "false";
    } else {
        return "";
    }
}

// increments the reference count of a reference-counted value
void value_incref(Value v) {
    if (IS_STRING(v)) {
        StringObject* str = AS_STRING(v);
        if (str && str->header.ref_count != INT_MAX) {
            str->header.ref_count++;
        }
    } else if (IS_TABLE(v)) {
        Table* table = AS_TABLE(v);
        if (table) table->header.ref_count++;
    }
}

// decrements the reference count and frees the object when it reaches zero
void value_decref(Value v) {
    if (IS_STRING(v)) {
        StringObject* str = AS_STRING(v);
        if (str) {
            if (str->header.ref_count == INT_MAX) {
                return;
            }
            if (--str->header.ref_count == 0) {
                string_destroy(str);
            }
        }
    } else if (IS_TABLE(v)) {
        Table* table = AS_TABLE(v);
        if (table && --table->header.ref_count == 0) {
            table_destroy(table);
        }
    }
}

// constructs a numeric value (unboxed double, nan-boxed if nan)
Value vm_make_number(double value) {
    return MAKE_NUMBER(value);
}

// constructs a string value (pointer stored in nan box)
Value vm_make_string(const char* value) {
    StringObject* str = string_create(value, (int)strlen(value));
    return MAKE_STRING(str);
}

// constructs a none/null value (special nan tag)
Value vm_make_none(void) {
    return MAKE_NONE();
}

// constructs a boolean value (special nan tag with boolean payload)
Value vm_make_bool(bool value) {
    return MAKE_BOOL(value);
}

// constructs a new empty table value (pointer in nan box)
Value vm_make_table(void) {
    Table* table = table_create(8);
    return MAKE_TABLE(table);
}

// copies a value with proper reference counting
Value vm_copy_value(Value value) {
    value_incref(value);
    return value;
}

// frees a value and decrements its reference count
void vm_free_value(Value value) {
    value_decref(value);
}

// returns a type name string for a value
const char* vm_value_type_name(Value value) {
    if (IS_NUMBER(value) || IS_NAN(value)) return "number";
    if (IS_STRING(value)) return "string";
    if (IS_NONE(value)) return "none";
    if (IS_BOOL(value)) return "bool";
    if (IS_TABLE(value)) return "table";
    if (IS_FUNCTION(value)) return "function";
    return "unknown";
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
    if (IS_STRING(key)) {
        StringObject* str = AS_STRING(key);
        if (!str->hash_computed) {
            uint32_t h = 5381;
            for (int i = 0; i < str->length; i++) {
                h = ((h << 5) + h) + (uint8_t)str->chars[i];
            }
            str->hash = h;
            str->hash_computed = true;
        }
        return str->hash;
    } else if (IS_NUMBER(key)) {
        double num = AS_NUMBER(key);
        if (num == 0.0) num = 0.0;
        union { double d; uint64_t u; } u;
        u.d = num;
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
    if (IS_NAN(a) && IS_NAN(b)) return false;
    if (IS_NAN(a) || IS_NAN(b)) return false;
    
    if (IS_STRING(a) && IS_STRING(b)) {
        StringObject* sa = AS_STRING(a);
        StringObject* sb = AS_STRING(b);
        if (sa == sb) return true;
        if (sa->length != sb->length) return false;
        return memcmp(sa->chars, sb->chars, sa->length) == 0;
    }
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    if (a == b) return true;
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
            
            if (IS_STRING(key)) {
                sb_append(sb, "\"", 1);
                sb_append(sb, AS_STRING(key)->chars, AS_STRING(key)->length);
                sb_append(sb, "\"", 1);
            } else if (IS_NUMBER(key)) {
                char num_buf[64];
                snprintf(num_buf, sizeof(num_buf), "%g", AS_NUMBER(key));
                sb_append(sb, num_buf, strlen(num_buf));
            }
            
            sb_append(sb, " = ", 3);
            
            char num_buf[64];
            if (IS_NUMBER(val)) {
                double num = AS_NUMBER(val);
                if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9)
                    snprintf(num_buf, sizeof(num_buf), "%.0f", num);
                else
                    snprintf(num_buf, sizeof(num_buf), "%.15g", num);
                sb_append(sb, num_buf, strlen(num_buf));
            } else if (IS_STRING(val)) {
                sb_append(sb, "\"", 1);
                sb_append(sb, AS_STRING(val)->chars, AS_STRING(val)->length);
                sb_append(sb, "\"", 1);
            } else if (IS_BOOL(val)) {
                sb_append(sb, AS_BOOL(val) ? "true" : "false", AS_BOOL(val) ? 4 : 5);
            } else if (IS_TABLE(val)) {
                table_to_string_builder(AS_TABLE(val), sb, indent_level + 1);
            } else if (IS_NONE(val)) {
                sb_append(sb, "none", 4);
            } else {
                sb_append(sb, "unknown", 7);
            }

            if (i < key_count - 1) sb_append(sb, ",", 1);
            sb_append(sb, "\n", 1);
            value_decref(val);
        }
        value_decref(key);
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
void vm_print_value(Value value) {
    if (IS_NAN(value)) {
        printf("nan");
    } else if (IS_NUMBER(value)) {
        double num = AS_NUMBER(value);
        if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9) printf("%.0f", num);
        else printf("%.15g", num);
    } else if (IS_STRING(value)) {
        printf("%s", AS_STRING(value)->chars);
    } else if (IS_NONE(value)) {
        printf("none");
    } else if (IS_BOOL(value)) {
        printf("%s", AS_BOOL(value) ? "true" : "false");
    } else if (IS_TABLE(value)) {
        print_table_recursive(AS_TABLE(value), 0);
    } else if (IS_FUNCTION(value)) {
        printf("<function>");
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
            value_decref(entry->key);
            value_decref(entry->value);
            free(entry);
            entry = next;
        }
    }
    free(table->entries);
    if (table->array_part) {
        for (int i = 0; i < table->array_count; i++) {
            value_decref(table->array_part[i]);
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
            table->array_part[i] = MAKE_BOOL(false);
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
        new_array[i] = MAKE_BOOL(false);
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
    
    value_decref(table->array_part[index]);
    table->array_part[index] = value;
    value_incref(table->array_part[index]);
    
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
        value_incref(*out_value);
    }
    return true;
}

// appends a value to the end of the array part
void table_append(Table* table, Value value) {
    table_set_int(table, table->array_count, value);
}

// sets a string-keyed value, with auto-resizing and duplicate detection
bool table_set(Table* table, Value key, Value value) {
    if (IS_NUMBER(key)) {
        double num = AS_NUMBER(key);
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
            value_decref(entry->value);
            entry->value = value;
            value_incref(entry->value);
            return true;
        }
        entry = entry->next;
    }

    entry = (TableEntry*)malloc(sizeof(TableEntry));
    entry->key = key;
    value_incref(entry->key);
    entry->hash = hash;
    entry->value = value;
    value_incref(entry->value);
    entry->next = table->entries[index];
    table->entries[index] = entry;
    table->hash_count++;
    return true;
}

// retrieves a string-keyed value from the table
bool table_get(Table* table, Value key, Value* out_value) {
    if (!table) return false;
    
    if (IS_NUMBER(key)) {
        double num = AS_NUMBER(key);
        if (num >= 1 && num == (int)num) {
            int idx = (int)num - 1;
            if (table->array_part != NULL && idx < table->array_count) {
                if (!IS_BOOL(table->array_part[idx]) || AS_BOOL(table->array_part[idx])) {
                    if (out_value) {
                        *out_value = table->array_part[idx];
                        value_incref(*out_value);
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
                value_incref(*out_value);
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
    
    if (IS_NUMBER(key)) {
        double num = AS_NUMBER(key);
        if (num >= 1 && num == (int)num && table->array_part != NULL) {
            int idx = (int)num - 1;
            if (idx < table->array_count) {
                if (!IS_BOOL(table->array_part[idx]) || AS_BOOL(table->array_part[idx])) {
                    value_decref(table->array_part[idx]);
                    table->array_part[idx] = MAKE_BOOL(false);
                    while (table->array_count > 0 &&
                           IS_BOOL(table->array_part[table->array_count - 1]) &&
                           !AS_BOOL(table->array_part[table->array_count - 1])) {
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
            value_decref(entry->key);
            value_decref(entry->value);
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
        if (!IS_BOOL(table->array_part[i]) || AS_BOOL(table->array_part[i])) count++;
    }
    return count;
}

// returns an array of all keys in the table
Value* table_keys(Table* table, int* out_count) {
    if (!table || !out_count) return NULL;
    int total = 0;
    for (int i = 0; i < table->array_count; i++) {
        if (!IS_BOOL(table->array_part[i]) || AS_BOOL(table->array_part[i])) total++;
    }
    total += table->hash_count;
    if (total == 0) { *out_count = 0; return NULL; }

    Value* keys = (Value*)malloc(sizeof(Value) * total);
    if (!keys) return NULL;
    int idx = 0;
    
    for (int i = 0; i < table->array_count; i++) {
        if (!IS_BOOL(table->array_part[i]) || AS_BOOL(table->array_part[i])) {
            keys[idx] = MAKE_NUMBER(i + 1);
            idx++;
        }
    }
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = table->entries[i];
        while (entry) {
            keys[idx] = entry->key;
            value_incref(keys[idx]);
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
            value_decref(entry->key);
            value_decref(entry->value);
            free(entry);
            entry = next;
        }
        table->entries[i] = NULL;
    }
    table->hash_count = 0;
    if (table->array_part) {
        for (int i = 0; i < table->array_count; i++) value_decref(table->array_part[i]);
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
        if (!IS_BOOL(table->array_part[i]) || AS_BOOL(table->array_part[i])) {
            table_set(copy, MAKE_NUMBER(i + 1), table->array_part[i]);
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
    vm->table_iter_depth = -1;
    vm->running = false;
    vm->had_error = false;
    vm->current_frame = 0;
    vm->args_top = 0;
    vm->source = source;

    for (int i = 0; i < VM_MAX_FRAMES * VM_REGS_PER_FRAME; i++) {
        vm->register_frames[i] = MAKE_NONE();
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
            value_decref(vm->register_frames[f * VM_REGS_PER_FRAME + i]);
        }
    }

    for (int i = 0; i < vm->global_count; i++) {
        value_decref(vm->globals[i]);
    }
    for (int i = 0; i < vm->args_top; i++) {
        value_decref(vm->args_stack[i]);
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
            if (IS_STRING(args[0])) {
                char* endptr;
                double val = strtod(AS_STRING(args[0])->chars, &endptr);
                
                if (endptr == AS_STRING(args[0])->chars || *endptr != '\0') {
                    *result = MAKE_NONE();
                } else {
                    *result = MAKE_NUMBER(val);
                }
            } else if (IS_NUMBER(args[0])) {
                *result = args[0];
                value_incref(*result);
            } else {
                *result = MAKE_NONE();
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }
    if (strcmp(name, "string") == 0) {
        if (arg_count >= 1) {
            char buffer[256];
            if (IS_NUMBER(args[0])) {
                snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(args[0]));
                *result = MAKE_STRING(string_intern(&vm->intern_table, buffer, strlen(buffer)));
            } else if (IS_NONE(args[0])) {
                *result = MAKE_STRING(string_intern(&vm->intern_table, "none", 4));
            } else if (IS_BOOL(args[0])) {
                *result = MAKE_STRING(string_intern(&vm->intern_table, AS_BOOL(args[0]) ? "true" : "false", AS_BOOL(args[0]) ? 4 : 5));
            } else if (IS_STRING(args[0])) {
                *result = args[0];
                value_incref(*result);
            } else if (IS_TABLE(args[0])) {
                char* table_str = table_to_string(AS_TABLE(args[0]));
                *result = MAKE_STRING(string_intern(&vm->intern_table, table_str, strlen(table_str)));
                free(table_str);
            } else {
                *result = MAKE_NONE();
            }
        }
        return true;
    }
    if (strcmp(name, "type") == 0) {
        if (arg_count >= 1) {
            *result = MAKE_STRING(string_intern(&vm->intern_table, vm_value_type_name(args[0]), strlen(vm_value_type_name(args[0]))));
        } else {
            *result = MAKE_NONE();
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
    for (int i = 0; i < chunk->global_count; i++) vm->globals[i] = MAKE_BOOL(false);
    static void* dispatch_table[] = {
        [OP_MOVE]             = &&OP_MOVE_LABEL,
        [OP_LOAD_CONST]       = &&OP_LOAD_CONST_LABEL,
        [OP_LOAD_CONST_NUM]   = &&OP_LOAD_CONST_NUM_LABEL,
        [OP_LOAD_BOOL]        = &&OP_LOAD_BOOL_LABEL,
        
        [OP_ADD]              = &&OP_ADD_LABEL,
        [OP_SUB]              = &&OP_SUB_LABEL,
        [OP_MUL]              = &&OP_MUL_LABEL,
        [OP_DIV]              = &&OP_DIV_LABEL,
        [OP_MOD]              = &&OP_MOD_LABEL,
        [OP_NEG]              = &&OP_NEG_LABEL,
        
        [OP_JUMP]             = &&OP_JUMP_LABEL,
        [OP_JUMP_IF_FALSE]    = &&OP_JUMP_IF_FALSE_LABEL,
        [OP_JUMP_IF_EQ]       = &&OP_JUMP_IF_EQ_LABEL,
        [OP_JUMP_IF_NEQ]      = &&OP_JUMP_IF_NEQ_LABEL,
        [OP_JUMP_IF_LT]       = &&OP_JUMP_IF_LT_LABEL,
        [OP_JUMP_IF_GT]       = &&OP_JUMP_IF_GT_LABEL,
        [OP_JUMP_IF_LTE]      = &&OP_JUMP_IF_LTE_LABEL,
        [OP_JUMP_IF_GTE]      = &&OP_JUMP_IF_GTE_LABEL,
        
        [OP_CMP_EQ]           = &&OP_CMP_EQ_LABEL,
        [OP_CMP_NEQ]          = &&OP_CMP_NEQ_LABEL,
        [OP_CMP_LT]           = &&OP_CMP_LT_LABEL,
        [OP_CMP_GT]           = &&OP_CMP_GT_LABEL,
        [OP_CMP_LTE]          = &&OP_CMP_LTE_LABEL,
        [OP_CMP_GTE]          = &&OP_CMP_GTE_LABEL,
        
        [OP_FOR_INIT]         = &&OP_FOR_INIT_LABEL,
        [OP_FOR_NEXT]         = &&OP_FOR_NEXT_LABEL,
        [OP_TABLE_ITER_INIT] = &&OP_TABLE_ITER_INIT_LABEL,
        [OP_TABLE_ITER_NEXT] = &&OP_TABLE_ITER_NEXT_LABEL,
        [OP_POP_ITER]         = &&OP_POP_ITER_LABEL,

        [OP_TABLE_GET]        = &&OP_TABLE_GET_LABEL,
        [OP_TABLE_GET_CONST]  = &&OP_TABLE_GET_CONST_LABEL,
        [OP_TABLE_SET]        = &&OP_TABLE_SET_LABEL,
        [OP_TABLE_SET_CONST]  = &&OP_TABLE_SET_CONST_LABEL,
        [OP_TABLE_APPEND]     = &&OP_TABLE_APPEND_LABEL,
        [OP_NEW_TABLE]        = &&OP_NEW_TABLE_LABEL,
        
        [OP_CONCAT]           = &&OP_CONCAT_LABEL,

        [OP_AND]              = &&OP_AND_LABEL,
        [OP_OR]               = &&OP_OR_LABEL,
        [OP_NOT]              = &&OP_NOT_LABEL,
        
        [OP_PUSH_ARG]         = &&OP_PUSH_ARG_LABEL,
        [OP_CALL]             = &&OP_CALL_LABEL,
        [OP_CALL_BUILTIN]     = &&OP_CALL_BUILTIN_LABEL,
        [OP_RETURN]           = &&OP_RETURN_LABEL,
        [OP_RETURN_VOID]      = &&OP_RETURN_VOID_LABEL,
        
        [OP_CALL_0] = &&OP_CALL_0_LABEL,
        [OP_CALL_1] = &&OP_CALL_1_LABEL,
        [OP_CALL_2] = &&OP_CALL_2_LABEL,
        [OP_RETURN_NUM] = &&OP_RETURN_NUM_LABEL,

        [OP_LOAD_GLOBAL]      = &&OP_LOAD_GLOBAL_LABEL,
        [OP_STORE_GLOBAL]     = &&OP_STORE_GLOBAL_LABEL,
        
        [OP_HALT]             = &&OP_HALT_LABEL,
    };
    register Instruction* ip = vm->code;
    register Value* regs = vm->registers;
    __builtin_prefetch(ip + 1, 0, 1);
    goto *dispatch_table[ip->opcode];

    OP_MOVE_LABEL: {
        int dest = ip->operands[0]; int src = ip->operands[1];  // dest and src register indices
        Value sv = regs[src];          // read source value
        if (IS_NUMBER(sv) || IS_BOOL(sv) || IS_NONE(sv)) {
            regs[dest] = sv;           // immediate types, no refcount needed
        } else {
            value_decref(regs[dest]);  // release old dest value
            regs[dest] = sv;           // copy pointer
            value_incref(regs[dest]);  // bump refcount for new reference
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_LOAD_CONST_LABEL: {
        int dest = ip->operands[0]; int const_idx = ip->operands[1];  // dest reg and constant pool index
        Constant* c = &chunk->constants[const_idx];  // fetch constant from pool
        value_decref(regs[dest]);                    // release old value in dest reg
        switch (c->type) {
            case CONST_NUMBER: 
                regs[dest] = MAKE_NUMBER(c->number_value);  // unboxed number, no refcount
                break;
            case CONST_STRING: 
                {
                    int len = (int)strlen(c->string_value);  // compute string length
                    StringObject* str = string_intern(&vm->intern_table, c->string_value, len);  // intern for dedup
                    regs[dest] = MAKE_STRING(str);           // store tagged string pointer
                    value_incref(regs[dest]);                // bump refcount, interned strings have INT_MAX but still
                }
                break;
            case CONST_FUNCTION:
                regs[dest] = MAKE_FUNCTION(c->function_index);  // store function index as tagged value
                break;
            case CONST_NONE:
                regs[dest] = MAKE_NONE();
                break;
            case CONST_BOOL: 
                regs[dest] = MAKE_BOOL(c->bool_value);
                break;
            default: 
                break;
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_LOAD_CONST_NUM_LABEL: {
        int dest = ip->operands[0]; int value = ip->operands[1];  // dest reg and integer constant
        regs[dest] = MAKE_NUMBER((double)value);                  // cast int to double and box
        ip++; goto *dispatch_table[ip->opcode];                   // advance to next instruction
    }
    OP_LOAD_BOOL_LABEL: {
        int dest = ip->operands[0];                    // dest register index
        regs[dest] = MAKE_BOOL(ip->operands[1] != 0);  // convert operand to bool and store
        ip++; goto *dispatch_table[ip->opcode];        // advance to next instruction
    }

    OP_ADD_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double
        double r = a.d + b.d;                    // perform addition
        if (r != r) {                            // check for nan result
            if (a.d != a.d || b.d != b.d) {      // one of the operands is nan (not a number)
                value_decref(regs[dest]);        // release old value
                regs[dest] = MAKE_NONE();        // nan operands produce none
            } else {
                regs[dest] = MAKE_NUMBER(r);     // genuine nan result, store as number
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);         // normal result, store as unboxed number
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_SUB_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double
        double r = a.d - b.d;                    // perform subtraction
        if (r != r) {                            // check for nan result
            if (a.d != a.d || b.d != b.d) {      // one of the operands is nan
                value_decref(regs[dest]);        // release old value
                regs[dest] = MAKE_NONE();        // nan operands produce none
            } else {
                regs[dest] = MAKE_NUMBER(r);     // genuine nan result, store as number
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);         // normal result
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_MUL_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double
        double r = a.d * b.d;                    // perform multiplication
        if (r != r) {                            // check for nan result
            if (a.d != a.d || b.d != b.d) {      // one of the operands is nan
                value_decref(regs[dest]);        // release old value
                regs[dest] = MAKE_NONE();        // nan operands produce none
            } else {
                regs[dest] = MAKE_NUMBER(r);     // genuine nan result, store as number
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);         // normal result
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_DIV_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double
        double r = a.d / b.d;                    // perform division
        if (r != r) {                            // check for nan result
            if (a.d != a.d || b.d != b.d) {      // one of the operands is nan
                value_decref(regs[dest]);        // release old value
                regs[dest] = MAKE_NONE();        // nan operands produce none
            } else {
                regs[dest] = MAKE_NUMBER(r);     // genuine nan result, store as number
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);         // normal result
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_MOD_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double
        double r = fmod(a.d, b.d);               // perform modulo using fmod
        if (r != r) {                            // check for nan result
            if (a.d != a.d || b.d != b.d) {      // one of the operands is nan
                value_decref(regs[dest]);        // release old value
                regs[dest] = MAKE_NONE();        // nan operands produce none
            } else {
                regs[dest] = MAKE_NUMBER(r);     // genuine nan result, store as number
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);         // normal result
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_NEG_LABEL: {
        int dest = ip->operands[0];                                   // dest register index
        regs[dest] = MAKE_NUMBER(-AS_NUMBER(regs[ip->operands[1]]));  // negate and store as unboxed number
        ip++; goto *dispatch_table[ip->opcode];                       // advance to next instruction
    }

    OP_JUMP_LABEL:
        ip = &vm->code[ip->operands[0]];          // jump to target address
        goto *dispatch_table[ip->opcode];         // dispatch next instruction
    OP_JUMP_IF_FALSE_LABEL: {
        int cond_reg = ip->operands[1];           // register holding the condition
        if (!AS_BOOL(vm->registers[cond_reg])) {  // if false, take the jump
            ip = &vm->code[ip->operands[0]];      // jump to target address
            goto *dispatch_table[ip->opcode];     // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];   // fall through to next instruction
    }
    OP_JUMP_IF_EQ_LABEL: {
        int target = ip->operands[0];         // jump target address
        Value left = regs[ip->operands[1]];   // left operand
        Value right = regs[ip->operands[2]];  // right operand
        bool jump = false;     // flag to determine if jump should be taken
        if (IS_NONE(left) && IS_NONE(right)) {
            jump = true;       // both none are equal
        } else if (IS_NONE(left) || IS_NONE(right)) {
            jump = false;      // one is none, the other is not
        } else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            jump = (AS_NUMBER(left) == AS_NUMBER(right));  // compare numeric values
        } else if (IS_STRING(left) && IS_STRING(right)) {
            jump = string_equal(AS_STRING(left), AS_STRING(right));  // compare string contents
        } else if (IS_BOOL(left) && IS_BOOL(right)) {
            jump = (AS_BOOL(left) == AS_BOOL(right));      // compare boolean values
        }
        if (jump) {
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_NEQ_LABEL: {
        int target = ip->operands[0];         // jump target address
        Value left = regs[ip->operands[1]];   // left operand
        Value right = regs[ip->operands[2]];  // right operand
        bool jump = false;     // flag to determine if jump should be taken
        if (IS_NONE(left) && IS_NONE(right)) {
            jump = false;      // both none are equal, so not neq
        } else if (IS_NONE(left) || IS_NONE(right)) {
            jump = true;       // one is none, the other is not, so they are neq
        } else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            jump = (AS_NUMBER(left) != AS_NUMBER(right));  // compare numeric values
        } else if (IS_STRING(left) && IS_STRING(right)) {
            jump = !string_equal(AS_STRING(left), AS_STRING(right));  // compare string contents
        } else if (IS_BOOL(left) && IS_BOOL(right)) {
            jump = (AS_BOOL(left) != AS_BOOL(right));      // compare boolean values
        }
        if (jump) {
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_LT_LABEL: {
        int target = ip->operands[0];            // jump target address
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double
        if (a.d < b.d) {                         // compare as doubles
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_LTE_LABEL: {
        int target = ip->operands[0];            // jump target address
        if (AS_NUMBER(regs[ip->operands[1]]) <= AS_NUMBER(regs[ip->operands[2]])) {
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_GT_LABEL: {
        int target = ip->operands[0];            // jump target address
        if (AS_NUMBER(regs[ip->operands[1]]) > AS_NUMBER(regs[ip->operands[2]])) {
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_GTE_LABEL: {
        int target = ip->operands[0];            // jump target address
        if (AS_NUMBER(vm->registers[ip->operands[1]]) >= AS_NUMBER(vm->registers[ip->operands[2]])) {
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }

    OP_CMP_EQ_LABEL: {
        int dest = ip->operands[0];              // dest register index
        Value left = regs[ip->operands[1]];      // left operand
        Value right = regs[ip->operands[2]];     // right operand
        int result = 0;                          // default to false
        if (IS_NONE(left) && IS_NONE(right)) {
            result = 1;                          // both none are equal
        }
        else if (IS_NONE(left) || IS_NONE(right)) {
            result = 0;                          // one is none, the other is not
        }
        else if (IS_FUNCTION(left) && IS_FUNCTION(right)) {
            result = (AS_FUNCTION(left) == AS_FUNCTION(right));  // compare function indices
        }
        else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            result = (AS_NUMBER(left) == AS_NUMBER(right));      // compare numeric values
        }
        else if (IS_STRING(left) && IS_STRING(right)) {
            result = string_equal(AS_STRING(left), AS_STRING(right));  // compare string contents
        }
        else if (IS_BOOL(left) && IS_BOOL(right)) {
            result = (AS_BOOL(left) == AS_BOOL(right));          // compare boolean values
        }
        regs[dest] = MAKE_BOOL(result);          // store result as bool
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_NEQ_LABEL: {
        int dest = ip->operands[0];              // dest register index
        Value left = regs[ip->operands[1]];      // left operand
        Value right = regs[ip->operands[2]];     // right operand
        int result = 1;                          // default to true
        if (IS_NONE(left) && IS_NONE(right)) {
            result = 0;                          // both none are equal, so not neq
        }
        else if (IS_NONE(left) || IS_NONE(right)) {
            result = 1;                          // one is none, the other is not, so they are neq
        }
        else if (IS_FUNCTION(left) && IS_FUNCTION(right)) {
            result = (AS_FUNCTION(left) != AS_FUNCTION(right));  // compare function indices
        }
        else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            result = (AS_NUMBER(left) != AS_NUMBER(right));      // compare numeric values
        }
        else if (IS_STRING(left) && IS_STRING(right)) {
            result = !string_equal(AS_STRING(left), AS_STRING(right));  // compare string contents
        }
        else if (IS_BOOL(left) && IS_BOOL(right)) {
            result = (AS_BOOL(left) != AS_BOOL(right));          // compare boolean values
        }
        regs[dest] = MAKE_BOOL(result);          // store result as bool
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_LT_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double
        regs[dest] = MAKE_BOOL(a.d < b.d);       // compare and store bool result
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_GT_LABEL: {
        int dest = ip->operands[0];              // dest register index
        regs[dest] = MAKE_BOOL(AS_NUMBER(regs[ip->operands[1]]) > AS_NUMBER(regs[ip->operands[2]]));  // compare and store
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_LTE_LABEL: {
        int dest = ip->operands[0];              // dest register index
        regs[dest] = MAKE_BOOL(AS_NUMBER(regs[ip->operands[1]]) <= AS_NUMBER(regs[ip->operands[2]])); // compare and store
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_GTE_LABEL: {
        int dest = ip->operands[0];              // dest register index
        regs[dest] = MAKE_BOOL(AS_NUMBER(regs[ip->operands[1]]) >= AS_NUMBER(regs[ip->operands[2]])); // compare and store
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }

    OP_FOR_INIT_LABEL: {
        int var_reg = ip->operands[0];           // loop variable register
        int end_reg = ip->operands[1];           // end value register
        int step_reg = ip->operands[2];          // step value register
        vm->iterator_depth++;                    // push new iterator frame
        vm->iterator_stack[vm->iterator_depth].index = AS_NUMBER(vm->registers[var_reg]);   // init start value
        vm->iterator_stack[vm->iterator_depth].end   = AS_NUMBER(vm->registers[end_reg]);   // init end value
        vm->iterator_stack[vm->iterator_depth].step  = AS_NUMBER(vm->registers[step_reg]);  // init step value
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_FOR_NEXT_LABEL: {
        int var_reg = ip->operands[0];          // loop variable register
        int end_or_size_reg = ip->operands[1];  // exit address when flag_or_exit == 0
        int flag_or_exit = ip->operands[2];     // 0 for numeric for, other for generic for
        if (flag_or_exit == 0) {                // numeric for loop
            int exit_addr = end_or_size_reg;    // address to jump when loop ends
            double c = vm->iterator_stack[vm->iterator_depth].index;   // current index
            double e = vm->iterator_stack[vm->iterator_depth].end;     // end value
            double s = vm->iterator_stack[vm->iterator_depth].step;    // step value
            if ((s > 0 && c <= e) || (s < 0 && c >= e)) {              // check if still within bounds
                vm->registers[var_reg] = MAKE_NUMBER(c);               // store current index to loop var
                vm->iterator_stack[vm->iterator_depth].index = c + s;  // advance index by step
                ip++;                              // move to loop body
                goto *dispatch_table[ip->opcode];  // dispatch next instruction
            } else {
                vm->iterator_depth--;              // pop iterator frame
                ip = &vm->code[exit_addr];         // jump to exit address
                goto *dispatch_table[ip->opcode];  // dispatch next instruction
            }
        }
    }
    OP_TABLE_ITER_INIT_LABEL: {
        int table_reg = ip->operands[0];         // register holding the table to iterate
        Value tv = regs[table_reg];              // fetch table value
        vm->table_iter_depth++;                  // push new table iterator frame
        TableIterState* iter = &vm->table_iters[vm->table_iter_depth];  // get current iterator state
        if (!IS_TABLE(tv)) {                     // not a table, set empty iterator
            iter->table = NULL;                  // mark as invalid
            iter->array_index = 0;               // reset array index
            iter->bucket_index = 0;              // reset bucket index
            iter->current_entry = NULL;          // reset current entry
        } else {
            iter->table = AS_TABLE(tv);          // store table pointer
            iter->array_index = 0;               // start at first array element
            iter->bucket_index = 0;              // start at first hash bucket
            iter->current_entry = NULL;          // no current entry yet
        }
        ip++;                                    // advance to next instruction
        goto *dispatch_table[ip->opcode];        // dispatch next instruction
    }
    OP_TABLE_ITER_NEXT_LABEL: {
        int var_reg = ip->operands[0];           // register to store the key
        int exit_addr = ip->operands[2];         // address to jump when iteration ends
        TableIterState* iter = &vm->table_iters[vm->table_iter_depth];  // get current iterator state
        Table* t = iter->table;                  // fetch table pointer
        if (t == NULL) {                         // empty or invalid table
            vm->table_iter_depth--;              // pop iterator frame
            ip = &vm->code[exit_addr];           // jump to exit
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        while (iter->array_index < t->array_count) {   // iterate over array part
            int idx = iter->array_index++;             // get current index and advance
            if (!IS_BOOL(t->array_part[idx]) || AS_BOOL(t->array_part[idx])) {  // slot is occupied
                value_decref(regs[var_reg]);           // release old key in var reg
                regs[var_reg] = MAKE_NUMBER(idx + 1);  // store 1-based index as key
                ip++;                                  // advance to loop body
                goto *dispatch_table[ip->opcode];      // dispatch next instruction
            }
        }
        while (iter->bucket_index < t->capacity) {    // iterate over hash part
            if (iter->current_entry == NULL) {        // need to advance to next bucket
                iter->current_entry = t->entries[iter->bucket_index++];  // get first entry in bucket
                continue;                             // retry with the new entry
            }
            TableEntry* entry = iter->current_entry;  // get current entry
            iter->current_entry = entry->next;        // advance to next entry in chain
            value_decref(regs[var_reg]);              // release old key in var reg
            regs[var_reg] = entry->key;               // store key from hash entry
            value_incref(regs[var_reg]);              // bump refcount for the stored key
            ip++;                                     // advance to loop body
            goto *dispatch_table[ip->opcode];         // dispatch next instruction
        }
        vm->table_iter_depth--;              // no more entries, pop iterator frame
        ip = &vm->code[exit_addr];           // jump to exit
        goto *dispatch_table[ip->opcode];    // dispatch next instruction
    }
    OP_POP_ITER_LABEL:
        if (vm->iterator_depth >= 0) vm->iterator_depth--;  // pop iterator frame if any exist
        ip++;                              // advance to next instruction
        goto *dispatch_table[ip->opcode];  // dispatch next instruction

    OP_TABLE_GET_LABEL: {
        int dest = ip->operands[0];                  // dest register index
        int table_reg = ip->operands[1];             // register holding the table
        int key_reg = ip->operands[2];               // register holding the key
        Value table_val = vm->registers[table_reg];  // fetch table value
        if (!IS_TABLE(table_val)) {                  // not a table, return none
            value_decref(vm->registers[dest]);       // release old dest value
            vm->registers[dest] = MAKE_NONE();       // store none
            ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
        }
        Table* table = AS_TABLE(table_val);      // unwrap table pointer
        Value key = vm->registers[key_reg];      // fetch key value
        Value val;
        val = MAKE_NONE();                       // default to none
        table_get(table, key, &val);             // lookup key in table, writes to val
        value_decref(vm->registers[dest]);       // release old dest value
        vm->registers[dest] = val;               // store result (already incref'd by table_get)
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_TABLE_SET_LABEL: {
        int table_reg = ip->operands[0];             // register holding the table
        int key_reg = ip->operands[1];               // register holding the key
        int val_reg = ip->operands[2];               // register holding the value
        Value table_val = vm->registers[table_reg];  // fetch table value
        if (!IS_TABLE(table_val)) {                  // not a table, error
            vm->had_error = true;                    // set error flag
            vm->running = false;                     // stop execution
            goto OP_HALT_LABEL;                      // jump to halt
        }
        Table* table = AS_TABLE(table_val);      // unwrap table pointer
        Value key = vm->registers[key_reg];      // fetch key value
        Value val = vm->registers[val_reg];      // fetch value to store
        table_set(table, key, val);              // perform table set with refcount handling
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_TABLE_GET_CONST_LABEL: {
        int dest = ip->operands[0];                  // dest register index
        int table_reg = ip->operands[1];             // register holding the table
        int key_idx = ip->operands[2];               // constant pool index for the string key
        Value table_val = vm->registers[table_reg];  // fetch table value
        if (!IS_TABLE(table_val)) {                  // not a table, return none
            value_decref(vm->registers[dest]);       // release old dest value
            vm->registers[dest] = MAKE_NONE();       // store none
            ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
        }
        Table* table = AS_TABLE(table_val);      // unwrap table pointer
        Value key = MAKE_STRING(string_intern(&vm->intern_table, chunk->constants[key_idx].string_value, strlen(chunk->constants[key_idx].string_value)));  // intern string key
        Value val;
        val = MAKE_NONE();                       // default to none
        table_get(table, key, &val);             // lookup key in table, writes to val
        value_decref(vm->registers[dest]);       // release old dest value
        vm->registers[dest] = val;               // store result (already incref'd by table_get)
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_TABLE_SET_CONST_LABEL: {
        int table_reg = ip->operands[0];         // register holding the table
        int key_idx = ip->operands[1];           // constant pool index for the string key
        int val_reg = ip->operands[2];           // register holding the value
        Table* table = AS_TABLE(vm->registers[table_reg]);  // unwrap table pointer
        Value key = MAKE_STRING(string_intern(&vm->intern_table, chunk->constants[key_idx].string_value, strlen(chunk->constants[key_idx].string_value)));  // intern string key
        table_set(table, key, vm->registers[val_reg]);      // perform table set with refcount handling
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_TABLE_APPEND_LABEL: {
        int table_reg = ip->operands[0];                    // register holding the table
        int val_reg = ip->operands[1];                      // register holding the value to append
        Table* table = AS_TABLE(vm->registers[table_reg]);  // unwrap table pointer
        Value val = vm->registers[val_reg];                 // fetch value to append
        if (table->array_part == NULL) {                    // lazy init of array part
            table->array_capacity = TABLE_ARRAY_INIT;       // set initial capacity
            table->array_part = (Value*)calloc(TABLE_ARRAY_INIT, sizeof(Value));  // allocate array
            for (int i = 0; i < TABLE_ARRAY_INIT; i++) {
                table->array_part[i] = MAKE_BOOL(false);    // fill with false (empty slot marker)
            }
        }
        int idx = table->array_count;            // index to append at
        if (idx >= table->array_capacity) {      // need to grow array part
            array_part_grow(table, idx);         // resize to accommodate new index
        }
        value_decref(table->array_part[idx]);    // release old value at slot
        table->array_part[idx] = val;            // store new value
        value_incref(table->array_part[idx]);    // bump refcount for stored value
        table->array_count++;                    // increment element count
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_NEW_TABLE_LABEL: {
        int dest = ip->operands[0];              // dest register index
        value_decref(vm->registers[dest]);       // release old value in dest
        vm->registers[dest] = MAKE_TABLE(table_create(8));  // create new table with default capacity
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }

    OP_CONCAT_LABEL: {
        int dest = ip->operands[0];                // dest register index
        Value left  = vm->registers[ip->operands[1]];  // left operand
        Value right = vm->registers[ip->operands[2]];  // right operand
        char lbuf[64], rbuf[64];                   // temp buffers for number/bool to string conversion
        const char* ls = value_to_cstr(left, lbuf, sizeof(lbuf));   // convert left to c string
        const char* rs = value_to_cstr(right, rbuf, sizeof(rbuf));  // convert right to c string
        int llen = IS_STRING(left) ? AS_STRING(left)->length : (int)strlen(ls);    // left length
        int rlen = IS_STRING(right) ? AS_STRING(right)->length : (int)strlen(rs);  // right length
        int total_len = llen + rlen;               // combined length
        if (total_len >= 16 && total_len <= 64) {  // small string, try interning
            char combined[65];                     // stack buffer for combined string
            memcpy(combined, ls, llen);            // copy left part
            memcpy(combined + llen, rs, rlen);     // copy right part
            combined[total_len] = '\0';            // null terminate
            StringObject* interned = string_intern(&vm->intern_table, combined, total_len);  // intern for dedup
            value_decref(vm->registers[dest]);     // release old dest value
            vm->registers[dest] = MAKE_STRING(interned);  // store interned string
            value_incref(vm->registers[dest]);     // bump refcount
        } else {
            StringBuilder sb;                      // use string builder for larger strings
            sb_init(&sb, total_len + 1);           // init with exact capacity
            sb_append(&sb, ls, llen);              // append left part
            sb_append(&sb, rs, rlen);              // append right part
            value_decref(vm->registers[dest]);     // release old dest value
            vm->registers[dest] = MAKE_STRING(sb_to_string(&sb));  // convert builder to string object
            sb_free(&sb);                          // free builder buffer
        }
        ip++; goto *dispatch_table[ip->opcode];    // advance to next instruction
    }

    OP_AND_LABEL: {
        int dest = ip->operands[0];              // dest register index
        regs[dest] = MAKE_BOOL(AS_BOOL(regs[ip->operands[1]]) && AS_BOOL(regs[ip->operands[2]]));  // logical and
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_OR_LABEL: {
        int dest = ip->operands[0];              // dest register index
        regs[dest] = MAKE_BOOL(AS_BOOL(regs[ip->operands[1]]) || AS_BOOL(regs[ip->operands[2]]));  // logical or
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_NOT_LABEL: {
        int dest = ip->operands[0];              // dest register index
        regs[dest] = MAKE_BOOL(!AS_BOOL(regs[ip->operands[1]]));  // logical not
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }

    OP_PUSH_ARG_LABEL: {
        if (vm->args_top >= VM_MAX_ARGS_STACK) {  // check for arg stack overflow
            fprintf(stderr, "\033[31mArgument stack overflow - maximum %d arguments exceeded. "
                    "Too many function arguments being passed.\n\033[0m",
                    VM_MAX_ARGS_STACK);           // print error message
            vm->had_error = true;                 // set error flag
            vm->running = false;                  // stop execution
            return false;                         // early return
        }
        int reg = ip->operands[0];                   // register holding the argument
        Value src = vm->registers[reg];              // fetch argument value
        vm->args_stack[vm->args_top] = src;          // push onto args stack
        value_incref(vm->args_stack[vm->args_top]);  // bump refcount for stored arg
        vm->args_top++;                              // increment args stack pointer
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
    }
    OP_CALL_LABEL: {
        if (vm->call_depth >= VM_MAX_CALL_FRAMES) {  // check for call stack overflow
            fprintf(stderr, "\033[31mStack overflow - maximum call depth (%d) exceeded. "
                    "Too many nested function calls or infinite recursion detected.\n\033[0m", 
                    VM_MAX_CALL_FRAMES);         // print error message
            vm->had_error = true;                // set error flag
            vm->running = false;                 // stop execution
            return false;                        // early return
        }
        int func_addr = ip->operands[1];         // address of function body
        int arg_count = ip->operands[2];         // number of arguments
        int dest_reg  = ip->operands[0];         // dest register for return value
        vm->call_stack[vm->call_depth].return_address = (ip + 1) - vm->code;      // save return address
        vm->call_stack[vm->call_depth].dest_reg       = dest_reg;                 // save dest register
        vm->call_stack[vm->call_depth].frame_index    = vm->current_frame;        // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;  // save iterator depth
        vm->call_depth++;                        // push call frame
        vm->current_frame++;                     // advance to next register frame
        vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];  // switch to new register frame
        regs = vm->registers;                    // update local regs pointer
        for (int i = 0; i < arg_count; i++) {
            regs[i] = vm->args_stack[vm->args_top - arg_count + i];  // copy args into new frame registers
        }
        vm->args_top -= arg_count;               // pop args from args stack
        ip = &vm->code[func_addr];               // jump to function body
        goto *dispatch_table[ip->opcode];        // dispatch first instruction of function
    }
    OP_CALL_BUILTIN_LABEL: {
        int dest_reg = ip->operands[0];             // dest register for return value
        int name_idx = ip->operands[1];             // constant pool index for builtin name
        int arg_count = ip->operands[2];            // number of arguments
        Value args[VM_MAX_ARGS_STACK];              // local args array
        for (int i = 0; i < arg_count && i < 16; i++) {
            args[i] = vm->args_stack[vm->args_top - arg_count + i];  // copy args from stack
        }
        Value result;                               // placeholder for return value
        bool ok = vm_call_builtin(vm, chunk->constants[name_idx].string_value, arg_count, args, &result);  // dispatch to builtin
        for (int i = 0; i < arg_count; i++) {
            value_decref(vm->args_stack[vm->args_top - arg_count + i]);  // release args from stack
        }
        vm->args_top -= arg_count;                  // pop args from args stack
        if (ok) {
            value_decref(vm->registers[dest_reg]);  // release old dest value
            vm->registers[dest_reg] = result;       // store result from builtin
        } else {
            value_decref(vm->registers[dest_reg]);  // release old dest value
            vm->registers[dest_reg] = MAKE_NONE();  // builtin failed, store none
        }
        if (dest_reg >= vm->register_count) vm->register_count = dest_reg + 1;  // track max register used
        ip++; goto *dispatch_table[ip->opcode];     // advance to next instruction
    }
    OP_RETURN_LABEL: {
        int value_reg = ip->operands[0];         // register holding return value
        Value ret_val = regs[value_reg];         // fetch return value
        value_incref(ret_val);                   // bump refcount for the returned value
        if (vm->call_depth > 0) {                // returning from a function call
            vm->call_depth--;                    // pop call frame
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;  // restore frame index
            vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];  // restore register frame
            regs = vm->registers;                // update local regs pointer
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;  // restore iterator depth
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;          // dest register for return value
            value_decref(regs[dest_reg]);        // release old value in dest
            regs[dest_reg] = ret_val;            // store return value in caller's dest reg
            ip = &vm->code[vm->call_stack[vm->call_depth].return_address];   // jump to return address
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        vm->running = false;                     // top-level return, stop execution
        value_decref(ret_val);                   // release return value
        goto OP_HALT_LABEL;                      // jump to halt
    }
    OP_RETURN_VOID_LABEL: {
        if (vm->call_depth > 0) {                // returning from a function call
            vm->call_depth--;                    // pop call frame
            int return_addr = vm->call_stack[vm->call_depth].return_address;  // get return address
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;           // get dest register
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;   // restore frame index
            vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];  // restore register frame
            regs = vm->registers;                // update local regs pointer
            value_decref(regs[dest_reg]);        // release old value in dest
            regs[dest_reg] = MAKE_NONE();        // void return, store none in dest
            ip = &vm->code[return_addr];         // jump to return address
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        vm->running = false;                     // top-level return, stop execution
        goto OP_HALT_LABEL;                      // jump to halt
    }

    OP_CALL_0_LABEL: {
        if (vm->call_depth >= VM_MAX_CALL_FRAMES) {  // check for call stack overflow
            fprintf(stderr, "\033[31mStack overflow - maximum call depth (%d) exceeded. "
                    "Too many nested function calls or infinite recursion detected.\n\033[0m", 
                    VM_MAX_CALL_FRAMES);             // print error message
            vm->had_error = true;                    // set error flag
            vm->running = false;                     // stop execution
            goto OP_HALT_LABEL;                      // jump to halt
        }
        int func_addr = ip->operands[1];             // address of function body
        int dest_reg = ip->operands[0];              // dest register for return value
        vm->args_top = 0;                            // reset args stack (0 args)
        vm->call_stack[vm->call_depth].return_address = (int)((ip + 1) - vm->code);  // save return address
        vm->call_stack[vm->call_depth].dest_reg = dest_reg;                          // save dest register
        vm->call_stack[vm->call_depth].frame_index = vm->current_frame;              // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;     // save iterator depth
        vm->call_depth++;                            // push call frame
        vm->current_frame++;                         // advance to next register frame
        vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];  // switch to new register frame
        regs = vm->registers;                        // update local regs pointer
        ip = &vm->code[func_addr];                   // jump to function body
        goto *dispatch_table[ip->opcode];            // dispatch first instruction of function
    }
    OP_CALL_1_LABEL: {
        if (vm->call_depth >= VM_MAX_CALL_FRAMES) {  // check for call stack overflow
            fprintf(stderr, "\033[31mStack overflow - maximum call depth (%d) exceeded. "
                    "Too many nested function calls or infinite recursion detected.\n\033[0m", 
                    VM_MAX_CALL_FRAMES);             // print error message
            vm->had_error = true;                    // set error flag
            vm->running = false;                     // stop execution
            goto OP_HALT_LABEL;                      // jump to halt
        }
        int func_addr = ip->operands[1];             // address of function body
        int dest_reg = ip->operands[0];              // dest register for return value
        int arg_reg = ip->operands[2];               // register holding the single argument
        vm->args_top = 0;                            // reset args stack (not used for fast call_1)
        vm->call_stack[vm->call_depth].return_address = (int)((ip + 1) - vm->code);  // save return address
        vm->call_stack[vm->call_depth].dest_reg = dest_reg;                          // save dest register
        vm->call_stack[vm->call_depth].frame_index = vm->current_frame;              // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;     // save iterator depth
        vm->call_depth++;                            // push call frame
        vm->current_frame++;                         // advance to next register frame
        vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];  // switch to new register frame
        regs = vm->registers;                        // update local regs pointer
        Value* prev_frame = &vm->register_frames[(vm->current_frame - 1) * VM_REGS_PER_FRAME];  // access caller's frame
        regs[0] = prev_frame[arg_reg];               // copy single arg into new frame's reg 0
        ip = &vm->code[func_addr];                   // jump to function body
        goto *dispatch_table[ip->opcode];            // dispatch first instruction of function
    }
    OP_CALL_2_LABEL: {
        if (vm->call_depth >= VM_MAX_CALL_FRAMES) {  // check for call stack overflow
            fprintf(stderr, "\033[31mStack overflow - maximum call depth (%d) exceeded. "
                    "Too many nested function calls or infinite recursion detected.\n\033[0m", 
                    VM_MAX_CALL_FRAMES);             // print error message
            vm->had_error = true;                    // set error flag
            vm->running = false;                     // stop execution
            goto OP_HALT_LABEL;                      // jump to halt
        }
        int func_addr = ip->operands[1];             // address of function body
        int dest_reg = ip->operands[0];              // dest register for return value
        int arg1_reg = ip->operands[2];              // register holding first argument
        int arg2_reg = arg1_reg + 1;                 // second argument is in the next register
        vm->args_top = 0;                            // reset args stack (not used for fast call_2)
        vm->call_stack[vm->call_depth].return_address = (int)((ip + 1) - vm->code);  // save return address
        vm->call_stack[vm->call_depth].dest_reg = dest_reg;                          // save dest register
        vm->call_stack[vm->call_depth].frame_index = vm->current_frame;              // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;     // save iterator depth
        vm->call_depth++;                            // push call frame
        vm->current_frame++;                         // advance to next register frame
        vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];  // switch to new register frame
        regs = vm->registers;                        // update local regs pointer
        Value* prev_frame = &vm->register_frames[(vm->current_frame - 1) * VM_REGS_PER_FRAME];  // access caller's frame
        regs[0] = prev_frame[arg1_reg];              // copy first arg into new frame's reg 0
        regs[1] = prev_frame[arg2_reg];              // copy second arg into new frame's reg 1
        ip = &vm->code[func_addr];                   // jump to function body
        goto *dispatch_table[ip->opcode];            // dispatch first instruction of function
    }
    OP_RETURN_NUM_LABEL: {
        int value_reg = ip->operands[0];         // register holding return value (guaranteed number)
        Value ret_val = regs[value_reg];         // fetch return value
        if (vm->call_depth > 0) {                // returning from a function call
            vm->call_depth--;                    // pop call frame
            int return_addr = vm->call_stack[vm->call_depth].return_address;  // get return address
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;           // get dest register
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;   // restore frame index
            vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];  // restore register frame
            regs = vm->registers;                // update local regs pointer
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;      // restore iterator depth
            regs[dest_reg] = ret_val;            // store return value in caller's dest reg (no incref, unboxed number)
            ip = &vm->code[return_addr];         // jump to return address
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        vm->running = false;                     // top-level return, stop execution
        goto OP_HALT_LABEL;                      // jump to halt
    }

    OP_LOAD_GLOBAL_LABEL: {
        int dest = ip->operands[0];              // dest register index
        int idx = ip->operands[1];               // global variable index
        Value gv = vm->globals[idx];             // fetch global value
        if (IS_NUMBER(gv) || IS_BOOL(gv) || IS_NONE(gv)) {
            regs[dest] = gv;                     // immediate types, no refcount needed
        } else {
            value_decref(regs[dest]);            // release old dest value
            regs[dest] = gv;                     // copy global value
            value_incref(regs[dest]);            // bump refcount for new reference
        }
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_STORE_GLOBAL_LABEL: {
        int src = ip->operands[0];               // source register index
        int idx = ip->operands[1];               // global variable index
        Value sv = regs[src];                    // fetch source value
        value_decref(vm->globals[idx]);          // release old global value
        vm->globals[idx] = sv;                   // store new value into global
        value_incref(vm->globals[idx]);          // bump refcount for stored value
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }

    OP_HALT_LABEL:
        vm->running = false;                     // stop vm execution
        return !vm->had_error;                   // return true if no errors occurred

    return !vm->had_error;
}
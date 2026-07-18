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

// returns the type of a NaN-boxed value
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

// constructs a numeric value (unboxed double, NaN-boxed if NaN)
Value vm_make_number(double value) {
    return MAKE_NUMBER(value);
}

// constructs a string value (pointer stored in NaN box)
Value vm_make_string(const char* value) {
    StringObject* str = string_create(value, (int)strlen(value));
    return MAKE_STRING(str);
}

// constructs a none/null value (special NaN tag)
Value vm_make_none(void) {
    return MAKE_NONE();
}

// constructs a boolean value (special NaN tag with boolean payload)
Value vm_make_bool(bool value) {
    return MAKE_BOOL(value);
}

// constructs a new empty table value (pointer in NaN box)
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
    for (int i = 0; i < chunk->global_count; i++) {
        vm->globals[i] = MAKE_BOOL(false);
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
        regs[dest] = MAKE_NUMBER((double)value);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_LOAD_CONST_LABEL: {
        int dest = ip->operands[0]; int const_idx = ip->operands[1];
        Constant* c = &chunk->constants[const_idx];
        value_decref(regs[dest]);
        switch (c->type) {
            case CONST_NUMBER: 
                regs[dest] = MAKE_NUMBER(c->number_value);
                break;
            case CONST_STRING: 
                {
                    int len = (int)strlen(c->string_value);
                    StringObject* str = string_intern(&vm->intern_table, c->string_value, len);
                    regs[dest] = MAKE_STRING(str);
                    value_incref(regs[dest]);
                }
                break;
            case CONST_FUNCTION:
                regs[dest] = MAKE_FUNCTION(c->function_index);
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
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MOVE_LABEL: {
        int dest = ip->operands[0]; int src = ip->operands[1];
        Value sv = regs[src];
        if (IS_NUMBER(sv) || IS_BOOL(sv) || IS_NONE(sv)) {
            regs[dest] = sv;
        } else {
            value_decref(regs[dest]);
            regs[dest] = sv;
            value_incref(regs[dest]);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_ADD_LABEL: {
        int dest = ip->operands[0];
        du64 a = {.u = regs[ip->operands[1]]};
        du64 b = {.u = regs[ip->operands[2]]};
        double r = a.d + b.d;
        if (r != r) {
            if (a.d != a.d || b.d != b.d) {
                value_decref(regs[dest]);
                regs[dest] = MAKE_NONE();
            } else {
                regs[dest] = MAKE_NUMBER(r);
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_SUB_LABEL: {
        int dest = ip->operands[0];
        du64 a = {.u = regs[ip->operands[1]]};
        du64 b = {.u = regs[ip->operands[2]]};
        double r = a.d - b.d;
        if (r != r) {
            if (a.d != a.d || b.d != b.d) {
                value_decref(regs[dest]);
                regs[dest] = MAKE_NONE();
            } else {
                regs[dest] = MAKE_NUMBER(r);
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MUL_LABEL: {
        int dest = ip->operands[0];
        du64 a = {.u = regs[ip->operands[1]]};
        du64 b = {.u = regs[ip->operands[2]]};
        double r = a.d * b.d;
        if (r != r) {
            if (a.d != a.d || b.d != b.d) {
                value_decref(regs[dest]);
                regs[dest] = MAKE_NONE();
            } else {
                regs[dest] = MAKE_NUMBER(r);
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_DIV_LABEL: {
        int dest = ip->operands[0];
        du64 a = {.u = regs[ip->operands[1]]};
        du64 b = {.u = regs[ip->operands[2]]};
        double r = a.d / b.d;
        if (r != r) {
            if (a.d != a.d || b.d != b.d) {
                value_decref(regs[dest]);
                regs[dest] = MAKE_NONE();
            } else {
                regs[dest] = MAKE_NUMBER(r);
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_MOD_LABEL: {
        int dest = ip->operands[0];
        du64 a = {.u = regs[ip->operands[1]]};
        du64 b = {.u = regs[ip->operands[2]]};
        double r = fmod(a.d, b.d);
        if (r != r) {
            if (a.d != a.d || b.d != b.d) {
                value_decref(regs[dest]);
                regs[dest] = MAKE_NONE();
            } else {
                regs[dest] = MAKE_NUMBER(r);
            }
        } else {
            regs[dest] = MAKE_NUMBER(r);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NEG_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_NUMBER(-AS_NUMBER(regs[ip->operands[1]]));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_EQ_LABEL: {
        int dest = ip->operands[0];
        Value left = regs[ip->operands[1]];
        Value right = regs[ip->operands[2]];
        int result = 0;
        
        if (IS_NONE(left) && IS_NONE(right)) {
            result = 1;
        }
        else if (IS_NONE(left) || IS_NONE(right)) {
            result = 0;
        }
        else if (IS_FUNCTION(left) && IS_FUNCTION(right)) {
            result = (AS_FUNCTION(left) == AS_FUNCTION(right));
        }
        else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            result = (AS_NUMBER(left) == AS_NUMBER(right));
        }
        else if (IS_STRING(left) && IS_STRING(right)) {
            result = string_equal(AS_STRING(left), AS_STRING(right));
        }
        else if (IS_BOOL(left) && IS_BOOL(right)) {
            result = (AS_BOOL(left) == AS_BOOL(right));
        }
        regs[dest] = MAKE_BOOL(result);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_NEQ_LABEL: {
        int dest = ip->operands[0];
        Value left = regs[ip->operands[1]];
        Value right = regs[ip->operands[2]];
        int result = 1;
        
        if (IS_NONE(left) && IS_NONE(right)) {
            result = 0;
        }
        else if (IS_NONE(left) || IS_NONE(right)) {
            result = 1;
        }
        else if (IS_FUNCTION(left) && IS_FUNCTION(right)) {
            result = (AS_FUNCTION(left) != AS_FUNCTION(right));
        }
        else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            result = (AS_NUMBER(left) != AS_NUMBER(right));
        }
        else if (IS_STRING(left) && IS_STRING(right)) {
            result = !string_equal(AS_STRING(left), AS_STRING(right));
        }
        else if (IS_BOOL(left) && IS_BOOL(right)) {
            result = (AS_BOOL(left) != AS_BOOL(right));
        }
        regs[dest] = MAKE_BOOL(result);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_LT_LABEL: {
        int dest = ip->operands[0];
        du64 a = {.u = regs[ip->operands[1]]};
        du64 b = {.u = regs[ip->operands[2]]};
        regs[dest] = MAKE_BOOL(a.d < b.d);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_GT_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_BOOL(AS_NUMBER(regs[ip->operands[1]]) > AS_NUMBER(regs[ip->operands[2]]));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_LTE_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_BOOL(AS_NUMBER(regs[ip->operands[1]]) <= AS_NUMBER(regs[ip->operands[2]]));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CMP_GTE_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_BOOL(AS_NUMBER(regs[ip->operands[1]]) >= AS_NUMBER(regs[ip->operands[2]]));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_AND_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_BOOL(AS_BOOL(regs[ip->operands[1]]) && AS_BOOL(regs[ip->operands[2]]));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_OR_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_BOOL(AS_BOOL(regs[ip->operands[1]]) || AS_BOOL(regs[ip->operands[2]]));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NOT_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_BOOL(!AS_BOOL(regs[ip->operands[1]]));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_LABEL: ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode];
    OP_JUMP_IF_FALSE_LABEL: {
        int cond_reg = ip->operands[1];
        if (!AS_BOOL(vm->registers[cond_reg])) { ip = &vm->code[ip->operands[0]]; goto *dispatch_table[ip->opcode]; }
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
            value_decref(vm->args_stack[vm->args_top - arg_count + i]);
        }
        vm->args_top -= arg_count;

        if (ok) {
            value_decref(vm->registers[dest_reg]);
            vm->registers[dest_reg] = result;
        } else {
            value_decref(vm->registers[dest_reg]);
            vm->registers[dest_reg] = MAKE_NONE();
        }
        if (dest_reg >= vm->register_count) vm->register_count = dest_reg + 1;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_RETURN_LABEL: {
        int value_reg = ip->operands[0];
        Value ret_val = regs[value_reg];
        value_incref(ret_val);
        
        if (vm->call_depth > 0) {
            vm->call_depth--;
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;
            
            vm->registers = &vm->register_frames[vm->current_frame * VM_REGS_PER_FRAME];
            regs = vm->registers;
            
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;
            value_decref(regs[dest_reg]);
            regs[dest_reg] = ret_val;
            
            ip = &vm->code[vm->call_stack[vm->call_depth].return_address];
            goto *dispatch_table[ip->opcode];
        }
        vm->running = false;
        value_decref(ret_val);
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
            value_decref(regs[dest_reg]);
            regs[dest_reg] = MAKE_NONE();
            ip = &vm->code[return_addr]; goto *dispatch_table[ip->opcode];
        }
        vm->running = false; goto OP_HALT_LABEL;
    }

    OP_LOAD_GLOBAL_LABEL: {
        int dest = ip->operands[0]; int idx = ip->operands[1];
        Value gv = vm->globals[idx];
        if (IS_NUMBER(gv) || IS_BOOL(gv) || IS_NONE(gv)) {
            regs[dest] = gv;
        } else {
            value_decref(regs[dest]);
            regs[dest] = gv;
            value_incref(regs[dest]);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_STORE_GLOBAL_LABEL: {
        int src = ip->operands[0]; int idx = ip->operands[1];
        Value sv = regs[src];
        value_decref(vm->globals[idx]);
        vm->globals[idx] = sv;
        value_incref(vm->globals[idx]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_NEW_TABLE_LABEL: {
        int dest = ip->operands[0];
        value_decref(vm->registers[dest]);
        vm->registers[dest] = MAKE_TABLE(table_create(8));
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_SET_LABEL: {
        int table_reg = ip->operands[0];
        int key_reg = ip->operands[1];
        int val_reg = ip->operands[2];
        
        Value table_val = vm->registers[table_reg];
        if (!IS_TABLE(table_val)) {
            vm->had_error = true;
            vm->running = false;
            goto OP_HALT_LABEL;
        }
        
        Table* table = AS_TABLE(table_val);
        Value key = vm->registers[key_reg];
        Value val = vm->registers[val_reg];
        table_set(table, key, val);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_SET_CONST_LABEL: {
        int table_reg = ip->operands[0];
        int key_idx = ip->operands[1];
        int val_reg = ip->operands[2];
        Table* table = AS_TABLE(vm->registers[table_reg]);
        Value key = MAKE_STRING(string_intern(&vm->intern_table, chunk->constants[key_idx].string_value, strlen(chunk->constants[key_idx].string_value)));
        table_set(table, key, vm->registers[val_reg]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_LABEL: {
        int dest = ip->operands[0];
        int table_reg = ip->operands[1];
        int key_reg = ip->operands[2];
        Value table_val = vm->registers[table_reg];
        
        if (!IS_TABLE(table_val)) {
            value_decref(vm->registers[dest]);
            vm->registers[dest] = MAKE_NONE();
            ip++; goto *dispatch_table[ip->opcode];
        }
        
        Table* table = AS_TABLE(table_val);
        Value key = vm->registers[key_reg];
        Value val;
        val = MAKE_NONE();
        table_get(table, key, &val);
        
        value_decref(vm->registers[dest]);
        vm->registers[dest] = val;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_GET_CONST_LABEL: {
        int dest = ip->operands[0];
        int table_reg = ip->operands[1];
        int key_idx = ip->operands[2];
        Value table_val = vm->registers[table_reg];
        
        if (!IS_TABLE(table_val)) {
            value_decref(vm->registers[dest]);
            vm->registers[dest] = MAKE_NONE();
            ip++; goto *dispatch_table[ip->opcode];
        }
        
        Table* table = AS_TABLE(table_val);
        Value key = MAKE_STRING(string_intern(&vm->intern_table, chunk->constants[key_idx].string_value, strlen(chunk->constants[key_idx].string_value)));
        
        Value val;
        val = MAKE_NONE();
        table_get(table, key, &val);
        
        value_decref(vm->registers[dest]);
        vm->registers[dest] = val;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_TABLE_APPEND_LABEL: {
        int table_reg = ip->operands[0];
        int val_reg = ip->operands[1];
        
        Table* table = AS_TABLE(vm->registers[table_reg]);
        Value val = vm->registers[val_reg];
        
        if (table->array_part == NULL) {
            table->array_capacity = TABLE_ARRAY_INIT;
            table->array_part = (Value*)calloc(TABLE_ARRAY_INIT, sizeof(Value));
            for (int i = 0; i < TABLE_ARRAY_INIT; i++) {
                table->array_part[i] = MAKE_BOOL(false);
            }
        }
        
        int idx = table->array_count;
        if (idx >= table->array_capacity) {
            array_part_grow(table, idx);
        }
        
        value_decref(table->array_part[idx]);
        table->array_part[idx] = val;
        value_incref(table->array_part[idx]);
        table->array_count++;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_CONCAT_LABEL: {
        int dest = ip->operands[0];
        Value left  = vm->registers[ip->operands[1]]; 
        Value right = vm->registers[ip->operands[2]];
        
        char lbuf[64], rbuf[64];
        const char* ls = value_to_cstr(left, lbuf, sizeof(lbuf));
        const char* rs = value_to_cstr(right, rbuf, sizeof(rbuf));
        int llen = IS_STRING(left) ? AS_STRING(left)->length : (int)strlen(ls);
        int rlen = IS_STRING(right) ? AS_STRING(right)->length : (int)strlen(rs);
        int total_len = llen + rlen;
        
        if (total_len >= 16 && total_len <= 64) {
            char combined[65];
            memcpy(combined, ls, llen);
            memcpy(combined + llen, rs, rlen);
            combined[total_len] = '\0';
            
            StringObject* interned = string_intern(&vm->intern_table, combined, total_len);
            value_decref(vm->registers[dest]);
            vm->registers[dest] = MAKE_STRING(interned);
            value_incref(vm->registers[dest]);
        } else {
            StringBuilder sb;
            sb_init(&sb, total_len + 1);
            sb_append(&sb, ls, llen);
            sb_append(&sb, rs, rlen);
            value_decref(vm->registers[dest]);
            vm->registers[dest] = MAKE_STRING(sb_to_string(&sb));
            sb_free(&sb);
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_FOR_INIT_LABEL: {
        int var_reg = ip->operands[0];
        int end_reg = ip->operands[1];
        int step_reg = ip->operands[2];
        vm->iterator_depth++;
        vm->iterator_stack[vm->iterator_depth].index = AS_NUMBER(vm->registers[var_reg]);
        vm->iterator_stack[vm->iterator_depth].end   = AS_NUMBER(vm->registers[end_reg]);
        vm->iterator_stack[vm->iterator_depth].step  = AS_NUMBER(vm->registers[step_reg]);
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_FOR_NEXT_LABEL: {
        int var_reg = ip->operands[0];      
        int end_or_size_reg = ip->operands[1];
        int flag_or_exit = ip->operands[2];

        if (flag_or_exit == 0) {
            int exit_addr = end_or_size_reg;

            double c = vm->iterator_stack[vm->iterator_depth].index;
            double e = vm->iterator_stack[vm->iterator_depth].end;
            double s = vm->iterator_stack[vm->iterator_depth].step;

            if ((s > 0 && c <= e) || (s < 0 && c >= e)) {
                vm->registers[var_reg] = MAKE_NUMBER(c);
                
                vm->iterator_stack[vm->iterator_depth].index = c + s;
                
                ip++;
                goto *dispatch_table[ip->opcode];
            } else {
                vm->iterator_depth--;
                ip = &vm->code[exit_addr];
                goto *dispatch_table[ip->opcode];
            }
        } else {
            int exit_addr = flag_or_exit;
            
            double index = AS_NUMBER(vm->registers[var_reg]);
            double size = AS_NUMBER(vm->registers[end_or_size_reg]);
            
            index += 1.0;
            vm->registers[var_reg] = MAKE_NUMBER(index);
            
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
        du64 a = {.u = regs[ip->operands[1]]};
        du64 b = {.u = regs[ip->operands[2]]};
        if (a.d < b.d) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_LTE_LABEL: {
        int target = ip->operands[0];
        if (AS_NUMBER(regs[ip->operands[1]]) <= AS_NUMBER(regs[ip->operands[2]])) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_GT_LABEL: {
        int target = ip->operands[0];
        if (AS_NUMBER(regs[ip->operands[1]]) > AS_NUMBER(regs[ip->operands[2]])) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_GTE_LABEL: {
        int target = ip->operands[0];
        if (AS_NUMBER(vm->registers[ip->operands[1]]) >= AS_NUMBER(vm->registers[ip->operands[2]])) { ip = &vm->code[target]; goto *dispatch_table[ip->opcode]; }
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_JUMP_IF_EQ_LABEL: {
        int target = ip->operands[0];
        Value left = regs[ip->operands[1]];
        Value right = regs[ip->operands[2]];
        bool jump = false;
        
        if (IS_NONE(left) && IS_NONE(right)) {
            jump = true;
        } else if (IS_NONE(left) || IS_NONE(right)) {
            jump = false;
        } else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            jump = (AS_NUMBER(left) == AS_NUMBER(right));
        } else if (IS_STRING(left) && IS_STRING(right)) {
            jump = string_equal(AS_STRING(left), AS_STRING(right));
        } else if (IS_BOOL(left) && IS_BOOL(right)) {
            jump = (AS_BOOL(left) == AS_BOOL(right));
        }
        if (jump) {
            ip = &vm->code[target];
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];
    }

    OP_JUMP_IF_NEQ_LABEL: {
        int target = ip->operands[0];
        Value left = regs[ip->operands[1]];
        Value right = regs[ip->operands[2]];
        bool jump = false;
        
        if (IS_NONE(left) && IS_NONE(right)) {
            jump = false;
        } else if (IS_NONE(left) || IS_NONE(right)) {
            jump = true;
        } else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            jump = (AS_NUMBER(left) != AS_NUMBER(right));
        } else if (IS_STRING(left) && IS_STRING(right)) {
            jump = !string_equal(AS_STRING(left), AS_STRING(right));
        } else if (IS_BOOL(left) && IS_BOOL(right)) {
            jump = (AS_BOOL(left) != AS_BOOL(right));
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
        Value src = vm->registers[reg];
        vm->args_stack[vm->args_top] = src;
        value_incref(vm->args_stack[vm->args_top]);
        vm->args_top++;
        ip++; goto *dispatch_table[ip->opcode];
    }
    OP_HALT_LABEL: vm->running = false; return !vm->had_error;
    OP_LOAD_BOOL_LABEL: {
        int dest = ip->operands[0];
        regs[dest] = MAKE_BOOL(ip->operands[1] != 0);
        ip++; goto *dispatch_table[ip->opcode];
    }
    return !vm->had_error;
}
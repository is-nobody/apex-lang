// source/core/vm.c
// Implementation of Virtual Machine for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "vm.h"
#if APEX_JIT_ENABLED
#include "jit.h"
#endif
#include "os_module.h"
#include "sys_module.h"
#include "math_module.h"
#include "string_module.h"
#include "table_module.h"
#include "random_module.h"
#include "json_module.h"
#include "xml_module.h"
#include "csv_module.h"
#include "base_module.h"
#include "regex_module.h"
#include "crypto_module.h"
#include "zip_module.h"
#include "network_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#include <process.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

// union for reinterpret double bits as uint64
typedef union { uint64_t u; double d; } du64;

// global runtime toggle for the JIT; false means "never call jit"
#if APEX_JIT_ENABLED
bool apex_jit_runtime_enabled = true;
#endif

// dynamic string builder for efficient concatenation
typedef struct {
    char* buffer;  // dynamically allocated char buffer
    int length;    // current string length
    int capacity;  // total buffer capacity
} StringBuilder;

// snapshot of the vm's currently active pool pointers
typedef struct {
    Value* register_pool;   // active register pool
    int* frame_offset;      // frame start offsets
    int* frame_capacity;    // frame capacities
    int* frame_used;        // highest register used per frame
    int pool_capacity;      // total pool capacity
    int current_frame;      // active frame index
    ForIter* iterator_stack;// active numeric for-loop storage
    TableIterState* table_iters;// active table iterator storage
} SavedVMContext;

// forward declarations
static char* table_to_string(Table* table);
static void table_to_string_builder(Table* table, StringBuilder* sb, int indent_level);

// djb2 hash function for string interning
static unsigned int intern_hash(const char* chars, int length) {
    unsigned int hash = 5381;                    // djb2 initial seed
    for (int i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)chars[i];  // hash * 33 + char
    }
    return hash;                                 // return computed hash
}

// initializes the string intern table with a fixed size
void string_intern_table_init(StringInternTable* it) {
    it->capacity = INTERN_INITIAL_SIZE;          // set initial bucket count
    it->count = 0;                               // no entries yet
    it->buckets = calloc(INTERN_INITIAL_SIZE, sizeof(StringObject*));  // allocate zeroed bucket array
}

// frees all interned strings and the table itself
void string_intern_table_free(StringInternTable* it) {
    for (int i = 0; i < it->capacity; i++) {
        if (it->buckets[i]) {
            free(it->buckets[i]);                // free each interned string
        }
    }
    free(it->buckets);                           // free bucket array
    it->buckets = NULL;                          // clear dangling pointer
    it->capacity = 0;                            // reset capacity
    it->count = 0;                               // reset count
}

// resizes the intern table when load factor exceeds the threshold
static void intern_table_resize(StringInternTable* it, int new_capacity) {
    StringObject** old_buckets = it->buckets;    // save old bucket array
    int old_capacity = it->capacity;             // save old capacity
    it->buckets = calloc(new_capacity, sizeof(StringObject*));  // allocate new zeroed bucket array
    it->capacity = new_capacity;                 // update capacity
    it->count = 0;                               // reset count, will recount during rehash
    for (int i = 0; i < old_capacity; i++) {
        StringObject* str = old_buckets[i];      // fetch string from old bucket
        if (str) {
            unsigned int idx = intern_hash(str->chars, str->length) % new_capacity;  // compute new bucket index
            while (it->buckets[idx] != NULL) {   // linear probe until empty slot
                idx = (idx + 1) % new_capacity;  // wrap around if needed
            }
            it->buckets[idx] = str;              // insert string into new bucket
            it->count++;                         // increment entry count
        }
    }
    free(old_buckets);                           // free old bucket array
}

// interns a string, returning a canonical object with linear probing
StringObject* string_intern(StringInternTable* it, const char* chars, int length) {
    if (!it || !chars) return NULL;                  // guard against null params
    if ((double)it->count / it->capacity > INTERN_MAX_LOAD) {      // check load factor
        intern_table_resize(it, it->capacity * 2);   // double capacity and rehash
    }
    unsigned int hash = intern_hash(chars, length);  // compute hash of input string
    unsigned int idx = hash % it->capacity;          // initial bucket index
    for (int i = 0; i < it->capacity; i++) {
        unsigned int probe_idx = (idx + i) % it->capacity;         // linear probe offset
        StringObject* existing = it->buckets[probe_idx];           // fetch existing string at slot
        if (existing == NULL) {                      // empty slot, insert new string
            StringObject* new_str = string_create(chars, length);  // allocate new string object
            new_str->hash = hash;                    // store precomputed hash
            new_str->hash_computed = true;           // mark hash as computed
            new_str->header.ref_count = INT_MAX;     // interned strings are immortal
            it->buckets[probe_idx] = new_str;        // insert into bucket
            it->count++;                             // increment entry count
            return new_str;                          // return newly interned string
        }
        if (existing->length == length &&            // length matches
            memcmp(existing->chars, chars, length) == 0) {         // content matches
            return existing;                         // string already interned, return existing
        }
    }
    return NULL;                                     // table is full, should never happen
}

// allocates a new string object with refcount and flexible array
StringObject* string_create(const char* chars, int length) {
    StringObject* str = (StringObject*)malloc(sizeof(StringObject) + length + 1);  // alloc struct + chars + null
    str->header.ref_count = 1;                   // fresh object starts with refcount 1
    str->header.type = VAL_STRING;               // mark type as string
    str->length = length;                        // store length
    str->hash_computed = false;                  // hash not yet computed (lazy)
    str->hash = 0;                               // clear hash field
    memcpy(str->chars, chars, length);           // copy character data
    str->chars[length] = '\0';                   // null terminate
    return str;                                  // return new string object
}

// computes the hash of a string lazily
static uint32_t string_get_hash(StringObject* str) {
    if (!str->hash_computed) {                   // compute hash lazily if not yet done
        uint32_t h = 5381;                       // djb2 initial seed
        for (int i = 0; i < str->length; i++) {
            h = ((h << 5) + h) + (uint8_t)str->chars[i];  // hash * 33 + char
        }
        str->hash = h;                           // store computed hash
        str->hash_computed = true;               // mark hash as computed
    }
    return str->hash;                            // return cached or newly computed hash
}

// compares two strings by length, hash, and content
static bool string_equal(StringObject* a, StringObject* b) {
    if (a == b) return true;                     // same pointer, definitely equal
    if (!a || !b) return false;                  // one is null, not equal
    if (a->length != b->length) return false;    // different lengths, not equal
    if (string_get_hash(a) != string_get_hash(b)) return false;  // different hashes, not equal
    return memcmp(a->chars, b->chars, a->length) == 0;           // compare character data byte by byte
}

// converts a value to a C string for concatenation
static const char* value_to_cstr(Value v, char* buf, int buf_size) {
    if (IS_NUMBER(v)) {
        double num = AS_NUMBER(v);
        if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9)
            snprintf(buf, buf_size, "%.0f", num);           // large or integer, no decimals
        else
            snprintf(buf, buf_size, "%.15g", num);          // use general format with high precision
        return buf;
    } else if (IS_STRING(v)) {
        return AS_STRING(v)->chars;                      // return string's internal chars
    } else if (IS_NONE(v)) {
        return "none";                                   // string representation of none
    } else if (IS_BOOL(v)) {
        return AS_BOOL(v) ? "true" : "false";            // string representation of bool
    } else if (IS_TABLE(v)) {
        char* str = table_to_string(AS_TABLE(v));        // convert table to string
        int len = (int)strlen(str);                      // get length
        if (len < buf_size) {                            // fits in buffer
            memcpy(buf, str, len + 1);                   // copy to buffer
        } else {                                         // too large
            memcpy(buf, str, buf_size - 1);              // copy what fits
            buf[buf_size - 1] = '\0';                    // null terminate
        }
        free(str);                                       // free temp string
        return buf;                                      // return buffer
    } else {
        return "";                                       // fallback empty string
    }
}

// frees a coroutine's private register pool and frame arrays
static void future_free_pool(FutureObject* fut) {
    if (!fut || !fut->owns_frame) return;                // nothing to free
    if (fut->register_pool) {                            // release live registers
        for (int i = 0; i < fut->pool_capacity; i++) {
            if ((fut->register_pool[i] & QNAN) == QNAN) {
                value_decref(fut->register_pool[i]);     // release heap-tagged slot
            }
            fut->register_pool[i] = MAKE_NONE();         // clear slot
        }
        free(fut->register_pool);                        // free pool buffer
        fut->register_pool = NULL;
    }
    free(fut->frame_offset);                             // free frame offsets
    free(fut->frame_capacity);                           // free frame capacities
    free(fut->frame_used);                               // free frame usage counters
    fut->frame_offset = NULL;
    fut->frame_capacity = NULL;
    fut->frame_used = NULL;
    fut->pool_capacity = 0;
    fut->current_frame = 0;
    fut->owns_frame = false;
}

// increments the reference count of a reference-counted value
void value_incref(Value v) {
    if (IS_STRING(v)) {
        StringObject* str = AS_STRING(v);               // unwrap string pointer
        if (str && str->header.ref_count != INT_MAX) {  // not interned (immortal)
            str->header.ref_count++;                    // bump refcount
        }
    } else if (IS_TABLE(v)) {
        Table* table = AS_TABLE(v);                     // unwrap table pointer
        if (table) table->header.ref_count++;           // bump refcount
    } else if (IS_FUTURE(v)) {
        FutureObject* fut = AS_FUTURE(v);               // unwrap future pointer
        if (fut) fut->header.ref_count++;               // bump refcount
    }
}

// decrements the reference count and frees the object when it reaches zero
void value_decref(Value v) {
    if ((v & QNAN) != QNAN) return;                    // unboxed immediate, no refcount needed
    
    if (IS_STRING(v)) {
        StringObject* str = AS_STRING(v);              // unwrap string pointer
        if (str->header.ref_count == INT_MAX) return;  // interned string, never freed
        if (--str->header.ref_count == 0) {            // decrement and check if dead
            if (str) free(str);                        // free string memory
        }
    } else if (IS_TABLE(v)) {
        Table* table = AS_TABLE(v);                    // unwrap table pointer
        if (--table->header.ref_count == 0) {          // decrement and check if dead
            table_destroy(table);                      // destroy table and all entries
        }
    } else if (IS_FUTURE(v)) {
        FutureObject* fut = AS_FUTURE(v);              // unwrap future pointer
        if (--fut->header.ref_count == 0) {            // decrement and check if dead
            for (int i = 0; i < fut->arg_count; i++) value_decref(fut->args[i]);  // release captured args
            free(fut->args);                           // free args array
            value_decref(fut->result);                 // release result value
            value_decref(fut->awaiting);               // release awaited future
            free(fut->waiters);                        // free waiter list
            future_free_pool(fut);                     // free private register pool
            free(fut);                                 // free future struct
        }
    }
}

// returns a type name string for a value
const char* vm_value_type_name(Value value) {
    if (IS_NUMBER(value) || IS_NAN(value)) return "number";  // all nan-boxed doubles including real nan
    if (IS_STRING(value)) return "string";                   // string object pointer
    if (IS_NONE(value)) return "none";                       // special none tag
    if (IS_BOOL(value)) return "boolean";                    // special bool tag
    if (IS_TABLE(value)) return "table";                     // table object pointer
    if (IS_FUNCTION(value)) return "function";               // function index tag
    if (IS_FUTURE(value)) return "future";                   // future object pointer
    return "unknown";                                        // fallback for unhandled types
}

// returns current wall-clock time in seconds
double apex_now_seconds(void) {
    struct timeval tv;                                  // timeval buffer
    gettimeofday(&tv, NULL);                            // read wall clock
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;  // combine into seconds
}

// init string builder with given capacity, min 16 bytes
static void sb_init(StringBuilder* sb, int initial_capacity) {
    sb->capacity = initial_capacity > 16 ? initial_capacity : 16;  // ensure minimum capacity
    sb->buffer = (char*)malloc(sb->capacity);    // allocate buffer
    sb->length = 0;                              // start empty
    sb->buffer[0] = '\0';                        // null terminate
}

// append len chars from str to string builder, growing if needed
static void sb_append(StringBuilder* sb, const char* str, int len) {
    if (sb->length + len + 1 > sb->capacity) {      // need more space
        sb->capacity = (sb->length + len + 1) * 2;  // double capacity to fit new data
        sb->buffer = (char*)realloc(sb->buffer, sb->capacity);  // resize buffer
    }
    memcpy(sb->buffer + sb->length, str, len);      // copy new chars after existing data
    sb->length += len;                              // update length
    sb->buffer[sb->length] = '\0';                  // null terminate
}

// free string builder internal buffer
static void sb_free(StringBuilder* sb) {
    free(sb->buffer);                              // release buffer memory
}

// compute hash for a value used as table key
static uint32_t hash_value_key(Value key) {
    if (IS_STRING(key)) {
        StringObject* str = AS_STRING(key);                   // unwrap string pointer
        if (!str->hash_computed) {                            // compute hash lazily
            uint32_t h = 5381;                                // djb2 initial seed
            for (int i = 0; i < str->length; i++) {
                h = ((h << 5) + h) + (uint8_t)str->chars[i];  // hash * 33 + char
            }
            str->hash = h;                                    // store computed hash
            str->hash_computed = true;                        // mark as computed
        }
        return str->hash;                                     // return string hash
    } else if (IS_NUMBER(key)) {
        double num = AS_NUMBER(key);                          // unwrap number
        if (num == 0.0) num = 0.0;                            // normalize negative zero to positive zero
        union { double d; uint64_t u; } u;
        u.d = num;                                            // reinterpret double bits as uint64
        uint32_t hash = 2166136261u;                          // fnv offset basis
        hash ^= (uint32_t)(u.u & 0xFFFFFFFF);                 // xor low 32 bits
        hash *= 16777619u;                                    // fnv prime
        hash ^= (uint32_t)(u.u >> 32);                        // xor high 32 bits
        hash *= 16777619u;                                    // fnv prime
        return hash;                                          // return number hash
    }
    return 0;                                                 // fallback for unhashable types
}

// compare two values for key equality
static bool key_equal(Value a, Value b) {
    if (IS_NAN(a) && IS_NAN(b)) return false;                  // nan never equals nan for keys
    if (IS_NAN(a) || IS_NAN(b)) return false;                  // one is nan, not equal
    if (IS_STRING(a) && IS_STRING(b)) {
        StringObject* sa = AS_STRING(a);                       // unwrap first string
        StringObject* sb = AS_STRING(b);                       // unwrap second string
        if (sa == sb) return true;                             // same pointer, definitely equal
        if (sa->length != sb->length) return false;            // different lengths, not equal
        return memcmp(sa->chars, sb->chars, sa->length) == 0;  // compare content byte by byte
    }
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);                   // compare numeric values
    }
    if (a == b) return true;                                   // bitwise identical tagged values
    return false;                                              // different types or values
}

// convert a table to a heap-allocated string representation
static char* table_to_string(Table* table) {
    StringBuilder sb;
    sb_init(&sb, 4096);                          // init builder with 4k buffer
    table_to_string_builder(table, &sb, 0);      // recursively build string with indent 0
    char* result = strdup(sb.buffer);            // copy to heap-allocated string
    sb_free(&sb);                                // free builder buffer
    return result;                               // return caller-owned string
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
    (void)indent_level;                          // indent_level handled inside table_to_string
    if (!table) return;                          // guard against null
    char* str = table_to_string(table);          // convert table to string
    printf("%s", str);                           // print to stdout
    free(str);                                   // free the temporary string
}

// prints a value to stdout with formatting
void vm_print_value(Value value) {
    if (IS_NAN(value)) {
        printf("nan");                                    // raw nan value
    } else if (IS_NUMBER(value)) {
        double num = AS_NUMBER(value);                    // unwrap number
        if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9) printf("%.0f", num);  // large or integer, no decimals
        else printf("%.15g", num);                        // use general format with high precision
    } else if (IS_STRING(value)) {
        printf("%s", AS_STRING(value)->chars);            // print raw string chars
    } else if (IS_NONE(value)) {
        printf("none");                                   // print none literal
    } else if (IS_BOOL(value)) {
        printf("%s", AS_BOOL(value) ? "true" : "false");  // print bool literal
    } else if (IS_TABLE(value)) {
        print_table_recursive(AS_TABLE(value), 0);        // recursively print table structure
    } else if (IS_FUNCTION(value)) {
        printf("<function>");                             // placeholder for function values
    }
}

// creates a new hash table with separate array part for integer keys
Table* table_create(int capacity) {
    Table* table = (Table*)malloc(sizeof(Table));   // allocate table struct
    table->header.ref_count = 1;                    // fresh object starts with refcount 1
    table->header.type = VAL_TABLE;                 // mark type as table
    table->capacity = capacity < 8 ? 8 : capacity;  // ensure minimum capacity of 8
    table->hash_count = 0;                          // no hash entries yet
    table->entries = NULL;                          // allocate zeroed bucket array
    table->array_capacity = 0;                      // array part not yet allocated (lazy)
    table->array_part = NULL;                       // no array part yet
    table->array_count = 0;                         // no array elements yet
    return table;                                   // return new table
}

// destroys a table and all its entries
void table_destroy(Table* table) {
    if (!table) return;                          // guard against null
    if (table->entries) {
        for (int i = 0; i < table->capacity; i++) {
            TableEntry* entry = table->entries[i];   // get head of bucket chain
            while (entry) {
                TableEntry* next = entry->next;      // save next pointer before freeing
                value_decref(entry->key);            // release key
                value_decref(entry->value);          // release value
                free(entry);                         // free entry struct
                entry = next;                        // advance to next entry
            }
        }
        free(table->entries);                        // free bucket array
    }
    if (table->array_part) {
        for (int i = 0; i < table->array_count; i++) {
            value_decref(table->array_part[i]);  // release each array element
        }
        free(table->array_part);                 // free array part
    }
    free(table);                                 // free table struct
}

// grow array part to fit needed_index, doubling until large enough
static void array_part_grow(Table* table, int needed_index) {
    if (table->array_part == NULL) {                 // array part not yet allocated
        table->array_capacity = TABLE_ARRAY_INIT;    // start with default size
        while (table->array_capacity <= needed_index) {
            table->array_capacity *= 2;              // double until index fits
        }
        table->array_part = (Value*)malloc(table->array_capacity * sizeof(Value));  // allocate array
        for (int i = 0; i < table->array_capacity; i++) {
            table->array_part[i] = MAKE_NONE();       // fill with none (empty slot marker)
        }
        return;                                       // done, array was just created
    }
    int new_capacity = table->array_capacity;         // start from current capacity
    while (new_capacity <= needed_index) {
        new_capacity *= 2;                            // double until index fits
    }
    Value* new_array = (Value*)malloc(new_capacity * sizeof(Value));  // allocate new larger array
    for (int i = 0; i < table->array_count; i++) {
        new_array[i] = table->array_part[i];          // copy existing elements
    }
    for (int i = table->array_count; i < new_capacity; i++) {
        new_array[i] = MAKE_NONE();                   // fill remaining slots with none
    }
    free(table->array_part);                          // free old array
    table->array_part = new_array;                    // point to new array
    table->array_capacity = new_capacity;             // update capacity
}

// set value at integer index in array part, growing if needed
bool table_set_int(Table* table, int index, Value value) {
    if (index < 0) return false;                 // negative indices not allowed
    if (table->array_part == NULL || index >= table->array_capacity) {
        array_part_grow(table, index);           // grow array to fit index
    }
    value_decref(table->array_part[index]);      // release old value at slot
    table->array_part[index] = value;            // store new value
    value_incref(table->array_part[index]);      // bump refcount for stored value
    if (index >= table->array_count) {
        table->array_count = index + 1;          // update array count if extending
    }
    return true;                                 // success
}

// sets a value in the table by key, with auto-resizing and duplicate detection
bool table_set(Table* table, Value key, Value value) {
    if (IS_NUMBER(key)) {                             // try array part for integer keys
        double num = AS_NUMBER(key);                  // unwrap number
        if (num >= 1 && num <= 2147483647.0 && (int)num == num) {  // positive integer
            int idx = (int)num - 1;                   // convert to 0-based index
            if (table->array_part == NULL) {
                if (idx < TABLE_ARRAY_INIT * 2) return table_set_int(table, idx, value);  // small index, use array
            } else if (idx < table->array_capacity) {
                return table_set_int(table, idx, value);  // fits in current array, use it
            } else {
                int gap = idx - table->array_count;       // gap between requested index and current end
                if (gap <= table->array_count * 2 && gap <= 1024) return table_set_int(table, idx, value);  // small gap, extend array
            }
        }
    }
    if (table->entries == NULL) {
        table->entries = (TableEntry**)calloc(table->capacity, sizeof(TableEntry*));
    }
    if ((double)(table->hash_count + 1) / table->capacity > TABLE_MAX_LOAD) {  // check load factor, resize if needed
        int old_capacity = table->capacity;           // save old capacity
        TableEntry** old_entries = table->entries;    // save old bucket array
        table->capacity = old_capacity * 2;           // double capacity
        table->entries = (TableEntry**)calloc(table->capacity, sizeof(TableEntry*));  // allocate new zeroed buckets
        table->hash_count = 0;                        // reset count, will recount during rehash
        for (int i = 0; i < old_capacity; i++) {
            TableEntry* entry = old_entries[i];       // get head of old bucket chain
            while (entry) {
                TableEntry* next = entry->next;       // save next before re-linking
                uint32_t idx = entry->hash % table->capacity;      // compute new bucket index
                entry->next = table->entries[idx];    // prepend to new bucket chain
                table->entries[idx] = entry;          // update new bucket head
                table->hash_count++;                  // increment count
                entry = next;                         // advance to next old entry
            }
        }
        free(old_entries);                            // free old bucket array
    }
    uint32_t hash = hash_value_key(key);              // compute hash for key
    uint32_t index = hash % table->capacity;          // get bucket index
    TableEntry* entry = table->entries[index];        // start of bucket chain
    while (entry) {
        if (entry->hash == hash && key_equal(entry->key, key)) {  // found existing key
            value_decref(entry->value);               // release old value
            entry->value = value;                     // store new value
            value_incref(entry->value);               // bump refcount for new value
            return true;                              // updated existing entry
        }
        entry = entry->next;                          // advance to next in chain
    }
    entry = (TableEntry*)malloc(sizeof(TableEntry));  // allocate new entry
    entry->key = key;                                 // store key
    value_incref(entry->key);                         // bump refcount for stored key
    entry->hash = hash;                               // store hash for fast comparison
    entry->value = value;                             // store value
    value_incref(entry->value);                       // bump refcount for stored value
    entry->next = table->entries[index];              // prepend to bucket chain
    table->entries[index] = entry;                    // update bucket head
    table->hash_count++;                              // increment hash entry count
    return true;                                      // inserted new entry
}

// retrieves a value from the table by key, returns true if found
bool table_get(Table* table, Value key, Value* out_value) {
    if (!table) return false;                      // guard against null
    if (IS_NUMBER(key)) {                          // try array part for integer keys
        double num = AS_NUMBER(key);               // unwrap number
        if (num >= 1 && num <= 2147483647.0 && (int)num == num) {  // positive integer
            int idx = (int)num - 1;                // convert to 0-based index
            if (table->array_part != NULL && idx < table->array_count) {  // within array bounds
                if (!IS_NONE(table->array_part[idx])) {                   // slot is occupied
                    if (out_value) {
                        *out_value = table->array_part[idx];              // copy value to output
                        value_incref(*out_value);  // bump refcount for caller
                    }
                    return true;                   // found in array part
                }
            }
        }
    }
    if (table->entries == NULL) return false;
    uint32_t hash = hash_value_key(key);           // compute hash for key
    uint32_t index = hash % table->capacity;       // get bucket index
    TableEntry* entry = table->entries[index];     // start of bucket chain
    while (entry) {
        if (entry->hash == hash && key_equal(entry->key, key)) {  // found matching entry
            if (out_value) {
                *out_value = entry->value;         // copy value to output
                value_incref(*out_value);          // bump refcount for caller
            }
            return true;                           // found in hash part
        }
        entry = entry->next;                       // advance to next in chain
    }
    return false;                                  // key not found
}

// removes a key-value pair from the table
void table_remove(Table* table, Value key) {
    if (!table) return;                                  // guard against null
    if (IS_NUMBER(key)) {                                // try array part for integer keys
        double num = AS_NUMBER(key);                     // unwrap number
        if (num >= 1 && num == (int)num && table->array_part != NULL) {  // positive integer with existing array
            int idx = (int)num - 1;                      // convert to 0-based index
            if (idx < table->array_count) {              // within array bounds
                if (!IS_NONE(table->array_part[idx])) {  // slot is occupied
                    value_decref(table->array_part[idx]);                // release old value
                    table->array_part[idx] = MAKE_NONE();                // mark as empty
                    while (table->array_count > 0 &&                     // shrink array count if trailing empty slots
                        IS_NONE(table->array_part[table->array_count - 1])) {
                        table->array_count--;          // decrement count for trailing none slots
                    }
                    return;                            // removed from array part
                }
            }
        }
    }
    if (table->entries == NULL) return;
    uint32_t hash = hash_value_key(key);               // compute hash for key
    uint32_t index = hash % table->capacity;           // get bucket index
    TableEntry* entry = table->entries[index];         // start of bucket chain
    TableEntry* prev = NULL;                           // previous entry for linked list removal
    while (entry) {
        if (entry->hash == hash && key_equal(entry->key, key)) {  // found matching entry
            if (prev) prev->next = entry->next;        // unlink from middle/end of chain
            else table->entries[index] = entry->next;  // unlink from head of chain
            value_decref(entry->key);                  // release key
            value_decref(entry->value);                // release value
            free(entry);                               // free entry struct
            table->hash_count--;                       // decrement hash entry count
            return;                                    // done
        }
        prev = entry;                                  // advance prev
        entry = entry->next;                           // advance to next entry
    }
}

// returns the total number of entries in the table
int table_size(Table* table) {
    if (!table) return 0;                             // guard against null
    int count = table->hash_count;                    // start with hash entry count
    for (int i = 0; i < table->array_count; i++) {
        if (!IS_NONE(table->array_part[i])) count++;  // count occupied array slots
    }
    return count;                                     // return total entries
}

// returns an array of all keys in the table, caller must free and decref each key
Value* table_keys(Table* table, int* out_count) {
    if (!table || !out_count) return NULL;                // guard against null params
    int total = 0;                                        // count occupied array slots
    for (int i = 0; i < table->array_count; i++) {
        if (!IS_NONE(table->array_part[i])) total++;
    }
    total += table->hash_count;                           // add hash entry count
    if (total == 0) { *out_count = 0; return NULL; }      // empty table
    Value* keys = (Value*)malloc(sizeof(Value) * total);  // allocate keys array
    if (!keys) return NULL;                               // allocation failed
    int idx = 0;                                          // insertion index
    for (int i = 0; i < table->array_count; i++) {
        if (!IS_NONE(table->array_part[i])) {
            keys[idx] = MAKE_NUMBER(i + 1);               // store 1-based index as key (unboxed, no incref)
            idx++;
        }
    }
    if (table->entries) {
        for (int i = 0; i < table->capacity; i++) {
            TableEntry* entry = table->entries[i];            // iterate over hash buckets
            while (entry) {
                keys[idx] = entry->key;                       // copy key pointer
                value_incref(keys[idx]);                      // bump refcount for returned key
                idx++;
                entry = entry->next;                          // advance to next in chain
            }
        }
    }
    *out_count = idx;                                     // store total key count
    return keys;                                          // return caller-owned keys array
}

// recursively compares two tables for deep equality
static bool table_equal(Table* a, Table* b, int depth) {
    if (a == b) return true;                           // same pointer, definitely equal
    if (!a || !b) return false;                        // one is null, not equal
    if (table_size(a) != table_size(b)) return false;  // different sizes, not equal
    
    if (depth > 100) return true;                      // assume equal to break cycles

    int key_count;
    Value* keys = table_keys(a, &key_count);           // get all keys from first table
    if (!keys) return table_size(a) == 0 && table_size(b) == 0;  // both empty

    bool result = true;
    for (int i = 0; i < key_count; i++) {
        Value key = keys[i];
        Value val_a, val_b;
        if (!table_get(a, key, &val_a) || !table_get(b, key, &val_b)) {
            result = false;      // key missing in one table
            value_decref(key);
            break;
        }

        // recursively compare values by type
        if (IS_NUMBER(val_a) && IS_NUMBER(val_b)) {
            if (AS_NUMBER(val_a) != AS_NUMBER(val_b)) {
                result = false;  // numbers differ
                value_decref(key);
                break;
            }
        } else if (IS_STRING(val_a) && IS_STRING(val_b)) {
            if (!string_equal(AS_STRING(val_a), AS_STRING(val_b))) {
                result = false;  // strings differ
                value_decref(key);
                break;
            }
        } else if (IS_BOOL(val_a) && IS_BOOL(val_b)) {
            if (AS_BOOL(val_a) != AS_BOOL(val_b)) {
                result = false;  // booleans differ
                value_decref(key);
                break;
            }
        } else if (IS_TABLE(val_a) && IS_TABLE(val_b)) {
            if (!table_equal(AS_TABLE(val_a), AS_TABLE(val_b), depth + 1)) {
                result = false;  // nested tables differ
                value_decref(key);
                break;
            }
        } else if (IS_NONE(val_a) && IS_NONE(val_b)) {
            // both none, equal
        } else {
            result = false;     // different types, not equal
            value_decref(key);
            break;
        }
        
        value_decref(val_a);    // release value references from table_get
        value_decref(val_b);
        value_decref(key);      // release key reference
    }

    free(keys);                 // free keys array
    return result;              // return comparison result
}

// appends a future to the ready queue, taking a reference
static void scheduler_push(VM* vm, FutureObject* fut) {
    if (vm->ready_count >= vm->ready_capacity) {        // need more space
        vm->ready_capacity = vm->ready_capacity == 0 ? 8 : vm->ready_capacity * 2;  // double capacity
        vm->ready = (FutureObject**)realloc(vm->ready,   // resize ready queue
                                            sizeof(FutureObject*) * vm->ready_capacity);
    }
    vm->ready[vm->ready_count++] = fut;                 // append to tail
    value_incref(MAKE_FUTURE(fut));                     // queue holds a reference
}

// pops the next ready future, transfers queue reference to caller
static FutureObject* scheduler_pop(VM* vm) {
    if (vm->ready_count == 0) return NULL;              // nothing to pop
    FutureObject* fut = vm->ready[0];                   // take head
    for (int i = 1; i < vm->ready_count; i++) {         // shift remaining entries down
        vm->ready[i - 1] = vm->ready[i];
    }
    vm->ready_count--;                                  // decrement count
    return fut;                                         // caller now owns the reference
}

// registers a waiter on a pending future
static void future_add_waiter(FutureObject* fut, FutureObject* waiter) {
    if (fut->waiter_count >= fut->waiter_capacity) {    // need more space
        fut->waiter_capacity = fut->waiter_capacity == 0 ? 4 : fut->waiter_capacity * 2;
        fut->waiters = (FutureObject**)realloc(fut->waiters,   // grow waiter array
                                               sizeof(FutureObject*) * fut->waiter_capacity);
    }
    fut->waiters[fut->waiter_count++] = waiter;         // append waiter
    value_incref(MAKE_FUTURE(waiter));                  // waiter array holds a reference
}

// resolves a pending future and queues every waiter as ready
void future_resolve(VM* vm, FutureObject* fut, Value value) {
    if (!fut || fut->state != 0) return;                // already resolved or invalid
    fut->result = value;                                // store result
    if ((value & QNAN) == QNAN) value_incref(value);    // keep reference
    fut->state = 1;                                     // mark resolved
    for (int i = 0; i < fut->waiter_count; i++) {       // wake each waiter
        FutureObject* w = fut->waiters[i];
        if (w->saved_dest_reg >= 0 && w->register_pool != NULL) {  // write awaited value into waiter's own pool
            Value* wregs = &w->register_pool[w->frame_offset[w->current_frame]];
            Value old = wregs[w->saved_dest_reg];
            if ((old & QNAN) == QNAN) value_decref(old);
            wregs[w->saved_dest_reg] = value;
            if ((value & QNAN) == QNAN) value_incref(value);
            w->saved_dest_reg = -1;                     // consumed, no pending await
        }
        value_decref(w->awaiting);                      // release awaited reference
        w->awaiting = MAKE_NONE();                      // no longer waiting
        scheduler_push(vm, w);                          // make runnable
        value_decref(MAKE_FUTURE(w));                   // release waiter-array reference
    }
    fut->waiter_count = 0;                              // clear waiter list
}

// pushes a completed background task for the scheduler to resolve
void vm_push_completion(VM* vm, FutureObject* fut, Value result) {
    Completion* c = (Completion*)malloc(sizeof(Completion));  // allocate node
    c->fut = fut;                                             // transfer future reference
    c->result = result;                                       // transfer result reference
    APEX_MUTEX_LOCK(&vm->completion_mutex);                   // lock queue
    c->next = vm->completions;                                // prepend to list
    vm->completions = c;                                      // publish
    APEX_MUTEX_UNLOCK(&vm->completion_mutex);                 // unlock
}

// drains the completion queue, resolving each future (scheduler thread only)
int vm_drain_completions(VM* vm) {
    APEX_MUTEX_LOCK(&vm->completion_mutex);                   // take entire list
    Completion* list = vm->completions;                       // snapshot head
    vm->completions = NULL;                                   // reset queue
    APEX_MUTEX_UNLOCK(&vm->completion_mutex);                 // unlock

    int count = 0;                                            // number of entries
    while (list) {
        Completion* next = list->next;                        // save next
        future_resolve(vm, list->fut, list->result);          // wake waiters, store result
        value_decref(MAKE_FUTURE(list->fut));                 // release worker's future ref
        if ((list->result & QNAN) == QNAN) {                  // heap-tagged result
            value_decref(list->result);                       // release node's result ref
        }
        free(list);                                           // free node
        list = next;                                          // advance
        count++;
    }
    if (count > 0) {                                          // update pending counter
        APEX_MUTEX_LOCK(&vm->completion_mutex);               // lock
        vm->pending_workers -= count;                         // release worker slots
        if (vm->pending_workers < 0) vm->pending_workers = 0; // clamp to zero
        APEX_MUTEX_UNLOCK(&vm->completion_mutex);             // unlock
    }
    return count;                                             // return drained count
}

// ensures a register frame has enough capacity for the given register index
static bool ensure_register_capacity(VM* vm, int frame_idx, int needed_reg) {
    if (needed_reg < vm->frame_capacity[frame_idx]) {
        if (needed_reg >= vm->frame_used[frame_idx]) {
            vm->frame_used[frame_idx] = needed_reg + 1;  // track highest register used
        }
        return true;                                     // no growth needed
    }
    
    int old_cap = vm->frame_capacity[frame_idx];         // current capacity of this frame
    int new_cap = old_cap;                               // start from current capacity
    if (new_cap == 0) {
        new_cap = REGISTER_INITIAL_SIZE;                 // first allocation for untouched frame
        while (new_cap <= needed_reg) new_cap *= 2;      // double until requested register fits
    } else {
        while (new_cap <= needed_reg) {
            new_cap *= 2;                                // grow existing frame
            if (new_cap > REGISTER_MAX_SIZE) {           // safety cap against runaway growth
                fprintf(stderr, "\033[31mFatal: register frame %d exceeded max size %d "
                        "(requested reg %d)\n\033[0m",
                        frame_idx, REGISTER_MAX_SIZE, needed_reg);
                return false;                            // allocation refused
            }
        }
    }
    
    int total_needed = 0;                                // accumulator for all frames
    for (int f = 0; f < VM_MAX_FRAMES; f++) {
        int cap = (f == frame_idx) ? new_cap : vm->frame_capacity[f];  // use new cap for this frame
        if (cap > 0) total_needed += cap;                // sum all allocated frame capacities
    }
    
    if (total_needed > vm->pool_capacity) {
        int new_pool_cap = vm->pool_capacity;            // current pool capacity
        if (new_pool_cap == 0) new_pool_cap = REGISTER_INITIAL_SIZE;  // initial pool size
        while (new_pool_cap < total_needed) new_pool_cap *= 2;        // double until all frames fit
        
        Value* new_pool = (Value*)realloc(vm->register_pool, new_pool_cap * sizeof(Value));  // resize pool
        if (!new_pool) {                                 // allocation failed
            fprintf(stderr, "\033[31mFatal: failed to grow register pool to %d registers\n\033[0m",
                    new_pool_cap);
            return false;
        }
        for (int i = vm->pool_capacity; i < new_pool_cap; i++) {
            new_pool[i] = MAKE_NONE();                   // fill new portion with none
        }
        vm->register_pool = new_pool;                    // update shared pool pointer
        vm->pool_capacity = new_pool_cap;                // update total pool capacity
    }
    
    vm->frame_capacity[frame_idx] = new_cap;             // store new capacity for this frame
    vm->frame_used[frame_idx] = needed_reg + 1;          // mark registers up to needed_reg as used
    
    int offset = 0;                                      // running offset in the pool
    for (int f = 0; f < VM_MAX_FRAMES; f++) {
        vm->frame_offset[f] = offset;                    // frame f starts at this pool offset
        if (vm->frame_capacity[f] > 0) offset += vm->frame_capacity[f];  // advance past this frame
    }
    
    vm->registers = &vm->register_pool[vm->frame_offset[vm->current_frame]];  // refresh current frame pointer
    
    return true;                                         // frame ready
}

// saves the vm's currently active pool context into a stack-local snapshot
static void vm_context_save(VM* vm, SavedVMContext* s) {
    s->register_pool  = vm->register_pool;
    s->frame_offset   = vm->frame_offset;
    s->frame_capacity = vm->frame_capacity;
    s->frame_used     = vm->frame_used;
    s->pool_capacity  = vm->pool_capacity;
    s->current_frame  = vm->current_frame;
    s->iterator_stack = vm->iterator_stack;
    s->table_iters    = vm->table_iters;
}

// restores a previously saved pool context into the vm's active slot
static void vm_context_restore(VM* vm, SavedVMContext* s) {
    vm->register_pool  = s->register_pool;
    vm->frame_offset   = s->frame_offset;
    vm->frame_capacity = s->frame_capacity;
    vm->frame_used     = s->frame_used;
    vm->pool_capacity  = s->pool_capacity;
    vm->current_frame  = s->current_frame;
    vm->iterator_stack = s->iterator_stack;
    vm->table_iters    = s->table_iters;
    vm->registers      = &vm->register_pool[vm->frame_offset[vm->current_frame]];
}

// installs a coroutine's private pool as the vm's active execution context
static void vm_context_enter(VM* vm, FutureObject* task) {
    vm->register_pool  = task->register_pool;
    vm->frame_offset   = task->frame_offset;
    vm->frame_capacity = task->frame_capacity;
    vm->frame_used     = task->frame_used;
    vm->pool_capacity  = task->pool_capacity;
    vm->current_frame  = task->current_frame;
    vm->registers      = &vm->register_pool[vm->frame_offset[vm->current_frame]];

    vm->current_task   = task;                         // enter coroutine mode
    vm->call_depth     = 0;                            // coroutine body starts at depth 0
    vm->iterator_stack = task->saved_iters;            // point at coroutine's numeric loop storage
    vm->iterator_depth = task->saved_iter_depth;       // restore numeric loop depth
    vm->table_iters    = task->saved_table_iters;      // point at coroutine's table iterator storage
    vm->table_iter_depth = task->saved_table_iter_depth;  // restore table iterator depth
}

// captures the vm's active pool state back into a coroutine after a slice
static void vm_context_capture(VM* vm, FutureObject* task) {
    task->register_pool  = vm->register_pool;          // save (possibly reallocated) pool
    task->frame_offset   = vm->frame_offset;
    task->frame_capacity = vm->frame_capacity;
    task->frame_used     = vm->frame_used;
    task->pool_capacity  = vm->pool_capacity;
    task->current_frame  = vm->current_frame;
    task->saved_iter_depth = vm->iterator_depth;       // save numeric loop depth
    task->saved_table_iter_depth = vm->table_iter_depth;  // save table iterator depth
    vm->iterator_stack = vm->top_level_iter_storage;   // restore top-level numeric loop storage
    vm->table_iters    = vm->top_level_table_iter_storage;  // restore top-level table iterator storage
    vm->current_task = NULL;                           // leave coroutine mode
}

// starts a pending future as a coroutine and queues it for execution
void future_start(VM* vm, FutureObject* fut) {
    if (fut->register_pool != NULL) return;             // already started
    if (fut->func_idx < 0) return;                      // leaf future, no body to run
    int needed = vm->chunk->functions[fut->func_idx].max_registers;
    if (needed < REGISTER_INITIAL_SIZE) needed = REGISTER_INITIAL_SIZE;
    if (needed < fut->arg_count) needed = fut->arg_count;

    fut->register_pool  = (Value*)malloc(sizeof(Value) * needed);   // private register pool
    fut->frame_offset   = (int*)calloc(VM_MAX_FRAMES, sizeof(int)); // private frame offsets
    fut->frame_capacity = (int*)calloc(VM_MAX_FRAMES, sizeof(int)); // private frame capacities
    fut->frame_used     = (int*)calloc(VM_MAX_FRAMES, sizeof(int)); // private usage counters
    if (!fut->register_pool || !fut->frame_offset ||
        !fut->frame_capacity || !fut->frame_used) {                 // allocation failed
        free(fut->register_pool);  fut->register_pool  = NULL;
        free(fut->frame_offset);   fut->frame_offset   = NULL;
        free(fut->frame_capacity); fut->frame_capacity = NULL;
        free(fut->frame_used);     fut->frame_used     = NULL;
        return;                                                     // give up quietly
    }
    fut->pool_capacity  = needed;                       // total capacity for this pool
    fut->current_frame  = 0;                            // body frame is index 0
    fut->frame_offset[0]   = 0;                         // body frame starts at pool offset 0
    fut->frame_capacity[0] = needed;                    // body frame capacity
    fut->frame_used[0]     = needed;                    // body frame fully used
    for (int i = 0; i < needed; i++) fut->register_pool[i] = MAKE_NONE();  // clear pool

    for (int i = 0; i < fut->arg_count; i++) {          // copy captured args into body frame
        fut->register_pool[i] = fut->args[i];
        if ((fut->args[i] & QNAN) == QNAN) value_incref(fut->args[i]);
    }
    fut->owns_frame = true;                             // pool now owned by this future
    fut->saved_ip = vm->chunk->functions[fut->func_idx].address;  // start of body
    fut->saved_dest_reg = -1;                           // no pending await
    fut->saved_iter_depth = -1;                         // no active loops
    fut->saved_table_iter_depth = -1;
    scheduler_push(vm, fut);                            // make it ready
}

// registers a timer that resolves the future after the given seconds
void vm_schedule_timer(VM* vm, double seconds, FutureObject* fut) {
    SleepTimer* t = (SleepTimer*)malloc(sizeof(SleepTimer));  // allocate timer node
    t->deadline = apex_now_seconds() + seconds;         // compute absolute deadline
    t->fut = fut;                                       // store future
    value_incref(MAKE_FUTURE(fut));                     // timer holds a reference
    t->next = vm->timers;                               // prepend to list
    vm->timers = t;                                     // update head
}

// fires all timers whose deadlines have passed
static bool poll_timers(VM* vm) {
    double now = apex_now_seconds();                    // current time
    bool fired = false;                                 // whether anything fired
    SleepTimer** pp = &vm->timers;                      // walk pointer chain
    while (*pp) {
        SleepTimer* t = *pp;                            // current timer
        if (t->deadline <= now) {                       // deadline reached
            *pp = t->next;                              // unlink
            future_resolve(vm, t->fut, MAKE_NONE());    // resolve with none
            value_decref(MAKE_FUTURE(t->fut));          // release timer's reference
            free(t);                                    // free node
            fired = true;                               // mark fired
        } else {
            pp = &t->next;                              // advance
        }
    }
    return fired;                                       // return status
}

// sleeps the thread until the earliest pending timer is due
static void wait_for_next_timer(VM* vm) {
    if (!vm->timers) {                              // nothing to wait on
        if (vm->pending_workers > 0) {              // but workers are running
#ifdef _WIN32
            Sleep(1);                               // short poll to check completions
#else
            usleep(1000);                           // short poll to check completions
#endif
        }
        return;
    }
    double earliest = vm->timers->deadline;         // scan for earliest deadline
    for (SleepTimer* t = vm->timers->next; t; t = t->next) {
        if (t->deadline < earliest) earliest = t->deadline;
    }
    double wait = earliest - apex_now_seconds();    // compute delay
    if (wait <= 0) return;                          // already due
    if (wait > 0.01) wait = 0.01;                   // cap so completions stay responsive
#ifdef _WIN32
    Sleep((DWORD)(wait * 1000));                    // windows sleep
#else
    usleep((useconds_t)(wait * 1000000));           // posix sleep
#endif
}

// drives the scheduler until the target future resolves
bool vm_drive_until(VM* vm, Value target, Value* out_result) {
    if (!IS_FUTURE(target)) {                           // non-future passthrough
        *out_result = target;
        if ((target & QNAN) == QNAN) value_incref(target);
        return true;
    }
    FutureObject* fut = AS_FUTURE(target);

    SavedVMContext saved_ctx;                           // snapshot of caller's pool
    vm_context_save(vm, &saved_ctx);

    int saved_iter_depth = vm->iterator_depth;          // save caller numeric loop depth
    int saved_titer = vm->table_iter_depth;             // save caller table iterator depth

    FutureObject* saved_task = vm->current_task;        // save caller's task mode

    if (fut->state == 0 && fut->register_pool == NULL) future_start(vm, fut);  // start target if pending

    while (fut->state == 0) {                           // drive until resolved
        if (vm->had_error) break;                       // abort scheduler on VM error
        bool ran = false;                               // did we run a slice?
        while (vm->ready_count > 0) {                   // drain ready queue
            FutureObject* task = scheduler_pop(vm);     // take next task
            ran = true;                                 // mark progress

            vm_context_enter(vm, task);                 // install task's private pool
            vm_execute(vm, vm->chunk);                  // run one slice until suspend or return
            vm_context_capture(vm, task);               // save task's pool state back

            vm_context_restore(vm, &saved_ctx);         // back to caller's pool between tasks
            value_decref(MAKE_FUTURE(task));            // drop queue reference
        }
        if (fut->state != 0) break;                     // target resolved
        if (vm->had_error) break;                       // VM error

        if (vm_drain_completions(vm) > 0) continue;     // finished workers woke futures

        if (vm->timers || vm->pending_workers > 0) {    // wait on timers or workers
            wait_for_next_timer(vm);
            poll_timers(vm);
        } else if (!ran) {
            break;                                      // nothing to do, deadlock
        }
    }

    vm_context_restore(vm, &saved_ctx);                 // restore caller's pool
    vm->current_task = saved_task;                      // restore caller's task mode
    vm->iterator_depth = saved_iter_depth;
    vm->table_iter_depth = saved_titer;

    if (fut->state == 1) {                              // resolved
        *out_result = fut->result;
        if ((fut->result & QNAN) == QNAN) value_incref(fut->result);
        return true;
    }
    *out_result = MAKE_NONE();                          // deadlock or error
    return false;
}

// creates a new vm instance with the given source code
VM* vm_create(const char* source) {
    VM* vm = (VM*)calloc(1, sizeof(VM));           // allocate and zero vm struct
    if (!vm) return NULL;                          // allocation failed
    
    vm->frame_offset = (int*)calloc(VM_MAX_FRAMES, sizeof(int));    // frame start offsets in pool
    vm->frame_capacity = (int*)calloc(VM_MAX_FRAMES, sizeof(int));  // frame capacities in registers
    vm->frame_used = (int*)calloc(VM_MAX_FRAMES, sizeof(int));      // highest register used per frame
    if (!vm->frame_offset || !vm->frame_capacity || !vm->frame_used) {
        free(vm->frame_offset);
        free(vm->frame_capacity);
        free(vm->frame_used);
        free(vm);
        return NULL;
    }
    
    if (!ensure_register_capacity(vm, 0, REGISTER_INITIAL_SIZE - 1)) {
        free(vm->frame_offset);
        free(vm->frame_capacity);
        free(vm->frame_used);
        free(vm->register_pool);               
        free(vm);
        return NULL;
    }
    
    vm->registers = vm->register_pool;            // fast pointer to frame 0 (offset 0)
    vm->current_frame = 0;                        // start at frame 0
    vm->global_count = 0;                         // no globals set yet
    vm->call_depth = 0;                           // no active calls
    vm->iterator_stack = vm->top_level_iter_storage;  // point at top-level numeric loop storage
    vm->iterator_depth = -1;                      // no active iterators
    vm->table_iters = vm->top_level_table_iter_storage;  // point at top-level table iterator storage
    vm->table_iter_depth = -1;                    // no active table iterators
    vm->running = false;                          // not running yet
    vm->had_error = false;                        // no errors yet
    vm->args_top = 0;                             // empty args stack
    vm->args_table = MAKE_NONE();                 // default to none until set
    for (int i = 0; i < VM_MAX_FRAMES; i++) vm->frame_futures[i] = MAKE_NONE();  // no frame owns a future yet
    vm->source = source;                          // store source pointer

    vm->ready = NULL;                             // no ready queue yet
    vm->ready_count = 0;                          // empty queue
    vm->ready_capacity = 0;                       // no capacity
    vm->current_task = NULL;                      // no active coroutine
    vm->timers = NULL;                            // no pending timers

    APEX_MUTEX_INIT(&vm->completion_mutex);       // init completion lock
    vm->completions = NULL;                       // no pending completions
    vm->pending_workers = 0;                      // no live workers

    string_intern_table_init(&vm->intern_table);  // init string intern table
    return vm;                                    // return new vm
}

// populates vm->args_table with user command line arguments only (1-indexed)
void vm_set_args(VM* vm, int argc, char** argv, bool skip_script_name) {
    int start = skip_script_name ? 2 : 1;                            // skip interpreter+script or just binary name
    int user_argc = argc > start ? argc - start : 0;                 // count of user-supplied args
    Table* t = table_create(user_argc > 8 ? user_argc : 8);          // allocate table with sufficient capacity
    vm->args_table = MAKE_TABLE(t);                                  // store as tagged table value

    for (int i = start; i < argc; i++) {                             // iterate over user args
        Value key = MAKE_NUMBER((double)(i - start + 1));            // 1-based index

        int len = (int)strlen(argv[i]);                              // compute argument length
        StringObject* str = string_intern(&vm->intern_table, argv[i], len);  // intern argument string
        Value val = MAKE_STRING(str);                                // box interned string pointer

        table_set(t, key, val);                                      // insert into args table
        value_decref(val);                                           // release local reference
    }
}

// destroys a vm and frees all resources
void vm_destroy(VM* vm) {
    if (!vm) return;                              // guard against null
    value_decref(vm->args_table);                 // release args table and all contained strings

    // wait for any live background worker threads to finish before teardown
    while (1) {
        APEX_MUTEX_LOCK(&vm->completion_mutex);   // read counter under lock
        int pending = vm->pending_workers;        // snapshot
        APEX_MUTEX_UNLOCK(&vm->completion_mutex); // release lock
        if (pending <= 0) break;                  // no workers left
        vm_drain_completions(vm);                 // discard finished results
#ifdef _WIN32
        Sleep(1);                                 // short poll interval
#else
        usleep(1000);                             // short poll interval
#endif
    }
    vm_drain_completions(vm);                     // final sweep

    // drain ready queue before touching top-level pool
    for (int i = 0; i < vm->ready_count; i++) {
        value_decref(MAKE_FUTURE(vm->ready[i]));  // release each queued future
    }
    free(vm->ready);                              // free ready queue array
    vm->ready = NULL;
    vm->ready_count = 0;

    // drain pending timers
    SleepTimer* t = vm->timers;                   // walk timer list
    while (t) {                                   // free every remaining timer
        SleepTimer* n = t->next;                  // save next before freeing
        value_decref(MAKE_FUTURE(t->fut));        // release timer's reference
        free(t);                                  // free timer node
        t = n;                                    // advance
    }
    vm->timers = NULL;

    int total_regs = 0;
    for (int f = 0; f < VM_MAX_FRAMES; f++) {
        if (vm->frame_capacity[f] > 0) total_regs += vm->frame_capacity[f];  // sum all frame capacities
    }
    for (int i = 0; i < total_regs && i < vm->pool_capacity; i++) {
        value_decref(vm->register_pool[i]);       // release each register value
    }
    free(vm->register_pool);                      // free the shared register pool
    vm->register_pool = NULL;
    free(vm->frame_offset);                       // free frame offset array
    free(vm->frame_capacity);                     // free frame capacity array
    free(vm->frame_used);                         // free frame used array
    vm->frame_offset = NULL;
    vm->frame_capacity = NULL;
    vm->frame_used = NULL;

    for (int i = 0; i < vm->global_count; i++) {
        value_decref(vm->globals[i]);             // release each global
    }
    for (int i = 0; i < vm->args_top; i++) {
        value_decref(vm->args_stack[i]);          // release any remaining args
    }

    APEX_MUTEX_DESTROY(&vm->completion_mutex);    // destroy lock
    string_intern_table_free(&vm->intern_table);  // free interned strings
#if APEX_JIT_ENABLED
    jit_destroy(vm->jit);                         // release JIT and its code page
#endif
    free(vm);                                     // free vm struct
}

// dispatches built-in function calls to module-specific handlers
static bool vm_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strncmp(name, "os.", 3) == 0) return os_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "sys.", 4) == 0) return sys_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "math.", 5) == 0) return math_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "string.", 7) == 0) return string_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "table.", 6) == 0) return table_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "random.", 7) == 0) return random_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "json.", 5) == 0) return json_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "xml.", 4) == 0) return xml_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "csv.", 4) == 0) return csv_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "base.", 5) == 0) return base_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "regex.", 6) == 0) return regex_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "crypto.", 7) == 0) return crypto_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "zip.", 4) == 0) return zip_call_builtin(vm, name, arg_count, args, result);
    if (strncmp(name, "network.", 8) == 0) return network_call_builtin(vm, name, arg_count, args, result);

    if (strcmp(name, "number") == 0) {           // builtin: number(value) -> number
        if (arg_count >= 1) {
            if (IS_STRING(args[0])) {            // convert string to number
                char* endptr;
                double val = strtod(AS_STRING(args[0])->chars, &endptr);       // parse double from string
                if (endptr == AS_STRING(args[0])->chars || *endptr != '\0') {  // parse failed
                    *result = MAKE_NONE();       // return none on invalid input
                } else {
                    *result = MAKE_NUMBER(val);  // return parsed number
                }
            } else if (IS_NUMBER(args[0])) {     // already a number
                *result = args[0];               // return as is
                value_incref(*result);           // bump refcount for caller
            } else {
                *result = MAKE_NONE();           // unsupported type, return none
            }
        } else {
            *result = MAKE_NONE();               // no args, return none
        }
        return true;                             // handled
    }
    if (strcmp(name, "string") == 0) {           // builtin: string(value) -> string
        if (arg_count >= 1) {
            char buffer[256];                    // temp buffer for number conversion
            if (IS_NUMBER(args[0])) {
                snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(args[0]));   // format number to string
                *result = MAKE_STRING(string_intern(&vm->intern_table, buffer, strlen(buffer)));  // intern result
            } else if (IS_NONE(args[0])) {
                *result = MAKE_STRING(string_intern(&vm->intern_table, "none", 4));  // intern "none"
            } else if (IS_BOOL(args[0])) {
                *result = MAKE_STRING(string_intern(&vm->intern_table, AS_BOOL(args[0]) ? "true" : "false", AS_BOOL(args[0]) ? 4 : 5));  // intern bool string
            } else if (IS_STRING(args[0])) {
                *result = args[0];               // already a string, return as is
                value_incref(*result);           // bump refcount for caller
            } else if (IS_TABLE(args[0])) {
                char* table_str = table_to_string(AS_TABLE(args[0]));  // convert table to string
                *result = MAKE_STRING(string_intern(&vm->intern_table, table_str, strlen(table_str)));  // intern result
                free(table_str);                 // free temp string
            } else {
                *result = MAKE_NONE();           // unsupported type, return none
            }
        }
        return true;                             // handled
    }
    if (strcmp(name, "type") == 0) {             // builtin: type(value) -> string
        if (arg_count >= 1) {
            const char* type_name = vm_value_type_name(args[0]);       // get type name string
            *result = MAKE_STRING(string_intern(&vm->intern_table, type_name, strlen(type_name)));  // intern type name
        } else {
            *result = MAKE_NONE();               // no args, return none
        }
        return true;                             // handled
    }
    return false;                                // not a recognized builtin
}

#if APEX_JIT_ENABLED
// wraps a raw jit return value into a tagged vm value based on the function's return kind
static inline Value jit_box_return(JITContext* jit, int func_idx, double r) {
    switch (jit_return_type(jit, func_idx)) {
        case JIT_RET_BOOL: return MAKE_BOOL(r != 0.0);           // bool tag
        case JIT_RET_NONE: return MAKE_NONE();                   // none tag, r ignored
        default:           return MAKE_NUMBER(r);                // plain number
    }
}
#endif

// main execution loop with direct threaded dispatch for performance
bool vm_execute(VM* vm, BytecodeChunk* chunk) {
    if (!vm || !chunk) return false;                      // validate arguments
    vm->chunk = chunk;                                    // store bytecode chunk
    vm->code = chunk->code;                               // pointer to instruction array
    vm->code_count = chunk->code_count;                   // total instruction count
    vm->running = true;                                   // mark vm as running
    vm->had_error = false;                                // reset error flag
    bool top_level = (vm->current_task == NULL);          // entering top-level or resuming a coroutine
    if (top_level) {                                      // top-level-only setup
        vm->global_count = chunk->global_count;           // number of globals to initialise
#if APEX_JIT_ENABLED
        if (apex_jit_runtime_enabled) {
            vm->jit = jit_create(chunk);                  // compile numeric-pure functions
            if (vm->jit) jit_set_vm(vm->jit, vm);         // back-pointer for numeric-for reseed
        } else {
            vm->jit = NULL;                               // no JIT context at all
        }
#endif

        int needed_regs = chunk->functions[0].max_registers;  // registers needed by main function
        if (needed_regs < REGISTER_INITIAL_SIZE) {
            needed_regs = REGISTER_INITIAL_SIZE;              // enforce minimum frame size
        }
        if (!ensure_register_capacity(vm, 0, needed_regs)) {  // allocate or grow frame 0
            vm->had_error = true;                             // allocation failed
            vm->running = false;                              // stop execution
            return false;                                     // bail out
        }

        for (int i = 0; i < chunk->global_count; i++) {
            vm->globals[i] = MAKE_NONE();                     // initialise each global slot
        }

        for (int i = 0; i < chunk->const_count; i++) {               // pre-intern string constants for fast comparison
            Constant* c = &chunk->constants[i];                      // current constant
            if (c->type == CONST_STRING && c->cached_str == NULL) {  // string not yet interned
                int len = (int)strlen(c->string_value);              // compute length once at load time
                c->cached_str = string_intern(&vm->intern_table,     // intern into vm's table, immortal object
                                              c->string_value, len);
            }
        }
    }
    
    static void* dispatch_table[] = {
        [OP_MOVE]             = &&OP_MOVE_LABEL,
        [OP_LOAD_CONST]       = &&OP_LOAD_CONST_LABEL,
        [OP_LOAD_NUM_IMM]     = &&OP_LOAD_NUM_IMM_LABEL,
        [OP_LOAD_NUM]         = &&OP_LOAD_NUM_LABEL,
        [OP_LOAD_BOOL]        = &&OP_LOAD_BOOL_LABEL,
        [OP_LOAD_NONE]        = &&OP_LOAD_NONE_LABEL,

        [OP_ADD]              = &&OP_ADD_LABEL,
        [OP_SUB]              = &&OP_SUB_LABEL,
        [OP_MUL]              = &&OP_MUL_LABEL,
        [OP_DIV]              = &&OP_DIV_LABEL,
        [OP_MOD]              = &&OP_MOD_LABEL,
        [OP_NEG]              = &&OP_NEG_LABEL,
        [OP_INC]              = &&OP_INC_LABEL,
        [OP_DEC]              = &&OP_DEC_LABEL,

        [OP_JUMP]             = &&OP_JUMP_LABEL,
        [OP_JUMP_IF_FALSE]    = &&OP_JUMP_IF_FALSE_LABEL,
        [OP_JUMP_IF_EQ]       = &&OP_JUMP_IF_EQ_LABEL,
        [OP_JUMP_IF_NEQ]      = &&OP_JUMP_IF_NEQ_LABEL,
        [OP_JUMP_IF_EQ_NUM]   = &&OP_JUMP_IF_EQ_NUM_LABEL,
        [OP_JUMP_IF_NEQ_NUM]  = &&OP_JUMP_IF_NEQ_NUM_LABEL,
        [OP_JUMP_IF_LT]       = &&OP_JUMP_IF_LT_LABEL,
        [OP_JUMP_IF_GT]       = &&OP_JUMP_IF_GT_LABEL,
        [OP_JUMP_IF_LTE]      = &&OP_JUMP_IF_LTE_LABEL,
        [OP_JUMP_IF_GTE]      = &&OP_JUMP_IF_GTE_LABEL,

        [OP_JUMP_MATCH_NUM]   = &&OP_JUMP_MATCH_NUM_LABEL,
        [OP_JUMP_MATCH_STR]   = &&OP_JUMP_MATCH_STR_LABEL,
        [OP_JUMP_MATCH_BOOL]  = &&OP_JUMP_MATCH_BOOL_LABEL,
        [OP_JUMP_MATCH_NONE]  = &&OP_JUMP_MATCH_NONE_LABEL,

        [OP_CMP_EQ]           = &&OP_CMP_EQ_LABEL,
        [OP_CMP_NEQ]          = &&OP_CMP_NEQ_LABEL,
        [OP_CMP_EQ_NUM]       = &&OP_CMP_EQ_NUM_LABEL,
        [OP_CMP_NEQ_NUM]      = &&OP_CMP_NEQ_NUM_LABEL,
        [OP_CMP_LT]           = &&OP_CMP_LT_LABEL,
        [OP_CMP_GT]           = &&OP_CMP_GT_LABEL,
        [OP_CMP_LTE]          = &&OP_CMP_LTE_LABEL,
        [OP_CMP_GTE]          = &&OP_CMP_GTE_LABEL,
        
        [OP_FOR_INIT]         = &&OP_FOR_INIT_LABEL,
        [OP_FOR_NEXT]         = &&OP_FOR_NEXT_LABEL,
        [OP_TABLE_ITER_INIT]  = &&OP_TABLE_ITER_INIT_LABEL,
        [OP_TABLE_ITER_NEXT]  = &&OP_TABLE_ITER_NEXT_LABEL,
        [OP_POP_ITER]         = &&OP_POP_ITER_LABEL,

        [OP_TABLE_GET]        = &&OP_TABLE_GET_LABEL,
        [OP_TABLE_GET_CONST]  = &&OP_TABLE_GET_CONST_LABEL,
        [OP_TABLE_GET_INT]    = &&OP_TABLE_GET_INT_LABEL,
        [OP_TABLE_SET]        = &&OP_TABLE_SET_LABEL,
        [OP_TABLE_SET_CONST]  = &&OP_TABLE_SET_CONST_LABEL,
        [OP_TABLE_SET_INT]    = &&OP_TABLE_SET_INT_LABEL,
        [OP_TABLE_APPEND]     = &&OP_TABLE_APPEND_LABEL,
        [OP_NEW_TABLE]        = &&OP_NEW_TABLE_LABEL,
        
        [OP_CONCAT]           = &&OP_CONCAT_LABEL,

        [OP_AND]              = &&OP_AND_LABEL,
        [OP_OR]               = &&OP_OR_LABEL,
        [OP_NOT]              = &&OP_NOT_LABEL,
        
        [OP_PUSH_ARG]         = &&OP_PUSH_ARG_LABEL,
        [OP_CALL]             = &&OP_CALL_LABEL,
        [OP_CALL_BUILTIN]     = &&OP_CALL_BUILTIN_LABEL,
        [OP_CALL_0]           = &&OP_CALL_0_LABEL,
        [OP_CALL_1]           = &&OP_CALL_1_LABEL,
        [OP_CALL_2]           = &&OP_CALL_2_LABEL,
        [OP_RETURN]           = &&OP_RETURN_LABEL,
        [OP_RETURN_NUM]       = &&OP_RETURN_NUM_LABEL,
        [OP_RETURN_BOOL]      = &&OP_RETURN_BOOL_LABEL,
        [OP_RETURN_NONE]      = &&OP_RETURN_NONE_LABEL,

        [OP_AWAIT]            = &&OP_AWAIT_LABEL,
        [OP_ASYNC_CALL]       = &&OP_ASYNC_CALL_LABEL,

        [OP_LOAD_GLOBAL]      = &&OP_LOAD_GLOBAL_LABEL,
        [OP_STORE_GLOBAL]     = &&OP_STORE_GLOBAL_LABEL,
        
        [OP_HALT]             = &&OP_HALT_LABEL,
    };
#if APEX_JIT_ENABLED
    #define APEX_TRY_JIT_LOOP() \
        do { \
            if (unlikely(apex_jit_runtime_enabled && vm->jit != NULL)) { \
                int _cur = (int)(ip - vm->code); \
                int _exit; \
                JitLoopResult _r = jit_try_native_loop(vm->jit, _cur, (uint64_t*)regs, &_exit); \
                if (_r == JIT_LOOP_RAN_NORMAL) { \
                    ip = &vm->code[_exit]; \
                    goto *dispatch_table[ip->opcode]; \
                } \
                if (_r == JIT_LOOP_RAN_FOR_NEXT) { \
                    vm->iterator_depth--; \
                    ip = &vm->code[_exit]; \
                    goto *dispatch_table[ip->opcode]; \
                } \
                if (_r == JIT_LOOP_RAN_TABLE_ITER) { \
                    vm->table_iter_depth--; \
                    ip = &vm->code[_exit]; \
                    goto *dispatch_table[ip->opcode]; \
                } \
            } \
        } while (0)
#else
    #define APEX_TRY_JIT_LOOP() ((void)0)
#endif
    register Instruction* ip = top_level ? vm->code : &vm->code[vm->current_task->saved_ip];  // resume point or entry
    register Value* regs = vm->registers;  // current frame registers in a register for speed
    register int* frame_cap = vm->frame_capacity;  // cached frame capacity array (stable per context)
    register int* frame_off = vm->frame_offset;    // cached frame offset array (may be rebuilt on growth)
    __builtin_prefetch(ip + 1, 0, 1);      // hint cpu to prefetch next instruction
    goto *dispatch_table[ip->opcode];      // jump to first opcode handler

#define RESOLVE_FRAME_FUTURE(rv) do { \
    Value _fv = vm->frame_futures[vm->current_frame]; \
    if (_fv != MAKE_NONE()) { \
        FutureObject* _f = AS_FUTURE(_fv); \
        value_decref(_f->result); \
        _f->result = (rv); \
        if (((rv) & QNAN) == QNAN) value_incref(rv); \
        _f->state = 1; \
        value_decref(_fv); \
        vm->frame_futures[vm->current_frame] = MAKE_NONE(); \
    } \
} while (0)

    OP_MOVE_LABEL: {
        int dest = ip->operands[0];              // dest register index
        int src = ip->operands[1];               // source register index
        Value sv = regs[src];                    // read source value
        Value old = regs[dest];                  // read old dest value
        if ((old & QNAN) == QNAN) {
            value_decref(old);                   // release old heap object
        }
        regs[dest] = sv;                         // copy source to dest
        if ((sv & QNAN) == QNAN) {
            value_incref(sv);                    // bump refcount for stored reference
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
                regs[dest] = MAKE_STRING((StringObject*)c->cached_str);  // use pre-interned string
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
    OP_LOAD_NUM_IMM_LABEL: {
        int dest = ip->operands[0];                    // dest register index
        int value = ip->operands[1];                   // immediate integer value (0-65535)
        Value old = regs[dest];                        // read current value in dest register
        if (unlikely((old & QNAN) == QNAN)) {         // fast nan-boxing check: only nan-tagged values
            value_decref(old);                         // are heap objects needing refcount cleanup
        }                                              // unboxed numbers skip this branch entirely
        regs[dest] = MAKE_NUMBER((double)value);       // store unboxed number, no incref needed
        ip++; goto *dispatch_table[ip->opcode];        // advance to next instruction
    }
    OP_LOAD_NUM_LABEL: {
        int dest = ip->operands[0];                    // dest register index
        int const_idx = ip->operands[1];               // constant pool index for double value
        double value = chunk->constants[const_idx].number_value;  // fetch double from constant pool
        Value old = regs[dest];                        // read current value in dest register
        if (unlikely((old & QNAN) == QNAN)) {         // fast nan-boxing check: only nan-tagged values
            value_decref(old);                         // are heap objects needing refcount cleanup
        }                                              // unboxed numbers skip this branch entirely
        regs[dest] = MAKE_NUMBER(value);               // store unboxed double, no incref needed
        ip++; goto *dispatch_table[ip->opcode];        // advance to next instruction
    }
    OP_LOAD_BOOL_LABEL: {
        int dest = ip->operands[0];                    // dest register index
        Value old = regs[dest];                        // read current value in dest register
        if (unlikely((old & QNAN) == QNAN)) {          // heap objects are less common
            value_decref(old);                         // are heap objects needing refcount cleanup
        }                                              // unboxed numbers and other immediates skip this
        regs[dest] = MAKE_BOOL(ip->operands[1] != 0);  // store new bool value
        ip++; goto *dispatch_table[ip->opcode];        // advance to next instruction
    }
    OP_LOAD_NONE_LABEL: {
        int dest = ip->operands[0];                    // dest register index
        Value old = regs[dest];                        // read current value in dest register
        if (unlikely((old & QNAN) == QNAN)) {          // heap objects are less common
            value_decref(old);                         // are heap objects needing refcount cleanup
        }                                              // unboxed numbers and other immediates skip this
        regs[dest] = MAKE_NONE();                      // store none value (no incref needed, immediate)
        ip++; goto *dispatch_table[ip->opcode];        // advance to next instruction
    }

    OP_ADD_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // right operand as double
        double r = a.d + b.d;                    // perform addition
        uint64_t old = regs[dest];               // save old value for decref

        if (unlikely(r != r)) {                  // nan result (rare)
            value_decref(old);                   // release heap object if any
            regs[dest] = MAKE_NONE();            // store none
        } else {
            if (unlikely((old & QNAN) == QNAN)) {  // fast check: old is nan-boxed
                value_decref(old);               // release heap object
            }
            regs[dest] = MAKE_NUMBER(r);         // store number
        }
        
        ip++; goto *dispatch_table[ip->opcode];  // next instruction
    }
    OP_SUB_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // right operand as double
        double r = a.d - b.d;                    // perform subtraction
        uint64_t old = regs[dest];               // save old value for decref

        if (unlikely(r != r)) {                  // nan result (rare)
            value_decref(old);                   // release heap object if any
            regs[dest] = MAKE_NONE();            // store none
        } else {
            if (unlikely((old & QNAN) == QNAN)) {  // fast check: old is nan-boxed
                value_decref(old);               // release heap object
            }
            regs[dest] = MAKE_NUMBER(r);         // store number
        }
        
        ip++; goto *dispatch_table[ip->opcode];  // next instruction
    }
    OP_MUL_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // right operand as double
        double r = a.d * b.d;                    // perform multiplication
        uint64_t old = regs[dest];               // save old value for decref

        if (unlikely(r != r)) {                  // nan result (rare)
            value_decref(old);                   // release heap object if any
            regs[dest] = MAKE_NONE();            // store none
        } else {
            if (unlikely((old & QNAN) == QNAN)) {  // fast check: old is nan-boxed
                value_decref(old);               // release heap object
            }
            regs[dest] = MAKE_NUMBER(r);         // store number
        }
        
        ip++; goto *dispatch_table[ip->opcode];  // next instruction
    }
    OP_DIV_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // right operand as double
        double r = a.d / b.d;                    // perform division
        uint64_t old = regs[dest];               // save old value for decref

        if (unlikely(r != r)) {                  // nan result (rare)
            value_decref(old);                   // release heap object if any
            regs[dest] = MAKE_NONE();            // store none
        } else {
            if (unlikely((old & QNAN) == QNAN)) {  // fast check: old is nan-boxed
                value_decref(old);               // release heap object
            }
            regs[dest] = MAKE_NUMBER(r);         // store number
        }
        
        ip++; goto *dispatch_table[ip->opcode];  // next instruction
    }
    OP_MOD_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // left operand as double
        du64 b = {.u = regs[ip->operands[2]]};   // right operand as double
        double r = fmod(a.d, b.d);               // perform modulo
        uint64_t old = regs[dest];               // save old value for decref

        if (unlikely(r != r)) {                  // nan result (rare)
            value_decref(old);                   // release heap object if any
            regs[dest] = MAKE_NONE();            // store none
        } else {
            if (unlikely((old & QNAN) == QNAN)) {  // fast check: old is nan-boxed
                value_decref(old);               // release heap object
            }
            regs[dest] = MAKE_NUMBER(r);         // store number
        }
        
        ip++; goto *dispatch_table[ip->opcode];  // next instruction
    }
    OP_NEG_LABEL: {
        int dest = ip->operands[0];                                   // dest register index
        uint64_t old = regs[dest];                                    // save old value for decref
        if (unlikely((old & QNAN) == QNAN)) {                         // fast check: old is nan-boxed
            value_decref(old);                                        // release heap object
        }
        regs[dest] = MAKE_NUMBER(-AS_NUMBER(regs[ip->operands[1]]));  // negate and store as unboxed number
        ip++; goto *dispatch_table[ip->opcode];                       // advance to next instruction
    }
    OP_INC_LABEL: {
        int reg_idx = ip->operands[0];               // register index to increment
        du64 a = {.u = regs[reg_idx]};               // reinterpret current value as double via union
        double r = a.d + 1.0;                        // increment by 1 (NaN for non-numbers)
        uint64_t old = regs[reg_idx];                // save old value for decref
        
        if (unlikely(r != r)) {                      // nan result (non-number input)
            value_decref(old);                       // release heap object if any
            regs[reg_idx] = MAKE_NONE();             // store none
        } else {
            if (unlikely((old & QNAN) == QNAN)) {    // old is nan-boxed heap object
                value_decref(old);                   // release heap object
            }
            regs[reg_idx] = MAKE_NUMBER(r);          // store incremented number
        }
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
    }
    OP_DEC_LABEL: {
        int reg_idx = ip->operands[0];               // register index to decrement
        du64 a = {.u = regs[reg_idx]};               // reinterpret current value as double via union
        double r = a.d - 1.0;                        // decrement by 1 (NaN for non-numbers)
        uint64_t old = regs[reg_idx];                // save old value for decref
        
        if (unlikely(r != r)) {                      // nan result (non-number input)
            value_decref(old);                       // release heap object if any
            regs[reg_idx] = MAKE_NONE();             // store none
        } else {
            if (unlikely((old & QNAN) == QNAN)) {    // old is nan-boxed heap object
                value_decref(old);                   // release heap object
            }
            regs[reg_idx] = MAKE_NUMBER(r);          // store decremented number
        }
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
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
        APEX_TRY_JIT_LOOP();
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
        } else if (IS_TABLE(left) && IS_TABLE(right)) {
            jump = table_equal(AS_TABLE(left), AS_TABLE(right), 0);  // compare tables
        }
        if (jump) {
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_NEQ_LABEL: {
        APEX_TRY_JIT_LOOP();
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
        } else if (IS_TABLE(left) && IS_TABLE(right)) {
            jump = !table_equal(AS_TABLE(left), AS_TABLE(right), 0);  // compare tables
        }
        if (jump) {
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_EQ_NUM_LABEL: {
        APEX_TRY_JIT_LOOP();
        int target = ip->operands[0];            // jump target address
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        if (a.d == b.d) {                        // compare doubles for equality (no type checks)
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through to next instruction
    }
    OP_JUMP_IF_NEQ_NUM_LABEL: {
        APEX_TRY_JIT_LOOP();
        int target = ip->operands[0];            // jump target address
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        if (a.d != b.d) {                        // compare doubles for inequality (no type checks)
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through to next instruction
    }
    OP_JUMP_IF_LT_LABEL: {
        APEX_TRY_JIT_LOOP();
        int target = ip->operands[0];            // jump target address
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        if (a.d < b.d) {                         // compare as doubles
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_LTE_LABEL: {
        APEX_TRY_JIT_LOOP();
        int target = ip->operands[0];            // jump target address
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        if (a.d <= b.d) {                        // compare as doubles, less or equal
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_GT_LABEL: {
        APEX_TRY_JIT_LOOP();
        int target = ip->operands[0];            // jump target address
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        if (a.d > b.d) {                         // compare as doubles
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }
    OP_JUMP_IF_GTE_LABEL: {
        APEX_TRY_JIT_LOOP();
        int target = ip->operands[0];            // jump target address
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        if (a.d >= b.d) {                        // compare as doubles, greater or equal
            ip = &vm->code[target];              // jump to target
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        ip++; goto *dispatch_table[ip->opcode];  // fall through
    }

    OP_JUMP_MATCH_NUM_LABEL: {
        int target = ip->operands[0];              // jump target address
        int subj_reg = ip->operands[1];            // register holding subject value
        int const_idx = ip->operands[2];           // constant pool index for number
        Value subj = regs[subj_reg];               // fetch subject value

        if (IS_NUMBER(subj)) {                     // only numbers can match number cases
            double case_val = chunk->constants[const_idx].number_value;
            if (AS_NUMBER(subj) == case_val) {     // exact numeric equality
                ip = &vm->code[target];            // jump to case body
                goto *dispatch_table[ip->opcode];
            }
        }
        ip++; goto *dispatch_table[ip->opcode];    // no match, continue to next check
    }
    OP_JUMP_MATCH_STR_LABEL: {
        int target = ip->operands[0];              // jump target address
        int subj_reg = ip->operands[1];            // register holding subject value
        int const_idx = ip->operands[2];           // constant pool index for string
        Value subj = regs[subj_reg];               // fetch subject value

        if (IS_STRING(subj)) {                     // only strings can match string cases
            StringObject* case_obj = (StringObject*)chunk->constants[const_idx].cached_str;  // pre-interned case string
            if (string_equal(AS_STRING(subj), case_obj)) {  // content compare, hash already cached
                ip = &vm->code[target];            // jump to case body
                goto *dispatch_table[ip->opcode];
            }
        }
        ip++; goto *dispatch_table[ip->opcode];    // no match, continue to next check
    }
    OP_JUMP_MATCH_BOOL_LABEL: {
        int target = ip->operands[0];              // jump target address
        int subj_reg = ip->operands[1];            // register holding subject value
        int bool_val = ip->operands[2];            // expected boolean value 0 or 1
        Value subj = regs[subj_reg];               // fetch subject value

        if (IS_BOOL(subj) && AS_BOOL(subj) == (bool_val != 0)) {
            ip = &vm->code[target];                // jump to case body
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];    // no match, continue to next check
    }
    OP_JUMP_MATCH_NONE_LABEL: {
        int target = ip->operands[0];              // jump target address
        int subj_reg = ip->operands[1];            // register holding subject value
        Value subj = regs[subj_reg];               // fetch subject value

        if (IS_NONE(subj)) {                       // subject is none
            ip = &vm->code[target];                // jump to case body
            goto *dispatch_table[ip->opcode];
        }
        ip++; goto *dispatch_table[ip->opcode];    // no match, continue to next check
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
        else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            result = (AS_NUMBER(left) == AS_NUMBER(right));      // compare numeric values
        }
        else if (IS_STRING(left) && IS_STRING(right)) {
            result = string_equal(AS_STRING(left), AS_STRING(right));  // compare string contents
        }
        else if (IS_BOOL(left) && IS_BOOL(right)) {
            result = (AS_BOOL(left) == AS_BOOL(right));          // compare boolean values
        }
        else if (IS_TABLE(left) && IS_TABLE(right)) {
            result = table_equal(AS_TABLE(left), AS_TABLE(right), 0);  // compare tables
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
        else if (IS_NUMBER(left) && IS_NUMBER(right)) {
            result = (AS_NUMBER(left) != AS_NUMBER(right));      // compare numeric values
        }
        else if (IS_STRING(left) && IS_STRING(right)) {
            result = !string_equal(AS_STRING(left), AS_STRING(right));  // compare string contents
        }
        else if (IS_BOOL(left) && IS_BOOL(right)) {
            result = (AS_BOOL(left) != AS_BOOL(right));          // compare boolean values
        }
        else if (IS_TABLE(left) && IS_TABLE(right)) {
            result = !table_equal(AS_TABLE(left), AS_TABLE(right), 0);  // compare tables
        }
        regs[dest] = MAKE_BOOL(result);          // store result as bool
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_EQ_NUM_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        regs[dest] = MAKE_BOOL(a.d == b.d);      // compare doubles and store bool result (no type checks)
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_NEQ_NUM_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        regs[dest] = MAKE_BOOL(a.d != b.d);      // compare doubles and store bool result (no type checks)
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_LT_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        regs[dest] = MAKE_BOOL(a.d < b.d);       // compare and store bool result
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_GT_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        regs[dest] = MAKE_BOOL(a.d > b.d);       // compare and store bool result
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_LTE_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        regs[dest] = MAKE_BOOL(a.d <= b.d);      // compare and store bool result
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_CMP_GTE_LABEL: {
        int dest = ip->operands[0];              // dest register index
        du64 a = {.u = regs[ip->operands[1]]};   // reinterpret left operand as double via union
        du64 b = {.u = regs[ip->operands[2]]};   // reinterpret right operand as double via union
        regs[dest] = MAKE_BOOL(a.d >= b.d);      // compare and store bool result
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
        APEX_TRY_JIT_LOOP();
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
        APEX_TRY_JIT_LOOP();
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
            if (!IS_NONE(t->array_part[idx])) {        // slot is occupied
                value_decref(regs[var_reg]);           // release old key in var reg
                regs[var_reg] = t->array_part[idx];    // store the value (not index)
                value_incref(regs[var_reg]);           // bump refcount for stored value
                ip++;                                  // advance to loop body
                goto *dispatch_table[ip->opcode];      // dispatch next instruction
            }
        }
        while (iter->bucket_index < t->capacity) {    // iterate over hash part
            if (t->entries == NULL) {                 // no hash part allocated (lazy init)
                vm->table_iter_depth--;               // pop iterator frame
                ip = &vm->code[exit_addr];            // jump to exit
                goto *dispatch_table[ip->opcode];     // dispatch next instruction
            }
            if (iter->current_entry == NULL) {        // need to advance to next bucket
                iter->current_entry = t->entries[iter->bucket_index++];  // get first entry in bucket
                continue;                             // retry with the new entry
            }
            TableEntry* entry = iter->current_entry;  // get current entry
            iter->current_entry = entry->next;        // advance to next entry in chain
            value_decref(regs[var_reg]);              // release old key in var reg
            regs[var_reg] = entry->value;             // store the value (not key)
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
        Table* table = AS_TABLE(table_val);          // unwrap table pointer
        Value key = MAKE_STRING((StringObject*)chunk->constants[key_idx].cached_str);  // use pre-interned key
        Value val;
        val = MAKE_NONE();                           // default to none
        table_get(table, key, &val);                 // lookup key in table, writes to val
        value_decref(vm->registers[dest]);           // release old dest value
        vm->registers[dest] = val;                   // store result (already incref'd by table_get)
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
    }
    OP_TABLE_GET_INT_LABEL: {
        int dest = ip->operands[0];                  // dest register index
        int table_reg = ip->operands[1];             // register holding the table
        int index = ip->operands[2];                 // immediate integer key (1-based)
        
        Value table_val = regs[table_reg];           // fetch table value
        Value val = MAKE_NONE();                     // default to none
        
        if (likely(IS_TABLE(table_val))) {
            Table* table = AS_TABLE(table_val);      // unwrap table pointer
            
            if (table->array_part != NULL && index >= 1 && index <= table->array_count) {
                val = table->array_part[index - 1];  // direct array access (0-based)
                value_incref(val);                   // bump refcount (no-op for numbers)
            }
        }
        
        value_decref(regs[dest]);                    // release old dest value
        regs[dest] = val;                            // store result
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
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
    OP_TABLE_SET_CONST_LABEL: {
        int table_reg = ip->operands[0];             // register holding the table
        int key_idx = ip->operands[1];               // constant pool index for the string key
        int val_reg = ip->operands[2];               // register holding the value
        Table* table = AS_TABLE(vm->registers[table_reg]);  // unwrap table pointer
        Value key = MAKE_STRING((StringObject*)chunk->constants[key_idx].cached_str);  // use pre-interned key
        table_set(table, key, vm->registers[val_reg]);      // perform table set with refcount handling
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
    }
    OP_TABLE_SET_INT_LABEL: {
        int table_reg = ip->operands[0];             // register holding the table
        int index = ip->operands[1];                 // immediate integer key (1-based)
        int val_reg = ip->operands[2];               // register holding the value
        
        Table* table = AS_TABLE(regs[table_reg]);    // parser guarantees table type
        
        if (table->array_part != NULL && index >= 1 && index <= table->array_capacity) {  // fits in array
            int idx = index - 1;                     // convert to 0-based
            
            value_decref(table->array_part[idx]);    // release old value at slot
            table->array_part[idx] = regs[val_reg];  // store new value
            value_incref(table->array_part[idx]);    // bump refcount for stored value
            
            if (index > table->array_count) {        // update array count if extending
                table->array_count = index;
            }
        } else {
            table_set_int(table, index - 1, regs[val_reg]);  // grow array part if needed
        }
        
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
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
                table->array_part[i] = MAKE_NONE();         // fill with none (empty slot marker)
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
        int dest = ip->operands[0];                 // dest register index
        int left_reg  = ip->operands[1];            // left operand register
        int right_reg = ip->operands[2];            // right operand register
        Value left  = regs[left_reg];               // fetch left operand
        Value right = regs[right_reg];              // fetch right operand

        if (IS_STRING(left) && IS_STRING(right) && dest == left_reg) {  // accumulator pattern: dest == left
            StringObject* a = AS_STRING(left);      // unwrap left string
            if (a->header.ref_count == 1) {         // uniquely owned, safe to realloc in place
                StringObject* b = AS_STRING(right); // unwrap right string
                int old_len = a->length;            // old string length
                int new_len = old_len + b->length;  // combined new length
                StringObject* out = (StringObject*)realloc(     // grow in place, keeps header
                        a, sizeof(StringObject) + new_len + 1);
                if (out) {                          // realloc succeeded
                    memcpy(out->chars + old_len, b->chars, b->length);  // append right part
                    out->length = new_len;          // update length
                    out->chars[new_len] = '\0';     // null terminate
                    out->hash_computed = false;     // invalidate cached hash
                    regs[dest] = MAKE_STRING(out);  // store (possibly moved) pointer
                    ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
                }
                // realloc failed: fall through to fresh allocation below
            }
        }

        if (IS_STRING(left) && IS_STRING(right)) {  // direct string + string
            StringObject* a = AS_STRING(left);      // unwrap left string
            StringObject* b = AS_STRING(right);     // unwrap right string
            int total = a->length + b->length;      // combined length

            StringObject* out = (StringObject*)malloc(  // exact single allocation
                    sizeof(StringObject) + total + 1);
            out->header.ref_count = 1;              // fresh object refcount
            out->header.type      = VAL_STRING;     // mark type as string
            out->length           = total;          // store length
            out->hash_computed    = false;          // hash not yet computed
            out->hash             = 0;              // clear hash field
            memcpy(out->chars, a->chars, a->length);            // copy left part
            memcpy(out->chars + a->length, b->chars, b->length);  // copy right part
            out->chars[total] = '\0';               // null terminate

            value_decref(regs[dest]);               // release old dest value
            regs[dest] = MAKE_STRING(out);          // store new string
            ip++; goto *dispatch_table[ip->opcode]; // advance to next instruction
        }

        if (IS_STRING(left) && IS_NUMBER(right)) {  // direct string + number
            StringObject* a = AS_STRING(left);      // unwrap left string
            char numbuf[64];                        // temp buffer for number
            double num = AS_NUMBER(right);          // unwrap right number
            int numlen;                             // formatted number length
            if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9)
                numlen = snprintf(numbuf, sizeof(numbuf), "%.0f", num);   // large or integer, no decimals
            else
                numlen = snprintf(numbuf, sizeof(numbuf), "%.15g", num);  // use general format with high precision

            int total = a->length + numlen;         // combined length
            StringObject* out = (StringObject*)malloc(  // exact single allocation
                    sizeof(StringObject) + total + 1);
            out->header.ref_count = 1;              // fresh object refcount
            out->header.type      = VAL_STRING;     // mark type as string
            out->length           = total;          // store length
            out->hash_computed    = false;          // hash not yet computed
            out->hash             = 0;              // clear hash field
            memcpy(out->chars, a->chars, a->length);            // copy left part
            memcpy(out->chars + a->length, numbuf, numlen);     // copy formatted number
            out->chars[total] = '\0';               // null terminate

            value_decref(regs[dest]);               // release old dest value
            regs[dest] = MAKE_STRING(out);          // store new string
            ip++; goto *dispatch_table[ip->opcode]; // advance to next instruction
        }

        if (IS_NUMBER(left) && IS_STRING(right)) {  // direct number + string
            StringObject* b = AS_STRING(right);     // unwrap right string
            char numbuf[64];                        // temp buffer for number
            double num = AS_NUMBER(left);           // unwrap left number
            int numlen;                             // formatted number length
            if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9)
                numlen = snprintf(numbuf, sizeof(numbuf), "%.0f", num);   // large or integer, no decimals
            else
                numlen = snprintf(numbuf, sizeof(numbuf), "%.15g", num);  // use general format with high precision

            int total = numlen + b->length;         // combined length
            StringObject* out = (StringObject*)malloc(  // exact single allocation
                    sizeof(StringObject) + total + 1);
            out->header.ref_count = 1;              // fresh object refcount
            out->header.type      = VAL_STRING;     // mark type as string
            out->length           = total;          // store length
            out->hash_computed    = false;          // hash not yet computed
            out->hash             = 0;              // clear hash field
            memcpy(out->chars, numbuf, numlen);                 // copy formatted number
            memcpy(out->chars + numlen, b->chars, b->length);   // copy right part
            out->chars[total] = '\0';               // null terminate

            value_decref(regs[dest]);               // release old dest value
            regs[dest] = MAKE_STRING(out);          // store new string
            ip++; goto *dispatch_table[ip->opcode]; // advance to next instruction
        }

        {                                           // generic fallback for other type combos
            char lbuf[4096], rbuf[4096];            // temp buffers for string conversion
            const char* ls = value_to_cstr(left,  lbuf, sizeof(lbuf));   // convert left to c string
            const char* rs = value_to_cstr(right, rbuf, sizeof(rbuf));   // convert right to c string
            int llen = IS_STRING(left)  ? AS_STRING(left)->length  : (int)strlen(ls);   // left length
            int rlen = IS_STRING(right) ? AS_STRING(right)->length : (int)strlen(rs);   // right length
            int total_len = llen + rlen;            // combined length

            StringObject* out = (StringObject*)malloc(  // exact single allocation
                    sizeof(StringObject) + total_len + 1);
            out->header.ref_count = 1;              // fresh object refcount
            out->header.type      = VAL_STRING;     // mark type as string
            out->length           = total_len;      // store length
            out->hash_computed    = false;          // hash not yet computed
            out->hash             = 0;              // clear hash field
            memcpy(out->chars, ls, llen);           // copy left part
            memcpy(out->chars + llen, rs, rlen);    // copy right part
            out->chars[total_len] = '\0';           // null terminate

            value_decref(regs[dest]);               // release old dest value
            regs[dest] = MAKE_STRING(out);          // store new string
        }

        ip++; goto *dispatch_table[ip->opcode];     // advance to next instruction
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
        if (vm->args_top >= VM_MAX_ARGS_STACK) {     // check for arg stack overflow
            fprintf(stderr, "\033[31mArgument stack overflow - maximum %d arguments exceeded. "
                    "Too many function arguments being passed.\n\033[0m",
                    VM_MAX_ARGS_STACK);              // print error message
            vm->had_error = true;                    // set error flag
            vm->running = false;                     // stop execution
            return false;                            // early return
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
                    VM_MAX_CALL_FRAMES);          // print error message
            vm->had_error = true;                 // set error flag
            vm->running = false;                  // stop execution
            return false;                         // early return
        }
        int func_idx = ip->operands[1];           // function index in chunk->functions[]
        int arg_count = ip->operands[2];          // number of arguments
        int dest_reg  = ip->operands[0];          // dest register for return value
        vm->call_stack[vm->call_depth].return_address = (ip + 1) - vm->code;      // save return address
        vm->call_stack[vm->call_depth].dest_reg       = dest_reg;                 // save dest register
        vm->call_stack[vm->call_depth].frame_index    = vm->current_frame;        // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;  // save iterator depth
        vm->call_depth++;                         // push call frame
        vm->current_frame++;                      // advance to next register frame
        
        int needed = chunk->functions[func_idx].max_registers;  // get max_registers from function metadata
        if (arg_count > needed) needed = arg_count;
        
        if (frame_cap[vm->current_frame] <= needed) {
            if (!ensure_register_capacity(vm, vm->current_frame, needed)) {
                frame_cap = vm->frame_capacity;  // refresh cached pointers after growth
                frame_off = vm->frame_offset;
                vm->had_error = true;             // failed to allocate frame
                vm->running = false;              // stop execution
                return false;                     // bail out
            }
        }
        vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // switch to new frame
        regs = vm->registers;                     // update local regs pointer
        for (int i = 0; i < arg_count; i++) {
            Value _arg = vm->args_stack[vm->args_top - arg_count + i];  // fetch source value
            Value _old = regs[i];                                       // old slot value
            regs[i] = _arg;                                             // take ownership (transfer from args stack)
            if ((_old & QNAN) == QNAN) value_decref(_old);              // release old slot value (no-op for none)
        }
        vm->args_top -= arg_count;                // pop args from args stack (refs now held by callee)
        ip = &vm->code[chunk->functions[func_idx].address];  // jump to function body
        goto *dispatch_table[ip->opcode];         // dispatch first instruction of function
    }
    OP_CALL_BUILTIN_LABEL: {
        int dest_reg = ip->operands[0];              // dest register for return value
        int name_idx = ip->operands[1];              // constant pool index for builtin name
        int arg_count = ip->operands[2];             // number of arguments
        Value args[VM_MAX_ARGS_STACK];               // local args array
        for (int i = 0; i < arg_count && i < 16; i++) {
            args[i] = vm->args_stack[vm->args_top - arg_count + i];  // copy args from stack
        }
        Value result;                                // placeholder for return value
        bool ok = vm_call_builtin(vm, chunk->constants[name_idx].string_value, arg_count, args, &result);  // dispatch to builtin
        for (int i = 0; i < arg_count; i++) {
            value_decref(vm->args_stack[vm->args_top - arg_count + i]);  // release args from stack
        }
        vm->args_top -= arg_count;                   // pop args from args stack
        if (ok) {
            value_decref(vm->registers[dest_reg]);   // release old dest value
            vm->registers[dest_reg] = result;        // store result from builtin
        } else {
            value_decref(vm->registers[dest_reg]);   // release old dest value
            vm->registers[dest_reg] = MAKE_NONE();   // builtin failed, store none
        }
        if (dest_reg >= vm->frame_used[vm->current_frame]) {
            vm->frame_used[vm->current_frame] = dest_reg + 1;  // track max register used for vm_destroy cleanup
        }
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
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
        int func_idx = ip->operands[1];              // function index
        int dest_reg = ip->operands[0];              // dest register for return value
#if APEX_JIT_ENABLED
        if (vm->jit && jit_has_native(vm->jit, func_idx)) {
            double r = jit_call_0(vm->jit, func_idx);
            value_decref(regs[dest_reg]);
            regs[dest_reg] = jit_box_return(vm->jit, func_idx, r);
            ip++; goto *dispatch_table[ip->opcode];
        }
#endif
        vm->call_stack[vm->call_depth].return_address = (int)((ip + 1) - vm->code);  // save return address
        vm->call_stack[vm->call_depth].dest_reg = dest_reg;                          // save dest register
        vm->call_stack[vm->call_depth].frame_index = vm->current_frame;              // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;     // save iterator depth
        vm->call_depth++;                            // push call frame
        vm->current_frame++;                         // advance to next register frame
        
        int needed = chunk->functions[func_idx].max_registers;  // get max_registers from function metadata
        
        if (frame_cap[vm->current_frame] <= needed) {
            if (!ensure_register_capacity(vm, vm->current_frame, needed)) {
                frame_cap = vm->frame_capacity;  // refresh cached pointers after growth
                frame_off = vm->frame_offset;
                vm->had_error = true;                // failed to allocate frame
                vm->running = false;                 // stop execution
                goto OP_HALT_LABEL;                  // jump to halt
            }
        }
        vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // switch to new frame
        regs = vm->registers;                        // update local regs pointer
        ip = &vm->code[chunk->functions[func_idx].address];  // jump to function body
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
        int func_idx = ip->operands[1];              // function index
        int dest_reg = ip->operands[0];              // dest register for return value
        int arg_reg = ip->operands[2];               // register holding the single argument
#if APEX_JIT_ENABLED
        if (vm->jit && jit_has_native(vm->jit, func_idx)) {
            Value av = regs[arg_reg];
            if (IS_NUMBER(av)) {
                double r = jit_call_1(vm->jit, func_idx, AS_NUMBER(av));
                value_decref(regs[dest_reg]);
                regs[dest_reg] = jit_box_return(vm->jit, func_idx, r);
                ip++; goto *dispatch_table[ip->opcode];
            }
        }
#endif
        vm->call_stack[vm->call_depth].return_address = (int)((ip + 1) - vm->code);  // save return address
        vm->call_stack[vm->call_depth].dest_reg = dest_reg;                          // save dest register
        vm->call_stack[vm->call_depth].frame_index = vm->current_frame;              // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;     // save iterator depth
        Value _arg = regs[arg_reg];                  // read arg from caller frame while regs still points there

        vm->call_depth++;                            // push call frame
        vm->current_frame++;                         // advance to next register frame
        
        int needed = chunk->functions[func_idx].max_registers;  // get max_registers from function metadata
        
        if (frame_cap[vm->current_frame] <= needed) {
            if (!ensure_register_capacity(vm, vm->current_frame, needed)) {
                frame_cap = vm->frame_capacity;  // refresh cached pointers after growth
                frame_off = vm->frame_offset;
                vm->had_error = true;  // failed to allocate frame
                vm->running = false;   // stop execution
                goto OP_HALT_LABEL;    // jump to halt
            }
        }
        vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // switch to new frame
        regs = vm->registers;                                       // update local regs pointer
        Value _old = regs[0];                                       // old slot value
        regs[0] = _arg;                                             // store arg into callee slot
        if ((_arg & QNAN) == QNAN) value_incref(_arg);              // bump refcount for callee
        if ((_old & QNAN) == QNAN) value_decref(_old);              // release old slot value (no-op for none)
        ip = &vm->code[chunk->functions[func_idx].address];         // jump to function body
        goto *dispatch_table[ip->opcode];                           // dispatch first instruction of function
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
        int func_idx = ip->operands[1];              // function index
        int dest_reg = ip->operands[0];              // dest register for return value
        int arg1_reg = ip->operands[2];              // register holding first argument
        int arg2_reg = arg1_reg + 1;                 // second argument is in the next register
#if APEX_JIT_ENABLED
        if (vm->jit && jit_has_native(vm->jit, func_idx)) {
            Value a1 = regs[arg1_reg], a2 = regs[arg2_reg];
            if (IS_NUMBER(a1) && IS_NUMBER(a2)) {
                double r = jit_call_2(vm->jit, func_idx,
                                    AS_NUMBER(a1), AS_NUMBER(a2));
                value_decref(regs[dest_reg]);
                regs[dest_reg] = jit_box_return(vm->jit, func_idx, r);
                ip++; goto *dispatch_table[ip->opcode];
            }
        }
#endif
        vm->call_stack[vm->call_depth].return_address = (int)((ip + 1) - vm->code);  // save return address
        vm->call_stack[vm->call_depth].dest_reg = dest_reg;                          // save dest register
        vm->call_stack[vm->call_depth].frame_index = vm->current_frame;              // save current frame index
        vm->call_stack[vm->call_depth].base_iterator_depth = vm->iterator_depth;     // save iterator depth
        Value _a1 = regs[arg1_reg];                  // read first arg from caller frame before switching
        Value _a2 = regs[arg2_reg];                  // read second arg from caller frame before switching

        vm->call_depth++;                            // push call frame
        vm->current_frame++;                         // advance to next register frame
        
        int needed = chunk->functions[func_idx].max_registers;  // get max_registers from function metadata
        
        if (frame_cap[vm->current_frame] <= needed) {
            if (!ensure_register_capacity(vm, vm->current_frame, needed)) {
                frame_cap = vm->frame_capacity;  // refresh cached pointers after growth
                frame_off = vm->frame_offset;
                vm->had_error = true;                // failed to allocate frame
                vm->running = false;                 // stop execution
                goto OP_HALT_LABEL;                  // jump to halt
            }
        }
        vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // switch to new frame
        regs = vm->registers;                                       // update local regs pointer
        Value _o0 = regs[0];                                        // old slot 0 value
        Value _o1 = regs[1];                                        // old slot 1 value
        regs[0] = _a1;                                              // store first arg into callee slot
        regs[1] = _a2;                                              // store second arg into callee slot
        if ((_a1 & QNAN) == QNAN) value_incref(_a1);                // bump refcount for callee
        if ((_a2 & QNAN) == QNAN) value_incref(_a2);                // bump refcount for callee
        if ((_o0 & QNAN) == QNAN) value_decref(_o0);                // release old slot 0 (no-op for none)
        if ((_o1 & QNAN) == QNAN) value_decref(_o1);                // release old slot 1 (no-op for none)
        ip = &vm->code[chunk->functions[func_idx].address];         // jump to function body
        goto *dispatch_table[ip->opcode];                           // dispatch first instruction of function
    }
    OP_RETURN_LABEL: {
        int value_reg = ip->operands[0];         // register holding return value
        Value ret_val = regs[value_reg];         // fetch return value
        if (vm->current_task != NULL && vm->call_depth == 0) {  // coroutine body returning
            FutureObject* cur = vm->current_task;               // coroutine being completed
            vm->current_task = NULL;                            // leave coroutine mode
            vm->running = false;                                // stop this slice
            future_resolve(vm, cur, ret_val);                   // resolve its future, wake waiters
            return true;                                        // back to scheduler
        }
        RESOLVE_FRAME_FUTURE(ret_val);
        if (unlikely((ret_val & QNAN) == QNAN)) {  // heap object (string/table) - less common
            value_incref(ret_val);               // bump refcount for the returned value
        }
        if (likely(vm->call_depth > 0)) {        // returning from a function call (common)
            vm->call_depth--;                    // pop call frame
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;           // restore frame index
            vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // restore register frame
            regs = vm->registers;                // update local regs pointer
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;  // restore iterator depth
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;                   // dest register for return value
            if (unlikely((regs[dest_reg] & QNAN) == QNAN)) {  // old value is heap object (rare)
                value_decref(regs[dest_reg]);    // release old value in dest
            }
            regs[dest_reg] = ret_val;            // store return value in caller's dest reg
            ip = &vm->code[vm->call_stack[vm->call_depth].return_address];   // jump to return address
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        vm->running = false;                     // top-level return, stop execution (rare)
        if (unlikely((ret_val & QNAN) == QNAN)) {  // heap object needs decref
            value_decref(ret_val);               // release return value
        }
        goto OP_HALT_LABEL;                      // jump to halt
    }
    OP_RETURN_NUM_LABEL: {
        int value_reg = ip->operands[0];         // register holding return value
        Value ret_val = regs[value_reg];         // fetch return value
        if (vm->current_task != NULL && vm->call_depth == 0) {  // coroutine body returning
            FutureObject* cur = vm->current_task;               // coroutine being completed
            vm->current_task = NULL;                            // leave coroutine mode
            vm->running = false;                                // stop this slice
            future_resolve(vm, cur, ret_val);                   // resolve its future, wake waiters
            return true;                                        // back to scheduler
        }
        RESOLVE_FRAME_FUTURE(ret_val);
        if (likely(vm->call_depth > 0)) {        // returning from a function call (common)
            vm->call_depth--;                    // pop call frame
            int return_addr = vm->call_stack[vm->call_depth].return_address;  // get return address
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;           // get dest register
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;   // restore frame index
            vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // restore register frame
            regs = vm->registers;                // update local regs pointer
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;  // restore iterator depth
            regs[dest_reg] = ret_val;            // store return value in caller's dest reg (no incref, unboxed number)
            ip = &vm->code[return_addr];         // jump to return address
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        vm->running = false;                     // top-level return, stop execution (rare)
        goto OP_HALT_LABEL;                      // jump to halt
    }
    OP_RETURN_BOOL_LABEL: {
        int value_reg = ip->operands[0];         // register holding return value
        Value ret_val = regs[value_reg];         // fetch return value
        if (vm->current_task != NULL && vm->call_depth == 0) {  // coroutine body returning
            FutureObject* cur = vm->current_task;               // coroutine being completed
            vm->current_task = NULL;                            // leave coroutine mode
            vm->running = false;                                // stop this slice
            future_resolve(vm, cur, ret_val);                   // resolve its future, wake waiters
            return true;                                        // back to scheduler
        }
        RESOLVE_FRAME_FUTURE(ret_val);
        if (likely(vm->call_depth > 0)) {        // returning from a function call (common)
            vm->call_depth--;                    // pop call frame
            int return_addr = vm->call_stack[vm->call_depth].return_address;  // get return address
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;           // get dest register
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;   // restore frame index
            vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // restore register frame
            regs = vm->registers;                // update local regs pointer
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;  // restore iterator depth
            regs[dest_reg] = ret_val;            // store return value in caller's dest reg (unboxed bool, no incref)
            ip = &vm->code[return_addr];         // jump to return address
            goto *dispatch_table[ip->opcode];    // dispatch next instruction
        }
        vm->running = false;                     // top-level return, stop execution (rare)
        goto OP_HALT_LABEL;                      // jump to halt
    }
    OP_RETURN_NONE_LABEL: {
        if (vm->current_task != NULL && vm->call_depth == 0) {  // coroutine body returning
            FutureObject* cur = vm->current_task;               // coroutine being completed
            vm->current_task = NULL;                            // leave coroutine mode
            vm->running = false;                                // stop this slice
            future_resolve(vm, cur, MAKE_NONE());               // resolve its future with none
            return true;                                        // back to scheduler
        }
        RESOLVE_FRAME_FUTURE(MAKE_NONE());
        if (likely(vm->call_depth > 0)) {           // most returns are from function calls
            vm->call_depth--;                       // pop call frame
            int return_addr = vm->call_stack[vm->call_depth].return_address;  // get return address
            int dest_reg = vm->call_stack[vm->call_depth].dest_reg;           // get dest register
            vm->current_frame = vm->call_stack[vm->call_depth].frame_index;   // restore frame index
            vm->registers = &vm->register_pool[frame_off[vm->current_frame]];  // restore register frame
            regs = vm->registers;                   // update local regs pointer
            vm->iterator_depth = vm->call_stack[vm->call_depth].base_iterator_depth;  // restore iterator depth
            if (unlikely((regs[dest_reg] & QNAN) == QNAN)) {  // old value is heap object (rare)
                value_decref(regs[dest_reg]);       // release old value in dest
            }
            regs[dest_reg] = MAKE_NONE();           // store none in caller's dest reg (no incref needed)
            ip = &vm->code[return_addr];            // jump to return address
            goto *dispatch_table[ip->opcode];       // dispatch next instruction
        }
        vm->running = false;                        // top-level return, stop execution (rare)
        goto OP_HALT_LABEL;                         // jump to halt
    }

    OP_ASYNC_CALL_LABEL: {
        int dest_reg = ip->operands[0];              // dest register for the pending future
        int func_idx = ip->operands[1];              // function table index of the async body
        int arg_count = ip->operands[2];             // number of arguments on the args stack

        FutureObject* fut = (FutureObject*)malloc(sizeof(FutureObject));  // allocate pending future
        fut->header.ref_count = 1;                   // fresh object refcount
        fut->header.type = VAL_FUTURE;               // mark type as future
        fut->result = MAKE_NONE();                   // filled in when awaited
        fut->state = 0;                              // pending
        fut->func_idx = func_idx;                    // body to run on await
        fut->arg_count = arg_count;                  // captured argument count
        fut->register_pool = NULL;                   // pool allocated lazily in future_start
        fut->frame_offset = NULL;
        fut->frame_capacity = NULL;
        fut->frame_used = NULL;
        fut->pool_capacity = 0;
        fut->current_frame = 0;
        fut->owns_frame = false;
        fut->frame_idx = -1;                         // legacy field, unused
        fut->saved_ip = 0;                           // no resume point yet
        fut->saved_dest_reg = -1;                    // no pending await
        fut->awaiting = MAKE_NONE();                 // nothing awaited
        fut->waiters = NULL;                         // no waiters yet
        fut->waiter_count = 0;                       // empty
        fut->waiter_capacity = 0;                    // no capacity
        fut->saved_iter_depth = -1;                  // no active loops
        fut->saved_table_iter_depth = -1;            // no active table iterators
        fut->args = arg_count > 0 ? (Value*)malloc(sizeof(Value) * arg_count) : NULL;  // capture buffer
        for (int i = 0; i < arg_count; i++) {        // capture args from args stack
            Value a = vm->args_stack[vm->args_top - arg_count + i];
            if ((a & QNAN) == QNAN) value_incref(a); // keep a reference in the future
            fut->args[i] = a;                        // store captured value
        }
        vm->args_top -= arg_count;                   // pop args stack
        if ((regs[dest_reg] & QNAN) == QNAN) value_decref(regs[dest_reg]);  // release old dest
        regs[dest_reg] = MAKE_FUTURE(fut);           // return pending future
        if (fut->func_idx >= 0) {                    // async body: schedule eagerly
            future_start(vm, fut);                   // enqueue coroutine for the scheduler
        }
        ip++; goto *dispatch_table[ip->opcode];      // no execution, caller continues
    }
    OP_AWAIT_LABEL: {
        int dest = ip->operands[0];                  // dest register index
        int src = ip->operands[1];                   // source register index
        Value v = regs[src];                         // value being awaited

        if (IS_FUTURE(v)) {                          // awaiting a future
            FutureObject* fut = AS_FUTURE(v);

            if (fut->state == 1) {                   // already resolved: read result
                Value result = fut->result;
                if ((result & QNAN) == QNAN) value_incref(result);
                if ((regs[dest] & QNAN) == QNAN) value_decref(regs[dest]);
                regs[dest] = result;
                ip++; goto *dispatch_table[ip->opcode];
            }

            if (vm->current_task != NULL) {          // inside a coroutine: suspend
                FutureObject* cur = vm->current_task;
                cur->saved_ip = (ip + 1) - vm->code; // resume after AWAIT
                cur->saved_dest_reg = dest;          // where the result goes
                value_decref(cur->awaiting);         // release previous awaited future
                cur->awaiting = v;                   // remember what we wait on
                value_incref(v);                     // keep reference
                cur->saved_iter_depth = vm->iterator_depth;               // save loop depth
                cur->saved_table_iter_depth = vm->table_iter_depth;       // save table iterator depth
                future_add_waiter(fut, cur);         // register as waiter
                if (fut->register_pool == NULL) future_start(vm, fut);  // launch awaited body if pending
                vm->current_task = NULL;             // leave coroutine mode
                vm->running = false;                 // stop this slice
                return true;                         // back to scheduler
            }

            // top-level await: drive the scheduler
            Value result;
            vm_drive_until(vm, v, &result);
            regs = vm->registers;                    // reload regs in case pool was reallocated
            frame_cap = vm->frame_capacity;          // refresh cached pointers after context switch
            frame_off = vm->frame_offset;
            if ((regs[dest] & QNAN) == QNAN) value_decref(regs[dest]);
            regs[dest] = result;
            ip++; goto *dispatch_table[ip->opcode];
        }

        if (dest != src) {                           // non-future passthrough: copy
            if ((regs[dest] & QNAN) == QNAN) value_decref(regs[dest]);
            if ((v & QNAN) == QNAN) value_incref(v);
            regs[dest] = v;
        }
        ip++; goto *dispatch_table[ip->opcode];      // advance to next instruction
    }

    OP_LOAD_GLOBAL_LABEL: {
        int dest = ip->operands[0];              // dest register index
        int idx = ip->operands[1];               // global variable index
        Value gv = vm->globals[idx];             // fetch global value
        
        if (unlikely((gv & QNAN) == QNAN)) {     // heap object (string/table) - rare
            value_decref(regs[dest]);            // release old value in dest
            regs[dest] = gv;                     // store heap-allocated value
            value_incref(regs[dest]);            // bump refcount for the new reference
        } else {                                 // number/bool/none - common
            regs[dest] = gv;                     // store directly without refcount
        }
        
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }
    OP_STORE_GLOBAL_LABEL: {
        int src = ip->operands[0];               // source register index
        int idx = ip->operands[1];               // global variable index
        Value sv = regs[src];                    // fetch source value
        
        if (unlikely((sv & QNAN) == QNAN)) {     // heap object (string/table) - rare
            value_decref(vm->globals[idx]);      // release old global value
            vm->globals[idx] = sv;               // store new value into global
            value_incref(vm->globals[idx]);      // bump refcount for stored value
        } else {                                 // number/bool/none - common
            vm->globals[idx] = sv;               // store directly without refcount
        }
        
        ip++; goto *dispatch_table[ip->opcode];  // advance to next instruction
    }

    OP_HALT_LABEL:
        vm->running = false;

        // top-level teardown: drive the scheduler until all fire-and-forget
        // coroutines, workers and timers have settled
        if (top_level && !vm->had_error) {
            SavedVMContext saved_ctx;                   // snapshot of top-level pool
            vm_context_save(vm, &saved_ctx);

            int saved_iter_depth = vm->iterator_depth;  // snapshot of top-level numeric loop depth
            int saved_titer = vm->table_iter_depth;     // snapshot of top-level table iterator depth

            while (!vm->had_error && (vm->ready_count > 0 || vm->timers != NULL
                                      || vm->pending_workers > 0)) {
                bool ran = false;

                while (vm->ready_count > 0 && !vm->had_error) {
                    FutureObject* task = scheduler_pop(vm);   // take next task
                    ran = true;

                    vm_context_enter(vm, task);               // install task's private pool
                    vm_execute(vm, chunk);                    // run one slice
                    vm_context_capture(vm, task);             // save pool state back

                    vm_context_restore(vm, &saved_ctx);       // back to top-level pool
                    value_decref(MAKE_FUTURE(task));          // drop queue reference
                }
                if (vm->had_error) break;

                if (vm_drain_completions(vm) > 0) continue;  // finished workers woke futures

                if (vm->timers || vm->pending_workers > 0) { // wait on timers or workers
                    wait_for_next_timer(vm);
                    poll_timers(vm);
                } else if (!ran) {
                    break;              // queue empty, no timers, no workers — exit
                }
            }

            vm_context_restore(vm, &saved_ctx);             // restore top-level pool
            frame_cap = vm->frame_capacity;                 // refresh cached pointers
            frame_off = vm->frame_offset;
            vm->iterator_depth = saved_iter_depth;
            vm->table_iter_depth = saved_titer;
        }

        return !vm->had_error;
}
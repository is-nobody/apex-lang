// source/libraries/table_module.c
// Implementation of Table Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "table_module.h"
#include "vm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// compares two table keys, attempting numeric sort for integer keys
int compare_keys(const void* a, const void* b) {
    const Value* va = (const Value*)a;                                // cast first key
    const Value* vb = (const Value*)b;                                // cast second key
    if (IS_NUMBER(*va) && IS_NUMBER(*vb)) {                           // both are numbers
        double diff = AS_NUMBER(*va) - AS_NUMBER(*vb);                // compute difference
        return (diff > 0) - (diff < 0);                               // return -1, 0, or 1
    }
    if (IS_STRING(*va) && IS_STRING(*vb)) {                           // both are strings
        return strcmp(AS_STRING(*va)->chars, AS_STRING(*vb)->chars);  // compare lexicographically
    }
    return 0;                                                         // fallback, treat as equal
}

// dispatches all table module built-in functions
bool table_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;                                                         // vm unused
    
    if (arg_count < 1 || !IS_TABLE(args[0])) {                        // validate table argument
        *result = MAKE_NONE();                                        // invalid, return none
        return true;                                                  // builtin handled
    }
    
    Table* table = AS_TABLE(args[0]);                                 // unwrap table pointer
    
    if (strcmp(name, "table.size") == 0) {                            // get table size
        *result = MAKE_NUMBER(table_size(table));                     // count all entries
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "table.has") == 0) {                             // check if key exists
        if (arg_count < 2) {                                          // validate key argument
            *result = MAKE_NONE();                                    // missing key
            return true;                                              // builtin handled
        }
        
        *result = MAKE_BOOL(table_has(table, args[1]));               // check key existence
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "table.remove") == 0) {                          // remove key from table
        if (arg_count < 2) {                                          // validate key argument
            *result = MAKE_NONE();                                    // missing key
            return true;                                              // builtin handled
        }
        
        bool existed = table_has(table, args[1]);                     // check if key existed
        table_remove(table, args[1]);                                 // remove key-value pair
        *result = MAKE_BOOL(existed);                                 // return whether key existed
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "table.keys") == 0) {                                 // get sorted keys
        Table* result_table = table_create(8);                             // create result table
        *result = MAKE_TABLE(result_table);                                // box table as result
        int count;                                                         // key count
        Value* keys = table_keys(table, &count);                           // get all keys
        if (keys && count > 0) {                                           // check if keys exist
            qsort(keys, count, sizeof(Value), compare_keys);               // sort keys
            for (int i = 0; i < count; i++) {                              // iterate over sorted keys
                Value idx_key = MAKE_NUMBER((double)(i + 1));              // create numeric index key
                table_set(result_table, idx_key, vm_copy_value(keys[i]));  // store key at index
                value_decref(keys[i]);                                     // release key reference
                value_decref(idx_key);                                     // release index key
            }
            free(keys);                                                    // free keys array
        }
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "table.values") == 0) {                       // get sorted values
        Table* result_table = table_create(8);                     // create result table
        *result = MAKE_TABLE(result_table);                        // box table as result
        int count;                                                 // key count
        Value* keys = table_keys(table, &count);                   // get all keys
        if (keys && count > 0) {                                   // check if keys exist
            qsort(keys, count, sizeof(Value), compare_keys);       // sort keys
            for (int i = 0; i < count; i++) {                      // iterate over sorted keys
                Value val;                                         // value storage
                if (table_get(table, keys[i], &val)) {             // lookup value by key
                    Value idx_key = MAKE_NUMBER((double)(i + 1));  // create numeric index key
                    table_set(result_table, idx_key, val);         // store value at index
                    value_decref(idx_key);                         // release index key
                }
                value_decref(keys[i]);                             // release key reference
            }
            free(keys);                                            // free keys array
        }
        return true;                                               // builtin handled
    }
    
    if (strcmp(name, "table.clear") == 0) {                        // clear all entries
        table_clear(table);                                        // remove all key-value pairs
        *result = MAKE_BOOL(true);                                 // return true
        return true;                                               // builtin handled
    }
    
    if (strcmp(name, "table.copy") == 0) {                                     // shallow copy table
        Table* dst = table_create(table->capacity > 0 ? table->capacity : 8);  // create destination table
        *result = MAKE_TABLE(dst);                                             // box table as result
        
        if (table->array_count > 0) {                                                     // copy array part if exists
            dst->array_count = table->array_count;                                        // copy array count
            dst->array_capacity = table->array_capacity > 0 ? table->array_capacity : 8;  // copy capacity
            dst->array_part = (Value*)calloc(dst->array_capacity, sizeof(Value));         // allocate array
            for (int i = 0; i < table->array_count; i++) {                    // iterate over elements
                dst->array_part[i] = table->array_part[i];                    // copy element
                value_incref(dst->array_part[i]);                             // bump refcount for copied value
            }
            for (int i = table->array_count; i < dst->array_capacity; i++) {  // fill remaining with false
                dst->array_part[i] = MAKE_BOOL(false);                        // initialize empty slots
            }
        }
        
        if (table->entries) {                                                 // copy hash part if exists
            for (int i = 0; i < table->capacity; i++) {                       // iterate over buckets
                TableEntry* entry = table->entries[i];                        // get bucket head
                while (entry) {                                               // traverse chain
                    table_set(dst, entry->key, entry->value);                 // copy entry
                    entry = entry->next;                                      // advance to next
                }
            }
        }
        return true;                                                          // builtin handled
    }
    
    if (strcmp(name, "table.merge") == 0) {                                   // merge two tables
        if (arg_count < 2 || !IS_TABLE(args[1])) {                            // validate second table
            *result = MAKE_NONE();                                            // invalid second argument
            return true;                                                      // builtin handled
        }
        
        Table* dst = table_create(8);                                         // create destination table
        *result = MAKE_TABLE(dst);                                            // box table as result
        Table* src1 = table;                                                  // first source table
        Table* src2 = AS_TABLE(args[1]);                                      // second source table
        
        int total_array = src1->array_count + src2->array_count;                   // combined array size
        if (total_array > 0) {                                                     // copy array parts if any
            dst->array_count = total_array;                                        // set array count
            dst->array_capacity = total_array > 8 ? total_array : 8;               // set array capacity
            if (dst->array_part) free(dst->array_part);                            // free existing array
            dst->array_part = (Value*)calloc(dst->array_capacity, sizeof(Value));  // allocate new array
            
            for (int i = 0; i < src1->array_count; i++) {                      // copy first table array
                dst->array_part[i] = src1->array_part[i];                      // copy element
                value_incref(dst->array_part[i]);                              // bump refcount
            }
            
            for (int i = 0; i < src2->array_count; i++) {                      // copy second table array
                dst->array_part[src1->array_count + i] = src2->array_part[i];  // append after first
                value_incref(dst->array_part[src1->array_count + i]);          // bump refcount
            }
            
            for (int i = total_array; i < dst->array_capacity; i++) {          // fill remaining with false
                dst->array_part[i] = MAKE_BOOL(false);                         // initialize empty slots
            }
        }
        
        if (src1->entries) {                                       // copy first table hash entries
            for (int i = 0; i < src1->capacity; i++) {             // iterate over buckets
                TableEntry* entry = src1->entries[i];              // get bucket head
                while (entry) {                                    // traverse chain
                    if (!table_has(dst, entry->key)) {             // check if key already exists
                        table_set(dst, entry->key, entry->value);  // copy entry
                    }
                    entry = entry->next;                           // advance to next
                }
            }
        }
        if (src2->entries) {                                       // copy second table hash entries
            for (int i = 0; i < src2->capacity; i++) {             // iterate over buckets
                TableEntry* entry = src2->entries[i];              // get bucket head
                while (entry) {                                    // traverse chain
                    table_set(dst, entry->key, entry->value);      // copy entry (overwrites if duplicate)
                    entry = entry->next;                           // advance to next
                }
            }
        }
        return true;  // builtin handled
    }
    
    return false;     // not a recognized builtin
}
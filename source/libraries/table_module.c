#include "table_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int compare_keys(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    
    char* enda = NULL;
    char* endb = NULL;
    long na = strtol(sa, &enda, 10);
    long nb = strtol(sb, &endb, 10);
    
    if (enda != NULL && *enda == '\0' && endb != NULL && *endb == '\0') {
        return (na > nb) - (na < nb);
    }
    
    return strcmp(sa, sb);
}

bool table_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    if (arg_count < 1 || args[0].type != VAL_TABLE) return false;
    
    // table.size — return number of entries in the table
    if (strcmp(name, "table.size") == 0) {
        *result = vm_make_number(table_size(args[0].table));
        return true;
    }
    
    // table.has — check if a key exists in the table
    if (strcmp(name, "table.has") == 0) {
        if (arg_count >= 2 && args[1].type == VAL_STRING) {
            *result = vm_make_bool(table_has(args[0].table, args[1].string->chars));
        } else if (arg_count >= 2 && args[1].type == VAL_NUMBER) {
            char key[32];
            snprintf(key, sizeof(key), "%g", args[1].number);
            *result = vm_make_bool(table_has(args[0].table, key));
        }
        return true;
    }
    
    // table.remove — delete an entry by key
    if (strcmp(name, "table.remove") == 0) {
        if (arg_count >= 2) {
            char key[256];
            if (args[1].type == VAL_STRING) {
                strcpy(key, args[1].string->chars);
            } else if (args[1].type == VAL_NUMBER) {
                snprintf(key, sizeof(key), "%g", args[1].number);
            } else {
                return false;
            }
            table_remove(args[0].table, key);
        }
        *result = vm_make_bool(false);
        return true;
    }
    
    // table.keys — return a new table containing all keys
    if (strcmp(name, "table.keys") == 0) {
        *result = vm_make_table();
        Table* table = args[0].table;
        
        int count;
        char** keys = table_keys(table, &count);  // <-- ИСПОЛЬЗУЕМ API!
        
        if (keys && count > 0) {
            qsort(keys, count, sizeof(char*), compare_keys);
            
            for (int i = 0; i < count; i++) {
                char key_str[32];
                snprintf(key_str, sizeof(key_str), "%d", i + 1);  // 1-based indexing
                table_set(result->table, key_str, vm_make_string(keys[i]));
            }
            
            free(keys);
        }
        return true;
    }
    
    // table.values — return a new table containing all values
    if (strcmp(name, "table.values") == 0) {
        *result = vm_make_table();
        Table* table = args[0].table;
        
        int count;
        char** keys = table_keys(table, &count);  // <-- ИСПОЛЬЗУЕМ API!
        
        if (keys && count > 0) {
            qsort(keys, count, sizeof(char*), compare_keys);
            
            for (int i = 0; i < count; i++) {
                Value val;
                if (table_get(table, keys[i], &val)) {
                    char key_str[32];
                    snprintf(key_str, sizeof(key_str), "%d", i + 1);
                    table_set(result->table, key_str, val);
                    value_decref(&val);
                }
            }
            
            free(keys);
        }
        return true;
    }
    
    // table.clear — remove all entries from the table
    if (strcmp(name, "table.clear") == 0) {
        table_clear(args[0].table);
        *result = vm_make_bool(false);
        return true;
    }
    
    // table.copy — create a shallow copy of the table
    if (strcmp(name, "table.copy") == 0) {
        *result = vm_make_table();
        Table* src = args[0].table;
        
        int count;
        char** keys = table_keys(src, &count);  // <-- ИСПОЛЬЗУЕМ API!
        
        if (keys && count > 0) {
            for (int i = 0; i < count; i++) {
                Value val;
                if (table_get(src, keys[i], &val)) {
                    table_set(result->table, keys[i], vm_copy_value(val));
                    value_decref(&val);
                }
            }
            free(keys);
        }
        return true;
    }
    
    // table.merge — merge two tables, second table overwrites duplicate keys
    if (strcmp(name, "table.merge") == 0) {
        if (arg_count >= 2 && args[1].type == VAL_TABLE) {
            // Create a copy of the first table
            Table* copy = table_copy(args[0].table);
            if (copy) {
                *result = vm_make_table();
                // Replace the newly created table with our copy
                table_destroy(result->table);
                result->table = copy;
            } else {
                *result = vm_make_table();
            }
            
            // Merge second table's entries
            Table* src2 = args[1].table;
            int count;
            char** keys = table_keys(src2, &count);
            
            if (keys && count > 0) {
                for (int i = 0; i < count; i++) {
                    Value val;
                    if (table_get(src2, keys[i], &val)) {
                        table_set(result->table, keys[i], vm_copy_value(val));
                        value_decref(&val);
                    }
                }
                free(keys);
            }
        }
        return true;
    }
    
    return false;
}
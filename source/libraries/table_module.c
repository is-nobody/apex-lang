#include "table_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int compare_keys(const void* a, const void* b) {
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
    
    if (strcmp(name, "table.size") == 0) {
        *result = vm_make_number(table_size(args[0].table));
        return true;
    }
    
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
    
    // table.remove — FAST PATH for numeric keys
    if (strcmp(name, "table.remove") == 0) {
        if (arg_count >= 2) {
            if (args[1].type == VAL_NUMBER) {
                double num = args[1].number;
                if (num >= 1 && num == (int)num) {
                    int idx = (int)num - 1;
                    Table* t = args[0].table;
                    bool existed = (t->array_part != NULL && idx < t->array_count);
                    if (existed) {
                        value_decref(&t->array_part[idx]);
                        t->array_part[idx].type = VAL_BOOL;
                        t->array_part[idx].boolean = false;
                    }
                    *result = vm_make_bool(existed);
                    return true;
                }
            }
            // String or non-integer: old path
            char key[256] = {0};
            if (args[1].type == VAL_STRING) {
                strcpy(key, args[1].string->chars);
            } else if (args[1].type == VAL_NUMBER) {
                snprintf(key, sizeof(key), "%g", args[1].number);
            } else {
                return false;
            }
            bool existed = table_has(args[0].table, key);
            table_remove(args[0].table, key);
            *result = vm_make_bool(existed);
            return true;
        }
        return false;
    }
    
    if (strcmp(name, "table.keys") == 0) {
        *result = vm_make_table();
        Table* table = args[0].table;
        int count;
        char** keys = table_keys(table, &count);
        if (keys && count > 0) {
            qsort(keys, count, sizeof(char*), compare_keys);
            for (int i = 0; i < count; i++) {
                char key_str[32];
                snprintf(key_str, sizeof(key_str), "%d", i + 1);
                table_set(result->table, key_str, vm_make_string(keys[i]));
            }
            free(keys);
        }
        return true;
    }
    
    if (strcmp(name, "table.values") == 0) {
        *result = vm_make_table();
        Table* table = args[0].table;
        int count;
        char** keys = table_keys(table, &count);
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
    
    if (strcmp(name, "table.clear") == 0) {
        table_clear(args[0].table);
        *result = vm_make_bool(false);
        return true;
    }
    
    // table.copy — FAST PATH for array part
    if (strcmp(name, "table.copy") == 0) {
        *result = vm_make_table();
        Table* src = args[0].table;
        Table* dst = result->table;
        
        // Fast path: copy array part directly
        if (src->array_count > 0) {
            dst->array_count = src->array_count;
            dst->array_capacity = src->array_capacity > 0 ? src->array_capacity : 8;
            dst->array_part = (Value*)calloc(dst->array_capacity, sizeof(Value));
            for (int i = 0; i < src->array_count; i++) {
                dst->array_part[i] = src->array_part[i];
                value_incref(&dst->array_part[i]);
            }
            for (int i = src->array_count; i < dst->array_capacity; i++) {
                dst->array_part[i].type = VAL_BOOL;
                dst->array_part[i].boolean = false;
            }
        }
        
        // Copy hash part
        for (int i = 0; i < src->capacity; i++) {
            TableEntry* entry = src->entries[i];
            while (entry) {
                table_set(dst, entry->key, entry->value);
                entry = entry->next;
            }
        }
        return true;
    }
    
    // table.merge — FAST PATH for array parts
    if (strcmp(name, "table.merge") == 0) {
        if (arg_count >= 2 && args[1].type == VAL_TABLE) {
            *result = vm_make_table();
            Table* dst = result->table;
            Table* src1 = args[0].table;
            Table* src2 = args[1].table;
            
            // Fast path: merge array parts
            int max_array = src1->array_count > src2->array_count ? src1->array_count : src2->array_count;
            if (max_array > 0) {
                dst->array_count = max_array;
                dst->array_capacity = max_array > 8 ? max_array : 8;
                dst->array_part = (Value*)calloc(dst->array_capacity, sizeof(Value));
                
                for (int i = 0; i < max_array; i++) {
                    if (i < src2->array_count && src2->array_part[i].type != VAL_BOOL) {
                        dst->array_part[i] = src2->array_part[i];
                    } else if (i < src1->array_count) {
                        dst->array_part[i] = src1->array_part[i];
                    } else {
                        dst->array_part[i].type = VAL_BOOL;
                        dst->array_part[i].boolean = false;
                    }
                    value_incref(&dst->array_part[i]);
                }
            }
            
            // Merge hash parts (src2 overwrites src1)
            for (int i = 0; i < src1->capacity; i++) {
                TableEntry* entry = src1->entries[i];
                while (entry) {
                    if (!table_has(dst, entry->key)) {
                        table_set(dst, entry->key, entry->value);
                    }
                    entry = entry->next;
                }
            }
            for (int i = 0; i < src2->capacity; i++) {
                TableEntry* entry = src2->entries[i];
                while (entry) {
                    table_set(dst, entry->key, entry->value);
                    entry = entry->next;
                }
            }
            return true;
        }
        return false;
    }
    
    return false;
}
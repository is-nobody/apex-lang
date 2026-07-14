#include "table_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// compares two table keys, attempting numeric sort for integer keys
int compare_keys(const void* a, const void* b) {
    const Value* va = (const Value*)a;
    const Value* vb = (const Value*)b;
    if (va->type == VAL_NUMBER && vb->type == VAL_NUMBER) {
        return (va->number > vb->number) - (va->number < vb->number);
    }
    if (va->type == VAL_STRING && vb->type == VAL_STRING) {
        return strcmp(va->string->chars, vb->string->chars);
    }
    return (va->type > vb->type) - (va->type < vb->type);
}

// dispatches all table module built-in functions
bool table_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    
    if (arg_count < 1 || args[0].type != VAL_TABLE) {
        *result = vm_make_none();
        return true;
    }
    
    Table* table = args[0].table;
    
    if (strcmp(name, "table.size") == 0) {
        *result = vm_make_number(table_size(table));
        return true;
    }
    
    if (strcmp(name, "table.has") == 0) {
        if (arg_count < 2) {
            *result = vm_make_none();
            return true;
        }
        
        *result = vm_make_bool(table_has(table, args[1]));
        return true;
    }
    
    if (strcmp(name, "table.remove") == 0) {
        if (arg_count < 2) {
            *result = vm_make_none();
            return true;
        }
        
        bool existed = table_has(table, args[1]);
        table_remove(table, args[1]);
        *result = vm_make_bool(existed);
        return true;
    }
    
    if (strcmp(name, "table.keys") == 0) {
        *result = vm_make_table();
        int count;
        Value* keys = table_keys(table, &count);
        if (keys && count > 0) {
            qsort(keys, count, sizeof(Value), compare_keys);
            for (int i = 0; i < count; i++) {
                Value idx_key = vm_make_number((double)(i + 1));
                table_set(result->table, idx_key, vm_copy_value(keys[i]));
                value_decref(&keys[i]);
                value_decref(&idx_key);
            }
            free(keys);
        }
        return true;
    }
    
    if (strcmp(name, "table.values") == 0) {
        *result = vm_make_table();
        int count;
        Value* keys = table_keys(table, &count);
        if (keys && count > 0) {
            qsort(keys, count, sizeof(Value), compare_keys);
            for (int i = 0; i < count; i++) {
                Value val;
                if (table_get(table, keys[i], &val)) {
                    Value idx_key = vm_make_number((double)(i + 1));
                    table_set(result->table, idx_key, val);
                    value_decref(&idx_key);
                }
                value_decref(&keys[i]);
            }
            free(keys);
        }
        return true;
    }
    
    if (strcmp(name, "table.clear") == 0) {
        table_clear(table);
        *result = vm_make_none();
        return true;
    }
    
    if (strcmp(name, "table.copy") == 0) {
        *result = vm_make_table();
        Table* dst = result->table;
        
        if (table->array_count > 0) {
            dst->array_count = table->array_count;
            dst->array_capacity = table->array_capacity > 0 ? table->array_capacity : 8;
            dst->array_part = (Value*)calloc(dst->array_capacity, sizeof(Value));
            for (int i = 0; i < table->array_count; i++) {
                dst->array_part[i] = table->array_part[i];
                value_incref(&dst->array_part[i]);
            }
            for (int i = table->array_count; i < dst->array_capacity; i++) {
                dst->array_part[i].type = VAL_BOOL;
                dst->array_part[i].boolean = false;
            }
        }
        
        for (int i = 0; i < table->capacity; i++) {
            TableEntry* entry = table->entries[i];
            while (entry) {
                table_set(dst, entry->key, entry->value);
                entry = entry->next;
            }
        }
        return true;
    }
    
    if (strcmp(name, "table.merge") == 0) {
        if (arg_count < 2 || args[1].type != VAL_TABLE) {
            *result = vm_make_none();
            return true;
        }
        
        *result = vm_make_table();
        Table* dst = result->table;
        Table* src1 = table;
        Table* src2 = args[1].table;
        
        int total_array = src1->array_count + src2->array_count;
        if (total_array > 0) {
            dst->array_count = total_array;
            dst->array_capacity = total_array > 8 ? total_array : 8;
            dst->array_part = (Value*)calloc(dst->array_capacity, sizeof(Value));
            
            for (int i = 0; i < src1->array_count; i++) {
                dst->array_part[i] = src1->array_part[i];
                value_incref(&dst->array_part[i]);
            }
            
            for (int i = 0; i < src2->array_count; i++) {
                dst->array_part[src1->array_count + i] = src2->array_part[i];
                value_incref(&dst->array_part[src1->array_count + i]);
            }
            
            for (int i = total_array; i < dst->array_capacity; i++) {
                dst->array_part[i].type = VAL_BOOL;
                dst->array_part[i].boolean = false;
            }
        }
        
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
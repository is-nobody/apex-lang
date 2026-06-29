#ifndef TABLE_MODULE_H
#define TABLE_MODULE_H

#include "vm.h"

// dispatcher for table/collection built-ins (remove, has, size, keys, values, clear, copy, merge)
bool table_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

// comparator for sorting table keys (used by table.keys and table.values)
int compare_keys(const void* a, const void* b);

#endif
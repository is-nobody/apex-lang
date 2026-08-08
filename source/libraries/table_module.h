// source/libraries/table_module.h
// Implementation of Table Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef TABLE_MODULE_H
#define TABLE_MODULE_H

#include "vm.h"

// dispatcher for table module built-in functions
bool table_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

// comparator for sorting table keys (used by table.keys and table.values)
int compare_keys(const void* a, const void* b);

#endif
// source/libraries/csv_module.h
// Implementation of CSV Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef CSV_MODULE_H
#define CSV_MODULE_H

#include "vm.h"

// dispatcher for csv module built-in functions
bool csv_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
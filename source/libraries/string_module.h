// source/libraries/string_module.h
// Implementation of String Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef STRING_MODULE_H
#define STRING_MODULE_H

#include "vm.h"

// dispatcher for string module built-in functions
bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
// source/libraries/base_module.h
// Implementation of Base Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef BASE_MODULE_H
#define BASE_MODULE_H

#include "vm.h"

// dispatcher for base module built-in functions
bool base_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
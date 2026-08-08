// source/libraries/math_module.h
// Implementation of Math Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef MATH_MODULE_H
#define MATH_MODULE_H

#include "vm.h"

// dispatcher for math module built-in functions
bool math_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
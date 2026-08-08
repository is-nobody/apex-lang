// source/libraries/os_module.h
// Implementation of OS Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef OS_MODULE_H
#define OS_MODULE_H

#include "vm.h"

// dispatcher for os module built-in functions
bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
// source/libraries/sys_module.h
// Implementation of Sys Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef SYS_MODULE_H
#define SYS_MODULE_H

#include "vm.h"

// dispatcher for system module built-in functions
bool sys_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
// source/libraries/json_module.h
// Implementation of JSON Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef JSON_MODULE_H
#define JSON_MODULE_H

#include "vm.h"

// dispatcher for json module built-in functions
bool json_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
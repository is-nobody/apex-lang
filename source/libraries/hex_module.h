// source/libraries/hex_module.h
// Implementation of Hex Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef HEX_MODULE_H
#define HEX_MODULE_H

#include "vm.h"

// dispatcher for hex module built-in functions
bool hex_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
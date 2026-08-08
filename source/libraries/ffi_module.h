// source/libraries/ffi_module.h
// Implementation of FFI Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef FFI_MODULE_H
#define FFI_MODULE_H

#include "vm.h"

// dispatcher for ffi module built-in functions
bool ffi_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
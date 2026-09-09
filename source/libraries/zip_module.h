// source/libraries/zip_module.h
// Implementation of Zip Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef ZIP_MODULE_H
#define ZIP_MODULE_H

#include "vm.h"

// dispatcher for zip module built-in functions
bool zip_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
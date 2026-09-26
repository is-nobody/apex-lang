// source/libraries/datetime_module.h
// Implementation of Datetime Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef DATETIME_MODULE_H
#define DATETIME_MODULE_H

#include "vm.h"

// dispatcher for datetime module built-in functions
bool datetime_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
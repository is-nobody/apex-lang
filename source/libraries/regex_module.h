// source/libraries/regex_module.h
// Implementation of Regex Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef REGEX_MODULE_H
#define REGEX_MODULE_H

#include "vm.h"

// dispatcher for regex module built-in functions
bool regex_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
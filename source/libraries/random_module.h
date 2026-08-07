// source/libraries/random_module.h
// Implementation of Random Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef RANDOM_MODULE_H
#define RANDOM_MODULE_H

#include "vm.h"

// dispatcher for random number generation built-ins (randint, choice, shuffle, etc.)
bool random_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
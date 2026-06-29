#ifndef RANDOM_MODULE_H
#define RANDOM_MODULE_H

#include "vm.h"

// dispatcher for random number generation built-ins (randint, choice, shuffle, etc.)
bool random_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
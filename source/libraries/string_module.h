#ifndef STRING_MODULE_H
#define STRING_MODULE_H

#include "vm.h"

// dispatcher for string manipulation built-ins (len, lower, upper, split, join, etc.)
bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
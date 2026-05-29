#ifndef STRING_MODULE_H
#define STRING_MODULE_H

#include "vm.h"

bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
#ifndef MATH_MODULE_H
#define MATH_MODULE_H

#include "vm.h"

bool math_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
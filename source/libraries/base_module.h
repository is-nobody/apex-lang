#ifndef BASE_MODULE_H
#define BASE_MODULE_H
#include "vm.h"

bool base_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
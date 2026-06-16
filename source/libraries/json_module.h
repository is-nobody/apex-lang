#ifndef JSON_MODULE_H
#define JSON_MODULE_H
#include "vm.h"

bool json_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
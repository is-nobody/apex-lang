#ifndef REGEX_MODULE_H
#define REGEX_MODULE_H

#include "vm.h"

bool regex_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
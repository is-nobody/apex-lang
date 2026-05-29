#ifndef OS_MODULE_H
#define OS_MODULE_H

#include "vm.h"

bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
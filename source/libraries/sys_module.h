#ifndef SYS_MODULE_H
#define SYS_MODULE_H

#include "vm.h"

bool sys_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
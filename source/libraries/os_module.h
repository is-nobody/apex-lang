#ifndef OS_MODULE_H
#define OS_MODULE_H

#include "vm.h"

// dispatcher for operating system built-in functions (file i/o, process control, etc.)
bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
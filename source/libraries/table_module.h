#ifndef TABLE_MODULE_H
#define TABLE_MODULE_H

#include "vm.h"

bool table_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
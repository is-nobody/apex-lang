#ifndef CSV_MODULE_H
#define CSV_MODULE_H
#include "vm.h"

bool csv_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
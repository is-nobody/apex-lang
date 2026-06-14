#ifndef FILES_MODULE_H
#define FILES_MODULE_H

#include "vm.h"

bool files_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
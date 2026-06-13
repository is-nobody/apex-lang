#ifndef FFI_MODULE_H
#define FFI_MODULE_H

#include "vm.h"

bool ffi_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
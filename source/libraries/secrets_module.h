#ifndef SECRETS_MODULE_H
#define SECRETS_MODULE_H

#include "vm.h"

bool secrets_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
#ifndef SYS_MODULE_H
#define SYS_MODULE_H

#include "vm.h"

// dispatcher for system information built-ins (platform, hostname, user, environment, etc.)
bool sys_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
#ifndef CODECS_MODULE_H
#define CODECS_MODULE_H

#include "vm.h"

// dispatcher for all codecs module built-in functions (json, csv, xml, base64)
bool codecs_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
// source/libraries/codecs_module.h
// Implementation of Codecs Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef CODECS_MODULE_H
#define CODECS_MODULE_H

#include "vm.h"

// dispatcher for all codecs module built-in functions (json, csv, xml, base64)
bool codecs_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
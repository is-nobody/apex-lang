// source/libraries/network_module.h
// Implementation of Network Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef NETWORK_MODULE_H
#define NETWORK_MODULE_H

#include "vm.h"

// dispatcher for network module built-in functions
bool network_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
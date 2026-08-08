// source/libraries/crypto_module.h
// Implementation of Crypto Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef CRYPTO_MODULE_H
#define CRYPTO_MODULE_H

#include "vm.h"

// dispatcher for crypto module built-in functions
bool crypto_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

#endif
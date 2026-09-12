// source/jit/jit.h
// Self-contained x86-64 JIT for Apex numeric-pure functions.
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_JIT_H
#define APEX_JIT_H

#include "bytecode.h"
#include <stdbool.h>

// opaque jit context holding all per-chunk JIT state
typedef struct JITContext JITContext;

// analyses the chunk and builds a jit context, or returns null
JITContext* jit_create(BytecodeChunk* chunk);

// frees the jit context and unloads its executable memory
void jit_destroy(JITContext* ctx);

// returns true if function `func_idx` has a native implementation
bool jit_has_native(JITContext* ctx, int func_idx);

// returns true if function `func_idx` was inferred to return a bool
bool jit_returns_bool(JITContext* ctx, int func_idx);

// invokes a compiled function taking no arguments
double jit_call_0(JITContext* ctx, int func_idx);

// invokes a compiled function taking one double argument
double jit_call_1(JITContext* ctx, int func_idx, double a);

// invokes a compiled function taking two double arguments
double jit_call_2(JITContext* ctx, int func_idx, double a, double b);

// returns the number of functions that were successfully JIT-compiled
int jit_compiled_count(JITContext* ctx);

#endif
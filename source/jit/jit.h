// source/jit/jit.h
// Self-contained x86-64 JIT for Apex numeric-pure functions.
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_JIT_H
#define APEX_JIT_H

#include "bytecode.h"
#include <stdbool.h>
#include <stdint.h>

// opaque JIT context holding all per-chunk JIT state
typedef struct JITContext JITContext;

// result of attempting to run a native loop at the current pc
typedef enum {
    JIT_LOOP_NOT_APPLICABLE = 0,  // not a native loop — interpreter should proceed
    JIT_LOOP_RAN_NORMAL,          // ran natively — continue at exit_pc
    JIT_LOOP_RAN_FOR_NEXT,        // ran natively — also pop iterator frame
} JitLoopResult;

// analyses the chunk and builds a JIT context, or returns NULL
JITContext* jit_create(BytecodeChunk* chunk);

// frees the JIT context and unloads its executable memory
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

// runs a compiled loop natively when pc matches an entry and live-in slots are numbers
JitLoopResult jit_try_native_loop(JITContext* ctx, int pc, uint64_t* regs, int* exit_pc);

#endif
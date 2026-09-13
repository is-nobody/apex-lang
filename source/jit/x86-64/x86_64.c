// source/jit/x86-64/x86_64.c
// x86-64 jit backend glue: picks the abi for the target os at compile time.
// only one of x86_64_linux.c / x86_64_windows.c is linked in by cmake,
// so the #if below must match the file cmake selected.
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_abi.h"

#if defined(_WIN32) || defined(_WIN64)
  extern const X86_64Abi x86_64_abi_win64;
  #define APEX_X86_64_ABI (&x86_64_abi_win64)
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
  extern const X86_64Abi x86_64_abi_sysv;
  #define APEX_X86_64_ABI (&x86_64_abi_sysv)
#else
  #error "x86-64 jit: unsupported os"
#endif

// returns the abi instance for the current compilation target
const X86_64Abi* x86_64_get_abi(void) { return APEX_X86_64_ABI; }

// adapter: forwards to the abi-parameterised function emitter
static bool backend_emit_function(JITContext* ctx, CodeBuf* cb,
                                  int func_idx, void** out_fn) {
    return x86_64_emit_function(APEX_X86_64_ABI, ctx, cb, func_idx, out_fn);
}

// adapter: forwards to the abi-parameterised loop emitter
static bool backend_emit_loop(JITContext* ctx, CodeBuf* cb, JitLoopInfo* info) {
    return x86_64_emit_loop(APEX_X86_64_ABI, ctx, cb, info);
}

// adapter: allocates executable memory through the active abi
static void* backend_alloc_exec(size_t size) {
    return APEX_X86_64_ABI->os_alloc_exec(size);
}

// adapter: releases executable memory through the active abi
static void backend_free_exec(void* p, size_t size) {
    APEX_X86_64_ABI->os_free_exec(p, size);
}

// adapter: flips a region rw -> rx through the active abi
static bool backend_make_exec(void* p, size_t size) {
    return APEX_X86_64_ABI->os_make_exec(p, size);
}

// x86-64 backend dispatch table
const JitBackend jit_backend_x86_64 = {
    .name = "x86-64",                             // human-readable identifier
    .bytes_per_instruction = 256,                 // upper bound used for buffer sizing
    .emit_function = backend_emit_function,       // pure-function emitter
    .emit_loop = backend_emit_loop,               // numeric-loop emitter
    .alloc_exec = backend_alloc_exec,             // os rw allocation (mmap / VirtualAlloc)
    .free_exec = backend_free_exec,               // os release counterpart
    .make_exec = backend_make_exec,               // rw -> rx transition
};
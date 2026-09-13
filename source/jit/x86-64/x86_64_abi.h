// source/jit/x86-64/x86_64_abi.h
// abi descriptor: everything the emitters need that differs between
// sysv (linux/bsd) and microsoft x64 calling conventions.
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_JIT_X86_64_ABI_H
#define APEX_JIT_X86_64_ABI_H

#include "jit_internal.h"

typedef struct X86_64Abi {
    const char* name;                             // human-readable identifier ("x86-64 sysv")
    int frame_reg;                                // modrm base register holding the Value* / uint64_t* frame (sysv: X86_RDI, win64: X86_RCX)
    int frame_extra;                              // bytes reserved beneath the register slots (sysv: 0, win64: 48 = 16 saved xmm6/xmm7 + 32 shadow space)
    void (*emit_prologue_saves)(CodeBuf* cb, int base_frame);     // non-volatile register saves; base_frame is aligned offset from rbp
    void (*emit_epilogue_restores)(CodeBuf* cb, int base_frame);  // non-volatile register restores, emitted before leave; ret
    void* (*os_alloc_exec)(size_t size);          // os executable-memory allocation (mmap / VirtualAlloc)
    void  (*os_free_exec)(void* p, size_t size);  // release a region returned by os_alloc_exec
    bool  (*os_make_exec)(void* p, size_t size);  // flip rw -> rx on a region from os_alloc_exec
} X86_64Abi;

extern const X86_64Abi x86_64_abi_sysv;
extern const X86_64Abi x86_64_abi_win64;

// returns the abi instance matching the current compilation target
const X86_64Abi* x86_64_get_abi(void);

// shared emitters, parameterised by an abi
bool x86_64_emit_function(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                          int func_idx, void** out_fn);
bool x86_64_emit_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                      JitLoopInfo* info);

#endif // APEX_JIT_X86_64_ABI_H
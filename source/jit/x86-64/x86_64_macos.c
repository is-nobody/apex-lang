// source/jit/x86-64/x86_64_macos.c
// Implementation of macOS x86-64 ABI backend for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_common.h"
#include "x86_64_abi.h"
#include <sys/mman.h>

// macos x86-64 has no non-volatile xmm regs (same as sysv)
static void macos_prologue_saves(CodeBuf* cb, int base_frame) {
    (void)cb; (void)base_frame;
}

// macos x86-64 has no non-volatile xmm regs to restore
static void macos_epilogue_restores(CodeBuf* cb, int base_frame) {
    (void)cb; (void)base_frame;
}

// allocates rw memory via anonymous mmap, later flipped to rx by macos_make_exec
static void* macos_alloc_exec(size_t size) {
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

// releases a region previously returned by macos_alloc_exec
static void macos_free_exec(void* p, size_t size) {
    if (p) munmap(p, size);
}

// flips rw -> rx via mprotect; on x86-64 the icache is coherent so no flush needed
static bool macos_make_exec(void* p, size_t size) {
    return mprotect(p, size, PROT_READ | PROT_EXEC) == 0;
}

// macos x86-64 abi descriptor
const X86_64Abi x86_64_abi_macos = {
    .name = "x86-64 macOS",                           // human-readable identifier
    .frame_reg = X86_RDI,                             // sysv-style: first pointer arg in rdi
    .frame_extra = 0,                                 // no shadow space, no non-volatile xmm
    .emit_prologue_saves = macos_prologue_saves,      // no-op on sysv/macos
    .emit_epilogue_restores = macos_epilogue_restores,// no-op on sysv/macos
    .os_alloc_exec = macos_alloc_exec,                // mmap rw
    .os_free_exec = macos_free_exec,                  // munmap
    .os_make_exec = macos_make_exec,                  // mprotect rw -> rx
};
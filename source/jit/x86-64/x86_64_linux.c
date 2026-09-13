// source/jit/x86-64/x86_64_linux.c
// sysv x86-64 abi (linux/bsd). uses mmap/mprotect for executable memory.
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_common.h"
#include "x86_64_abi.h"
#include <sys/mman.h>

// sysv has no non-volatile xmm regs to save (xmm0-xmm15 are all caller-saved)
static void sysv_prologue_saves(CodeBuf* cb, int base_frame) {
    (void)cb; (void)base_frame;
}

// sysv has no non-volatile xmm regs to restore
static void sysv_epilogue_restores(CodeBuf* cb, int base_frame) {
    (void)cb; (void)base_frame;
}

// allocates rw memory via mmap, later flipped to rx by sysv_make_exec
static void* sysv_alloc_exec(size_t size) {
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

// releases a region previously returned by sysv_alloc_exec
static void sysv_free_exec(void* p, size_t size) {
    if (p) munmap(p, size);
}

// flips rw -> rx via mprotect
static bool sysv_make_exec(void* p, size_t size) {
    return mprotect(p, size, PROT_READ | PROT_EXEC) == 0;
}

// sysv/linux abi descriptor
const X86_64Abi x86_64_abi_sysv = {
    .name = "x86-64 SysV",                            // human-readable identifier
    .frame_reg = X86_RDI,                             // sysv passes the first pointer arg in rdi
    .frame_extra = 0,                                 // no shadow space, no non-volatile xmm
    .emit_prologue_saves = sysv_prologue_saves,       // no-op on sysv
    .emit_epilogue_restores = sysv_epilogue_restores, // no-op on sysv
    .os_alloc_exec = sysv_alloc_exec,                 // mmap rw
    .os_free_exec = sysv_free_exec,                   // munmap
    .os_make_exec = sysv_make_exec,                   // mprotect rw -> rx
};
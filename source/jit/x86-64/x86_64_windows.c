// source/jit/x86-64/x86_64_windows.c
// Implementation of Microsoft x64 (Win64) ABI backend for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_common.h"
#include "x86_64_abi.h"

#include <windows.h>

// saves xmm6 and xmm7 (win64 non-volatile) into the reserved frame slots
static void win64_prologue_saves(CodeBuf* cb, int base_frame) {
    x86_emit_movsd_store(cb, 6, -(base_frame + 8));   // xmm6 -> [rbp - base_frame -  8]
    x86_emit_movsd_store(cb, 7, -(base_frame + 16));  // xmm7 -> [rbp - base_frame - 16]
}

// restores xmm6 and xmm7 before leave; ret
static void win64_epilogue_restores(CodeBuf* cb, int base_frame) {
    x86_emit_movsd_load(cb, 6, -(base_frame + 8));    // [rbp - base_frame -  8] -> xmm6
    x86_emit_movsd_load(cb, 7, -(base_frame + 16));   // [rbp - base_frame - 16] -> xmm7
}

// allocates rw memory via VirtualAlloc, later flipped to rx
static void* win64_alloc_exec(size_t size) {
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

// releases a region previously returned by win64_alloc_exec
static void win64_free_exec(void* p, size_t size) {
    (void)size;                                       // ignored: MEM_RELEASE frees the whole region
    if (p) VirtualFree(p, 0, MEM_RELEASE);
}

// flips rw -> rx via VirtualProtect
static bool win64_make_exec(void* p, size_t size) {
    DWORD old;
    return VirtualProtect(p, size, PAGE_EXECUTE_READ, &old) != 0;
}

// win64 abi descriptor
const X86_64Abi x86_64_abi_win64 = {
    .name = "x86-64 Win64",                           // human-readable identifier
    .frame_reg = X86_RCX,                             // win64 passes the first pointer arg in rcx
    .frame_extra = 48,                                // 16 saved xmm6/xmm7 + 32 bytes shadow space
    .emit_prologue_saves = win64_prologue_saves,      // save non-volatile xmm regs
    .emit_epilogue_restores = win64_epilogue_restores,// restore non-volatile xmm regs
    .os_alloc_exec = win64_alloc_exec,                // VirtualAlloc rw
    .os_free_exec = win64_free_exec,                  // VirtualFree
    .os_make_exec = win64_make_exec,                  // VirtualProtect rw -> rx
};
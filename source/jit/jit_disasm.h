// source/jit/jit_disasm.h
// Minimal x86-64 disassembler for APEX_JIT_TRACE output.
// MIT license
#ifndef APEX_JIT_DISASM_H
#define APEX_JIT_DISASM_H

#include <stddef.h>
#include <stdint.h>

// Prints an objdump-style hex+mnemonic listing of `len` bytes at `code`
// to stderr, prefixed by `label` (may be NULL).
void jit_disasm_dump(const char* label, const uint8_t* code, size_t len);

#endif
// source/jit/jit_internal.h
// Shared types between the JIT core, the analysis pass, and arch backends.
// To add a new architecture:
//   1. implement jit_backend_<arch> in backend_<arch>.c
//   2. select it in jit_get_backend() inside jit.c
//   3. add the source file to CMakeLists.txt
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_JIT_INTERNAL_H
#define APEX_JIT_INTERNAL_H

#include "jit.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// upper bound on slots any backend can track
#define JIT_MAX_SLOTS 256

// growable byte buffer used during code emission
typedef struct {
    uint8_t* buf;  // destination buffer (mmap'd)
    size_t   len;  // bytes written so far
    size_t   cap;  // total buffer capacity
} CodeBuf;

// pending jump whose target is not yet known
typedef struct {
    size_t patch_at;   // offset in code buffer of the 4-byte rel32 placeholder
    int    target_pc;  // bytecode pc that the jump should resolve to
} JumpFixup;

// one numeric loop detected inside a function
typedef struct {
    int  entry_pc;      // first bytecode of the loop body
    int  back_edge_pc;  // JUMP returning to entry_pc
    int  exit_pc;       // bytecode reached when the loop exits
    bool is_for_next;   // true if the entry opcode is FOR_NEXT
    int  for_end_reg;   // FOR_NEXT: register holding the end bound
    int  for_step_reg;  // FOR_NEXT: register holding the step
    int  nregs;         // function frame size (max_registers)

    uint64_t live_in;   // bitmask of slots read inside the loop
    uint64_t live_out;  // bitmask of slots written inside the loop

    void (*native_fn)(uint64_t*);  // compiled entry, NULL if emit failed
} JitLoopInfo;

// per-chunk JIT state
struct JITContext {
    BytecodeChunk* chunk;  // bytecode being compiled
    int  func_count;       // number of functions in the chunk

    bool*  pure;           // per-function: numeric-pure?
    bool*  has_native;     // per-function: has native code emitted?
    JitReturnType* return_type;  // per-function: JIT_RET_NUMBER / BOOL / NONE
    int*   range_start;    // per-function: first bytecode pc
    int*   range_end;      // per-function: one past last bytecode pc

    void**   func_table;   // runtime slots for each compiled function
    uint8_t* code;         // executable code buffer
    size_t   code_size;    // size of that buffer

    int compiled_count;    // number of functions successfully emitted

    JitLoopInfo* loops;        // dynamic array of native loops
    int          loop_count;   // number of live entries
    int          loop_capacity;// allocated capacity
    int*         pc_to_loop;   // code_count entries: -1 or index into loops[]

    // reusable scratch buffers, sized once for the entire chunk,
    // to avoid per-function calloc/malloc during emission
    bool*      scratch_is_target;    // jump-target marks, one per bytecode pc
    int32_t*   scratch_label_off;    // emitted label offset per bytecode pc
    JumpFixup* scratch_fixups;       // pending jump fixups, one per bytecode pc
    uint8_t*   scratch_code_buf;     // scratch emitter buffer for the loop fixpoint pass
    size_t     scratch_code_buf_cap; // capacity of scratch_code_buf in bytes
};

// backend interface: one implementation per target architecture
typedef struct JitBackend {
    const char* name;             // human-readable identifier ("x86-64")
    size_t bytes_per_instruction; // upper bound used for buffer sizing

    // emit native code for a pure function; writes entry pointer into *out_fn
    bool (*emit_function)(JITContext* ctx, CodeBuf* cb, int func_idx, void** out_fn);

    // emit native code for a numeric loop; writes entry pointer into info->native_fn
    bool (*emit_loop)(JITContext* ctx, CodeBuf* cb, JitLoopInfo* info);
} JitBackend;

// x86-64 backend (defined in backend_x86_64.c)
extern const JitBackend jit_backend_x86_64;

// runs the full analysis pipeline on the chunk:
// computes function ranges, purity fixpoint, bool-return flags, loop candidates.
// returns false if there are no candidates at all.
bool jit_analyze(JITContext* ctx);

// appends one byte to the code buffer
static inline void emit_u8 (CodeBuf* b, uint8_t  v) { b->buf[b->len++] = v; }

// appends a little-endian u32 to the code buffer
static inline void emit_u32(CodeBuf* b, uint32_t v) { memcpy(b->buf+b->len, &v, 4); b->len += 4; }

// appends a little-endian u64 to the code buffer
static inline void emit_u64(CodeBuf* b, uint64_t v) { memcpy(b->buf+b->len, &v, 8); b->len += 8; }

// appends a little-endian i32 to the code buffer
static inline void emit_i32(CodeBuf* b, int32_t  v) { memcpy(b->buf+b->len, &v, 4); b->len += 4; }

#endif
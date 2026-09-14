// source/jit/jit_internal.h
// Implementation of shared JIT internal types for Apex language
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

// loop kind: distinguishes the entry opcode family
typedef enum {
    JIT_LOOP_NUMERIC_FOR = 0,  // OP_FOR_NEXT entry
    JIT_LOOP_CONDITION   = 1,  // OP_JUMP_IF_* entry
    JIT_LOOP_TABLE_ITER  = 2,  // OP_TABLE_ITER_NEXT entry
} JitLoopKind;

// how a live-in slot is validated before the native loop runs
typedef enum {
    JIT_SLOT_NUM = 0,  // must satisfy IS_NUMBER
    JIT_SLOT_TABLE,    // must be a table with usable array part
} JitSlotKind;

// table usage detected inside a numeric loop body (baseline: at most one table per loop)
typedef struct {
    bool used;               // any table op seen?
    int  slot;               // register holding the table
    int  min_count;          // required array_count (max static index accessed)
    int  max_idx;            // for SET_INT: required array_capacity
    bool written;            // has OP_TABLE_SET_INT
    bool indexed_by_counter; // has OP_TABLE_GET with key == for_var_reg
} JitTableUse;

// one numeric loop detected inside a function
typedef struct {
    int  entry_pc;      // first bytecode of the loop body
    int  back_edge_pc;  // JUMP returning to entry_pc
    int  exit_pc;       // bytecode reached when the loop exits
    JitLoopKind kind;   // which family of entry opcodes

    int  for_var_reg;   // FOR_NEXT: counter slot; -1 otherwise
    int  for_end_reg;   // FOR_NEXT: end bound slot; -1 otherwise
    int  for_step_reg;  // FOR_NEXT: step slot; -1 otherwise

    uint64_t live_in;   // bitmask of slots read inside the loop
    uint64_t live_out;  // bitmask of slots written inside the loop
    uint64_t ref_writes;  // slots whose last in-loop write came from TABLE_GET/_INT

    JitSlotKind live_in_kind[JIT_MAX_SLOTS];  // per-slot validation kind
    bool        touches_tables;               // any table op seen -> NaN checks on arithmetic

    JitTableUse table;  // baseline: at most one table per loop

    int  nregs;         // function frame size (max_registers)

    bool trace_entered;  // APEX_JIT_TRACE_BUILD: first-entry dump already printed?

    void (*native_fn)(uint64_t*);  // compiled entry, NULL if emit failed
} JitLoopInfo;

// backend interface, declared before JITContext so the context can hold a pointer back to it
typedef struct JitBackend {
    const char* name;             // human-readable identifier ("x86-64")
    size_t bytes_per_instruction; // upper bound used for buffer sizing

    bool (*emit_function)(JITContext* ctx, CodeBuf* cb, int func_idx, void** out_fn);  // emits native code for a pure function
    bool (*emit_loop)(JITContext* ctx, CodeBuf* cb, JitLoopInfo* info);                // emits native code for a numeric or table loop

    void* (*alloc_exec)(size_t size);          // os executable-memory allocation (mmap / VirtualAlloc)
    void  (*free_exec)(void* p, size_t size);  // release a region returned by alloc_exec
    bool  (*make_exec)(void* p, size_t size);  // flip rw -> rx on a region from alloc_exec
} JitBackend;

// per-chunk jit state
struct JITContext {
    BytecodeChunk* chunk;             // bytecode being compiled
    int  func_count;                  // number of functions in the chunk

    const JitBackend* backend;        // owning backend, used to free the code page

    bool*  pure;                      // per-function: numeric-pure?
    bool*  has_native;                // per-function: has native code emitted?
    JitReturnType* return_type;       // per-function: JIT_RET_NUMBER / BOOL / NONE
    int*   range_start;               // per-function: first bytecode pc
    int*   range_end;                 // per-function: one past last bytecode pc

    void**   func_table;              // runtime slots for each compiled function
    uint8_t* code;                    // executable code buffer
    size_t   code_size;               // size of that buffer

    int compiled_count;               // number of functions successfully emitted

    JitLoopInfo* loops;               // dynamic array of native loops
    int          loop_count;          // number of live entries
    int          loop_capacity;       // allocated capacity
    int*         pc_to_loop;          // code_count entries: -1 or index into loops[]

    void* vm;                         // opaque VM pointer for numeric-for seeding (set via jit_set_vm)

    bool*      scratch_is_target;     // reusable: jump-target marks, one per bytecode pc
    int32_t*   scratch_label_off;     // reusable: emitted label offset per bytecode pc
    JumpFixup* scratch_fixups;        // reusable: pending jump fixups, one per bytecode pc
    uint8_t*   scratch_code_buf;      // reusable: scratch emitter buffer for the loop fixpoint pass
    size_t     scratch_code_buf_cap;  // capacity of scratch_code_buf in bytes
};

// x86-64 backend (defined in source/jit/x86-64/x86_64.c)
extern const JitBackend jit_backend_x86_64;

// runs the full analysis pipeline on the chunk: function ranges, purity fixpoint, return kinds, loop candidates
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
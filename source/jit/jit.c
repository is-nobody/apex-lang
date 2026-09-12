// source/jit/jit.c
// JIT core — context lifecycle, backend selection, dispatch.
// Architecture-independent; delegates code emission to a JitBackend.
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit_internal.h"
#include "vm.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>

// returns the backend for the compile-target architecture, or NULL
static const JitBackend* jit_get_backend(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return &jit_backend_x86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return NULL;  // aarch64 backend not implemented yet
#else
    return NULL;
#endif
}

// compiles all numeric-pure functions and returns a jit context (or null)
JITContext* jit_create(BytecodeChunk* chunk) {
    if (!chunk || chunk->func_count <= 1) return NULL;           // nothing to compile

    const JitBackend* be = jit_get_backend();                    // pick backend
    if (!be) return NULL;                                        // no backend for this arch

    JITContext* ctx = (JITContext*)calloc(1, sizeof(JITContext));  // zero-initialised state
    if (!ctx) return NULL;                                         // allocation failed
    ctx->chunk = chunk;                                            // remember bytecode
    ctx->func_count = chunk->func_count;                           // number of functions

    int n = ctx->func_count;                                     // shorthand
    ctx->pure         = (bool*)calloc(n, sizeof(bool));          // per-fn purity
    ctx->has_native   = (bool*)calloc(n, sizeof(bool));          // per-fn code ptr
    ctx->range_start  = (int*) calloc(n, sizeof(int));           // per-fn pc start
    ctx->range_end    = (int*) calloc(n, sizeof(int));           // per-fn pc end
    ctx->func_table   = (void**)calloc(n, sizeof(void*));        // per-fn runtime slot
    ctx->returns_bool = (bool*)calloc(n, sizeof(bool));          // per-fn bool flag

    if (!ctx->pure || !ctx->has_native || !ctx->range_start ||   // verify allocations
        !ctx->range_end || !ctx->func_table || !ctx->returns_bool) {
        jit_destroy(ctx);                                        // cleanup on failure
        return NULL;
    }

    if (!jit_analyze(ctx)) { jit_destroy(ctx); return NULL; }    // purity + loops

    size_t cap = (size_t)chunk->code_count * be->bytes_per_instruction + 16384;
    ctx->code = (uint8_t*)mmap(NULL, cap, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);  // anonymous page
    if (ctx->code == MAP_FAILED) {                               // mmap failed
        ctx->code = NULL;                                        // clear pointer
        jit_destroy(ctx);                                        // cleanup
        return NULL;
    }
    ctx->code_size = cap;                                        // remember for munmap

    CodeBuf cb = { ctx->code, 0, cap };                          // code emission state

    for (int i = 0; i < n; i++) {                                // emit each pure fn
        if (!ctx->pure[i]) continue;                             // skip non-pure
        if (!be->emit_function(ctx, &cb, i, &ctx->func_table[i])) {
            ctx->func_table[i] = NULL;                           // clear partial slot
            continue;
        }
        ctx->has_native[i] = true;                               // mark as compiled
        ctx->compiled_count++;                                   // count successful
    }

    for (int i = 0; i < ctx->loop_count; i++) {                  // emit each loop
        if (!be->emit_loop(ctx, &cb, &ctx->loops[i])) {
            ctx->loops[i].native_fn = NULL;
            continue;
        }
        ctx->compiled_count++;
    }

    if (ctx->compiled_count == 0) { jit_destroy(ctx); return NULL; }  // nothing usable

    // build the pc → loop lookup table for loops that compiled successfully
    if (ctx->loop_count > 0) {
        ctx->pc_to_loop = (int*)malloc(sizeof(int) * chunk->code_count);
        if (ctx->pc_to_loop) {
            for (int i = 0; i < chunk->code_count; i++) ctx->pc_to_loop[i] = -1;
            for (int i = 0; i < ctx->loop_count; i++) {
                if (!ctx->loops[i].native_fn) continue;
                ctx->pc_to_loop[ctx->loops[i].entry_pc] = i;
            }
        }
    }

    if (mprotect(ctx->code, ctx->code_size, PROT_READ | PROT_EXEC) != 0) {
        jit_destroy(ctx);                                        // flip RW -> RX failed
        return NULL;
    }
    __builtin___clear_cache((char*)ctx->code, (char*)ctx->code + cb.len);  // icache flush

    if (getenv("APEX_JIT_DEBUG")) {
        fprintf(stderr, "[jit/%s] functions: %d, loops: %d (native: %d)\n",
                be->name, n, ctx->loop_count, ctx->compiled_count);
        for (int i = 0; i < ctx->loop_count; i++) {
            fprintf(stderr, "  loop entry=%d back=%d exit=%d for_next=%d native=%p\n",
                    ctx->loops[i].entry_pc,
                    ctx->loops[i].back_edge_pc,
                    ctx->loops[i].exit_pc,
                    ctx->loops[i].is_for_next,
                    (void*)ctx->loops[i].native_fn);
        }
    }

    return ctx;                                                  // success
}

// releases all resources held by the JIT context, including the code page
void jit_destroy(JITContext* ctx) {
    if (!ctx) return;                                            // null guard
    if (ctx->code) munmap(ctx->code, ctx->code_size);            // release executable page
    free(ctx->pure);                                             // free purity flags
    free(ctx->has_native);                                       // free native flags
    free(ctx->range_start);                                      // free range starts
    free(ctx->range_end);                                        // free range ends
    free(ctx->func_table);                                       // free runtime slots
    free(ctx->returns_bool);                                     // free bool flags
    free(ctx->loops);                                            // free loop info array
    free(ctx->pc_to_loop);                                       // free pc->loop lookup
    free(ctx);                                                   // free context itself
}

// returns true if function `func_idx` has a native implementation
bool jit_has_native(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return false;  // validate args
    return ctx->has_native[func_idx];                            // true if code was emitted
}

// returns true if function `func_idx` was inferred to return a bool
bool jit_returns_bool(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return false;  // validate args
    return ctx->returns_bool[func_idx];                          // true if returns bool
}

// invokes a compiled function taking no arguments
double jit_call_0(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return 0.0;  // validate args
    void* p = ctx->func_table[func_idx];                         // fetch native entry pointer
    if (!p) return 0.0;                                          // function was not compiled
    double (*fn)(void);                                          // declare fn-ptr type
    memcpy(&fn, &p, sizeof(void*));                              // avoid fn-ptr cast UB
    return fn();                                                 // call native code
}

// invokes a compiled function taking one double argument
double jit_call_1(JITContext* ctx, int func_idx, double a) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return 0.0;  // validate args
    void* p = ctx->func_table[func_idx];                         // fetch native entry pointer
    if (!p) return 0.0;                                          // function was not compiled
    double (*fn)(double);                                        // declare fn-ptr type
    memcpy(&fn, &p, sizeof(void*));                              // avoid fn-ptr cast UB
    return fn(a);                                                // call native code with arg
}

// invokes a compiled function taking two double arguments
double jit_call_2(JITContext* ctx, int func_idx, double a, double b) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return 0.0;  // validate args
    void* p = ctx->func_table[func_idx];                         // fetch native entry pointer
    if (!p) return 0.0;                                          // function was not compiled
    double (*fn)(double, double);                                // declare fn-ptr type
    memcpy(&fn, &p, sizeof(void*));                              // avoid fn-ptr cast UB
    return fn(a, b);                                             // call native code with args
}

// returns the number of functions that were successfully JIT-compiled
int jit_compiled_count(JITContext* ctx) {
    return ctx ? ctx->compiled_count : 0;                        // number of JITted functions
}

// if the current bytecode pc is a native loop entry and all live-in slots are numeric, runs the loop natively and writes the exit pc to *exit_pc
JitLoopResult jit_try_native_loop(JITContext* ctx, int pc, uint64_t* regs, int* exit_pc) {
    if (!ctx || !ctx->pc_to_loop) return JIT_LOOP_NOT_APPLICABLE;
    if (pc < 0 || pc >= ctx->chunk->code_count) return JIT_LOOP_NOT_APPLICABLE;
    int li = ctx->pc_to_loop[pc];
    if (li < 0) return JIT_LOOP_NOT_APPLICABLE;
    JitLoopInfo* info = &ctx->loops[li];
    if (!info->native_fn) return JIT_LOOP_NOT_APPLICABLE;

    uint64_t m = info->live_in;                              // type-check live-in slots
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) return JIT_LOOP_NOT_APPLICABLE;
        if (IS_NUMBER(regs[s])) continue;
        return JIT_LOOP_NOT_APPLICABLE;                      // not a number — fall back
    }

    info->native_fn(regs);                                   // run the native loop
    *exit_pc = info->exit_pc;
    return info->is_for_next ? JIT_LOOP_RAN_FOR_NEXT : JIT_LOOP_RAN_NORMAL;
}
// source/jit/jit.c
// Implementation of JIT core for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit_internal.h"
#include "vm.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(APEX_JIT_TRACE_BUILD) && APEX_JIT_TRACE_BUILD
// lazy-cached env lookup: getenv is called once per process, not per loop entry
static int jit_trace_env = -1;

// returns true if APEX_JIT_TRACE is set; result is cached after first call
static bool jit_trace_enabled(void) {
    if (jit_trace_env < 0)
        jit_trace_env = getenv("APEX_JIT_TRACE") ? 1 : 0;
    return jit_trace_env != 0;
}

// dumps live-in slots on the first successful entry into a given loop
static void jit_trace_first_entry(JitLoopInfo* info, int pc, uint64_t* regs,
                                  bool counter_write) {
    if (!jit_trace_enabled() || info->trace_entered) return;
    info->trace_entered = true;                              // never print again for this loop

    fprintf(stderr, "[loop pc=%d kind=%d live_in=%llx counter_write=%d]\n",
            pc, info->kind, (unsigned long long)info->live_in, counter_write);
    for (int s = 0; s < 32; s++) {
        if (info->live_in & (1ULL << s)) {
            fprintf(stderr, "  slot %d = 0x%016llx type=%d table_p=%p count=%d\n",
                    s, (unsigned long long)regs[s], (int)GET_TYPE(regs[s]),
                    IS_TABLE(regs[s]) ? (void*)AS_TABLE(regs[s]) : NULL,
                    IS_TABLE(regs[s]) ? AS_TABLE(regs[s])->array_count : -1);
        }
    }
}
#else
// release build: everything collapses to nothing
static inline bool jit_trace_enabled(void) { return false; }

static inline void jit_trace_first_entry(JitLoopInfo* info, int pc, uint64_t* regs,
                                         bool counter_write) {
    (void)info; (void)pc; (void)regs; (void)counter_write;
}
#endif

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

    const JitBackend* be = jit_get_backend();                    // pick backend for this arch
    if (!be) return NULL;                                        // no backend, no jit

    JITContext* ctx = (JITContext*)calloc(1, sizeof(JITContext));  // zero-initialised state
    if (!ctx) return NULL;                                         // allocation failed
    ctx->chunk = chunk;                                            // remember bytecode
    ctx->func_count = chunk->func_count;                           // number of functions
    ctx->backend = be;                                             // backend owns the code page

    int n = ctx->func_count;                                     // shorthand
    ctx->pure         = (bool*)calloc(n, sizeof(bool));          // per-fn purity
    ctx->has_native   = (bool*)calloc(n, sizeof(bool));          // per-fn: code emitted?
    ctx->range_start  = (int*) calloc(n, sizeof(int));           // per-fn first pc
    ctx->range_end    = (int*) calloc(n, sizeof(int));           // per-fn one past last pc
    ctx->func_table   = (void**)calloc(n, sizeof(void*));        // per-fn runtime entry slot
    ctx->return_type  = (JitReturnType*)calloc(n, sizeof(JitReturnType));  // per-fn return kind

    if (!ctx->pure || !ctx->has_native || !ctx->range_start ||   // verify allocations
        !ctx->range_end || !ctx->func_table || !ctx->return_type) {
        jit_destroy(ctx);                                        // cleanup on failure
        return NULL;
    }

    if (!jit_analyze(ctx)) { jit_destroy(ctx); return NULL; }    // purity + loops

    // allocate executable memory via the backend (mmap / virtualalloc)
    size_t cap = (size_t)chunk->code_count * be->bytes_per_instruction + 16384;
    ctx->code = (uint8_t*)be->alloc_exec(cap);
    if (!ctx->code) {                                            // allocation failed
        jit_destroy(ctx);                                        // cleanup
        return NULL;
    }
    ctx->code_size = cap;                                        // remember for free_exec

    // reusable scratch buffers, sized once for the whole chunk
    int code_count = chunk->code_count;
    ctx->scratch_is_target = (bool*)calloc(code_count, sizeof(bool));
    ctx->scratch_label_off = (int32_t*)malloc(sizeof(int32_t) * code_count);
    ctx->scratch_fixups    = (JumpFixup*)malloc(sizeof(JumpFixup) * code_count);
    ctx->scratch_code_buf_cap = (size_t)code_count * be->bytes_per_instruction + 1024;
    ctx->scratch_code_buf  = (uint8_t*)malloc(ctx->scratch_code_buf_cap);
    if (!ctx->scratch_is_target || !ctx->scratch_label_off ||    // verify scratch allocations
        !ctx->scratch_fixups || !ctx->scratch_code_buf) {
        jit_destroy(ctx);                                        // cleanup on failure
        return NULL;
    }

    CodeBuf cb = { ctx->code, 0, cap };                          // code emission state

    for (int i = 0; i < n; i++) {                                // emit each pure function
        if (!ctx->pure[i]) continue;                             // skip non-pure
        if (!be->emit_function(ctx, &cb, i, &ctx->func_table[i])) {
            ctx->func_table[i] = NULL;                           // clear partial slot
            continue;                                            // emit failed, move on
        }
        ctx->has_native[i] = true;                               // mark as compiled
        ctx->compiled_count++;                                   // count successful
    }

    for (int i = 0; i < ctx->loop_count; i++) {                  // emit each loop body
        if (!be->emit_loop(ctx, &cb, &ctx->loops[i])) {
            ctx->loops[i].native_fn = NULL;                      // emit failed, mark unusable
            continue;
        }
        ctx->compiled_count++;                                   // count successful
    }

    if (ctx->compiled_count == 0) { jit_destroy(ctx); return NULL; }  // nothing usable

    // build the pc -> loop lookup used by jit_try_native_loop on each entry pc
    if (ctx->loop_count > 0) {
        ctx->pc_to_loop = (int*)malloc(sizeof(int) * chunk->code_count);
        if (ctx->pc_to_loop) {
            for (int i = 0; i < chunk->code_count; i++) ctx->pc_to_loop[i] = -1;  // default: not a loop
            for (int i = 0; i < ctx->loop_count; i++) {
                if (!ctx->loops[i].native_fn) continue;          // skip failed emits
                ctx->pc_to_loop[ctx->loops[i].entry_pc] = i;     // entry pc -> loop index
            }
        }
    }

    // flip rw -> rx through the backend (mprotect on linux, virtualprotect on windows)
    if (!be->make_exec(ctx->code, ctx->code_size)) {
        jit_destroy(ctx);                                        // flip failed
        return NULL;
    }
    __builtin___clear_cache((char*)ctx->code, (char*)ctx->code + cb.len);  // icache flush

    return ctx;                                                  // success
}

// releases all resources held by the JIT context, including the code page
void jit_destroy(JITContext* ctx) {
    if (!ctx) return;                                            // null guard
    // release the executable page through the backend that allocated it
    if (ctx->code && ctx->backend) {
        ctx->backend->free_exec(ctx->code, ctx->code_size);
    }
    free(ctx->pure);                                             // free purity flags
    free(ctx->has_native);                                       // free native flags
    free(ctx->range_start);                                      // free range starts
    free(ctx->range_end);                                        // free range ends
    free(ctx->func_table);                                       // free runtime slots
    free(ctx->return_type);                                      // free return-kind table
    free(ctx->loops);                                            // free loop info array
    free(ctx->pc_to_loop);                                       // free pc->loop lookup
    free(ctx->scratch_is_target);                                // free shared jump-target marks
    free(ctx->scratch_label_off);                                // free shared label offsets
    free(ctx->scratch_fixups);                                   // free shared jump fixups
    free(ctx->scratch_code_buf);                                 // free shared scratch emitter
    free(ctx);                                                   // free context itself
}

// returns true if function `func_idx` has a native implementation
bool jit_has_native(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return false;  // validate args
    return ctx->has_native[func_idx];                            // true if code was emitted
}

// returns how function `func_idx` returns its value (number, bool, or none)
JitReturnType jit_return_type(JITContext* ctx, int func_idx) {
    if (!ctx || func_idx < 0 || func_idx >= ctx->func_count) return JIT_RET_NUMBER;  // fallback
    return ctx->return_type[func_idx];                           // declared return kind
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

// back-pointer for numeric-for reseed; ctx dies before vm so no dangling risk
void jit_set_vm(JITContext* ctx, void* vm) { if (ctx) ctx->vm = vm; }

// checks a single live-in slot against the loop's expected kind
static bool live_in_slot_ok(Value v, JitSlotKind kind, JitTableUse* use) {
    if (kind == JIT_SLOT_NUM) {
        return IS_NUMBER(v);                               // numeric slot: any unboxed double
    }
    if (!IS_TABLE(v)) return false;                        // table slot: must be a table
    Table* t = AS_TABLE(v);
    if (t->array_part == NULL) return false;               // empty table, nothing to read
    if (t->hash_count != 0) return false;                  // only array-only tables
    if (t->array_count < use->min_count) return false;     // static index out of bounds
    if (use->written && t->array_capacity < use->max_idx) return false;  // set would grow the array
    return true;
}
// ensures t->array_part has capacity for `end` slots and array_count >= end;
// uses table_set_int on the last slot so existing entries are preserved (the
// slot at end-1 is only touched when it is already NONE / freshly grown)
static bool counter_write_prepare(Table* t, int end) {
    if (!t || end < 1) return false;
    if (end > t->array_count) {
        table_set_int(t, end - 1, MAKE_NONE());
        if (t->array_count < end) return false;   // allocation failed
    }
    return true;
}

// runs a compiled loop natively if its entry pc matches and all guards pass
JitLoopResult jit_try_native_loop(JITContext* ctx, int pc, uint64_t* regs, int* exit_pc) {
    if (!ctx || !ctx->pc_to_loop) return JIT_LOOP_NOT_APPLICABLE;   // no jit, no loops
    if (pc < 0 || pc >= ctx->chunk->code_count) return JIT_LOOP_NOT_APPLICABLE;  // pc out of range
    int li = ctx->pc_to_loop[pc];
    if (li < 0) return JIT_LOOP_NOT_APPLICABLE;                     // pc is not a loop entry
    JitLoopInfo* info = &ctx->loops[li];
    if (!info->native_fn) return JIT_LOOP_NOT_APPLICABLE;           // emit failed, no native code

    // counter-indexed writes grow the array themselves; the table slot's
    // live_in check would otherwise reject fresh / undersized arrays
    bool counter_write =
        info->kind == JIT_LOOP_NUMERIC_FOR &&
        info->table.written && info->table.indexed_by_counter;
    int table_slot = info->table.used ? info->table.slot : -1;

    uint64_t m = info->live_in;
    while (m) {                                                     // validate every live-in slot
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) return JIT_LOOP_NOT_APPLICABLE;                // slot beyond tracking range
        if (counter_write && s == table_slot) continue;             // handled explicitly below
        if (!live_in_slot_ok(regs[s], info->live_in_kind[s], &info->table)) {
            return JIT_LOOP_NOT_APPLICABLE;                         // wrong type or table shape
        }
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && info->table.indexed_by_counter) {
        Value vv = regs[info->for_var_reg];
        Value ve = regs[info->for_end_reg];
        Value vs = regs[info->for_step_reg];
        if (!IS_NUMBER(vv) || !IS_NUMBER(ve) || !IS_NUMBER(vs)) return JIT_LOOP_NOT_APPLICABLE;  // counter/end/step must be numeric
        double start = AS_NUMBER(vv);
        double end   = AS_NUMBER(ve);
        double step  = AS_NUMBER(vs);
        if (start != (double)(long long)start) return JIT_LOOP_NOT_APPLICABLE;  // start must be integer
        if (step  != (double)(long long)step)  return JIT_LOOP_NOT_APPLICABLE;  // step must be integer
        if (end   != (double)(long long)end)   return JIT_LOOP_NOT_APPLICABLE;  // end must be integer
        if (start < 1) return JIT_LOOP_NOT_APPLICABLE;                          // keys are 1-based
        if (start > end) return JIT_LOOP_NOT_APPLICABLE;                        // zero-iteration loop
        if (end > 2147483647.0) return JIT_LOOP_NOT_APPLICABLE;                 // array index range

        Value tv = regs[info->table.slot];
        if (!IS_TABLE(tv)) return JIT_LOOP_NOT_APPLICABLE;
        Table* t = AS_TABLE(tv);

        if (info->table.written) {
            // grow + bump count up-front so the native loop can write directly
            // into array_part without realloc and later reads see the entries
            if (!counter_write_prepare(t, (int)end)) return JIT_LOOP_NOT_APPLICABLE;
        } else {
            // counter-indexed read: every read must land on a populated slot
            if (t->array_part == NULL) return JIT_LOOP_NOT_APPLICABLE;
            if (end > (double)t->array_count) return JIT_LOOP_NOT_APPLICABLE;
        }
    }

    if (info->kind == JIT_LOOP_TABLE_ITER) {
        Value tv = regs[info->table.slot];
        if (!IS_TABLE(tv)) return JIT_LOOP_NOT_APPLICABLE;
        Table* t = AS_TABLE(tv);
        if (t->array_count <= 0) return JIT_LOOP_NOT_APPLICABLE;    // nothing to iterate
    }

    jit_trace_first_entry(info, pc, regs, counter_write);           // one line per loop per run

    if (info->kind == JIT_LOOP_NUMERIC_FOR && ctx->vm) {            // reseed counter from interpreter
        VM* v = (VM*)ctx->vm;
        if (v->iterator_depth >= 0 && info->for_var_reg >= 0) {
            double idx = v->iterator_stack[v->iterator_depth].index;
            regs[info->for_var_reg] = MAKE_NUMBER(idx);             // interpreter is source of truth
        }
    }

    info->native_fn(regs);                                          // run the compiled loop
    *exit_pc = info->exit_pc;

    // the native loop wrote directly into array_part and never touched
    // array_count; sync it so later interpreter / JIT reads see the entries
    if (counter_write && table_slot >= 0) {
        Value tv = regs[table_slot];
        if (IS_TABLE(tv)) {
            Table* t = AS_TABLE(tv);
            if (t->array_part) {
                int last = (int)AS_NUMBER(regs[info->for_var_reg]);
                if (last > t->array_count) t->array_count = last;
            }
        }
    }

    switch (info->kind) {
        case JIT_LOOP_NUMERIC_FOR: return JIT_LOOP_RAN_FOR_NEXT;    // vm pops iterator frame
        case JIT_LOOP_TABLE_ITER:  return JIT_LOOP_RAN_TABLE_ITER;  // vm pops table iterator frame
        default:                    return JIT_LOOP_RAN_NORMAL;     // plain conditional loop
    }
}
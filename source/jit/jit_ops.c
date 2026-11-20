// source/jit/jit_ops.c
// Standard (basic) opcode helpers for the Apex language JIT
// https://github.com/is-nobody/apex-lang
// MIT license

#include "jit_ops.h"
#include "jit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// releases the old value in *dst and publishes the new one with a reference
static void store_owned(Value* dst, Value v) {
    Value old = *dst;                                              // previous slot content
    if ((old & QNAN) == QNAN) value_decref(old);                   // release old heap object
    *dst = v;                                                      // publish new value
    if ((v & QNAN) == QNAN) value_incref(v);                       // take a reference for the slot
}

// releases the old value and publishes a freshly created one whose single reference transfers to the slot
static void store_transfer(Value* dst, Value v) {
    Value old = *dst;                                              // previous slot content
    if ((old & QNAN) == QNAN) value_decref(old);                   // release old heap object
    *dst = v;                                                      // slot owns the fresh ref
}

// OP_MOVE: refcount-aware register copy, exact interpreter semantics
void jit_op_move(Value* dst, Value src) {
    if (dst == &src) return;                                       // self-move is a no-op
    store_owned(dst, src);                                         // decref old, incref new
}

// formats a whole number when it fits the fast path, mirroring the interpreter
static int format_num_tail(char* buf, size_t cap, double n) {
    long long inum = (long long)n;                                 // truncate to integer
    if ((double)inum == n && inum > -1000000000000000LL &&
        inum < 1000000000000000LL) {
        int len = 0;                                               // digits written
        char tmp[24];                                              // reversed scratch
        unsigned long long u;                                      // magnitude
        if (inum < 0) { buf[len++] = '-'; u = (unsigned long long)(-(inum + 1)) + 1ULL; }
        else u = (unsigned long long)inum;                         // non-negative magnitude
        int i = 0;                                                 // scratch index
        do { tmp[i++] = (char)('0' + (u % 10)); u /= 10; } while (u);  // reversed digits
        while (i) { buf[len++] = tmp[--i]; }                       // unreversed into buf
        return len;                                                // total length
    }
    return snprintf(buf, cap, "%.15g", n);                         // fractional or huge
}

// allocates a fresh string object with the given bytes, or none on failure
static Value make_string(const char* a, int alen, const char* b, int blen) {
    int total = alen + blen;                                       // combined length
    StringObject* out = (StringObject*)malloc(sizeof(StringObject) + total + 1);
    if (!out) return MAKE_NONE();                                  // oom: yield none
    out->header.ref_count = 1;                                     // fresh object refcount
    out->header.type      = VAL_STRING;                            // mark type as string
    out->length           = total;                                 // store length
    out->hash_computed    = false;                                 // hash not yet computed
    out->hash             = 0;                                     // clear hash field
    if (alen > 0) memcpy(out->chars, a, alen);                     // copy left part
    if (blen > 0) memcpy(out->chars + alen, b, blen);              // copy right part
    out->chars[total] = '\0';                                      // null terminate
    return MAKE_STRING(out);                                       // boxed fresh string
}

// OP_CONCAT: string concatenation with the interpreter's accumulator fast path
void jit_op_concat(Value* dst, Value* l, Value* r) {
    if (IS_STRING(*l) && IS_STRING(*r) && dst == l) {               // accumulator pattern
        StringObject* a = AS_STRING(*l);                            // unwrap left string
        StringObject* b = AS_STRING(*r);                            // unwrap right string
        if (a->header.ref_count == 1) {                            // uniquely owned, safe to realloc
            int old_len = a->length;                               // old string length
            int new_len = old_len + b->length;                     // combined new length
            StringObject* out = (StringObject*)realloc(            // grow in place, keeps header
                    a, sizeof(StringObject) + new_len + 1);
            if (out) {                                             // realloc succeeded
                memcpy(out->chars + old_len, b->chars, b->length); // append right part
                out->length = new_len;                             // update length
                out->chars[new_len] = '\0';                        // null terminate
                out->hash_computed = false;                        // invalidate cached hash
                *dst = MAKE_STRING(out);                           // store (possibly moved) pointer
                return;                                            // refcount unchanged
            }
        }
    }

    if (IS_STRING(*l) && IS_STRING(*r)) {                            // direct string + string
        StringObject* a = AS_STRING(*l);                            // unwrap left string
        StringObject* b = AS_STRING(*r);                            // unwrap right string
        Value out = make_string(a->chars, a->length, b->chars, b->length);
        store_transfer(dst, out);                                     // release old, store new
        return;
    }

    if (IS_STRING(*l) && IS_NUMBER(*r)) {                            // string + number
        StringObject* a = AS_STRING(*l);                            // unwrap left string
        char numbuf[64];                                           // temp buffer for number
        double num = AS_NUMBER(*r);                                 // unwrap right number
        int numlen;                                                // formatted number length
        if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9)
            numlen = snprintf(numbuf, sizeof(numbuf), "%.0f", num); // large or integer
        else
            numlen = snprintf(numbuf, sizeof(numbuf), "%.15g", num); // general format
        Value out = make_string(a->chars, a->length, numbuf, numlen);
        store_transfer(dst, out);                                     // release old, store new
        return;
    }

    if (IS_NUMBER(*l) && IS_STRING(*r)) {                            // number + string
        StringObject* b = AS_STRING(*r);                            // unwrap right string
        char numbuf[64];                                           // temp buffer for number
        double num = AS_NUMBER(*l);                                 // unwrap left number
        int numlen;                                                // formatted number length
        if (fabs(num) >= 1e6 || fabs(num - (long long)num) < 1e-9)
            numlen = snprintf(numbuf, sizeof(numbuf), "%.0f", num); // large or integer
        else
            numlen = snprintf(numbuf, sizeof(numbuf), "%.15g", num); // general format
        Value out = make_string(numbuf, numlen, b->chars, b->length);
        store_transfer(dst, out);                                     // release old, store new
        return;
    }

    {                                                              // generic fallback
        char lbuf[4096], rbuf[4096];                               // temp conversion buffers
        const char* ls = vm_value_to_cstr(*l, lbuf, sizeof(lbuf));  // convert left to c string
        const char* rs = vm_value_to_cstr(*r, rbuf, sizeof(rbuf));  // convert right to c string
        int llen = IS_STRING(*l) ? AS_STRING(*l)->length : (int)strlen(ls);   // left length
        int rlen = IS_STRING(*r) ? AS_STRING(*r)->length : (int)strlen(rs);   // right length
        Value out = make_string(ls, llen, rs, rlen);               // fresh combined string
        store_transfer(dst, out);                                     // release old, store new
    }
}

// OP_NEW_TABLE: fresh empty table with default capacity
void jit_op_new_table(Value* dst) {
    Value old = *dst;                                              // previous slot content
    if ((old & QNAN) == QNAN) value_decref(old);                   // release old heap object
    *dst = MAKE_TABLE(table_create(8));                            // new table owns one reference
}

// OP_LOAD_TABLE: materialize a compile-time table literal
void jit_op_load_table(VM* vm, Value* dst, int const_idx) {
    Table* t = vm_materialize_const_table(vm->chunk,               // fresh table from the pool entry
                                          &vm->chunk->constants[const_idx]);
    Value old = *dst;                                              // previous slot content
    if ((old & QNAN) == QNAN) value_decref(old);                   // release old heap object
    *dst = MAKE_TABLE(t);                                          // publish fresh table
}

// OP_TABLE_GET: generic keyed read, miss or non-table yields none
void jit_op_table_get(Value* dst, Value tbl, Value key) {
    Value val = MAKE_NONE();                                       // default result
    if (IS_TABLE(tbl)) table_get(AS_TABLE(tbl), key, &val);        // table_get increfs on hit
    store_transfer(dst, val);                                         // release old, store result
}

// borrowed-view variant for native loop slow paths
void jit_table_get_view(Value* dst, Value tbl, Value key) {
    Value val = MAKE_NONE();                                       // default result
    if (IS_TABLE(tbl)) table_get(AS_TABLE(tbl), key, &val);        // table_get increfs on hit
    if ((val & QNAN) == QNAN) value_decref(val);                   // drop the lookup's reference
    *dst = val;                                                    // publish a borrowed view
}

// OP_TABLE_GET_INT: fixed 1-based array index read
void jit_op_table_get_int(Value* dst, Value tbl, int index) {
    Value val = MAKE_NONE();                                       // default result
    if (IS_TABLE(tbl)) {                                           // table slot must hold a table
        Table* t = AS_TABLE(tbl);                                  // unwrap table pointer
        if ((unsigned)(index - 1) < (unsigned)t->array_count) {    // bounds check
            val = t->array_part[index - 1];                        // direct array access
            if ((val & QNAN) == QNAN) value_incref(val);           // heap-only incref
        }
    }
    store_transfer(dst, val);                                         // release old, store result
}

// OP_TABLE_GET_NUM: numeric-key read with the interpreter's two paths
void jit_op_table_get_num(Value* dst, Value tbl, Value key) {
    Value val = MAKE_NONE();                                       // default result
    if (IS_TABLE(tbl)) {                                           // table slot must hold a table
        Table* t = AS_TABLE(tbl);                                  // unwrap table pointer
        double num = AS_NUMBER(key);                               // unbox key
        int idx = (int)num - 1;                                    // 1-based -> 0-based
        if (t->array_part != NULL &&
            (unsigned)idx < (unsigned)t->array_count &&
            num == (double)(idx + 1)) {                            // whole number in range
            val = t->array_part[idx];                              // fast array read
            if ((val & QNAN) == QNAN) value_incref(val);           // heap-only incref
        } else {
            table_get(t, key, &val);                               // slow path: hash or out of range
        }
    }
    store_transfer(dst, val);                                         // release old, store result
}

// OP_TABLE_GET_CONST: interned string key read (plain lookup, no inline cache)
void jit_op_table_get_const(Value* dst, Value tbl, StringObject* key) {
    Value val = MAKE_NONE();                                       // default result
    if (IS_TABLE(tbl) && key) {                                    // table and key both valid
        table_get(AS_TABLE(tbl), MAKE_STRING(key), &val);          // table_get increfs on hit
    }
    store_transfer(dst, val);                                         // release old, store result
}

// OP_TABLE_GET_KEY_STR: tbl["prefix" .. num] fused read
void jit_op_table_get_key_str(Value* dst, Value tbl, StringObject* prefix, Value num) {
    char nbuf[32];                                                 // formatted numeric tail
    int nlen = format_num_tail(nbuf, sizeof(nbuf), AS_NUMBER(num)); // format the tail
    Value val = MAKE_NONE();                                       // default result
    if (IS_TABLE(tbl) && prefix) {                                 // table and prefix both valid
        table_get_concat_key(AS_TABLE(tbl), prefix, nbuf, nlen, &val); // increfs on hit
    }
    store_transfer(dst, val);                                         // release old, store result
}

// stops the vm exactly the way the interpreter's error paths do
static void jit_halt_vm(VM* vm) {
    vm->had_error = true;                                          // set error flag
    vm->running = false;                                           // stop execution
}

// OP_TABLE_SET: generic keyed write, non-table is a runtime error
void jit_op_table_set(VM* vm, Value tbl, Value key, Value val) {
    if (!IS_TABLE(tbl)) {                                          // parser guarantees a table
        jit_halt_vm(vm);                                           // mirror interpreter error
        return;
    }
    table_set(AS_TABLE(tbl), key, val);                            // update-or-insert with refcounts
}

// OP_TABLE_SET_INT: fixed 1-based array index write
void jit_op_table_set_int(VM* vm, Value tbl, int index, Value val) {
    if (!IS_TABLE(tbl)) {                                          // defensive: halt like the interpreter
        jit_halt_vm(vm);
        return;
    }
    Table* table = AS_TABLE(tbl);                                  // unwrap table pointer
    if ((unsigned)(index - 1) < (unsigned)table->array_capacity) { // fits without growth
        int idx = index - 1;                                       // 0-based slot index
        Value old = table->array_part[idx];                        // old value at slot
        if ((old & QNAN) == QNAN) value_decref(old);               // heap-only decref
        table->array_part[idx] = val;                              // store new value
        if ((val & QNAN) == QNAN) value_incref(val);               // heap-only incref
        if (index > table->array_count) table->array_count = index; // extended logical end
    } else {
        table_set_int(table, index - 1, val);                      // grow array part if needed
    }
}

// OP_TABLE_SET_NUM: numeric-key write with array fast path
void jit_op_table_set_num(VM* vm, Value tbl, Value key, Value val) {
    if (!IS_TABLE(tbl)) {                                          // non-table is an error
        jit_halt_vm(vm);
        return;
    }
    Table* t = AS_TABLE(tbl);                                      // unwrap table pointer
    double num = AS_NUMBER(key);                                   // unbox key
    int idx = (int)num - 1;                                        // 1-based -> 0-based
    if (t->array_part != NULL &&
        (unsigned)idx < (unsigned)t->array_capacity &&
        num == (double)(idx + 1)) {                                // whole number in capacity
        Value old = t->array_part[idx];                            // old value at slot
        if ((old & QNAN) == QNAN) value_decref(old);               // heap-only decref
        t->array_part[idx] = val;                                  // store new value
        if ((val & QNAN) == QNAN) value_incref(val);               // heap-only incref
        if ((unsigned)idx >= (unsigned)t->array_count) t->array_count = idx + 1;
    } else if (num == (double)(int)num && idx >= 0) {              // integer key past capacity
        table_set_int(t, idx, val);                                // grows array part if needed
    } else {
        table_set(t, key, val);                                    // non-integer or non-positive key
    }
}

// OP_TABLE_SET_CONST: interned string key write
void jit_op_table_set_const(VM* vm, Value tbl, StringObject* key, Value val) {
    (void)vm;                                                      // vm unused: this op cannot fail
    if (!IS_TABLE(tbl) || !key) return;                            // defensive: skip like the interpreter
    table_set(AS_TABLE(tbl), MAKE_STRING(key), val);               // update-or-insert
}

// OP_TABLE_SET_KEY_STR: tbl["prefix" .. num] = val fused write
void jit_op_table_set_key_str(VM* vm, Value tbl, StringObject* prefix, const Value* pair) {
    if (!IS_TABLE(tbl) || !prefix) {                               // non-table is an error
        jit_halt_vm(vm);
        return;
    }
    char nbuf[32];                                                 // formatted numeric tail
    int nlen = format_num_tail(nbuf, sizeof(nbuf), AS_NUMBER(pair[0])); // format the tail
    table_set_concat_key(AS_TABLE(tbl), prefix, nbuf, nlen, pair[1]); // hash once, alloc on insert
}

// OP_TABLE_APPEND: append a value as the next positional item
void jit_op_table_append(VM* vm, Value tbl, Value val) {
    (void)vm;                                                      // vm unused, kept for symmetry
    if (!IS_TABLE(tbl)) return;                                    // defensive: parser guarantees table
    Table* table = AS_TABLE(tbl);                                  // unwrap table pointer
    table_set_int(table, table->array_count, val);                 // grow, refcount, bump count
}

// OP_LOAD_GLOBAL: read a global slot with refcounting
void jit_op_load_global(VM* vm, Value* dst, int idx) {
    store_owned(dst, vm->globals[idx]);                            // decref old, incref new
}

// OP_STORE_GLOBAL: write a global slot with refcounting
void jit_op_store_global(VM* vm, Value src, int idx) {
    Value old = vm->globals[idx];                                  // previous global value
    if ((old & QNAN) == QNAN) value_decref(old);                   // release old heap object
    vm->globals[idx] = src;                                        // store new value
    if ((src & QNAN) == QNAN) value_incref(src);                   // take a reference for the slot
}

// OP_FOR_INIT: push a numeric iterator frame, error on overflow
void jit_op_for_init(VM* vm, Value start, Value end, Value step) {
    vm->iterator_depth++;                                          // push new iterator frame
    if (vm->iterator_depth >= VM_MAX_ITER_STACK) {                 // guard against overflow
        fprintf(stderr, "\033[31mRuntime Error: Iterator stack overflow - "
                "maximum nested loop depth (%d) exceeded.\n\033[0m",
                VM_MAX_ITER_STACK);
        jit_halt_vm(vm);                                           // mirror interpreter error
        return;
    }
    if (IS_NUMBER(start) && IS_NUMBER(end) && IS_NUMBER(step)) {   // all controls numeric
        vm->iterator_stack[vm->iterator_depth].index = AS_NUMBER(start);
        vm->iterator_stack[vm->iterator_depth].end   = AS_NUMBER(end);
        vm->iterator_stack[vm->iterator_depth].step  = AS_NUMBER(step);
    } else {                                                       // non-numeric: zeroed frame
        vm->iterator_stack[vm->iterator_depth].index = 0.0;        // runs zero iterations
        vm->iterator_stack[vm->iterator_depth].end   = 0.0;
        vm->iterator_stack[vm->iterator_depth].step  = 0.0;
    }
}

// shared core of the FOR_NEXT variants; writes var and advances the frame
static bool for_next_advance(VM* vm, Value* var_dst, bool pop_on_exit) {
    double c = vm->iterator_stack[vm->iterator_depth].index;       // current index
    double e = vm->iterator_stack[vm->iterator_depth].end;         // end value
    double s = vm->iterator_stack[vm->iterator_depth].step;        // step value
    if ((s > 0 && c <= e) || (s < 0 && c >= e)) {                  // still within bounds
        *var_dst = MAKE_NUMBER(c);                                 // store current index
        vm->iterator_stack[vm->iterator_depth].index = c + s;      // advance by step
        return true;                                               // body should run
    }
    if (pop_on_exit) vm->iterator_depth--;                         // pop iterator frame
    return false;                                                  // loop is done
}

// OP_FOR_NEXT: advance the loop, pop the frame when it ends
bool jit_op_for_next(VM* vm, Value* var_dst) {
    return for_next_advance(vm, var_dst, true);                    // interpreter semantics
}

// native-loop entry-test variant; the vm-side hook pops the frame at exit
bool jit_op_for_next_keep(VM* vm, Value* var_dst) {
    return for_next_advance(vm, var_dst, false);                   // pop deferred to the caller
}

// OP_FOR_NEXT_LOOP: loop-inverted variant, pops the frame when it ends
bool jit_op_for_next_loop(VM* vm, Value* var_dst) {
    return for_next_advance(vm, var_dst, true);                    // same core, inverted branch
}

// OP_POP_ITER: pop one numeric iterator frame if any exist
void jit_op_pop_iter(VM* vm) {
    if (vm->iterator_depth >= 0) vm->iterator_depth--;             // pop if non-negative
}

// OP_TABLE_ITER_INIT: push a table iterator frame, error on overflow
void jit_op_table_iter_init(VM* vm, Value tbl) {
    vm->table_iter_depth++;                                        // push new iterator frame
    if (vm->table_iter_depth >= VM_MAX_ITER_STACK) {               // guard against overflow
        fprintf(stderr, "\033[31mRuntime Error: Iterator stack overflow - "
                "maximum nested table iteration depth (%d) exceeded.\n\033[0m",
                VM_MAX_ITER_STACK);
        jit_halt_vm(vm);                                           // mirror interpreter error
        return;
    }
    TableIterState* iter = &vm->table_iters[vm->table_iter_depth]; // current iterator state
    if (!IS_TABLE(tbl)) {                                          // not a table: empty iterator
        iter->table = NULL;                                        // mark as invalid
        iter->array_index = 0;                                     // reset array index
        iter->bucket_index = 0;                                    // reset bucket index
        iter->current_entry = NULL;                                // reset current entry
    } else {
        iter->table = AS_TABLE(tbl);                               // store table pointer
        iter->array_index = 0;                                     // start at first array element
        iter->bucket_index = 0;                                    // start at first hash bucket
        iter->current_entry = NULL;                                // no current entry yet
        iter->start_generation = iter->table->generation;          // record generation at init
    }
}

// shared core of the TABLE_ITER_NEXT variants; walks array then hash part
static bool table_iter_advance(VM* vm, Value* var_dst, bool pop_on_exit) {
    TableIterState* iter = &vm->table_iters[vm->table_iter_depth]; // current iterator state
    Table* t = iter->table;                                        // fetch table pointer
    if (t == NULL) {                                               // empty or invalid table
        if (pop_on_exit) vm->table_iter_depth--;                   // pop iterator frame
        return false;                                              // iteration ended
    }
    if (t->generation != iter->start_generation) {                 // mutated during iteration
        if (pop_on_exit) vm->table_iter_depth--;                   // bail out cleanly
        return false;                                              // iteration ended
    }
    while (iter->array_index < t->array_count) {                   // iterate over array part
        int idx = iter->array_index++;                             // current index, then advance
        if (!IS_NONE(t->array_part[idx])) {                        // slot is occupied
            value_decref(*var_dst);                                // release old value in var
            *var_dst = t->array_part[idx];                         // store the element
            value_incref(*var_dst);                                // bump refcount for stored value
            return true;                                           // body should run
        }
    }
    while (iter->bucket_index < t->capacity) {                     // iterate over hash part
        if (t->entries == NULL) {                                  // no hash part allocated
            if (pop_on_exit) vm->table_iter_depth--;               // pop iterator frame
            return false;                                          // iteration ended
        }
        if (iter->current_entry == NULL) {                         // advance to next bucket
            iter->current_entry = t->entries[iter->bucket_index++];
            continue;                                              // retry with the new entry
        }
        TableEntry* entry = iter->current_entry;                   // current entry
        iter->current_entry = entry->next;                         // advance within the chain
        value_decref(*var_dst);                                    // release old value in var
        *var_dst = entry->value;                                   // store the value
        value_incref(*var_dst);                                    // bump refcount for stored value
        return true;                                               // body should run
    }
    if (pop_on_exit) vm->table_iter_depth--;                       // no more entries, pop frame
    return false;                                                  // iteration ended
}

// OP_TABLE_ITER_NEXT: advance the iterator, pop the frame when it ends
bool jit_op_table_iter_next(VM* vm, Value* var_dst) {
    return table_iter_advance(vm, var_dst, true);                  // interpreter semantics
}

// native-loop variant; the vm-side hook pops the frame at exit
bool jit_op_table_iter_next_keep(VM* vm, Value* var_dst) {
    return table_iter_advance(vm, var_dst, false);                 // pop deferred to the caller
}

// OP_POP_TABLE_ITER: pop one table iterator frame if any exist
void jit_op_pop_table_iter(VM* vm) {
    if (vm->table_iter_depth >= 0) vm->table_iter_depth--;         // pop if non-negative
}

// OP_CALL: many-argument call driven through the coroutine scheduler
void jit_op_call(VM* vm, Value* dst, int func_idx, int arg_count) {
#if APEX_JIT_ENABLED
    // compiled callee with a matching arity
    if (vm->jit && arg_count <= JIT_MAX_CALL_ARITY &&       // stub covers up to eight args
        jit_has_native(vm->jit, func_idx) &&
        func_idx < vm->chunk->func_count &&
        vm->chunk->functions[func_idx].arity == arg_count) { // convention needs exact arity
        bool args_ok = true;                                 // checked paths accept any value
        if (jit_strict_pure(vm->jit, func_idx)) {            // strict callee wants raw doubles
            for (int i = 0; i < arg_count; i++) {            // scan every pushed argument
                if (!IS_NUMBER(vm->args_stack[vm->args_top - arg_count + i])) {
                    args_ok = false;                         // tagged arg: drive a coroutine instead
                    break;
                }
            }
        }
        if (args_ok) {
            double r = jit_call_many(vm->jit, func_idx,      // args transfer their references
                                     &vm->args_stack[vm->args_top - arg_count],
                                     arg_count);
            vm->args_top -= arg_count;                        // callee cleanup walk owns the args
            store_transfer(dst, jit_box_value(vm->jit, func_idx, r));  // caller owns the returned ref
            return;
        }
    }
#endif
    FutureObject* fut = (FutureObject*)calloc(1, sizeof(FutureObject));
    if (!fut) {                                                    // oom: yield none
        store_owned(dst, MAKE_NONE());
        return;
    }
    fut->header.ref_count = 1;                                     // fresh object refcount
    fut->header.type      = VAL_FUTURE;                            // mark type as future
    fut->result           = MAKE_NONE();                           // filled when resolved
    fut->state            = 0;                                     // pending
    fut->func_idx         = func_idx;                              // body to run
    fut->arg_count        = arg_count;                             // captured argument count
    fut->args             = arg_count > 0
                          ? (Value*)malloc(sizeof(Value) * arg_count) : NULL;
    fut->saved_dest_reg   = -1;                                    // no pending await
    fut->awaiting         = MAKE_NONE();                           // nothing awaited
    fut->saved_iter_depth = -1;                                    // no active loops
    fut->saved_table_iter_depth = -1;                              // no active table iterators
    fut->saved_call_depth = 0;                                     // coroutine starts at depth 0
    if (arg_count > 0 && !fut->args) {                             // oom: args unavailable
        free(fut);
        store_owned(dst, MAKE_NONE());
        return;
    }
    for (int i = 0; i < arg_count; i++) {                          // capture args; the stack's
        fut->args[i] = vm->args_stack[vm->args_top - arg_count + i];  // reference moves here
    }
    vm->args_top -= arg_count;                                     // pop args from the stack

    future_start(vm, fut);                                         // enqueue the coroutine
    Value target = MAKE_FUTURE(fut);                               // drive handle
    Value result;                                                  // resolved value
    bool was_running = vm->running;                                // a coroutine return stops
    vm_drive_until(vm, target, &result);                           // its own slice by clearing
    if (was_running && !vm->had_error) vm->running = true;         // running; the caller continues
    value_decref(target);                                          // drop the drive reference
    store_transfer(dst, result);                                      // caller owns one ref
}

// OP_ASYNC_CALL: capture args into a pending future without running the body
void jit_op_async_call(VM* vm, Value* dst, int func_idx, int arg_count) {
    FutureObject* fut = (FutureObject*)malloc(sizeof(FutureObject));
    if (!fut) {                                                    // oom: yield none
        store_owned(dst, MAKE_NONE());
        jit_halt_vm(vm);
        return;
    }
    fut->args = arg_count > 0 ? (Value*)malloc(sizeof(Value) * arg_count) : NULL;
    if (arg_count > 0 && !fut->args) {                             // oom: args unavailable
        free(fut);
        store_owned(dst, MAKE_NONE());
        jit_halt_vm(vm);
        return;
    }
    fut->header.ref_count = 1;                                     // fresh object refcount
    fut->header.type      = VAL_FUTURE;                            // mark type as future
    fut->result           = MAKE_NONE();                           // filled in when awaited
    fut->state            = 0;                                     // pending
    fut->func_idx         = func_idx;                              // body to run on await
    fut->arg_count        = arg_count;                             // captured argument count
    fut->register_pool    = NULL;                                  // pool allocated lazily
    fut->frame_offset     = NULL;
    fut->frame_capacity   = NULL;
    fut->frame_used       = NULL;
    fut->pool_capacity    = 0;
    fut->current_frame    = 0;
    fut->frame_arrays_size = 0;                                    // arrays allocated lazily
    fut->owns_frame       = false;
    fut->frame_idx        = -1;                                    // legacy field, unused
    fut->saved_ip         = 0;                                     // no resume point yet
    fut->saved_dest_reg   = -1;                                    // no pending await
    fut->awaiting         = MAKE_NONE();                           // nothing awaited
    fut->waiters          = NULL;                                  // no waiters yet
    fut->waiter_count     = 0;                                     // empty
    fut->waiter_capacity  = 0;                                     // no capacity
    fut->saved_iter_depth = -1;                                    // no active loops
    fut->saved_table_iter_depth = -1;                              // no active table iterators
    fut->saved_call_depth = 0;                                     // fresh coroutine depth
    for (int i = 0; i < arg_count; i++) {                          // capture args from the stack
        Value a = vm->args_stack[vm->args_top - arg_count + i];
        if ((a & QNAN) == QNAN) value_incref(a);                   // keep a reference alive
        fut->args[i] = a;                                          // store captured value
    }
    vm->args_top -= arg_count;                                     // pop args from the stack
    store_owned(dst, MAKE_FUTURE(fut));                            // caller owns the future
    if (fut->func_idx >= 0) future_start(vm, fut);                 // async body: schedule eagerly
}

// type-dispatched equality, identical to the interpreter's CMP_EQ core
bool jit_op_values_equal(Value a, Value b) {
    if (IS_NONE(a) && IS_NONE(b)) return true;                     // both none are equal
    if (IS_NONE(a) || IS_NONE(b)) return false;                    // one none, one not
    if (IS_NUMBER(a) && IS_NUMBER(b))
        return AS_NUMBER(a) == AS_NUMBER(b);                       // compare numeric values
    if (IS_BOOL(a) && IS_BOOL(b)) return AS_BOOL(a) == AS_BOOL(b); // compare boolean bits
    if (IS_STRING(a) && IS_STRING(b)) {                            // string contents
        StringObject* sa = AS_STRING(a);                           // unwrap left string
        StringObject* sb = AS_STRING(b);                           // unwrap right string
        if (sa == sb) return true;                                 // interned fast path
        if (sa->length != sb->length) return false;                // different lengths
        return memcmp(sa->chars, sb->chars, sa->length) == 0;      // content compare
    }
    if (IS_TABLE(a) && IS_TABLE(b))                                // deep table equality
        return table_equal(AS_TABLE(a), AS_TABLE(b), 0) ? true : false;
    return false;                                                  // mixed types: not equal
}

// value_incref wrapper callable from emitted code
void jit_incref_value(Value v) { value_incref(v); }

// value_decref wrapper callable from emitted code
void jit_decref_value(Value v) { value_decref(v); }

// transfers a frame-owned reference into a vm register slot
void jit_writeback_owned(Value* pool_slot, Value frame_val) {
    if (frame_val == *pool_slot) {                                 // untouched since the seed
        value_decref(frame_val);                                   // drop the frame's extra ref
        return;                                                    // pool keeps its own copy
    }
    value_decref(*pool_slot);                                      // release the stale pool value
    *pool_slot = frame_val;                                        // frame's ref moves to the pool
}
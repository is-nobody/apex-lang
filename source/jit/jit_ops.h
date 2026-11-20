// source/jit/jit_ops.h
// Standard (basic) opcode helpers for the Apex language JIT
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_JIT_OPS_H
#define APEX_JIT_OPS_H

#include "vm.h"
#include "bytecode.h"
#include "jit.h"
#include <stdbool.h>

// the helpers below implement the interpreter's exact semantics for opcodes that have no proven-fast native lowering
void jit_op_move(Value* dst, Value src);                                                    // OP_MOVE with refcounting
void jit_op_concat(Value* dst, Value* l, Value* r);                                         // OP_CONCAT, all type combos
void jit_op_new_table(Value* dst);                                                          // OP_NEW_TABLE
void jit_op_load_table(VM* vm, Value* dst, int const_idx);                                  // OP_LOAD_TABLE literal
void jit_op_table_get(Value* dst, Value tbl, Value key);                                    // OP_TABLE_GET generic
void jit_table_get_view(Value* dst, Value tbl, Value key);                                  // borrowed-view variant for loops
void jit_op_table_get_int(Value* dst, Value tbl, int idx);                                  // OP_TABLE_GET_INT
void jit_op_table_get_num(Value* dst, Value tbl, Value key);                                // OP_TABLE_GET_NUM
void jit_op_table_get_const(Value* dst, Value tbl, StringObject* key);                      // interned key read
void jit_op_table_get_key_str(Value* dst, Value tbl, StringObject* prefix, Value num);      // fused get
void jit_op_table_set(VM* vm, Value tbl, Value key, Value val);                             // OP_TABLE_SET, error-capable
void jit_op_table_set_int(VM* vm, Value tbl, int idx, Value val);                           // OP_TABLE_SET_INT
void jit_op_table_set_num(VM* vm, Value tbl, Value key, Value val);                         // OP_TABLE_SET_NUM
void jit_op_table_set_const(VM* vm, Value tbl, StringObject* key, Value val);               // interned key write
void jit_op_table_set_key_str(VM* vm, Value tbl, StringObject* prefix, const Value* pair);  // fused set
void jit_op_table_append(VM* vm, Value tbl, Value val);                                     // OP_TABLE_APPEND
void jit_op_load_global(VM* vm, Value* dst, int idx);                                       // OP_LOAD_GLOBAL
void jit_op_store_global(VM* vm, Value src, int idx);                                       // OP_STORE_GLOBAL
void jit_op_for_init(VM* vm, Value start, Value end, Value step);                           // OP_FOR_INIT, error-capable
bool jit_op_for_next(VM* vm, Value* var_dst);                                               // OP_FOR_NEXT, pops on exit
bool jit_op_for_next_keep(VM* vm, Value* var_dst);                                          // native-loop variant, no pop
bool jit_op_for_next_loop(VM* vm, Value* var_dst);                                          // OP_FOR_NEXT_LOOP, pops on exit
void jit_op_pop_iter(VM* vm);                                                               // OP_POP_ITER
void jit_op_table_iter_init(VM* vm, Value tbl);                                             // OP_TABLE_ITER_INIT
bool jit_op_table_iter_next(VM* vm, Value* var_dst);                                        // pops on exit
bool jit_op_table_iter_next_keep(VM* vm, Value* var_dst);                                   // native-loop variant, no pop
void jit_op_pop_table_iter(VM* vm);                                                         // OP_POP_TABLE_ITER
void jit_op_call(VM* vm, Value* dst, int func_idx, int arg_count);                          // OP_CALL via coroutines
void jit_op_async_call(VM* vm, Value* dst, int func_idx, int arg_count);                    // OP_ASYNC_CALL
bool jit_op_values_equal(Value a, Value b);                                                 // type-dispatched equality

// refcount primitives used by the emitted cleanup walk and owned writeback
void jit_incref_value(Value v);                               // value_incref wrapper
void jit_decref_value(Value v);                               // value_decref wrapper
void jit_writeback_owned(Value* pool_slot, Value frame_val);  // transfer an owned ref to the pool

// re-tags a raw jit return value according to the callee's proven return kind
Value jit_box_value(JITContext* jit, int func_idx, double r);

#endif
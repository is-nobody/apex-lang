// source/utils/apex_api.h
// Implementation of C API for embedding Apex in host applications
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_API_H
#define APEX_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// opaque state handle, one per embedding context
// each state owns an independent apex vm with its own globals,
// string intern table, and value stack
typedef struct ApexState ApexState;

// type tags returned by apex_type and apex_type_name
typedef enum {
    APEX_TNONE          = 0,  // none, the absence of a value
    APEX_TBOOLEAN       = 1,  // true or false
    APEX_TNUMBER        = 2,  // double-precision number
    APEX_TSTRING        = 3,  // string
    APEX_TTABLE         = 4,  // table
    APEX_TFUNCTION      = 5,  // apex function reference as a value
    APEX_TLIGHTUSERDATA = 6,  // light userdata (opaque c pointer)
    APEX_TFUTURE        = 7,  // pending or resolved future
} ApexType;

// signature for c functions registered with apex_register_function
// arguments are already pushed onto the state's stack when the function
// is called, and the function should push its return value (if any) and
// return the count of results (0 or 1)
typedef int (*ApexCFunction)(ApexState* S);

// creates a new apex state with a fresh vm, returns null on failure
ApexState* apex_open(void);

// destroys an apex state and releases all associated resources
void apex_close(ApexState* S);

// raises an error, formatted printf-style, and longjmps back to the
// nearest protected context if one is active
int apex_raise(ApexState* S, const char* fmt, ...);

// returns the last error message, or an empty string if none
const char* apex_last_error(ApexState* S);

// returns the number of elements currently on the stack
int  apex_get_top(ApexState* S);

// sets the stack top to the given index, growing or shrinking as needed
void apex_set_top(ApexState* S, int index);

// pops n elements off the top of the stack
void apex_pop(ApexState* S, int n);

// ensures the stack has room for at least extra more elements
int  apex_check_stack(ApexState* S, int extra);

// pushes a copy of the value at the given stack index
void apex_push_value(ApexState* S, int index);

// removes the element at the given index, shifting down elements above it
void apex_remove(ApexState* S, int index);

// moves the top element to the given index, shifting up elements above it
void apex_insert(ApexState* S, int index);

// pops the top element and sets it at the given index without shifting
void apex_replace(ApexState* S, int index);

// pushes none onto the stack
void apex_push_none(ApexState* S);

// pushes a boolean (any non-zero value becomes true)
void apex_push_boolean(ApexState* S, int b);

// pushes an integer as a number value
void apex_push_integer(ApexState* S, long long n);

// pushes a floating-point number
void apex_push_number(ApexState* S, double n);

// pushes a null-terminated string as an owned, non-interned value
void apex_push_string(ApexState* S, const char* s);

// pushes a string of a given length, may contain embedded zeros
void apex_push_lstring(ApexState* S, const char* s, size_t len);

// pushes a light userdata value wrapping an opaque c pointer
void apex_push_light_userdata(ApexState* S, void* p);

// creates a new empty table and pushes it onto the stack
void apex_new_table(ApexState* S);

// creates a new empty table with pre-sized array and hash parts
void apex_new_table_sized(ApexState* S, int narr, int nrec);

// returns the type tag of the value at the given index
ApexType    apex_type(ApexState* S, int index);

// returns the type name of the value at the given index as a c string
const char* apex_type_name(ApexState* S, int index);

// type predicates, return 1 if the value at the given index matches
int apex_is_none(ApexState* S, int index);
int apex_is_boolean(ApexState* S, int index);
int apex_is_number(ApexState* S, int index);
int apex_is_string(ApexState* S, int index);
int apex_is_table(ApexState* S, int index);
int apex_is_function(ApexState* S, int index);
int apex_is_light_userdata(ApexState* S, int index);
int apex_is_future(ApexState* S, int index);

// converts the value at the given index to a c boolean
int         apex_to_boolean(ApexState* S, int index);

// converts the value at the given index to a c integer
long long   apex_to_integer(ApexState* S, int index);

// converts the value at the given index to a c double
double      apex_to_number(ApexState* S, int index);

// converts the value at the given index to a c string, or null on failure
const char* apex_to_string(ApexState* S, int index);

// converts to a c string and returns its length via the len out-parameter
const char* apex_to_lstring(ApexState* S, int index, size_t* len);

// returns the light userdata pointer, or null if not a userdata
void*       apex_to_light_userdata(ApexState* S, int index);

// returns a generic opaque pointer for identity comparison, or null
const void* apex_to_pointer(ApexState* S, int index);

// checks that argument narg is a number and returns it as an integer
long long   apex_check_integer(ApexState* S, int narg);

// checks that argument narg is a number and returns it as a double
double      apex_check_number(ApexState* S, int narg);

// checks that argument narg is a string and returns it as a c string
const char* apex_check_string(ApexState* S, int narg);

// checks that argument narg is a string and returns its length via len
const char* apex_check_lstring(ApexState* S, int narg, size_t* len);

// checks that argument narg is a light userdata and returns its pointer
void*       apex_check_light_userdata(ApexState* S, int narg);

// checks that argument narg has the given type, raising an error if not
void        apex_check_type(ApexState* S, int narg, ApexType type);

// pops a key and pushes the resulting value (none on miss)
void apex_get_table(ApexState* S, int index);

// pushes the value stored under the given string key (none on miss)
void apex_get_field(ApexState* S, int index, const char* k);

// pushes the value stored under the given numeric key (none on miss)
void apex_get_index(ApexState* S, int index, long long n);

// raw variants kept for api parity with lua, same semantics as above today
void apex_raw_get(ApexState* S, int index);
void apex_raw_get_i(ApexState* S, int index, long long n);
void apex_raw_get_field(ApexState* S, int index, const char* k);

// pops value and key, assigns tbl[key] = value
void apex_set_table(ApexState* S, int index);

// pops value, assigns tbl[k] = value
void apex_set_field(ApexState* S, int index, const char* k);

// pops value, assigns tbl[n] = value
void apex_set_index(ApexState* S, int index, long long n);

// raw variants kept for api parity with lua, same semantics as above today
void apex_raw_set(ApexState* S, int index);
void apex_raw_set_i(ApexState* S, int index, long long n);
void apex_raw_set_field(ApexState* S, int index, const char* k);

// returns the number of entries in the table, 0 if not a table
long long apex_table_size(ApexState* S, int index);

// pushes the value of a global, returns 1 if found and 0 otherwise
int  apex_get_global(ApexState* S, const char* name);

// pops a value and stores it in the named global, silently ignored
// if the global does not exist in the currently active chunk
void apex_set_global(ApexState* S, const char* name);

// compiles and runs apex source, returns 0 on success and non-zero on error
// chunkname is used for error messages and import resolution
int apex_run_string(ApexState* S, const char* code, const char* chunkname);

// reads the file at the given path and runs it as apex source
int apex_run_file(ApexState* S, const char* path);

// registers a c function under the given name, apex code can call it
// by that name like any other function, registrations are per-state
void apex_register_function(ApexState* S, const char* name, ApexCFunction f);

// returns 1 if the state has a pending error (set by a failed push, a
// raised error, or a failed run), 0 otherwise
int apex_had_error(ApexState* S);

// clears the pending error flag and message. the host calls this after
// inspecting apex_last_error, before attempting further operations
void apex_clear_error(ApexState* S);

// returns the underlying vm pointer, provided for advanced use only
struct VM* apex_get_vm(ApexState* S);

#ifdef __cplusplus
}
#endif

#endif // APEX_API_H
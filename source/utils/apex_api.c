// source/utils/apex_api.c
// Implementation of C API for embedding Apex in host applications
// https://github.com/is-nobody/apex-lang
// MIT license

#include "apex_api.h"
#include "vm.h"
#include "tokenizer.h"
#include "parser.h"
#include "ast.h"
#include "bytecode.h"
#include "codegen.h"
#include "execute.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>

// one registered c function, stored in a singly linked list per state
typedef struct CFunctionEntry {
    char* name;                     // function name as seen from apex code
    ApexCFunction fn;               // user's c function
    struct CFunctionEntry* next;    // next entry in the list, null at tail
} CFunctionEntry;

// one host-provided global that must survive the next apex run
typedef struct PendingGlobal {
    char* name;                     // global name
    Value value;                    // value to install before each run
    struct PendingGlobal* next;     // next entry
} PendingGlobal;

// full state of an embedding context, opaque to the host
struct ApexState {
    VM* vm;                         // underlying apex virtual machine
    BytecodeChunk* active_chunk;    // chunk of the most recently run program

    Value* stack;                   // dynamically grown value stack
    int    top;                     // number of elements currently on the stack
    int    capacity;                // allocated capacity of the stack array

    CFunctionEntry* c_funcs;        // head of the registered c function list
    CFunctionEntry* c_funcs_tail;   // tail of the list, for o(1) appends

    PendingGlobal* pending_globals; // host-provided globals to seed on each run

    char num_buf[64];               // scratch buffer for number -> string conversion

    char error_message[1024];       // last raised error message
    int  error_active;              // non-zero when an error is pending
    int  jmp_active;                // non-zero while a protected call is in flight
    jmp_buf error_jmp;              // longjmp target for apex_raise
};

// forward declaration of the c function dispatcher installed on the vm
static bool apex_c_function_dispatch(VM* vm,
                                     const char* name,
                                     int arg_count,
                                     Value* args,
                                     Value* result);

// grows the stack array to hold at least min_cap elements
static void stack_grow(ApexState* S, int min_cap) {
    if (S->capacity >= min_cap) return;             // already large enough
    int new_cap = S->capacity == 0 ? 32 : S->capacity;  // start from a small default
    while (new_cap < min_cap) new_cap *= 2;         // double until it fits
    S->stack = (Value*)realloc(S->stack, sizeof(Value) * new_cap);  // resize the buffer
    S->capacity = new_cap;                          // update capacity
}

// pushes a value, taking a new reference on heap-allocated values
static void stack_push(ApexState* S, Value v) {
    stack_grow(S, S->top + 1);                      // ensure room for one more element
    if ((v & QNAN) == QNAN) value_incref(v);        // bump refcount for heap values
    S->stack[S->top++] = v;                         // store and advance top
}

// pops one value and transfers ownership of one reference to the caller
static Value stack_pop(ApexState* S) {
    if (S->top <= 0) return MAKE_NONE();            // empty stack: return none
    return S->stack[--S->top];                      // return element and shrink top
}

// translates a public (possibly negative) index into a 1-based absolute one
static int stack_abs(ApexState* S, int index) {
    if (index > 0) return index;                    // positive indices are absolute
    if (index < 0) return S->top + index + 1;       // negative indices count from top
    return 0;                                       // 0 is always invalid
}

// returns a pointer to the value at the given index, or null if out of range
static Value* stack_at(ApexState* S, int index) {
    int abs = stack_abs(S, index);                  // translate to absolute
    if (abs < 1 || abs > S->top) return NULL;       // out of range
    return &S->stack[abs - 1];                      // return slot pointer
}

// returns the name of a type tag as a c string, used in error messages
static const char* apex_type_to_name(ApexType t) {
    switch (t) {
        case APEX_TNONE:          return "none";
        case APEX_TBOOLEAN:       return "boolean";
        case APEX_TNUMBER:        return "number";
        case APEX_TSTRING:        return "string";
        case APEX_TTABLE:         return "table";
        case APEX_TFUNCTION:      return "function";
        case APEX_TLIGHTUSERDATA: return "userdata";
        case APEX_TFUTURE:        return "future";
    }
    return "unknown";                               // fallback for invalid tags
}

// builds an interned string value for the given c string
static Value make_key_string(ApexState* S, const char* k) {
    int n = (int)strlen(k);                                      // length of the key
    StringObject* s = string_intern(&S->vm->intern_table, k, n); // intern once
    return MAKE_STRING(s);                                       // wrap as a value
}

// shared implementation for apex_get_table and apex_raw_get; pops a key and pushes the resulting value
static void table_get_common(ApexState* S, int index) {
    if (!S) return;                                 // guard against null state
    Value* tbl_slot = stack_at(S, index);           // resolve target first
    Value key = stack_pop(S);                       // then pop the key
    if (!tbl_slot || !IS_TABLE(*tbl_slot)) {        // not a table: push none
        stack_push(S, MAKE_NONE());
    } else {
        Value out = MAKE_NONE();                    // default result
        table_get(AS_TABLE(*tbl_slot), key, &out);  // actual lookup
        stack_push(S, out);                         // push on stack
        if ((out & QNAN) == QNAN) value_decref(out); // stack_push took its own ref
    }
    if ((key & QNAN) == QNAN) value_decref(key);    // release the local key ref
}

// releases a compilation pipeline, all handles are optional
static void cleanup_compile(Tokenizer* tok, Parser* par,
                            ASTNode* ast, CodeGenerator* cg) {
    if (cg)  codegen_destroy(cg);                   // free codegen state
    if (ast) ast_free_node(ast);                    // free ast tree
    if (par) parser_destroy(par);                   // free parser state
    if (tok) tokenizer_destroy(tok);                // free tokenizer state
}

// returns the pending entry for name, creating it if needed
static PendingGlobal* pending_global_get(ApexState* S, const char* name) {
    for (PendingGlobal* p = S->pending_globals; p; p = p->next) {   // search existing
        if (strcmp(p->name, name) == 0) return p;
    }
    PendingGlobal* p = (PendingGlobal*)malloc(sizeof(PendingGlobal)); // allocate new
    p->name = strdup(name);                                          // own the name
    p->value = MAKE_NONE();                                          // default value
    p->next = S->pending_globals;                                    // prepend
    S->pending_globals = p;
    return p;
}

// raises an error, formatted printf-style, and longjmps back if protected
int apex_raise(ApexState* S, const char* fmt, ...) {
    if (!S) return 0;                               // null guard
    va_list ap;                                     // varargs list
    va_start(ap, fmt);                              // start varargs
    vsnprintf(S->error_message, sizeof(S->error_message), fmt, ap);  // format into buffer
    va_end(ap);                                     // end varargs
    S->error_active = 1;                            // mark error as pending
    if (S->jmp_active) longjmp(S->error_jmp, 1);    // unwind to protected context
    return 0;                                       // unreachable when jmp_active
}

// returns the last error message, or an empty string if none
const char* apex_last_error(ApexState* S) {
    if (!S) return "";                              // null guard
    return S->error_message;                        // return stored message
}

// creates a new apex state with a fresh vm, returns null on failure
ApexState* apex_open(void) {
    ApexState* S = (ApexState*)calloc(1, sizeof(ApexState));  // allocate zeroed state
    if (!S) return NULL;                                      // allocation failed

    S->vm = vm_create("");                          // create a fresh vm with empty source
    if (!S->vm) { free(S); return NULL; }           // vm creation failed, rollback

    S->capacity = 32;                               // initial stack capacity
    S->stack = (Value*)malloc(sizeof(Value) * S->capacity);  // allocate stack buffer
    S->top = 0;                                     // empty stack

    S->active_chunk = NULL;                         // no chunk loaded yet
    S->c_funcs = NULL;                              // no c functions registered
    S->c_funcs_tail = NULL;                         // tail starts null
    S->pending_globals = NULL;                      // no pending globals

    S->num_buf[0] = '\0';                           // clear scratch buffer
    S->error_message[0] = '\0';                     // clear error message
    S->error_active = 0;                            // no pending error
    S->jmp_active = 0;                              // not inside a protected call

    S->vm->c_function_state    = S;                 // wire vm back-pointer to our state
    S->vm->c_function_dispatch = apex_c_function_dispatch;  // install dispatcher

    return S;                                       // return the new state
}

// destroys an apex state and releases all associated resources
void apex_close(ApexState* S) {
    if (!S) return;                                 // null guard

    for (int i = 0; i < S->top; i++) {              // release every value on the stack
        if ((S->stack[i] & QNAN) == QNAN) value_decref(S->stack[i]);
    }
    free(S->stack);                                 // free the stack buffer
    S->stack = NULL;                                // clear pointer

    CFunctionEntry* e = S->c_funcs;                 // walk the c function list
    while (e) {
        CFunctionEntry* next = e->next;             // save next before freeing
        free(e->name);                              // free the name copy
        free(e);                                    // free the entry struct
        e = next;                                   // advance
    }
    S->c_funcs = NULL;                              // clear head
    S->c_funcs_tail = NULL;                         // clear tail

    PendingGlobal* pg = S->pending_globals;         // walk the pending global list
    while (pg) {
        PendingGlobal* next = pg->next;             // save next before freeing
        free(pg->name);                             // free the name copy
        if ((pg->value & QNAN) == QNAN) value_decref(pg->value);  // release stored value
        free(pg);                                   // free the entry struct
        pg = next;                                  // advance
    }
    S->pending_globals = NULL;                      // clear the list

    if (S->active_chunk) {                          // free the active bytecode chunk
        bytecode_destroy(S->active_chunk);
        S->active_chunk = NULL;
    }

    vm_destroy(S->vm);                              // destroy the vm and all its state
    S->vm = NULL;                                   // clear pointer

    free(S);                                        // free the state itself
}

// returns the number of elements currently on the stack
int apex_get_top(ApexState* S) { return S ? S->top : 0; }

// sets the stack top to the given index, growing or shrinking as needed
void apex_set_top(ApexState* S, int index) {
    if (!S) return;                                 // null guard
    int abs = stack_abs(S, index);                  // translate to absolute
    if (abs < 0) abs = 0;                           // clamp negative to zero
    if (abs < S->top) {                             // shrinking: release excess values
        for (int i = abs; i < S->top; i++) {
            if ((S->stack[i] & QNAN) == QNAN) value_decref(S->stack[i]);
        }
        S->top = abs;                               // publish new top
    } else if (abs > S->top) {                      // growing: pad with none
        stack_grow(S, abs);                         // ensure capacity
        while (S->top < abs) S->stack[S->top++] = MAKE_NONE();  // fill with none
    }
}

// pops n elements off the top of the stack
void apex_pop(ApexState* S, int n) {
    if (!S || n <= 0) return;                       // null guard and nothing-to-pop
    if (n > S->top) n = S->top;                     // clamp to actual stack size
    for (int i = 0; i < n; i++) {                   // release each popped value
        Value v = S->stack[--S->top];
        if ((v & QNAN) == QNAN) value_decref(v);
    }
}

// ensures the stack has room for at least extra more elements
int apex_check_stack(ApexState* S, int extra) {
    if (!S) return 0;                               // null guard
    stack_grow(S, S->top + extra);                  // grow to fit
    return 1;                                       // stack grows on demand so always succeeds
}

// pushes a copy of the value at the given stack index
void apex_push_value(ApexState* S, int index) {
    if (!S) return;                                 // null guard
    Value* v = stack_at(S, index);                  // resolve source slot
    Value copy = v ? *v : MAKE_NONE();              // copy value or none if out of range
    stack_push(S, copy);                            // push the copy
}

// removes the element at the given index, shifting down elements above it
void apex_remove(ApexState* S, int index) {
    if (!S) return;                                 // null guard
    int abs = stack_abs(S, index);                  // translate to absolute
    if (abs < 1 || abs > S->top) return;            // out of range
    Value v = S->stack[abs - 1];                    // save the removed value
    memmove(&S->stack[abs - 1], &S->stack[abs],     // shift down elements above
            sizeof(Value) * (S->top - abs));
    S->top--;                                       // shrink top
    if ((v & QNAN) == QNAN) value_decref(v);        // release the removed value
}

// moves the top element to the given index, shifting up elements above it
void apex_insert(ApexState* S, int index) {
    if (!S || S->top == 0) return;                  // null guard and empty stack
    int abs = stack_abs(S, index);                  // translate to absolute
    if (abs < 1) abs = 1;                           // clamp below to 1
    if (abs > S->top) abs = S->top;                 // clamp above to top
    Value top_val = S->stack[S->top - 1];           // save the top value
    memmove(&S->stack[abs], &S->stack[abs - 1],     // shift up elements above
            sizeof(Value) * (S->top - abs));
    S->stack[abs - 1] = top_val;                    // place top at target index
}

// pops the top element and sets it at the given index without shifting
void apex_replace(ApexState* S, int index) {
    if (!S || S->top == 0) return;                  // null guard and empty stack
    int abs = stack_abs(S, index);                  // translate to absolute
    if (abs < 1 || abs > S->top) return;            // out of range
    Value newv = S->stack[S->top - 1];              // save the new value
    S->top--;                                       // pop top, keeping the reference
    Value old = S->stack[abs - 1];                  // save old value for release
    S->stack[abs - 1] = newv;                       // install new value
    if ((old & QNAN) == QNAN) value_decref(old);    // release old value
}

// pushes none onto the stack
void apex_push_none(ApexState* S) {
    if (S) stack_push(S, MAKE_NONE());              // push and guard for null
}

// pushes a boolean, any non-zero value becomes true
void apex_push_boolean(ApexState* S, int b) {
    if (S) stack_push(S, MAKE_BOOL(b != 0));        // normalize to 0 or 1
}

// pushes an integer as a number value
void apex_push_integer(ApexState* S, long long n) {
    if (S) stack_push(S, MAKE_NUMBER((double)n));   // convert to double and box
}

// pushes a floating-point number
void apex_push_number(ApexState* S, double n) {
    if (S) stack_push(S, MAKE_NUMBER(n));           // box the double
}

// pushes a null-terminated string as an owned, non-interned value
void apex_push_string(ApexState* S, const char* s) {
    if (!S) return;                                 // null guard
    if (!s) { stack_push(S, MAKE_NONE()); return; } // null string becomes none
    StringObject* str = string_create(s, (int)strlen(s));  // allocate a fresh string
    stack_push(S, MAKE_STRING(str));                // push the string value
    value_decref(MAKE_STRING(str));                 // stack_push took its own ref
}

// pushes a string of a given length, may contain embedded zeros
void apex_push_lstring(ApexState* S, const char* s, size_t len) {
    if (!S) return;                                 // null guard
    if (!s) { stack_push(S, MAKE_NONE()); return; } // null string becomes none
    StringObject* str = string_create(s, (int)len); // allocate a fresh string
    stack_push(S, MAKE_STRING(str));                // push the string value
    value_decref(MAKE_STRING(str));                 // stack_push took its own ref
}

// pushes a light userdata value wrapping an opaque c pointer
void apex_push_light_userdata(ApexState* S, void* p) {
    if (S) stack_push(S, MAKE_LIGHTUSERDATA(p));    // box the pointer
}

// creates a new empty table and pushes it onto the stack
void apex_new_table(ApexState* S) {
    if (!S) return;                                 // null guard
    Table* t = table_create(8);                     // new table with default capacity
    stack_push(S, MAKE_TABLE(t));                   // push the table value
    value_decref(MAKE_TABLE(t));                    // stack holds the live reference
}

// creates a new empty table with pre-sized array and hash parts
void apex_new_table_sized(ApexState* S, int narr, int nrec) {
    if (!S) return;                                 // null guard
    int cap = narr > nrec ? narr : nrec;            // use the larger of the two hints
    if (cap < 8) cap = 8;                           // enforce a small minimum
    Table* t = table_create(cap);                   // new table with requested capacity
    stack_push(S, MAKE_TABLE(t));                   // push the table value
    value_decref(MAKE_TABLE(t));                    // stack holds the live reference
}

// returns the type tag of the value at the given index
ApexType apex_type(ApexState* S, int index) {
    if (!S) return APEX_TNONE;                      // null state: no type
    Value* vp = stack_at(S, index);                 // resolve slot
    if (!vp) return APEX_TNONE;                     // out of range: no type
    Value v = *vp;                                  // copy the value
    if (IS_NONE(v))          return APEX_TNONE;     // none
    if (IS_BOOL(v))          return APEX_TBOOLEAN;  // boolean
    if (IS_NUMBER(v) || IS_NAN(v)) return APEX_TNUMBER;  // number including nan
    if (IS_STRING(v))        return APEX_TSTRING;   // string
    if (IS_TABLE(v))         return APEX_TTABLE;    // table
    if (IS_FUNCTION(v))      return APEX_TFUNCTION; // function reference
    if (IS_LIGHTUSERDATA(v)) return APEX_TLIGHTUSERDATA;  // userdata
    if (IS_FUTURE(v))        return APEX_TFUTURE;   // future
    return APEX_TNONE;                              // fallback for unknown tags
}

// returns the type name of the value at the given index as a c string
const char* apex_type_name(ApexState* S, int index) {
    return apex_type_to_name(apex_type(S, index));  // map tag to name
}

// type predicates, return 1 if the value at the given index matches
int apex_is_none(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && IS_NONE(*v);                        // true only for none
}
int apex_is_boolean(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && IS_BOOL(*v);                        // true only for boolean
}
int apex_is_number(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && (IS_NUMBER(*v) || IS_NAN(*v));      // true for any numeric value
}
int apex_is_string(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && IS_STRING(*v);                      // true only for string
}
int apex_is_table(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && IS_TABLE(*v);                       // true only for table
}
int apex_is_function(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && IS_FUNCTION(*v);                    // true only for function ref
}
int apex_is_light_userdata(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && IS_LIGHTUSERDATA(*v);               // true only for userdata
}
int apex_is_future(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    return v && IS_FUTURE(*v);                      // true only for future
}

// converts the value at the given index to a c boolean
int apex_to_boolean(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    if (!v) return 0;                               // out of range: false
    if (IS_BOOL(*v)) return AS_BOOL(*v) ? 1 : 0;    // only real booleans convert
    return 0;                                       // everything else: false
}

// converts the value at the given index to a c integer
long long apex_to_integer(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    if (!v || !IS_NUMBER(*v)) return 0;             // non-number: 0
    return (long long)AS_NUMBER(*v);                // truncate toward zero
}

// converts the value at the given index to a c double
double apex_to_number(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    if (!v) return 0.0;                             // out of range: 0
    if (IS_NUMBER(*v)) return AS_NUMBER(*v);        // unbox the double
    if (IS_NAN(*v))    return 0.0;                  // real nan: 0
    return 0.0;                                     // everything else: 0
}

// converts the value at the given index to a c string, or null on failure
const char* apex_to_string(ApexState* S, int index) {
    return apex_to_lstring(S, index, NULL);         // delegate, ignore length
}

// converts to a c string and returns its length via the len out-parameter
const char* apex_to_lstring(ApexState* S, int index, size_t* len) {
    if (!S) { if (len) *len = 0; return NULL; }     // null state
    Value* vp = stack_at(S, index);                 // resolve slot
    if (!vp) { if (len) *len = 0; return NULL; }    // out of range
    Value v = *vp;                                  // copy the value

    if (IS_STRING(v)) {                             // string: return raw chars
        StringObject* s = AS_STRING(v);
        if (len) *len = (size_t)s->length;          // expose length
        return s->chars;                            // return internal buffer
    }

    if (IS_NUMBER(v)) {                             // number: format into scratch buffer
        double n = AS_NUMBER(v);
        int n_written;
        if (n == (long long)n && n >= -1e15 && n <= 1e15) {
            n_written = snprintf(S->num_buf, sizeof(S->num_buf),
                                 "%lld", (long long)n);  // integer form
        } else {
            n_written = snprintf(S->num_buf, sizeof(S->num_buf),
                                 "%.15g", n);           // general form
        }
        if (len) *len = (size_t)n_written;
        return S->num_buf;
    }

    if (IS_NAN(v)) {                                // real nan: return literal string
        if (len) *len = 3;
        return "nan";
    }

    if (IS_NONE(v)) {                               // none: return literal string
        if (len) *len = 4;
        return "none";
    }

    if (IS_BOOL(v)) {                               // boolean: return literal string
        const char* s = AS_BOOL(v) ? "true" : "false";
        if (len) *len = strlen(s);
        return s;
    }

    if (len) *len = 0;                              // unsupported: null with zero length
    return NULL;
}

// returns the light userdata pointer, or null if not a userdata
void* apex_to_light_userdata(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    if (!v || !IS_LIGHTUSERDATA(*v)) return NULL;   // non-userdata: null
    return AS_LIGHTUSERDATA(*v);                    // unwrap the pointer
}

// returns a generic opaque pointer for identity comparison, or null
const void* apex_to_pointer(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    if (!v) return NULL;                            // out of range
    Value vv = *v;                                  // copy the value
    if (IS_STRING(vv))        return (const void*)AS_STRING(vv);         // string pointer
    if (IS_TABLE(vv))         return (const void*)AS_TABLE(vv);          // table pointer
    if (IS_LIGHTUSERDATA(vv)) return AS_LIGHTUSERDATA(vv);               // userdata pointer
    if (IS_FUTURE(vv))        return (const void*)AS_FUTURE(vv);         // future pointer
    return NULL;                                    // everything else has no pointer form
}

// checks that argument narg has the given type, raising an error if not
void apex_check_type(ApexState* S, int narg, ApexType type) {
    ApexType t = apex_type(S, narg);                // actual type of argument
    if (t != type) {                                // mismatch: raise
        apex_raise(S, "argument %d: expected %s, got %s",
                   narg,
                   apex_type_to_name(type),         // expected type name
                   apex_type_to_name(t));           // actual type name
    }
}

// checks that argument narg is a number and returns it as an integer
long long apex_check_integer(ApexState* S, int narg) {
    apex_check_type(S, narg, APEX_TNUMBER);         // enforce number
    return apex_to_integer(S, narg);                // convert and return
}

// checks that argument narg is a number and returns it as a double
double apex_check_number(ApexState* S, int narg) {
    apex_check_type(S, narg, APEX_TNUMBER);         // enforce number
    return apex_to_number(S, narg);                 // convert and return
}

// checks that argument narg is a string and returns it as a c string
const char* apex_check_string(ApexState* S, int narg) {
    apex_check_type(S, narg, APEX_TSTRING);         // enforce string
    return apex_to_string(S, narg);                 // convert and return
}

// checks that argument narg is a string and returns its length via len
const char* apex_check_lstring(ApexState* S, int narg, size_t* len) {
    apex_check_type(S, narg, APEX_TSTRING);         // enforce string
    return apex_to_lstring(S, narg, len);           // convert and return
}

// checks that argument narg is a light userdata and returns its pointer
void* apex_check_light_userdata(ApexState* S, int narg) {
    apex_check_type(S, narg, APEX_TLIGHTUSERDATA);  // enforce userdata
    return apex_to_light_userdata(S, narg);         // unwrap and return
}

// pops a key and pushes the resulting value (none on miss)
void apex_get_table(ApexState* S, int index) {
    table_get_common(S, index);                     // shared with raw variant
}

// raw variant kept for api parity with lua, same semantics as above today
void apex_raw_get(ApexState* S, int index) {
    table_get_common(S, index);                     // shared with regular variant
}

// pushes the value stored under the given string key (none on miss)
void apex_get_field(ApexState* S, int index, const char* k) {
    if (!S || !k) return;                           // null guard
    Value key = make_key_string(S, k);              // intern the key string
    Value* tbl_slot = stack_at(S, index);           // resolve table slot
    if (!tbl_slot || !IS_TABLE(*tbl_slot)) {        // not a table: push none
        stack_push(S, MAKE_NONE());
        return;
    }
    Value out = MAKE_NONE();                        // default result
    table_get(AS_TABLE(*tbl_slot), key, &out);      // actual lookup
    stack_push(S, out);                             // push on stack
    if ((out & QNAN) == QNAN) value_decref(out);    // stack_push took its own ref
}

// raw variant kept for api parity with lua, same semantics as above today
void apex_raw_get_field(ApexState* S, int index, const char* k) {
    apex_get_field(S, index, k);                    // shared with regular variant
}

// pushes the value stored under the given numeric key (none on miss)
void apex_get_index(ApexState* S, int index, long long n) {
    if (!S) return;                                 // null guard
    Value key = MAKE_NUMBER((double)n);             // box the numeric key
    Value* tbl_slot = stack_at(S, index);           // resolve table slot
    if (!tbl_slot || !IS_TABLE(*tbl_slot)) {        // not a table: push none
        stack_push(S, MAKE_NONE());
        return;
    }
    Value out = MAKE_NONE();                        // default result
    table_get(AS_TABLE(*tbl_slot), key, &out);      // actual lookup
    stack_push(S, out);                             // push on stack
    if ((out & QNAN) == QNAN) value_decref(out);    // stack_push took its own ref
}

// raw variant kept for api parity with lua, same semantics as above today
void apex_raw_get_i(ApexState* S, int index, long long n) {
    apex_get_index(S, index, n);                    // shared with regular variant
}

// pops value and key, assigns tbl[key] = value
void apex_set_table(ApexState* S, int index) {
    if (!S) return;                                 // null guard
    Value* tbl_slot = stack_at(S, index);           // resolve target first
    Value val = stack_pop(S);                       // value on top
    Value key = stack_pop(S);                       // key below it
    if (tbl_slot && IS_TABLE(*tbl_slot)) {          // only if it's a table
        table_set(AS_TABLE(*tbl_slot), key, val);   // perform assignment
    }
    if ((key & QNAN) == QNAN) value_decref(key);    // release local key ref
    if ((val & QNAN) == QNAN) value_decref(val);    // release local value ref
}

// raw variant kept for api parity with lua, same semantics as above today
void apex_raw_set(ApexState* S, int index) {
    apex_set_table(S, index);                       // shared with regular variant
}

// pops value, assigns tbl[k] = value
void apex_set_field(ApexState* S, int index, const char* k) {
    if (!S || !k) return;                           // null guard
    Value* tbl_slot = stack_at(S, index);           // resolve target first
    Value val = stack_pop(S);                       // then pop the value
    Value key = make_key_string(S, k);              // intern the key string
    if (tbl_slot && IS_TABLE(*tbl_slot)) {          // only if it's a table
        table_set(AS_TABLE(*tbl_slot), key, val);   // perform assignment
    }
    if ((val & QNAN) == QNAN) value_decref(val);    // release local value ref
}

// raw variant kept for api parity with lua, same semantics as above today
void apex_raw_set_field(ApexState* S, int index, const char* k) {
    apex_set_field(S, index, k);                    // shared with regular variant
}

// pops value, assigns tbl[n] = value
void apex_set_index(ApexState* S, int index, long long n) {
    if (!S) return;                                 // null guard
    Value* tbl_slot = stack_at(S, index);           // resolve target first
    Value val = stack_pop(S);                       // then pop the value
    Value key = MAKE_NUMBER((double)n);             // box the numeric key
    if (tbl_slot && IS_TABLE(*tbl_slot)) {          // only if it's a table
        table_set(AS_TABLE(*tbl_slot), key, val);   // perform assignment
    }
    if ((val & QNAN) == QNAN) value_decref(val);    // release local value ref
}

// raw variant kept for api parity with lua, same semantics as above today
void apex_raw_set_i(ApexState* S, int index, long long n) {
    apex_set_index(S, index, n);                    // shared with regular variant
}

// returns the number of entries in the table, 0 if not a table
long long apex_table_size(ApexState* S, int index) {
    Value* v = S ? stack_at(S, index) : NULL;       // resolve slot with null guard
    if (!v || !IS_TABLE(*v)) return 0;              // non-table: zero
    return (long long)table_size(AS_TABLE(*v));     // count entries
}

// pushes the value of a global, returns 1 if found and 0 otherwise
int apex_get_global(ApexState* S, const char* name) {
    if (!S || !name || !S->active_chunk) return 0;  // null guard and no chunk loaded
    int idx = bytecode_get_global(S->active_chunk, name);  // lookup global index
    if (idx < 0) return 0;                          // not found
    Value v = S->vm->globals[idx];                  // read from vm global table
    stack_push(S, v);                               // push on stack
    return 1;                                       // report success
}

// pops a value and stores it in the named global
void apex_set_global(ApexState* S, const char* name) {
    if (!S || !name) {                              // null guard
        Value v = stack_pop(S);                     // still pop to keep stack balanced
        if ((v & QNAN) == QNAN) value_decref(v);    // release the popped value
        return;
    }

    Value v = stack_pop(S);                         // pop the new value

    PendingGlobal* p = pending_global_get(S, name); // find or create pending entry
    value_decref(p->value);                         // release old pending value
    p->value = v;                                   // store the new value
    if ((v & QNAN) == QNAN) value_incref(v);        // pending list holds a reference

    if (S->active_chunk) {                          // if a chunk is loaded
        int idx = bytecode_get_global(S->active_chunk, name);
        if (idx >= 0) {                             // and the global exists in it
            Value old = S->vm->globals[idx];        // save old for release
            S->vm->globals[idx] = v;                // install new value
            if ((v & QNAN) == QNAN) value_incref(v);  // vm holds a reference
            if ((old & QNAN) == QNAN) value_decref(old);  // release old
        }
    }

    if ((v & QNAN) == QNAN) value_decref(v);        // release the pop reference
}

// compiles and runs apex source, returns 0 on success and non-zero on error
int apex_run_string(ApexState* S, const char* code, const char* chunkname) {
    if (!S || !code) return 1;                      // null guard

    const char* name = chunkname ? chunkname : "<string>";  // default chunk name
    S->error_message[0] = '\0';                     // clear previous error
    S->error_active = 0;                            // reset error flag

    Tokenizer* tok = tokenizer_create(code, name);  // create tokenizer
    int tcount = 0;                                 // token count output
    Token* tokens = tokenizer_tokenize(tok, &tcount);  // tokenize the source
    if (!tokens || tokenizer_has_error(tok)) {      // tokenization failed
        tokenizer_destroy(tok);                     // free tokenizer
        snprintf(S->error_message, sizeof(S->error_message),
                 "tokenization failed");            // record error
        return 1;                                   // report failure
    }

    Parser* par = parser_create(tokens, tcount, name, code);  // create parser

    // seed the parser's symbol table with every name the host provides.
    for (CFunctionEntry* e = S->c_funcs; e; e = e->next) {
        parser_declare_symbol(par, e->name, PARSER_SYM_FUNCTION,
                              TYPE_FUNCTION, -1, 0, 0);
    }
    for (PendingGlobal* p = S->pending_globals; p; p = p->next) {
        parser_declare_symbol(par, p->name, PARSER_SYM_VARIABLE,
                              TYPE_ANY, 0, 0, 0);
    }

    ASTNode* ast = parse_program(par);              // parse the program
    if (!ast || parser_had_errors(par)) {           // parsing failed
        cleanup_compile(tok, par, ast, NULL);       // free compilation state
        snprintf(S->error_message, sizeof(S->error_message),
                 "parse failed");                   // record error
        return 1;                                   // report failure
    }

    BytecodeChunk* chunk = bytecode_create();       // create a fresh bytecode chunk
    CodeGenerator* cg = codegen_create(chunk);      // create a code generator
    if (!codegen_generate(cg, ast)) {               // code generation failed
        cleanup_compile(tok, par, ast, cg);         // free compile state
        bytecode_destroy(chunk);                    // free the fresh chunk
        snprintf(S->error_message, sizeof(S->error_message),
                 "code generation failed");         // record error
        return 1;                                   // report failure
    }

    // replace the active chunk.
    if (S->active_chunk) bytecode_destroy(S->active_chunk);  // free old chunk
    S->active_chunk = chunk;                        // install new chunk

    // install every pending global into the chunk's global table and pre-load the value into vm->globals
    for (PendingGlobal* p = S->pending_globals; p; p = p->next) {
        int idx = bytecode_add_global(chunk, p->name);
        if (idx >= 0 && idx < VM_MAX_GLOBALS) {
            Value old = S->vm->globals[idx];        // save for release
            S->vm->globals[idx] = p->value;         // install the pending value
            if ((p->value & QNAN) == QNAN) value_incref(p->value);
            if ((old & QNAN) == QNAN) value_decref(old);
        }
    }
    S->vm->preserve_globals = true;                 // tell vm_execute to keep them

    vm_execute(S->vm, chunk);                       // run the program to completion

    int rc = S->vm->had_error ? 1 : 0;              // success if no vm error

    cleanup_compile(tok, par, ast, cg);             // free compile intermediates

    return rc;                                      // report result
}

// reads the file at the given path and runs it as apex source
int apex_run_file(ApexState* S, const char* path) {
    if (!S || !path) return 1;                      // null guard

    FILE* f = fopen(path, "rb");                    // open the file
    if (!f) {                                       // open failed
        snprintf(S->error_message, sizeof(S->error_message),
                 "cannot open file '%s'", path);    // record error
        return 1;                                   // report failure
    }

    fseek(f, 0, SEEK_END);                          // seek to end
    long size = ftell(f);                           // get file size
    fseek(f, 0, SEEK_SET);                          // seek back to start
    if (size < 0) { fclose(f); return 1; }          // negative size is an error

    char* code = (char*)malloc((size_t)size + 1);   // allocate source buffer
    if (!code) { fclose(f); return 1; }             // allocation failed

    size_t nread = fread(code, 1, (size_t)size, f); // read the file
    code[nread] = '\0';                             // null terminate
    fclose(f);                                      // close the file

    int rc = apex_run_string(S, code, path);        // run the loaded source
    free(code);                                     // release the source buffer
    return rc;                                      // report result
}

// registers a c function under the given name
void apex_register_function(ApexState* S, const char* name, ApexCFunction f) {
    if (!S || !name || !f) return;                  // null guard

    for (CFunctionEntry* e = S->c_funcs; e; e = e->next) {  // look for existing entry
        if (strcmp(e->name, name) == 0) {           // same name: replace
            e->fn = f;                              // update the function pointer
            return;                                 // done
        }
    }

    CFunctionEntry* e = (CFunctionEntry*)malloc(sizeof(CFunctionEntry));  // allocate entry
    e->name = strdup(name);                         // copy the name
    e->fn = f;                                      // store the function
    e->next = NULL;                                 // new tail

    if (S->c_funcs_tail) {                          // append to existing list
        S->c_funcs_tail->next = e;
    } else {                                        // first entry
        S->c_funcs = e;
    }
    S->c_funcs_tail = e;                            // update tail
}

// called from the vm when apex code invokes a name that matches a registered c function
static bool apex_c_function_dispatch(VM* vm,
                                     const char* name,
                                     int arg_count,
                                     Value* args,
                                     Value* result) {
    ApexState* S = (ApexState*)vm->c_function_state;  // recover our state
    if (!S) return false;                             // no state attached

    CFunctionEntry* e = S->c_funcs;                   // walk registered functions
    while (e) {
        if (strcmp(e->name, name) == 0) {             // name matches
            int base = S->top;                        // remember pre-call stack depth

            for (int i = 0; i < arg_count; i++)       // push every argument
                stack_push(S, args[i]);               // so the c function can read them

            S->error_active = 0;                      // clear pending error

            int nres = 0;                             // number of results pushed
            if (setjmp(S->error_jmp) == 0) {          // set up protected context
                S->jmp_active = 1;                    // mark protected
                nres = e->fn(S);                      // call the user function
                S->jmp_active = 0;                    // leave protected state
            } else {                                  // apex_raise longjmp'd back
                S->jmp_active = 0;                    // leave protected state
                vm->had_error = true;                 // propagate to vm
                *result = MAKE_NONE();                // no result
                while (S->top > base) {               // drop leftover stack values
                    Value v = S->stack[--S->top];
                    if ((v & QNAN) == QNAN) value_decref(v);
                }
                return true;                          // handled, error reported
            }

            if (S->error_active) {                    // c function set an error without raising
                vm->had_error = true;                 // propagate to vm
                *result = MAKE_NONE();                // no result
            } else if (nres >= 1 && S->top > base) {  // c function returned a value
                *result = S->stack[S->top - 1];       // hand ownership to vm
                S->top--;                             // pop, keep reference
            } else {                                  // c function returned nothing
                *result = MAKE_NONE();                // default result
            }

            while (S->top > base) {                   // clean up any extra pushed values
                Value v = S->stack[--S->top];
                if ((v & QNAN) == QNAN) value_decref(v);
            }
            return true;                              // handled successfully
        }
        e = e->next;                                  // advance
    }
    return false;                                     // not a registered function
}

// returns the underlying vm pointer, provided for advanced use only
struct VM* apex_get_vm(ApexState* S) {
    return S ? S->vm : NULL;                          // null-safe accessor
}
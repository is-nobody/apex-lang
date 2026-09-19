// source/libraries/json_module.c
// Implementation of JSON Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "json_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif

// dynamic string builder for efficient text assembly
typedef struct {
    char* buffer;                                                                 // dynamic buffer
    int length;                                                                   // current length
    int capacity;                                                                 // total capacity
} StringBuilder;

static void sb_init(StringBuilder* sb, int initial_capacity) {
    sb->capacity = initial_capacity > 16 ? initial_capacity : 16;                 // min capacity
    sb->buffer = (char*)malloc(sb->capacity);                                     // allocate buffer
    if (!sb->buffer) { sb->length = 0; return; }                                  // allocation failed
    sb->length = 0;                                                               // start empty
    sb->buffer[0] = '\0';                                                         // null terminate
}

static void sb_append(StringBuilder* sb, const char* str, int len) {
    if (!sb->buffer) return;                                                      // buffer not initialized
    if (sb->length + len + 1 > sb->capacity) {                                    // need more space
        int new_cap = (sb->length + len + 1) * 2;                                 // double capacity
        char* new_buf = (char*)realloc(sb->buffer, new_cap);                      // resize
        if (!new_buf) return;                                                     // allocation failed
        sb->buffer = new_buf;                                                     // update buffer
        sb->capacity = new_cap;                                                   // update capacity
    }
    memcpy(sb->buffer + sb->length, str, len);                                    // copy string
    sb->length += len;                                                            // update length
    sb->buffer[sb->length] = '\0';                                                // null terminate
}

static void sb_free(StringBuilder* sb) {
    if (sb->buffer) free(sb->buffer);                                             // free buffer
    sb->buffer = NULL;                                                            // clear pointer
    sb->length = 0;                                                               // reset length
}

// skips whitespace in a json string
static void skip_ws(const char** s) {
    while (**s && isspace((unsigned char)**s)) (*s)++;                            // skip whitespace chars
}

// parses a json string with escape sequence handling
static bool parse_string_raw(const char** s, char** out_str, int* out_len) {
    if (**s != '"') return false;                                                 // must start with quote
    (*s)++;                                                                       // skip opening quote
    const char* start = *s;                                                       // start of string content
    int len = 0;                                                                  // length counter
    while (**s && **s != '"') {                                                   // find closing quote
        if (**s == '\\') {                                                        // escape sequence
            (*s)++;                                                               // skip backslash
            if (!**s) return false;                                               // unexpected end
        }
        (*s)++;                                                                   // advance
        len++;                                                                    // count character
    }
    if (**s != '"') return false;                                                 // missing closing quote
    
    char* buffer = (char*)malloc(len + 1);                                        // allocate buffer
    if (!buffer) return false;                                                    // allocation failed
    
    const char* p = start;                                                        // parse pointer
    int idx = 0;                                                                  // output index
    while (p < *s) {                                                              // process escaped string
        if (*p == '\\') {                                                         // escape sequence
            p++;                                                                  // skip backslash
            switch (*p) {                                                         // handle escape
                case '"': buffer[idx++] = '"'; break;                             // quote
                case '\\': buffer[idx++] = '\\'; break;                           // backslash
                case '/': buffer[idx++] = '/'; break;                             // slash
                case 'b': buffer[idx++] = '\b'; break;                            // backspace
                case 'f': buffer[idx++] = '\f'; break;                            // form feed
                case 'n': buffer[idx++] = '\n'; break;                            // newline
                case 'r': buffer[idx++] = '\r'; break;                            // carriage return
                case 't': buffer[idx++] = '\t'; break;                            // tab
                case 'u': buffer[idx++] = '?'; p += 4; break;                     // unicode (simplified)
                default: buffer[idx++] = *p; break;                               // unknown escape
            }
        } else {
            buffer[idx++] = *p;                                                   // copy char
        }
        p++;
    }
    buffer[idx] = '\0';                                                           // null terminate
    *out_str = buffer;                                                            // return buffer
    *out_len = idx;                                                               // return length
    (*s)++;                                                                       // skip closing quote
    return true;                                                                  // success
}

// parses a json number
static bool parse_number(const char** s, Value* out_value) {
    char* endptr;                                                                 // end pointer for strtod
    double val = strtod(*s, &endptr);                                             // parse double
    if (endptr == *s) return false;                                               // no number parsed
    *out_value = MAKE_NUMBER(val);                                                // store as number
    *s = endptr;                                                                  // advance pointer
    return true;                                                                  // success
}

// recursive json parser that builds vm values; VM-agnostic so it can run on a worker
static bool json_parse_value(const char** json_str, Value* out_value) {
    skip_ws(json_str);                                                            // skip whitespace
    if (!**json_str) return false;                                                // unexpected end
    
    char c = **json_str;                                                          // current char
    
    if (strncmp(*json_str, "null", 4) == 0) {                                     // null literal
        *out_value = MAKE_NONE();                                                 // store as none
        *json_str += 4;                                                           // advance
        return true;                                                              // success
    }
    if (strncmp(*json_str, "true", 4) == 0) {                                     // true literal
        *out_value = MAKE_BOOL(true);                                             // store true
        *json_str += 4;                                                           // advance
        return true;                                                              // success
    }
    if (strncmp(*json_str, "false", 5) == 0) {                                    // false literal
        *out_value = MAKE_BOOL(false);                                            // store false
        *json_str += 5;                                                           // advance
        return true;                                                              // success
    }
    
    if (c == '"') {                                                               // string value
        char* str_val = NULL;                                                     // parsed string
        int len = 0;                                                              // string length
        if (!parse_string_raw(json_str, &str_val, &len)) return false;            // parse failed
        *out_value = MAKE_STRING(string_create(str_val, len));                    // fresh refcounted string
        free(str_val);                                                            // free temporary
        return true;                                                              // success
    }
    
    if (c == '-' || isdigit(c)) {                                                 // number value
        return parse_number(json_str, out_value);                                 // parse number
    }
    
    if (c == '[') {                                                               // array value
        (*json_str)++;                                                            // skip opening bracket
        Table* table = table_create(8);                                           // create table
        *out_value = MAKE_TABLE(table);                                           // store as table
        skip_ws(json_str);                                                        // skip whitespace
        int index = 1;                                                            // 1-based index
        if (**json_str != ']') {                                                  // non-empty array
            while (1) {                                                           // parse elements
                Value item;                                                       // element value
                if (!json_parse_value(json_str, &item)) {                         // parse element
                    value_decref(*out_value);                                     // release table
                    return false;                                                 // parse failed
                }
                Value k = MAKE_NUMBER((double)index++);                           // create index key
                table_set(table, k, item);                                        // store element
                value_decref(item);                                               // release element
                skip_ws(json_str);                                                // skip whitespace
                if (**json_str == ',') {                                          // comma separator
                    (*json_str)++;                                                // skip comma
                } else {
                    break;                                                        // end of array
                }
            }
        }
        if (**json_str != ']') {                                                  // missing closing bracket
            value_decref(*out_value);                                             // release table
            return false;                                                         // parse failed
        }
        (*json_str)++;                                                            // skip closing bracket
        return true;                                                              // success
    }
    
    if (c == '{') {                                                               // object value
        (*json_str)++;                                                            // skip opening brace
        Table* table = table_create(8);                                           // create table
        *out_value = MAKE_TABLE(table);                                           // store as table
        skip_ws(json_str);                                                        // skip whitespace
        if (**json_str != '}') {                                                  // non-empty object
            while (1) {                                                           // parse key-value pairs
                skip_ws(json_str);                                                // skip whitespace
                if (**json_str != '"') {                                          // key must be string
                    value_decref(*out_value);                                     // release table
                    return false;                                                 // parse failed
                }
                char* key_str = NULL;                                             // parsed key
                int key_len = 0;                                                  // key length
                if (!parse_string_raw(json_str, &key_str, &key_len)) {            // parse key
                    value_decref(*out_value);                                     // release table
                    return false;                                                 // parse failed
                }
                skip_ws(json_str);                                                // skip whitespace
                if (**json_str != ':') {                                          // missing colon
                    free(key_str);                                                // free key
                    value_decref(*out_value);                                     // release table
                    return false;                                                 // parse failed
                }
                (*json_str)++;                                                    // skip colon
                Value val;                                                        // value
                if (!json_parse_value(json_str, &val)) {                          // parse value
                    free(key_str);                                                // free key
                    value_decref(*out_value);                                     // release table
                    return false;                                                 // parse failed
                }
                Value k = MAKE_STRING(string_create(key_str, key_len));           // fresh refcounted key
                table_set(table, k, val);                                         // store key-value
                value_decref(k);                                                  // release key
                value_decref(val);                                                // release value
                free(key_str);                                                    // free key buffer
                skip_ws(json_str);                                                // skip whitespace
                if (**json_str == ',') {                                          // comma separator
                    (*json_str)++;                                                // skip comma
                } else {
                    break;                                                        // end of object
                }
            }
        }
        if (**json_str != '}') {                                                  // missing closing brace
            value_decref(*out_value);                                             // release table
            return false;                                                         // parse failed
        }
        (*json_str)++;                                                            // skip closing brace
        return true;                                                              // success
    }
    
    return false;                                                                 // unknown token
}

// appends a json-escaped string to the builder
static void append_escaped(StringBuilder* sb, const char* str) {
    sb_append(sb, "\"", 1);                                                       // opening quote
    while (*str) {                                                                // iterate over string
        unsigned char c = *str;                                                   // current char
        switch (c) {                                                              // handle escapes
            case '"': sb_append(sb, "\\\"", 2); break;                            // quote
            case '\\': sb_append(sb, "\\\\", 2); break;                           // backslash
            case '\b': sb_append(sb, "\\b", 2); break;                            // backspace
            case '\f': sb_append(sb, "\\f", 2); break;                            // form feed
            case '\n': sb_append(sb, "\\n", 2); break;                            // newline
            case '\r': sb_append(sb, "\\r", 2); break;                            // carriage return
            case '\t': sb_append(sb, "\\t", 2); break;                            // tab
            default:
                if (c < 0x20) {                                                   // control char
                    char buf[8];                                                  // unicode escape buffer
                    snprintf(buf, sizeof(buf), "\\u%04x", c);                     // format as unicode
                    sb_append(sb, buf, 6);                                        // append escape
                } else {
                    char buf[2] = { (char)c, 0 };                                 // single char
                    sb_append(sb, buf, 1);                                        // append char
                }
                break;
        }
        str++;                                                                    // advance
    }
    sb_append(sb, "\"", 1);                                                       // closing quote
}

// recursively encodes a vm value to json; VM-agnostic so it can run on a worker
static void json_encode_value(Value value, StringBuilder* sb) {
    if (IS_NUMBER(value)) {                                                       // number value
        char buf[64];                                                             // buffer for number
        double num = AS_NUMBER(value);                                            // extract number
        if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) {              // integer
            snprintf(buf, sizeof(buf), "%lld", (long long)num);                   // format as integer
        } else {
            snprintf(buf, sizeof(buf), "%.15g", num);                             // format as float
        }
        sb_append(sb, buf, (int)strlen(buf));                                     // append number
    } else if (IS_BOOL(value)) {                                                  // boolean value
        sb_append(sb, AS_BOOL(value) ? "true" : "false", AS_BOOL(value) ? 4 : 5); // append bool
    } else if (IS_STRING(value)) {                                                // string value
        append_escaped(sb, AS_STRING(value)->chars);                              // append escaped string
    } else if (IS_TABLE(value)) {                                                 // table value
        Table* t = AS_TABLE(value);                                               // unwrap table
        if (TABLE_TOTAL_COUNT(t) == 0) {                                          // empty table
            sb_append(sb, "{}", 2);                                               // empty object
            return;
        }
        
        bool is_array = (t->array_count > 0 && t->hash_count == 0);              // check if array
        
        if (is_array) {                                                           // array
            sb_append(sb, "[", 1);                                                // opening bracket
            for (int i = 0; i < t->array_count; i++) {                            // iterate elements
                if (i > 0) sb_append(sb, ", ", 2);                                // comma separator
                json_encode_value(t->array_part[i], sb);                          // encode element
            }
            sb_append(sb, "]", 1);                                                // closing bracket
        } else {                                                                  // object
            sb_append(sb, "{", 1);                                                // opening brace
            bool first = true;                                                    // first item flag
            
            for (int i = 0; i < t->array_count; i++) {                            // array part as key-value
                if (!IS_BOOL(t->array_part[i]) || AS_BOOL(t->array_part[i])) {    // skip false values
                    if (!first) sb_append(sb, ", ", 2);                           // comma separator
                    first = false;                                                // not first anymore
                    char key[32];                                                 // numeric key buffer
                    snprintf(key, sizeof(key), "%d", i + 1);                      // 1-based index
                    append_escaped(sb, key);                                      // append key
                    sb_append(sb, ": ", 2);                                       // colon separator
                    json_encode_value(t->array_part[i], sb);                      // encode value
                }
            }
            
            for (int i = 0; i < t->capacity; i++) {                               // hash part
                TableEntry* entry = t->entries[i];                                // bucket head
                while (entry) {                                                   // traverse chain
                    if (!first) sb_append(sb, ", ", 2);                           // comma separator
                    first = false;                                                // not first anymore
                    if (IS_STRING(entry->key)) {                                  // string key
                        append_escaped(sb, AS_STRING(entry->key)->chars);         // append key
                    } else if (IS_NUMBER(entry->key)) {                           // number key
                        char num_buf[64];                                         // number buffer
                        snprintf(num_buf, sizeof(num_buf), "%g", AS_NUMBER(entry->key));  // format
                        append_escaped(sb, num_buf);                              // append key
                    }
                    sb_append(sb, ": ", 2);                                       // colon separator
                    json_encode_value(entry->value, sb);                          // encode value
                    entry = entry->next;                                          // advance
                }
            }
            sb_append(sb, "}", 1);                                                // closing brace
        }
    } else {
        sb_append(sb, "null", 4);                                                 // null
    }
}

// parses a json string into a vm value; runs on the caller's thread or a worker
static Value json_decode_impl(const char* str) {
    const char* p = str;                                                          // cursor over input
    Value result;                                                                 // parsed result
    if (!json_parse_value(&p, &result)) {                                         // parse failed
        return MAKE_NONE();                                                       // return none
    }
    return result;                                                                // return parsed value
}

// serializes a vm value to json text; runs on the caller's thread or a worker
static Value json_encode_impl(Value value) {
    StringBuilder sb;                                                             // string builder
    sb_init(&sb, 256);                                                            // init builder
    json_encode_value(value, &sb);                                                // encode value
    const char* buf = sb.buffer ? sb.buffer : "";                                 // guard against OOM
    Value out = MAKE_STRING(string_create(buf, sb.length));                       // fresh refcounted string
    sb_free(&sb);                                                                 // free builder
    return out;                                                                   // return encoded json
}

// forward declaration for the recursive snapshot helper; must precede any
// caller because json_snapshot_value recurses into it
static Table* json_snapshot_table(Table* src);

// copy a value for the worker's private view: interned strings are shared
// because their refcount is immortal and never written, non-interned strings
// are duplicated so the worker never races the main thread on their
// refcounts, and tables are deep-copied because json.encode recurses through
// arbitrarily nested structures
static Value json_snapshot_value(Value v) {
    if (IS_STRING(v)) {                                          // string: interned shared, others copied
        StringObject* s = AS_STRING(v);
        if (s->header.ref_count == INT_MAX) return v;            // interned: immortal, no refcount mutation
        return MAKE_STRING(string_create(s->chars, s->length));  // fresh copy, worker-owned
    }
    if (IS_TABLE(v)) {                                           // table: recurse into a worker-owned copy
        return MAKE_TABLE(json_snapshot_table(AS_TABLE(v)));
    }
    return v;                                                    // number, bool, none: no refcount handling
}

// duplicate a table's structure (buckets, chains, array part) for a worker,
// deep-copying every string and nested table reachable from it
static Table* json_snapshot_table(Table* src) {
    if (!src) return NULL;                                       // guard against null
    Table* dst = table_create(src->capacity);                    // fresh table, same bucket count

    if (src->array_part != NULL) {                               // copy occupied array slots
        for (int i = 0; i < src->array_count; i++) {
            Value v = src->array_part[i];
            if (!IS_NONE(v)) {
                Value cv = json_snapshot_value(v);
                table_set_int(dst, i, cv);                       // table_set_int increfs the value
                value_decref(cv);                                // release our temporary reference
            }
        }
    }

    if (src->entries != NULL) {                                  // copy hash bucket chains
        for (int i = 0; i < src->capacity; i++) {
            TableEntry* e = src->entries[i];
            while (e) {
                Value ck = json_snapshot_value(e->key);
                Value cv = json_snapshot_value(e->value);
                table_set(dst, ck, cv);                          // table_set increfs key and value
                value_decref(ck);                                // release our temporary references
                value_decref(cv);
                e = e->next;
            }
        }
    }
    return dst;                                                  // return worker-owned copy
}

// create a leaf future ready to be resolved by a background worker
static FutureObject* json_make_leaf_future(void) {
    FutureObject* fut = (FutureObject*)calloc(1, sizeof(FutureObject));  // zero-init for safe teardown
    fut->header.ref_count = 1;                   // caller holds one reference
    fut->header.type = VAL_FUTURE;               // mark type as future
    fut->result = MAKE_NONE();                   // filled in when resolved
    fut->state = 0;                              // pending
    fut->func_idx = -1;                          // leaf: no coroutine body
    fut->awaiting = MAKE_NONE();                 // not awaiting
    fut->saved_dest_reg = -1;                    // unused for leaf
    fut->saved_iter_depth = -1;                  // no active loops
    fut->saved_table_iter_depth = -1;            // no table iterators
    return fut;                                  // return fresh future
}

// packed arguments for the json decode worker
typedef struct {
    char* input;                             // owned copy of the json text
    int input_len;                           // length in bytes
} JsonDecodeArgs;

// packed arguments for the json encode worker
typedef struct {
    Value snapshot;                          // worker-owned snapshot of the input value
} JsonEncodeArgs;

// task descriptor handed to a background worker
typedef struct {
    VM* vm;                                  // vm pointer for completion push
    FutureObject* fut;                       // future to resolve with the task result
    Value (*fn)(void*);                      // kernel executed on the worker
    void (*free_fn)(void*);                  // releases the packed argument struct
    void* arg;                               // packed argument struct
} JsonAsyncTask;

// release a JsonDecodeArgs and its owned input buffer
static void json_decode_args_free(void* p) {
    JsonDecodeArgs* a = (JsonDecodeArgs*)p;  // unpack argument struct
    free(a->input);                          // release the copied json text
    free(a);                                 // release struct itself
}

// release a JsonEncodeArgs and its owned snapshot value
static void json_encode_args_free(void* p) {
    JsonEncodeArgs* a = (JsonEncodeArgs*)p;  // unpack argument struct
    value_decref(a->snapshot);               // destroy the snapshot and everything it holds
    free(a);                                 // release struct itself
}

// release a JsonAsyncTask and its packed argument struct
static void json_task_destroy(void* p) {
    JsonAsyncTask* t = (JsonAsyncTask*)p;    // unpack task descriptor
    if (t->free_fn) t->free_fn(t->arg);      // release argument struct
    free(t);                                 // free descriptor itself
}

// worker thread entry: runs the kernel and posts the result
static void* json_thread(void* p) {
    JsonAsyncTask* t = (JsonAsyncTask*)p;    // unpack task descriptor
    Value result = t->fn(t->arg);            // run the kernel off the event loop
    vm_push_completion(t->vm, t->fut, result);  // hand off to scheduler
    json_task_destroy(t);                    // release descriptor and args
    return NULL;                             // thread exit
}

// spawn a detached worker thread running the given task
static void json_spawn_worker(VM* vm, FutureObject* fut, void* (*fn)(void*),
                              void* arg, void (*arg_free)(void*)) {
    APEX_MUTEX_LOCK(&vm->completion_mutex);  // reserve a worker slot
    vm->pending_workers++;                   // count pending worker
    APEX_MUTEX_UNLOCK(&vm->completion_mutex);// release lock

    bool ok = false;                         // spawn success flag
#ifdef _WIN32
    uintptr_t h = _beginthreadex(NULL, 0,               // windows thread
                                 (unsigned (__stdcall *)(void*))(void*)fn,
                                 arg, 0, NULL);
    if (h) { CloseHandle((HANDLE)h); ok = true; }       // detach handle
#else
    pthread_t tid;                           // posix thread handle
    if (pthread_create(&tid, NULL, fn, arg) == 0) {     // start thread
        pthread_detach(tid);                 // detach
        ok = true;                           // mark success
    }
#endif

    if (!ok) {                               // spawn failed
        vm_push_completion(vm, fut, MAKE_NONE());  // complete future with none
        if (arg_free) arg_free(arg);         // release unused argument
    }
}

// run fn asynchronously inside a coroutine, synchronously otherwise
static bool json_run_async_or_sync(VM* vm, Value (*fn)(void*),
                                   void (*free_fn)(void*), void* arg,
                                   Value* result) {
    if (vm->builtin_async) {          // inside a coroutine: never block the loop
        FutureObject* fut = json_make_leaf_future();  // fresh pending future
        value_incref(MAKE_FUTURE(fut));      // worker holds one reference

        JsonAsyncTask* t = (JsonAsyncTask*)malloc(sizeof(JsonAsyncTask));  // pack task descriptor
        if (!t) {                            // allocation failed
            value_decref(MAKE_FUTURE(fut));  // release the future
            if (free_fn) free_fn(arg);       // release argument
            *result = MAKE_NONE();           // return none
            return true;                     // builtin handled
        }
        t->vm = vm;                          // store vm pointer
        t->fut = fut;                        // store future
        t->fn = fn;                          // store kernel
        t->free_fn = free_fn;                // store cleanup function
        t->arg = arg;                        // store argument struct

        json_spawn_worker(vm, fut, json_thread, t, json_task_destroy);  // offload
        *result = MAKE_FUTURE(fut);          // return pending future
    } else {                                 // top level: nothing else is runnable
        *result = fn(arg);                   // run the kernel inline
        if (free_fn) free_fn(arg);           // release argument struct
    }
    return true;                             // builtin handled
}

// worker entry point for json.decode
static Value json_decode_kernel(void* p) {
    JsonDecodeArgs* a = (JsonDecodeArgs*)p;  // unpack argument struct
    return json_decode_impl(a->input);       // parse the copied json text
}

// worker entry point for json.encode
static Value json_encode_kernel(void* p) {
    JsonEncodeArgs* a = (JsonEncodeArgs*)p;  // unpack argument struct
    return json_encode_impl(a->snapshot);    // encode the worker-owned snapshot
}

// main dispatcher for json module built-in functions
bool json_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "json.decode") == 0) {                                       // parse json
        if (arg_count < 1 || !IS_STRING(args[0])) {                               // validate string
            *result = MAKE_NONE();                                                // invalid
            return true;                                                          // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                             // input string

        if (vm->builtin_async) {                                           // inside coroutine: copy and offload
            JsonDecodeArgs* a = (JsonDecodeArgs*)malloc(sizeof(JsonDecodeArgs));  // pack argument struct
            if (!a) { *result = MAKE_NONE(); return true; }                       // allocation failed
            a->input = (char*)malloc(input_str->length + 1);                      // copy json text
            if (!a->input) {                                                      // allocation failed
                free(a);                                                          // release struct
                *result = MAKE_NONE();                                            // return none
                return true;                                                      // builtin handled
            }
            memcpy(a->input, input_str->chars, input_str->length);                // capture input bytes
            a->input[input_str->length] = '\0';                                   // NUL terminate
            a->input_len = input_str->length;                                     // store length
            return json_run_async_or_sync(vm, json_decode_kernel,
                                          json_decode_args_free, a, result);
        }

        *result = json_decode_impl(input_str->chars);                             // top level: parse directly
        return true;                                                              // builtin handled
    }
    
    if (strcmp(name, "json.encode") == 0) {                                       // json serialize
        if (arg_count < 1) {                                                      // need value
            *result = MAKE_NONE();                                                // invalid
            return true;                                                          // builtin handled
        }

        if (vm->builtin_async) {                                           // inside coroutine: snapshot and offload
            JsonEncodeArgs* a = (JsonEncodeArgs*)malloc(sizeof(JsonEncodeArgs));  // pack argument struct
            if (!a) { *result = MAKE_NONE(); return true; }                       // allocation failed
            a->snapshot = json_snapshot_value(args[0]);                           // worker-owned structural copy
            return json_run_async_or_sync(vm, json_encode_kernel,
                                          json_encode_args_free, a, result);
        }

        *result = json_encode_impl(args[0]);                                      // top level: encode directly
        return true;                                                              // builtin handled
    }

    return false;                                                                 // not a recognized builtin
}
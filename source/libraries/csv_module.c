// source/libraries/csv_module.c
// Implementation of CSV Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "csv_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <process.h>
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

static void sb_append_char(StringBuilder* sb, char c) {
    if (!sb->buffer) return;                                                      // buffer not initialized
    if (sb->length + 2 > sb->capacity) {                                          // need more space
        int new_cap = (sb->length + 2) * 2;                                       // double capacity
        char* new_buf = (char*)realloc(sb->buffer, new_cap);                      // resize
        if (!new_buf) return;                                                     // allocation failed
        sb->buffer = new_buf;                                                     // update buffer
        sb->capacity = new_cap;                                                   // update capacity
    }
    sb->buffer[sb->length++] = c;                                                 // append char
    sb->buffer[sb->length] = '\0';                                                // null terminate
}

static void sb_free(StringBuilder* sb) {
    if (sb->buffer) free(sb->buffer);                                             // free buffer
    sb->buffer = NULL;                                                            // clear pointer
    sb->length = 0;                                                               // reset length
}

// csv parser state with position (rfc 4180 compliant)
typedef struct {
    const char* data;                                                            // input data
    int pos;                                                                     // current position
    int len;                                                                     // total length
} CsvParser;

static char csv_peek(CsvParser* p) {
    if (!p->data || p->pos >= p->len) return '\0';                               // end of input
    return p->data[p->pos];                                                      // peek char
}

static char csv_advance(CsvParser* p) {
    if (!p->data || p->pos >= p->len) return '\0';                              // end of input
    return p->data[p->pos++];                                                   // return and advance
}

static bool csv_has_next(CsvParser* p) {
    return p->data && p->pos < p->len;                                          // check if more input
}

// checks if a string looks like a number
static bool is_numeric(const char* str) {
    if (!str || *str == '\0') return false;                                     // empty string
    char* endptr;                                                               // end pointer for strtod
    strtod(str, &endptr);                                                       // parse double
    return *endptr == '\0';                                                     // entire string consumed
}

// checks if a string is a boolean literal
static bool is_bool(const char* str) {
    return (strcmp(str, "true") == 0 || strcmp(str, "false") == 0);             // check true/false
}

// parses a csv field into a typed vm value; VM-agnostic so it can run on a worker
static Value csv_parse_value(const char* str) {
    if (!str) return MAKE_NONE();                                               // null string
    if (is_numeric(str)) {                                                      // numeric
        return MAKE_NUMBER(atof(str));                                          // parse as number
    }
    if (is_bool(str)) {                                                         // boolean
        return MAKE_BOOL(strcmp(str, "true") == 0);                             // parse as bool
    }
    return MAKE_STRING(string_create(str, strlen(str)));                        // fresh refcounted string
}

// extracts a single csv field with quoted field support (rfc 4180)
static char* csv_parse_field(CsvParser* p) {
    StringBuilder sb;                                                           // string builder
    sb_init(&sb, 64);                                                           // init builder
    
    if (!p->data) {                                                             // no data
        sb_free(&sb);                                                           // free builder
        return strdup("");                                                      // return empty string
    }
    
    char c = csv_peek(p);                                                       // peek char
    if (c == '"') {                                                             // quoted field
        csv_advance(p);                                                         // skip opening quote
        while (csv_has_next(p)) {                                               // parse quoted content
            c = csv_advance(p);                                                 // read char
            if (c == '"') {                                                     // possible end of quote
                if (csv_peek(p) == '"') {                                       // escaped quote
                    sb_append_char(&sb, '"');                                   // append quote
                    csv_advance(p);                                             // skip second quote
                } else {
                    break;                                                      // end of quoted field
                }
            } else {
                sb_append_char(&sb, c);                                         // append char
            }
        }
        // after closing quote, only skip comma if present
        if (csv_peek(p) == ',') csv_advance(p);                                 // skip comma
    } else {                                                                    // unquoted field
        while (csv_has_next(p)) {                                               // parse field
            c = csv_peek(p);                                                    // peek char
            if (c == ',' || c == '\n' || c == '\r') {                           // comma or line ending
                break;                                                          // end of field
            }
            sb_append_char(&sb, csv_advance(p));                                // append char
        }
        // only skip comma if present, leave line endings for outer loop
        if (csv_peek(p) == ',') csv_advance(p);                                 // skip comma
    }
    
    char* result = sb.buffer ? strdup(sb.buffer) : strdup("");                  // copy result
    sb_free(&sb);                                                               // free builder
    return result;                                                              // return field
}

// depth of table nesting csv.encode is allowed to copy when snapshotting
// an input table for a worker; anything deeper is never read by the encoder
#define CSV_SNAPSHOT_MAX_DEPTH 1

// create a leaf future ready to be resolved by a background worker
static FutureObject* csv_make_leaf_future(void) {
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

// packed arguments for the csv decode worker
typedef struct {
    char* input;                             // owned copy of the csv text
    int input_len;                           // length in bytes
} CsvDecodeArgs;

// packed arguments for the csv encode worker
typedef struct {
    Table* snapshot;                         // worker-owned snapshot of the input table
} CsvEncodeArgs;

// task descriptor handed to a background worker
typedef struct {
    VM* vm;                                  // vm pointer for completion push
    FutureObject* fut;                       // future to resolve with the task result
    Value (*fn)(void*);                      // kernel executed on the worker
    void (*free_fn)(void*);                  // releases the packed argument struct
    void* arg;                               // packed argument struct
} CsvAsyncTask;

// release a CsvDecodeArgs and its owned input buffer
static void csv_decode_args_free(void* p) {
    CsvDecodeArgs* a = (CsvDecodeArgs*)p;    // unpack argument struct
    free(a->input);                          // release the copied csv text
    free(a);                                 // release struct itself
}

// release a CsvEncodeArgs and its owned snapshot table
static void csv_encode_args_free(void* p) {
    CsvEncodeArgs* a = (CsvEncodeArgs*)p;    // unpack argument struct
    if (a->snapshot) value_decref(MAKE_TABLE(a->snapshot));  // destroy the snapshot and everything it holds
    free(a);                                 // release struct itself
}

// release a CsvAsyncTask and its packed argument struct
static void csv_task_destroy(void* p) {
    CsvAsyncTask* t = (CsvAsyncTask*)p;      // unpack task descriptor
    if (t->free_fn) t->free_fn(t->arg);      // release argument struct
    free(t);                                 // free descriptor itself
}

// worker thread entry: runs the kernel and posts the result
static void* csv_thread(void* p) {
    CsvAsyncTask* t = (CsvAsyncTask*)p;      // unpack task descriptor
    Value result = t->fn(t->arg);            // run the kernel off the event loop
    vm_push_completion(t->vm, t->fut, result);  // hand off to scheduler
    csv_task_destroy(t);                     // release descriptor and args
    return NULL;                             // thread exit
}

// spawn a detached worker thread running the given task
static void csv_spawn_worker(VM* vm, FutureObject* fut, void* (*fn)(void*),
                             void* arg, void (*arg_free)(void*)) {
    APEX_MUTEX_LOCK(&vm->completion_mutex);  // reserve a worker slot
    vm->pending_workers++;                   // count pending worker
    APEX_MUTEX_UNLOCK(&vm->completion_mutex);// release lock

    bool ok = false;                         // spawn success flag
#ifdef _WIN32
    uintptr_t h = _beginthreadex(NULL, 0,               // windows thread
                                 (unsigned __stdcall (*)(void*))fn,
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
static bool csv_run_async_or_sync(VM* vm, Value (*fn)(void*),
                                  void (*free_fn)(void*), void* arg,
                                  Value* result) {
    if (vm->builtin_async) {          // inside a coroutine: never block the loop
        FutureObject* fut = csv_make_leaf_future();  // fresh pending future
        value_incref(MAKE_FUTURE(fut));      // worker holds one reference

        CsvAsyncTask* t = (CsvAsyncTask*)malloc(sizeof(CsvAsyncTask));  // pack task descriptor
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

        csv_spawn_worker(vm, fut, csv_thread, t, csv_task_destroy);  // offload
        *result = MAKE_FUTURE(fut);          // return pending future
    } else {                                 // top level: nothing else is runnable
        *result = fn(arg);                   // run the kernel inline
        if (free_fn) free_fn(arg);           // release argument struct
    }
    return true;                             // builtin handled
}

// forward declaration for the recursive snapshot helper; must precede any
// caller because csv_snapshot_value recurses into it
static Table* csv_snapshot_table_recursive_impl(Table* src, int remaining_levels);

// copy a value for the worker's private view of the input table; interned
// strings are shared since they are immortal and never mutate their refcount,
// non-interned strings are duplicated so the worker never races the main
// thread on their refcounts, and tables are copied up to the depth that
// csv.encode actually reads
static Value csv_snapshot_value(Value v, int remaining_levels) {
    if (IS_STRING(v)) {                                          // string: interned shared, others copied
        StringObject* s = AS_STRING(v);
        if (s->header.ref_count == INT_MAX) return v;            // interned: immortal, no refcount mutation
        return MAKE_STRING(string_create(s->chars, s->length));  // fresh copy, worker-owned
    }
    if (IS_TABLE(v)) {                                           // table: recurse or replace with none
        if (remaining_levels <= 0) return MAKE_NONE();           // beyond what csv.encode ever reads
        return MAKE_TABLE(csv_snapshot_table_recursive_impl(AS_TABLE(v), remaining_levels - 1));
    }
    return v;                                                    // number, bool, none: no refcount handling
}

// duplicate a table's structure (buckets, chains, array part) for a worker,
// deep-copying strings and nested tables up to the given remaining depth
static Table* csv_snapshot_table_recursive_impl(Table* src, int remaining_levels) {
    if (!src) return NULL;                                       // guard against null
    Table* dst = table_create(src->capacity);                    // fresh table, same bucket count

    if (src->array_part != NULL) {                               // copy occupied array slots
        for (int i = 0; i < src->array_count; i++) {
            Value v = src->array_part[i];
            if (!IS_NONE(v)) {
                Value cv = csv_snapshot_value(v, remaining_levels);
                table_set_int(dst, i, cv);                       // table_set_int increfs the value
                value_decref(cv);                                // release our temporary reference
            }
        }
    }

    if (src->entries != NULL) {                                  // copy hash bucket chains
        for (int i = 0; i < src->capacity; i++) {
            TableEntry* e = src->entries[i];
            while (e) {
                Value ck = csv_snapshot_value(e->key, remaining_levels);
                Value cv = csv_snapshot_value(e->value, remaining_levels);
                table_set(dst, ck, cv);                          // table_set increfs key and value
                value_decref(ck);                                // release our temporary references
                value_decref(cv);
                e = e->next;
            }
        }
    }
    return dst;                                                  // return worker-owned copy
}

// build a worker-owned snapshot of an input table for csv.encode
static Table* csv_snapshot_table(Table* data) {
    return csv_snapshot_table_recursive_impl(data, CSV_SNAPSHOT_MAX_DEPTH);
}

// parse a csv string into a 1-indexed table of row tables; runs on a worker
static Value csv_decode_impl(const char* data, int len) {
    if (len <= 0) {                                                          // empty input
        return MAKE_NONE();                                                  // return none
    }

    CsvParser parser = { .data = data, .pos = 0, .len = len };               // init parser
    Table* table_list_table = table_create(8);                               // result table
    Value table_list = MAKE_TABLE(table_list_table);                         // box table
    char** headers = NULL;                                                   // header array
    int col_count = 0;                                                       // column count

    if (parser.len > 0) {                                                    // parse header row
        int temp_pos = parser.pos;                                           // save position
        int count = 0;                                                       // column count
        while (parser.pos < parser.len && parser.data[parser.pos] != '\n' && parser.data[parser.pos] != '\r') {
            if (parser.data[parser.pos] == ',') count++;                     // count commas
            parser.pos++;                                                    // advance
        }
        col_count = count + 1;                                               // number of columns
        parser.pos = temp_pos;                                               // restore position

        if (col_count > 0) {                                                 // valid columns
            headers = (char**)malloc(sizeof(char*) * col_count);             // allocate headers
            if (headers) {                                                   // allocation succeeded
                for (int i = 0; i < col_count; i++) headers[i] = csv_parse_field(&parser);  // parse each
            }
        }
    }

    int row_index = 1;                                                       // row counter
    while (parser.pos < parser.len) {                                        // parse data rows
        if (parser.data[parser.pos] == '\n' || parser.data[parser.pos] == '\r') {  // skip blank lines
            if (parser.data[parser.pos] == '\r') parser.pos++;               // skip \r
            if (parser.pos < parser.len && parser.data[parser.pos] == '\n') parser.pos++;  // skip \n
            continue;                                                        // continue
        }

        Table* row_table = table_create(8);                                  // row table
        Value row_table_val = MAKE_TABLE(row_table);                         // box row
        if (headers && col_count > 0) {                                      // header mode
            for (int i = 0; i < col_count; i++) {                            // parse columns
                if (!csv_has_next(&parser)) break;                           // end of row
                char* field = csv_parse_field(&parser);                      // parse field
                Value val = csv_parse_value(field);                          // parse value
                if (headers[i]) {                                            // valid header
                    Value k = MAKE_STRING(string_create(headers[i], strlen(headers[i])));  // fresh key
                    table_set(row_table, k, val);                            // store value
                    value_decref(k);                                         // release key
                }
                value_decref(val);                                           // release value
                free(field);                                                 // free field
            }
        }

        Value k_idx = MAKE_NUMBER((double)row_index++);                      // row index key
        table_set(table_list_table, k_idx, row_table_val);                   // store row
        value_decref(k_idx);                                                 // release key
        value_decref(row_table_val);                                         // release row
    }

    if (headers) {                                                           // free headers
        for (int i = 0; i < col_count; i++) free(headers[i]);                // free each
        free(headers);                                                       // free array
    }
    return table_list;                                                       // return table
}

// serialize a table of row tables into csv text; runs on a worker
static Value csv_encode_impl(Table* data) {
    int row_count = table_size(data);                                        // number of rows
    if (row_count == 0) {                                                    // empty data
        return MAKE_STRING(string_create("", 0));                            // empty string
    }

    Value first_row_val;                                                     // first row value
    bool found_first = false;                                                // found flag

    if (data->array_count > 0 && IS_TABLE(data->array_part[0])) {            // array part has rows
        first_row_val = data->array_part[0];                                 // get first row (0-based)
        value_incref(first_row_val);                                         // increment ref for ownership
        found_first = true;                                                  // found
    }

    if (!found_first) {                                                      // not found yet
        Value k_num = MAKE_NUMBER(1.0);                                      // numeric key
        if (table_get(data, k_num, &first_row_val) && IS_TABLE(first_row_val)) {  // found
            value_incref(first_row_val);                                     // increment ref for ownership
            found_first = true;                                              // mark found
        }
    }

    if (!found_first) {                                                      // not found yet
        StringObject* tmp = string_create("1", 1);                           // temporary string key
        Value k_str = MAKE_STRING(tmp);
        if (table_get(data, k_str, &first_row_val) && IS_TABLE(first_row_val)) {  // found
            value_incref(first_row_val);                                     // increment ref for ownership
            found_first = true;                                              // mark found
        }
        value_decref(k_str);                                                 // release temporary key
    }

    if (!found_first) {                                                      // no first row
        return MAKE_NONE();                                                  // return none
    }

    int header_count = 0;                                                    // header count
    Value* headers = table_keys(AS_TABLE(first_row_val), &header_count);     // get headers from first row

    value_decref(first_row_val);                                             // header keys are already owned by table_keys

    StringBuilder sb;                                                        // string builder
    sb_init(&sb, 256);                                                       // init builder

    if (headers && header_count > 0) {                                       // write headers
        for (int i = 0; i < header_count; i++) {                             // iterate headers
            if (i > 0) sb_append_char(&sb, ',');                             // comma delimiter
            const char* h = "";                                              // default header
            char h_buf[64];                                                  // buffer for number
            if (IS_STRING(headers[i])) h = AS_STRING(headers[i])->chars;     // string header
            else if (IS_NUMBER(headers[i])) {                                // number header
                snprintf(h_buf, sizeof(h_buf), "%g", AS_NUMBER(headers[i])); // format
                h = h_buf;                                                   // use buffer
            }
            bool needs_quote = strchr(h, ',') || strchr(h, '"') || strchr(h, '\n') || strchr(h, '\r');  // needs quoting
            if (needs_quote) {                                               // quote field
                sb_append_char(&sb, '"');                                    // opening quote
                for (const char* p = h; *p; p++) {                           // escape quotes
                    if (*p == '"') sb_append_char(&sb, '"');                 // double quote
                    sb_append_char(&sb, *p);                                 // append char
                }
                sb_append_char(&sb, '"');                                    // closing quote
            } else {
                sb_append(&sb, h, strlen(h));                                // append header
            }
        }
        sb_append(&sb, "\r\n", 2);                                           // crlf line ending (rfc 4180)
    }

    int written = 0;                                                         // rows written
    int total_rows = data->array_count > 0 ? data->array_count : row_count;  // rows to try

    for (int r = 1; r <= total_rows + 10; r++) {                             // iterate possible rows
        Value row_val;                                                       // row value
        bool got_row = false;                                                // found flag

        if (r - 1 < data->array_count && IS_TABLE(data->array_part[r - 1])) {  // in array bounds
            row_val = data->array_part[r - 1];                               // get from array
            value_incref(row_val);                                           // increment ref
            got_row = true;                                                  // found
        }

        if (!got_row) {                                                      // not found yet
            Value k_row = MAKE_NUMBER((double)r);                            // numeric key
            if (table_get(data, k_row, &row_val) && IS_TABLE(row_val)) {     // found
                value_incref(row_val);                                       // increment ref for ownership
                got_row = true;                                              // mark found
            }
        }

        if (!got_row) {                                                      // not found yet
            char rbuf[32];                                                   // buffer
            int rlen = snprintf(rbuf, sizeof(rbuf), "%d", r);                // format
            StringObject* tmp = string_create(rbuf, rlen);                   // temporary string key
            Value k_str = MAKE_STRING(tmp);
            if (table_get(data, k_str, &row_val) && IS_TABLE(row_val)) {     // found
                value_incref(row_val);                                       // increment ref for ownership
                got_row = true;                                              // mark found
            }
            value_decref(k_str);                                             // release temporary key
        }

        if (!got_row) break;                                                 // no more rows

        if (written > 0) sb_append(&sb, "\r\n", 2);                          // crlf line ending between rows

        for (int i = 0; i < header_count; i++) {                             // iterate columns
            if (i > 0) sb_append_char(&sb, ',');                             // comma delimiter
            Value cell_val;                                                  // cell value
            char buf[64];                                                    // buffer for number
            const char* str_val = "";                                        // string value

            if (table_get(AS_TABLE(row_val), headers[i], &cell_val)) {       // get cell
                if (IS_NUMBER(cell_val)) {                                   // number
                    snprintf(buf, sizeof(buf), "%g", AS_NUMBER(cell_val));   // format
                    str_val = buf;                                           // use buffer
                } else if (IS_BOOL(cell_val)) {                              // boolean
                    str_val = AS_BOOL(cell_val) ? "true" : "false";          // bool string
                } else if (IS_STRING(cell_val)) {                            // string
                    str_val = AS_STRING(cell_val)->chars;                    // use string
                }
                value_decref(cell_val);                                      // release cell
            }

            bool needs_quote = strchr(str_val, ',') || strchr(str_val, '"') || strchr(str_val, '\n') || strchr(str_val, '\r');  // needs quoting
            if (needs_quote) {                                               // quote field
                sb_append_char(&sb, '"');                                    // opening quote
                for (const char* p = str_val; *p; p++) {                     // escape quotes
                    if (*p == '"') sb_append_char(&sb, '"');                 // double quote
                    sb_append_char(&sb, *p);                                 // append char
                }
                sb_append_char(&sb, '"');                                    // closing quote
            } else {
                sb_append(&sb, str_val, strlen(str_val));                    // append value
            }
        }

        value_decref(row_val);                                               // release row
        written++;                                                           // count written
    }

    if (headers) {                                                           // free headers
        for (int i = 0; i < header_count; i++) value_decref(headers[i]);     // release each header
        free(headers);                                                       // free header array
    }

    const char* buf = sb.buffer ? sb.buffer : "";                            // guard against OOM
    Value out = MAKE_STRING(string_create(buf, sb.length));                  // fresh refcounted string
    sb_free(&sb);                                                            // free builder
    return out;                                                              // return encoded csv
}

// worker entry point for csv.decode
static Value csv_decode_kernel(void* p) {
    CsvDecodeArgs* a = (CsvDecodeArgs*)p;                                    // unpack argument struct
    return csv_decode_impl(a->input, a->input_len);                          // parse the copied csv text
}

// worker entry point for csv.encode
static Value csv_encode_kernel(void* p) {
    CsvEncodeArgs* a = (CsvEncodeArgs*)p;                                    // unpack argument struct
    return csv_encode_impl(a->snapshot);                                     // encode the worker-owned snapshot
}

// main dispatcher for csv module built-in functions
bool csv_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "csv.decode") == 0) {                                       // parse csv
        if (arg_count != 1 || !IS_STRING(args[0])) {                             // validate string
            *result = MAKE_NONE();                                               // invalid
            return true;                                                         // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                            // input string
        if (input_str->length <= 0) {                                            // empty input
            *result = MAKE_NONE();                                               // return none
            return true;                                                         // builtin handled
        }

        if (vm->builtin_async) {                                          // inside coroutine: copy and offload
            CsvDecodeArgs* a = (CsvDecodeArgs*)malloc(sizeof(CsvDecodeArgs));    // pack argument struct
            if (!a) { *result = MAKE_NONE(); return true; }                      // allocation failed
            a->input = (char*)malloc(input_str->length + 1);                     // copy csv text
            if (!a->input) {                                                     // allocation failed
                free(a);                                                         // release struct
                *result = MAKE_NONE();                                           // return none
                return true;                                                     // builtin handled
            }
            memcpy(a->input, input_str->chars, input_str->length);               // capture input bytes
            a->input[input_str->length] = '\0';                                  // NUL terminate for safety
            a->input_len = input_str->length;                                    // store length
            return csv_run_async_or_sync(vm, csv_decode_kernel,
                                         csv_decode_args_free, a, result);
        }

        *result = csv_decode_impl(input_str->chars, input_str->length);          // top level: parse directly
        return true;                                                             // builtin handled
    }

    if (strcmp(name, "csv.encode") == 0) {                                       // csv serialize
        if (arg_count != 1 || !IS_TABLE(args[0])) {                              // validate table
            *result = MAKE_NONE();                                               // invalid
            return true;                                                         // builtin handled
        }
        Table* data = AS_TABLE(args[0]);                                         // data table

        if (vm->builtin_async) {                                          // inside coroutine: snapshot and offload
            Table* snap = csv_snapshot_table(data);                              // worker-owned structural copy
            if (!snap) { *result = MAKE_NONE(); return true; }                   // snapshot failed
            CsvEncodeArgs* a = (CsvEncodeArgs*)malloc(sizeof(CsvEncodeArgs));    // pack argument struct
            if (!a) {                                                            // allocation failed
                value_decref(MAKE_TABLE(snap));                                  // release snapshot
                *result = MAKE_NONE();                                           // return none
                return true;                                                     // builtin handled
            }
            a->snapshot = snap;                                                  // store worker-owned snapshot
            return csv_run_async_or_sync(vm, csv_encode_kernel,
                                         csv_encode_args_free, a, result);
        }

        *result = csv_encode_impl(data);                                         // top level: encode directly
        return true;                                                             // builtin handled
    }

    return false;      // not a recognized builtin
}
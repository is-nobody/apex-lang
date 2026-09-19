// source/libraries/xml_module.c
// Implementation of XML Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "xml_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

// xml parser state
typedef struct {
    const char* p;                                                              // current parse position
} XmlParser;

// skips whitespace in xml
static void xml_skip_ws(XmlParser* xp) {
    while (*xp->p && isspace((unsigned char)*xp->p)) xp->p++;                   // skip whitespace
}

// parses xml attributes into a table with @ prefix; VM-agnostic so it can run on a worker
static void xml_parse_attrs(XmlParser* xp, Table* t) {
    xml_skip_ws(xp);                                                            // skip whitespace
    while (*xp->p && *xp->p != '>' && *xp->p != '/') {                          // parse attributes
        char key[128] = {0};                                                    // attribute key buffer
        int ki = 0;                                                             // key index
        while (*xp->p && (isalnum((unsigned char)*xp->p) || *xp->p == '_' || *xp->p == '-') && ki < 127) {
            key[ki++] = *xp->p++;                                               // copy key char
        }
        key[ki] = '\0';                                                         // null terminate
        if (!*key) break;                                                       // empty key
        
        xml_skip_ws(xp);                                                        // skip whitespace
        if (*xp->p != '=') break;                                               // missing equals
        xp->p++;                                                                // skip equals
        xml_skip_ws(xp);                                                        // skip whitespace
        if (*xp->p != '"') break;                                               // missing quote
        xp->p++;                                                                // skip opening quote
        
        StringBuilder val_sb;                                                   // value builder
        sb_init(&val_sb, 64);                                                   // init builder
        while (*xp->p && *xp->p != '"') {                                       // parse value
            sb_append_char(&val_sb, *xp->p);                                    // append char
            xp->p++;                                                            // advance
        }
        if (*xp->p == '"') xp->p++;                                             // skip closing quote
        
        char attr_key_buf[150];                                                 // attribute key buffer
        snprintf(attr_key_buf, sizeof(attr_key_buf), "@%s", key);               // prefix with @
        Value k = MAKE_STRING(string_create(attr_key_buf, strlen(attr_key_buf)));        // fresh key
        Value v = MAKE_STRING(string_create(val_sb.buffer, val_sb.length));              // fresh value
        table_set(t, k, v);                                                     // store attribute
        value_decref(k);                                                        // release key
        value_decref(v);                                                        // release value
        sb_free(&val_sb);                                                       // free builder
        
        xml_skip_ws(xp);                                                        // skip whitespace
    }
}

// parses a single xml element recursively; VM-agnostic so it can run on a worker
static Value xml_parse_element(XmlParser* xp) {
    xml_skip_ws(xp);                                                            // skip whitespace
    if (*xp->p != '<') return MAKE_NONE();                                      // not an element
    xp->p++;                                                                    // skip opening '<'
    
    char tag[128] = {0};                                                        // tag name buffer
    int ti = 0;                                                                 // tag index
    while (*xp->p && (isalnum((unsigned char)*xp->p) || *xp->p == '_' || *xp->p == '-') && ti < 127) {
        tag[ti++] = *xp->p++;                                                   // copy tag char
    }
    tag[ti] = '\0';                                                             // null terminate
    if (!*tag) return MAKE_NONE();                                              // empty tag
    
    Table* elem_table = table_create(8);                                        // create element table
    Value elem = MAKE_TABLE(elem_table);                                        // box as value
    
    Value k_tag = MAKE_STRING(string_create("__tag", 5));                       // tag key
    Value v_tag = MAKE_STRING(string_create(tag, strlen(tag)));                 // tag value
    table_set(elem_table, k_tag, v_tag);                                        // store tag
    value_decref(k_tag);                                                        // release key
    value_decref(v_tag);                                                        // release value
    
    xml_parse_attrs(xp, elem_table);                                            // parse attributes
    
    xml_skip_ws(xp);                                                            // skip whitespace
    bool self_closing = false;                                                  // self-closing flag
    if (*xp->p == '/') {                                                        // self-closing
        self_closing = true;                                                    // set flag
        xp->p++;                                                                // skip '/'
    }
    if (*xp->p == '>') {                                                        // opening tag end
        xp->p++;                                                                // skip '>'
        if (self_closing) return elem;                                          // self-closing element
        
        int index = 1;                                                          // child index
        
        while (*xp->p) {                                                        // parse content
            if (*xp->p == '<') {                                                // start of child or closing
                if (*(xp->p + 1) == '/') {                                      // closing tag
                    xp->p += 2;                                                 // skip '</'
                    char close_tag[128] = {0};                                  // closing tag buffer
                    int ci = 0;                                                 // index
                    while (*xp->p && *xp->p != '>' && ci < 127) {               // parse closing tag
                        close_tag[ci++] = *xp->p++;                             // copy char
                    }
                    close_tag[ci] = '\0';                                       // null terminate
                    if (strcmp(tag, close_tag) != 0) {                          // tag mismatch
                        value_decref(elem);                                     // release element
                        return MAKE_NONE();                                     // error
                    }
                    if (*xp->p == '>') xp->p++;                                 // skip '>'
                    return elem;                                                // done
                } else {                                                        // child element
                    Value child = xml_parse_element(xp);                        // parse child
                    if (IS_NONE(child)) {                                       // parse failed
                        value_decref(elem);                                     // release element
                        return MAKE_NONE();                                     // error
                    }
                    if (IS_TABLE(child)) {                                      // valid child
                        Value k = MAKE_NUMBER((double)index++);                 // create index key
                        table_set(elem_table, k, child);                        // store child
                        value_decref(k);                                        // release key
                    }
                    value_decref(child);                                        // release child
                }
            } else {                                                            // text content
                StringBuilder text_sb;                                          // text builder
                sb_init(&text_sb, 64);                                          // init builder
                while (*xp->p && *xp->p != '<') {                               // collect text
                    sb_append_char(&text_sb, *xp->p);                           // append char
                    xp->p++;                                                    // advance
                }
                if (text_sb.length > 0) {                                       // non-empty text
                    Value k_text = MAKE_STRING(string_create("#text", 5));      // text key
                    Value v_text = MAKE_STRING(string_create(text_sb.buffer, text_sb.length));  // text value
                    table_set(elem_table, k_text, v_text);                      // store text
                    value_decref(k_text);                                       // release key
                    value_decref(v_text);                                       // release value
                }
                sb_free(&text_sb);                                              // free builder
            }
            xml_skip_ws(xp);                                                    // skip whitespace
        }
        
        value_decref(elem);                                                     // release element
        return MAKE_NONE();                                                     // error
    }
    value_decref(elem);                                                         // release element
    return MAKE_NONE();                                                         // error
}

// recursively writes an xml node from a vm table; VM-agnostic so it can run on a worker
static void xml_encode_node(Value v, int depth, StringBuilder* sb) {
    if (!IS_TABLE(v)) return;                                                  // not a table
    
    Table* table = AS_TABLE(v);                                                // unwrap table
    
    Value k_tag = MAKE_STRING(string_create("__tag", 5));                      // tag key
    Value tag_val;                                                             // tag value
    if (!table_get(table, k_tag, &tag_val) || !IS_STRING(tag_val)) {           // missing tag
        value_decref(k_tag);                                                   // release key
        return;
    }
    value_decref(k_tag);                                                       // release key
    
    for (int i = 0; i < depth; i++) sb_append(sb, "  ", 2);                    // indent
    sb_append(sb, "<", 1);                                                     // opening tag
    StringObject* tag_str = AS_STRING(tag_val);                                // get tag string
    sb_append(sb, tag_str->chars, tag_str->length);                            // append tag name
    value_decref(tag_val);                                                     // release tag value
    
    for (int i = 0; i < table->capacity; i++) {                                // iterate hash part
        TableEntry* e = table->entries[i];                                     // bucket head
        while (e) {                                                            // traverse chain
            if (IS_STRING(e->key)) {                                           // string key
                StringObject* key_str = AS_STRING(e->key);                     // key string
                if (key_str->chars[0] == '@' && IS_STRING(e->value)) {         // attribute
                    sb_append(sb, " ", 1);                                     // space
                    sb_append(sb, key_str->chars + 1, key_str->length - 1);    // attr name
                    sb_append(sb, "=\"", 2);                                   // equals and quote
                    StringObject* val_str = AS_STRING(e->value);               // value string
                    sb_append(sb, val_str->chars, val_str->length);            // append value
                    sb_append(sb, "\"", 1);                                    // closing quote
                }
            }
            e = e->next;                                                       // advance
        }
    }
    
    Value k_text = MAKE_STRING(string_create("#text", 5));                     // text key
    Value text_val;                                                            // text value
    bool has_text = table_get(table, k_text, &text_val);                       // check for text node
    value_decref(k_text);                                                      // release text key
    
    bool has_children = false;                                                 // child elements flag
    
    for (int i = 1; i <= table->array_count; i++) {                            // scan array part
        Value k = MAKE_NUMBER((double)i);                                      // create index key
        Value child_val;                                                       // child value placeholder
        if (table_get(table, k, &child_val)) {                                 // child exists at this index
            has_children = true;                                               // mark has children
            value_decref(child_val);                                           // release child value
            value_decref(k);                                                   // release key
            break;                                                             // found at least one, stop scanning
        }
        value_decref(k);                                                       // release key
    }
    
    if (!has_children) {                                                       // still not found in array
        for (int i = 0; i < table->capacity; i++) {                            // iterate hash buckets
            TableEntry* e = table->entries[i];                                 // bucket head
            while (e) {                                                        // traverse chain
                if (IS_STRING(e->key)) {                                       // string key
                    StringObject* key_str = AS_STRING(e->key);                 // key string
                    if (key_str->chars[0] != '@' && 
                        key_str->chars[0] != '_' &&
                        key_str->chars[0] != '#') {                            // likely a child key
                        bool is_numeric_key = true;                            // assume numeric
                        for (int j = 0; j < key_str->length; j++) {            // check each char
                            if (!isdigit((unsigned char)key_str->chars[j])) {  // not a digit
                                is_numeric_key = false;                        // not numeric
                                break;
                            }
                        }
                        if (is_numeric_key && key_str->length > 0) {           // numeric string key found
                            has_children = true;                               // mark has children
                            break;                                             // stop scanning
                        }
                    }
                }
                e = e->next;                                                   // advance
            }
            if (has_children) break;                                           // break outer loop
        }
    }
    
    if (has_children || has_text) {                                            // element has content
        sb_append(sb, ">", 1);                                                 // close opening tag
        if (has_text) {                                                        // has text content
            if (IS_STRING(text_val)) {                                         // valid text
                StringObject* text_str = AS_STRING(text_val);                  // text string
                sb_append(sb, text_str->chars, text_str->length);              // append text
            }
            value_decref(text_val);                                            // release text value
        }
        if (has_children) sb_append(sb, "\n", 1);                              // newline before children
        
        for (int i = 1; i <= table->array_count; i++) {                        // iterate array children
            Value k = MAKE_NUMBER((double)i);                                  // child index key
            Value child_val;                                                   // child value
            if (table_get(table, k, &child_val)) {                             // child found
                value_decref(k);                                               // release key
                xml_encode_node(child_val, depth + 1, sb);                     // recursively write child
                value_decref(child_val);                                       // release child value
            } else {
                value_decref(k);                                               // release key
                break;                                                         // no more children
            }
        }
        
        for (int i = 0; i < table->capacity; i++) {                            // iterate hash buckets
            TableEntry* e = table->entries[i];                                 // bucket head
            while (e) {                                                        // traverse chain
                if (IS_STRING(e->key)) {                                       // string key
                    StringObject* key_str = AS_STRING(e->key);                 // key string
                    if (key_str->chars[0] != '@' && 
                        key_str->chars[0] != '_' &&
                        key_str->chars[0] != '#') {                            // likely a child key
                        bool is_numeric_key = true;                            // assume numeric
                        for (int j = 0; j < key_str->length; j++) {            // check each char
                            if (!isdigit((unsigned char)key_str->chars[j])) {  // not a digit
                                is_numeric_key = false;                        // not numeric
                                break;
                            }
                        }
                        if (is_numeric_key && key_str->length > 0) {           // numeric string key
                            xml_encode_node(e->value, depth + 1, sb);          // recursively write child
                        }
                    }
                }
                e = e->next;                                                   // advance
            }
        }
        
        if (has_children) {                                                    // indent closing tag if has children
            for (int i = 0; i < depth; i++) sb_append(sb, "  ", 2);            // indent
        }
        sb_append(sb, "</", 2);                                                // closing tag start
        
        Value k_close = MAKE_STRING(string_create("__tag", 5));                // tag key
        if (table_get(table, k_close, &tag_val)) {                             // get tag
            value_decref(k_close);                                             // release key
            if (IS_STRING(tag_val)) {                                          // valid tag string
                StringObject* close_str = AS_STRING(tag_val);                  // tag string
                sb_append(sb, close_str->chars, close_str->length);            // append tag name
            }
            value_decref(tag_val);                                             // release tag value
        } else {
            value_decref(k_close);                                             // release key
        }
        sb_append(sb, ">\n", 2);                                               // closing tag end with newline
    } else {
        sb_append(sb, "/>\n", 3);                                              // self-closing tag
    }
}

// parse xml text into a vm value; runs on the caller's thread or a worker
static Value xml_decode_impl(const char* str) {
    XmlParser xp;                                                              // xml parser
    xp.p = str;                                                                // set pointer
    Value root = xml_parse_element(&xp);                                       // parse root
    if (IS_TABLE(root)) return root;                                           // valid root
    value_decref(root);                                                        // release invalid
    return MAKE_NONE();                                                        // return none
}

// serialize a vm value to xml text; runs on the caller's thread or a worker
static Value xml_encode_impl(Value value) {
    StringBuilder sb;                                                          // string builder
    sb_init(&sb, 256);                                                         // init builder
    xml_encode_node(value, 0, &sb);                                            // write xml
    if (sb.length == 0) {                                                      // empty output
        sb_free(&sb);                                                          // free builder
        return MAKE_NONE();                                                    // return none
    }
    Value out = MAKE_STRING(string_create(sb.buffer, sb.length));              // fresh refcounted string
    sb_free(&sb);                                                              // free builder
    return out;                                                                // return encoded xml
}

// forward declaration for the recursive snapshot helper; must precede any
// caller because xml_snapshot_value recurses into it
static Table* xml_snapshot_table(Table* src);

// copy a value for the worker's private view: interned strings are shared
// because their refcount is immortal and never written, non-interned strings
// are duplicated so the worker never races the main thread on their
// refcounts, and tables are deep-copied because xml.encode recurses through
// arbitrarily nested structures
static Value xml_snapshot_value(Value v) {
    if (IS_STRING(v)) {                                          // string: interned shared, others copied
        StringObject* s = AS_STRING(v);
        if (s->header.ref_count == INT_MAX) return v;            // interned: immortal, no refcount mutation
        return MAKE_STRING(string_create(s->chars, s->length));  // fresh copy, worker-owned
    }
    if (IS_TABLE(v)) {                                           // table: recurse into a worker-owned copy
        return MAKE_TABLE(xml_snapshot_table(AS_TABLE(v)));
    }
    return v;                                                    // number, bool, none: no refcount handling
}

// duplicate a table's structure (buckets, chains, array part) for a worker,
// deep-copying every string and nested table reachable from it
static Table* xml_snapshot_table(Table* src) {
    if (!src) return NULL;                                       // guard against null
    Table* dst = table_create(src->capacity);                    // fresh table, same bucket count

    if (src->array_part != NULL) {                               // copy occupied array slots
        for (int i = 0; i < src->array_count; i++) {
            Value v = src->array_part[i];
            if (!IS_NONE(v)) {
                Value cv = xml_snapshot_value(v);
                table_set_int(dst, i, cv);                       // table_set_int increfs the value
                value_decref(cv);                                // release our temporary reference
            }
        }
    }

    if (src->entries != NULL) {                                  // copy hash bucket chains
        for (int i = 0; i < src->capacity; i++) {
            TableEntry* e = src->entries[i];
            while (e) {
                Value ck = xml_snapshot_value(e->key);
                Value cv = xml_snapshot_value(e->value);
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
static FutureObject* xml_make_leaf_future(void) {
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

// packed arguments for the xml decode worker
typedef struct {
    char* input;                             // owned copy of the xml text
    int input_len;                           // length in bytes
} XmlDecodeArgs;

// packed arguments for the xml encode worker
typedef struct {
    Value snapshot;                          // worker-owned snapshot of the input value
} XmlEncodeArgs;

// task descriptor handed to a background worker
typedef struct {
    VM* vm;                                  // vm pointer for completion push
    FutureObject* fut;                       // future to resolve with the task result
    Value (*fn)(void*);                      // kernel executed on the worker
    void (*free_fn)(void*);                  // releases the packed argument struct
    void* arg;                               // packed argument struct
} XmlAsyncTask;

// release a XmlDecodeArgs and its owned input buffer
static void xml_decode_args_free(void* p) {
    XmlDecodeArgs* a = (XmlDecodeArgs*)p;    // unpack argument struct
    free(a->input);                          // release the copied xml text
    free(a);                                 // release struct itself
}

// release a XmlEncodeArgs and its owned snapshot value
static void xml_encode_args_free(void* p) {
    XmlEncodeArgs* a = (XmlEncodeArgs*)p;    // unpack argument struct
    value_decref(a->snapshot);               // destroy the snapshot and everything it holds
    free(a);                                 // release struct itself
}

// release a XmlAsyncTask and its packed argument struct
static void xml_task_destroy(void* p) {
    XmlAsyncTask* t = (XmlAsyncTask*)p;      // unpack task descriptor
    if (t->free_fn) t->free_fn(t->arg);      // release argument struct
    free(t);                                 // free descriptor itself
}

// worker thread entry: runs the kernel and posts the result
static void* xml_thread(void* p) {
    XmlAsyncTask* t = (XmlAsyncTask*)p;      // unpack task descriptor
    Value result = t->fn(t->arg);            // run the kernel off the event loop
    vm_push_completion(t->vm, t->fut, result);  // hand off to scheduler
    xml_task_destroy(t);                     // release descriptor and args
    return NULL;                             // thread exit
}

// spawn a detached worker thread running the given task
static void xml_spawn_worker(VM* vm, FutureObject* fut, void* (*fn)(void*),
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
static bool xml_run_async_or_sync(VM* vm, Value (*fn)(void*),
                                  void (*free_fn)(void*), void* arg,
                                  Value* result) {
    if (vm->builtin_async) {          // inside a coroutine: never block the loop
        FutureObject* fut = xml_make_leaf_future();  // fresh pending future
        value_incref(MAKE_FUTURE(fut));      // worker holds one reference

        XmlAsyncTask* t = (XmlAsyncTask*)malloc(sizeof(XmlAsyncTask));  // pack task descriptor
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

        xml_spawn_worker(vm, fut, xml_thread, t, xml_task_destroy);  // offload
        *result = MAKE_FUTURE(fut);          // return pending future
    } else {                                 // top level: nothing else is runnable
        *result = fn(arg);                   // run the kernel inline
        if (free_fn) free_fn(arg);           // release argument struct
    }
    return true;                             // builtin handled
}

// worker entry point for xml.decode
static Value xml_decode_kernel(void* p) {
    XmlDecodeArgs* a = (XmlDecodeArgs*)p;    // unpack argument struct
    return xml_decode_impl(a->input);        // parse the copied xml text
}

// worker entry point for xml.encode
static Value xml_encode_kernel(void* p) {
    XmlEncodeArgs* a = (XmlEncodeArgs*)p;    // unpack argument struct
    return xml_encode_impl(a->snapshot);     // encode the worker-owned snapshot
}

// main dispatcher for xml module built-in functions
bool xml_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "xml.decode") == 0) {                                   // parse xml
        if (arg_count < 1 || !IS_STRING(args[0])) {                          // validate string
            *result = MAKE_NONE();                                           // invalid
            return true;                                                     // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                        // input string

        if (vm->builtin_async) {                                      // inside coroutine: copy and offload
            XmlDecodeArgs* a = (XmlDecodeArgs*)malloc(sizeof(XmlDecodeArgs));  // pack argument struct
            if (!a) { *result = MAKE_NONE(); return true; }                  // allocation failed
            a->input = (char*)malloc(input_str->length + 1);                 // copy xml text
            if (!a->input) {                                                 // allocation failed
                free(a);                                                     // release struct
                *result = MAKE_NONE();                                       // return none
                return true;                                                 // builtin handled
            }
            memcpy(a->input, input_str->chars, input_str->length);           // capture input bytes
            a->input[input_str->length] = '\0';                              // NUL terminate
            a->input_len = input_str->length;                                // store length
            return xml_run_async_or_sync(vm, xml_decode_kernel,
                                         xml_decode_args_free, a, result);
        }

        *result = xml_decode_impl(input_str->chars);                         // top level: parse directly
        return true;                                                         // builtin handled
    }

    if (strcmp(name, "xml.encode") == 0) {                                   // xml serialize
        if (arg_count < 1 || !IS_TABLE(args[0])) {                           // validate table
            *result = MAKE_NONE();                                           // invalid
            return true;                                                     // builtin handled
        }

        if (vm->builtin_async) {                                      // inside coroutine: snapshot and offload
            XmlEncodeArgs* a = (XmlEncodeArgs*)malloc(sizeof(XmlEncodeArgs));  // pack argument struct
            if (!a) { *result = MAKE_NONE(); return true; }                  // allocation failed
            a->snapshot = xml_snapshot_value(args[0]);                       // worker-owned structural copy
            return xml_run_async_or_sync(vm, xml_encode_kernel,
                                         xml_encode_args_free, a, result);
        }

        *result = xml_encode_impl(args[0]);                                  // top level: encode directly
        return true;                                                         // builtin handled
    }

    return false;                                                            // not a recognized builtin
}
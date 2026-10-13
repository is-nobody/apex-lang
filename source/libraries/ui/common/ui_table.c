// source/libraries/ui/common/ui_table.c
// Table helpers for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_table.h"
#include <stdlib.h>
#include <string.h>

// interns a c string into the vm's intern table, returning a tagged string value
Value ui_intern(VM* vm, const char* s) {
    return MAKE_STRING(string_intern(&vm->intern_table, s, (int)strlen(s)));
}

// reads a string field from a table, returns a heap copy or null on miss
char* table_get_str_dup(VM* vm, Table* t, const char* key) {
    Value k = ui_intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return NULL; }  // key absent
    value_decref(k);
    if (!IS_STRING(v)) { value_decref(v); return NULL; }         // wrong type
    StringObject* s = AS_STRING(v);
    char* out = (char*)malloc(s->length + 1);                    // allocate copy buffer
    memcpy(out, s->chars, s->length);                            // copy bytes
    out[s->length] = 0;                                          // null terminate
    value_decref(v);                                             // release the table's value
    return out;                                                  // caller owns the copy
}

// reads a number field from a table, returns default on miss or wrong type
double table_get_num(VM* vm, Table* t, const char* key, double def) {
    Value k = ui_intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return def; }   // key absent
    value_decref(k);
    double r = IS_NUMBER(v) ? AS_NUMBER(v) : def;                // use value only if numeric
    value_decref(v);
    return r;
}

// reads a boolean field from a table, returns default on miss or wrong type
bool table_get_bool(VM* vm, Table* t, const char* key, bool def) {
    Value k = ui_intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return def; }   // key absent
    value_decref(k);
    bool r = IS_BOOL(v) ? AS_BOOL(v) : def;                      // use value only if boolean
    value_decref(v);
    return r;
}

// reads a string field via an already-interned key
bool table_get_str_view(Table* t, Value key, StrView* out) {
    Value v;
    if (!table_get(t, key, &v)) return false;
    if (!IS_STRING(v)) { value_decref(v); return false; }
    StringObject* s = AS_STRING(v);
    out->ptr = s->chars;
    out->len = s->length;
    value_decref(v);                                 // table keeps its own ref: buffer stays alive
    return true;
}

// reads a number field via a cached key; returns default on miss or wrong type. no interning.
double table_get_num_key(Table* t, Value key, double def) {
    Value v;
    if (!table_get(t, key, &v)) return def;
    double r = IS_NUMBER(v) ? AS_NUMBER(v) : def;
    value_decref(v);
    return r;
}

// reads a boolean field via a cached key; returns default on miss or wrong type. no interning.
bool table_get_bool_key(Table* t, Value key, bool def) {
    Value v;
    if (!table_get(t, key, &v)) return def;
    bool r = IS_BOOL(v) ? AS_BOOL(v) : def;
    value_decref(v);
    return r;
}
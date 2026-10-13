// source/libraries/ui/common/ui_table.h
// Table helpers for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_TABLE_H
#define UI_TABLE_H

#include "ui_internal.h"

// interns a c string into the vm's intern table, returning a tagged string value
Value ui_intern(VM* vm, const char* s);

// reads a string field from a table, returns a heap copy or null on miss
char* table_get_str_dup(VM* vm, Table* t, const char* key);

// reads a number field from a table, returns default on miss or wrong type
double table_get_num(VM* vm, Table* t, const char* key, double def);

// reads a boolean field from a table, returns default on miss or wrong type
bool table_get_bool(VM* vm, Table* t, const char* key, bool def);

// reads a string field via an already-interned key
bool table_get_str_view(Table* t, Value key, StrView* out);

// reads a number field via a cached key; returns default on miss or wrong type. no interning.
double table_get_num_key(Table* t, Value key, double def);

// reads a boolean field via a cached key; returns default on miss or wrong type. no interning.
bool table_get_bool_key(Table* t, Value key, bool def);

#endif
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

// recursive json parser that builds vm values
static bool json_parse_value(VM* vm, const char** json_str, Value* out_value) {
    (void)vm;                                                                     // vm unused
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
        StringObject* interned = string_intern(&vm->intern_table, str_val, len);  // intern string
        *out_value = MAKE_STRING(interned);                                       // store string
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
                if (!json_parse_value(vm, json_str, &item)) {                     // parse element
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
                if (!json_parse_value(vm, json_str, &val)) {                      // parse value
                    free(key_str);                                                // free key
                    value_decref(*out_value);                                     // release table
                    return false;                                                 // parse failed
                }
                Value k = MAKE_STRING(string_intern(&vm->intern_table, key_str, key_len));  // intern key
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

// recursively encodes a vm value to json
static void json_encode_value(VM* vm, Value value, StringBuilder* sb) {
    (void)vm;                                                                     // vm unused
    
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
                json_encode_value(vm, t->array_part[i], sb);                      // encode element
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
                    json_encode_value(vm, t->array_part[i], sb);                  // encode value
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
                    json_encode_value(vm, entry->value, sb);                      // encode value
                    entry = entry->next;                                          // advance
                }
            }
            sb_append(sb, "}", 1);                                                // closing brace
        }
    } else {
        sb_append(sb, "null", 4);                                                 // null
    }
}

// main dispatcher for json module built-in functions
bool json_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "json.decode") == 0) {                                       // parse json
        if (arg_count < 1 || !IS_STRING(args[0])) {                               // validate string
            *result = MAKE_NONE();                                                // invalid
            return true;                                                          // builtin handled
        }
        const char* json_str = AS_STRING(args[0])->chars;                         // json string
        if (json_parse_value(vm, &json_str, result)) {                            // parse
            return true;                                                          // success
        }
        *result = MAKE_NONE();                                                    // parse failed
        return true;                                                              // builtin handled
    }
    
    if (strcmp(name, "json.encode") == 0) {                                       // json serialize
        if (arg_count < 1) {                                                      // need value
            *result = MAKE_NONE();                                                // invalid
            return true;                                                          // builtin handled
        }
        StringBuilder sb;                                                         // string builder
        sb_init(&sb, 256);                                                        // init builder
        json_encode_value(vm, args[0], &sb);                                      // encode value
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length));  // intern result
        sb_free(&sb);                                                             // free builder
        return true;                                                              // builtin handled
    }

    return false;                                                                 // not a recognized builtin
}
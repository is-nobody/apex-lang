#include "json_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// ========== String Builder ==========
typedef struct {
    char* buffer;
    int length;
    int capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb, int initial_capacity) {
    sb->capacity = initial_capacity > 16 ? initial_capacity : 16;
    sb->buffer = (char*)malloc(sb->capacity);
    sb->length = 0;
    sb->buffer[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str, int len) {
    if (sb->length + len + 1 > sb->capacity) {
        sb->capacity = (sb->length + len + 1) * 2;
        sb->buffer = (char*)realloc(sb->buffer, sb->capacity);
    }
    memcpy(sb->buffer + sb->length, str, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0';
}

static void sb_free(StringBuilder* sb) {
    free(sb->buffer);
}

// ========== Forward Declarations ==========
static bool parse_value(VM* vm, const char** json_str, Value* out_value);
static void encode_value(VM* vm, Value* value, StringBuilder* sb);
void value_decref(Value* v);
void value_incref(Value* v);
bool table_get(Table* table, const char* key, Value* out_value);
int table_size(Table* table);

// ========== Helper: Skip Whitespace ==========
static void skip_ws(const char** s) {
    while (**s && isspace((unsigned char)**s)) (*s)++;
}

// ========== Helper: Parse String ==========
static bool parse_string_raw(const char** s, char** out_str, int* out_len) {
    if (**s != '"') return false;
    (*s)++; // skip opening quote
    
    // First pass: calculate length
    const char* start = *s;
    int len = 0;
    while (**s && **s != '"') {
        if (**s == '\\') {
            (*s)++;
            if (!**s) return false;
        }
        (*s)++;
        len++;
    }
    
    if (**s != '"') return false;
    
    char* buffer = (char*)malloc(len + 1);
    if (!buffer) return false;
    
    // Second pass: copy and unescape
    const char* p = start;
    int idx = 0;
    while (p < *s) {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '"': buffer[idx++] = '"'; break;
                case '\\': buffer[idx++] = '\\'; break;
                case '/': buffer[idx++] = '/'; break;
                case 'b': buffer[idx++] = '\b'; break;
                case 'f': buffer[idx++] = '\f'; break;
                case 'n': buffer[idx++] = '\n'; break;
                case 'r': buffer[idx++] = '\r'; break;
                case 't': buffer[idx++] = '\t'; break;
                case 'u': 
                    // Simple unicode handling: just skip for now or insert '?'
                    // A full implementation would decode UTF-16 surrogate pairs
                    buffer[idx++] = '?'; 
                    p += 4; 
                    break;
                default: buffer[idx++] = *p; break;
            }
        } else {
            buffer[idx++] = *p;
        }
        p++;
    }
    buffer[idx] = '\0';
    
    *out_str = buffer;
    *out_len = idx;
    (*s)++; // skip closing quote
    return true;
}

// ========== Helper: Parse Number ==========
static bool parse_number(const char** s, Value* out_value) {
    char* endptr;
    double val = strtod(*s, &endptr);
    if (endptr == *s) return false;
    
    *out_value = vm_make_number(val);
    *s = endptr;
    return true;
}

// ========== Recursive Descent Parser ==========

static bool parse_value(VM* vm, const char** json_str, Value* out_value) {
    skip_ws(json_str);
    if (!**json_str) return false;

    char c = **json_str;

    // 1. Null -> false
    if (strncmp(*json_str, "null", 4) == 0) {
        *out_value = vm_make_bool(false);
        *json_str += 4;
        return true;
    }

    // 2. Boolean
    if (strncmp(*json_str, "true", 4) == 0) {
        *out_value = vm_make_bool(true);
        *json_str += 4;
        return true;
    }
    if (strncmp(*json_str, "false", 5) == 0) {
        *out_value = vm_make_bool(false);
        *json_str += 5;
        return true;
    }

    // 3. String
    if (c == '"') {
        char* str_val = NULL;
        int len = 0;
        if (!parse_string_raw(json_str, &str_val, &len)) return false;
        *out_value = vm_make_string(str_val);
        free(str_val);
        return true;
    }

    // 4. Number
    if (c == '-' || isdigit(c)) {
        return parse_number(json_str, out_value);
    }

    // 5. Array -> Table (1-indexed)
    if (c == '[') {
        (*json_str)++;
        *out_value = vm_make_table();
        skip_ws(json_str);
        
        int index = 1;
        if (**json_str != ']') {
            while (1) {
                Value item;
                if (!parse_value(vm, json_str, &item)) return false;
                
                char key[32];
                snprintf(key, sizeof(key), "%d", index++);
                table_set(out_value->table, key, item);
                
                skip_ws(json_str);
                if (**json_str == ',') {
                    (*json_str)++;
                } else {
                    break;
                }
            }
        }
        
        if (**json_str != ']') return false;
        (*json_str)++;
        return true;
    }

    // 6. Object -> Table
    if (c == '{') {
        (*json_str)++;
        *out_value = vm_make_table();
        skip_ws(json_str);
        
        if (**json_str != '}') {
            while (1) {
                skip_ws(json_str);
                if (**json_str != '"') return false;
                
                char* key_str = NULL;
                int key_len = 0;
                if (!parse_string_raw(json_str, &key_str, &key_len)) return false;
                
                skip_ws(json_str);
                if (**json_str != ':') {
                    free(key_str);
                    return false;
                }
                (*json_str)++;
                
                Value val;
                if (!parse_value(vm, json_str, &val)) {
                    free(key_str);
                    return false;
                }
                
                table_set(out_value->table, key_str, val);
                free(key_str);
                
                skip_ws(json_str);
                if (**json_str == ',') {
                    (*json_str)++;
                } else {
                    break;
                }
            }
        }
        
        if (**json_str != '}') return false;
        (*json_str)++;
        return true;
    }

    return false;
}

// ========== Encoder Helpers ==========

static void append_escaped(StringBuilder* sb, const char* str) {
    sb_append(sb, "\"", 1);
    while (*str) {
        unsigned char c = *str;
        switch (c) {
            case '"': sb_append(sb, "\\\"", 2); break;
            case '\\': sb_append(sb, "\\\\", 2); break;
            case '\b': sb_append(sb, "\\b", 2); break;
            case '\f': sb_append(sb, "\\f", 2); break;
            case '\n': sb_append(sb, "\\n", 2); break;
            case '\r': sb_append(sb, "\\r", 2); break;
            case '\t': sb_append(sb, "\\t", 2); break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    sb_append(sb, buf, 6);
                } else {
                    char buf[2] = { (char)c, 0 };
                    sb_append(sb, buf, 1);
                }
                break;
        }
        str++;
    }
    sb_append(sb, "\"", 1);
}

static void encode_value(VM* vm, Value* value, StringBuilder* sb) {
    if (!value) {
        sb_append(sb, "false", 5);
        return;
    }

    switch (value->type) {
        case VAL_NUMBER: {
            char buf[64];
            double num = value->number;
            if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) {
                snprintf(buf, sizeof(buf), "%lld", (long long)num);
            } else {
                snprintf(buf, sizeof(buf), "%.15g", num);
            }
            sb_append(sb, buf, (int)strlen(buf));
            break;
        }
        case VAL_BOOL:
            sb_append(sb, value->boolean ? "true" : "false", value->boolean ? 4 : 5);
            break;
        case VAL_STRING:
            append_escaped(sb, value->string->chars);
            break;
        case VAL_TABLE: {
            Table* t = value->table;
            if (t->count == 0) {
                sb_append(sb, "{}", 2);
                break;
            }

            // Check if it's an array-like table (keys are "1", "2", ...)
            bool is_array = true;
            int count = 0;
            for (int i = 0; i < t->capacity; i++) {
                TableEntry* entry = t->entries[i];
                while (entry) {
                    count++;
                    // Simple check: if key is not a number string, it's an object
                    char* endptr;
                    strtol(entry->key, &endptr, 10);
                    if (*endptr != '\0') {
                        is_array = false;
                    }
                    entry = entry->next;
                }
            }

            if (is_array && count > 0) {
                sb_append(sb, "[", 1);
                for (int i = 1; i <= count; i++) {
                    if (i > 1) sb_append(sb, ", ", 2);
                    char key[32];
                    snprintf(key, sizeof(key), "%d", i);
                    Value val;
                    if (table_get(t, key, &val)) {
                        encode_value(vm, &val, sb);
                        value_decref(&val);
                    } else {
                        sb_append(sb, "null", 4);
                    }
                }
                sb_append(sb, "]", 1);
            } else {
                sb_append(sb, "{", 1);
                bool first = true;
                for (int i = 0; i < t->capacity; i++) {
                    TableEntry* entry = t->entries[i];
                    while (entry) {
                        if (!first) sb_append(sb, ", ", 2);
                        first = false;
                        
                        append_escaped(sb, entry->key);
                        sb_append(sb, ": ", 2);
                        encode_value(vm, &entry->value, sb);
                        
                        entry = entry->next;
                    }
                }
                sb_append(sb, "}", 1);
            }
            break;
        }
        default:
            sb_append(sb, "null", 4);
            break;
    }
}

// ========== Public API ==========

bool json_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;

    if (strcmp(name, "json.decode") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            const char* json_str = args[0].string->chars;
            if (parse_value(vm, &json_str, result)) {
                return true;
            }
        }
        *result = vm_make_bool(false);
        return true;
    }

    if (strcmp(name, "json.encode") == 0) {
        if (arg_count >= 1) {
            StringBuilder sb;
            sb_init(&sb, 256);
            encode_value(vm, &args[0], &sb);
            
            *result = vm_make_string(sb.buffer);
            sb_free(&sb);
            return true;
        }
        *result = vm_make_string("");
        return true;
    }

    return false;
}
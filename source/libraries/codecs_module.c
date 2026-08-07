// source/libraries/codecs_module.c
// Implementation of Codecs Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codecs_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>

// standard base64 character set with padding
static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

// encodes binary data to base64 with proper padding
static void base64_encode(const unsigned char* data, int len, char* out) {
    int i = 0, j = 0;                                                        // input and output indices
    uint32_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining in buffer
    
    while (i < len) {                                                        // iterate over input bytes
        buffer = (buffer << 8) | data[i++];                                  // add byte to buffer
        bits_left += 8;                                                      // increment bits
        while (bits_left >= 6) {                                             // extract 6-bit chunks
            int char_idx = (buffer >> (bits_left - 6)) & 0x3F;               // get 6-bit index
            out[j++] = b64_chars[char_idx];                                  // append base64 char
            bits_left -= 6;                                                  // remove processed bits
        }
    }
    
    if (bits_left > 0) {                                                     // leftover bits
        int char_idx = (buffer << (6 - bits_left)) & 0x3F;                   // pad with zeros
        out[j++] = b64_chars[char_idx];                                      // append last char
    }
    
    while (j % 4 != 0) {                                                     // add padding
        out[j++] = '=';                                                      // padding char
    }
    out[j] = '\0';                                                           // null terminate
}

// decodes a single base64 character, returns -1 on invalid
static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';                                // uppercase
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;                           // lowercase
    if (c >= '0' && c <= '9') return c - '0' + 52;                           // digits
    if (c == '+') return 62;                                                 // plus
    if (c == '/') return 63;                                                 // slash
    if (c == '=') return 0;                                                  // padding
    return -1;                                                               // invalid
}

// decodes a base64 string into binary data
static bool base64_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);                                                   // input length
    if (len == 0) { *out_len = 0; return true; }                             // empty input
    
    for (int i = 0; i < len; i++) {                                          // validate all chars
        if (str[i] != '=' && base64_decode_char(str[i]) < 0) return false;   // invalid char
    }
    
    uint32_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining
    *out_len = 0;                                                            // output length
    int padding = 0;                                                         // padding count
    
    for (int i = 0; i < len; i++) {                                          // iterate over input
        if (str[i] == '=') {                                                 // padding
            padding++;                                                       // count padding
            continue;                                                        // skip
        }
        int val = base64_decode_char(str[i]);                                // decode char
        if (val < 0) return false;                                           // invalid
        
        buffer = (buffer << 6) | val;                                        // add to buffer
        bits_left += 6;                                                      // increment bits
        
        if (bits_left >= 8) {                                                // have enough for byte
            out[(*out_len)++] = (unsigned char)(buffer >> (bits_left - 8));  // extract byte
            bits_left -= 8;                                                  // remove processed bits
        }
    }
    
    if (padding == 1 && bits_left != 2) return false;                      // 1 '=' = 2 extra bits
    if (padding == 2 && bits_left != 4) return false;                      // 2 '=' = 4 extra bits
    if (padding > 2) return false;                                         // too much padding
    if (padding == 0 && len % 4 != 0) return false;                        // no padding, length must be multiple of 4
    
    return true;                                                             // success
}

// url-safe base64 character set (uses - and _ instead of + and /)
static const char b64url_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";

// encodes binary data to url-safe base64 without padding
static void base64url_encode(const unsigned char* data, int len, char* out) {
    int i = 0, j = 0;                                                        // input and output indices
    uint32_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining
    
    while (i < len) {                                                        // iterate over input
        buffer = (buffer << 8) | data[i++];                                  // add byte
        bits_left += 8;                                                      // increment bits
        while (bits_left >= 6) {                                             // extract 6-bit chunks
            int char_idx = (buffer >> (bits_left - 6)) & 0x3F;               // get index
            out[j++] = b64url_chars[char_idx];                               // append url-safe char
            bits_left -= 6;                                                  // remove processed bits
        }
    }
    
    if (bits_left > 0) {                                                     // leftover bits
        int char_idx = (buffer << (6 - bits_left)) & 0x3F;                   // pad with zeros
        out[j++] = b64url_chars[char_idx];                                   // append last char
    }
    
    out[j] = '\0';                                                           // null terminate (no padding)
}

// decodes a url-safe base64 character
static int base64url_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';                                // uppercase
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;                           // lowercase
    if (c >= '0' && c <= '9') return c - '0' + 52;                           // digits
    if (c == '-') return 62;                                                 // dash (url-safe)
    if (c == '_') return 63;                                                 // underscore (url-safe)
    if (c == '=') return 0;                                                  // padding
    return -1;                                                               // invalid
}

// decodes a url-safe base64 string
static bool base64url_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);                                                   // input length
    if (len == 0) {                                                          // empty input
        *out_len = 0;                                                        // empty output
        return true;
    }
    
    uint32_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining
    *out_len = 0;                                                            // output length
    
    for (int i = 0; i < len; i++) {                                          // iterate over input
        if (str[i] == '=') break;                                            // padding stops decoding
        int val = base64url_decode_char(str[i]);                             // decode char
        if (val < 0) return false;                                           // invalid
        
        buffer = (buffer << 6) | val;                                        // add to buffer
        bits_left += 6;                                                      // increment bits
        
        if (bits_left >= 8) {                                                // have enough for byte
            out[(*out_len)++] = (unsigned char)(buffer >> (bits_left - 8));  // extract byte
            bits_left -= 8;                                                  // remove processed bits
        }
    }
    return true;                                                             // success
}

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
    
    if (c == '-' || isdigit(c)) {                                        // number value
        return parse_number(json_str, out_value);                        // parse number
    }
    
    if (c == '[') {                                                      // array value
        (*json_str)++;                                                   // skip opening bracket
        Table* table = table_create(8);                                  // create table
        *out_value = MAKE_TABLE(table);                                  // store as table
        skip_ws(json_str);                                               // skip whitespace
        int index = 1;                                                   // 1-based index
        if (**json_str != ']') {                                         // non-empty array
            while (1) {                                                  // parse elements
                Value item;                                              // element value
                if (!json_parse_value(vm, json_str, &item)) {            // parse element
                    value_decref(*out_value);                            // release table
                    return false;                                        // parse failed
                }
                Value k = MAKE_NUMBER((double)index++);                  // create index key
                table_set(table, k, item);                               // store element
                value_decref(item);                                      // release element
                skip_ws(json_str);                                       // skip whitespace
                if (**json_str == ',') {                                 // comma separator
                    (*json_str)++;                                       // skip comma
                } else {
                    break;                                               // end of array
                }
            }
        }
        if (**json_str != ']') {                                         // missing closing bracket
            value_decref(*out_value);                                    // release table
            return false;                                                // parse failed
        }
        (*json_str)++;                                                   // skip closing bracket
        return true;                                                     // success
    }
    
    if (c == '{') {                                                      // object value
        (*json_str)++;                                                   // skip opening brace
        Table* table = table_create(8);                                  // create table
        *out_value = MAKE_TABLE(table);                                  // store as table
        skip_ws(json_str);                                               // skip whitespace
        if (**json_str != '}') {                                         // non-empty object
            while (1) {                                                  // parse key-value pairs
                skip_ws(json_str);                                       // skip whitespace
                if (**json_str != '"') {                                 // key must be string
                    value_decref(*out_value);                            // release table
                    return false;                                        // parse failed
                }
                char* key_str = NULL;                                    // parsed key
                int key_len = 0;                                         // key length
                if (!parse_string_raw(json_str, &key_str, &key_len)) {   // parse key
                    value_decref(*out_value);                            // release table
                    return false;                                        // parse failed
                }
                skip_ws(json_str);                                       // skip whitespace
                if (**json_str != ':') {                                 // missing colon
                    free(key_str);                                       // free key
                    value_decref(*out_value);                            // release table
                    return false;                                        // parse failed
                }
                (*json_str)++;                                           // skip colon
                Value val;                                               // value
                if (!json_parse_value(vm, json_str, &val)) {             // parse value
                    free(key_str);                                       // free key
                    value_decref(*out_value);                            // release table
                    return false;                                        // parse failed
                }
                Value k = MAKE_STRING(string_intern(&vm->intern_table, key_str, key_len));  // intern key
                table_set(table, k, val);                                // store key-value
                value_decref(k);                                         // release key
                value_decref(val);                                       // release value
                free(key_str);                                           // free key buffer
                skip_ws(json_str);                                       // skip whitespace
                if (**json_str == ',') {                                 // comma separator
                    (*json_str)++;                                       // skip comma
                } else {
                    break;                                               // end of object
                }
            }
        }
        if (**json_str != '}') {                                         // missing closing brace
            value_decref(*out_value);                                    // release table
            return false;                                                // parse failed
        }
        (*json_str)++;                                                   // skip closing brace
        return true;                                                     // success
    }
    
    return false;                                                        // unknown token
}

// appends a json-escaped string to the builder
static void append_escaped(StringBuilder* sb, const char* str) {
    sb_append(sb, "\"", 1);                                              // opening quote
    while (*str) {                                                       // iterate over string
        unsigned char c = *str;                                          // current char
        switch (c) {                                                     // handle escapes
            case '"': sb_append(sb, "\\\"", 2); break;                   // quote
            case '\\': sb_append(sb, "\\\\", 2); break;                  // backslash
            case '\b': sb_append(sb, "\\b", 2); break;                   // backspace
            case '\f': sb_append(sb, "\\f", 2); break;                   // form feed
            case '\n': sb_append(sb, "\\n", 2); break;                   // newline
            case '\r': sb_append(sb, "\\r", 2); break;                   // carriage return
            case '\t': sb_append(sb, "\\t", 2); break;                   // tab
            default:
                if (c < 0x20) {                                          // control char
                    char buf[8];                                         // unicode escape buffer
                    snprintf(buf, sizeof(buf), "\\u%04x", c);            // format as unicode
                    sb_append(sb, buf, 6);                               // append escape
                } else {
                    char buf[2] = { (char)c, 0 };                        // single char
                    sb_append(sb, buf, 1);                               // append char
                }
                break;
        }
        str++;                                                           // advance
    }
    sb_append(sb, "\"", 1);                                              // closing quote
}

// recursively encodes a vm value to json
static void json_encode_value(VM* vm, Value value, StringBuilder* sb) {
    (void)vm;                                                            // vm unused
    
    if (IS_NUMBER(value)) {                                              // number value
        char buf[64];                                                    // buffer for number
        double num = AS_NUMBER(value);                                   // extract number
        if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) {     // integer
            snprintf(buf, sizeof(buf), "%lld", (long long)num);          // format as integer
        } else {
            snprintf(buf, sizeof(buf), "%.15g", num);                    // format as float
        }
        sb_append(sb, buf, (int)strlen(buf));                                      // append number
    } else if (IS_BOOL(value)) {                                                   // boolean value
        sb_append(sb, AS_BOOL(value) ? "true" : "false", AS_BOOL(value) ? 4 : 5);  // append bool
    } else if (IS_STRING(value)) {                                                 // string value
        append_escaped(sb, AS_STRING(value)->chars);                               // append escaped string
    } else if (IS_TABLE(value)) {                                                  // table value
        Table* t = AS_TABLE(value);                                                // unwrap table
        if (TABLE_TOTAL_COUNT(t) == 0) {                                           // empty table
            sb_append(sb, "{}", 2);                                                // empty object
            return;
        }
        
        bool is_array = (t->array_count > 0 && t->hash_count == 0);              // check if array
        
        if (is_array) {                                                          // array
            sb_append(sb, "[", 1);                                               // opening bracket
            for (int i = 0; i < t->array_count; i++) {                           // iterate elements
                if (i > 0) sb_append(sb, ", ", 2);                               // comma separator
                json_encode_value(vm, t->array_part[i], sb);                     // encode element
            }
            sb_append(sb, "]", 1);                                               // closing bracket
        } else {                                                                 // object
            sb_append(sb, "{", 1);                                               // opening brace
            bool first = true;                                                   // first item flag
            
            for (int i = 0; i < t->array_count; i++) {                           // array part as key-value
                if (!IS_BOOL(t->array_part[i]) || AS_BOOL(t->array_part[i])) {   // skip false values
                    if (!first) sb_append(sb, ", ", 2);                          // comma separator
                    first = false;                                               // not first anymore
                    char key[32];                                                // numeric key buffer
                    snprintf(key, sizeof(key), "%d", i + 1);                     // 1-based index
                    append_escaped(sb, key);                                     // append key
                    sb_append(sb, ": ", 2);                                      // colon separator
                    json_encode_value(vm, t->array_part[i], sb);                 // encode value
                }
            }
            
            for (int i = 0; i < t->capacity; i++) {                              // hash part
                TableEntry* entry = t->entries[i];                               // bucket head
                while (entry) {                                                  // traverse chain
                    if (!first) sb_append(sb, ", ", 2);                          // comma separator
                    first = false;                                               // not first anymore
                    if (IS_STRING(entry->key)) {                                 // string key
                        append_escaped(sb, AS_STRING(entry->key)->chars);        // append key
                    } else if (IS_NUMBER(entry->key)) {                          // number key
                        char num_buf[64];                                        // number buffer
                        snprintf(num_buf, sizeof(num_buf), "%g", AS_NUMBER(entry->key));  // format
                        append_escaped(sb, num_buf);                             // append key
                    }
                    sb_append(sb, ": ", 2);                                      // colon separator
                    json_encode_value(vm, entry->value, sb);                     // encode value
                    entry = entry->next;                                         // advance
                }
            }
            sb_append(sb, "}", 1);                                               // closing brace
        }
    } else {
        sb_append(sb, "null", 4);                                                // null
    }
}

// csv parser state with position and delimiter
typedef struct {
    const char* data;                                                            // input data
    int pos;                                                                     // current position
    int len;                                                                     // total length
    char delimiter;                                                              // field delimiter
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

// parses a csv field into a typed vm value
static Value csv_parse_value(VM* vm, const char* str) {
    if (!str) return MAKE_NONE();                                               // null string
    if (is_numeric(str)) {                                                      // numeric
        return MAKE_NUMBER(atof(str));                                          // parse as number
    }
    if (is_bool(str)) {                                                         // boolean
        return MAKE_BOOL(strcmp(str, "true") == 0);                             // parse as bool
    }
    return MAKE_STRING(string_intern(&vm->intern_table, str, strlen(str)));     // intern string
}

// extracts a single csv field with quoted field support
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
        if (csv_peek(p) == p->delimiter) csv_advance(p);                        // skip delimiter
        else if (csv_peek(p) == '\r') {                                         // carriage return
            csv_advance(p);                                                     // skip \r
            if (csv_peek(p) == '\n') csv_advance(p);                            // skip \n
        } else if (csv_peek(p) == '\n') {                                       // newline
            csv_advance(p);                                                     // skip \n
        }
    } else {                                                                    // unquoted field
        while (csv_has_next(p)) {                                               // parse field
            c = csv_peek(p);                                                    // peek char
            if (c == p->delimiter || c == '\n' || c == '\r') {                  // delimiter or newline
                break;                                                          // end of field
            }
            sb_append_char(&sb, csv_advance(p));                                // append char
        }
        if (csv_peek(p) == p->delimiter) csv_advance(p);                        // skip delimiter
        else if (csv_peek(p) == '\r') {                                         // carriage return
            csv_advance(p);                                                     // skip \r
            if (csv_peek(p) == '\n') csv_advance(p);                            // skip \n
        } else if (csv_peek(p) == '\n') {                                       // newline
            csv_advance(p);                                                     // skip \n
        }
    }
    
    char* result = sb.buffer ? strdup(sb.buffer) : strdup("");                  // copy result
    sb_free(&sb);                                                               // free builder
    return result;                                                              // return field
}

// xml parser state
typedef struct {
    const char* p;                                                              // current parse position
} XmlParser;

// skips whitespace in xml
static void xml_skip_ws(XmlParser* xp) {
    while (*xp->p && isspace((unsigned char)*xp->p)) xp->p++;                   // skip whitespace
}

// parses xml attributes into a table with @ prefix
static void xml_parse_attrs(VM* vm, XmlParser* xp, Table* t) {
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
        Value k = MAKE_STRING(string_intern(&vm->intern_table, attr_key_buf, strlen(attr_key_buf)));  // intern key
        Value v = MAKE_STRING(string_intern(&vm->intern_table, val_sb.buffer, val_sb.length));        // intern value
        table_set(t, k, v);                                                     // store attribute
        value_decref(k);                                                        // release key
        value_decref(v);                                                        // release value
        sb_free(&val_sb);                                                       // free builder
        
        xml_skip_ws(xp);                                                        // skip whitespace
    }
}

// parses a single xml element recursively
static Value xml_parse_element(VM* vm, XmlParser* xp) {
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
    
    Value k_tag = MAKE_STRING(string_intern(&vm->intern_table, "__tag", 5));        // tag key
    Value v_tag = MAKE_STRING(string_intern(&vm->intern_table, tag, strlen(tag)));  // tag value
    table_set(elem_table, k_tag, v_tag);                                            // store tag
    value_decref(k_tag);                                                            // release key
    value_decref(v_tag);                                                            // release value
    
    xml_parse_attrs(vm, xp, elem_table);                                        // parse attributes
    
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
                    Value child = xml_parse_element(vm, xp);                    // parse child
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
                    Value k_text = MAKE_STRING(string_intern(&vm->intern_table, "#text", 5));                      // text key
                    Value v_text = MAKE_STRING(string_intern(&vm->intern_table, text_sb.buffer, text_sb.length));  // text value
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

// recursively writes an xml node from a vm table
static void xml_write_node(VM* vm, Value v, int depth, StringBuilder* sb) {
    if (!IS_TABLE(v)) return;                                                  // not a table
    
    Table* table = AS_TABLE(v);                                                // unwrap table
    
    Value k_tag = MAKE_STRING(string_intern(&vm->intern_table, "__tag", 5));   // tag key
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
    
    Value k_text = MAKE_STRING(string_intern(&vm->intern_table, "#text", 5));  // text key
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
                xml_write_node(vm, child_val, depth + 1, sb);                  // recursively write child
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
                            xml_write_node(vm, e->value, depth + 1, sb);       // recursively write child
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
        
        Value k_close = MAKE_STRING(string_intern(&vm->intern_table, "__tag", 5));  // tag key
        if (table_get(table, k_close, &tag_val)) {                                  // get tag
            value_decref(k_close);                                                  // release key
            if (IS_STRING(tag_val)) {                                               // valid tag string
                StringObject* close_str = AS_STRING(tag_val);                       // tag string
                sb_append(sb, close_str->chars, close_str->length);                 // append tag name
            }
            value_decref(tag_val);                                                  // release tag value
        } else {
            value_decref(k_close);                                                  // release key
        }
        sb_append(sb, ">\n", 2);                                                    // closing tag end with newline
    } else {
        sb_append(sb, "/>\n", 3);                                                   // self-closing tag
    }
}

// main dispatcher for all codecs module built-in functions
bool codecs_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "codecs.base_write") == 0) {                                   // base64 encode
        if (arg_count < 1 || !IS_STRING(args[0])) {                                 // validate string
            *result = MAKE_NONE();                                                  // invalid
            return true;                                                            // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                               // input string
        int input_len = input_str->length;                                          // input length
        int out_size = ((input_len + 2) / 3) * 4 + 1;                               // output size
        char* out = (char*)malloc(out_size);                                        // allocate output
        if (!out) { *result = MAKE_NONE(); return true; }                           // allocation failed
        base64_encode((const unsigned char*)input_str->chars, input_len, out);      // encode
        *result = MAKE_STRING(string_intern(&vm->intern_table, out, strlen(out)));  // intern result
        free(out);                                                                  // free output
        return true;                                                                // builtin handled
    }
    
    if (strcmp(name, "codecs.base_read") == 0) {                                    // base64 decode
        if (arg_count < 1 || !IS_STRING(args[0])) {                                 // validate string
            *result = MAKE_NONE();                                                  // invalid
            return true;                                                            // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                               // input string
        int input_len = input_str->length;                                          // input length
        unsigned char* out = (unsigned char*)malloc(input_len + 1);                 // allocate output
        if (!out) { *result = MAKE_NONE(); return true; }                           // allocation failed
        int out_len = 0;                                                            // output length
        if (base64_decode(input_str->chars, out, &out_len)) {                       // decode
            out[out_len] = '\0';                                                    // null terminate
            *result = MAKE_STRING(string_intern(&vm->intern_table, (char*)out, out_len)); // intern result
        } else {
            *result = MAKE_NONE();                                                  // decode failed
        }
        free(out);                                                                  // free output
        return true;                                                                // builtin handled
    }

    if (strcmp(name, "codecs.baseurl_write") == 0) {                                // base64url encode
        if (arg_count < 1 || !IS_STRING(args[0])) {                                 // validate string
            *result = MAKE_NONE();                                                  // invalid
            return true;                                                            // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                               // input string
        int input_len = input_str->length;                                          // input length
        int out_size = ((input_len + 2) / 3) * 4 + 1;                               // output size
        char* out = (char*)malloc(out_size);                                        // allocate output
        if (!out) { *result = MAKE_NONE(); return true; }                           // allocation failed
        base64url_encode((const unsigned char*)input_str->chars, input_len, out);   // encode
        *result = MAKE_STRING(string_intern(&vm->intern_table, out, strlen(out)));  // intern result
        free(out);                                                                  // free output
        return true;                                                                // builtin handled
    }
    
    if (strcmp(name, "codecs.baseurl_read") == 0) {                                 // base64url decode
        if (arg_count < 1 || !IS_STRING(args[0])) {                                 // validate string
            *result = MAKE_NONE();                                                  // invalid
            return true;                                                            // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                               // input string
        int input_len = input_str->length;                                          // input length
        unsigned char* out = (unsigned char*)malloc(input_len + 1);                 // allocate output
        if (!out) { *result = MAKE_NONE(); return true; }                           // allocation failed
        int out_len = 0;                                                            // output length
        if (base64url_decode(input_str->chars, out, &out_len)) {                    // decode
            out[out_len] = '\0';                                                    // null terminate
            *result = MAKE_STRING(string_intern(&vm->intern_table, (char*)out, out_len));  // intern result
        } else {
            *result = MAKE_NONE();                                                  // decode failed
        }
        free(out);                                                                  // free output
        return true;                                                                // builtin handled
    }

    if (strcmp(name, "codecs.json_read") == 0) {                                    // parse json
        if (arg_count < 1 || !IS_STRING(args[0])) {                                 // validate string
            *result = MAKE_NONE();                                                  // invalid
            return true;                                                            // builtin handled
        }
        const char* json_str = AS_STRING(args[0])->chars;                           // json string
        if (json_parse_value(vm, &json_str, result)) {                              // parse
            return true;                                                            // success
        }
        *result = MAKE_NONE();                                                      // parse failed
        return true;                                                                // builtin handled
    }
    
    if (strcmp(name, "codecs.json_write") == 0) {                                   // json serialize
        if (arg_count < 1) {                                                        // need value
            *result = MAKE_NONE();                                                  // invalid
            return true;                                                            // builtin handled
        }
        StringBuilder sb;                                                               // string builder
        sb_init(&sb, 256);                                                              // init builder
        json_encode_value(vm, args[0], &sb);                                            // encode value
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length));  // intern result
        sb_free(&sb);                                                                   // free builder
        return true;                                                                    // builtin handled
    }

    if (strcmp(name, "codecs.csv_read") == 0) {                                  // parse csv
        if (arg_count < 1 || !IS_STRING(args[0])) {                              // validate string
            *result = MAKE_NONE();                                               // invalid
            return true;                                                         // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                            // input string
        const char* data = input_str->chars;                                     // csv data
        int len = input_str->length;                                             // data length
        bool has_header = true;                                                  // default header
        char delimiter = ',';                                                    // default delimiter
        
        if (arg_count >= 2 && IS_BOOL(args[1])) has_header = AS_BOOL(args[1]);       // custom header
        if (arg_count >= 3 && IS_STRING(args[2]) && AS_STRING(args[2])->length > 0)  // custom delimiter
            delimiter = AS_STRING(args[2])->chars[0];
            
        if (len <= 0) {                                                          // empty input
            *result = MAKE_NONE();                                               // return none
            return true;                                                         // builtin handled
        }
        
        CsvParser parser = { .data = data, .pos = 0, .len = len, .delimiter = delimiter };  // init parser
        Table* table_list_table = table_create(8);                                          // result table
        Value table_list = MAKE_TABLE(table_list_table);                                    // box table
        char** headers = NULL;                                                              // header array
        int col_count = 0;                                                                  // column count
        
        if (has_header && parser.len > 0) {                                                 // parse header row
            int temp_pos = parser.pos;                                                      // save position
            int count = 0;                                                                  // column count
            while (parser.pos < parser.len && parser.data[parser.pos] != '\n' && parser.data[parser.pos] != '\r') {
                if (parser.data[parser.pos] == delimiter) count++;               // count delimiters
                parser.pos++;                                                     // advance
            }
            col_count = count + 1;                                                // number of columns
            parser.pos = temp_pos;                                                // restore position
            
            if (col_count > 0) {                                                                // valid columns
                headers = (char**)malloc(sizeof(char*) * col_count);                            // allocate headers
                if (headers) {                                                                  // allocation succeeded
                    for (int i = 0; i < col_count; i++) headers[i] = csv_parse_field(&parser);  // parse each
                }
            }
        }
        
        int row_index = 1;                                                                     // row counter
        while (parser.pos < parser.len) {                                                      // parse data rows
            if (parser.data[parser.pos] == '\n' || parser.data[parser.pos] == '\r') {          // skip blank lines
                if (parser.data[parser.pos] == '\r') parser.pos++;                             // skip \r
                if (parser.pos < parser.len && parser.data[parser.pos] == '\n') parser.pos++;  // skip \n
                continue;                                                                      // continue
            }
            
            Table* row_table = table_create(8);                    // row table
            Value row_table_val = MAKE_TABLE(row_table);           // box row
            if (has_header && headers && col_count > 0) {          // header mode
                for (int i = 0; i < col_count; i++) {              // parse columns
                    if (!csv_has_next(&parser)) break;             // end of row
                    char* field = csv_parse_field(&parser);        // parse field
                    Value val = csv_parse_value(vm, field);        // parse value
                    if (headers[i]) {                              // valid header
                        Value k = MAKE_STRING(string_intern(&vm->intern_table, headers[i], strlen(headers[i])));  // intern key
                        table_set(row_table, k, val);              // store value
                        value_decref(k);                           // release key
                    }
                    value_decref(val);                             // release value
                    free(field);                                   // free field
                }
            } else {                                               // no header mode
                int idx = 0;                                       // field index
                while (csv_has_next(&parser)) {                    // parse fields
                    Value k = MAKE_NUMBER((double)(idx + 1));      // numeric key
                    char* field = csv_parse_field(&parser);        // parse field
                    Value val = csv_parse_value(vm, field);        // parse value
                    table_set(row_table, k, val);                  // store value
                    value_decref(k);                               // release key
                    value_decref(val);                             // release value
                    free(field);                                   // free field
                    idx++;                                         // increment index
                }
            }
            
            Value k_idx = MAKE_NUMBER((double)row_index++);        // row index key
            table_set(table_list_table, k_idx, row_table_val);     // store row
            value_decref(k_idx);                                   // release key
            value_decref(row_table_val);                           // release row
        }
        
        if (headers) {                                             // free headers
            for (int i = 0; i < col_count; i++) free(headers[i]);  // free each
            free(headers);                                         // free array
        }
        *result = table_list;                                      // return table
        return true;                                               // builtin handled
    }
    
    if (strcmp(name, "codecs.csv_write") == 0) {                   // csv serialize
        if (arg_count < 1 || !IS_TABLE(args[0])) {                 // validate table
            *result = MAKE_NONE();                                 // invalid
            return true;                                           // builtin handled
        }
        Table* data = AS_TABLE(args[0]);                           // data table
        bool has_header = true;                                    // default header
        char delimiter = ',';                                      // default delimiter
        
        if (arg_count >= 2 && IS_BOOL(args[1])) has_header = AS_BOOL(args[1]);        // custom header
        if (arg_count >= 3 && IS_STRING(args[2]) && AS_STRING(args[2])->length > 0)   // custom delimiter
            delimiter = AS_STRING(args[2])->chars[0];
            
        int row_count = table_size(data);                                             // number of rows
        if (row_count == 0) {                                                         // empty data
            *result = MAKE_STRING(string_intern(&vm->intern_table, "", 0));           // empty string
            return true;                                                              // builtin handled
        }
        
        Value first_row_val;                                                          // first row value
        bool found_first = false;                                                     // found flag
        
        if (data->array_count > 0 && IS_TABLE(data->array_part[0])) {                 // array part has rows
            first_row_val = data->array_part[0];                                      // get first row (0-based)
            value_incref(first_row_val);                                              // increment ref for ownership
            found_first = true;                                                       // found
        }
        
        if (!found_first) {                                                           // not found yet
            Value k_num = MAKE_NUMBER(1.0);                                           // numeric key
            if (table_get(data, k_num, &first_row_val) && IS_TABLE(first_row_val)) {  // found
                found_first = true;                                                   // mark found
            } else {
                value_decref(first_row_val);                                          // release if invalid
            }
            value_decref(k_num);                                                      // release key
        }
        
        if (!found_first) {                                                           // not found yet
            Value k_str = MAKE_STRING(string_intern(&vm->intern_table, "1", 1));      // string key
            if (table_get(data, k_str, &first_row_val) && IS_TABLE(first_row_val)) {  // found
                found_first = true;                                                   // mark found
            } else {
                value_decref(first_row_val);                                          // release if invalid
            }
            value_decref(k_str);                                                      // release key
        }
        
        if (!found_first) {                                                           // no first row
            *result = MAKE_NONE();                                                    // return none
            return true;                                                              // builtin handled
        }
        
        int header_count = 0;                                                     // header count
        Value* headers = table_keys(AS_TABLE(first_row_val), &header_count);      // get headers
        value_decref(first_row_val);                                              // release first row
        
        StringBuilder sb;                                                         // string builder
        sb_init(&sb, 256);                                                        // init builder
        
        if (has_header && headers && header_count > 0) {                          // write headers
            for (int i = 0; i < header_count; i++) {                              // iterate headers
                if (i > 0) sb_append_char(&sb, delimiter);                        // delimiter
                const char* h = "";                                               // default header
                char h_buf[64];                                                   // buffer for number
                if (IS_STRING(headers[i])) h = AS_STRING(headers[i])->chars;      // string header
                else if (IS_NUMBER(headers[i])) {                                 // number header
                    snprintf(h_buf, sizeof(h_buf), "%g", AS_NUMBER(headers[i]));  // format
                    h = h_buf;                                                    // use buffer
                }
                bool needs_quote = strchr(h, delimiter) || strchr(h, '"') || strchr(h, '\n');  // needs quoting
                if (needs_quote) {                                              // quote field
                    sb_append_char(&sb, '"');                                   // opening quote
                    for (const char* p = h; *p; p++) {                          // escape quotes
                        if (*p == '"') sb_append_char(&sb, '"');                // double quote
                        sb_append_char(&sb, *p);                                // append char
                    }
                    sb_append_char(&sb, '"');                                   // closing quote
                } else {
                    sb_append(&sb, h, strlen(h));                               // append header
                }
            }
            sb_append(&sb, "\n", 1);                                            // newline
        }
        
        int written = 0;                                                               // rows written
        int total_rows = data->array_count > 0 ? data->array_count : row_count;        // rows to try
        
        for (int r = 1; r <= total_rows + 10; r++) {                                   // iterate possible rows
            Value row_val;                                                             // row value
            bool got_row = false;                                                      // found flag
            
            if (r - 1 < data->array_count && IS_TABLE(data->array_part[r - 1])) {      // in array bounds
                row_val = data->array_part[r - 1];                                     // get from array
                value_incref(row_val);                                                 // increment ref
                got_row = true;                                                        // found
            }
            
            if (!got_row) {                                                            // not found yet
                Value k_row = MAKE_NUMBER((double)r);                                  // numeric key
                if (table_get(data, k_row, &row_val) && IS_TABLE(row_val)) {           // found
                    got_row = true;                                                    // mark found
                } else {
                    value_decref(row_val);                                             // release if invalid
                    value_decref(k_row);                                               // release key
                }
            }
            
            if (!got_row) {                                                            // not found yet
                char rbuf[32];                                                         // buffer
                snprintf(rbuf, sizeof(rbuf), "%d", r);                                 // format
                Value k_str = MAKE_STRING(string_intern(&vm->intern_table, rbuf, strlen(rbuf)));  // string key
                if (table_get(data, k_str, &row_val) && IS_TABLE(row_val)) {           // found
                    got_row = true;                                                    // mark found
                } else {
                    value_decref(row_val);                                             // release if invalid
                }
                value_decref(k_str);                                                   // release key
            }
            
            if (!got_row) break;                                                       // no more rows
            
            if (written > 0) sb_append(&sb, "\n", 1);                                  // newline between rows
            
            for (int i = 0; i < header_count; i++) {                                   // iterate columns
                if (i > 0) sb_append_char(&sb, delimiter);                             // delimiter
                Value cell_val;                                                        // cell value
                char buf[64];                                                          // buffer for number
                const char* str_val = "";                                              // string value
                
                if (table_get(AS_TABLE(row_val), headers[i], &cell_val)) {             // get cell
                    if (IS_NUMBER(cell_val)) {                                         // number
                        snprintf(buf, sizeof(buf), "%g", AS_NUMBER(cell_val));         // format
                        str_val = buf;                                                 // use buffer
                    } else if (IS_BOOL(cell_val)) {                                    // boolean
                        str_val = AS_BOOL(cell_val) ? "true" : "false";                // bool string
                    } else if (IS_STRING(cell_val)) {                                  // string
                        str_val = AS_STRING(cell_val)->chars;                          // use string
                    }
                    value_decref(cell_val);                                            // release cell
                }
                
                bool needs_quote = strchr(str_val, delimiter) || strchr(str_val, '"') || strchr(str_val, '\n');  // needs quoting
                if (needs_quote) {                                                   // quote field
                    sb_append_char(&sb, '"');                                        // opening quote
                    for (const char* p = str_val; *p; p++) {                         // escape quotes
                        if (*p == '"') sb_append_char(&sb, '"');                     // double quote
                        sb_append_char(&sb, *p);                                     // append char
                    }
                    sb_append_char(&sb, '"');                                        // closing quote
                } else {
                    sb_append(&sb, str_val, strlen(str_val));                        // append value
                }
            }
            
            value_decref(row_val);                                                   // release row
            written++;                                                               // count written
        }

        if (headers) {                                                        // free headers
            for (int i = 0; i < header_count; i++) value_decref(headers[i]);  // release each
            free(headers);                                                    // free array
        }
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length)); // intern result
        sb_free(&sb);                                                         // free builder
        return true;                                                          // builtin handled
    }

    if (strcmp(name, "codecs.xml_read") == 0) {       // parse xml
        if (arg_count < 1 || !IS_STRING(args[0])) {   // validate string
            *result = MAKE_NONE();                    // invalid
            return true;                              // builtin handled
        }
        const char* xml = AS_STRING(args[0])->chars;  // xml string
        XmlParser xp;                                 // xml parser
        xp.p = xml;                                   // set pointer
        
        Value root = xml_parse_element(vm, &xp);      // parse root
        if (IS_TABLE(root)) {                         // valid root
            *result = root;                           // return root
        } else {
            value_decref(root);                       // release invalid
            *result = MAKE_NONE();                    // return none
        }
        return true;                                  // builtin handled
    }

    if (strcmp(name, "codecs.xml_write") == 0) {    // xml serialize
        if (arg_count < 1 || !IS_TABLE(args[0])) {  // validate table
            *result = MAKE_NONE();                  // invalid
            return true;                            // builtin handled
        }
        StringBuilder sb;                           // string builder
        sb_init(&sb, 256);                          // init builder
        xml_write_node(vm, args[0], 0, &sb);        // write xml
        if (sb.length == 0) {                       // empty output
            sb_free(&sb);                           // free builder
            *result = MAKE_NONE();                  // return none
            return true;                            // builtin handled
        }
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length));  // intern result
        sb_free(&sb);  // free builder
        return true;   // builtin handled
    }

    return false;      // not a recognized builtin
}
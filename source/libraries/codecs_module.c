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
    int i = 0, j = 0;
    uint32_t buffer = 0;
    int bits_left = 0;
    
    while (i < len) {
        buffer = (buffer << 8) | data[i++];
        bits_left += 8;
        while (bits_left >= 6) {
            int char_idx = (buffer >> (bits_left - 6)) & 0x3F;
            out[j++] = b64_chars[char_idx];
            bits_left -= 6;
        }
    }
    
    if (bits_left > 0) {
        int char_idx = (buffer << (6 - bits_left)) & 0x3F;
        out[j++] = b64_chars[char_idx];
    }
    
    while (j % 4 != 0) {
        out[j++] = '=';
    }
    out[j] = '\0';
}

// decodes a single base64 character, returns -1 on invalid
static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return 0;
    return -1;
}

// decodes a base64 string into binary data
static bool base64_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);
    if (len == 0) { *out_len = 0; return true; }
    
    for (int i = 0; i < len; i++) {
        if (str[i] != '=' && base64_decode_char(str[i]) < 0) return false;
    }
    
    uint32_t buffer = 0;
    int bits_left = 0;
    *out_len = 0;
    int padding = 0;
    
    for (int i = 0; i < len; i++) {
        if (str[i] == '=') { 
            padding++; 
            continue; 
        }
        int val = base64_decode_char(str[i]);
        if (val < 0) return false;
        
        buffer = (buffer << 6) | val;
        bits_left += 6;
        
        if (bits_left >= 8) {
            out[(*out_len)++] = (unsigned char)(buffer >> (bits_left - 8));
            bits_left -= 8;
        }
    }
    
    if (padding > 0) {
        int eq_pos = -1;
        for (int i = len - 1; i >= 0; i--) {
            if (str[i] == '=') eq_pos = i;
            else break;
        }
        if (eq_pos > 0 && str[eq_pos - 1] != '=') {
            return false;
        }
        if ((len - padding) % 4 != 0) {
            return false;
        }
        if (padding > 2) return false;
        *out_len -= padding;
        if (*out_len < 0) *out_len = 0;
    } else {
        if (len % 4 != 0) return false;
    }
    
    return true;
}

// url-safe base64 character set (uses - and _ instead of + and /)
static const char b64url_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";

// encodes binary data to url-safe base64 without padding
static void base64url_encode(const unsigned char* data, int len, char* out) {
    int i = 0, j = 0;
    uint32_t buffer = 0;
    int bits_left = 0;
    
    while (i < len) {
        buffer = (buffer << 8) | data[i++];
        bits_left += 8;
        while (bits_left >= 6) {
            int char_idx = (buffer >> (bits_left - 6)) & 0x3F;
            out[j++] = b64url_chars[char_idx];
            bits_left -= 6;
        }
    }
    
    if (bits_left > 0) {
        int char_idx = (buffer << (6 - bits_left)) & 0x3F;
        out[j++] = b64url_chars[char_idx];
    }
    
    out[j] = '\0';
}

// decodes a url-safe base64 character
static int base64url_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    if (c == '=') return 0;
    return -1;
}

// decodes a url-safe base64 string
static bool base64url_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);
    if (len == 0) {
        *out_len = 0;
        return true;
    }
    
    uint32_t buffer = 0;
    int bits_left = 0;
    *out_len = 0;
    
    for (int i = 0; i < len; i++) {
        if (str[i] == '=') break;
        int val = base64url_decode_char(str[i]);
        if (val < 0) return false;
        
        buffer = (buffer << 6) | val;
        bits_left += 6;
        
        if (bits_left >= 8) {
            out[(*out_len)++] = (unsigned char)(buffer >> (bits_left - 8));
            bits_left -= 8;
        }
    }
    return true;
}

// dynamic string builder for efficient text assembly
typedef struct {
    char* buffer;
    int length;
    int capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb, int initial_capacity) {
    sb->capacity = initial_capacity > 16 ? initial_capacity : 16;
    sb->buffer = (char*)malloc(sb->capacity);
    if (!sb->buffer) { sb->length = 0; return; }
    sb->length = 0;
    sb->buffer[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str, int len) {
    if (!sb->buffer) return;
    if (sb->length + len + 1 > sb->capacity) {
        int new_cap = (sb->length + len + 1) * 2;
        char* new_buf = (char*)realloc(sb->buffer, new_cap);
        if (!new_buf) return;
        sb->buffer = new_buf;
        sb->capacity = new_cap;
    }
    memcpy(sb->buffer + sb->length, str, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0';
}

static void sb_append_char(StringBuilder* sb, char c) {
    if (!sb->buffer) return;
    if (sb->length + 2 > sb->capacity) {
        int new_cap = (sb->length + 2) * 2;
        char* new_buf = (char*)realloc(sb->buffer, new_cap);
        if (!new_buf) return;
        sb->buffer = new_buf;
        sb->capacity = new_cap;
    }
    sb->buffer[sb->length++] = c;
    sb->buffer[sb->length] = '\0';
}

static void sb_free(StringBuilder* sb) {
    if (sb->buffer) free(sb->buffer);
    sb->buffer = NULL;
    sb->length = 0;
}

// skips whitespace in a json string
static void skip_ws(const char** s) {
    while (**s && isspace((unsigned char)**s)) (*s)++;
}

// parses a json string with escape sequence handling
static bool parse_string_raw(const char** s, char** out_str, int* out_len) {
    if (**s != '"') return false;
    (*s)++;
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
                case 'u': buffer[idx++] = '?'; p += 4; break;
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
    (*s)++;
    return true;
}

// parses a json number
static bool parse_number(const char** s, Value* out_value) {
    char* endptr;
    double val = strtod(*s, &endptr);
    if (endptr == *s) return false;
    *out_value = MAKE_NUMBER(val);
    *s = endptr;
    return true;
}

// recursive json parser that builds vm values
static bool json_parse_value(VM* vm, const char** json_str, Value* out_value) {
    (void)vm;
    skip_ws(json_str);
    if (!**json_str) return false;
    
    char c = **json_str;
    
    if (strncmp(*json_str, "null", 4) == 0) {
        *out_value = MAKE_BOOL(false);
        *json_str += 4;
        return true;
    }
    if (strncmp(*json_str, "true", 4) == 0) {
        *out_value = MAKE_BOOL(true);
        *json_str += 4;
        return true;
    }
    if (strncmp(*json_str, "false", 5) == 0) {
        *out_value = MAKE_BOOL(false);
        *json_str += 5;
        return true;
    }
    
    if (c == '"') {
        char* str_val = NULL;
        int len = 0;
        if (!parse_string_raw(json_str, &str_val, &len)) return false;
        StringObject* interned = string_intern(&vm->intern_table, str_val, len);
        *out_value = MAKE_STRING(interned);
        free(str_val);
        return true;
    }
    
    if (c == '-' || isdigit(c)) {
        return parse_number(json_str, out_value);
    }
    
    if (c == '[') {
        (*json_str)++;
        Table* table = table_create(8);
        *out_value = MAKE_TABLE(table);
        skip_ws(json_str);
        int index = 1;
        if (**json_str != ']') {
            while (1) {
                Value item;
                if (!json_parse_value(vm, json_str, &item)) {
                    value_decref(*out_value);
                    return false;
                }
                Value k = MAKE_NUMBER((double)index++);
                table_set(table, k, item);
                value_decref(item);
                skip_ws(json_str);
                if (**json_str == ',') {
                    (*json_str)++;
                } else {
                    break;
                }
            }
        }
        if (**json_str != ']') {
            value_decref(*out_value);
            return false;
        }
        (*json_str)++;
        return true;
    }
    
    if (c == '{') {
        (*json_str)++;
        Table* table = table_create(8);
        *out_value = MAKE_TABLE(table);
        skip_ws(json_str);
        if (**json_str != '}') {
            while (1) {
                skip_ws(json_str);
                if (**json_str != '"') {
                    value_decref(*out_value);
                    return false;
                }
                char* key_str = NULL;
                int key_len = 0;
                if (!parse_string_raw(json_str, &key_str, &key_len)) {
                    value_decref(*out_value);
                    return false;
                }
                skip_ws(json_str);
                if (**json_str != ':') {
                    free(key_str);
                    value_decref(*out_value);
                    return false;
                }
                (*json_str)++;
                Value val;
                if (!json_parse_value(vm, json_str, &val)) {
                    free(key_str);
                    value_decref(*out_value);
                    return false;
                }
                Value k = MAKE_STRING(string_intern(&vm->intern_table, key_str, key_len));
                table_set(table, k, val);
                value_decref(k);
                value_decref(val);
                free(key_str);
                skip_ws(json_str);
                if (**json_str == ',') {
                    (*json_str)++;
                } else {
                    break;
                }
            }
        }
        if (**json_str != '}') {
            value_decref(*out_value);
            return false;
        }
        (*json_str)++;
        return true;
    }
    
    return false;
}

// appends a json-escaped string to the builder
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

// recursively encodes a vm value to json
static void json_encode_value(VM* vm, Value value, StringBuilder* sb) {
    (void)vm;
    
    if (IS_NUMBER(value)) {
        char buf[64];
        double num = AS_NUMBER(value);
        if (fabs(num - (long long)num) < 1e-9 && fabs(num) < 1e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)num);
        } else {
            snprintf(buf, sizeof(buf), "%.15g", num);
        }
        sb_append(sb, buf, (int)strlen(buf));
    } else if (IS_BOOL(value)) {
        sb_append(sb, AS_BOOL(value) ? "true" : "false", AS_BOOL(value) ? 4 : 5);
    } else if (IS_STRING(value)) {
        append_escaped(sb, AS_STRING(value)->chars);
    } else if (IS_TABLE(value)) {
        Table* t = AS_TABLE(value);
        if (TABLE_TOTAL_COUNT(t) == 0) {
            sb_append(sb, "{}", 2);
            return;
        }
        
        bool is_array = (t->array_count > 0 && t->hash_count == 0);
        
        if (is_array) {
            sb_append(sb, "[", 1);
            for (int i = 0; i < t->array_count; i++) {
                if (i > 0) sb_append(sb, ", ", 2);
                json_encode_value(vm, t->array_part[i], sb);
            }
            sb_append(sb, "]", 1);
        } else {
            sb_append(sb, "{", 1);
            bool first = true;
            
            for (int i = 0; i < t->array_count; i++) {
                if (!IS_BOOL(t->array_part[i]) || AS_BOOL(t->array_part[i])) {
                    if (!first) sb_append(sb, ", ", 2);
                    first = false;
                    char key[32];
                    snprintf(key, sizeof(key), "%d", i + 1);
                    append_escaped(sb, key);
                    sb_append(sb, ": ", 2);
                    json_encode_value(vm, t->array_part[i], sb);
                }
            }
            
            for (int i = 0; i < t->capacity; i++) {
                TableEntry* entry = t->entries[i];
                while (entry) {
                    if (!first) sb_append(sb, ", ", 2);
                    first = false;
                    if (IS_STRING(entry->key)) {
                        append_escaped(sb, AS_STRING(entry->key)->chars);
                    } else if (IS_NUMBER(entry->key)) {
                        char num_buf[64];
                        snprintf(num_buf, sizeof(num_buf), "%g", AS_NUMBER(entry->key));
                        append_escaped(sb, num_buf);
                    }
                    sb_append(sb, ": ", 2);
                    json_encode_value(vm, entry->value, sb);
                    entry = entry->next;
                }
            }
            sb_append(sb, "}", 1);
        }
    } else {
        sb_append(sb, "null", 4);
    }
}

// csv parser state with position and delimiter
typedef struct {
    const char* data;
    int pos;
    int len;
    char delimiter;
} CsvParser;

static char csv_peek(CsvParser* p) {
    if (!p->data || p->pos >= p->len) return '\0';
    return p->data[p->pos];
}

static char csv_advance(CsvParser* p) {
    if (!p->data || p->pos >= p->len) return '\0';
    return p->data[p->pos++];
}

static bool csv_has_next(CsvParser* p) {
    return p->data && p->pos < p->len;
}

// checks if a string looks like a number
static bool is_numeric(const char* str) {
    if (!str || *str == '\0') return false;
    char* endptr;
    strtod(str, &endptr);
    return *endptr == '\0';
}

// checks if a string is a boolean literal
static bool is_bool(const char* str) {
    return (strcmp(str, "true") == 0 || strcmp(str, "false") == 0);
}

// parses a csv field into a typed vm value
static Value csv_parse_value(VM* vm, const char* str) {
    if (!str) return MAKE_NONE();
    if (is_numeric(str)) {
        return MAKE_NUMBER(atof(str));
    }
    if (is_bool(str)) {
        return MAKE_BOOL(strcmp(str, "true") == 0);
    }
    return MAKE_STRING(string_intern(&vm->intern_table, str, strlen(str)));
}

// extracts a single csv field with quoted field support
static char* csv_parse_field(CsvParser* p) {
    StringBuilder sb;
    sb_init(&sb, 64);
    
    if (!p->data) {
        sb_free(&sb);
        return strdup("");
    }
    
    char c = csv_peek(p);
    if (c == '"') {
        csv_advance(p);
        while (csv_has_next(p)) {
            c = csv_advance(p);
            if (c == '"') {
                if (csv_peek(p) == '"') {
                    sb_append_char(&sb, '"');
                    csv_advance(p);
                } else {
                    break;
                }
            } else {
                sb_append_char(&sb, c);
            }
        }
        if (csv_peek(p) == p->delimiter) csv_advance(p);
        else if (csv_peek(p) == '\r') {
            csv_advance(p);
            if (csv_peek(p) == '\n') csv_advance(p);
        } else if (csv_peek(p) == '\n') {
            csv_advance(p);
        }
    } else {
        while (csv_has_next(p)) {
            c = csv_peek(p);
            if (c == p->delimiter || c == '\n' || c == '\r') {
                break;
            }
            sb_append_char(&sb, csv_advance(p));
        }
        if (csv_peek(p) == p->delimiter) csv_advance(p);
        else if (csv_peek(p) == '\r') {
            csv_advance(p);
            if (csv_peek(p) == '\n') csv_advance(p);
        } else if (csv_peek(p) == '\n') {
            csv_advance(p);
        }
    }
    
    char* result = sb.buffer ? strdup(sb.buffer) : strdup("");
    sb_free(&sb);
    return result;
}

// xml parser state
typedef struct {
    const char* p;
} XmlParser;

// skips whitespace in xml
static void xml_skip_ws(XmlParser* xp) {
    while (*xp->p && isspace((unsigned char)*xp->p)) xp->p++;
}

// parses xml attributes into a table with @ prefix
static void xml_parse_attrs(VM* vm, XmlParser* xp, Table* t) {
    xml_skip_ws(xp);
    while (*xp->p && *xp->p != '>' && *xp->p != '/') {
        char key[128] = {0};
        int ki = 0;
        while (*xp->p && (isalnum((unsigned char)*xp->p) || *xp->p == '_' || *xp->p == '-') && ki < 127) {
            key[ki++] = *xp->p++;
        }
        key[ki] = '\0';
        if (!*key) break;
        
        xml_skip_ws(xp);
        if (*xp->p != '=') break;
        xp->p++;
        xml_skip_ws(xp);
        if (*xp->p != '"') break;
        xp->p++;
        
        StringBuilder val_sb;
        sb_init(&val_sb, 64);
        while (*xp->p && *xp->p != '"') {
            sb_append_char(&val_sb, *xp->p);
            xp->p++;
        }
        if (*xp->p == '"') xp->p++;
        
        char attr_key_buf[150];
        snprintf(attr_key_buf, sizeof(attr_key_buf), "@%s", key);
        Value k = MAKE_STRING(string_intern(&vm->intern_table, attr_key_buf, strlen(attr_key_buf)));
        Value v = MAKE_STRING(string_intern(&vm->intern_table, val_sb.buffer, val_sb.length));
        table_set(t, k, v);
        value_decref(k);
        value_decref(v);
        sb_free(&val_sb);
        
        xml_skip_ws(xp);
    }
}

// parses a single xml element recursively
static Value xml_parse_element(VM* vm, XmlParser* xp) {
    xml_skip_ws(xp);
    if (*xp->p != '<') return MAKE_NONE();
    xp->p++;
    
    char tag[128] = {0};
    int ti = 0;
    while (*xp->p && (isalnum((unsigned char)*xp->p) || *xp->p == '_' || *xp->p == '-') && ti < 127) {
        tag[ti++] = *xp->p++;
    }
    tag[ti] = '\0';
    if (!*tag) return MAKE_NONE();
    
    Table* elem_table = table_create(8);
    Value elem = MAKE_TABLE(elem_table);
    
    Value k_tag = MAKE_STRING(string_intern(&vm->intern_table, "__tag", 5));
    Value v_tag = MAKE_STRING(string_intern(&vm->intern_table, tag, strlen(tag)));
    table_set(elem_table, k_tag, v_tag);
    value_decref(k_tag);
    value_decref(v_tag);
    
    xml_parse_attrs(vm, xp, elem_table);
    
    xml_skip_ws(xp);
    bool self_closing = false;
    if (*xp->p == '/') {
        self_closing = true;
        xp->p++;
    }
    if (*xp->p == '>') {
        xp->p++;
        if (self_closing) return elem;
        
        int index = 1;
        
        while (*xp->p) {
            if (*xp->p == '<') {
                if (*(xp->p + 1) == '/') {
                    xp->p += 2;
                    char close_tag[128] = {0};
                    int ci = 0;
                    while (*xp->p && *xp->p != '>' && ci < 127) {
                        close_tag[ci++] = *xp->p++;
                    }
                    close_tag[ci] = '\0';
                    if (strcmp(tag, close_tag) != 0) {
                        value_decref(elem);
                        return MAKE_NONE();
                    }
                    if (*xp->p == '>') xp->p++;
                    return elem;
                } else {
                    Value child = xml_parse_element(vm, xp);
                    if (IS_NONE(child)) {
                        value_decref(elem);
                        return MAKE_NONE();
                    }
                    if (IS_TABLE(child)) {
                        Value k = MAKE_NUMBER((double)index++);
                        table_set(elem_table, k, child);
                        value_decref(k);
                    }
                    value_decref(child);
                }
            } else {
                StringBuilder text_sb;
                sb_init(&text_sb, 64);
                while (*xp->p && *xp->p != '<') {
                    sb_append_char(&text_sb, *xp->p);
                    xp->p++;
                }
                if (text_sb.length > 0) {
                    Value k_text = MAKE_STRING(string_intern(&vm->intern_table, "#text", 5));
                    Value v_text = MAKE_STRING(string_intern(&vm->intern_table, text_sb.buffer, text_sb.length));
                    table_set(elem_table, k_text, v_text);
                    value_decref(k_text);
                    value_decref(v_text);
                }
                sb_free(&text_sb);
            }
            xml_skip_ws(xp);
        }
        
        value_decref(elem);
        return MAKE_NONE();
    }
    value_decref(elem);
    return MAKE_NONE();
}

// recursively writes an xml node from a vm table
static void xml_write_node(VM* vm, Value v, int depth, StringBuilder* sb) {
    if (!IS_TABLE(v)) return;
    
    Table* table = AS_TABLE(v);
    
    Value k_tag = MAKE_STRING(string_intern(&vm->intern_table, "__tag", 5));
    Value tag_val;
    if (!table_get(table, k_tag, &tag_val) || !IS_STRING(tag_val)) {
        value_decref(k_tag);
        return;
    }
    value_decref(k_tag);
    
    for (int i = 0; i < depth; i++) sb_append(sb, "  ", 2);
    sb_append(sb, "<", 1);
    StringObject* tag_str = AS_STRING(tag_val);
    sb_append(sb, tag_str->chars, tag_str->length);
    value_decref(tag_val);
    
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* e = table->entries[i];
        while (e) {
            if (IS_STRING(e->key)) {
                StringObject* key_str = AS_STRING(e->key);
                if (key_str->chars[0] == '@' && IS_STRING(e->value)) {
                    sb_append(sb, " ", 1);
                    sb_append(sb, key_str->chars + 1, key_str->length - 1);
                    sb_append(sb, "=\"", 2);
                    StringObject* val_str = AS_STRING(e->value);
                    sb_append(sb, val_str->chars, val_str->length);
                    sb_append(sb, "\"", 1);
                }
            }
            e = e->next;
        }
    }
    
    bool has_children = false;
    Value k_text = MAKE_STRING(string_intern(&vm->intern_table, "#text", 5));
    Value text_val;
    bool has_text = table_get(table, k_text, &text_val);
    value_decref(k_text);
    
    for (int i = 1; ; i++) {
        Value k = MAKE_NUMBER((double)i);
        Value child;
        if (table_get(table, k, &child)) {
            has_children = true;
            value_decref(child);
            value_decref(k);
        } else {
            value_decref(k);
            break;
        }
    }
    
    if (has_children || has_text) {
        sb_append(sb, ">", 1);
        if (has_text) {
            if (IS_STRING(text_val)) {
                StringObject* text_str = AS_STRING(text_val);
                sb_append(sb, text_str->chars, text_str->length);
            }
            value_decref(text_val);
        }
        if (has_children) sb_append(sb, "\n", 1);
        
        for (int i = 1; ; i++) {
            Value k = MAKE_NUMBER((double)i);
            Value child;
            if (table_get(table, k, &child)) {
                value_decref(k);
                xml_write_node(vm, child, depth + 1, sb);
                value_decref(child);
            } else {
                value_decref(k);
                break;
            }
        }
        
        for (int i = 0; i < depth; i++) sb_append(sb, "  ", 2);
        sb_append(sb, "</", 2);
        Value k_close = MAKE_STRING(string_intern(&vm->intern_table, "__tag", 5));
        if (table_get(table, k_close, &tag_val)) {
            value_decref(k_close);
            if (IS_STRING(tag_val)) {
                StringObject* close_str = AS_STRING(tag_val);
                sb_append(sb, close_str->chars, close_str->length);
            }
            value_decref(tag_val);
        } else {
            value_decref(k_close);
        }
        sb_append(sb, ">\n", 2);
    } else {
        sb_append(sb, "/>\n", 3);
    }
}

// main dispatcher for all codecs module built-in functions
bool codecs_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "codecs.base_write") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        StringObject* input_str = AS_STRING(args[0]);
        int input_len = input_str->length;
        int out_size = ((input_len + 2) / 3) * 4 + 1;
        char* out = (char*)malloc(out_size);
        if (!out) { *result = MAKE_NONE(); return true; }
        base64_encode((const unsigned char*)input_str->chars, input_len, out);
        *result = MAKE_STRING(string_intern(&vm->intern_table, out, strlen(out)));
        free(out);
        return true;
    }
    
    if (strcmp(name, "codecs.base_read") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        StringObject* input_str = AS_STRING(args[0]);
        int input_len = input_str->length;
        unsigned char* out = (unsigned char*)malloc(input_len + 1);
        if (!out) { *result = MAKE_NONE(); return true; }
        int out_len = 0;
        if (base64_decode(input_str->chars, out, &out_len)) {
            out[out_len] = '\0';
            *result = MAKE_STRING(string_intern(&vm->intern_table, (char*)out, out_len));
        } else {
            *result = MAKE_NONE();
        }
        free(out);
        return true;
    }

    if (strcmp(name, "codecs.baseurl_write") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        StringObject* input_str = AS_STRING(args[0]);
        int input_len = input_str->length;
        int out_size = ((input_len + 2) / 3) * 4 + 1;
        char* out = (char*)malloc(out_size);
        if (!out) { *result = MAKE_NONE(); return true; }
        base64url_encode((const unsigned char*)input_str->chars, input_len, out);
        *result = MAKE_STRING(string_intern(&vm->intern_table, out, strlen(out)));
        free(out);
        return true;
    }
    
    if (strcmp(name, "codecs.baseurl_read") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        StringObject* input_str = AS_STRING(args[0]);
        int input_len = input_str->length;
        unsigned char* out = (unsigned char*)malloc(input_len + 1);
        if (!out) { *result = MAKE_NONE(); return true; }
        int out_len = 0;
        if (base64url_decode(input_str->chars, out, &out_len)) {
            out[out_len] = '\0';
            *result = MAKE_STRING(string_intern(&vm->intern_table, (char*)out, out_len));
        } else {
            *result = MAKE_NONE();
        }
        free(out);
        return true;
    }

    if (strcmp(name, "codecs.json_read") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        const char* json_str = AS_STRING(args[0])->chars;
        if (json_parse_value(vm, &json_str, result)) {
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "codecs.json_write") == 0) {
        if (arg_count < 1) {
            *result = MAKE_NONE();
            return true;
        }
        StringBuilder sb;
        sb_init(&sb, 256);
        json_encode_value(vm, args[0], &sb);
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length));
        sb_free(&sb);
        return true;
    }

    if (strcmp(name, "codecs.csv_read") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        StringObject* input_str = AS_STRING(args[0]);
        const char* data = input_str->chars;
        int len = input_str->length;
        bool has_header = true;
        char delimiter = ',';
        
        if (arg_count >= 2 && IS_BOOL(args[1])) has_header = AS_BOOL(args[1]);
        if (arg_count >= 3 && IS_STRING(args[2]) && AS_STRING(args[2])->length > 0)
            delimiter = AS_STRING(args[2])->chars[0];
            
        if (len <= 0) {
            *result = MAKE_NONE();
            return true;
        }
        
        CsvParser parser = { .data = data, .pos = 0, .len = len, .delimiter = delimiter };
        Table* table_list_table = table_create(8);
        Value table_list = MAKE_TABLE(table_list_table);
        char** headers = NULL;
        int col_count = 0;
        
        if (has_header && parser.len > 0) {
            int temp_pos = parser.pos;
            int count = 0;
            while (parser.pos < parser.len && parser.data[parser.pos] != '\n' && parser.data[parser.pos] != '\r') {
                if (parser.data[parser.pos] == delimiter) count++;
                parser.pos++;
            }
            col_count = count + 1;
            parser.pos = temp_pos;
            
            if (col_count > 0) {
                headers = (char**)malloc(sizeof(char*) * col_count);
                if (headers) {
                    for (int i = 0; i < col_count; i++) headers[i] = csv_parse_field(&parser);
                }
            }
        }
        
        int row_index = 1;
        while (parser.pos < parser.len) {
            if (parser.data[parser.pos] == '\n' || parser.data[parser.pos] == '\r') {
                if (parser.data[parser.pos] == '\r') parser.pos++;
                if (parser.pos < parser.len && parser.data[parser.pos] == '\n') parser.pos++;
                continue;
            }
            
            Table* row_table = table_create(8);
            Value row_table_val = MAKE_TABLE(row_table);
            if (has_header && headers && col_count > 0) {
                for (int i = 0; i < col_count; i++) {
                    if (!csv_has_next(&parser)) break;
                    char* field = csv_parse_field(&parser);
                    Value val = csv_parse_value(vm, field);
                    if (headers[i]) {
                        Value k = MAKE_STRING(string_intern(&vm->intern_table, headers[i], strlen(headers[i])));
                        table_set(row_table, k, val);
                        value_decref(k);
                    }
                    value_decref(val);
                    free(field);
                }
            } else {
                int idx = 0;
                while (csv_has_next(&parser)) {
                    Value k = MAKE_NUMBER((double)(idx + 1));
                    char* field = csv_parse_field(&parser);
                    Value val = csv_parse_value(vm, field);
                    table_set(row_table, k, val);
                    value_decref(k);
                    value_decref(val);
                    free(field);
                    idx++;
                }
            }
            
            Value k_idx = MAKE_NUMBER((double)row_index++);
            table_set(table_list_table, k_idx, row_table_val);
            value_decref(k_idx);
            value_decref(row_table_val);
        }
        
        if (headers) {
            for (int i = 0; i < col_count; i++) free(headers[i]);
            free(headers);
        }
        *result = table_list;
        return true;
    }
    
    if (strcmp(name, "codecs.csv_write") == 0) {
        if (arg_count < 1 || !IS_TABLE(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        Table* data = AS_TABLE(args[0]);
        bool has_header = true;
        char delimiter = ',';
        
        if (arg_count >= 2 && IS_BOOL(args[1])) has_header = AS_BOOL(args[1]);
        if (arg_count >= 3 && IS_STRING(args[2]) && AS_STRING(args[2])->length > 0)
            delimiter = AS_STRING(args[2])->chars[0];
            
        int row_count = table_size(data);
        if (row_count == 0) {
            *result = MAKE_STRING(string_intern(&vm->intern_table, "", 0));
            return true;
        }
        
        Value k_first = MAKE_NUMBER(1.0);
        Value first_row_val;
        if (!table_get(data, k_first, &first_row_val) || !IS_TABLE(first_row_val)) {
            value_decref(k_first);
            *result = MAKE_NONE();
            return true;
        }
        value_decref(k_first);
        
        int header_count = 0;
        Value* headers = table_keys(AS_TABLE(first_row_val), &header_count);
        value_decref(first_row_val);
        
        StringBuilder sb;
        sb_init(&sb, 256);
        
        if (has_header && headers && header_count > 0) {
            for (int i = 0; i < header_count; i++) {
                if (i > 0) sb_append_char(&sb, delimiter);
                const char* h = "";
                char h_buf[64];
                if (IS_STRING(headers[i])) h = AS_STRING(headers[i])->chars;
                else if (IS_NUMBER(headers[i])) {
                    snprintf(h_buf, sizeof(h_buf), "%g", AS_NUMBER(headers[i]));
                    h = h_buf;
                }
                bool needs_quote = strchr(h, delimiter) || strchr(h, '"') || strchr(h, '\n');
                if (needs_quote) {
                    sb_append_char(&sb, '"');
                    for (const char* p = h; *p; p++) {
                        if (*p == '"') sb_append_char(&sb, '"');
                        sb_append_char(&sb, *p);
                    }
                    sb_append_char(&sb, '"');
                } else {
                    sb_append(&sb, h, strlen(h));
                }
            }
            sb_append(&sb, "\n", 1);
        }
        
        for (int r = 1; r <= row_count; r++) {
            Value k_row = MAKE_NUMBER((double)r);
            Value row_val;
            if (!table_get(data, k_row, &row_val) || !IS_TABLE(row_val)) {
                value_decref(k_row);
                continue;
            }
            
            for (int i = 0; i < header_count; i++) {
                if (i > 0) sb_append_char(&sb, delimiter);
                Value cell_val;
                char buf[64];
                const char* str_val = "";
                
                if (table_get(AS_TABLE(row_val), headers[i], &cell_val)) {
                    if (IS_NUMBER(cell_val)) { snprintf(buf, sizeof(buf), "%g", AS_NUMBER(cell_val)); str_val = buf; }
                    else if (IS_BOOL(cell_val)) str_val = AS_BOOL(cell_val) ? "true" : "false";
                    else if (IS_STRING(cell_val)) str_val = AS_STRING(cell_val)->chars;
                    value_decref(cell_val);
                }
                
                bool needs_quote = strchr(str_val, delimiter) || strchr(str_val, '"') || strchr(str_val, '\n');
                if (needs_quote) {
                    sb_append_char(&sb, '"');
                    for (const char* p = str_val; *p; p++) {
                        if (*p == '"') sb_append_char(&sb, '"');
                        sb_append_char(&sb, *p);
                    }
                    sb_append_char(&sb, '"');
                } else {
                    sb_append(&sb, str_val, strlen(str_val));
                }
            }
            if (r < row_count) sb_append(&sb, "\n", 1);
            value_decref(row_val);
            value_decref(k_row);
        }

        if (headers) {
            for (int i = 0; i < header_count; i++) value_decref(headers[i]);
            free(headers);
        }
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length));
        sb_free(&sb);
        return true;
    }

    if (strcmp(name, "codecs.xml_read") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        const char* xml = AS_STRING(args[0])->chars;
        XmlParser xp;
        xp.p = xml;
        
        Value root = xml_parse_element(vm, &xp);
        if (IS_TABLE(root)) {
            *result = root;
        } else {
            value_decref(root);
            *result = MAKE_NONE();
        }
        return true;
    }

    if (strcmp(name, "codecs.xml_write") == 0) {
        if (arg_count < 1 || !IS_TABLE(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        StringBuilder sb;
        sb_init(&sb, 256);
        xml_write_node(vm, args[0], 0, &sb);
        if (sb.length == 0) {
            sb_free(&sb);
            *result = MAKE_NONE();
            return true;
        }
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length));
        sb_free(&sb);
        return true;
    }

    return false;
}
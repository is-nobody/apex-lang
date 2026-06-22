#include "codecs_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>

// ========== Helper: Base64 (Standard) ==========
static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

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
    
    // Padding
    while (j % 4 != 0) {
        out[j++] = '=';
    }
    out[j] = '\0';
}

static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return 0;
    return -1;
}

static bool base64_decode(const char* str, unsigned char* out, int* out_len) {
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
        int val = base64_decode_char(str[i]);
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

// ========== Helper: Base64 URL Safe ==========
static const char b64url_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";

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
    
    // No padding for URL safe usually, but keeping consistent with standard logic if needed. 
    // Standard base64url often omits padding. Let's omit it for "url safe" convention unless specified otherwise.
    // However, to keep decode simple and symmetric with standard logic that might expect padding, 
    // we'll leave padding out as is common for URL safe, but decode must handle it.
    out[j] = '\0';
}

static int base64url_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    if (c == '=') return 0;
    return -1;
}

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

// ========== String Builder ==========
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

// ========== Forward Declarations for JSON ==========
static bool json_parse_value(VM* vm, const char** json_str, Value* out_value);
static void json_encode_value(VM* vm, Value* value, StringBuilder* sb);

// ========== Forward Declarations for CSV ==========
typedef struct {
    const char* data;
    int pos;
    int len;
    char delimiter;
} CsvParser;

static char csv_peek(CsvParser* p);
static char csv_advance(CsvParser* p);
static bool csv_has_next(CsvParser* p);
static char* csv_parse_field(CsvParser* p);
static char detect_delimiter(const char* data, int len);
static bool is_numeric(const char* str);
static bool is_bool(const char* str);
static Value csv_parse_value(const char* str);

// ========== JSON Implementation ==========
static void skip_ws(const char** s) {
    while (**s && isspace((unsigned char)**s)) (*s)++;
}

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

static bool parse_number(const char** s, Value* out_value) {
    char* endptr;
    double val = strtod(*s, &endptr);
    if (endptr == *s) return false;
    *out_value = vm_make_number(val);
    *s = endptr;
    return true;
}

static bool json_parse_value(VM* vm, const char** json_str, Value* out_value) {
    skip_ws(json_str);
    if (!**json_str) return false;
    
    char c = **json_str;
    
    if (strncmp(*json_str, "null", 4) == 0) {
        *out_value = vm_make_bool(false);
        *json_str += 4;
        return true;
    }
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
    
    if (c == '"') {
        char* str_val = NULL;
        int len = 0;
        if (!parse_string_raw(json_str, &str_val, &len)) return false;
        *out_value = vm_make_string(str_val);
        free(str_val);
        return true;
    }
    
    if (c == '-' || isdigit(c)) {
        return parse_number(json_str, out_value);
    }
    
    if (c == '[') {
        (*json_str)++;
        *out_value = vm_make_table();
        skip_ws(json_str);
        int index = 1;
        if (**json_str != ']') {
            while (1) {
                Value item;
                if (!json_parse_value(vm, json_str, &item)) return false;
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
                if (!json_parse_value(vm, json_str, &val)) {
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

static void json_encode_value(VM* vm, Value* value, StringBuilder* sb) {
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
            
            bool is_array = true;
            int count = 0;
            for (int i = 0; i < t->capacity; i++) {
                TableEntry* entry = t->entries[i];
                while (entry) {
                    count++;
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
                        json_encode_value(vm, &val, sb);
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
                        json_encode_value(vm, &entry->value, sb);
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

// ========== CSV Implementation ==========
static bool is_numeric(const char* str) {
    if (!str || *str == '\0') return false;
    char* endptr;
    strtod(str, &endptr);
    return *endptr == '\0';
}

static bool is_bool(const char* str) {
    return (strcmp(str, "true") == 0 || strcmp(str, "false") == 0);
}

static Value csv_parse_value(const char* str) {
    if (!str) return vm_make_bool(false);
    if (is_numeric(str)) {
        return vm_make_number(atof(str));
    }
    if (is_bool(str)) {
        return vm_make_bool(strcmp(str, "true") == 0);
    }
    return vm_make_string(str);
}

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

static char detect_delimiter(const char* data, int len) {
    if (!data || len <= 0) return ',';
    
    int commas = 0, semicolons = 0, tabs = 0;
    int limit = len < 1024 ? len : 1024;
    
    for (int i = 0; i < limit; i++) {
        if (data[i] == ',') commas++;
        else if (data[i] == ';') semicolons++;
        else if (data[i] == '\t') tabs++;
    }
    
    if (tabs > commas && tabs > semicolons) return '\t';
    if (semicolons > commas) return ';';
    return ',';
}

// ========== XML Helpers (Self-made) ==========
typedef struct {
    const char* p;
} XmlParser;

static void xml_skip_ws(XmlParser* xp) {
    while (*xp->p && isspace((unsigned char)*xp->p)) xp->p++;
}

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
        xp->p++; // skip =
        xml_skip_ws(xp);
        if (*xp->p != '"') break;
        xp->p++; // skip "
        
        StringBuilder val_sb;
        sb_init(&val_sb, 64);
        while (*xp->p && *xp->p != '"') {
            sb_append_char(&val_sb, *xp->p);
            xp->p++;
        }
        if (*xp->p == '"') xp->p++;
        
        char attr_key[150];
        snprintf(attr_key, sizeof(attr_key), "@%s", key);
        table_set(t, attr_key, vm_make_string(val_sb.buffer));
        sb_free(&val_sb);
        
        xml_skip_ws(xp);
    }
}

static Value xml_parse_element(VM* vm, XmlParser* xp);

static Value xml_parse_element(VM* vm, XmlParser* xp) {
    xml_skip_ws(xp);
    if (*xp->p != '<') return vm_make_bool(false);
    xp->p++; // skip <
    
    char tag[128] = {0};
    int ti = 0;
    while (*xp->p && (isalnum((unsigned char)*xp->p) || *xp->p == '_' || *xp->p == '-') && ti < 127) {
        tag[ti++] = *xp->p++;
    }
    tag[ti] = '\0';
    if (!*tag) return vm_make_bool(false);
    
    Value elem = vm_make_table();
    table_set(elem.table, "__tag", vm_make_string(tag));
    
    xml_parse_attrs(vm, xp, elem.table);
    
    xml_skip_ws(xp);
    bool self_closing = false;
    if (*xp->p == '/') {
        self_closing = true;
        xp->p++;
    }
    if (*xp->p == '>') {
        xp->p++;
        if (self_closing) return elem;
        
        // Parse children
        int index = 1;
        while (*xp->p) {
            if (*xp->p == '<') {
                if (*(xp->p + 1) == '/') {
                    // End tag
                    xp->p += 2;
                    while (*xp->p && *xp->p != '>') xp->p++;
                    if (*xp->p == '>') xp->p++;
                    break;
                } else {
                    Value child = xml_parse_element(vm, xp);
                    if (child.type == VAL_TABLE) {
                        char k[32];
                        snprintf(k, sizeof(k), "%d", index++);
                        table_set(elem.table, k, child);
                        value_decref(&child);
                    }
                }
            } else {
                // Text content
                StringBuilder text_sb;
                sb_init(&text_sb, 64);
                while (*xp->p && *xp->p != '<') {
                    sb_append_char(&text_sb, *xp->p);
                    xp->p++;
                }
                if (text_sb.length > 0) {
                    table_set(elem.table, "#text", vm_make_string(text_sb.buffer));
                }
                sb_free(&text_sb);
            }
            xml_skip_ws(xp);
        }
        return elem;
    }
    value_decref(&elem);
    return vm_make_bool(false);
}

// XML Write Helper
static void xml_write_node(VM* vm, Value v, int depth, StringBuilder* sb) {
    if (v.type != VAL_TABLE) return;
    
    Value tag_val;
    if (!table_get(v.table, "__tag", &tag_val) || tag_val.type != VAL_STRING) {
        value_decref(&tag_val);
        return;
    }
    
    for (int i = 0; i < depth; i++) sb_append(sb, "  ", 2);
    sb_append(sb, "<", 1);
    sb_append(sb, tag_val.string->chars, tag_val.string->length);
    value_decref(&tag_val);
    
    // Write attributes
    for (int i = 0; i < v.table->capacity; i++) {
        TableEntry* e = v.table->entries[i];
        while (e) {
            if (e->key[0] == '@' && e->value.type == VAL_STRING) {
                sb_append(sb, " ", 1);
                sb_append(sb, e->key + 1, strlen(e->key + 1));
                sb_append(sb, "=\"", 2);
                sb_append(sb, e->value.string->chars, e->value.string->length);
                sb_append(sb, "\"", 1);
            }
            e = e->next;
        }
    }
    
    // Check for children or text
    bool has_children = false;
    Value text_val;
    bool has_text = table_get(v.table, "#text", &text_val);
    
    for (int i = 1; ; i++) {
        char k[32];
        snprintf(k, sizeof(k), "%d", i);
        Value child;
        if (table_get(v.table, k, &child)) {
            has_children = true;
            value_decref(&child);
        } else {
            break;
        }
    }
    
    if (has_children || has_text) {
        sb_append(sb, ">", 1);
        if (has_text) {
            sb_append(sb, text_val.string->chars, text_val.string->length);
            value_decref(&text_val);
        }
        if (has_children) sb_append(sb, "\n", 1);
        
        for (int i = 1; ; i++) {
            char k[32];
            snprintf(k, sizeof(k), "%d", i);
            Value child;
            if (table_get(v.table, k, &child)) {
                xml_write_node(vm, child, depth + 1, sb);
                value_decref(&child);
            } else {
                break;
            }
        }
        
        for (int i = 0; i < depth; i++) sb_append(sb, "  ", 2);
        sb_append(sb, "</", 2);
        // Re-get tag name for closing
        if (table_get(v.table, "__tag", &tag_val)) {
            sb_append(sb, tag_val.string->chars, tag_val.string->length);
            value_decref(&tag_val);
        }
        sb_append(sb, ">\n", 2);
    } else {
        sb_append(sb, "/>\n", 3);
    }
}

// ========== Public API ==========
bool codecs_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (arg_count < 1 || args[0].type != VAL_STRING) {
        *result = vm_make_bool(false);
        return true;
    }
    
    const char* input = args[0].string->chars;
    int input_len = args[0].string->length;

    // --- Base64 Functions ---
    if (strcmp(name, "codecs.base_write") == 0) {
        // Max size is ceil(len/3)*4 + 1
        int out_size = ((input_len + 2) / 3) * 4 + 1;
        char* out = (char*)malloc(out_size);
        if (!out) { *result = vm_make_bool(false); return true; }
        base64_encode((const unsigned char*)input, input_len, out);
        *result = vm_make_string(out);
        free(out);
        return true;
    }
    
    if (strcmp(name, "codecs.base_read") == 0) {
        unsigned char* out = (unsigned char*)malloc(input_len + 1);
        if (!out) { *result = vm_make_bool(false); return true; }
        int out_len = 0;
        if (base64_decode(input, out, &out_len)) {
            out[out_len] = '\0';
            *result = vm_make_string((char*)out);
        } else {
            *result = vm_make_bool(false);
        }
        free(out);
        return true;
    }

    if (strcmp(name, "codecs.baseurl_write") == 0) {
        // Max size is ceil(len/3)*4 + 1
        int out_size = ((input_len + 2) / 3) * 4 + 1;
        char* out = (char*)malloc(out_size);
        if (!out) { *result = vm_make_bool(false); return true; }
        base64url_encode((const unsigned char*)input, input_len, out);
        *result = vm_make_string(out);
        free(out);
        return true;
    }
    
    if (strcmp(name, "codecs.baseurl_read") == 0) {
        unsigned char* out = (unsigned char*)malloc(input_len + 1);
        if (!out) { *result = vm_make_bool(false); return true; }
        int out_len = 0;
        if (base64url_decode(input, out, &out_len)) {
            out[out_len] = '\0';
            *result = vm_make_string((char*)out);
        } else {
            *result = vm_make_bool(false);
        }
        free(out);
        return true;
    }

    // --- JSON ---
    if (strcmp(name, "codecs.json_read") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            const char* json_str = args[0].string->chars;
            if (json_parse_value(vm, &json_str, result)) {
                return true;
            }
        }
        *result = vm_make_bool(false);
        return true;
    }
    
    if (strcmp(name, "codecs.json_write") == 0) {
        if (arg_count >= 1) {
            StringBuilder sb;
            sb_init(&sb, 256);
            json_encode_value(vm, &args[0], &sb);
            *result = vm_make_string(sb.buffer);
            sb_free(&sb);
            return true;
        }
        *result = vm_make_string("");
        return true;
    }

    // --- CSV ---
    if (strcmp(name, "codecs.csv_read") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_bool(false);
            return true;
        }
        const char* data = args[0].string->chars;
        int len = args[0].string->length;
        bool has_header = true;
        char delimiter = ',';
        
        if (arg_count >= 2 && args[1].type == VAL_BOOL) has_header = args[1].boolean;
        if (arg_count >= 3 && args[2].type == VAL_STRING && args[2].string->length > 0)
            delimiter = args[2].string->chars[0];
            
        if (len <= 0) {
            *result = vm_make_table();
            return true;
        }
        
        CsvParser parser = { .data = data, .pos = 0, .len = len, .delimiter = delimiter };
        Value table_list = vm_make_table();
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
            
            Value row_table = vm_make_table();
            if (has_header && headers && col_count > 0) {
                for (int i = 0; i < col_count; i++) {
                    if (!csv_has_next(&parser)) break;
                    char* field = csv_parse_field(&parser);
                    Value val = csv_parse_value(field);
                    if (headers[i]) table_set(row_table.table, headers[i], val);
                    value_decref(&val);
                    free(field);
                }
            } else {
                int idx = 0;
                while (csv_has_next(&parser)) {
                    char key[32];
                    snprintf(key, sizeof(key), "%d", idx + 1);
                    char* field = csv_parse_field(&parser);
                    Value val = csv_parse_value(field);
                    table_set(row_table.table, key, val);
                    value_decref(&val);
                    free(field);
                    idx++;
                }
            }
            
            char index_key[32];
            snprintf(index_key, sizeof(index_key), "%d", row_index++);
            table_set(table_list.table, index_key, row_table);
            value_decref(&row_table);
        }
        
        if (headers) {
            for (int i = 0; i < col_count; i++) free(headers[i]);
            free(headers);
        }
        *result = table_list;
        return true;
    }
    
    if (strcmp(name, "codecs.csv_write") == 0) {
        if (arg_count < 1 || args[0].type != VAL_TABLE) {
            *result = vm_make_bool(false);
            return true;
        }
        Table* data = args[0].table;
        bool has_header = true;
        char delimiter = ',';
        
        if (arg_count >= 2 && args[1].type == VAL_BOOL) has_header = args[1].boolean;
        if (arg_count >= 3 && args[2].type == VAL_STRING && args[2].string->length > 0)
            delimiter = args[2].string->chars[0];
            
        int row_count = table_size(data);
        if (row_count == 0) {
            *result = vm_make_string("");
            return true;
        }
        
        Value first_row_val;
        if (!table_get(data, "1", &first_row_val) || first_row_val.type != VAL_TABLE) {
            *result = vm_make_bool(false);
            return true;
        }
        
        int header_count = 0;
        char** headers = table_keys(first_row_val.table, &header_count);
        
        StringBuilder sb;
        sb_init(&sb, 256);
        
        // Write Header
        if (has_header && headers && header_count > 0) {
            for (int i = 0; i < header_count; i++) {
                if (i > 0) sb_append_char(&sb, delimiter);
                const char* h = headers[i];
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
        
        // Write Rows
        for (int r = 1; r <= row_count; r++) {
            char key[32];
            snprintf(key, sizeof(key), "%d", r);
            Value row_val;
            if (!table_get(data, key, &row_val) || row_val.type != VAL_TABLE) continue;
            
            for (int i = 0; i < header_count; i++) {
                if (i > 0) sb_append_char(&sb, delimiter);
                Value cell_val;
                char buf[64];
                const char* str_val = "";
                
                if (table_get(row_val.table, headers[i], &cell_val)) {
                    switch (cell_val.type) {
                        case VAL_NUMBER: snprintf(buf, sizeof(buf), "%g", cell_val.number); str_val = buf; break;
                        case VAL_BOOL: str_val = cell_val.boolean ? "true" : "false"; break;
                        case VAL_STRING: str_val = cell_val.string->chars; break;
                        default: str_val = ""; break;
                    }
                    value_decref(&cell_val);
                }
                
                bool needs_quote = strchr(str_val, delimiter) || strchr(str_val, '"') || strchr(str_val, '\n');
                if (needs_quote) {
                    sb_append_char(&sb, '"');
                    for (const char* p = str_val; *p; p++) {
                        if (*p == '"') sb_append_char(&sb, '"'); // Correct escaping: " -> ""
                        sb_append_char(&sb, *p);
                    }
                    sb_append_char(&sb, '"');
                } else {
                    sb_append(&sb, str_val, strlen(str_val));
                }
            }
            // Only add newline if NOT the last row
            if (r < row_count) sb_append(&sb, "\n", 1);
            value_decref(&row_val);
        }

        if (headers) free(headers);
        *result = vm_make_string(sb.buffer);
        sb_free(&sb);
        return true;
    }

    // --- XML ---
    if (strcmp(name, "codecs.xml_read") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            const char* xml = args[0].string->chars;

            XmlParser xp;
            xp.p = xml;
            
            Value root = xml_parse_element(vm, &xp);
            if (root.type == VAL_TABLE) {
                *result = root;
            } else {
                value_decref(&root);
                *result = vm_make_bool(false);
            }
            return true;
        }
        *result = vm_make_bool(false);
        return true;
    }

    if (strcmp(name, "codecs.xml_write") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_TABLE) {
            StringBuilder sb;
            sb_init(&sb, 256);
            
            xml_write_node(vm, args[0], 0, &sb);

            *result = vm_make_string(sb.buffer);
            sb_free(&sb);
            return true;
        }
        *result = vm_make_bool(false);
        return true;
    }

    // --- YAML (Basic Subset) ---
    if (strcmp(name, "codecs.yaml_read") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            const char* data = args[0].string->chars;
            Value root = vm_make_table();
            
            // Very basic YAML parser: supports key: value and - list items at root level
            char line[1024];
            const char* p = data;
            int index = 1;
            
            while (*p) {
                // Read line
                int i = 0;
                while (*p && *p != '\n' && *p != '\r' && i < 1023) {
                    line[i++] = *p++;
                }
                line[i] = '\0';
                if (*p == '\r') p++;
                if (*p == '\n') p++;
                
                // Skip empty lines and comments
                char* l = line;
                while (*l && isspace((unsigned char)*l)) l++;
                if (!*l || *l == '#') continue;
                
                // Check for list item
                if (*l == '-') {
                    l++;
                    while (*l && isspace((unsigned char)*l)) l++;
                    if (*l) {
                        char k[32];
                        snprintf(k, sizeof(k), "%d", index++);
                        // Try to parse as number or bool
                        if (strcmp(l, "true") == 0) table_set(root.table, k, vm_make_bool(true));
                        else if (strcmp(l, "false") == 0) table_set(root.table, k, vm_make_bool(false));
                        else {
                            char* endptr;
                            double num = strtod(l, &endptr);
                            if (*endptr == '\0') table_set(root.table, k, vm_make_number(num));
                            else table_set(root.table, k, vm_make_string(l));
                        }
                    }
                } else {
                    // Key: Value
                    char* colon = strchr(l, ':');
                    if (colon) {
                        *colon = '\0';
                        char* key = l;
                        char* val = colon + 1;
                        while (*val && isspace((unsigned char)*val)) val++;
                        
                        // Trim key
                        char* end = key + strlen(key) - 1;
                        while (end > key && isspace((unsigned char)*end)) *end-- = '\0';
                        
                        if (*val) {
                            if (strcmp(val, "true") == 0) table_set(root.table, key, vm_make_bool(true));
                            else if (strcmp(val, "false") == 0) table_set(root.table, key, vm_make_bool(false));
                            else {
                                char* endptr;
                                double num = strtod(val, &endptr);
                                if (*endptr == '\0') table_set(root.table, key, vm_make_number(num));
                                else table_set(root.table, key, vm_make_string(val));
                            }
                        }
                    }
                }
            }
            *result = root;
            return true;
        }
        *result = vm_make_bool(false);
        return true;
    }

    // --- TOML (Basic Subset) ---
    if (strcmp(name, "codecs.toml_read") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            const char* data = args[0].string->chars;
            Value root = vm_make_table();
            Value current_table = root;
            value_incref(&root);
            
            char line[1024];
            const char* p = data;
            
            while (*p) {
                int i = 0;
                while (*p && *p != '\n' && *p != '\r' && i < 1023) {
                    line[i++] = *p++;
                }
                line[i] = '\0';
                if (*p == '\r') p++;
                if (*p == '\n') p++;
                
                char* l = line;
                while (*l && isspace((unsigned char)*l)) l++;
                if (!*l || *l == '#') continue;
                
                // Section [table]
                if (*l == '[') {
                    l++;
                    char* end = strchr(l, ']');
                    if (end) {
                        *end = '\0';
                        // Trim trailing spaces from section name
                        char* trim_end = end - 1;
                        while (trim_end > l && isspace((unsigned char)*trim_end)) *trim_end-- = '\0';
                        
                        Value new_t = vm_make_table();
                        table_set(root.table, l, new_t);
                        value_decref(&current_table);
                        current_table = new_t;
                        value_incref(&new_t);
                    }
                    continue;
                }
                
                // Key = Value
                char* eq = strchr(l, '=');
                if (eq) {
                    *eq = '\0';
                    char* key = l;
                    char* val = eq + 1;
                    
                    // Trim key
                    char* kend = key + strlen(key) - 1;
                    while (kend > key && isspace((unsigned char)*kend)) *kend-- = '\0';
                    
                    // Trim val
                    while (*val && isspace((unsigned char)*val)) val++;
                    char* vend = val + strlen(val) - 1;
                    while (vend > val && isspace((unsigned char)*vend)) *vend-- = '\0';
                    
                    if (*val) {
                        Value v;
                        if (*val == '"') {
                            // String
                            val++;
                            char* close = strchr(val, '"');
                            if (close) *close = '\0';
                            v = vm_make_string(val);
                        } else if (strcmp(val, "true") == 0) {
                            v = vm_make_bool(true);
                        } else if (strcmp(val, "false") == 0) {
                            v = vm_make_bool(false);
                        } else {
                            char* endptr;
                            double num = strtod(val, &endptr);
                            if (*endptr == '\0') v = vm_make_number(num);
                            else v = vm_make_string(val);
                        }
                        table_set(current_table.table, key, v);
                        value_decref(&v);
                    }
                }
            }
            value_decref(&current_table);
            *result = root;
            return true;
        }
        *result = vm_make_bool(false);
        return true;
    }

    return false;
}
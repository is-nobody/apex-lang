#include "codecs_module.h"
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

// ========== Public API ==========

bool codecs_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
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
        const char* filename = args[0].string->chars;
        bool has_header = true;
        char delimiter = ',';
        if (arg_count >= 2 && args[1].type == VAL_BOOL) {
            has_header = args[1].boolean;
        }
        if (arg_count >= 3 && args[2].type == VAL_STRING && args[2].string->length > 0) {
            delimiter = args[2].string->chars[0];
        }

        FILE* f = fopen(filename, "rb");
        if (!f) {
            *result = vm_make_bool(false);
            return true;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size <= 0) {
            fclose(f);
            *result = vm_make_table(); 
            return true;
        }
        char* buffer = (char*)malloc(size + 1);
        if (!buffer) {
            fclose(f);
            *result = vm_make_bool(false);
            return true;
        }
        size_t read_size = fread(buffer, 1, size, f);
        buffer[read_size] = '\0';
        fclose(f);

        if (delimiter == 0) delimiter = detect_delimiter(buffer, read_size);

        CsvParser parser;
        parser.data = buffer;
        parser.pos = 0;
        parser.len = read_size;
        parser.delimiter = delimiter;

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
                    for (int i = 0; i < col_count; i++) {
                        headers[i] = csv_parse_field(&parser);
                    }
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
                    if (headers[i]) {
                        table_set(row_table.table, headers[i], val);
                    }
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
        free(buffer);
        *result = table_list;
        return true;
    }

    if (strcmp(name, "codecs.csv_write") == 0) {
        if (arg_count < 2 || args[0].type != VAL_STRING || args[1].type != VAL_TABLE) {
            *result = vm_make_bool(false);
            return true;
        }
        const char* filename = args[0].string->chars;
        Table* data = args[1].table;
        bool has_header = true;
        char delimiter = ',';
        if (arg_count >= 3 && args[2].type == VAL_BOOL) {
            has_header = args[2].boolean;
        }
        if (arg_count >= 4 && args[3].type == VAL_STRING && args[3].string->length > 0) {
            delimiter = args[3].string->chars[0];
        }

        FILE* f = fopen(filename, "wb");
        if (!f) {
            *result = vm_make_bool(false);
            return true;
        }

        int row_count = table_size(data);
        if (row_count == 0) {
            fclose(f);
            *result = vm_make_bool(true);
            return true;
        }

        Value first_row_val;
        if (!table_get(data, "1", &first_row_val) || first_row_val.type != VAL_TABLE) {
            fclose(f);
            *result = vm_make_bool(false);
            return true;
        }
        Table* first_row = first_row_val.table;
        int header_count = 0;
        char** headers = table_keys(first_row, &header_count);

        if (has_header && headers && header_count > 0) {
            for (int i = 0; i < header_count; i++) {
                if (i > 0) fputc(delimiter, f);
                const char* h = headers[i];
                bool needs_quote = strchr(h, delimiter) || strchr(h, '"') || strchr(h, '\n');
                if (needs_quote) {
                    fputc('"', f);
                    for (const char* p = h; *p; p++) {
                        if (*p == '"') fputc('"', f);
                        fputc(*p, f);
                    }
                    fputc('"', f);
                } else {
                    fputs(h, f);
                }
            }
            fprintf(f, "\r\n");
        }

        for (int r = 1; r <= row_count; r++) {
            char key[32];
            snprintf(key, sizeof(key), "%d", r);
            Value row_val;
            if (!table_get(data, key, &row_val) || row_val.type != VAL_TABLE) continue;
            Table* row = row_val.table;
            for (int i = 0; i < header_count; i++) {
                if (i > 0) fputc(delimiter, f);
                Value cell_val;
                char buf[256];
                const char* str_val = "";
                if (table_get(row, headers[i], &cell_val)) {
                    switch (cell_val.type) {
                        case VAL_NUMBER:
                            snprintf(buf, sizeof(buf), "%g", cell_val.number);
                            str_val = buf;
                            break;
                        case VAL_BOOL:
                            str_val = cell_val.boolean ? "true" : "false";
                            break;
                        case VAL_STRING:
                            str_val = cell_val.string->chars;
                            break;
                        default:
                            str_val = "";
                            break;
                    }
                    value_decref(&cell_val);
                }
                bool needs_quote = strchr(str_val, delimiter) || strchr(str_val, '"') || strchr(str_val, '\n');
                if (needs_quote) {
                    fputc('"', f);
                    for (const char* p = str_val; *p; p++) {
                        if (*p == '"') fputc('"', f);
                        fputc(*p, f);
                    }
                    fputc('"', f);
                } else {
                    fputs(str_val, f);
                }
            }
            fprintf(f, "\r\n");
            value_decref(&row_val);
        }

        if (headers) free(headers);
        fclose(f);
        *result = vm_make_bool(true);
        return true;
    }

    return false;
}
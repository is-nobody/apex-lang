#include "csv_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ========== String Builder (Local Implementation) ==========
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

// ========== Helper Functions ==========

static bool is_numeric(const char* str) {
    if (!str || *str == '\0') return false;
    char* endptr;
    strtod(str, &endptr);
    return *endptr == '\0';
}

static bool is_bool(const char* str) {
    return (strcmp(str, "true") == 0 || strcmp(str, "false") == 0);
}

static Value parse_value(const char* str) {
    if (!str) return vm_make_bool(false);
    
    if (is_numeric(str)) {
        return vm_make_number(atof(str));
    }
    if (is_bool(str)) {
        return vm_make_bool(strcmp(str, "true") == 0);
    }
    return vm_make_string(str);
}

// Simple CSV parser state
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

// Parse a single field, handling quotes and escapes
static char* csv_parse_field(CsvParser* p) {
    StringBuilder sb;
    sb_init(&sb, 64);
    
    if (!p->data) {
        sb_free(&sb);
        return strdup("");
    }

    char c = csv_peek(p);
    
    if (c == '"') {
        // Quoted field
        csv_advance(p); // skip opening quote
        while (csv_has_next(p)) {
            c = csv_advance(p);
            if (c == '"') {
                if (csv_peek(p) == '"') {
                    // Escaped quote ""
                    sb_append_char(&sb, '"');
                    csv_advance(p);
                } else {
                    // End of quoted field
                    break;
                }
            } else {
                sb_append_char(&sb, c);
            }
        }
        // Skip delimiter or newline after closing quote
        if (csv_peek(p) == p->delimiter) csv_advance(p);
        else if (csv_peek(p) == '\r') {
            csv_advance(p);
            if (csv_peek(p) == '\n') csv_advance(p);
        } else if (csv_peek(p) == '\n') {
            csv_advance(p);
        }
    } else {
        // Unquoted field
        while (csv_has_next(p)) {
            c = csv_peek(p);
            if (c == p->delimiter || c == '\n' || c == '\r') {
                break;
            }
            sb_append_char(&sb, csv_advance(p));
        }
        // Skip delimiter or newline
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

// Detect delimiter by checking first few lines
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

// ========== API Implementation ==========

bool csv_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    
    // --- csv.read(filename, [has_header], [delimiter]) ---
    if (strcmp(name, "csv.read") == 0) {
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
            *result = vm_make_table(); // Return empty table for empty file
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
        
        Value table_list = vm_make_table(); // List of rows/objects
        
        // Parse headers if present
        char** headers = NULL;
        int col_count = 0;
        
        if (has_header && parser.len > 0) {
            // Count columns in first row to allocate headers
            int temp_pos = parser.pos;
            int count = 0;
            while (parser.pos < parser.len && parser.data[parser.pos] != '\n' && parser.data[parser.pos] != '\r') {
                if (parser.data[parser.pos] == delimiter) count++;
                parser.pos++;
            }
            col_count = count + 1;
            parser.pos = temp_pos; // Reset
            
            if (col_count > 0) {
                headers = (char**)malloc(sizeof(char*) * col_count);
                if (headers) {
                    for (int i = 0; i < col_count; i++) {
                        headers[i] = csv_parse_field(&parser);
                    }
                }
            }
        }
        
        // Parse data rows
        int row_index = 1;
        while (parser.pos < parser.len) {
            // Skip empty lines
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
                    Value val = parse_value(field);
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
                    Value val = parse_value(field);
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
    
    // --- csv.write(filename, data, [has_header], [delimiter]) ---
    if (strcmp(name, "csv.write") == 0) {
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
        
        // Get first row to determine headers/columns
        Value first_row_val;
        if (!table_get(data, "1", &first_row_val) || first_row_val.type != VAL_TABLE) {
            fclose(f);
            *result = vm_make_bool(false);
            return true;
        }
        
        Table* first_row = first_row_val.table;
        int header_count = 0;
        char** headers = table_keys(first_row, &header_count);
        
        // Write Header
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
        
        // Write Rows
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
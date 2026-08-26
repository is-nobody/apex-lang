// source/libraries/csv_module.c
// Implementation of CSV Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "csv_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// main dispatcher for csv module built-in functions
bool csv_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "csv.decode") == 0) {                                       // parse csv
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
    
    if (strcmp(name, "csv.encode") == 0) {                                      // csv serialize
        if (arg_count < 1 || !IS_TABLE(args[0])) {                              // validate table
            *result = MAKE_NONE();                                              // invalid
            return true;                                                        // builtin handled
        }
        Table* data = AS_TABLE(args[0]);                                        // data table
        bool has_header = true;                                                 // default header
        char delimiter = ',';                                                   // default delimiter
        
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
                value_incref(first_row_val);                                          // increment ref for ownership
                found_first = true;                                                   // mark found
            }
            value_decref(k_num);                                                      // release key
        }
        
        if (!found_first) {                                                           // not found yet
            Value k_str = MAKE_STRING(string_intern(&vm->intern_table, "1", 1));      // string key
            if (table_get(data, k_str, &first_row_val) && IS_TABLE(first_row_val)) {  // found
                value_incref(first_row_val);                                          // increment ref for ownership
                found_first = true;                                                   // mark found
            }
            value_decref(k_str);                                                      // release key
        }
        
        if (!found_first) {                                                           // no first row
            *result = MAKE_NONE();                                                    // return none
            return true;                                                              // builtin handled
        }
        
        int header_count = 0;                                                     // header count
        Value* headers = table_keys(AS_TABLE(first_row_val), &header_count);      // get headers from first row
        
        if (headers) {
            for (int i = 0; i < header_count; i++) {
                value_incref(headers[i]);                                         // take ownership of each header
            }
        }
        
        value_decref(first_row_val);                                              // safe to release first row now
        
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
                    value_incref(row_val);                                             // increment ref for ownership
                    got_row = true;                                                    // mark found
                }
                value_decref(k_row);                                                   // release key
            }
            
            if (!got_row) {                                                            // not found yet
                char rbuf[32];                                                         // buffer
                snprintf(rbuf, sizeof(rbuf), "%d", r);                                 // format
                Value k_str = MAKE_STRING(string_intern(&vm->intern_table, rbuf, strlen(rbuf)));  // string key
                if (table_get(data, k_str, &row_val) && IS_TABLE(row_val)) {           // found
                    value_incref(row_val);                                             // increment ref for ownership
                    got_row = true;                                                    // mark found
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
            for (int i = 0; i < header_count; i++) value_decref(headers[i]);  // release each header we incref'd
            free(headers);                                                    // free header array
        }
        *result = MAKE_STRING(string_intern(&vm->intern_table, sb.buffer, sb.length)); // intern result
        sb_free(&sb);                                                         // free builder
        return true;                                                          // builtin handled
    }

    return false;      // not a recognized builtin
}
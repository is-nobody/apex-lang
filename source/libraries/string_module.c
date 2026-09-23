// source/libraries/string_module.c
// Implementation of String Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "string_module.h"
#include "vm.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// returns the byte length of a UTF-8 character from its first byte
static int utf8_char_len(unsigned char c) {
    if (c < 0x80) return 1;                                           // ascii character
    if ((c & 0xE0) == 0xC0) return 2;                                 // 2-byte utf-8 sequence
    if ((c & 0xF0) == 0xE0) return 3;                                 // 3-byte utf-8 sequence
    if ((c & 0xF8) == 0xF0) return 4;                                 // 4-byte utf-8 sequence
    return 1;                                                         // fallback
}

// returns the number of characters (code points) in a UTF-8 string
static size_t utf8_strlen(const char* s) {
    if (!s) return 0;                                                // guard against null
    size_t len = 0;                                                  // character counter
    while (*s) {                                                     // iterate until null terminator
        len++;                                                       // count one character
        s += utf8_char_len((unsigned char)*s);                       // skip to next character
    }
    return len;                                                      // return character count
}

// returns the byte offset of the n-th character in a UTF-8 string
static size_t utf8_byte_offset(const char* s, size_t char_pos) {
    if (!s) return 0;                                                // guard against null
    const char* start = s;                                           // remember start position
    while (*s && char_pos > 0) {                                     // iterate until target position
        s += utf8_char_len((unsigned char)*s);                       // skip one character
        char_pos--;                                                  // decrement remaining positions
    }
    return s - start;                                                // return byte offset
}

// returns the number of characters in the first byte_len bytes of a UTF-8 string
static size_t utf8_strlen_len(const char* s, size_t byte_len) {
    if (!s) return 0;                                                // guard against null
    size_t count = 0;                                                // character counter
    size_t pos = 0;                                                  // byte position counter
    
    while (s[pos] && pos < byte_len) {                               // iterate until byte_len or null
        count++;                                                     // count one character
        pos += utf8_char_len((unsigned char)s[pos]);                 // skip to next character
    }
    return count;                                                    // return character count
}

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                      // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));  // intern and box as value
}

// dynamic string builder for joins whose length is not known up front;
// a fixed stack buffer used to overflow once joined output exceeded 64 KiB
typedef struct {
    char* buffer;                                                    // dynamically growing buffer
    int length;                                                      // current used length
    int capacity;                                                    // total allocated capacity
} StringBuilder;

// initializes the builder with the requested capacity (min 16 bytes)
static void sb_init(StringBuilder* sb, int initial_capacity) {
    sb->capacity = initial_capacity > 16 ? initial_capacity : 16;    // enforce minimum capacity
    sb->buffer = (char*)malloc(sb->capacity);                        // allocate initial buffer
    if (!sb->buffer) { sb->length = 0; sb->capacity = 0; return; }   // allocation failed
    sb->length = 0;                                                  // start empty
    sb->buffer[0] = '\0';                                            // null terminate
}

// grows the buffer to guarantee room for extra bytes plus a null terminator
static void sb_reserve(StringBuilder* sb, int extra) {
    if (!sb->buffer) return;                                         // buffer not initialized
    if (sb->length + extra + 1 <= sb->capacity) return;              // already has room
    int new_cap = sb->capacity > 0 ? sb->capacity : 16;              // start from current capacity
    while (new_cap < sb->length + extra + 1) new_cap *= 2;           // double until it fits
    char* new_buf = (char*)realloc(sb->buffer, new_cap);             // resize buffer
    if (!new_buf) return;                                            // realloc failed, keep old
    sb->buffer = new_buf;                                            // install new buffer
    sb->capacity = new_cap;                                          // update capacity
}

// appends len bytes from str to the builder, growing if needed
static void sb_append(StringBuilder* sb, const char* str, int len) {
    if (!sb->buffer || len <= 0) return;                             // nothing to append
    sb_reserve(sb, len);                                             // ensure capacity
    memcpy(sb->buffer + sb->length, str, len);                       // copy bytes
    sb->length += len;                                               // update length
    sb->buffer[sb->length] = '\0';                                   // null terminate
}

// releases the builder's internal buffer and resets its state
static void sb_free(StringBuilder* sb) {
    if (sb->buffer) free(sb->buffer);                                // release buffer
    sb->buffer = NULL;                                               // clear pointer
    sb->length = 0;                                                  // reset length
    sb->capacity = 0;                                                // reset capacity
}

// comparison function for sorting table keys as documented:
// numbers first in ascending order, then strings lexicographically
static int compare_keys(const void* a, const void* b) {
    Value va = *(const Value*)a;                                     // cast first key
    Value vb = *(const Value*)b;                                     // cast second key

    bool na = IS_NUMBER(va);                                         // first key is a number
    bool nb = IS_NUMBER(vb);                                         // second key is a number

    if (na && nb) {                                                  // both numbers: ascending
        double diff = AS_NUMBER(va) - AS_NUMBER(vb);                 // compute difference
        return (diff > 0) - (diff < 0);                              // return -1, 0, or 1
    }
    if (na) return -1;                                               // numbers come before strings
    if (nb) return 1;                                                // strings come after numbers

    bool isa = IS_STRING(va);                                        // first key is a string
    bool isb = IS_STRING(vb);                                        // second key is a string
    if (isa && isb) {                                                // both strings: lexicographic
        return strcmp(AS_STRING(va)->chars, AS_STRING(vb)->chars);   // compare contents
    }
    return 0;                                                        // otherwise: unspecified
}

// dispatcher for string manipulation built-in functions
bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (arg_count < 1) return false;                                 // need at least one argument
    
    if (strcmp(name, "string.length") == 0) {                        // get string length in characters
        if (arg_count < 1 || !IS_STRING(args[0])) {                  // validate string argument
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        size_t char_count = utf8_strlen(AS_STRING(args[0])->chars);  // count unicode characters
        *result = MAKE_NUMBER((double)char_count);                   // return character count
        return true;                                                 // builtin handled
    }
    
    if (strcmp(name, "string.upper") == 0) {                         // convert to uppercase
        if (arg_count < 1 || !IS_STRING(args[0])) {                  // validate string argument
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        char* str = strdup(AS_STRING(args[0])->chars);               // duplicate string
        for (char* p = str; *p; p++) *p = toupper(*p);               // uppercase each ascii char
        *result = make_string_val(vm, str);                          // intern and return
        free(str);                                                   // free temporary
        return true;                                                 // builtin handled
    }
    
    if (strcmp(name, "string.lower") == 0) {                         // convert to lowercase
        if (arg_count < 1 || !IS_STRING(args[0])) {                  // validate string argument
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        char* str = strdup(AS_STRING(args[0])->chars);               // duplicate string
        for (char* p = str; *p; p++) *p = tolower(*p);               // lowercase each ascii char
        *result = make_string_val(vm, str);                          // intern and return
        free(str);                                                   // free temporary
        return true;                                                 // builtin handled
    }
    
    if (strcmp(name, "string.trim") == 0) {                          // trim whitespace
        if (arg_count < 1 || !IS_STRING(args[0])) {                  // validate string argument
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        const char* str = AS_STRING(args[0])->chars;                 // get string
        while (isspace(*str)) str++;                                 // skip leading whitespace
        const char* end = str + strlen(str) - 1;                     // point to last char
        while (end > str && isspace(*end)) end--;                    // skip trailing whitespace
        
        int len = end - str + 1;                                     // trimmed length
        char* trimmed = (char*)malloc(len + 1);                      // allocate trimmed string
        strncpy(trimmed, str, len);                                  // copy trimmed content
        trimmed[len] = '\0';                                         // null terminate
        
        *result = make_string_val(vm, trimmed);                      // intern and return
        free(trimmed);                                               // free temporary
        return true;                                                 // builtin handled
    }
    
    if (strcmp(name, "string.find") == 0) {                                 // find substring position
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
            *result = MAKE_NONE();                                          // invalid, return none
            return true;                                                    // builtin handled
        }
        
        const char* haystack = AS_STRING(args[0])->chars;                   // source string
        const char* needle = AS_STRING(args[1])->chars;                     // substring to find
        
        char* byte_pos = strstr(haystack, needle);                          // find byte position
        if (byte_pos) {                                                     // found
            size_t byte_offset = byte_pos - haystack;                       // byte offset
            size_t char_pos = utf8_strlen_len(haystack, byte_offset);       // count chars before match
            *result = MAKE_NUMBER((double)(char_pos + 1));                  // return 1-based char position
        } else {                                                            // not found
            *result = MAKE_NUMBER(-1);                                      // return -1
        }
        return true;                                                        // builtin handled
    }
    
    if (strcmp(name, "string.replace") == 0) {                       // replace all occurrences
        if (arg_count < 3 || !IS_STRING(args[0]) ||                  // validate 3 string args
            !IS_STRING(args[1]) || !IS_STRING(args[2])) {
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        
        const char* str = AS_STRING(args[0])->chars;                 // source string
        const char* find = AS_STRING(args[1])->chars;                // substring to find
        const char* replacement = AS_STRING(args[2])->chars;         // replacement string
        
        size_t find_len = strlen(find);                              // length of search pattern
        size_t repl_len = strlen(replacement);                       // length of replacement
        
        if (find_len == 0) {                                         // empty search pattern
            *result = make_string_val(vm, str);                      // return original string
            return true;                                             // builtin handled
        }
        
        size_t count = 0;                                            // occurrence counter
        const char* scan = str;                                      // scanning pointer
        while ((scan = strstr(scan, find)) != NULL) {                // find all occurrences
            count++;                                                 // increment counter
            scan += find_len;                                        // move past match
        }
        
        if (count == 0) {                                            // no matches found
            *result = make_string_val(vm, str);                      // return original string
            return true;                                             // builtin handled
        }
        
        size_t str_len = strlen(str);                                // source length
        size_t result_len = str_len + count * (repl_len - find_len); // final length
        if (repl_len < find_len) {                                   // result will be shorter
            result_len = str_len - count * (find_len - repl_len);    // subtract difference
        }
        
        char* result_str = (char*)malloc(result_len + 1);            // allocate result buffer
        if (!result_str) {                                           // allocation failed
            *result = MAKE_NONE();                                   // return none
            return true;                                             // builtin handled
        }
        
        char* dest = result_str;                                     // destination pointer
        const char* src = str;                                       // source pointer
        const char* match;                                           // match pointer
        
        while ((match = strstr(src, find)) != NULL) {                // find next occurrence
            size_t prefix_len = match - src;                         // length before match
            memcpy(dest, src, prefix_len);                           // copy prefix
            dest += prefix_len;                                      // advance destination
            memcpy(dest, replacement, repl_len);                     // copy replacement
            dest += repl_len;                                        // advance destination
            src = match + find_len;                                  // move past match
        }
        strcpy(dest, src);                                           // copy remaining tail
        
        *result = make_string_val(vm, result_str);                   // intern and return
        free(result_str);                                            // free temporary
        return true;                                                 // builtin handled
    }
    
    if (strcmp(name, "string.slice") == 0) {                 // substring extraction
        if (arg_count < 3 || !IS_STRING(args[0])) {        // validate string and indices
            *result = MAKE_NONE();                         // invalid, return none
            return true;                                   // builtin handled
        }
        if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {  // validate start/end numbers
            *result = MAKE_NONE();                         // invalid, return none
            return true;                                   // builtin handled
        }
        double start_d = AS_NUMBER(args[1]);               // start position (1-based, raw)
        double end_d   = AS_NUMBER(args[2]);               // end position (raw)
        const char* str = AS_STRING(args[0])->chars;       // source string
        size_t char_count = utf8_strlen(str);              // total character count

        int start_char;                                    // resolved start (0-based)
        if (start_d < 1.0) {
            start_char = 0;                                // below range → clamp to 0
        } else if (start_d > (double)char_count) {
            start_char = (int)char_count;                  // above range → clamp to end
        } else {
            start_char = (int)start_d - 1;                 // in range → 0-based
        }

        int end_char;                                      // resolved end
        if (end_d < 0.0) {
            end_char = (int)char_count;                    // negative → end of string
        } else if (end_d > (double)char_count) {
            end_char = (int)char_count;                    // above range → clamp to end
        } else {
            end_char = (int)end_d;                         // in range
        }

        if (start_char >= end_char) {                      // empty range
            *result = make_string_val(vm, "");             // return empty string
        } else {
            size_t start_byte = utf8_byte_offset(str, (size_t)start_char);  // byte offset of start
            size_t end_byte = utf8_byte_offset(str, (size_t)end_char);      // byte offset of end
            size_t sub_len = end_byte - start_byte;                 // length in bytes
            
            char* sub = (char*)malloc(sub_len + 1);           // allocate substring
            if (!sub) {                                       // allocation failed
                *result = MAKE_NONE();                        // return none
                return true;                                  // builtin handled
            }
            
            memcpy(sub, str + start_byte, sub_len);           // copy substring
            sub[sub_len] = '\0';                              // null terminate
            
            *result = make_string_val(vm, sub);               // intern and return
            free(sub);                                        // free temporary
        }
        return true;                                          // builtin handled
    }
    
    if (strcmp(name, "string.split") == 0) {                  // split string by delimiter
        if (arg_count < 1 || !IS_STRING(args[0])) {           // validate string argument
            *result = MAKE_NONE();                            // invalid, return none
            return true;                                      // builtin handled
        }
        const char* sep = " ";                                // default separator is space
        if (arg_count >= 2 && IS_STRING(args[1])) {           // custom separator provided
            sep = AS_STRING(args[1])->chars;                  // use custom separator
        }
        
        Table* t = table_create(8);                           // create result table
        *result = MAKE_TABLE(t);                              // box table as result
        
        const char* str = AS_STRING(args[0])->chars;          // source string to split
        size_t sep_len = strlen(sep);                          // length of separator
        int idx = 1;                                           // starting index (1-based)
        
        if (sep_len == 0) {                                    // empty separator case
            const char* p = str;                               // pointer to current char
            while (*p) {                                       // iterate through all chars
                int char_len = utf8_char_len((unsigned char)*p); // get UTF-8 char length
                char* token = (char*)malloc(char_len + 1);     // allocate token buffer
                strncpy(token, p, char_len);                   // copy single character
                token[char_len] = '\0';                        // null terminate token
                
                Value k = MAKE_NUMBER((double)idx++);          // create index key
                table_set(t, k, make_string_val(vm, token));   // store character as string
                value_decref(k);                               // release key reference
                free(token);                                   // free token buffer
                
                p += char_len;                                 // move to next character
            }
        } else {                                               // non-empty separator case
            const char* start = str;                           // start of current token
            const char* p = str;                               // scanning pointer
            
            while (1) {                                          // infinite loop until break
                if (strncmp(p, sep, sep_len) == 0) {             // found separator match
                    size_t token_len = p - start;                // calculate token length
                    char* token = (char*)malloc(token_len + 1);  // allocate token buffer
                    strncpy(token, start, token_len);            // copy token content
                    token[token_len] = '\0';                     // null terminate token
                    
                    Value k = MAKE_NUMBER((double)idx++);        // create index key
                    table_set(t, k, make_string_val(vm, token)); // store token in table
                    value_decref(k);                             // release key reference
                    free(token);                                 // free token buffer
                    
                    p += sep_len;                              // move past separator
                    start = p;                                 // next token starts here
                } else if (*p == '\0') {                       // reached end of string
                    size_t token_len = p - start;              // calculate token length
                    char* token = (char*)malloc(token_len + 1); // allocate token buffer
                    strncpy(token, start, token_len);          // copy token content
                    token[token_len] = '\0';                   // null terminate token
                    
                    Value k = MAKE_NUMBER((double)idx++);      // create index key
                    table_set(t, k, make_string_val(vm, token)); // store token in table
                    value_decref(k);                           // release key reference
                    free(token);                               // free token buffer
                    
                    break;                                     // exit loop
                } else {                                       // not separator, not end
                    p++;                                       // move to next character
                }
            }
        }
        
        return true;                                          // builtin handled
    }
    
    if (strcmp(name, "string.join") == 0) {                   // join table elements
        if (arg_count < 1 || !IS_TABLE(args[0])) {            // validate table argument
            *result = MAKE_NONE();                            // invalid, return none
            return true;                                      // builtin handled
        }
        const char* sep = "";                                 // default separator
        int sep_len = 0;                                      // default separator length
        if (arg_count >= 2 && IS_STRING(args[1])) {           // custom separator provided
            sep = AS_STRING(args[1])->chars;                  // use custom separator
            sep_len = AS_STRING(args[1])->length;             // cache its length
        }
        
        StringBuilder sb;                                     // dynamic result buffer
        sb_init(&sb, 256);                                    // init with a modest initial capacity
        Table* table = AS_TABLE(args[0]);                     // unwrap table
        
        int count;                                            // key count
        Value* keys = table_keys(table, &count);              // get all keys

        if (keys && count > 0) {                              // check if keys exist
            qsort(keys, count, sizeof(Value), compare_keys);  // sort keys
            bool first = true;                                // flag for first element

            for (int i = 0; i < count; i++) {                 // iterate over sorted keys
                Value val;                                    // value storage
                if (table_get(table, keys[i], &val)) {        // lookup value by key
                    if (!first) sb_append(&sb, sep, sep_len); // add separator if not first
                    first = false;                            // no longer first

                    if (IS_STRING(val)) {                                             // string value
                        sb_append(&sb, AS_STRING(val)->chars, AS_STRING(val)->length); // append raw bytes
                    } else if (IS_NUMBER(val)) {                                      // number value
                        char num[64];                                                 // number buffer
                        int n = snprintf(num, sizeof(num), "%g", AS_NUMBER(val));     // format number
                        if (n > 0) sb_append(&sb, num, n);                            // append formatted digits
                    } else if (IS_BOOL(val)) {                                        // boolean value
                        const char* b = AS_BOOL(val) ? "true" : "false";              // bool text
                        sb_append(&sb, b, (int)strlen(b));                            // append bool text
                    }
                    value_decref(val);                                            // release value reference
                }
                value_decref(keys[i]);                                            // release key reference
            }
            free(keys);                                                           // free keys array
        }

        // join results vary per call and can be very large, so build a regular
        // refcounted string instead of interning; interning would permanently
        // pin each unique result inside the intern table
        const char* buf = sb.buffer ? sb.buffer : "";                             // guard against OOM
        *result = MAKE_STRING(string_create(buf, sb.length));                     // fresh refcounted string
        sb_free(&sb);                                                             // release builder
        return true;                                                              // builtin handled
    }
    
    return false;                               // not a recognized builtin
}
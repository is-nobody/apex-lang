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

// decodes a UTF-8 sequence into a Unicode code point
static int utf8_decode(const char* s, unsigned int* codepoint) {
    unsigned char c = (unsigned char)s[0];                            // get first byte
    
    if (c < 0x80) {                                                   // ascii character
        *codepoint = c;                                               // store ascii value
        return 1;                                                     // 1 byte consumed
    }
    if ((c & 0xE0) == 0xC0) {                                         // 2-byte sequence
        *codepoint = ((c & 0x1F) << 6) | (s[1] & 0x3F);               // decode 2-byte
        return 2;                                                     // 2 bytes consumed
    }
    if ((c & 0xF0) == 0xE0) {                                         // 3-byte sequence
        *codepoint = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);  // decode 3-byte
        return 3;                                                     // 3 bytes consumed
    }
    if ((c & 0xF8) == 0xF0) {                                         // 4-byte sequence
        *codepoint = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);  // decode 4-byte
        return 4;                                                     // 4 bytes consumed
    }
    *codepoint = c;                                                   // fallback
    return 1;                                                         // 1 byte consumed
}

// checks if a Unicode code point is a letter (all modern writing systems)
static bool unicode_is_letter(unsigned int cp) {
    return (cp >= 'A' && cp <= 'Z') ||                               // basic latin uppercase
           (cp >= 'a' && cp <= 'z') ||                               // basic latin lowercase
           (cp >= 0x00C0 && cp <= 0x024F) ||                         // latin extended
           (cp >= 0x0400 && cp <= 0x04FF) ||                         // cyrillic
           (cp >= 0x0370 && cp <= 0x03FF) ||                         // greek
           (cp >= 0x0530 && cp <= 0x058F) ||                         // armenian
           (cp >= 0x0590 && cp <= 0x05FF) ||                         // hebrew
           (cp >= 0x0600 && cp <= 0x06FF) ||                         // arabic
           (cp >= 0x0750 && cp <= 0x077F) ||                         // arabic extended
           (cp >= 0xFB50 && cp <= 0xFDFF) ||                         // arabic presentation
           (cp >= 0x0900 && cp <= 0x097F) ||                         // devanagari
           (cp >= 0x0980 && cp <= 0x09FF) ||                         // bengali
           (cp >= 0x0A00 && cp <= 0x0A7F) ||                         // gurmukhi
           (cp >= 0x0A80 && cp <= 0x0AFF) ||                         // gujarati
           (cp >= 0x0B80 && cp <= 0x0BFF) ||                         // oriya
           (cp >= 0x0C00 && cp <= 0x0C7F) ||                         // telugu
           (cp >= 0x0C80 && cp <= 0x0CFF) ||                         // kannada
           (cp >= 0x0D00 && cp <= 0x0D7F) ||                         // malayalam
           (cp >= 0x0D80 && cp <= 0x0DFF) ||                         // sinhala
           (cp >= 0x0E00 && cp <= 0x0E7F) ||                         // thai
           (cp >= 0x0E80 && cp <= 0x0EFF) ||                         // lao
           (cp >= 0x0F00 && cp <= 0x0FFF) ||                         // tibetan
           (cp >= 0x1000 && cp <= 0x109F) ||                         // myanmar
           (cp >= 0x10A0 && cp <= 0x10FF) ||                         // georgian
           (cp >= 0x1100 && cp <= 0x11FF) ||                         // hangul jamo
           (cp >= 0x3130 && cp <= 0x318F) ||                         // hangul compatibility
           (cp >= 0xAC00 && cp <= 0xD7AF) ||                         // hangul syllables
           (cp >= 0x1200 && cp <= 0x137F) ||                         // ethiopic
           (cp >= 0x2D80 && cp <= 0x2DDF) ||                         // ethiopic extended
           (cp >= 0x13A0 && cp <= 0x13FF) ||                         // cherokee
           (cp >= 0x1400 && cp <= 0x167F) ||                         // canadian aboriginal
           (cp >= 0x1780 && cp <= 0x17FF) ||                         // khmer
           (cp >= 0x1800 && cp <= 0x18AF) ||                         // mongolian
           (cp >= 0x3040 && cp <= 0x30FF) ||                         // hiragana + katakana
           (cp >= 0x3400 && cp <= 0x4DBF) ||                         // cjk extended a
           (cp >= 0x4E00 && cp <= 0x9FFF) ||                         // cjk unified
           (cp >= 0xF900 && cp <= 0xFAFF) ||                         // cjk compatibility
           (cp >= 0xFF00 && cp <= 0xFFEF);                           // halfwidth/fullwidth
}

// checks if a Unicode code point is a digit
static bool unicode_is_digit(unsigned int cp) {
    return (cp >= '0' && cp <= '9');                                 // ascii digits only
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

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                      // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));  // intern and box as value
}

// comparison function for sorting table keys
static int compare_keys(const void* a, const void* b) {
    Value va = *(const Value*)a;                                     // cast first key
    Value vb = *(const Value*)b;                                     // cast second key
    
    if (IS_NUMBER(va) && IS_NUMBER(vb)) {                            // both are numbers
        double diff = AS_NUMBER(va) - AS_NUMBER(vb);                 // compute difference
        return (diff > 0) - (diff < 0);                              // return -1, 0, or 1
    }
    return 0;                                                        // fallback
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
    
    if (strcmp(name, "string.is_letter") == 0) {                      // check if first char is letter
        if (arg_count < 1 || !IS_STRING(args[0])) {                  // validate string argument
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        if (AS_STRING(args[0])->length == 0) {                       // empty string
            *result = MAKE_BOOL(false);                              // not a letter
            return true;                                             // builtin handled
        }
        unsigned int cp;                                             // code point storage
        utf8_decode(AS_STRING(args[0])->chars, &cp);                 // decode first character
        *result = MAKE_BOOL(unicode_is_letter(cp));                  // check if letter
        return true;                                                 // builtin handled
    }
    
    if (strcmp(name, "string.is_number") == 0) {                      // check if first char is digit
        if (arg_count < 1 || !IS_STRING(args[0])) {                  // validate string argument
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        if (AS_STRING(args[0])->length == 0) {                       // empty string
            *result = MAKE_BOOL(false);                              // not a digit
            return true;                                             // builtin handled
        }
        unsigned int cp;                                             // code point storage
        utf8_decode(AS_STRING(args[0])->chars, &cp);                 // decode first character
        *result = MAKE_BOOL(unicode_is_digit(cp));                   // check if digit
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
        char* pos = strstr(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);  // find substring
        if (pos) {                                                          // found
            *result = MAKE_NUMBER((pos - AS_STRING(args[0])->chars) + 1);   // return byte offset
        } else {                                                            // not found
            *result = MAKE_NUMBER(-1);                                      // return -1
        }
        return true;                                                        // builtin handled
    }
    
    if (strcmp(name, "string.replace") == 0) {                       // replace first occurrence
        if (arg_count < 3 || !IS_STRING(args[0]) ||                  // validate 3 string args
            !IS_STRING(args[1]) || !IS_STRING(args[2])) {
            *result = MAKE_NONE();                                   // invalid, return none
            return true;                                             // builtin handled
        }
        char* str = strdup(AS_STRING(args[0])->chars);                   // duplicate original
        char* pos = strstr(str, AS_STRING(args[1])->chars);              // find target substring
        if (pos) {                                                       // found
            int prefix_len = pos - str;                                  // length before match
            const char* replacement = AS_STRING(args[2])->chars;         // replacement string
            char* result_str = (char*)malloc(strlen(str) + strlen(replacement) + 1);  // allocate result
            strncpy(result_str, str, prefix_len);                         // copy prefix
            result_str[prefix_len] = '\0';                                // null terminate
            strcat(result_str, replacement);                              // append replacement
            strcat(result_str, pos + strlen(AS_STRING(args[1])->chars));  // append rest
            *result = make_string_val(vm, result_str);                    // intern and return
            free(result_str);                              // free temporary
        } else {                                           // not found
            *result = make_string_val(vm, str);            // return original
        }
        free(str);                                         // free duplicate
        return true;                                       // builtin handled
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
        int start_char = (int)AS_NUMBER(args[1]) - 1;      // start position (1-based)
        size_t end_char = (size_t)AS_NUMBER(args[2]);      // end position
        const char* str = AS_STRING(args[0])->chars;       // source string
        size_t char_count = utf8_strlen(str);              // total character count
        
        if (start_char < 0) start_char = 0;                               // clamp start
        if (end_char > char_count) end_char = char_count;                 // clamp end
        if ((size_t)start_char >= end_char) {                             // empty range
            *result = make_string_val(vm, "");                            // return empty string
        } else {
            size_t start_byte = utf8_byte_offset(str, start_char);  // byte offset of start
            size_t end_byte = utf8_byte_offset(str, end_char);      // byte offset of end
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
        const char* sep = " ";                                // default separator
        if (arg_count >= 2 && IS_STRING(args[1])) {           // custom separator provided
            sep = AS_STRING(args[1])->chars;                  // use custom separator
        }
        
        Table* t = table_create(8);                           // create result table
        *result = MAKE_TABLE(t);                              // box table as result
        char* str = strdup(AS_STRING(args[0])->chars);        // duplicate for tokenization
        char* token = strtok(str, sep);                       // get first token
        int idx = 1;                                          // starting index
        
        while (token) {                                       // iterate over tokens
            Value k = MAKE_NUMBER((double)idx++);             // create index key
            table_set(t, k, make_string_val(vm, token));      // store token
            value_decref(k);                                  // release key reference
            token = strtok(NULL, sep);                        // get next token
        }
        free(str);                                            // free duplicate
        return true;                                          // builtin handled
    }
    
    if (strcmp(name, "string.join") == 0) {                   // join table elements
        if (arg_count < 1 || !IS_TABLE(args[0])) {            // validate table argument
            *result = MAKE_NONE();                            // invalid, return none
            return true;                                      // builtin handled
        }
        const char* sep = "";                                 // default separator
        if (arg_count >= 2 && IS_STRING(args[1])) {           // custom separator provided
            sep = AS_STRING(args[1])->chars;                  // use custom separator
        }
        
        char buffer[65536] = "";                              // static buffer for result
        Table* table = AS_TABLE(args[0]);                     // unwrap table
        
        int count;                                            // key count
        Value* keys = table_keys(table, &count);              // get all keys

        if (keys && count > 0) {                              // check if keys exist
            qsort(keys, count, sizeof(Value), compare_keys);  // sort keys
            bool first = true;                                // flag for first element

            for (int i = 0; i < count; i++) {                 // iterate over sorted keys
                Value val;                                    // value storage
                if (table_get(table, keys[i], &val)) {        // lookup value by key
                    if (!first) strcat(buffer, sep);          // add separator if not first
                    first = false;                            // no longer first
                    
                    if (IS_STRING(val)) {                                  // string value
                        strcat(buffer, AS_STRING(val)->chars);             // append string
                    } else if (IS_NUMBER(val)) {                           // number value
                        char num[64];                                      // number buffer
                        snprintf(num, sizeof(num), "%g", AS_NUMBER(val));  // format number
                        strcat(buffer, num);                               // append number
                    } else if (IS_BOOL(val)) {                             // boolean value
                        strcat(buffer, AS_BOOL(val) ? "true" : "false");   // append bool string
                    }
                    value_decref(val);          // release value reference
                }
                value_decref(keys[i]);          // release key reference
            }
            free(keys);                         // free keys array
        }
        *result = make_string_val(vm, buffer);  // intern and return
        return true;                            // builtin handled
    }
    
    return false;                               // not a recognized builtin
}
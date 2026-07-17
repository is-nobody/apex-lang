#include "string_module.h"
#include "vm.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// returns the byte length of a UTF-8 character from its first byte
static int utf8_char_len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// decodes a UTF-8 sequence into a Unicode code point
static int utf8_decode(const char* s, unsigned int* codepoint) {
    unsigned char c = (unsigned char)s[0];
    
    if (c < 0x80) {
        *codepoint = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        *codepoint = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        *codepoint = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        *codepoint = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    *codepoint = c;
    return 1;
}

// checks if a Unicode code point is a letter (all modern writing systems)
static bool unicode_is_letter(unsigned int cp) {
    return (cp >= 'A' && cp <= 'Z') ||
           (cp >= 'a' && cp <= 'z') ||
           (cp >= 0x00C0 && cp <= 0x024F) ||
           (cp >= 0x0400 && cp <= 0x04FF) ||
           (cp >= 0x0370 && cp <= 0x03FF) ||
           (cp >= 0x0530 && cp <= 0x058F) ||
           (cp >= 0x0590 && cp <= 0x05FF) ||
           (cp >= 0x0600 && cp <= 0x06FF) ||
           (cp >= 0x0750 && cp <= 0x077F) ||
           (cp >= 0xFB50 && cp <= 0xFDFF) ||
           (cp >= 0x0900 && cp <= 0x097F) ||
           (cp >= 0x0980 && cp <= 0x09FF) ||
           (cp >= 0x0A00 && cp <= 0x0A7F) ||
           (cp >= 0x0A80 && cp <= 0x0AFF) ||
           (cp >= 0x0B80 && cp <= 0x0BFF) ||
           (cp >= 0x0C00 && cp <= 0x0C7F) ||
           (cp >= 0x0C80 && cp <= 0x0CFF) ||
           (cp >= 0x0D00 && cp <= 0x0D7F) ||
           (cp >= 0x0D80 && cp <= 0x0DFF) ||
           (cp >= 0x0E00 && cp <= 0x0E7F) ||
           (cp >= 0x0E80 && cp <= 0x0EFF) ||
           (cp >= 0x0F00 && cp <= 0x0FFF) ||
           (cp >= 0x1000 && cp <= 0x109F) ||
           (cp >= 0x10A0 && cp <= 0x10FF) ||
           (cp >= 0x1100 && cp <= 0x11FF) ||
           (cp >= 0x3130 && cp <= 0x318F) ||
           (cp >= 0xAC00 && cp <= 0xD7AF) ||
           (cp >= 0x1200 && cp <= 0x137F) ||
           (cp >= 0x2D80 && cp <= 0x2DDF) ||
           (cp >= 0x13A0 && cp <= 0x13FF) ||
           (cp >= 0x1400 && cp <= 0x167F) ||
           (cp >= 0x1780 && cp <= 0x17FF) ||
           (cp >= 0x1800 && cp <= 0x18AF) ||
           (cp >= 0x3040 && cp <= 0x30FF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFF00 && cp <= 0xFFEF);
}

// checks if a Unicode code point is a digit
static bool unicode_is_digit(unsigned int cp) {
    return (cp >= '0' && cp <= '9');
}

// returns the number of characters (code points) in a UTF-8 string
static size_t utf8_strlen(const char* s) {
    if (!s) return 0;
    size_t len = 0;
    while (*s) {
        len++;
        s += utf8_char_len((unsigned char)*s);
    }
    return len;
}

// returns the byte offset of the n-th character in a UTF-8 string
static size_t utf8_byte_offset(const char* s, size_t char_pos) {
    if (!s) return 0;
    const char* start = s;
    while (*s && char_pos > 0) {
        s += utf8_char_len((unsigned char)*s);
        char_pos--;
    }
    return s - start;
}

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    (void)vm;
    int len = (int)strlen(str);
    if (len >= 16 && len <= 64 && vm->intern_table.count < 50000) {
        return MAKE_STRING(string_intern(&vm->intern_table, str, len));
    }
    return MAKE_STRING(string_create(str, len));
}

// comparison function for sorting table keys
static int compare_keys(const void* a, const void* b) {
    Value va = *(const Value*)a;
    Value vb = *(const Value*)b;
    
    if (IS_NUMBER(va) && IS_NUMBER(vb)) {
        double diff = AS_NUMBER(va) - AS_NUMBER(vb);
        return (diff > 0) - (diff < 0);
    }
    return 0;
}

// dispatcher for string manipulation built-in functions
bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (arg_count < 1) return false;
    
    if (strcmp(name, "string.length") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        size_t char_count = utf8_strlen(AS_STRING(args[0])->chars);
        *result = MAKE_NUMBER((double)char_count);
        return true;
    }
    
    if (strcmp(name, "string.isletter") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        if (AS_STRING(args[0])->length == 0) {
            *result = MAKE_BOOL(false);
            return true;
        }
        unsigned int cp;
        utf8_decode(AS_STRING(args[0])->chars, &cp);
        *result = MAKE_BOOL(unicode_is_letter(cp));
        return true;
    }
    
    if (strcmp(name, "string.isnumber") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        if (AS_STRING(args[0])->length == 0) {
            *result = MAKE_BOOL(false);
            return true;
        }
        unsigned int cp;
        utf8_decode(AS_STRING(args[0])->chars, &cp);
        *result = MAKE_BOOL(unicode_is_digit(cp));
        return true;
    }
    
    if (strcmp(name, "string.upper") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        char* str = strdup(AS_STRING(args[0])->chars);
        for (char* p = str; *p; p++) *p = toupper(*p);
        *result = make_string_val(vm, str);
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.lower") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        char* str = strdup(AS_STRING(args[0])->chars);
        for (char* p = str; *p; p++) *p = tolower(*p);
        *result = make_string_val(vm, str);
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.trim") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        const char* str = AS_STRING(args[0])->chars;
        while (isspace(*str)) str++;
        const char* end = str + strlen(str) - 1;
        while (end > str && isspace(*end)) end--;
        
        int len = end - str + 1;
        char* trimmed = (char*)malloc(len + 1);
        strncpy(trimmed, str, len);
        trimmed[len] = '\0';
        
        *result = make_string_val(vm, trimmed);
        free(trimmed);
        return true;
    }
    
    if (strcmp(name, "string.find") == 0) {
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
            *result = MAKE_NONE();
            return true;
        }
        char* pos = strstr(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
        if (pos) {
            *result = MAKE_NUMBER(pos - AS_STRING(args[0])->chars);
        } else {
            *result = MAKE_NUMBER(-1);
        }
        return true;
    }
    
    if (strcmp(name, "string.replace") == 0) {
        if (arg_count < 3 || !IS_STRING(args[0]) || 
            !IS_STRING(args[1]) || !IS_STRING(args[2])) {
            *result = MAKE_NONE();
            return true;
        }
        char* str = strdup(AS_STRING(args[0])->chars);
        char* pos = strstr(str, AS_STRING(args[1])->chars);
        if (pos) {
            int prefix_len = pos - str;
            const char* replacement = AS_STRING(args[2])->chars;
            char* result_str = (char*)malloc(strlen(str) + strlen(replacement) + 1);
            strncpy(result_str, str, prefix_len);
            result_str[prefix_len] = '\0';
            strcat(result_str, replacement);
            strcat(result_str, pos + strlen(AS_STRING(args[1])->chars));
            *result = make_string_val(vm, result_str);
            free(result_str);
        } else {
            *result = make_string_val(vm, str);
        }
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.sub") == 0) {
        if (arg_count < 3 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
            *result = MAKE_NONE();
            return true;
        }
        int start_char = (int)AS_NUMBER(args[1]);
        size_t end_char = (size_t)AS_NUMBER(args[2]);
        const char* str = AS_STRING(args[0])->chars;
        size_t char_count = utf8_strlen(str);
        
        if (start_char < 0) start_char = 0;
        if (end_char > char_count) end_char = char_count;
        if ((size_t)start_char >= end_char) {
            *result = make_string_val(vm, "");
        } else {
            size_t start_byte = utf8_byte_offset(str, start_char);
            size_t end_byte = utf8_byte_offset(str, end_char);
            size_t sub_len = end_byte - start_byte;
            
            char* sub = (char*)malloc(sub_len + 1);
            if (!sub) {
                *result = MAKE_NONE();
                return true;
            }
            
            memcpy(sub, str + start_byte, sub_len);
            sub[sub_len] = '\0';
            
            *result = make_string_val(vm, sub);
            free(sub);
        }
        return true;
    }
    
    if (strcmp(name, "string.split") == 0) {
        if (arg_count < 1 || !IS_STRING(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        const char* sep = " ";
        if (arg_count >= 2 && IS_STRING(args[1])) {
            sep = AS_STRING(args[1])->chars;
        }
        
        Table* t = table_create(8);
        *result = MAKE_TABLE(t);
        char* str = strdup(AS_STRING(args[0])->chars);
        char* token = strtok(str, sep);
        int idx = 1;
        
        while (token) {
            Value k = MAKE_NUMBER((double)idx++);
            table_set(t, k, make_string_val(vm, token));
            value_decref(k);
            token = strtok(NULL, sep);
        }
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.join") == 0) {
        if (arg_count < 1 || !IS_TABLE(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        const char* sep = "";
        if (arg_count >= 2 && IS_STRING(args[1])) {
            sep = AS_STRING(args[1])->chars;
        }
        
        char buffer[65536] = "";
        Table* table = AS_TABLE(args[0]);
        
        int count;
        Value* keys = table_keys(table, &count);

        if (keys && count > 0) {
            qsort(keys, count, sizeof(Value), compare_keys);
            bool first = true;

            for (int i = 0; i < count; i++) {
                Value val;
                if (table_get(table, keys[i], &val)) {
                    if (!first) strcat(buffer, sep);
                    first = false;
                    
                    if (IS_STRING(val)) {
                        strcat(buffer, AS_STRING(val)->chars);
                    } else if (IS_NUMBER(val)) {
                        char num[64];
                        snprintf(num, sizeof(num), "%g", AS_NUMBER(val));
                        strcat(buffer, num);
                    } else if (IS_BOOL(val)) {
                        strcat(buffer, AS_BOOL(val) ? "true" : "false");
                    }
                    value_decref(val);
                }
                value_decref(keys[i]);
            }
            free(keys);
        }
        *result = make_string_val(vm, buffer);
        return true;
    }
    
    return false;
}
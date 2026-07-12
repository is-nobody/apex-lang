#include "string_module.h"
#include "table_module.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// returns the byte length of a UTF-8 character from its first byte
static int utf8_char_len(unsigned char c) {
    if (c < 0x80) return 1;           // ASCII
    if ((c & 0xE0) == 0xC0) return 2; // 2-byte sequence
    if ((c & 0xF0) == 0xE0) return 3; // 3-byte sequence (Cyrillic, etc.)
    if ((c & 0xF8) == 0xF0) return 4; // 4-byte sequence
    return 1; // invalid UTF-8, treat as single byte
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

// dispatcher for string manipulation built-in functions
bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    if (arg_count < 1) return false;
    
    if (strcmp(name, "string.len") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        *result = vm_make_number(args[0].string->length);
        return true;
    }
    
    if (strcmp(name, "string.upper") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        char* str = strdup(args[0].string->chars);
        for (char* p = str; *p; p++) *p = toupper(*p);
        *result = vm_make_string(str);
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.lower") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        char* str = strdup(args[0].string->chars);
        for (char* p = str; *p; p++) *p = tolower(*p);
        *result = vm_make_string(str);
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.trim") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        const char* str = args[0].string->chars;
        while (isspace(*str)) str++;
        const char* end = str + strlen(str) - 1;
        while (end > str && isspace(*end)) end--;
        
        int len = end - str + 1;
        char* trimmed = (char*)malloc(len + 1);
        strncpy(trimmed, str, len);
        trimmed[len] = '\0';
        
        *result = vm_make_string(trimmed);
        free(trimmed);
        return true;
    }
    
    if (strcmp(name, "string.find") == 0) {
        if (arg_count < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        char* pos = strstr(args[0].string->chars, args[1].string->chars);
        if (pos) {
            *result = vm_make_number(pos - args[0].string->chars);
        } else {
            *result = vm_make_number(-1);
        }
        return true;
    }
    
    if (strcmp(name, "string.replace") == 0) {
        if (arg_count < 3 || args[0].type != VAL_STRING || 
            args[1].type != VAL_STRING || args[2].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        char* str = strdup(args[0].string->chars);
        char* pos = strstr(str, args[1].string->chars);
        if (pos) {
            int prefix_len = pos - str;
            char* result_str = (char*)malloc(strlen(str) + strlen(args[2].string->chars) + 1);
            strncpy(result_str, str, prefix_len);
            result_str[prefix_len] = '\0';
            strcat(result_str, args[2].string->chars);
            strcat(result_str, pos + strlen(args[1].string->chars));
            *result = vm_make_string(result_str);
            free(result_str);
        } else {
            *result = vm_make_string(str);
        }
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.sub") == 0) {
        if (arg_count < 3 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        if (args[1].type != VAL_NUMBER || args[2].type != VAL_NUMBER) {
            *result = vm_make_none();
            return true;
        }
        int start_char = (int)args[1].number;
        size_t end_char = (size_t)args[2].number;
        const char* str = args[0].string->chars;
        size_t char_count = utf8_strlen(str);
        
        if (start_char < 0) start_char = 0;
        if (end_char > char_count) end_char = char_count;
        if ((size_t)start_char >= end_char) {
            *result = vm_make_string("");
        } else {
            size_t start_byte = utf8_byte_offset(str, start_char);
            size_t end_byte = utf8_byte_offset(str, end_char);
            size_t sub_len = end_byte - start_byte;
            
            char* sub = (char*)malloc(sub_len + 1);
            if (!sub) {
                *result = vm_make_none();
                return true;
            }
            
            memcpy(sub, str + start_byte, sub_len);
            sub[sub_len] = '\0';
            
            *result = vm_make_string(sub);
            free(sub);
        }
        return true;
    }
    
    if (strcmp(name, "string.split") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        const char* sep = " ";
        if (arg_count >= 2 && args[1].type == VAL_STRING) {
            sep = args[1].string->chars;
        }
        
        *result = vm_make_table();
        char* str = strdup(args[0].string->chars);
        char* token = strtok(str, sep);
        int idx = 1;
        
        while (token) {
            char key[32];
            snprintf(key, sizeof(key), "%d", idx++);
            table_set(result->table, key, vm_make_string(token));
            token = strtok(NULL, sep);
        }
        free(str);
        return true;
    }
    
    if (strcmp(name, "string.join") == 0) {
        if (arg_count < 1 || args[0].type != VAL_TABLE) {
            *result = vm_make_none();
            return true;
        }
        const char* sep = "";
        if (arg_count >= 2 && args[1].type == VAL_STRING) {
            sep = args[1].string->chars;
        }
        
        char buffer[65536] = "";
        Table* table = args[0].table;
        
        int count;
        char** keys = table_keys(table, &count);
        
        if (keys && count > 0) {
            qsort(keys, count, sizeof(char*), compare_keys);
            
            bool first = true;
            
            for (int i = 0; i < count; i++) {
                Value val;
                if (table_get(table, keys[i], &val)) {
                    if (!first) strcat(buffer, sep);
                    first = false;
                    
                    switch (val.type) {
                        case VAL_STRING:
                            strcat(buffer, val.string->chars);
                            break;
                        case VAL_NUMBER: {
                            char num[64];
                            snprintf(num, sizeof(num), "%g", val.number);
                            strcat(buffer, num);
                            break;
                        }
                        case VAL_BOOL:
                            strcat(buffer, val.boolean ? "true" : "false");
                            break;
                        default:
                            break;
                    }
                    value_decref(&val);
                }
            }
            free(keys);
        }
        
        *result = vm_make_string(buffer);
        return true;
    }
    
    return false;
}
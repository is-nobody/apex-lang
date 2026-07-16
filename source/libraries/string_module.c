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

// decodes a UTF-8 sequence into a Unicode code point
// returns the number of bytes consumed
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

// checks if a Unicode code point is a letter (supports Latin, Cyrillic, and more)
// checks if a Unicode code point is a letter (all modern writing systems)
static bool unicode_is_letter(unsigned int cp) {
    return (cp >= 'A' && cp <= 'Z') ||
           (cp >= 'a' && cp <= 'z') ||
           // Latin Extended (European languages)
           (cp >= 0x00C0 && cp <= 0x024F) ||
           // Cyrillic (Russian, Ukrainian, Serbian, etc.)
           (cp >= 0x0400 && cp <= 0x04FF) ||
           // Greek
           (cp >= 0x0370 && cp <= 0x03FF) ||
           // Armenian
           (cp >= 0x0530 && cp <= 0x058F) ||
           // Hebrew
           (cp >= 0x0590 && cp <= 0x05FF) ||
           // Arabic + Persian + Urdu
           (cp >= 0x0600 && cp <= 0x06FF) ||
           (cp >= 0x0750 && cp <= 0x077F) ||
           (cp >= 0xFB50 && cp <= 0xFDFF) ||
           // Devanagari (Hindi, Nepali, etc.)
           (cp >= 0x0900 && cp <= 0x097F) ||
           // Bengali
           (cp >= 0x0980 && cp <= 0x09FF) ||
           // Gurmukhi (Punjabi)
           (cp >= 0x0A00 && cp <= 0x0A7F) ||
           // Gujarati
           (cp >= 0x0A80 && cp <= 0x0AFF) ||
           // Tamil
           (cp >= 0x0B80 && cp <= 0x0BFF) ||
           // Telugu
           (cp >= 0x0C00 && cp <= 0x0C7F) ||
           // Kannada
           (cp >= 0x0C80 && cp <= 0x0CFF) ||
           // Malayalam
           (cp >= 0x0D00 && cp <= 0x0D7F) ||
           // Sinhala
           (cp >= 0x0D80 && cp <= 0x0DFF) ||
           // Thai
           (cp >= 0x0E00 && cp <= 0x0E7F) ||
           // Lao
           (cp >= 0x0E80 && cp <= 0x0EFF) ||
           // Tibetan
           (cp >= 0x0F00 && cp <= 0x0FFF) ||
           // Myanmar (Burmese)
           (cp >= 0x1000 && cp <= 0x109F) ||
           // Georgian
           (cp >= 0x10A0 && cp <= 0x10FF) ||
           // Hangul Jamo + Syllables (Korean)
           (cp >= 0x1100 && cp <= 0x11FF) ||
           (cp >= 0x3130 && cp <= 0x318F) ||
           (cp >= 0xAC00 && cp <= 0xD7AF) ||
           // Ethiopic (Amharic, Tigrinya, etc.)
           (cp >= 0x1200 && cp <= 0x137F) ||
           (cp >= 0x2D80 && cp <= 0x2DDF) ||
           // Cherokee
           (cp >= 0x13A0 && cp <= 0x13FF) ||
           // Canadian Aboriginal Syllabics
           (cp >= 0x1400 && cp <= 0x167F) ||
           // Khmer (Cambodian)
           (cp >= 0x1780 && cp <= 0x17FF) ||
           // Mongolian
           (cp >= 0x1800 && cp <= 0x18AF) ||
           // Hiragana + Katakana (Japanese)
           (cp >= 0x3040 && cp <= 0x30FF) ||
           // CJK Unified Ideographs (Chinese, Japanese, Korean)
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           // Halfwidth and Fullwidth Forms
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

// dispatcher for string manipulation built-in functions
bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    if (arg_count < 1) return false;
    
    if (strcmp(name, "string.length") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        size_t char_count = utf8_strlen(args[0].string->chars);
        *result = vm_make_number((double)char_count);
        return true;
    }
    
    if (strcmp(name, "string.isletter") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        if (args[0].string->length == 0) {
            *result = vm_make_bool(false);
            return true;
        }
        unsigned int cp;
        utf8_decode(args[0].string->chars, &cp);
        *result = vm_make_bool(unicode_is_letter(cp));
        return true;
    }
    
    if (strcmp(name, "string.isnumber") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }
        if (args[0].string->length == 0) {
            *result = vm_make_bool(false);
            return true;
        }
        unsigned int cp;
        utf8_decode(args[0].string->chars, &cp);
        *result = vm_make_bool(unicode_is_digit(cp));
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
            Value k = vm_make_number((double)idx++);
            table_set(result->table, k, vm_make_string(token));
            value_decref(&k);
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
        Value* keys = table_keys(table, &count);

        if (keys && count > 0) {
            qsort(keys, count, sizeof(Value), compare_keys);
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
                value_decref(&keys[i]);
            }
            free(keys);
        }
        *result = vm_make_string(buffer);
        return true;
    }
    
    return false;
}
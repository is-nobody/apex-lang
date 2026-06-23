#include "string_module.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

bool string_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    if (arg_count < 1) return false;
    
    // string.len — return the length of a string
    if (strcmp(name, "string.len") == 0) {
        *result = vm_make_number(args[0].type == VAL_STRING ? strlen(args[0].string->chars) : 0);
        return true;
    }
    
    // string.upper — convert string to uppercase
    if (strcmp(name, "string.upper") == 0) {
        if (args[0].type == VAL_STRING) {
            char* str = strdup(args[0].string->chars);
            for (char* p = str; *p; p++) *p = toupper(*p);
            *result = vm_make_string(str);
            free(str);
        }
        return true;
    }
    
    // string.lower — convert string to lowercase
    if (strcmp(name, "string.lower") == 0) {
        if (args[0].type == VAL_STRING) {
            char* str = strdup(args[0].string->chars);
            for (char* p = str; *p; p++) *p = tolower(*p);
            *result = vm_make_string(str);
            free(str);
        }
        return true;
    }
    
    // string.trim — remove leading and trailing whitespace
    if (strcmp(name, "string.trim") == 0) {
        if (args[0].type == VAL_STRING) {
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
        }
        return true;
    }
    
    // string.find — locate substring, returns index or -1
    if (strcmp(name, "string.find") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            char* pos = strstr(args[0].string->chars, args[1].string->chars);
            *result = vm_make_number(pos ? (pos - args[0].string->chars) : -1);
        }
        return true;
    }
    
    // string.replace — replace first occurrence of substring
    if (strcmp(name, "string.replace") == 0) {
        if (arg_count >= 3 && args[0].type == VAL_STRING && 
            args[1].type == VAL_STRING && args[2].type == VAL_STRING) {
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
        }
        return true;
    }
    
    // string.sub — extract substring from start to end index
    if (strcmp(name, "string.sub") == 0) {
        if (arg_count >= 3 && args[0].type == VAL_STRING) {
            int start = args[1].type == VAL_NUMBER ? (int)args[1].number : 0;
            size_t end = args[2].type == VAL_NUMBER ? (size_t)args[2].number : strlen(args[0].string->chars);
            
            if (start < 0) start = 0;
            size_t len = strlen(args[0].string->chars);
            if ((size_t)end > len) end = (int)len;
            if ((size_t)start >= (size_t)end) {
                *result = vm_make_string("");
            } else {
                int len = end - start;
                char* sub = (char*)malloc(len + 1);
                strncpy(sub, args[0].string->chars + start, len);
                sub[len] = '\0';
                *result = vm_make_string(sub);
                free(sub);
            }
        }
        return true;
    }
    
    // string.split — split string by separator, returns 1-indexed table
    if (strcmp(name, "string.split") == 0) {
        if (args[0].type == VAL_STRING) {
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
        }
        return true;
    }
    
    // string.join — join table elements into a single string
    if (strcmp(name, "string.join") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_TABLE) {
            const char* sep = "";
            if (arg_count >= 2 && args[1].type == VAL_STRING) {
                sep = args[1].string->chars;
            }
            
            char buffer[65536] = "";
            Table* table = args[0].table;
            bool first = true;
            
            for (int i = 0; i < table->capacity; i++) {
                TableEntry* entry = table->entries[i];
                while (entry) {
                    if (!first) strcat(buffer, sep);
                    first = false;
                    
                    switch (entry->value.type) {
                        case VAL_STRING:
                            strcat(buffer, entry->value.string->chars);
                            break;
                        case VAL_NUMBER: {
                            char num[64];
                            snprintf(num, sizeof(num), "%g", entry->value.number);
                            strcat(buffer, num);
                            break;
                        }
                        default:
                            break;
                    }
                    entry = entry->next;
                }
            }
            
            *result = vm_make_string(buffer);
        }
        return true;
    }
    
    return false;
}
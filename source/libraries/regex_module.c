#include "regex_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vm.h"

// ========== Robust Backtracking Regex Engine ==========

typedef struct {
    bool matched;
    int length;
} MatchResult;

static MatchResult match_here_len(const char* pattern, const char* text);

static MatchResult match_star_len(int c, const char* pattern, const char* text) {
    int len = 0;
    // Greedily consume matching characters
    while (*text != '\0' && (*text == c || c == '.')) {
        text++;
        len++;
    }
    // Backtrack if the rest of the pattern doesn't match
    while (1) {
        MatchResult res = match_here_len(pattern, text);
        if (res.matched) {
            res.length += len;
            return res;
        }
        if (len == 0) break;
        len--;
        text--;
    }
    MatchResult fail = {false, 0};
    return fail;
}

static MatchResult match_plus_len(int c, const char* pattern, const char* text) {
    int len = 0;
    if (*text == '\0' && c != '.') {
        MatchResult fail = {false, 0};
        return fail;
    }
    if (*text != c && c != '.') {
        MatchResult fail = {false, 0};
        return fail;
    }
    while (*text != '\0' && (*text == c || c == '.')) {
        text++;
        len++;
    }
    while (1) {
        MatchResult res = match_here_len(pattern, text);
        if (res.matched) {
            res.length += len;
            return res;
        }
        if (len == 0) break;
        len--;
        text--;
    }
    MatchResult fail = {false, 0};
    return fail;
}

static MatchResult match_question_len(int c, const char* pattern, const char* text) {
    // Try matching 1 character first (greedy)
    if (*text != '\0' && (*text == c || c == '.')) {
        MatchResult res = match_here_len(pattern, text + 1);
        if (res.matched) {
            res.length += 1;
            return res;
        }
    }
    // Fallback to matching 0 characters
    return match_here_len(pattern, text);
}

static MatchResult match_here_len(const char* pattern, const char* text) {
    if (pattern[0] == '\0') {
        MatchResult res = {true, 0};
        return res;
    }
    if (pattern[1] == '*') return match_star_len(pattern[0], pattern + 2, text);
    if (pattern[1] == '+') return match_plus_len(pattern[0], pattern + 2, text);
    if (pattern[1] == '?') return match_question_len(pattern[0], pattern + 2, text);
    if (pattern[0] == '$' && pattern[1] == '\0') {
        MatchResult res = {*text == '\0', 0};
        return res;
    }
    if (*text != '\0' && (pattern[0] == '.' || pattern[0] == *text)) {
        MatchResult res = match_here_len(pattern + 1, text + 1);
        if (res.matched) {
            res.length += 1;
        }
        return res;
    }
    MatchResult fail = {false, 0};
    return fail;
}

static bool regex_search(const char* pattern, const char* text, int* out_start, int* out_len) {
    if (pattern[0] == '^') {
        MatchResult res = match_here_len(pattern + 1, text);
        if (res.matched) {
            if (out_start) *out_start = 0;
            if (out_len) *out_len = res.length;
            return true;
        }
        return false;
    }
    int offset = 0;
    do {
        MatchResult res = match_here_len(pattern, text + offset);
        if (res.matched) {
            if (out_start) *out_start = offset;
            if (out_len) *out_len = res.length;
            return true;
        }
        offset++;
    } while (*(text + offset - 1) != '\0');
    return false;
}

static bool regex_match(const char* pattern, const char* text, int* out_start, int* out_len) {
    if (pattern[0] == '^') {
        MatchResult res = match_here_len(pattern + 1, text);
        if (res.matched) {
            if (out_start) *out_start = 0;
            if (out_len) *out_len = res.length;
            return true;
        }
        return false;
    }
    MatchResult res = match_here_len(pattern, text);
    if (res.matched) {
        if (out_start) *out_start = 0;
        if (out_len) *out_len = res.length;
        return true;
    }
    return false;
}

static bool regex_fullmatch(const char* pattern, const char* text, int* out_start, int* out_len) {
    size_t len = strlen(pattern);
    // Safe check for empty pattern to prevent out-of-bounds read
    if (len > 0 && pattern[0] == '^' && pattern[len - 1] == '$') {
        MatchResult res = match_here_len(pattern, text);
        if (res.matched && res.length == (int)strlen(text)) {
            if (out_start) *out_start = 0;
            if (out_len) *out_len = res.length;
            return true;
        }
        return false;
    }
    char full_pattern[1024];
    snprintf(full_pattern, sizeof(full_pattern), "^%s$", pattern);
    return regex_search(full_pattern, text, out_start, out_len);
}

// ========== VM Integration Helpers ==========

static Table* create_table(VM* vm) {
    Value t_val = vm_make_table();
    return t_val.table;
}

// ========== Main Builtin Dispatcher ==========

bool regex_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    
    if (strcmp(name, "regex.escape") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STRING) {
            *result = vm_make_bool(false);
            return true;
        }
        const char* input = args[0].string->chars;
        int len = 0;
        const char* p = input;
        while (*p) {
            if (strchr("\\.^$|?*+()[]{}", *p)) len++;
            len++;
            p++;
        }
        char* escaped = (char*)malloc(len + 1);
        if (!escaped) {
            *result = vm_make_bool(false);
            return true;
        }
        char* dest = escaped;
        p = input;
        while (*p) {
            if (strchr("\\.^$|?*+()[]{}", *p)) {
                *dest++ = '\\';
            }
            *dest++ = *p++;
        }
        *dest = '\0';
        *result = vm_make_string(escaped);
        free(escaped);
        return true;
    }

    if (strcmp(name, "regex.search") == 0 || 
        strcmp(name, "regex.match") == 0 || 
        strcmp(name, "regex.fullmatch") == 0 ||
        strcmp(name, "regex.findall") == 0 ||
        strcmp(name, "regex.finditer") == 0 ||
        strcmp(name, "regex.sub") == 0 ||
        strcmp(name, "regex.split") == 0) {
        
        if (arg_count < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
            *result = vm_make_bool(false);
            return true;
        }

        const char* pattern_str = args[0].string->chars;
        const char* subject_str = args[1].string->chars;
        
        if (strcmp(name, "regex.search") == 0) {
            int start = 0, len = 0;
            bool found = regex_search(pattern_str, subject_str, &start, &len);
            if (found) {
                Table* match_table = create_table(vm);
                char match_str[256];
                int copy_len = len < 255 ? len : 255;
                strncpy(match_str, subject_str + start, copy_len);
                match_str[copy_len] = '\0';
                
                table_set(match_table, "0", vm_make_string(match_str));
                table_set(match_table, "start", vm_make_number((double)start));
                table_set(match_table, "end", vm_make_number((double)(start + len)));
                
                Value res_val;
                res_val.type = VAL_TABLE;
                res_val.table = match_table;
                *result = res_val;
            } else {
                *result = vm_make_bool(false);
            }
            return true;
        }
        
        if (strcmp(name, "regex.match") == 0) {
            int start = 0, len = 0;
            bool found = regex_match(pattern_str, subject_str, &start, &len);
            if (found) {
                Table* match_table = create_table(vm);
                char match_str[256];
                int copy_len = len < 255 ? len : 255;
                strncpy(match_str, subject_str + start, copy_len);
                match_str[copy_len] = '\0';
                
                table_set(match_table, "0", vm_make_string(match_str));
                table_set(match_table, "start", vm_make_number((double)start));
                table_set(match_table, "end", vm_make_number((double)(start + len)));
                
                Value res_val;
                res_val.type = VAL_TABLE;
                res_val.table = match_table;
                *result = res_val;
            } else {
                *result = vm_make_bool(false);
            }
            return true;
        }

        if (strcmp(name, "regex.fullmatch") == 0) {
            int start = 0, len = 0;
            bool found = regex_fullmatch(pattern_str, subject_str, &start, &len);
            if (found) {
                Table* match_table = create_table(vm);
                char match_str[256];
                int copy_len = len < 255 ? len : 255;
                strncpy(match_str, subject_str + start, copy_len);
                match_str[copy_len] = '\0';
                
                table_set(match_table, "0", vm_make_string(match_str));
                table_set(match_table, "start", vm_make_number((double)start));
                table_set(match_table, "end", vm_make_number((double)(start + len)));
                
                Value res_val;
                res_val.type = VAL_TABLE;
                res_val.table = match_table;
                *result = res_val;
            } else {
                *result = vm_make_bool(false);
            }
            return true;
        }

        if (strcmp(name, "regex.findall") == 0) {
            Table* results = create_table(vm);
            int count = 0;
            int offset = 0;
            int subject_len = strlen(subject_str);
            
            while (offset <= subject_len) {
                int start = 0, len = 0;
                if (regex_search(pattern_str, subject_str + offset, &start, &len)) {
                    int actual_start = offset + start;
                    int copy_len = len < 255 ? len : 255;
                    char match_str[256];
                    strncpy(match_str, subject_str + actual_start, copy_len);
                    match_str[copy_len] = '\0';
                    
                    char key_buf[32];
                    snprintf(key_buf, sizeof(key_buf), "%d", count++);
                    table_set(results, key_buf, vm_make_string(match_str));
                    
                    // Prevent infinite loops on zero-length matches (e.g., pattern "a*")
                    if (len == 0) {
                        offset = actual_start + 1;
                    } else {
                        offset = actual_start + len;
                    }
                } else {
                    break;
                }
            }
            
            Value res_val;
            res_val.type = VAL_TABLE;
            res_val.table = results;
            *result = res_val;
            return true;
        }
        
        if (strcmp(name, "regex.finditer") == 0) {
            Table* iterators = create_table(vm);
            int count = 0;
            int offset = 0;
            int subject_len = strlen(subject_str);
            
            while (offset <= subject_len) {
                int start = 0, len = 0;
                if (regex_search(pattern_str, subject_str + offset, &start, &len)) {
                    int actual_start = offset + start;
                    int copy_len = len < 255 ? len : 255;
                    char match_str[256];
                    strncpy(match_str, subject_str + actual_start, copy_len);
                    match_str[copy_len] = '\0';
                    
                    Table* m_obj = create_table(vm);
                    char key_buf[32];
                    snprintf(key_buf, sizeof(key_buf), "%d", count);
                    
                    table_set(m_obj, "0", vm_make_string(match_str));
                    table_set(m_obj, "start", vm_make_number((double)actual_start));
                    table_set(m_obj, "end", vm_make_number((double)(actual_start + len)));
                    
                    Value val;
                    val.type = VAL_TABLE;
                    val.table = m_obj;
                    table_set(iterators, key_buf, val);
                    
                    count++;
                    if (len == 0) {
                        offset = actual_start + 1;
                    } else {
                        offset = actual_start + len;
                    }
                } else {
                    break;
                }
            }
            
            Value res_val;
            res_val.type = VAL_TABLE;
            res_val.table = iterators;
            *result = res_val;
            return true;
        }

        if (strcmp(name, "regex.sub") == 0) {
            if (arg_count < 3 || args[2].type != VAL_STRING) {
                *result = vm_make_string(subject_str);
                return true;
            }
            
            const char* replacement = args[2].string->chars;
            int cap = strlen(subject_str) + strlen(replacement) + 1;
            char* buf = (char*)malloc(cap);
            int len = 0;
            
            int offset = 0;
            int subject_len = strlen(subject_str);
            
            while (offset <= subject_len) {
                int start = 0, match_len = 0;
                if (regex_search(pattern_str, subject_str + offset, &start, &match_len)) {
                    int actual_start = offset + start;
                    int prefix_len = actual_start - offset;
                    
                    if (len + prefix_len + strlen(replacement) >= cap) {
                        cap = (len + prefix_len + strlen(replacement)) * 2;
                        buf = (char*)realloc(buf, cap);
                    }
                    
                    memcpy(buf + len, subject_str + offset, prefix_len);
                    len += prefix_len;
                    strcpy(buf + len, replacement);
                    len += strlen(replacement);
                    
                    if (match_len == 0) {
                        offset = actual_start + 1;
                    } else {
                        offset = actual_start + match_len;
                    }
                } else {
                    break;
                }
            }
            
            int rem_len = subject_len - offset;
            if (len + rem_len + 1 >= cap) {
                cap = len + rem_len + 2;
                buf = (char*)realloc(buf, cap);
            }
            memcpy(buf + len, subject_str + offset, rem_len);
            len += rem_len;
            buf[len] = '\0';
            
            *result = vm_make_string(buf);
            free(buf);
            return true;
        }

        if (strcmp(name, "regex.split") == 0) {
            Table* results = create_table(vm);
            int count = 0;
            int offset = 0;
            int last_split = 0;
            int subject_len = strlen(subject_str);
            
            while (offset <= subject_len) {
                int start = 0, match_len = 0;
                if (regex_search(pattern_str, subject_str + offset, &start, &match_len)) {
                    int actual_start = offset + start;
                    int split_len = actual_start - last_split;
                    
                    char part[256];
                    int copy_len = split_len < 255 ? split_len : 255;
                    strncpy(part, subject_str + last_split, copy_len);
                    part[copy_len] = '\0';
                    
                    char key_buf[32];
                    snprintf(key_buf, sizeof(key_buf), "%d", count++);
                    table_set(results, key_buf, vm_make_string(part));
                    
                    if (match_len == 0) {
                        offset = actual_start + 1;
                    } else {
                        offset = actual_start + match_len;
                    }
                    last_split = offset;
                } else {
                    break;
                }
            }
            
            int rem_len = subject_len - last_split;
            char part[256];
            int copy_len = rem_len < 255 ? rem_len : 255;
            strncpy(part, subject_str + last_split, copy_len);
            part[copy_len] = '\0';
            
            char key_buf[32];
            snprintf(key_buf, sizeof(key_buf), "%d", count++);
            table_set(results, key_buf, vm_make_string(part));

            Value res_val;
            res_val.type = VAL_TABLE;
            res_val.table = results;
            *result = res_val;
            return true;
        }
    }

    return false;
}
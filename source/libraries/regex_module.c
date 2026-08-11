// source/libraries/regex_module.c
// Implementation of Regex Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "regex_module.h"
#include "vm.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

static int match_pattern(const char* text, int text_len, int text_pos,
                         const char* pattern, int pattern_len, int pattern_pos,
                         bool case_insensitive, bool dot_matches_newline);

static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                              // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));          // intern and box as value
}

static bool match_char(char c, char pc, bool ci) {
    if (pc == '.') return true;                                              // dot matches any char
    if (ci) return tolower((unsigned char)c) == tolower((unsigned char)pc);  // case insensitive compare
    return c == pc;                                                          // exact match
}

static bool esc_match(char esc, char c) {
    switch (esc) {
        case 'd': return isdigit((unsigned char)c);                          // digit class
        case 'D': return !isdigit((unsigned char)c);                         // non-digit class
        case 'w': return isalnum((unsigned char)c) || c == '_';              // word class
        case 'W': return !(isalnum((unsigned char)c) || c == '_');           // non-word class
        case 's': return isspace((unsigned char)c);                          // whitespace class
        case 'S': return !isspace((unsigned char)c);                         // non-whitespace class
        default:  return c == esc;                                           // literal escape
    }
}

static bool charclass_match_single(char c, const char* pattern, int pattern_pos, int* out_end, bool ci) {
    int pos = pattern_pos + 1;                                                       // skip opening bracket
    bool negated = false;                                                            // negation flag
    if (pattern[pos] == '^') { negated = true; pos++; }                              // check negation
    bool matched = false;                                                            // match result
    
    while (pattern[pos] != ']' && pattern[pos] != '\0') {                            // iterate class contents
        if (pattern[pos] == '\\' && pattern[pos + 1] != '\0') {                      // escape sequence
            pos++;                                                                   // skip backslash
            char esc = pattern[pos];                                                 // escaped char
            switch (esc) {
                case 's': if (isspace((unsigned char)c)) matched = true; break;      // whitespace
                case 'S': if (!isspace((unsigned char)c)) matched = true; break;     // non-whitespace
                case 'd': if (isdigit((unsigned char)c)) matched = true; break;      // digit
                case 'D': if (!isdigit((unsigned char)c)) matched = true; break;     // non-digit
                case 'w': if (isalnum((unsigned char)c) || c == '_') matched = true; break;  // word
                case 'W': if (!(isalnum((unsigned char)c) || c == '_')) matched = true; break;  // non-word
                default: if (match_char(c, esc, ci)) matched = true; break;          // literal
            }
            pos++;                                                                   // advance past escape
        } else if (pattern[pos + 1] == '-' && pattern[pos + 2] != ']' && pattern[pos + 2] != '\0') {  // range
            char start = pattern[pos];                                               // range start
            char end = pattern[pos + 2];                                             // range end
            if (ci) {                                                                // case insensitive
                char lc = (char)tolower((unsigned char)c);                           // lowercase char
                char ls = (char)tolower((unsigned char)start);                       // lowercase start
                char le = (char)tolower((unsigned char)end);                         // lowercase end
                if (lc >= ls && lc <= le) matched = true;                            // check range
            } else {
                if ((unsigned char)c >= (unsigned char)start && (unsigned char)c <= (unsigned char)end) matched = true;  // check range
            }
            pos += 3;                                                                // skip range
        } else {
            if (match_char(c, pattern[pos], ci)) matched = true;                     // single char
            pos++;                                                                   // advance
        }
    }
    
    if (out_end) *out_end = pos;                                                     // store end position
    return negated ? !matched : matched;                                             // return with negation
}

static int match_escape(const char* text, int text_len, int text_pos,
                        const char* pattern, int pattern_len, int pattern_pos,
                        bool ci, bool dotnl) {
    if (pattern_pos + 1 >= pattern_len) return -1;                                   // incomplete escape
    char esc = pattern[pattern_pos + 1];                                             // escape character
    int next_pos = pattern_pos + 2;                                                  // position after escape
    
    if (next_pos < pattern_len) {                                                    // check quantifier
        if (pattern[next_pos] == '*') {                                              // zero or more
            int max_end = text_pos;                                                  // maximum end
            while (max_end < text_len && esc_match(esc, text[max_end])) max_end++;   // find max match
            for (int i = max_end; i >= text_pos; i--) {                              // backtrack
                int r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r >= 0) return r;                                                // found match
            }
            return -1;                                                               // no match
        }
        if (pattern[next_pos] == '+') {                                              // one or more
            if (text_pos >= text_len || !esc_match(esc, text[text_pos])) return -1;  // need at least one
            int max_end = text_pos + 1;                                              // start after one
            while (max_end < text_len && esc_match(esc, text[max_end])) max_end++;   // find max match
            for (int i = max_end; i > text_pos; i--) {                               // backtrack
                int r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r >= 0) return r;                                                // found match
            }
            return -1;                                                               // no match
        }
        if (pattern[next_pos] == '?') {                                              // zero or one
            int r = match_pattern(text, text_len, text_pos, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try zero
            if (r >= 0) return r;                                                    // found match
            if (text_pos >= text_len || !esc_match(esc, text[text_pos])) return -1;  // need char for one
            return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try one
        }
    }
    
    if (text_pos >= text_len || !esc_match(esc, text[text_pos])) return -1;        // must match one
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos, ci, dotnl);  // continue
}

static int match_charclass(const char* text, int text_len, int text_pos,
                           const char* pattern, int pattern_len, int pattern_pos,
                           bool ci, bool dotnl) {
    int class_end;                                                                 // end of class
    if (!charclass_match_single(text[text_pos], pattern, pattern_pos, &class_end, ci)) return -1;  // check match
    if (pattern[class_end] != ']') return -1;                                      // must have closing bracket
    
    int next_pos = class_end + 1;                                                  // position after class
    
    if (next_pos < pattern_len) {                                                  // check quantifier
        if (pattern[next_pos] == '*') {                                            // zero or more
            int max_end = text_pos;                                                // maximum end
            while (max_end < text_len && charclass_match_single(text[max_end], pattern, pattern_pos, NULL, ci)) max_end++;  // find max
            for (int i = max_end; i >= text_pos; i--) {                            // backtrack
                int r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r >= 0) return r;                                              // found match
            }
            return -1;                                                             // no match
        }
        if (pattern[next_pos] == '+') {                                            // one or more
            if (text_pos >= text_len || !charclass_match_single(text[text_pos], pattern, pattern_pos, NULL, ci)) return -1;  // need one
            int max_end = text_pos + 1;                                            // start after one
            while (max_end < text_len && charclass_match_single(text[max_end], pattern, pattern_pos, NULL, ci)) max_end++;  // find max
            for (int i = max_end; i > text_pos; i--) {                             // backtrack
                int r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r >= 0) return r;                                              // found match
            }
            return -1;                                                             // no match
        }
        if (pattern[next_pos] == '?') {                                            // zero or one
            int r = match_pattern(text, text_len, text_pos, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try zero
            if (r >= 0) return r;                                                  // found match
            if (text_pos >= text_len || !charclass_match_single(text[text_pos], pattern, pattern_pos, NULL, ci)) return -1;  // need char
            return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try one
        }
    }
    
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos, ci, dotnl);  // continue
}

static int match_star(const char* text, int text_len, int text_pos,
                      const char* pattern, int pattern_len, int pattern_pos,
                      char pc, bool ci, bool dotnl) {
    int max_end = text_pos;                                                        // maximum end
    while (max_end < text_len) {                                                   // find maximum match
        if (pc == '.') {                                                           // dot character
            if (!dotnl && text[max_end] == '\n') break;                            // stop at newline if not dotall
        } else {
            if (!match_char(text[max_end], pc, ci)) break;                         // stop at non-match
        }
        max_end++;                                                                 // advance
    }
    for (int i = max_end; i >= text_pos; i--) {                                    // backtrack
        int r = match_pattern(text, text_len, i, pattern, pattern_len, pattern_pos + 2, ci, dotnl);
        if (r >= 0) return r;                                                      // found match
    }
    return -1;                                                                     // no match
}

static int match_plus(const char* text, int text_len, int text_pos,
                      const char* pattern, int pattern_len, int pattern_pos,
                      char pc, bool ci, bool dotnl) {
    if (text_pos >= text_len) return -1;                                           // need at least one char
    if (pc == '.') {                                                               // dot character
        if (!dotnl && text[text_pos] == '\n') return -1;                           // newline without dotall
    } else {
        if (!match_char(text[text_pos], pc, ci)) return -1;                        // must match first char
    }
    int max_end = text_pos + 1;                                                    // start after one
    while (max_end < text_len) {                                                   // find maximum match
        if (pc == '.') {                                                           // dot character
            if (!dotnl && text[max_end] == '\n') break;                            // stop at newline
        } else {
            if (!match_char(text[max_end], pc, ci)) break;                         // stop at non-match
        }
        max_end++;                                                                 // advance
    }
    for (int i = max_end; i > text_pos; i--) {                                     // backtrack (must match at least one)
        int r = match_pattern(text, text_len, i, pattern, pattern_len, pattern_pos + 2, ci, dotnl);
        if (r >= 0) return r;                                                      // found match
    }
    return -1;                                                                     // no match
}

static int match_question(const char* text, int text_len, int text_pos,
                          const char* pattern, int pattern_len, int pattern_pos,
                          char pc, bool ci, bool dotnl) {
    int r = match_pattern(text, text_len, text_pos, pattern, pattern_len, pattern_pos + 2, ci, dotnl);  // try zero
    if (r >= 0) return r;                                                          // found match
    if (text_pos >= text_len) return -1;                                           // need char for one
    if (pc == '.' && !dotnl && text[text_pos] == '\n') return -1;                  // newline without dotall
    if (pc != '.' && !match_char(text[text_pos], pc, ci)) return -1;               // must match char
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, pattern_pos + 2, ci, dotnl);  // try one
}

static int match_pattern(const char* text, int text_len, int text_pos,
                         const char* pattern, int pattern_len, int pattern_pos,
                         bool case_insensitive, bool dot_matches_newline) {
    if (pattern_pos >= pattern_len) return text_pos;                               // end of pattern, success
    
    if (text_pos > text_len) return -1;                                            // past end of text
    
    if (text_pos == text_len) {                                                    // at end of text
        if (pattern[pattern_pos] == '$') {                                         // end anchor
            return match_pattern(text, text_len, text_pos, pattern, pattern_len, pattern_pos + 1, case_insensitive, dot_matches_newline);  // continue
        }
        return -1;                                                                 // no more text to match
    }
    
    char pc = pattern[pattern_pos];                                                // current pattern char
    
    if (pc == '\\') {                                                              // escape sequence
        return match_escape(text, text_len, text_pos, pattern, pattern_len, pattern_pos, case_insensitive, dot_matches_newline);
    }
    
    if (pc == '[') {                                                               // character class
        return match_charclass(text, text_len, text_pos, pattern, pattern_len, pattern_pos, case_insensitive, dot_matches_newline);
    }
    
    if (pc == '^') {                                                               // start anchor
        if (text_pos != 0) return -1;                                              // must be at start
        return match_pattern(text, text_len, text_pos, pattern, pattern_len, pattern_pos + 1, case_insensitive, dot_matches_newline);  // continue
    }
    
    if (pc == '$') {                                                               // end anchor
        if (text_pos != text_len) return -1;                                       // must be at end
        return text_pos;                                                           // success
    }
    
    if (pattern_pos + 1 < pattern_len) {                                           // check for quantifier
        char next = pattern[pattern_pos + 1];                                      // next char
        if (next == '*') return match_star(text, text_len, text_pos, pattern, pattern_len, pattern_pos, pc, case_insensitive, dot_matches_newline);  // zero or more
        if (next == '+') return match_plus(text, text_len, text_pos, pattern, pattern_len, pattern_pos, pc, case_insensitive, dot_matches_newline);  // one or more
        if (next == '?') return match_question(text, text_len, text_pos, pattern, pattern_len, pattern_pos, pc, case_insensitive, dot_matches_newline);  // zero or one
    }
    
    if (pc == '.') {                                                               // dot character
        if (!dot_matches_newline && text[text_pos] == '\n') return -1;             // newline without dotall
        return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, pattern_pos + 1, case_insensitive, dot_matches_newline);  // continue
    }
    
    if (!match_char(text[text_pos], pc, case_insensitive)) return -1;              // must match literal char
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, pattern_pos + 1, case_insensitive, dot_matches_newline);  // continue
}

static Table* find_all_matches(const char* text, const char* pattern, bool ci, bool dotnl, VM* vm) {
    if (!text || !pattern) return NULL;                                            // validate inputs
    
    Table* result = table_create(8);                                               // create result table
    if (!result) return NULL;                                                      // allocation failed
    
    int text_len = (int)strlen(text);                                              // text length
    int pattern_len = (int)strlen(pattern);                                        // pattern length
    
    if (text_len == 0 || pattern_len == 0) return result;                          // empty input, return empty table
    
    int match_count = 0;                                                           // match counter
    
    for (int start = 0; start <= text_len; start++) {                              // iterate over text positions
        int end = match_pattern(text, text_len, start, pattern, pattern_len, 0, ci, dotnl);  // find match
        if (end > start) {                                                         // found non-empty match
            int match_len = end - start;                                           // match length
            char* match_str = (char*)malloc(match_len + 1);                        // allocate match string
            if (!match_str) {                                                      // allocation failed
                table_destroy(result);                                             // clean up
                return NULL;                                                       // return error
            }
            memcpy(match_str, text + start, match_len);                            // copy match
            match_str[match_len] = '\0';                                           // null terminate
            
            Value key = MAKE_NUMBER((double)(match_count + 1));                    // create index key
            Value val = make_string_val(vm, match_str);                            // create string value
            table_set(result, key, val);                                           // store in table
            value_decref(key);                                                     // release key
            value_decref(val);                                                     // release value
            free(match_str);                                                       // free temporary string
            
            match_count++;                                                         // increment counter
            start = end - 1;                                                       // continue after match
        }
    }
    
    return result;                                                                 // return result table
}

static char* substitute_pattern(const char* text, const char* pattern, const char* replacement, bool ci, bool dotnl) {
    if (!text || !pattern || !replacement) return NULL;                            // validate inputs
    
    int text_len = (int)strlen(text);                                              // text length
    int pattern_len = (int)strlen(pattern);                                        // pattern length
    int repl_len = (int)strlen(replacement);                                       // replacement length
    
    if (text_len == 0 || pattern_len == 0) {                                       // empty input
        char* result = (char*)malloc(text_len + 1);                                // allocate result
        if (result) strcpy(result, text);                                          // copy text if allocation succeeded
        return result;                                                             // return copy or NULL
    }
    
    int result_capacity = text_len + 256;                                          // initial capacity
    char* result = (char*)malloc(result_capacity);                                 // allocate result buffer
    if (!result) return NULL;                                                      // allocation failed
    
    int result_pos = 0;                                                            // current position in result
    int text_pos = 0;                                                              // current position in text
    
    while (text_pos <= text_len) {                                                 // process entire text
        int end = match_pattern(text, text_len, text_pos, pattern, pattern_len, 0, ci, dotnl);  // find match
        if (end > text_pos) {                                                      // found match
            while (result_pos + repl_len + 1 > result_capacity) {                  // ensure capacity
                result_capacity *= 2;                                              // double capacity
                char* new_result = (char*)realloc(result, result_capacity);        // reallocate
                if (!new_result) {                                                 // realloc failed
                    free(result);                                                  // free old buffer
                    return NULL;                                                   // return error
                }
                result = new_result;                                               // update pointer
            }
            memcpy(result + result_pos, replacement, repl_len);                    // copy replacement
            result_pos += repl_len;                                                // advance position
            text_pos = end;                                                        // skip matched text
        } else {                                                                   // no match at this position
            if (text_pos < text_len) {                                             // still have text
                if (result_pos + 2 > result_capacity) {                            // ensure capacity
                    result_capacity *= 2;                                          // double capacity
                    char* new_result = (char*)realloc(result, result_capacity);    // reallocate
                    if (!new_result) {                                             // realloc failed
                        free(result);                                              // free old buffer
                        return NULL;                                               // return error
                    }
                    result = new_result;                                           // update pointer
                }
                result[result_pos++] = text[text_pos++];                           // copy character
            } else {
                break;                                                             // end of text
            }
        }
    }
    
    result[result_pos] = '\0';                                                     // null terminate
    return result;                                                                 // return substituted string
}

static Table* split_by_pattern(const char* text, const char* pattern, bool ci, bool dotnl, VM* vm) {
    if (!text || !pattern) return NULL;                                            // validate inputs
    
    Table* result = table_create(8);                                               // create result table
    if (!result) return NULL;                                                      // allocation failed
    
    int text_len = (int)strlen(text);                                              // text length
    int pattern_len = (int)strlen(pattern);                                        // pattern length
    
    if (text_len == 0 || pattern_len == 0) {                                       // empty input
        if (text_len > 0) {                                                        // text but no pattern
            Value key = MAKE_NUMBER(1.0);                                          // key for first element
            Value val = make_string_val(vm, text);                                 // create string value
            table_set(result, key, val);                                           // store in table
            value_decref(key);                                                     // release key
            value_decref(val);                                                     // release value
        }
        return result;                                                             // return table
    }
    
    int split_count = 0;                                                           // split counter
    int segment_start = 0;                                                         // start of current segment
    bool first_segment = true;                                                     // track first segment for empty handling
    
    for (int pos = 0; pos <= text_len; pos++) {                                    // iterate over text
        int end = match_pattern(text, text_len, pos, pattern, pattern_len, 0, ci, dotnl);  // find delimiter
        if (end > pos) {                                                           // found delimiter
            int seg_len = pos - segment_start;                                     // segment length
            
            if (seg_len == 0 && first_segment) {
                // skip this empty segment
            } else {
                char* seg_str = (char*)malloc(seg_len > 0 ? seg_len + 1 : 1);  // allocate segment
                if (!seg_str) {                                                // allocation failed
                    table_destroy(result);                                     // clean up
                    return NULL;                                               // return error
                }
                if (seg_len > 0) {
                    memcpy(seg_str, text + segment_start, seg_len);            // copy segment
                    seg_str[seg_len] = '\0';                                   // null terminate
                } else {
                    seg_str[0] = '\0';                                         // empty string
                }
                
                Value key = MAKE_NUMBER((double)(split_count + 1));            // create index key
                Value val = make_string_val(vm, seg_str);                      // create string value
                table_set(result, key, val);                                   // store in table
                value_decref(key);                                             // release key
                value_decref(val);                                             // release value
                free(seg_str);                                                 // free temporary
                split_count++;                                                 // increment counter
            }
            
            pos = end - 1;                                                     // skip delimiter
            segment_start = end;                                               // start new segment
            first_segment = false;                                             // no longer first segment
        }
    }
    
    // Handle last segment
    if (segment_start <= text_len) {
        int seg_len = text_len - segment_start;                                // remaining length
        
        // Skip empty last segment if it's the first and only segment
        if (seg_len == 0 && split_count > 0) {
            return result;                                                     // don't add empty trailing segment
        }
        
        char* seg_str = (char*)malloc(seg_len > 0 ? seg_len + 1 : 1);          // allocate segment
        if (!seg_str) {                                                        // allocation failed
            table_destroy(result);                                             // clean up
            return NULL;                                                       // return error
        }
        if (seg_len > 0) {
            memcpy(seg_str, text + segment_start, seg_len);                    // copy segment
            seg_str[seg_len] = '\0';                                           // null terminate
        } else {
            seg_str[0] = '\0';                                                 // empty string
        }
        
        Value key = MAKE_NUMBER((double)(split_count + 1));                    // create index key
        Value val = make_string_val(vm, seg_str);                              // create string value
        table_set(result, key, val);                                           // store in table
        value_decref(key);                                                     // release key
        value_decref(val);                                                     // release value
        free(seg_str);                                                         // free temporary
    }
    
    return result;                                                             // return result table
}

static bool get_opts(VM* vm, Value opts_val, bool* ci, bool* dotnl) {
    *ci = false;                                                               // default case sensitive
    *dotnl = false;                                                            // default dot doesn't match newline
    
    // If not a table, use defaults (not an error)
    if (!IS_TABLE(opts_val)) {
        return true;                                                           // no options provided
    }
    
    Table* opts = AS_TABLE(opts_val);                                          // extract table
    if (!opts) return true;                                                    // invalid table, use defaults
    
    Value val;                                                                 // temporary value
    
    Value key_ic = MAKE_STRING(string_intern(&vm->intern_table, "ignorecase", 10));  // ignorecase key
    if (table_get(opts, key_ic, &val)) {                                             // key exists
        if (IS_BOOL(val)) *ci = AS_BOOL(val);                                        // set if boolean
        value_decref(val);                                                           // release value
    }
    value_decref(key_ic);                                                            // release key
    
    Value key_da = MAKE_STRING(string_intern(&vm->intern_table, "dotall", 6));       // dotall key
    if (table_get(opts, key_da, &val)) {                                             // key exists
        if (IS_BOOL(val)) *dotnl = AS_BOOL(val);                                     // set if boolean
        value_decref(val);                                                           // release value
    }
    value_decref(key_da);                                                            // release key
    
    return true;                                                                     // options processed
}

// dispatcher for regex built-in functions
bool regex_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "regex.findall") == 0) {                               // find all pattern matches
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                    // extract pattern
        const char* text = AS_STRING(args[1])->chars;                       // extract text
        
        if (!pattern || !text) {                                            // validate pointers
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        bool ci = false, dotnl = false;                                     // default options
        if (arg_count >= 3 && IS_TABLE(args[2])) {                          // options provided and is table
            if (!get_opts(vm, args[2], &ci, &dotnl)) {                      // process options
                *result = MAKE_NONE();                                      // return none on error
                return true;                                                // builtin handled
            }
        }
        
        Table* matches = find_all_matches(text, pattern, ci, dotnl, vm);  // find matches
        if (!matches) {                                                   // error occurred
            *result = MAKE_NONE();                                        // return none
            return true;                                                  // builtin handled
        }
        
        *result = MAKE_TABLE(matches);                                    // return result table
        return true;                                                      // builtin handled
    }
    
    if (strcmp(name, "regex.replace") == 0) {                                 // substitute pattern matches
        if (arg_count < 3 || !IS_STRING(args[0]) || !IS_STRING(args[1]) || !IS_STRING(args[2])) {  // validate arguments
            *result = MAKE_NONE();                                        // return none
            return true;                                                  // builtin handled
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                  // extract pattern
        const char* replacement = AS_STRING(args[1])->chars;              // extract replacement
        const char* text = AS_STRING(args[2])->chars;                     // extract text
        
        if (!pattern || !replacement || !text) {                          // validate pointers
            *result = MAKE_NONE();                                        // return none
            return true;                                                  // builtin handled
        }
        
        bool ci = false, dotnl = false;                                   // default options
        if (arg_count >= 4 && IS_TABLE(args[3])) {                        // options provided and is table
            if (!get_opts(vm, args[3], &ci, &dotnl)) {                    // process options
                *result = MAKE_NONE();                                    // return none on error
                return true;                                              // builtin handled
            }
        }
        
        char* substituted = substitute_pattern(text, pattern, replacement, ci, dotnl);  // perform substitution
        if (!substituted) {                                                             // error occurred
            *result = MAKE_NONE();                                                      // return none
            return true;                                                                // builtin handled
        }
        
        *result = make_string_val(vm, substituted);                         // return result string
        free(substituted);                                                  // free temporary
        return true;                                                        // builtin handled
    }
    
    if (strcmp(name, "regex.split") == 0) {                                 // split by pattern
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                    // extract pattern
        const char* text = AS_STRING(args[1])->chars;                       // extract text
        
        if (!pattern || !text) {                                            // validate pointers
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        bool ci = false, dotnl = false;                                     // default options
        if (arg_count >= 3 && IS_TABLE(args[2])) {                          // options provided and is table
            if (!get_opts(vm, args[2], &ci, &dotnl)) {                      // process options
                *result = MAKE_NONE();                                      // return none on error
                return true;                                                // builtin handled
            }
        }
        
        Table* splits = split_by_pattern(text, pattern, ci, dotnl, vm);     // split text
        if (!splits) {                                                      // error occurred
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        *result = MAKE_TABLE(splits);                                       // return result table
        return true;                                                        // builtin handled
    }
    
    if (strcmp(name, "regex.search") == 0) {                                // search for first match
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                    // extract pattern
        const char* text = AS_STRING(args[1])->chars;                       // extract text
        
        if (!pattern || !text) {                                            // validate pointers
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        bool ci = false, dotnl = false;                                     // default options
        if (arg_count >= 3 && IS_TABLE(args[2])) {                          // options provided and is table
            if (!get_opts(vm, args[2], &ci, &dotnl)) {                      // process options
                *result = MAKE_NONE();                                      // return none on error
                return true;                                                // builtin handled
            }
        }
        
        int text_len = (int)strlen(text);                                   // text length
        int pattern_len = (int)strlen(pattern);                             // pattern length
        
        if (text_len == 0 || pattern_len == 0) {                            // empty input
            *result = MAKE_TABLE(table_create(4));                          // return empty table
            return true;                                                    // builtin handled
        }
        
        Table* search_result = table_create(4);                             // create result table
        if (!search_result) {                                               // allocation failed
            *result = MAKE_NONE();                                          // return none
            return true;                                                    // builtin handled
        }
        
        for (int start = 0; start <= text_len; start++) {                   // search all positions
            int end = match_pattern(text, text_len, start, pattern, pattern_len, 0, ci, dotnl);  // find match
            if (end > start) {                                                         // found non-empty match
                int match_len = end - start;                                           // match length
                char* match_str = (char*)malloc(match_len + 1);                        // allocate match string
                if (!match_str) {                                                      // allocation failed
                    table_destroy(search_result);                                      // clean up
                    *result = MAKE_NONE();                                             // return none
                    return true;                                                       // builtin handled
                }
                memcpy(match_str, text + start, match_len);                            // copy match
                match_str[match_len] = '\0';                                           // null terminate
                
                Value ks = MAKE_STRING(string_intern(&vm->intern_table, "start", 5));  // start key
                Value vs = MAKE_NUMBER((double)(start + 1));                           // start value (1-based)
                table_set(search_result, ks, vs);                                      // store start
                value_decref(ks);                                                      // release key
                value_decref(vs);                                                      // release value
                
                Value ke = MAKE_STRING(string_intern(&vm->intern_table, "end", 3));    // end key
                Value ve = MAKE_NUMBER((double)end);                                   // end value
                table_set(search_result, ke, ve);                                      // store end
                value_decref(ke);                                                      // release key
                value_decref(ve);                                                      // release value
                
                Value km = MAKE_STRING(string_intern(&vm->intern_table, "match", 5));  // match key
                Value vm_val = make_string_val(vm, match_str);                         // match value
                table_set(search_result, km, vm_val);                                  // store match
                value_decref(km);                                                      // release key
                value_decref(vm_val);                                                  // release value
                
                free(match_str);                                                       // free temporary
                break;                                                                 // stop after first match
            }
        }
        
        *result = MAKE_TABLE(search_result);  // return result table
        return true;                          // builtin handled
    }
    
    return false;                             // not a recognized builtin
}
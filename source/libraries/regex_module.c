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

// group capture result structure
typedef struct {
    int start;          // group start position in text
    int end;            // group end position in text
    bool matched;       // whether group participated in match
} GroupCapture;

// maximum number of capture groups supported
#define MAX_GROUPS 32

// match result structure for pattern matching
typedef struct {
    int end_pos;                        // end position of match in text
    GroupCapture groups[MAX_GROUPS];    // capture groups
    int group_count;                    // number of groups in pattern
} MatchResult;

static MatchResult match_pattern(const char* text, int text_len, int text_pos,
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
        case 't': return c == '\t';                                          // tab character
        case 'n': return c == '\n';                                          // newline character
        case 'r': return c == '\r';                                          // carriage return
        case 'f': return c == '\f';                                          // form feed
        case 'v': return c == '\v';                                          // vertical tab
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
                case 's': if (isspace((unsigned char)c)) matched = true; break;      // whitespace in class
                case 'S': if (!isspace((unsigned char)c)) matched = true; break;     // non-whitespace in class
                case 'd': if (isdigit((unsigned char)c)) matched = true; break;      // digit in class
                case 'D': if (!isdigit((unsigned char)c)) matched = true; break;     // non-digit in class
                case 'w': if (isalnum((unsigned char)c) || c == '_') matched = true; break;  // word in class
                case 'W': if (!(isalnum((unsigned char)c) || c == '_')) matched = true; break;  // non-word in class
                case 't': if (c == '\t') matched = true; break;                      // tab in class
                case 'n': if (c == '\n') matched = true; break;                      // newline in class
                case 'r': if (c == '\r') matched = true; break;                      // carriage return in class
                case 'f': if (c == '\f') matched = true; break;                      // form feed in class
                case 'v': if (c == '\v') matched = true; break;                      // vertical tab in class
                default: if (match_char(c, esc, ci)) matched = true; break;          // literal in class
            }
            pos++;                                                                   // advance past escape
        } else if (pattern[pos + 1] == '-' && pattern[pos + 2] != ']' && pattern[pos + 2] != '\0') {  // range
            char start = pattern[pos];                                               // range start
            char end = pattern[pos + 2];                                             // range end
            if (ci) {                                                                // case insensitive range
                char lc = (char)tolower((unsigned char)c);                           // lowercase char
                char ls = (char)tolower((unsigned char)start);                       // lowercase start
                char le = (char)tolower((unsigned char)end);                         // lowercase end
                if (lc >= ls && lc <= le) matched = true;                            // check range
            } else {
                if ((unsigned char)c >= (unsigned char)start && (unsigned char)c <= (unsigned char)end) matched = true;  // check range
            }
            pos += 3;                                                                // skip range
        } else {
            if (match_char(c, pattern[pos], ci)) matched = true;                     // single char in class
            pos++;                                                                   // advance
        }
    }
    
    if (out_end) *out_end = pos;                                                     // store end position
    return negated ? !matched : matched;                                             // return with negation
}

// find matching closing parenthesis for group starting at open_pos
static int find_group_end(const char* pattern, int pattern_len, int open_pos) {
    int depth = 1;                                                           // track nesting depth
    bool in_class = false;                                                   // char class flag
    for (int i = open_pos + 1; i < pattern_len; i++) {
        if (pattern[i] == '\\' && i + 1 < pattern_len) {                     // skip escapes
            i++;
            continue;
        }
        if (pattern[i] == '[') in_class = true;                              // enter char class
        else if (pattern[i] == ']') in_class = false;                        // exit char class
        else if (!in_class) {
            if (pattern[i] == '(') depth++;                                  // nested group
            else if (pattern[i] == ')') {                                    // close group
                depth--;
                if (depth == 0) return i;                                    // found matching close
            }
        }
    }
    return -1;                                                               // no matching close
}

// get group number for group at position (accounting for nested groups)
static int get_group_number(const char* pattern, int pattern_len, int group_pos) {
    int count = 0;                                                           // group counter
    bool in_class = false;                                                   // char class flag
    for (int i = 0; i < group_pos; i++) {
        if (pattern[i] == '\\' && i + 1 < pattern_len) {                     // skip escapes
            i++;
            continue;
        }
        if (pattern[i] == '[') in_class = true;                              // enter char class
        else if (pattern[i] == ']') in_class = false;                        // exit char class
        else if (pattern[i] == '(' && !in_class) {
            if (i + 2 < pattern_len && pattern[i + 1] == '?' && pattern[i + 2] == ':') {  // non-capturing group
                i += 2;                                                      // skip (?:
                continue;
            }
            count++;                                                         // capturing group
        }
    }
    return count;                                                            // zero-based group index
}

// initialize match result with empty groups
static MatchResult init_match_result(int end_pos, int group_count) {
    MatchResult result;
    result.end_pos = end_pos;                                                // set end position
    result.group_count = group_count;                                        // set group count
    for (int i = 0; i < MAX_GROUPS; i++) {
        result.groups[i].start = -1;                                         // no start
        result.groups[i].end = -1;                                           // no end
        result.groups[i].matched = false;                                    // not matched
    }
    return result;
}

// set group capture in match result
static void set_group_capture(MatchResult* result, int group_index, int start, int end) {
    if (group_index >= 0 && group_index < MAX_GROUPS) {                      // validate index
        result->groups[group_index].start = start;                           // set start
        result->groups[group_index].end = end;                               // set end
        result->groups[group_index].matched = true;                          // mark matched
    }
}

// merge group captures from sub-match into parent result
static void merge_groups(MatchResult* parent, const MatchResult* child) {
    if (!child) return;                                                      // guard against null
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (child->groups[i].matched) {                                      // copy matched groups
            parent->groups[i].start = child->groups[i].start;
            parent->groups[i].end = child->groups[i].end;
            parent->groups[i].matched = true;
        }
    }
    if (child->group_count > parent->group_count) {
        parent->group_count = child->group_count;                            // update group count
    }
}

// match escape sequence with quantifier support
static MatchResult match_escape(const char* text, int text_len, int text_pos,
                                const char* pattern, int pattern_len, int pattern_pos,
                                bool ci, bool dotnl) {
    if (pattern_pos + 1 >= pattern_len) return init_match_result(-1, 0);     // incomplete escape
    char esc = pattern[pattern_pos + 1];                                     // escape character
    int next_pos = pattern_pos + 2;                                          // position after escape
    
    if (next_pos < pattern_len) {                                            // check quantifier
        if (pattern[next_pos] == '*') {                                      // zero or more
            int max_end = text_pos;                                          // maximum end
            while (max_end < text_len && esc_match(esc, text[max_end])) max_end++;  // find max match
            for (int i = max_end; i >= text_pos; i--) {                      // backtrack
                MatchResult r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r.end_pos >= 0) return r;                                // found match
            }
            return init_match_result(-1, 0);                                 // no match
        }
        if (pattern[next_pos] == '+') {                                      // one or more
            if (text_pos >= text_len || !esc_match(esc, text[text_pos])) return init_match_result(-1, 0);  // need at least one
            int max_end = text_pos + 1;                                      // start after one
            while (max_end < text_len && esc_match(esc, text[max_end])) max_end++;  // find max match
            for (int i = max_end; i > text_pos; i--) {                       // backtrack
                MatchResult r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r.end_pos >= 0) return r;                                // found match
            }
            return init_match_result(-1, 0);                                 // no match
        }
        if (pattern[next_pos] == '?') {                                      // zero or one
            MatchResult r = match_pattern(text, text_len, text_pos, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try zero
            if (r.end_pos >= 0) return r;                                    // found match
            if (text_pos >= text_len || !esc_match(esc, text[text_pos])) return init_match_result(-1, 0);  // need char for one
            return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try one
        }
    }
    
    if (text_pos >= text_len || !esc_match(esc, text[text_pos])) return init_match_result(-1, 0);  // must match one
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos, ci, dotnl);  // continue
}

// match character class with quantifier support
static MatchResult match_charclass(const char* text, int text_len, int text_pos,
                                   const char* pattern, int pattern_len, int pattern_pos,
                                   bool ci, bool dotnl) {
    int class_end;                                                           // end of class
    if (text_pos >= text_len) return init_match_result(-1, 0);               // no text to match
    if (!charclass_match_single(text[text_pos], pattern, pattern_pos, &class_end, ci)) return init_match_result(-1, 0);  // check match
    if (pattern[class_end] != ']') return init_match_result(-1, 0);          // must have closing bracket
    
    int next_pos = class_end + 1;                                            // position after class
    
    if (next_pos < pattern_len) {                                            // check quantifier
        if (pattern[next_pos] == '*') {                                      // zero or more
            int max_end = text_pos;                                          // maximum end
            while (max_end < text_len && charclass_match_single(text[max_end], pattern, pattern_pos, NULL, ci)) max_end++;  // find max
            for (int i = max_end; i >= text_pos; i--) {                      // backtrack
                MatchResult r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r.end_pos >= 0) return r;                                // found match
            }
            return init_match_result(-1, 0);                                 // no match
        }
        if (pattern[next_pos] == '+') {                                      // one or more
            if (text_pos >= text_len || !charclass_match_single(text[text_pos], pattern, pattern_pos, NULL, ci)) return init_match_result(-1, 0);  // need one
            int max_end = text_pos + 1;                                      // start after one
            while (max_end < text_len && charclass_match_single(text[max_end], pattern, pattern_pos, NULL, ci)) max_end++;  // find max
            for (int i = max_end; i > text_pos; i--) {                       // backtrack
                MatchResult r = match_pattern(text, text_len, i, pattern, pattern_len, next_pos + 1, ci, dotnl);
                if (r.end_pos >= 0) return r;                                // found match
            }
            return init_match_result(-1, 0);                                 // no match
        }
        if (pattern[next_pos] == '?') {                                      // zero or one
            MatchResult r = match_pattern(text, text_len, text_pos, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try zero
            if (r.end_pos >= 0) return r;                                    // found match
            if (text_pos >= text_len || !charclass_match_single(text[text_pos], pattern, pattern_pos, NULL, ci)) return init_match_result(-1, 0);  // need char
            return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos + 1, ci, dotnl);  // try one
        }
    }
    
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, next_pos, ci, dotnl);  // continue
}

// match star quantifier for literal char or dot
static MatchResult match_star(const char* text, int text_len, int text_pos,
                              const char* pattern, int pattern_len, int pattern_pos,
                              char pc, bool ci, bool dotnl) {
    int max_end = text_pos;                                                  // maximum end
    while (max_end < text_len) {                                             // find maximum match
        if (pc == '.') {                                                     // dot character
            if (!dotnl && text[max_end] == '\n') break;                      // stop at newline if not dotall
        } else {
            if (!match_char(text[max_end], pc, ci)) break;                   // stop at non-match
        }
        max_end++;                                                           // advance
    }
    for (int i = max_end; i >= text_pos; i--) {                              // backtrack
        MatchResult r = match_pattern(text, text_len, i, pattern, pattern_len, pattern_pos + 2, ci, dotnl);
        if (r.end_pos >= 0) return r;                                        // found match
    }
    return init_match_result(-1, 0);                                         // no match
}

// match plus quantifier for literal char or dot
static MatchResult match_plus(const char* text, int text_len, int text_pos,
                              const char* pattern, int pattern_len, int pattern_pos,
                              char pc, bool ci, bool dotnl) {
    if (text_pos >= text_len) return init_match_result(-1, 0);               // need at least one char
    if (pc == '.') {                                                         // dot character
        if (!dotnl && text[text_pos] == '\n') return init_match_result(-1, 0);  // newline without dotall
    } else {
        if (!match_char(text[text_pos], pc, ci)) return init_match_result(-1, 0);  // must match first char
    }
    int max_end = text_pos + 1;                                              // start after one
    while (max_end < text_len) {                                             // find maximum match
        if (pc == '.') {                                                     // dot character
            if (!dotnl && text[max_end] == '\n') break;                      // stop at newline
        } else {
            if (!match_char(text[max_end], pc, ci)) break;                   // stop at non-match
        }
        max_end++;                                                           // advance
    }
    for (int i = max_end; i > text_pos; i--) {                               // backtrack (must match at least one)
        MatchResult r = match_pattern(text, text_len, i, pattern, pattern_len, pattern_pos + 2, ci, dotnl);
        if (r.end_pos >= 0) return r;                                        // found match
    }
    return init_match_result(-1, 0);                                         // no match
}

// match question quantifier for literal char or dot
static MatchResult match_question(const char* text, int text_len, int text_pos,
                                  const char* pattern, int pattern_len, int pattern_pos,
                                  char pc, bool ci, bool dotnl) {
    MatchResult r = match_pattern(text, text_len, text_pos, pattern, pattern_len, pattern_pos + 2, ci, dotnl);  // try zero
    if (r.end_pos >= 0) return r;                                            // found match
    if (text_pos >= text_len) return init_match_result(-1, 0);               // need char for one
    if (pc == '.' && !dotnl && text[text_pos] == '\n') return init_match_result(-1, 0);  // newline without dotall
    if (pc != '.' && !match_char(text[text_pos], pc, ci)) return init_match_result(-1, 0);  // must match char
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, pattern_pos + 2, ci, dotnl);  // try one
}

// find end of alternation scope starting at pattern_pos
static int find_alternation_end(const char* pattern, int pattern_len, int pattern_pos) {
    int depth = 0;                                                           // nesting depth
    bool in_class = false;                                                   // char class flag
    for (int i = pattern_pos; i < pattern_len; i++) {
        if (pattern[i] == '\\' && i + 1 < pattern_len) {                     // skip escapes
            i++;
            continue;
        }
        if (pattern[i] == '[') in_class = true;                              // enter char class
        else if (pattern[i] == ']') in_class = false;                        // exit char class
        else if (!in_class) {
            if (pattern[i] == '(') depth++;                                  // nested group
            else if (pattern[i] == ')') {                                    // close group
                if (depth == 0) return i;                                    // end of current scope
                depth--;
            }
            else if (pattern[i] == '|' && depth == 0) {
                return i;                                                    // found alternation at current level
            }
        }
    }
    return pattern_len;                                                      // end of pattern
}

// find full scope end for alternation
static int find_scope_end(const char* pattern, int pattern_len, int pattern_pos) {
    int depth = 0;                                                           // nesting depth
    bool in_class = false;                                                   // char class flag
    for (int i = pattern_pos; i < pattern_len; i++) {
        if (pattern[i] == '\\' && i + 1 < pattern_len) {                     // skip escapes
            i++;
            continue;
        }
        if (pattern[i] == '[') in_class = true;                              // enter char class
        else if (pattern[i] == ']') in_class = false;                        // exit char class
        else if (!in_class) {
            if (pattern[i] == '(') depth++;                                  // nested group
            else if (pattern[i] == ')') {                                    // close group
                if (depth == 0) return i;                                    // end of scope
                depth--;
            }
        }
    }
    return pattern_len;                                                      // end of pattern
}

// match alternation at pattern position
static MatchResult match_alternation(const char* text, int text_len, int text_pos,
                                     const char* pattern, int pattern_len, int pattern_pos,
                                     int scope_end, bool ci, bool dotnl) {
    int alt_start = pattern_pos;                                             // start of first alternative
    int pos = pattern_pos;                                                   // current scan position
    int depth = 0;                                                           // nesting depth
    bool in_class = false;                                                   // char class flag
    
    while (pos <= scope_end) {                                               // iterate alternatives
        if (pos < scope_end && pattern[pos] == '\\' && pos + 1 < scope_end) {  // skip escapes
            pos += 2;
            continue;
        }
        if (pos < scope_end && pattern[pos] == '[') {                        // enter char class
            in_class = true;
            pos++;
            continue;
        }
        if (pos < scope_end && pattern[pos] == ']') {                        // exit char class
            in_class = false;
            pos++;
            continue;
        }
        if (pos < scope_end && pattern[pos] == '(' && !in_class) {           // nested group
            depth++;
            pos++;
            continue;
        }
        if (pos < scope_end && pattern[pos] == ')' && !in_class) {           // close nested group
            if (depth > 0) depth--;
            pos++;
            continue;
        }
        
        if (!in_class && depth == 0 && (pos == scope_end || pattern[pos] == '|')) {  // found alternation separator or end
            int alt_end = pos;                                               // end of current alternative
            
            if (alt_end >= alt_start) {                                      // allow empty alternative
                MatchResult result = match_pattern(text, text_len, text_pos,
                                                   pattern, alt_end, alt_start,
                                                   ci, dotnl);
                if (result.end_pos >= 0) {                                   // alternative matched
                    MatchResult rest = match_pattern(text, text_len, result.end_pos,
                                                     pattern, pattern_len, scope_end + 1,
                                                     ci, dotnl);
                    if (rest.end_pos >= 0) {                                 // rest matched
                        merge_groups(&rest, &result);                        // merge captures
                        rest.end_pos = result.end_pos;                       // fix end position
                        return rest;
                    }
                }
            }
            
            if (pos == scope_end) break;                                     // no more alternatives
            alt_start = pos + 1;                                             // move to next alternative
        }
        pos++;
    }
    
    return init_match_result(-1, 0);                                         // no alternative matched
}

// match group at pattern position (pattern[pattern_pos] == '(')
static MatchResult match_group(const char* text, int text_len, int text_pos,
                               const char* pattern, int pattern_len, int pattern_pos,
                               bool ci, bool dotnl) {
    int close_pos = find_group_end(pattern, pattern_len, pattern_pos);       // find matching close paren
    if (close_pos < 0) return init_match_result(-1, 0);                      // no matching close
    
    int next_pos = close_pos + 1;                                            // position after group
    char quant = '\0';                                                       // quantifier after group
    if (next_pos < pattern_len) {
        quant = pattern[next_pos];
        if (quant != '*' && quant != '+' && quant != '?') quant = '\0';      // only these quantifiers supported
    }
    
    bool is_capturing = true;                                                // capturing group flag
    int group_index = -1;                                                    // group index
    int group_start = pattern_pos + 1;                                       // start of group content
    
    if (pattern_pos + 2 < pattern_len && pattern[pattern_pos + 1] == '?' && pattern[pattern_pos + 2] == ':') {
        is_capturing = false;                                                // non-capturing group
        group_start = pattern_pos + 3;                                       // skip (?:
    } else {
        group_index = get_group_number(pattern, pattern_len, pattern_pos);   // get group index
    }
    
    // helper: match group content once
    #define MATCH_GROUP_ONCE(pos) \
        (group_start >= close_pos ? init_match_result((pos), 0) : \
         match_pattern(text, text_len, (pos), pattern, close_pos, group_start, ci, dotnl))
    
    if (quant == '\0') {                                                     // no quantifier on group
        MatchResult inner = MATCH_GROUP_ONCE(text_pos);
        if (inner.end_pos < 0) return inner;                                 // group failed
        
        MatchResult best = init_match_result(-1, 0);                         // best result
        int best_group2_len = -1;                                            // track length of group 2 for greedy selection
        
        for (int end = inner.end_pos; end >= text_pos; end--) {              // backtrack from longest to shortest
            MatchResult rest = match_pattern(text, text_len, end, pattern, pattern_len, close_pos + 1, ci, dotnl);
            if (rest.end_pos >= 0) {                                         // rest matched
                if (is_capturing && group_index >= 0) {
                    set_group_capture(&rest, group_index, text_pos, end);    // capture outer group
                    if (rest.group_count < group_index + 1) rest.group_count = group_index + 1;
                }
                for (int i = 0; i < MAX_GROUPS; i++) {                       // merge inner groups
                    if (inner.groups[i].matched && inner.groups[i].end <= end) {
                        rest.groups[i].start = inner.groups[i].start;
                        rest.groups[i].end = inner.groups[i].end;
                        rest.groups[i].matched = true;
                    }
                }
                if (inner.group_count > rest.group_count) {
                    rest.group_count = inner.group_count;                    // update group count
                }
                
                if (best.end_pos < 0) {                                      // first match found
                    best = rest;
                    if (rest.group_count > 1 && rest.groups[1].matched) {
                        best_group2_len = rest.groups[1].end - rest.groups[1].start;
                    }
                } else {
                    int current_group2_len = -1;                             // check if this match is more greedy
                    if (rest.group_count > 1 && rest.groups[1].matched) {
                        current_group2_len = rest.groups[1].end - rest.groups[1].start;
                    }
                    if (current_group2_len > best_group2_len) {              // prefer longer group 2
                        best = rest;
                        best_group2_len = current_group2_len;
                    }
                }
            }
        }
        
        if (best.end_pos >= 0) return best;                                  // return best match
        return init_match_result(-1, 0);                                     // no match
    }
    
    if (quant == '?') {                                                      // zero or one group
        MatchResult zero = match_pattern(text, text_len, text_pos, pattern, pattern_len, next_pos + 1, ci, dotnl);
        if (zero.end_pos >= 0) return zero;                                  // try zero
        
        MatchResult one = MATCH_GROUP_ONCE(text_pos);
        if (one.end_pos < 0) return one;                                     // try one
        
        if (is_capturing && group_index >= 0) {
            set_group_capture(&one, group_index, text_pos, one.end_pos);     // capture group
            if (one.group_count < group_index + 1) one.group_count = group_index + 1;
        }
        
        MatchResult rest = match_pattern(text, text_len, one.end_pos, pattern, pattern_len, next_pos + 1, ci, dotnl);
        if (rest.end_pos < 0) return init_match_result(-1, 0);               // rest failed
        merge_groups(&rest, &one);                                           // merge captures
        return rest;
    }
    
    if (quant == '*') {                                                      // zero or more group
        MatchResult zero = match_pattern(text, text_len, text_pos, pattern, pattern_len, next_pos + 1, ci, dotnl);
        if (zero.end_pos >= 0) return zero;                                  // try zero
        
        int max_reps = 0;                                                    // maximum repetitions
        int pos = text_pos;
        while (pos <= text_len) {                                            // find max repetitions
            MatchResult inner = MATCH_GROUP_ONCE(pos);
            if (inner.end_pos < 0) break;
            pos = inner.end_pos;
            max_reps++;
        }
        
        for (int r = max_reps; r >= 1; r--) {                                // backtrack from max to 1
            int cur = text_pos;
            MatchResult combined = init_match_result(text_pos, 0);
            int last_start = -1;
            
            for (int i = 0; i < r; i++) {                                    // build combined result
                last_start = cur;
                MatchResult inner = MATCH_GROUP_ONCE(cur);
                if (inner.end_pos < 0) break;
                cur = inner.end_pos;
                merge_groups(&combined, &inner);
            }
            
            if (is_capturing && group_index >= 0 && last_start >= 0) {
                set_group_capture(&combined, group_index, last_start, cur);  // capture last repetition
                if (combined.group_count < group_index + 1) combined.group_count = group_index + 1;
            }
            
            MatchResult rest = match_pattern(text, text_len, cur, pattern, pattern_len, next_pos + 1, ci, dotnl);
            if (rest.end_pos >= 0) {                                         // rest matched
                merge_groups(&rest, &combined);
                return rest;
            }
        }
        return init_match_result(-1, 0);                                     // no match
    }
    
    if (quant == '+') {                                                      // one or more group
        MatchResult first = MATCH_GROUP_ONCE(text_pos);
        if (first.end_pos < 0) return first;                                 // need at least one
        
        int pos = first.end_pos;
        int max_reps = 1;                                                    // already matched one
        while (pos <= text_len) {                                            // find max repetitions
            MatchResult inner = MATCH_GROUP_ONCE(pos);
            if (inner.end_pos < 0) break;
            pos = inner.end_pos;
            max_reps++;
        }
        
        for (int r = max_reps; r >= 1; r--) {                                // backtrack from max to 1
            int cur = text_pos;
            MatchResult combined = init_match_result(text_pos, 0);
            int last_start = -1;
            
            for (int i = 0; i < r; i++) {                                    // build combined result
                last_start = cur;
                MatchResult inner = MATCH_GROUP_ONCE(cur);
                if (inner.end_pos < 0) break;
                cur = inner.end_pos;
                merge_groups(&combined, &inner);
            }
            
            if (is_capturing && group_index >= 0 && last_start >= 0) {
                set_group_capture(&combined, group_index, last_start, cur);  // capture last repetition
                if (combined.group_count < group_index + 1) combined.group_count = group_index + 1;
            }
            
            MatchResult rest = match_pattern(text, text_len, cur, pattern, pattern_len, next_pos + 1, ci, dotnl);
            if (rest.end_pos >= 0) {                                         // rest matched
                merge_groups(&rest, &combined);
                return rest;
            }
        }
        return init_match_result(-1, 0);                                     // no match
    }
    
    return init_match_result(-1, 0);                                         // unknown quantifier
}

// main pattern matching function
static MatchResult match_pattern(const char* text, int text_len, int text_pos,
                                 const char* pattern, int pattern_len, int pattern_pos,
                                 bool case_insensitive, bool dot_matches_newline) {
    if (pattern_pos >= pattern_len) {
        return init_match_result(text_pos, 0);                               // end of pattern, success
    }
    
    if (text_pos > text_len) {
        return init_match_result(-1, 0);                                     // past end of text
    }
    
    if (text_pos == text_len) {                                              // at end of text
        if (pattern[pattern_pos] == '$') {                                   // end anchor
            return match_pattern(text, text_len, text_pos, pattern, pattern_len, 
                               pattern_pos + 1, case_insensitive, dot_matches_newline);
        }
        if (pattern[pattern_pos] == '(') {                                   // try group at end (empty groups)
            MatchResult r = match_group(text, text_len, text_pos, pattern, pattern_len,
                                       pattern_pos, case_insensitive, dot_matches_newline);
            if (r.end_pos >= 0) return r;
        }
        int alt_pos2 = find_alternation_end(pattern, pattern_len, pattern_pos);  // try alternation at end
        if (alt_pos2 < pattern_len && pattern[alt_pos2] == '|') {
            int scope_end2 = find_scope_end(pattern, pattern_len, pattern_pos);
            MatchResult r = match_alternation(text, text_len, text_pos, pattern, pattern_len,
                                             pattern_pos, scope_end2, case_insensitive, dot_matches_newline);
            if (r.end_pos >= 0) return r;
        }
        return init_match_result(-1, 0);                                     // no more text to match
    }
    
    char pc = pattern[pattern_pos];                                          // current pattern char
    
    int alt_pos = find_alternation_end(pattern, pattern_len, pattern_pos);   // check alternation before everything
    if (alt_pos < pattern_len && pattern[alt_pos] == '|') {
        int scope_end = find_scope_end(pattern, pattern_len, pattern_pos);
        return match_alternation(text, text_len, text_pos, pattern, pattern_len, 
                                pattern_pos, scope_end, case_insensitive, dot_matches_newline);
    }
    
    if (pc == '\\') {                                                        // escape sequence
        return match_escape(text, text_len, text_pos, pattern, pattern_len, 
                          pattern_pos, case_insensitive, dot_matches_newline);
    }
    
    if (pc == '[') {                                                         // character class
        return match_charclass(text, text_len, text_pos, pattern, pattern_len, 
                             pattern_pos, case_insensitive, dot_matches_newline);
    }
    
    if (pc == '^') {                                                         // start anchor
        if (text_pos != 0) return init_match_result(-1, 0);                  // must be at start
        return match_pattern(text, text_len, text_pos, pattern, pattern_len, 
                           pattern_pos + 1, case_insensitive, dot_matches_newline);
    }
    
    if (pc == '$') {                                                         // end anchor
        if (text_pos != text_len) return init_match_result(-1, 0);           // must be at end
        return init_match_result(text_pos, 0);                               // success
    }
    
    if (pc == '(') {                                                         // group
        return match_group(text, text_len, text_pos, pattern, pattern_len, 
                         pattern_pos, case_insensitive, dot_matches_newline);
    }
    
    if (pattern_pos + 1 < pattern_len) {                                     // check for quantifier
        char next = pattern[pattern_pos + 1];
        if (next == '*') return match_star(text, text_len, text_pos, pattern, pattern_len, 
                                          pattern_pos, pc, case_insensitive, dot_matches_newline);
        if (next == '+') return match_plus(text, text_len, text_pos, pattern, pattern_len, 
                                          pattern_pos, pc, case_insensitive, dot_matches_newline);
        if (next == '?') return match_question(text, text_len, text_pos, pattern, pattern_len, 
                                              pattern_pos, pc, case_insensitive, dot_matches_newline);
    }
    
    if (pc == '.') {                                                         // dot character
        if (!dot_matches_newline && text[text_pos] == '\n') return init_match_result(-1, 0);  // newline without dotall
        return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, 
                           pattern_pos + 1, case_insensitive, dot_matches_newline);
    }
    
    if (!match_char(text[text_pos], pc, case_insensitive)) return init_match_result(-1, 0);  // literal char match
    return match_pattern(text, text_len, text_pos + 1, pattern, pattern_len, 
                       pattern_pos + 1, case_insensitive, dot_matches_newline);  // continue
}

// find all matches in text
static Table* find_all_matches(const char* text, const char* pattern, bool ci, bool dotnl, VM* vm) {
    if (!text || !pattern) return NULL;                                      // validate inputs
    
    Table* result = table_create(8);                                         // create result table
    if (!result) return NULL;                                                // allocation failed
    
    int text_len = (int)strlen(text);                                        // text length
    int pattern_len = (int)strlen(pattern);                                  // pattern length
    
    if (text_len == 0 || pattern_len == 0) return result;                    // empty input
    
    int match_count = 0;                                                     // match counter
    
    for (int start = 0; start <= text_len; start++) {                        // iterate over text positions
        MatchResult match = match_pattern(text, text_len, start, pattern, pattern_len, 0, ci, dotnl);
        if (match.end_pos > start) {                                         // found non-empty match
            int match_len = match.end_pos - start;                           // match length
            char* match_str = (char*)malloc(match_len + 1);                  // allocate match string
            if (!match_str) {                                                // allocation failed
                table_destroy(result);
                return NULL;
            }
            memcpy(match_str, text + start, match_len);                      // copy match
            match_str[match_len] = '\0';                                     // null terminate
            
            Value key = MAKE_NUMBER((double)(match_count + 1));              // index key
            Value val = make_string_val(vm, match_str);                      // string value
            table_set(result, key, val);                                     // store in table
            value_decref(key);                                               // release key
            value_decref(val);                                               // release value
            free(match_str);                                                 // free temporary
            
            match_count++;                                                   // increment counter
            start = match.end_pos - 1;                                       // continue after match
        }
    }
    
    return result;                                                           // return result table
}

// substitute pattern matches in text
static char* substitute_pattern(const char* text, const char* pattern, const char* replacement, bool ci, bool dotnl) {
    if (!text || !pattern || !replacement) return NULL;                      // validate inputs
    
    int text_len = (int)strlen(text);                                        // text length
    int pattern_len = (int)strlen(pattern);                                  // pattern length
    int repl_len = (int)strlen(replacement);                                 // replacement length
    
    if (text_len == 0 || pattern_len == 0) {                                 // empty input
        char* result = (char*)malloc(text_len + 1);                          // allocate result
        if (result) strcpy(result, text);                                    // copy text
        return result;
    }
    
    int result_capacity = text_len + 256;                                    // initial capacity
    char* result = (char*)malloc(result_capacity);                           // allocate result buffer
    if (!result) return NULL;                                                // allocation failed
    
    int result_pos = 0;                                                      // current position in result
    int text_pos = 0;                                                        // current position in text
    
    while (text_pos <= text_len) {                                           // process entire text
        MatchResult match = match_pattern(text, text_len, text_pos, pattern, pattern_len, 0, ci, dotnl);
        if (match.end_pos > text_pos) {                                      // found match
            while (result_pos + repl_len + 1 > result_capacity) {            // ensure capacity
                result_capacity *= 2;
                char* new_result = (char*)realloc(result, result_capacity);
                if (!new_result) {                                           // realloc failed
                    free(result);
                    return NULL;
                }
                result = new_result;
            }
            memcpy(result + result_pos, replacement, repl_len);              // copy replacement
            result_pos += repl_len;                                          // advance position
            text_pos = match.end_pos;                                        // skip matched text
        } else {
            if (text_pos < text_len) {                                       // still have text
                if (result_pos + 2 > result_capacity) {                      // ensure capacity
                    result_capacity *= 2;
                    char* new_result = (char*)realloc(result, result_capacity);
                    if (!new_result) {                                       // realloc failed
                        free(result);
                        return NULL;
                    }
                    result = new_result;
                }
                result[result_pos++] = text[text_pos++];                     // copy character
            } else {
                break;                                                       // end of text
            }
        }
    }
    
    result[result_pos] = '\0';                                               // null terminate
    return result;                                                           // return substituted string
}

// split text by pattern
static Table* split_by_pattern(const char* text, const char* pattern, bool ci, bool dotnl, VM* vm) {
    if (!text || !pattern) return NULL;                                      // validate inputs
    
    Table* result = table_create(8);                                         // create result table
    if (!result) return NULL;                                                // allocation failed
    
    int text_len = (int)strlen(text);                                        // text length
    int pattern_len = (int)strlen(pattern);                                  // pattern length
    
    if (text_len == 0 || pattern_len == 0) {                                 // empty input
        if (text_len > 0) {                                                  // text but no pattern
            Value key = MAKE_NUMBER(1.0);                                    // key for first element
            Value val = make_string_val(vm, text);                           // string value
            table_set(result, key, val);                                     // store in table
            value_decref(key);                                               // release key
            value_decref(val);                                               // release value
        }
        return result;                                                       // return table
    }
    
    int split_count = 0;                                                     // split counter
    int segment_start = 0;                                                   // start of current segment
    
    for (int pos = 0; pos <= text_len; pos++) {                              // iterate over text
        MatchResult match = match_pattern(text, text_len, pos, pattern, pattern_len, 0, ci, dotnl);
        if (match.end_pos > pos) {                                           // found delimiter
            int seg_len = pos - segment_start;                               // segment length
            
            if (seg_len == 0) {                                              // skip empty segments
                // skip this empty segment
            } else {
                char* seg_str = (char*)malloc(seg_len > 0 ? seg_len + 1 : 1);  // allocate segment
                if (!seg_str) {                                              // allocation failed
                    table_destroy(result);
                    return NULL;
                }
                if (seg_len > 0) {
                    memcpy(seg_str, text + segment_start, seg_len);          // copy segment
                    seg_str[seg_len] = '\0';                                 // null terminate
                } else {
                    seg_str[0] = '\0';                                       // empty string
                }
                
                Value key = MAKE_NUMBER((double)(split_count + 1));          // index key
                Value val = make_string_val(vm, seg_str);                    // string value
                table_set(result, key, val);                                 // store in table
                value_decref(key);                                           // release key
                value_decref(val);                                           // release value
                free(seg_str);                                               // free temporary
                split_count++;                                               // increment counter
            }
            
            pos = match.end_pos - 1;                                         // skip delimiter
            segment_start = match.end_pos;                                   // start new segment
        }
    }
    
    if (segment_start <= text_len) {                                         // handle last segment
        int seg_len = text_len - segment_start;                              // remaining length
        
        if (seg_len == 0 && split_count > 0) {
            return result;                                                   // don't add empty trailing segment
        }
        
        char* seg_str = (char*)malloc(seg_len > 0 ? seg_len + 1 : 1);        // allocate segment
        if (!seg_str) {                                                      // allocation failed
            table_destroy(result);
            return NULL;
        }
        if (seg_len > 0) {
            memcpy(seg_str, text + segment_start, seg_len);                  // copy segment
            seg_str[seg_len] = '\0';                                         // null terminate
        } else {
            seg_str[0] = '\0';                                               // empty string
        }
        
        Value key = MAKE_NUMBER((double)(split_count + 1));                  // index key
        Value val = make_string_val(vm, seg_str);                            // string value
        table_set(result, key, val);                                         // store in table
        value_decref(key);                                                   // release key
        value_decref(val);                                                   // release value
        free(seg_str);                                                       // free temporary
    }
    
    return result;                                                           // return result table
}

// get options from table
static bool get_opts(VM* vm, Value opts_val, bool* ci, bool* dotnl) {
    *ci = false;                                                             // default case sensitive
    *dotnl = false;                                                          // default dot doesn't match newline
    
    if (!IS_TABLE(opts_val)) {
        return true;                                                         // no options provided
    }
    
    Table* opts = AS_TABLE(opts_val);                                        // extract table
    if (!opts) return true;                                                  // invalid table
    
    Value val;                                                               // temporary value
    
    Value key_ic = MAKE_STRING(string_intern(&vm->intern_table, "ignorecase", 10));  // ignorecase key
    if (table_get(opts, key_ic, &val)) {                                     // key exists
        if (IS_BOOL(val)) *ci = AS_BOOL(val);                                // set if boolean
        value_decref(val);                                                   // release value
    }
    value_decref(key_ic);                                                    // release key
    
    Value key_da = MAKE_STRING(string_intern(&vm->intern_table, "dotall", 6));  // dotall key
    if (table_get(opts, key_da, &val)) {                                     // key exists
        if (IS_BOOL(val)) *dotnl = AS_BOOL(val);                             // set if boolean
        value_decref(val);                                                   // release value
    }
    value_decref(key_da);                                                    // release key
    
    return true;                                                             // options processed
}

// dispatcher for regex built-in functions
bool regex_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "regex.find_all") == 0) {                               // find all pattern matches
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
            *result = MAKE_NONE();
            return true;
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                    // extract pattern
        const char* text = AS_STRING(args[1])->chars;                       // extract text
        
        if (!pattern || !text) {                                            // validate pointers
            *result = MAKE_NONE();
            return true;
        }
        
        bool ci = false, dotnl = false;                                     // default options
        if (arg_count >= 3 && IS_TABLE(args[2])) {                          // options provided
            if (!get_opts(vm, args[2], &ci, &dotnl)) {                      // process options
                *result = MAKE_NONE();
                return true;
            }
        }
        
        Table* matches = find_all_matches(text, pattern, ci, dotnl, vm);    // find matches
        if (!matches) {                                                     // error occurred
            *result = MAKE_NONE();
            return true;
        }
        
        *result = MAKE_TABLE(matches);                                      // return result table
        return true;
    }
    
    if (strcmp(name, "regex.replace") == 0) {                               // substitute pattern matches
        if (arg_count < 3 || !IS_STRING(args[0]) || !IS_STRING(args[1]) || !IS_STRING(args[2])) {  // validate arguments
            *result = MAKE_NONE();
            return true;
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                    // extract pattern
        const char* replacement = AS_STRING(args[1])->chars;                // extract replacement
        const char* text = AS_STRING(args[2])->chars;                       // extract text
        
        if (!pattern || !replacement || !text) {                            // validate pointers
            *result = MAKE_NONE();
            return true;
        }
        
        bool ci = false, dotnl = false;                                     // default options
        if (arg_count >= 4 && IS_TABLE(args[3])) {                          // options provided
            if (!get_opts(vm, args[3], &ci, &dotnl)) {                      // process options
                *result = MAKE_NONE();
                return true;
            }
        }
        
        char* substituted = substitute_pattern(text, pattern, replacement, ci, dotnl);  // perform substitution
        if (!substituted) {                                                 // error occurred
            *result = MAKE_NONE();
            return true;
        }
        
        *result = make_string_val(vm, substituted);                         // return result string
        free(substituted);                                                  // free temporary
        return true;
    }
    
    if (strcmp(name, "regex.split") == 0) {                                 // split by pattern
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
            *result = MAKE_NONE();
            return true;
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                    // extract pattern
        const char* text = AS_STRING(args[1])->chars;                       // extract text
        
        if (!pattern || !text) {                                            // validate pointers
            *result = MAKE_NONE();
            return true;
        }
        
        bool ci = false, dotnl = false;                                     // default options
        if (arg_count >= 3 && IS_TABLE(args[2])) {                          // options provided
            if (!get_opts(vm, args[2], &ci, &dotnl)) {                      // process options
                *result = MAKE_NONE();
                return true;
            }
        }
        
        Table* splits = split_by_pattern(text, pattern, ci, dotnl, vm);     // split text
        if (!splits) {                                                      // error occurred
            *result = MAKE_NONE();
            return true;
        }
        
        *result = MAKE_TABLE(splits);                                       // return result table
        return true;
    }
    
    if (strcmp(name, "regex.search") == 0) {                                // search for first match
        if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
            *result = MAKE_NONE();
            return true;
        }
        
        const char* pattern = AS_STRING(args[0])->chars;                    // extract pattern
        const char* text = AS_STRING(args[1])->chars;                       // extract text
        
        if (!pattern || !text) {                                            // validate pointers
            *result = MAKE_NONE();
            return true;
        }
        
        bool ci = false, dotnl = false;                                     // default options
        if (arg_count >= 3 && IS_TABLE(args[2])) {                          // options provided
            if (!get_opts(vm, args[2], &ci, &dotnl)) {                      // process options
                *result = MAKE_NONE();
                return true;
            }
        }
        
        int text_len = (int)strlen(text);                                   // text length
        int pattern_len = (int)strlen(pattern);                             // pattern length
        
        Table* search_result = table_create(4);                             // create result table
        if (!search_result) {                                               // allocation failed
            *result = MAKE_NONE();
            return true;
        }
        
        if (text_len == 0 || pattern_len == 0) {                            // empty input
            *result = MAKE_TABLE(search_result);                            // return empty table
            return true;
        }
        
        for (int start = 0; start <= text_len; start++) {                   // search all positions
            MatchResult match = match_pattern(text, text_len, start, pattern, pattern_len, 0, ci, dotnl);
            if (match.end_pos >= start && pattern_len > 0) {                // found match (including empty)
                int match_len = match.end_pos - start;                      // match length
                char* match_str = (char*)malloc(match_len + 1);             // allocate match string
                if (!match_str) {                                           // allocation failed
                    table_destroy(search_result);
                    *result = MAKE_NONE();
                    return true;
                }
                memcpy(match_str, text + start, match_len);                 // copy match
                match_str[match_len] = '\0';                                // null terminate
                
                Value ks = MAKE_STRING(string_intern(&vm->intern_table, "start", 5));  // start key
                Value vs = MAKE_NUMBER((double)(start + 1));                // start value (1-based)
                table_set(search_result, ks, vs);                           // store start
                value_decref(ks);                                           // release key
                value_decref(vs);                                           // release value
                
                Value ke = MAKE_STRING(string_intern(&vm->intern_table, "end", 3));  // end key
                Value ve = MAKE_NUMBER((double)match.end_pos);              // end value
                table_set(search_result, ke, ve);                           // store end
                value_decref(ke);                                           // release key
                value_decref(ve);                                           // release value
                
                Value km = MAKE_STRING(string_intern(&vm->intern_table, "match", 5));  // match key
                Value vm_val = make_string_val(vm, match_str);              // match value
                table_set(search_result, km, vm_val);                       // store match
                value_decref(km);                                           // release key
                value_decref(vm_val);                                       // release value
                
                free(match_str);                                            // free temporary
                
                if (match.group_count > 0) {                                // store capture groups if any
                    Table* groups_table = table_create(match.group_count);  // create groups table
                    if (groups_table) {
                        for (int g = 0; g < match.group_count && g < MAX_GROUPS; g++) {
                            if (match.groups[g].matched) {                  // group matched
                                int g_start = match.groups[g].start;        // group start
                                int g_end = match.groups[g].end;            // group end
                                int g_len = g_end - g_start;                // group length
                                
                                char* g_str = (char*)malloc(g_len + 1);     // allocate group string
                                if (g_str) {
                                    memcpy(g_str, text + g_start, g_len);   // copy group
                                    g_str[g_len] = '\0';                    // null terminate
                                    
                                    Value gk = MAKE_NUMBER((double)(g + 1));  // group key
                                    Value gv = make_string_val(vm, g_str);  // group value
                                    table_set(groups_table, gk, gv);        // store group
                                    value_decref(gk);                       // release key
                                    value_decref(gv);                       // release value
                                    free(g_str);                            // free temporary
                                }
                            }
                        }
                        
                        Value kg = MAKE_STRING(string_intern(&vm->intern_table, "groups", 6));  // groups key
                        Value vg = MAKE_TABLE(groups_table);                // groups value
                        table_set(search_result, kg, vg);                   // store groups
                        value_decref(kg);                                   // release key
                        value_decref(vg);                                   // release value
                    }
                }
                
                break;                                                      // stop after first match
            }
        }
        
        *result = MAKE_TABLE(search_result);                                // return result table
        return true;
    }
    
    return false;                                                           // not a recognized builtin
}
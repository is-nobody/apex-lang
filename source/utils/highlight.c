#include "highlight.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

// ANSI colors for syntax highlighting
#define ANSI_RESET  "\033[0m"
#define ANSI_BLUE   "\033[34m"  // Operators
#define ANSI_ORANGE "\033[33m"  // Keywords
#define ANSI_GREEN  "\033[32m"  // Strings
#define ANSI_CYAN   "\033[36m"  // Numbers
#define ANSI_GRAY   "\033[90m"  // Comments

static const char* keywords[] = {
    "function", "if", "elif", "else", "while", "for", "break", "continue",
    "return", "import", "and", "or", "not", "true", "false", NULL
};

static bool is_keyword(const char* word, int len) {
    for (int i = 0; keywords[i]; i++) {
        if ((int)strlen(keywords[i]) == len && memcmp(keywords[i], word, len) == 0)
            return true;
    }
    return false;
}

char* highlight_line(const char* line) {
    if (!line || !*line) return strdup("");
    
    // Allocate buffer with extra space for ANSI codes (roughly x10)
    size_t line_len = strlen(line);
    char* out = (char*)malloc(line_len * 12 + 64);
    if (!out) return strdup(line);
    
    int pos = 0, i = 0;
    int len = (int)line_len;

    while (i < len) {
        // Comments: //
        if (i + 1 < len && line[i] == '/' && line[i+1] == '/') {
            pos += sprintf(out + pos, "%s", ANSI_GRAY);
            while (i < len && line[i] != '\n') out[pos++] = line[i++];
            pos += sprintf(out + pos, "%s", ANSI_RESET);
        }
        // Strings: "..."
        else if (line[i] == '"') {
            pos += sprintf(out + pos, "%s", ANSI_GREEN);
            out[pos++] = line[i++];
            while (i < len && line[i] != '"' && line[i] != '\n') {
                if (line[i] == '\\' && i + 1 < len) out[pos++] = line[i++];
                out[pos++] = line[i++];
            }
            if (i < len && line[i] == '"') out[pos++] = line[i++];
            pos += sprintf(out + pos, "%s", ANSI_RESET);
        }
        // Numbers
        else if (isdigit((unsigned char)line[i]) || 
                (line[i] == '.' && i + 1 < len && isdigit((unsigned char)line[i+1]))) {
            pos += sprintf(out + pos, "%s", ANSI_CYAN);
            while (i < len && (isdigit((unsigned char)line[i]) || line[i] == '.')) 
                out[pos++] = line[i++];
            pos += sprintf(out + pos, "%s", ANSI_RESET);
        }
        // Identifiers / Keywords
        else if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            int start = i;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            if (is_keyword(line + start, i - start)) {
                pos += sprintf(out + pos, "%s", ANSI_ORANGE);
            }
            memcpy(out + pos, line + start, i - start);
            pos += i - start;
            if (is_keyword(line + start, i - start)) 
                pos += sprintf(out + pos, "%s", ANSI_RESET);
        }
        // Operators
        else if (strchr("+-*/%=<>!&|(),.", line[i])) {
            int op_len = 1;
            if (i + 1 < len) {
                if ((line[i]=='=' && line[i+1]=='=') || (line[i]=='!' && line[i+1]=='=') ||
                    (line[i]=='<' && line[i+1]=='=') || (line[i]=='>' && line[i+1]=='='))
                    op_len = 2;
            }
            pos += sprintf(out + pos, "%s", ANSI_BLUE);
            memcpy(out + pos, line + i, op_len);
            pos += op_len;
            pos += sprintf(out + pos, "%s", ANSI_RESET);
            i += op_len;
        }
        // Other characters
        else {
            out[pos++] = line[i++];
        }
    }
    out[pos] = '\0';
    return out;
}
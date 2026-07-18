#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
// enables ansi color codes on windows terminals
static void ensure_vt_support() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#endif

#define ANSI_RED    "\033[31m"
#define ANSI_RESET  "\033[0m"

// returns the number of bytes in a utf-8 character by its first byte
static int utf8_char_bytes(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// prints a formatted error with source context and highlighted underline
void print_error_with_context(const char* filename, const char* source, 
                              int line, int col, int len, 
                              const char* type, const char* message) {
    #ifdef _WIN32
    ensure_vt_support();
    #endif

    // find the line in source
    int cur_line = 1;
    const char* line_start = source;
    const char* p = source;
    while (*p) {
        if (cur_line == line) { line_start = p; break; }
        if (*p == '\n') cur_line++;
        p++;
    }
    
    // find end of line
    const char* line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
    int line_byte_len = (int)(line_end - line_start);

    // copy the line
    char* line_buf = (char*)malloc(line_byte_len + 1);
    if (!line_buf) return;
    memcpy(line_buf, line_start, line_byte_len);
    line_buf[line_byte_len] = '\0';

    fprintf(stderr, "%s in %s on line %d:\n", type, filename, line);

    // col and len are in characters, convert to byte offsets
    int char_count = 0;
    int byte_pos = 0;
    int err_start_byte = 0;
    int err_end_byte = line_byte_len;
    
    // find byte offset for error start (col - 1 characters from line start)
    while (byte_pos < line_byte_len && char_count < col - 1) {
        int char_bytes = utf8_char_bytes((unsigned char)line_buf[byte_pos]);
        byte_pos += char_bytes;
        char_count++;
    }
    err_start_byte = byte_pos;
    
    // find byte offset for error end (len characters from error start)
    int target_chars = char_count + len;
    while (byte_pos < line_byte_len && char_count < target_chars) {
        int char_bytes = utf8_char_bytes((unsigned char)line_buf[byte_pos]);
        byte_pos += char_bytes;
        char_count++;
    }
    err_end_byte = byte_pos;

    if (err_start_byte < 0) err_start_byte = 0;
    if (err_end_byte > line_byte_len) err_end_byte = line_byte_len;
    if (err_start_byte > err_end_byte) err_start_byte = err_end_byte;

    // print the line with highlighted error region
    fprintf(stderr, "    ");
    fwrite(line_buf, 1, err_start_byte, stderr);
    fprintf(stderr, "%s", ANSI_RED);
    fwrite(line_buf + err_start_byte, 1, err_end_byte - err_start_byte, stderr);
    fprintf(stderr, "%s", ANSI_RESET);
    fwrite(line_buf + err_end_byte, 1, line_byte_len - err_end_byte, stderr);
    fprintf(stderr, "\n");

    // print underline aligned by characters (one space per character, not byte)
    fprintf(stderr, "    ");
    byte_pos = 0;
    for (int i = 0; i < col - 1; i++) {
        if (byte_pos < line_byte_len) {
            int char_bytes = utf8_char_bytes((unsigned char)line_buf[byte_pos]);
            fprintf(stderr, " ");
            byte_pos += char_bytes;
        }
    }
    
    fprintf(stderr, "%s^", ANSI_RED);
    int underline = len;
    if (underline < 1) underline = 1;
    for (int i = 1; i < underline; i++) {
        fprintf(stderr, "~");
    }
    fprintf(stderr, "%s\n", ANSI_RESET);

    fprintf(stderr, "%s%s%s\n", ANSI_RED, message, ANSI_RESET);

    free(line_buf);
}
// source/utils/error.c
// Implementation of Errors for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
// enables ansi color codes on windows terminals
static void ensure_vt_support() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);  // get stdout handle
    DWORD dwMode = 0;                               // mode storage
    GetConsoleMode(hOut, &dwMode);                  // get current mode
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;   // enable vt processing
    SetConsoleMode(hOut, dwMode);                   // apply new mode
}
#endif

#define ANSI_RED    "\033[31m"  // red color for errors
#define ANSI_RESET  "\033[0m"   // reset terminal color

// returns the number of bytes in a utf-8 character by its first byte
static int utf8_char_bytes(unsigned char c) {
    if (c < 0x80) return 1;            // ascii character
    if ((c & 0xE0) == 0xC0) return 2;  // 2-byte utf-8 sequence
    if ((c & 0xF0) == 0xE0) return 3;  // 3-byte utf-8 sequence
    if ((c & 0xF8) == 0xF0) return 4;  // 4-byte utf-8 sequence
    return 1;                          // fallback
}

// prints a formatted error with source context and highlighted underline
void print_error_with_context(const char* filename, const char* source, 
                              int line, int col, int len, 
                              const char* type, const char* message) {
    #ifdef _WIN32
    ensure_vt_support();                                  // enable colors on windows
    #endif

    // find the line in source
    int cur_line = 1;                                     // current line counter
    const char* line_start = source;                      // start of line pointer
    const char* p = source;                               // scan pointer
    while (*p) {                                          // iterate through source
        if (cur_line == line) { line_start = p; break; }  // found target line
        if (*p == '\n') cur_line++;                       // increment line on newline
        p++;                                              // advance pointer
    }
    
    // find end of line
    const char* line_end = line_start;                                        // end of line pointer
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;   // find line terminator
    int line_byte_len = (int)(line_end - line_start);                         // line length in bytes

    // copy the line
    char* line_buf = (char*)malloc(line_byte_len + 1);                        // allocate line buffer
    if (!line_buf) return;                                                    // allocation failed
    memcpy(line_buf, line_start, line_byte_len);                              // copy line content
    line_buf[line_byte_len] = '\0';                                           // null terminate

    // add artificial spaces if error position extends past end of line
    if (col + len > (int)strlen(line_buf)) {
        int extra = 10;                                                       // extra spaces to append
        line_buf = (char*)realloc(line_buf, line_byte_len + extra + 1);       // resize buffer
        if (line_buf) {
            memset(line_buf + line_byte_len, ' ', extra);                     // fill with spaces
            line_buf[line_byte_len + extra] = '\0';                           // null terminate
            line_byte_len += extra;                                           // update line length
        }
    }

    fprintf(stderr, "%s in %s on line %d:\n", type, filename, line);          // print error header

    // col and len are in characters, convert to byte offsets
    int char_count = 0;                                                       // character counter
    int byte_pos = 0;                                                         // byte position
    int err_start_byte = 0;                                                   // error start byte offset
    int err_end_byte = line_byte_len;                                         // error end byte offset
    
    // find byte offset for error start (col - 1 characters from line start)
    while (byte_pos < line_byte_len && char_count < col - 1) {                // iterate to start position
        int char_bytes = utf8_char_bytes((unsigned char)line_buf[byte_pos]);  // get char byte length
        byte_pos += char_bytes;                                               // advance byte position
        char_count++;                                                         // increment character count
    }
    err_start_byte = byte_pos;                                                // store start byte offset
    
    // find byte offset for error end (len characters from error start)
    int target_chars = char_count + len;                                      // target character position
    while (byte_pos < line_byte_len && char_count < target_chars) {           // iterate to end position
        int char_bytes = utf8_char_bytes((unsigned char)line_buf[byte_pos]);  // get char byte length
        byte_pos += char_bytes;                                               // advance byte position
        char_count++;                                                         // increment character count
    }
    err_end_byte = byte_pos;                                                  // store end byte offset

    if (err_start_byte < 0) err_start_byte = 0;                               // clamp start
    if (err_end_byte > line_byte_len) err_end_byte = line_byte_len;           // clamp end
    if (err_start_byte > err_end_byte) err_start_byte = err_end_byte;         // ensure valid range

    // print the line with highlighted error region
    fprintf(stderr, "    ");                                                      // indent
    fwrite(line_buf, 1, err_start_byte, stderr);                                  // print before error
    fprintf(stderr, "%s", ANSI_RED);                                              // start red color
    fwrite(line_buf + err_start_byte, 1, err_end_byte - err_start_byte, stderr);  // print error region
    fprintf(stderr, "%s", ANSI_RESET);                                            // reset color
    fwrite(line_buf + err_end_byte, 1, line_byte_len - err_end_byte, stderr);     // print after error
    fprintf(stderr, "\n");                                                        // newline

    // print underline aligned by characters (one space per character, not byte)
    fprintf(stderr, "    ");                                                      // indent
    byte_pos = 0;                                                                 // reset byte position
    for (int i = 0; i < col - 1; i++) {                                           // skip to error position
        if (byte_pos < line_byte_len) {                                           // check bounds
            int char_bytes = utf8_char_bytes((unsigned char)line_buf[byte_pos]);  // get char byte length
            fprintf(stderr, " ");                                                 // print space per character
            byte_pos += char_bytes;                                               // advance byte position
        }
    }
    
    fprintf(stderr, "%s^", ANSI_RED);                            // print caret at error start
    int underline = len;                                         // underline length
    if (underline < 1) underline = 1;                            // minimum length 1
    for (int i = 1; i < underline; i++) {                        // print tilde for rest
        fprintf(stderr, "~");
    }
    fprintf(stderr, "%s\n", ANSI_RESET);                         // reset color and newline

    fprintf(stderr, "%s%s%s\n", ANSI_RED, message, ANSI_RESET);  // print error message

    free(line_buf);                                              // free line buffer
}
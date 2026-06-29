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

// prints a formatted error with source context and highlighted underline
void print_error_with_context(const char* filename, const char* source, 
                              int line, int col, int len, 
                              const char* type, const char* message) {
    #ifdef _WIN32
    ensure_vt_support();
    #endif

    int cur_line = 1;
    const char* line_start = source;
    const char* p = source;
    while (*p) {
        if (cur_line == line) { line_start = p; break; }
        if (*p == '\n') cur_line++;
        p++;
    }
    const char* line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
    int line_len = line_end - line_start;

    char* line_buf = (char*)malloc(line_len + 1);
    if (!line_buf) return;
    memcpy(line_buf, line_start, line_len);
    line_buf[line_len] = '\0';

    fprintf(stderr, "%s in %s on line %d:\n", type, filename, line);

    int err_start = col - 1;
    int err_end   = (len > 0) ? err_start + len : err_start + 1;
    if (err_start < 0) err_start = 0;
    if (err_end > line_len) err_end = line_len;

    fprintf(stderr, "    ");
    fwrite(line_buf, 1, err_start, stderr);
    fprintf(stderr, "%s", ANSI_RED);
    fwrite(line_buf + err_start, 1, err_end - err_start, stderr);
    fprintf(stderr, "%s", ANSI_RESET);
    fwrite(line_buf + err_end, 1, line_len - err_end, stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, "    ");
    for (int i = 0; i < err_start; i++) fprintf(stderr, " ");
    fprintf(stderr, "%s^", ANSI_RED);
    int underline = err_end - err_start;
    if (underline < 1) underline = 1;
    for (int i = 1; i < underline; i++) fprintf(stderr, "~");
    fprintf(stderr, "%s\n", ANSI_RESET);

    fprintf(stderr, "%s%s%s\n", ANSI_RED, message, ANSI_RESET);

    free(line_buf);
}
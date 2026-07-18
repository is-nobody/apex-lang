#include "repl.h"
#include "platform.h"
#include "execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define MAX_LINE 4096
#define MAX_INPUT 65536

static volatile sig_atomic_t g_should_exit = 0;

static __attribute__((unused)) void signal_handler(int sig) {
    (void)sig;
    g_should_exit = 1;
}

static void setup_signals(void) {
#ifndef _WIN32
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
#endif
}

// returns the number of bytes in a utf-8 character
static int utf8_char_bytes(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// counts characters in a byte string
static int count_chars(const char* s, int byte_len) {
    int chars = 0;
    int i = 0;
    while (i < byte_len) {
        i += utf8_char_bytes((unsigned char)s[i]);
        chars++;
    }
    return chars;
}

// redraws the current input line - cursor_pos is in CHARACTERS
static void redraw_line(const char* line, int cursor_pos) {
    int line_chars = count_chars(line, strlen(line));
    
    printf("\r\033[K> %s", line);
    
    int chars_back = line_chars - cursor_pos;
    
    if (chars_back > 0) {
        printf("\033[%dD", chars_back);
    } else if (chars_back < 0) {
        printf("\033[%dC", -chars_back);
    }
    
    fflush(stdout);
}

static void execute_code(const char* code, const char* display_name) {
    if (!code || strlen(code) == 0) return;
    
    char* temp_path = platform_create_temp_file(code, strlen(code));
    if (!temp_path) {
        print_error("Cannot create temporary file");
        return;
    }
    
    execute_source(temp_path, display_name);
    
    platform_delete_temp_file(temp_path);
    free(temp_path);
}

void repl_run(void) {
    setup_signals();
    printf("Apex 26.07 on %s. Type code, always ready.\n", platform_get_name());
    
    terminal_enable_raw_mode();
    
    char full_input[MAX_INPUT];
    char line[MAX_LINE];
    int total_len = 0;
    int char_pos = 0;
    int byte_pos = 0;
    full_input[0] = '\0';
    line[0] = '\0';
    
    printf("> ");
    fflush(stdout);
    
    while (!g_should_exit) {
        unsigned char c;
        ssize_t n = terminal_read_blocking((char*)&c);
        
        if (n <= 0) {
            if (n == 0) break;
#ifndef _WIN32
            if (errno == EINTR) continue;
#endif
            break;
        }
        
        // Ctrl+D or Ctrl+C
        if (c == 4 || c == 3) {
            break;
        }
        
        // Enter
        if (c == '\r' || c == '\n') {
            printf("\r\n");
            line[byte_pos] = '\0';
            if (byte_pos > 0) {
                if (total_len + byte_pos + 2 < MAX_INPUT) {
                    memcpy(full_input + total_len, line, byte_pos);
                    total_len += byte_pos;
                    full_input[total_len++] = '\n';
                    full_input[total_len] = '\0';
                }
            }
            byte_pos = 0;
            char_pos = 0;
            line[0] = '\0';
            
            if (!terminal_has_input()) {
                if (total_len > 0) {
                    execute_code(full_input, "REPL");
                }
                total_len = 0;
                full_input[0] = '\0';
            }
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        // Backspace
        if (c == 127 || c == '\b') {
            if (byte_pos > 0) {
                int start = byte_pos - 1;
                while (start > 0 && ((unsigned char)line[start] & 0xC0) == 0x80) {
                    start--;
                }
                memmove(&line[start], &line[byte_pos], MAX_LINE - byte_pos);
                byte_pos = start;
                char_pos = count_chars(line, byte_pos);
                line[byte_pos] = '\0';
                redraw_line(line, char_pos);
            }
            continue;
        }
        
        // printable characters
        if (c >= 32 && byte_pos < MAX_LINE - 4) {
            memmove(&line[byte_pos + 1], &line[byte_pos], MAX_LINE - byte_pos - 1);
            line[byte_pos++] = c;
            line[byte_pos] = '\0';
            char_pos = count_chars(line, byte_pos);
            redraw_line(line, char_pos);
            continue;
        }
    }
    
    terminal_disable_raw_mode();
    printf("\r\n");
}
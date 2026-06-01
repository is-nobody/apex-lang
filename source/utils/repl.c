#include "repl.h"
#include "platform.h"
#include "highlight.h"
#include "execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h> 

#define MAX_LINE 4096
#define MAX_INPUT 65536

static volatile sig_atomic_t g_should_exit = 0;

static void signal_handler(int sig) {
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

static void redraw_line(const char* line, int cursor_pos) {
    char* highlighted = highlight_line(line);
    // \r = move to beginning of line, \033[K = clear to end of line
    printf("\r\033[K> %s", highlighted);
    free(highlighted);
    // Cursor: 3 = length of "> " + 1, cursor_pos = visible characters before cursor
    printf("\033[%dG", 3 + cursor_pos);
    fflush(stdout);
}

static void execute_code(const char* code, const char* display_name) {
    if (!code || strlen(code) == 0) return;
    
    char* temp_path = platform_create_temp_file(code, strlen(code));
    if (!temp_path) {
        fprintf(stderr, "Error: Cannot create temporary file\n");
        return;
    }
    
    execute_source(temp_path, display_name);
    
    platform_delete_temp_file(temp_path);
    free(temp_path);
}

void repl_run(void) {
    setup_signals();
    
    printf("Apex v26.06 on %s. Type code, always ready.\n", platform_get_name());
    
    set_repl_mode(1);
    terminal_enable_raw_mode();
    
    char full_input[MAX_INPUT];
    char line[MAX_LINE];
    int total_len = 0;
    int pos = 0;
    
    full_input[0] = '\0';
    
    printf("> ");
    fflush(stdout);
    
    while (!g_should_exit) {
        char c;
        ssize_t n = terminal_read_blocking(&c);
        
        if (n <= 0) {
#ifndef _WIN32
            if (errno == EINTR) continue;
#endif
            break;
        }
        
        if (c == 4 || c == 3) break;
        
        // Enter
        if (c == '\r' || c == '\n') {
            printf("\n");
            line[pos] = '\0';
            
            if (pos > 0) {
                if (total_len + pos + 2 < MAX_INPUT) {
                    memcpy(full_input + total_len, line, pos);
                    total_len += pos;
                    full_input[total_len++] = '\n';
                    full_input[total_len] = '\0';
                }
            }
            pos = 0;
            
            if (!terminal_has_input()) {
                if (total_len > 0) {
                    execute_code(full_input, "REPL");
                }
                total_len = 0;
                full_input[0] = '\0';
                printf("> ");
                fflush(stdout);
            }
        }
        else if (c == 127 || c == '\b') {
            if (pos > 0) {
                memmove(&line[pos-1], &line[pos], strlen(&line[pos]) + 1);
                pos--;
                redraw_line(line, pos);
            }
        }
        else if (c >= 32 && c < 127 && pos < MAX_LINE - 1) {
            line[pos++] = c;
            line[pos] = '\0';
            redraw_line(line, pos);
        }
    }
    
    terminal_disable_raw_mode();
    set_repl_mode(0);
}
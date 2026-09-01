// source/utils/repl.c
// Implementation of REPL for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "repl.h"
#include "platform.h"
#include "execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define MAX_LINE 4096    // maximum line length
#define MAX_INPUT 65536  // maximum total input

static volatile sig_atomic_t g_should_exit = 0;  // exit flag for signal handling

static __attribute__((unused)) void signal_handler(int sig) {
    (void)sig;                                  // unused parameter
    g_should_exit = 1;                          // set exit flag
}

static void setup_signals(void) {
#ifndef _WIN32
    struct sigaction sa;                        // signal action struct
    memset(&sa, 0, sizeof(sa));                 // zero initialize
    sa.sa_handler = signal_handler;             // set handler
    sigemptyset(&sa.sa_mask);                   // clear signal mask
    sa.sa_flags = 0;                            // no flags
    sigaction(SIGINT, &sa, NULL);               // handle ctrl+c
    sigaction(SIGTSTP, &sa, NULL);              // handle ctrl+z
#endif
}

// returns the number of bytes in a utf-8 character
static int utf8_char_bytes(unsigned char c) {
    if (c < 0x80) return 1;                     // ascii character
    if ((c & 0xE0) == 0xC0) return 2;           // 2-byte utf-8 sequence
    if ((c & 0xF0) == 0xE0) return 3;           // 3-byte utf-8 sequence
    if ((c & 0xF8) == 0xF0) return 4;           // 4-byte utf-8 sequence
    return 1;                                   // fallback
}

// counts characters in a byte string
static int count_chars(const char* s, int byte_len) {
    int chars = 0;                                  // character counter
    int i = 0;                                      // byte index
    while (i < byte_len) {                          // iterate over bytes
        i += utf8_char_bytes((unsigned char)s[i]);  // skip character bytes
        chars++;                                    // increment character count
    }
    return chars;                                   // return character count
}

// redraws the current input line - cursor_pos is in CHARACTERS
static void redraw_line(const char* line, int cursor_pos) {
    int line_chars = count_chars(line, strlen(line));   // total characters in line
    
    printf("\r\033[K> %s", line);                       // clear line and print prompt
    
    int chars_back = line_chars - cursor_pos;           // characters to move back
    
    if (chars_back > 0) {                               // need to move cursor left
        printf("\033[%dD", chars_back);                 // move cursor back
    } else if (chars_back < 0) {                        // need to move cursor right
        printf("\033[%dC", -chars_back);                // move cursor forward
    }
    
    fflush(stdout);                                     // flush output
}

static void execute_code(const char* code, const char* display_name) {
    if (!code || strlen(code) == 0) return;                   // empty code, skip
    
    char* temp_path = platform_create_temp_file(code, strlen(code));  // create temp file
    if (!temp_path) {                                         // check creation
        print_error("Cannot create temporary file");          // print error
        return;                                               // return
    }
    
    execute_source(temp_path, display_name, 0, NULL, false);  // repl: no args
    
    platform_delete_temp_file(temp_path);                     // delete temp file
    free(temp_path);                                          // free path string
}

void repl_run(void) {
    setup_signals();                                    // setup signal handlers
    printf("Apex 26.09 on %s. Type code, always ready.\n", platform_get_name());  // print banner
    
    terminal_enable_raw_mode();                         // enable raw terminal mode
    
    char full_input[MAX_INPUT];                         // accumulated input buffer
    char line[MAX_LINE];                                // current line buffer
    int total_len = 0;                                  // total input length
    int char_pos = 0;                                   // cursor character position
    int byte_pos = 0;                                   // cursor byte position
    full_input[0] = '\0';                               // initialize full input
    line[0] = '\0';                                     // initialize line
    
    printf("> ");                                       // print prompt
    fflush(stdout);                                     // flush output
    
    while (!g_should_exit) {                            // main input loop
        unsigned char c;                                // character buffer
        ssize_t n = terminal_read_blocking((char*)&c);  // read character
        
        if (n <= 0) {                                   // read error or eof
            if (n == 0) break;                          // eof
#ifndef _WIN32
            if (errno == EINTR) continue;               // interrupted, retry
#endif
            break;                                      // break on error
        }
        
        // Ctrl+D or Ctrl+C
        if (c == 4 || c == 3) {                                      // eof or interrupt
            break;                                                   // exit repl
        }
        
        // Enter
        if (c == '\r' || c == '\n') {                                // newline
            printf("\r\n");                                          // print newline
            line[byte_pos] = '\0';                                   // null terminate line
            if (byte_pos > 0) {                                      // non-empty line
                if (total_len + byte_pos + 2 < MAX_INPUT) {          // check buffer space
                    memcpy(full_input + total_len, line, byte_pos);  // copy line to input
                    total_len += byte_pos;                           // update total length
                    full_input[total_len++] = '\n';                  // add newline
                    full_input[total_len] = '\0';                    // null terminate
                }
            }
            byte_pos = 0;                                            // reset byte position
            char_pos = 0;                                            // reset char position
            line[0] = '\0';                                          // reset line
            
            if (!terminal_has_input()) {                             // no more input waiting
                if (total_len > 0) {                                 // has accumulated input
                    execute_code(full_input, "REPL");                // execute code
                }
                total_len = 0;                                       // reset total length
                full_input[0] = '\0';                                // reset input
            }
            printf("> ");                                            // print prompt
            fflush(stdout);                                          // flush output
            continue;                                                // continue loop
        }
        
        // Backspace
        if (c == 127 || c == '\b') {                                                // backspace
            if (byte_pos > 0) {                                                     // something to delete
                int start = byte_pos - 1;                                           // start of last char
                while (start > 0 && ((unsigned char)line[start] & 0xC0) == 0x80) {  // skip continuation bytes
                    start--;                                                        // move to start of char
                }
                memmove(&line[start], &line[byte_pos], MAX_LINE - byte_pos);        // shift line left
                byte_pos = start;                        // update byte position
                char_pos = count_chars(line, byte_pos);  // update char position
                line[byte_pos] = '\0';                   // null terminate
                redraw_line(line, char_pos);             // redraw updated line
            }
            continue;                                    // continue loop
        }
        
        // printable characters
        if (c >= 32 && byte_pos < MAX_LINE - 4) {    // printable char and space
            memmove(&line[byte_pos + 1], &line[byte_pos], MAX_LINE - byte_pos - 1);  // make room
            line[byte_pos++] = c;                    // insert character
            line[byte_pos] = '\0';                   // null terminate
            char_pos = count_chars(line, byte_pos);  // update char position
            redraw_line(line, char_pos);             // redraw updated line
            continue;                                // continue loop
        }
    }
    
    terminal_disable_raw_mode();  // restore terminal mode
    printf("\r\n");               // print newline
}
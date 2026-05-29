#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#include "execute.h"
#include "add_to_path.h"

#define MAX_LINE 4096
#define MAX_INPUT 65536

static volatile sig_atomic_t g_should_exit = 0;
static struct termios orig_termios;

static void signal_handler(int sig) {
    (void)sig;
    g_should_exit = 1;
}

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
}

static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static int kbhit() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static const char* get_platform_name(void) {
#ifdef _WIN32
    return "Windows";
#elif __ANDROID__
    return "Android";
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS
        return "iOS";
    #elif TARGET_OS_MAC
        return "macOS";
    #else
        return "Apple Unknown";
    #endif
#elif __linux__
    return "Linux";
#else
    return "Unknown OS";
#endif
}

static void execute_code(const char* code, const char* display_name) {
    if (!code || strlen(code) == 0) return;
    
    char temp_file[] = "/tmp/apex_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) return;
    
    FILE* f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(temp_file);
        return;
    }
    
    fwrite(code, 1, strlen(code), f);
    fclose(f);
    
    execute_source(temp_file, display_name);
    
    unlink(temp_file);
}

int main(int argc, char** argv) {
    ensure_path_updated(argv[0]);

    // File argument mode
    if (argc > 1) {
        return execute_source(argv[1], argv[1]) ? 0 : 1;
    }

    // Pipe mode (non-interactive stdin)
    if (!isatty(0)) {
        char temp_file[] = "/tmp/apex_XXXXXX";
        int fd = mkstemp(temp_file);
        if (fd == -1) return 1;
        
        FILE* f = fdopen(fd, "w");
        if (!f) { close(fd); return 1; }
        
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
            fwrite(buf, 1, n, f);
        }
        fclose(f);
        
        bool ok = execute_source(temp_file, "stdin");
        unlink(temp_file);
        return ok ? 0 : 1;
    }

    // Interactive REPL mode
    setup_signals();
    printf("Apex v26.06 on %s. Type code, always ready.\n", get_platform_name());

    set_repl_mode(1);
    enable_raw_mode();

    char full_input[MAX_INPUT];
    char line[MAX_LINE];
    int total_len = 0;
    int pos = 0;
    int prompt_shown = 0;
    
    full_input[0] = '\0';
    
    printf("> ");
    fflush(stdout);
    prompt_shown = 1;

    while (!g_should_exit) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        
        if (n <= 0) {
            if (errno == EINTR) continue;
            break;
        }

        // Ctrl+D — exit
        if (c == 4) {
            break;
        }

        // Ctrl+C — exit immediately
        if (c == 3) {
            break;
        }

        // Enter — submit accumulated input if no pending keystrokes
        if (c == '\r' || c == '\n') {
            printf("\r\n");
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
            
            usleep(50000);
            
            if (!kbhit()) {
                if (total_len > 0) {
                    execute_code(full_input, "<repl>");
                }
                
                total_len = 0;
                full_input[0] = '\0';
                printf("> ");
                fflush(stdout);
                prompt_shown = 1;
            } else {
                prompt_shown = 0;
            }
        }
        // Backspace
        else if (c == 127 || c == '\b') {
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        }
        // Printable characters
        else if (c >= 32 && c < 127 && pos < MAX_LINE - 1) {
            if (!prompt_shown) {
                printf("> ");
                prompt_shown = 1;
            }
            line[pos++] = c;
            printf("%c", c);
            fflush(stdout);
        }
    }

    disable_raw_mode();
    set_repl_mode(0);
    return 0;
}
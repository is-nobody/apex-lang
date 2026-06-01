#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #include <io.h>
    #define isatty _isatty
    #define STDIN_FILENO 0
    #define usleep(x) Sleep((x)/1000)
    
    static HANDLE hStdin;
    static DWORD prev_mode;
    
    static void enable_raw_mode() {
        hStdin = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(hStdin, &prev_mode);
        SetConsoleMode(hStdin, prev_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    }
    
    static void disable_raw_mode() {
        SetConsoleMode(hStdin, prev_mode);
    }
    
    static int getch_nonblock() {
        if (_kbhit()) {
            return _getch();
        }
        return -1;
    }
    
    static ssize_t my_read(int fd, void* buf, size_t count) {
        (void)fd;
        if (count == 0) return 0;
        DWORD nread;
        if (ReadConsole(hStdin, buf, 1, &nread, NULL)) {
            return nread;
        }
        return -1;
    }
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
    #include <sys/time.h>
    
    static struct termios orig_termios;
    
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
    
    #define my_read read
#endif

#include "execute.h"
#include "add_to_path.h"

// ========== Syntax Highlighting ==========
#define ANSI_RESET  "\033[0m"
#define ANSI_BLUE   "\033[34m"  // Операторы
#define ANSI_ORANGE "\033[33m"  // Ключевые слова
#define ANSI_GREEN  "\033[32m"  // Строки
#define ANSI_CYAN   "\033[36m"  // Числа
#define ANSI_GRAY   "\033[90m"  // Комментарии

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

static char* highlight_line(const char* line) {
    if (!line || !*line) return strdup("");
    // Выделяем буфер с запасом под ANSI-коды (примерно x10)
    char* out = (char*)malloc(strlen(line) * 12 + 64);
    int pos = 0, i = 0, len = strlen(line);

    while (i < len) {
        // Комментарии: //
        if (i + 1 < len && line[i] == '/' && line[i+1] == '/') {
            pos += sprintf(out + pos, "%s", ANSI_GRAY);
            while (i < len && line[i] != '\n') out[pos++] = line[i++];
            pos += sprintf(out + pos, "%s", ANSI_RESET);
        }
        // Строки: "..."
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
        // Числа
        else if (isdigit((unsigned char)line[i]) || 
                (line[i] == '.' && i + 1 < len && isdigit((unsigned char)line[i+1]))) {
            pos += sprintf(out + pos, "%s", ANSI_CYAN);
            while (i < len && (isdigit((unsigned char)line[i]) || line[i] == '.')) out[pos++] = line[i++];
            pos += sprintf(out + pos, "%s", ANSI_RESET);
        }
        // Идентификаторы / Ключевые слова
        else if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            int start = i;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            if (is_keyword(line + start, i - start)) {
                pos += sprintf(out + pos, "%s", ANSI_ORANGE);
            }
            memcpy(out + pos, line + start, i - start);
            pos += i - start;
            if (is_keyword(line + start, i - start)) pos += sprintf(out + pos, "%s", ANSI_RESET);
        }
        // Операторы
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
        // Остальные символы
        else {
            out[pos++] = line[i++];
        }
    }
    out[pos] = '\0';
    return out;
}

static void redraw_line(const char* line, int cursor_pos) {
    char* highlighted = highlight_line(line);
    // \r = в начало строки, \033[K = очистить до конца строки
    printf("\r\033[K> %s", highlighted);
    free(highlighted);
    // Курсор: 3 = длина "> " + 1, cursor_pos = видимые символы до курсора
    printf("\033[%dG", 3 + cursor_pos);
    fflush(stdout);
}

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
    #else
    (void)signal_handler;
    #endif
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
    
    #ifdef _WIN32
        char temp_path[MAX_PATH];
        char temp_file[MAX_PATH];
        GetTempPath(MAX_PATH, temp_path);
        GetTempFileName(temp_path, "apex", 0, temp_file);
        FILE* f = fopen(temp_file, "wb");
    #else
        char temp_file[] = "/tmp/apex_XXXXXX";
        int fd = mkstemp(temp_file);
        if (fd == -1) return;
        FILE* f = fdopen(fd, "w");
    #endif
    
    if (!f) {
        #ifndef _WIN32
        close(fd);
        #endif
        return;
    }
    
    fwrite(code, 1, strlen(code), f);
    fclose(f);
    
    execute_source(temp_file, display_name);
    
    unlink(temp_file);
}

int main(int argc, char** argv) {
    ensure_path_updated(argv[0]);

    if (argc > 1) {
        return execute_source(argv[1], argv[1]) ? 0 : 1;
    }

    if (!isatty(0)) {
        #ifdef _WIN32
            char temp_path[MAX_PATH];
            char temp_file[MAX_PATH];
            GetTempPath(MAX_PATH, temp_path);
            GetTempFileName(temp_path, "apex", 0, temp_file);
            FILE* f = fopen(temp_file, "wb");
        #else
            char temp_file[] = "/tmp/apex_XXXXXX";
            int fd = mkstemp(temp_file);
            if (fd == -1) return 1;
            FILE* f = fdopen(fd, "w");
        #endif
        
        if (!f) {
            #ifndef _WIN32
            close(fd);
            #endif
            return 1;
        }
        
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
        ssize_t n = my_read(STDIN_FILENO, &c, 1);
        
        if (n <= 0) {
            #ifndef _WIN32
            if (errno == EINTR) continue;
            #endif
            break;
        }

        if (c == 4) break; // Ctrl+D
        if (c == 3) break; // Ctrl+C

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
            
            #ifdef _WIN32
            int has_kb = _kbhit();
            #else
            int has_kb = kbhit();
            #endif
            
            if (!has_kb) {
                if (total_len > 0) execute_code(full_input, "<repl>");
                total_len = 0;
                full_input[0] = '\0';
                printf("> ");
                fflush(stdout);
            }
        }
        else if (c == 127 || c == '\b') { // Backspace
            if (pos > 0) {
                // Сдвигаем строку влево
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

    disable_raw_mode();
    set_repl_mode(0);
    return 0;
}
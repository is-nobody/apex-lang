#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #include <io.h>
    
    static HANDLE hStdin;
    static DWORD prev_mode;
    
    void platform_init(void) {
        hStdin = GetStdHandle(STD_INPUT_HANDLE);
    }
    
    void terminal_enable_raw_mode(void) {
        GetConsoleMode(hStdin, &prev_mode);
        SetConsoleMode(hStdin, prev_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    }
    
    void terminal_disable_raw_mode(void) {
        SetConsoleMode(hStdin, prev_mode);
    }
    
    bool terminal_has_input(void) {
        return _kbhit() != 0;
    }
    
    int terminal_read_char(void) {
        if (_kbhit()) {
            return _getch();
        }
        return -1;
    }
    
    ssize_t terminal_read_blocking(char* c) {
        DWORD nread;
        if (ReadConsole(hStdin, c, 1, &nread, NULL)) {
            return nread;
        }
        return -1;
    }
    
    const char* platform_get_name(void) {
        return "Windows";
    }
    
    char* platform_create_temp_file(const char* data, size_t len) {
        char temp_path[MAX_PATH];
        char temp_file[MAX_PATH];
        GetTempPath(MAX_PATH, temp_path);
        GetTempFileName(temp_path, "apex", 0, temp_file);
        
        FILE* f = fopen(temp_file, "wb");
        if (!f) return NULL;
        
        fwrite(data, 1, len, f);
        fclose(f);
        
        return strdup(temp_file);
    }
    
    void platform_delete_temp_file(const char* path) {
        DeleteFile(path);
    }
    
#else
    #include <termios.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <errno.h>
    
    static struct termios orig_termios;
    
    void platform_init(void) {
        // Nothing
    }
    
    void terminal_enable_raw_mode(void) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    
    void terminal_disable_raw_mode(void) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
    
    bool terminal_has_input(void) {
        struct timeval tv = {0, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
    }
    
    int terminal_read_char(void) {
        // In POSIX systems, we use select for non-blocking reading
        if (terminal_has_input()) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1) {
                return c;
            }
        }
        return -1;
    }
    
    ssize_t terminal_read_blocking(char* c) {
        return read(STDIN_FILENO, c, 1);
    }
    
    const char* platform_get_name(void) {
        #ifdef __ANDROID__
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
    
    char* platform_create_temp_file(const char* data, size_t len) {
        char temp_file[] = "/tmp/apex_XXXXXX";
        int fd = mkstemp(temp_file);
        if (fd == -1) return NULL;
        
        FILE* f = fdopen(fd, "w");
        if (!f) {
            close(fd);
            return NULL;
        }
        
        fwrite(data, 1, len, f);
        fclose(f);
        
        return strdup(temp_file);
    }
    
    void platform_delete_temp_file(const char* path) {
        unlink(path);
    }
    
#endif
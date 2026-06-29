#define _GNU_SOURCE
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#include <shellapi.h>

#ifdef _MSC_VER
#pragma comment(lib, "shell32.lib")
#endif

static HANDLE hStdin;
static DWORD prev_mode;

// finds a valid temporary directory path on windows
static void get_valid_temp_path(char* out_path, size_t size) {
    DWORD len = GetTempPathA(size, out_path);
    if (len > 0 && GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES) {
        return;
    }
    
    const char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        snprintf(out_path, size, "%s\\AppData\\Local\\Temp", userprofile);
        if (GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES) {
            return;
        }
    }
    
    strcpy(out_path, "C:\\Windows\\Temp");
    CreateDirectoryA(out_path, NULL);
}

// recursively deletes a directory using shell operations
static void delete_directory_recursive(const char* path) {
    SHFILEOPSTRUCT file_op = {
        NULL, FO_DELETE, path, "",
        FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI,
        FALSE, NULL, NULL
    };
    SHFileOperation(&file_op);
}

// initializes windows console handles for raw input
void platform_init(void) {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
}

// enables raw input mode on windows (no echo, no line buffering)
void terminal_enable_raw_mode(void) {
    GetConsoleMode(hStdin, &prev_mode);
    SetConsoleMode(hStdin, prev_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
}

// restores the previous console mode
void terminal_disable_raw_mode(void) {
    SetConsoleMode(hStdin, prev_mode);
}

// checks if a key has been pressed
bool terminal_has_input(void) {
    return _kbhit() != 0;
}

// reads a character without blocking, returns -1 if none available
int terminal_read_char(void) {
    if (_kbhit()) return _getch();
    return -1;
}

// blocks until a character is read
ssize_t terminal_read_blocking(char* c) {
    DWORD nread;
    if (ReadConsole(hStdin, c, 1, &nread, NULL)) return nread;
    return -1;
}

// returns the platform name string
const char* platform_get_name(void) {
    return "Windows";
}

// creates a temporary file with the given data and returns its path
char* platform_create_temp_file(const char* data, size_t len) {
    char temp_path[MAX_PATH];
    char temp_file[MAX_PATH];
    
    get_valid_temp_path(temp_path, MAX_PATH);
    
    UINT res = GetTempFileNameA(temp_path, "apx", 0, temp_file);
    if (res == 0) return NULL;

    FILE* f = fopen(temp_file, "wb");
    if (!f) {
        DeleteFileA(temp_file);
        return NULL;
    }
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    if (written != len) {
        DeleteFileA(temp_file);
        return NULL;
    }
    
    return strdup(temp_file);
}

// deletes a temporary file or directory
void platform_delete_temp_file(const char* path) {
    if (!path) return;
    if (!DeleteFileA(path)) {
        delete_directory_recursive(path);
    }
}

#else
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <ftw.h>
#include <unistd.h>

// callback for nftw to recursively delete files and directories
static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    return remove(fpath);
}

static struct termios orig_termios;

// no special initialization needed on unix
void platform_init(void) {}

// enables raw terminal mode on unix (no echo, no canonical processing)
void terminal_enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// restores the original terminal settings
void terminal_disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// checks if input is available on stdin using select
bool terminal_has_input(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

// reads a character if available, returns -1 otherwise
int terminal_read_char(void) {
    if (terminal_has_input()) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) return c;
    }
    return -1;
}

// blocks until a character is read
ssize_t terminal_read_blocking(char* c) {
    return read(STDIN_FILENO, c, 1);
}

// returns the unix platform name
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

// creates a temporary file in /tmp with the given data
char* platform_create_temp_file(const char* data, size_t len) {
    char temp_file[] = "/tmp/apex_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) return NULL;
    
    FILE* f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        unlink(temp_file);
        return NULL;
    }
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    if (written != len) {
        unlink(temp_file);
        return NULL;
    }
    
    return strdup(temp_file);
}

// deletes a temporary file or directory recursively
void platform_delete_temp_file(const char* path) {
    if (!path) return;
    if (unlink(path) != 0) {
        nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
    }
}
#endif
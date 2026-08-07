// source/utils/platform.c
// Implementation of Platform for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

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

static HANDLE hStdin;    // stdin handle
static DWORD prev_mode;  // previous console mode

// finds a valid temporary directory path on windows
static void get_valid_temp_path(char* out_path, size_t size) {
    DWORD len = GetTempPathA(size, out_path);                                  // get system temp path
    if (len > 0 && GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES) {  // check if exists
        return;                                                                // valid path found
    }
    
    const char* userprofile = getenv("USERPROFILE");                        // try user profile
    if (userprofile) {                                                      // check if set
        snprintf(out_path, size, "%s\\AppData\\Local\\Temp", userprofile);  // build user temp path
        if (GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES) {      // check if exists
            return;                                                         // valid path found
        }
    }
    
    strcpy(out_path, "C:\\Windows\\Temp");                                      // fallback to system temp
    CreateDirectoryA(out_path, NULL);                                           // create if missing
}

// recursively deletes a directory using shell operations
static void delete_directory_recursive(const char* path) {
    SHFILEOPSTRUCT file_op = {                            // shell operation struct
        NULL, FO_DELETE, path, "",                        // delete operation
        FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI,  // silent flags
        FALSE, NULL, NULL
    };
    SHFileOperation(&file_op);                            // execute delete
}

// initializes windows console handles for raw input
void platform_init(void) {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);              // get stdin handle
}

// enables raw input mode on windows (no echo, no line buffering)
void terminal_enable_raw_mode(void) {
    GetConsoleMode(hStdin, &prev_mode);                                            // save current mode
    SetConsoleMode(hStdin, prev_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));  // disable echo and line mode
}

// restores the previous console mode
void terminal_disable_raw_mode(void) {
    SetConsoleMode(hStdin, prev_mode);  // restore saved mode
}

// checks if a key has been pressed
bool terminal_has_input(void) {
    return _kbhit() != 0;           // check for keyboard input
}

// reads a character without blocking, returns -1 if none available
int terminal_read_char(void) {
    if (_kbhit()) return _getch();  // get char if available
    return -1;                      // no input
}

// blocks until a character is read
ssize_t terminal_read_blocking(char* c) {
    DWORD nread;                    // bytes read
    if (ReadConsole(hStdin, c, 1, &nread, NULL)) return nread;  // read one char
    return -1;                      // read failed
}

// returns the platform name string
const char* platform_get_name(void) {
    return "Windows";                          // windows platform
}

// creates a temporary file with the given data and returns its path
char* platform_create_temp_file(const char* data, size_t len) {
    char temp_path[MAX_PATH];                  // temp directory buffer
    char temp_file[MAX_PATH];                  // temp file buffer
    
    get_valid_temp_path(temp_path, MAX_PATH);  // get valid temp path
    
    UINT res = GetTempFileNameA(temp_path, "apx", 0, temp_file);  // create temp filename
    if (res == 0) return NULL;                 // creation failed

    FILE* f = fopen(temp_file, "wb");          // open temp file
    if (!f) {                                  // check open
        DeleteFileA(temp_file);                // cleanup on failure
        return NULL;                           // return null
    }
    
    size_t written = fwrite(data, 1, len, f);   // write data
    fclose(f);                                  // close file
    
    if (written != len) {        // check write success
        DeleteFileA(temp_file);  // cleanup on failure
        return NULL;                                                            // return null
    }
    
    return strdup(temp_file);    // return duplicated path
}

// deletes a temporary file or directory
void platform_delete_temp_file(const char* path) {
    if (!path) return;                     // guard against null
    if (!DeleteFileA(path)) {              // try to delete file
        delete_directory_recursive(path);  // if fails, delete directory recursively
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
    (void)sb;              // unused parameter
    (void)typeflag;        // unused parameter
    (void)ftwbuf;          // unused parameter
    return remove(fpath);  // delete file/directory
}

static struct termios orig_termios;                                             // original terminal settings

// no special initialization needed on unix
void platform_init(void) {}

// enables raw terminal mode on unix (no echo, no canonical processing)
void terminal_enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);                                     // save current settings
    struct termios raw = orig_termios;                                          // copy settings
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);             // disable echo, canonical, signals
    raw.c_cc[VMIN] = 1;                                 // minimum chars to read
    raw.c_cc[VTIME] = 0;                                // no timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);           // apply settings
}

// restores the original terminal settings
void terminal_disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);  // restore saved settings
}

// checks if input is available on stdin using select
bool terminal_has_input(void) {
    struct timeval tv = {0, 0};                                  // no timeout
    fd_set fds;                                                  // file descriptor set
    FD_ZERO(&fds);                                               // clear set
    FD_SET(STDIN_FILENO, &fds);                                  // add stdin
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;  // check for input
}

// reads a character if available, returns -1 otherwise
int terminal_read_char(void) {
    if (terminal_has_input()) {                        // check if input available
        char c;                                        // char buffer
        if (read(STDIN_FILENO, &c, 1) == 1) return c;  // read char
    }
    return -1;                                         // no input
}

// blocks until a character is read
ssize_t terminal_read_blocking(char* c) {
    return read(STDIN_FILENO, c, 1);  // blocking read
}

// returns the unix platform name
const char* platform_get_name(void) {
#ifdef __ANDROID__
    return "Android";                 // android platform
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS
        return "iOS";                 // ios platform
    #elif TARGET_OS_MAC
        return "macOS";               // macos platform
    #else
        return "Apple Unknown";       // unknown apple
    #endif
#elif __linux__
    return "Linux";                   // linux platform
#else
    return "Unknown OS";              // unknown platform
#endif
}

// creates a temporary file in /tmp with the given data
char* platform_create_temp_file(const char* data, size_t len) {
    char temp_file[] = "/tmp/apex_XXXXXX";     // template for temp file
    int fd = mkstemp(temp_file);               // create unique temp file
    if (fd == -1) return NULL;                 // creation failed
    
    FILE* f = fdopen(fd, "wb");                // open file descriptor
    if (!f) {                                  // check open
        close(fd);                             // close fd
        unlink(temp_file);                     // remove file
        return NULL;                           // return null
    }
    
    size_t written = fwrite(data, 1, len, f);  // write data
    fclose(f);                                 // close file
    
    if (written != len) {                      // check write success
        unlink(temp_file);                     // remove file on failure
        return NULL;                           // return null
    }
    
    return strdup(temp_file);                  // return duplicated path
}

// deletes a temporary file or directory recursively
void platform_delete_temp_file(const char* path) {
    if (!path) return;                                    // guard against null
    if (unlink(path) != 0) {                              // try to delete file
        nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);  // recursively delete directory
    }
}
#endif
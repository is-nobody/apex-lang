// main.c
// Implementation of Main Entry Point for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)  // windows uses _mkdir instead of mkdir
#define isatty _isatty                  // windows uses _isatty instead of isatty
#define STDIN_FILENO 0                  // stdin file descriptor on windows
#else
#include <unistd.h>
#include <limits.h>
#endif

#include "execute.h"
#include "repl.h"
#include "platform.h"
#include "commands.h"

#define MARKER "__APEX_BIN_PAYLOAD__"  // magic marker identifying embedded payload in binary

// recursively creates directories for a given path
static void mkdirp(const char* path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);     // copy path to mutable buffer
    for (char* p = tmp + 1; *p; p++) {          // skip leading slash, walk through path
        if (*p == '/') {
            *p = 0;                             // temporarily null terminate at separator
            mkdir(tmp, 0755);                   // create directory component
            *p = '/';                           // restore separator
        }
    }
    mkdir(tmp, 0755);                           // create final directory in path
}

// extracts and executes embedded source from a compiled apex binary
static int execute_embedded_source(int argc, char** argv) {
    char exe_path[4096];                        // buffer for executable path
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));  // get full path of current executable
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);  // read symlink to get exe path
    if (len == -1) return -1;                   // failed to read executable path
    exe_path[len] = '\0';                       // null terminate the path string
#endif

    FILE* f = fopen(exe_path, "rb");            // open own binary for reading
    if (!f) return -1;                          // failed to open, not embedded or error
    fseek(f, 0, SEEK_END);                      // seek to end of file
    long file_size = ftell(f);                  // get total file size
    if (file_size < 24) { fclose(f); return -1; }  // too small to contain embedded payload

    fseek(f, file_size - 20, SEEK_SET);         // seek to where marker should be (20 bytes from end)
    char marker[21] = {0};                      // buffer for marker string + null
    if (fread(marker, 1, 20, f) != 20 || strcmp(marker, MARKER) != 0) {  // read and verify marker
        fclose(f); return -1;                   // marker mismatch, not an embedded binary
    }

    fseek(f, file_size - 24, SEEK_SET);         // seek to payload size field (4 bytes before marker)
    uint32_t payload_size = 0;                  // size of embedded payload
    if (fread(&payload_size, 4, 1, f) != 1 || payload_size == 0) {  // read payload size
        fclose(f); return -1;                   // failed to read or empty payload
    }

    long payload_start = file_size - 24 - payload_size;  // calculate start offset of payload data
    fseek(f, payload_start, SEEK_SET);                   // seek to payload start
    char* payload = (char*)malloc(payload_size);         // allocate buffer for payload
    if (!payload) { fclose(f); return -1; }              // allocation failed
    if (fread(payload, 1, payload_size, f) != payload_size) {  // read entire payload into memory
        free(payload);                                   // clean up on read failure
        fclose(f);
        return -1;
    }
    fclose(f);                                      // done reading binary

    char temp_dir[4096];                            // buffer for temporary directory path
#ifdef _WIN32
    char temp_base[MAX_PATH];                       // buffer for temp directory base path
    DWORD len = GetTempPathA(MAX_PATH, temp_base);  // get system temp directory
    
    if (len == 0 || GetFileAttributesA(temp_base) == INVALID_FILE_ATTRIBUTES) {      // temp path invalid or unavailable
        const char* userprofile = getenv("USERPROFILE");  // fallback to user profile directory
        if (userprofile) {
            snprintf(temp_base, MAX_PATH, "%s\\AppData\\Local\\Temp", userprofile);  // use local appdata temp
            if (GetFileAttributesA(temp_base) == INVALID_FILE_ATTRIBUTES) {  // still invalid
                strcpy(temp_base, "C:\\Windows\\Temp");   // ultimate fallback to windows temp
                CreateDirectoryA(temp_base, NULL);        // ensure directory exists
            }
        } else {
            strcpy(temp_base, "C:\\Windows\\Temp");       // no userprofile, use windows temp
            CreateDirectoryA(temp_base, NULL);            // ensure directory exists
        }
    }
    
    snprintf(temp_dir, sizeof(temp_dir), "%s\\apex_embedded_%lu", temp_base, GetCurrentProcessId());  // unique temp dir per process
#else
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/apex_embedded_%d", getpid());  // unique temp dir per process on unix
#endif
    mkdirp(temp_dir);                                // create the temporary directory tree

    char* ptr = payload;                             // pointer to walk through payload data
    uint32_t num_files = *(uint32_t*)ptr; ptr += 4;  // read number of embedded files
    char main_script[4096] = {0};                    // buffer for main script path

    for (uint32_t i = 0; i < num_files; i++) {
        uint32_t name_len = *(uint32_t*)ptr; ptr += 4;     // read filename length
        char name[4096] = {0};                             // buffer for filename
        memcpy(name, ptr, name_len); ptr += name_len;      // copy filename and advance pointer
        
        uint32_t content_len = *(uint32_t*)ptr; ptr += 4;  // read file content length
        char* content = ptr; ptr += content_len;           // point to content and advance pointer
        
        char full_path[8192];                              // buffer for full output path
        snprintf(full_path, sizeof(full_path), "%s/%s", temp_dir, name);  // construct full path
        
        char* last_slash = strrchr(full_path, '/');        // find last path separator
        if (last_slash) {
            *last_slash = '\0';                            // temporarily truncate to directory path
            mkdirp(full_path);                             // create parent directories
            *last_slash = '/';                             // restore full path
        }

        FILE* out_f = fopen(full_path, "wb");              // open output file for writing
        if (out_f) {
            fwrite(content, 1, content_len, out_f);        // write file content
            fclose(out_f);                                 // close output file
        }

        if (i == 0) strncpy(main_script, full_path, sizeof(main_script) - 1);  // first file is the main script
    }
    free(payload);                                     // release payload memory

    bool ok = execute_source(main_script, main_script, argc, argv, false);  // execute the extracted main script
    
    platform_delete_temp_file(temp_dir);               // clean up temporary directory
    
    return ok ? 0 : 1;                                 // return exit code based on success
}

// executes code piped from stdin
static int execute_from_stdin(void) {
    char* temp_path = NULL;                            // path to temporary file
    size_t buf_size = 4096;                            // initial read buffer size
    size_t total_read = 0;                             // total bytes read from stdin
    char* data = malloc(buf_size);                     // allocate initial read buffer
    if (!data) return 1;                               // allocation failed
    size_t n;
    while ((n = fread(data + total_read, 1, buf_size - total_read - 1, stdin)) > 0) {  // read stdin in chunks
        total_read += n;                               // accumulate total bytes read
        if (total_read + 4096 >= buf_size) {           // buffer nearly full, need to grow
            buf_size *= 2;                             // double buffer size
            char* new_data = realloc(data, buf_size);  // resize buffer
            if (!new_data) {                           // realloc failed
                free(data);                            // free original buffer
                return 1;                              // return error
            }
            data = new_data;                           // update data pointer
        }
    }
    data[total_read] = '\0';                          // null terminate the input data
    temp_path = platform_create_temp_file(data, total_read);  // write stdin data to temp file
    free(data);                                       // free input buffer
    if (!temp_path) {                                 // temp file creation failed
        print_error("Cannot create temporary file");  // report error
        return 1;                                     // return error
    }
    bool ok = execute_source(temp_path, "stdin", 0, NULL, false);  // execute the temp file as "stdin"
    platform_delete_temp_file(temp_path);          // clean up temp file
    free(temp_path);                               // free path string
    return ok ? 0 : 1;                             // return exit code based on success
}

// main entry point: checks for embedded binary, then commands, then file or repl
int main(int argc, char** argv) {
    int embedded_result = execute_embedded_source(argc, argv);  // try to run as embedded binary first
    if (embedded_result >= 0) {
        return embedded_result;                    // embedded payload found and executed
    }

    platform_init();                               // initialize platform-specific features

    int cmd_result = handle_commands(argc, argv);  // check for built-in commands (build, run, etc.)
    if (cmd_result >= 0) {
        return cmd_result;                         // command handled, exit with its result
    }

    int result = 0;                                // default exit code

    if (argc > 1) {
        result = execute_source(argv[1], argv[1], argc, argv, true) ? 0 : 1;  // execute script file from argument
    }
    else if (!isatty(STDIN_FILENO)) {
        result = execute_from_stdin();             // no file argument, read from piped stdin
    }
    else {
        repl_run();                                // no file and no pipe, start interactive repl
    }

    return result;                                 // return final exit code
}
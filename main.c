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
#include "loader.h"

#define MARKER "__APEX_BIN_PAYLOAD__"  // magic marker identifying embedded payload in binary

// reads a uint32_t in little-endian order
static uint32_t read_u32_from_file(FILE* f) {
    uint8_t buf[4];                          // byte buffer
    if (fread(buf, 1, 4, f) != 4) return 0;  // read 4 bytes or fail
    return (uint32_t)buf[0] |                // reconstruct little-endian
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

// extracts and executes embedded bytecode from a compiled apex binary
static int execute_embedded_bytecode(int argc, char** argv) {
    char exe_path[4096];                         // buffer for executable path
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));  // get full path of current executable
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);  // read symlink to get exe path
    if (len == -1) {                                       // readlink failed
        strncpy(exe_path, argv[0], sizeof(exe_path) - 1);  // fallback to argv[0]
        exe_path[sizeof(exe_path) - 1] = '\0';             // null terminate
    } else {
        exe_path[len] = '\0';                              // null terminate the path string
    }
#endif

    FILE* f = fopen(exe_path, "rb");  // open own binary for reading
    if (!f) return -1;                // failed to open, not embedded or error
    
    fseek(f, 0, SEEK_END);            // seek to end of file
    long file_size = ftell(f);        // get total file size
    
    if (file_size < 24) {             // too small to contain embedded payload
        fclose(f);
        return -1;
    }
    
    fseek(f, file_size - 20, SEEK_SET);   // seek to marker position (20 bytes from end)
    char marker[21] = {0};                // buffer for marker string + null
    if (fread(marker, 1, 20, f) != 20) {  // read marker
        fclose(f);
        return -1;
    }
    
    if (strcmp(marker, MARKER) != 0) {    // verify marker matches
        fclose(f);
        return -1;                        // marker mismatch, not an embedded binary
    }
    
    fseek(f, file_size - 24, SEEK_SET);             // seek to payload size field (4 bytes before marker)
    uint32_t payload_size = read_u32_from_file(f);  // read payload size
    
    if (payload_size == 0 || payload_size > 100 * 1024 * 1024) {  // sanity check (max 100MB)
        fclose(f);
        return -1;
    }
    
    // Read payload (payload_size bytes before size)
    long payload_start = file_size - 24 - payload_size;  // calculate start offset of payload data
    if (payload_start < 0) {                             // invalid payload start
        fclose(f);
        return -1;
    }
    
    fseek(f, payload_start, SEEK_SET);            // seek to payload start
    char* payload = (char*)malloc(payload_size);  // allocate buffer for payload
    if (!payload) {                               // allocation failed
        fclose(f);
        return -1;
    }
    
    if (fread(payload, 1, payload_size, f) != payload_size) {  // read entire payload into memory
        free(payload);                                         // clean up on read failure
        fclose(f);
        return -1;
    }
    fclose(f);  // done reading binary
    
    char temp_path[4096];                           // buffer for temporary file path
#ifdef _WIN32
    char temp_base[MAX_PATH];                       // buffer for temp directory base path
    DWORD len = GetTempPathA(MAX_PATH, temp_base);  // get system temp directory
    
    if (len == 0 || GetFileAttributesA(temp_base) == INVALID_FILE_ATTRIBUTES) {  // temp path invalid or unavailable
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
    
    snprintf(temp_path, sizeof(temp_path), "%s\\apex_bytecode_%lu.apexc", temp_base, GetCurrentProcessId());  // unique temp file per process
#else
    snprintf(temp_path, sizeof(temp_path), "/tmp/apex_bytecode_%d.apexc", getpid());  // unique temp file per process on unix
#endif

    FILE* out = fopen(temp_path, "wb");     // open temp file for writing
    if (!out) {                             // failed to create temp file
        free(payload);
        return -1;
    }
    
    fwrite(payload, 1, payload_size, out);  // write payload to temp file
    fclose(out);                            // close temp file
    free(payload);                          // release payload memory
    
    // Execute the bytecode file
    bool ok = execute_bytecode_file(temp_path, argc, argv, false);  // execute the extracted bytecode
    
    // Clean up
    remove(temp_path);                      // delete temporary file
    
    return ok ? 0 : 1;                      // return exit code based on success
}

// executes code piped from stdin
static int execute_from_stdin(void) {
    char* temp_path = NULL;                 // path to temporary file
    size_t buf_size = 4096;                 // initial read buffer size
    size_t total_read = 0;                  // total bytes read from stdin
    char* data = malloc(buf_size);          // allocate initial read buffer
    if (!data) return 1;                    // allocation failed
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
    data[total_read] = '\0';                                  // null terminate the input data
    temp_path = platform_create_temp_file(data, total_read);  // write stdin data to temp file
    free(data);                                               // free input buffer
    if (!temp_path) {                                         // temp file creation failed
        print_error("Cannot create temporary file");          // report error
        return 1;                                             // return error
    }
    bool ok = execute_source(temp_path, "stdin", 0, NULL, false);  // execute the temp file as "stdin"
    platform_delete_temp_file(temp_path);                          // clean up temp file
    free(temp_path);                                               // free path string
    return ok ? 0 : 1;                                             // return exit code based on success
}

// checks if a file has a given extension
static bool has_extension(const char* filename, const char* ext) {
    size_t len = strlen(filename);                      // filename length
    size_t ext_len = strlen(ext);                       // extension length
    if (len < ext_len) return false;                    // filename too short
    return strcmp(filename + len - ext_len, ext) == 0;  // compare extension
}

// main entry point: checks for embedded bytecode, then commands, then file or repl
int main(int argc, char** argv) {
    int embedded_result = execute_embedded_bytecode(argc, argv);  // try to run as embedded binary first
    if (embedded_result >= 0) {
        return embedded_result;                    // embedded payload found and executed
    }

    platform_init();                               // initialize platform-specific features

    int cmd_result = handle_commands(argc, argv);  // check for built-in commands (build, compile, etc.)
    if (cmd_result >= 0) {
        return cmd_result;                         // command handled, exit with its result
    }

    int result = 0;                                // default exit code

    if (argc > 1) {                                // file argument provided
        const char* filename = argv[1];            // get filename
        
        if (has_extension(filename, ".apexc")) {   // check if bytecode file
            result = execute_bytecode_file(filename, argc, argv, true) ? 0 : 1;  // execute bytecode
        } else {
            result = execute_source(filename, filename, argc, argv, true) ? 0 : 1;  // execute source
        }
    }
    else if (!isatty(STDIN_FILENO)) {              // no file argument, read from piped stdin
        result = execute_from_stdin();
    }
    else {                                         // no file and no pipe, start interactive repl
        repl_run();
    }

    return result;                                 // return final exit code
}
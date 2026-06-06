#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define isatty _isatty
#define STDIN_FILENO 0
#else
#include <unistd.h>
#include <limits.h>
#endif
#include "execute.h"
#include "repl.h"
#include "platform.h"
#include "add_to_path.h"
#include "commands.h"

// Marker used in the build process
#define MARKER "__APEX_BIN_PAYLOAD__"

static int execute_embedded_source(void) {
    char exe_path[4096];
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        fprintf(stderr, "Cannot resolve executable path for embedded execution\n");
        return 1;
    } else {
        exe_path[len] = '\0';
    }
#endif

    FILE* f = fopen(exe_path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open self for embedded execution\n");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    
    // Footer format: [N bytes: payload] [4 bytes: payload_size] [20 bytes: MARKER]
    if (file_size < 24) {
        fclose(f);
        return -1; 
    }

    // 1. Check Marker (last 20 bytes)
    fseek(f, file_size - 20, SEEK_SET);
    char marker[21] = {0};
    if (fread(marker, 1, 20, f) != 20 || strcmp(marker, MARKER) != 0) {
        fclose(f);
        return -1; 
    }

    // 2. Read Payload Size (4 bytes before Marker)
    fseek(f, file_size - 24, SEEK_SET);
    uint32_t payload_size = 0;
    if (fread(&payload_size, 4, 1, f) != 1) {
        fclose(f);
        return 1;
    }

    if (payload_size == 0 || file_size < 24 + payload_size) {
        fclose(f);
        fprintf(stderr, "Corrupted embedded binary\n");
        return 1;
    }

    // 3. Read Payload
    long payload_start = file_size - 24 - payload_size;
    fseek(f, payload_start, SEEK_SET);
    
    char* payload = (char*)malloc(payload_size + 1);
    if (!payload) {
        fclose(f);
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    if (fread(payload, 1, payload_size, f) != payload_size) {
        free(payload);
        fclose(f);
        fprintf(stderr, "Failed to read embedded payload\n");
        return 1;
    }
    payload[payload_size] = '\0';
    fclose(f);

    // 4. Execute the extracted source
    char* temp_path = platform_create_temp_file(payload, payload_size);
    free(payload);
    
    if (!temp_path) {
        fprintf(stderr, "Cannot create temporary file for embedded execution\n");
        return 1;
    }

    bool ok = execute_source(temp_path, "embedded_script");
    
    platform_delete_temp_file(temp_path);
    free(temp_path);

    return ok ? 0 : 1;
}

// Execute code from stdin (pipe)
static int execute_from_stdin(void) {
    char* temp_path = NULL;
    size_t buf_size = 4096;
    size_t total_read = 0;
    char* data = malloc(buf_size);
    if (!data) return 1;
    size_t n;
    while ((n = fread(data + total_read, 1, buf_size - total_read - 1, stdin)) > 0) {
        total_read += n;
        if (total_read + 4096 >= buf_size) {
            buf_size *= 2;
            char* new_data = realloc(data, buf_size);
            if (!new_data) {
                free(data);
                return 1;
            }
            data = new_data;
        }
    }
    data[total_read] = '\0';
    temp_path = platform_create_temp_file(data, total_read);
    free(data);
    if (!temp_path) {
        print_error("Cannot create temporary file");
        return 1;
    }
    bool ok = execute_source(temp_path, "stdin");
    platform_delete_temp_file(temp_path);
    free(temp_path);
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    // 1. Check if this is a compiled Apex binary
    int embedded_result = execute_embedded_source();
    if (embedded_result >= 0) {
        return embedded_result;
    }

    // 2. Normal Interpreter Logic
    ensure_path_updated(argv[0]);
    platform_init();

    // Handle specific commands (version, build, etc.)
    int cmd_result = handle_commands(argc, argv);
    if (cmd_result >= 0) {
        return cmd_result;
    }

    int result = 0;

    // Execute file passed as argument
    if (argc > 1) {
        result = execute_source(argv[1], argv[1]) ? 0 : 1;
    }
    // Read from stdin (pipe/redirect)
    else if (!isatty(STDIN_FILENO)) {
        result = execute_from_stdin();
    }
    // Interactive mode (REPL)
    else {
        repl_run();
    }

    return result;
}
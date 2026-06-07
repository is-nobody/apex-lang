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
#define mkdir(path, mode) _mkdir(path)
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

static void mkdirp(const char* path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static int execute_embedded_source(void) {
    char exe_path[4096];
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) return -1;
    exe_path[len] = '\0';
#endif

    FILE* f = fopen(exe_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    if (file_size < 24) { fclose(f); return -1; }

    fseek(f, file_size - 20, SEEK_SET);
    char marker[21] = {0};
    if (fread(marker, 1, 20, f) != 20 || strcmp(marker, MARKER) != 0) {
        fclose(f); return -1;
    }

    fseek(f, file_size - 24, SEEK_SET);
    uint32_t payload_size = 0;
    if (fread(&payload_size, 4, 1, f) != 1 || payload_size == 0) {
        fclose(f); return -1;
    }

    long payload_start = file_size - 24 - payload_size;
    fseek(f, payload_start, SEEK_SET);
    char* payload = (char*)malloc(payload_size);
    if (!payload) { fclose(f); return -1; }
    fread(payload, 1, payload_size, f);
    fclose(f);

    // Create temporary directory
    char temp_dir[4096];
#ifdef _WIN32
    char temp_base[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_base);
    snprintf(temp_dir, sizeof(temp_dir), "%s\\apex_embedded_%lu", temp_base, GetCurrentProcessId());
#else
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/apex_embedded_%d", getpid());
#endif
    mkdirp(temp_dir);

    char* ptr = payload;
    uint32_t num_files = *(uint32_t*)ptr; ptr += 4;
    char main_script[4096] = {0};

    for (uint32_t i = 0; i < num_files; i++) {
        uint32_t name_len = *(uint32_t*)ptr; ptr += 4;
        char name[4096] = {0};
        memcpy(name, ptr, name_len); ptr += name_len;
        
        uint32_t content_len = *(uint32_t*)ptr; ptr += 4;
        char* content = ptr; ptr += content_len;
        
        char full_path[8192];
        snprintf(full_path, sizeof(full_path), "%s/%s", temp_dir, name);
        
        // Create subdirectories if needed
        char* last_slash = strrchr(full_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            mkdirp(full_path);
            *last_slash = '/';
        }

        FILE* out_f = fopen(full_path, "wb");
        if (out_f) {
            fwrite(content, 1, content_len, out_f);
            fclose(out_f);
        }

        if (i == 0) strncpy(main_script, full_path, sizeof(main_script) - 1);
    }
    free(payload);

    // Execute the extracted main script
    bool ok = execute_source(main_script, main_script);
    
    // Cleanup (Optional: you might want to leave it for debugging or delete it)
    // system("rm -rf ..."); 
    
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
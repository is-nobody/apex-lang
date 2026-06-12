#include "bridge.h"
#include "execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

// DISTINCT MARKER for built-in language scripts
#define BUILTIN_MARKER "__APEX_INT_PAYLOAD__"

bool bridge_execute(const char* target_path) {
    char exe_path[4096];
#ifdef _WIN32
    if (!GetModuleFileNameA(NULL, exe_path, sizeof(exe_path))) return false;
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) return false;
    exe_path[len] = '\0';
#endif

    FILE* f = fopen(exe_path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    if (file_size < 24) { fclose(f); return false; }

    // 1. Verify built-in payload marker
    fseek(f, file_size - 20, SEEK_SET);
    char marker[21] = {0};
    if (fread(marker, 1, 20, f) != 20 || strcmp(marker, BUILTIN_MARKER) != 0) {
        fclose(f); 
        fprintf(stderr, "[Bridge Error] No built-in payload found.\n");
        return false;
    }

    // 2. Read total payload size
    fseek(f, file_size - 24, SEEK_SET);
    uint32_t payload_size = 0;
    if (fread(&payload_size, 4, 1, f) != 1 || payload_size == 0) {
        fclose(f); return false;
    }

    // 3. Seek to the start of the VFS payload
    long payload_start = file_size - 24 - payload_size;
    fseek(f, payload_start, SEEK_SET);

    uint32_t num_files = 0;
    if (fread(&num_files, 4, 1, f) != 1) { fclose(f); return false; }

    bool found = false;
    bool result = false;

    // 4. Iterate through embedded files to find the target
    for (uint32_t i = 0; i < num_files; i++) {
        uint32_t name_len = 0;
        if (fread(&name_len, 4, 1, f) != 1) break;

        char* name = (char*)malloc(name_len + 1);
        if (!name) break;
        if (fread(name, 1, name_len, f) != name_len) { free(name); break; }
        name[name_len] = '\0';

        uint32_t content_len = 0;
        if (fread(&content_len, 4, 1, f) != 1) { free(name); break; }

        if (strcmp(name, target_path) == 0) {
            char* content = (char*)malloc(content_len + 1);
            if (content) {
                if (fread(content, 1, content_len, f) == content_len) {
                    content[content_len] = '\0';
                    // Execute directly from memory
                    result = execute_source_string(content, target_path);
                    found = true;
                }
                free(content);
            }
            free(name);
            break; // Target found and processed
        } else {
            // Skip this file's content to read the next header
            fseek(f, content_len, SEEK_CUR);
        }
        free(name);
    }

    fclose(f);
    
    if (!found) {
        fprintf(stderr, "[Bridge Error] File '%s' not found in built-in payload.\n", target_path);
    }
    
    return found ? result : false;
}

bool bridge_execute_settings(void) {
    return bridge_execute(".EMBEDDED/source/embedded/settings.apex");
}
#include "apex_api.h"
#include "execute.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <process.h>
  #define getpid _getpid
#else
  #include <unistd.h>
#endif

static bool is_initialized = false;

// initializes the apex runtime with platform-specific setup
void apex_init(void) {
    if (is_initialized) return;
    
    platform_init();
    
    is_initialized = true;
}

// shuts down the apex runtime and releases global resources
void apex_shutdown(void) {
    if (!is_initialized) return;
    
    is_initialized = false;
}

// executes a source code string with the given filename for error context
bool apex_execute(const char* filepath, const char* source_code) {
    if (!is_initialized) {
        apex_init();
    }

    if (!filepath) {
        print_error("Invalid filepath provided to apex_execute");
        return false;
    }

    if (!source_code) {
        return execute_source(filepath, filepath);
    }

    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/apex_exec_%d.apex", getpid());

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        print_error("Failed to create temporary file for execution");
        return false;
    }

    size_t len = strlen(source_code);
    if (fwrite(source_code, 1, len, f) != len) {
        print_error("Failed to write source to temporary file");
        fclose(f);
        remove(tmp_path);
        return false;
    }
    fclose(f);

    bool result = execute_source(tmp_path, filepath);
    remove(tmp_path);
    return result;
}
#include "apex_api.h"
#include "execute.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
bool apex_execute(const char* source_code, const char* filename) {
    if (!is_initialized) {
        apex_init();
    }

    if (!source_code || !filename) {
        print_error("Invalid arguments provided to apex_execute");
        return false;
    }

    return execute_source_string(source_code, filename);
}

// executes a source file by its filesystem path
bool apex_execute_file(const char* filepath) {
    if (!is_initialized) {
        apex_init();
    }

    if (!filepath) {
        print_error("Invalid filepath provided to apex_execute_file");
        return false;
    }

    return execute_source(filepath, filepath);
}
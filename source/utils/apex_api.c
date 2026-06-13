#include "apex_api.h"
#include "execute.h"
#include "platform.h"
#include "add_to_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool is_initialized = false;

void apex_init(void) {
    if (is_initialized) return;
    
    // Initialize platform-specific features (path handling, etc.)
    platform_init();
    
    // Ensure the Apex binary directory is in the path for module loading
    // We pass a dummy argv[0] here; in a real app, you might want to pass the actual path
    ensure_path_updated("apex");
    
    is_initialized = true;
}

void apex_shutdown(void) {
    if (!is_initialized) return;
    
    // Add any global cleanup here if needed in the future
    is_initialized = false;
}

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

bool apex_execute_file(const char* filepath) {
    if (!is_initialized) {
        apex_init();
    }

    if (!filepath) {
        print_error("Invalid filepath provided to apex_execute_file");
        return false;
    }

    // Use the filepath as the filename identifier for error reporting
    return execute_source(filepath, filepath);
}
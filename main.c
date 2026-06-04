#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <io.h>
    #define isatty _isatty
    #define STDIN_FILENO 0
#else
    #include <unistd.h>
#endif

#include "execute.h"
#include "repl.h"
#include "platform.h"
#include "add_to_path.h"

// Execute code from stdin (pipe)
static int execute_from_stdin(void) {
    char* temp_path = NULL;
    
    // Read all stdin into buffer
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
    
    // Create temporary file
    temp_path = platform_create_temp_file(data, total_read);
    free(data);
    
    if (!temp_path) {
        print_error("Cannot create temporary file");
        return 1;
    }
    
    // Execute code
    bool ok = execute_source(temp_path, "stdin");
    
    // Delete temporary file
    platform_delete_temp_file(temp_path);
    free(temp_path);
    
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    // Update PATH if necessary
    ensure_path_updated(argv[0]);
    
    // Initialize platform
    platform_init();
    
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
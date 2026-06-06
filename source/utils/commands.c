#include "commands.h"
#include "execute.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Detect compiler for the version string
#if defined(__clang__)
    #define COMPILER_INFO "Clang " __clang_version__
#elif defined(__GNUC__)
    #define COMPILER_INFO "GCC " __VERSION__
#elif defined(_MSC_VER)
    #define COMPILER_INFO "MSVC"
#else
    #define COMPILER_INFO "Unknown Compiler"
#endif

int handle_commands(int argc, char** argv) {
    if (argc < 2) return -1;

    // --- VERSION COMMAND ---
    if (strcmp(argv[1], "version") == 0) {
        printf("apex-lang:main v26.06 [%s] on %s\n", COMPILER_INFO, platform_get_name());
        return 0;
    }

    // --- BUILD COMMAND ---
    if (strcmp(argv[1], "build") == 0) {
        if (argc < 3) {
            print_error("Missing filename for 'build' command.");
            return 1;
        }
        
        const char* filename = argv[2];
        
        // Green message: Building...
        printf("\033[32mBuilding %s...\033[0m\n", filename);
        
        // Execute the source file
        bool success = execute_source(filename, filename);
        
        if (success) {
            // Green message: Completed!
            printf("\033[32mBuilding %s completed!\033[0m\n", filename);
            return 0;
        } else {
            // execute_source already prints detailed errors in red
            return 1;
        }
    }

    // --- INSTALL COMMAND ---
    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            print_error("Missing package name for 'install' command.");
            return 1;
        }

        const char* name = argv[2];

        // Green message: Requesting...
        printf("\033[32mRequest to the repository %s...\033[0m\n", name);

        // Red message: Error (Dummy implementation)
        fprintf(stderr, "\033[31mError: no response from the repository\033[0m\n");
        
        return 1;
    }

    // --- PACK COMMAND ---
    if (strcmp(argv[1], "pack") == 0) {
        if (argc < 3) {
            print_error("Missing project path for 'pack' command.");
            return 1;
        }

        const char* path = argv[2];

        // Green message
        printf("\033[32mPacking %s...\033[0m\n", path);

        // Red message: Dummy error
        fprintf(stderr, "\033[31mError: packing is not yet implemented\033[0m\n");
        
        return 1;
    }

    // --- PUBLISH COMMAND ---
    if (strcmp(argv[1], "publish") == 0) {
        if (argc < 3) {
            print_error("Missing package name for 'publish' command.");
            return 1;
        }

        const char* name = argv[2];

        // Green message
        printf("\033[32mPublishing %s to the registry...\033[0m\n", name);

        // Red message: Dummy error
        fprintf(stderr, "\033[31mError: publishing is not yet implemented\033[0m\n");
        
        return 1;
    }

    // Not a recognized command, fallback to default behavior
    return -1;
}
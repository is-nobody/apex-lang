#include "commands.h"
#include "execute.h"
#include "platform.h"
#include "build.h"
#include "generate_ir.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#endif

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
        return build_command(argc, argv);
    }

    // --- COMPILE COMMAND ---
    if (strcmp(argv[1], "compile") == 0) {
        return compile_command(argc, argv);
    }

    // --- INSTALL COMMAND ---
    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            print_error("Missing package name for 'install' command.");
            return 1;
        }
        const char* name = argv[2];
        printf("\033[32mRequest to the repository %s...\033[0m\n", name);
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
        printf("\033[32mPacking %s...\033[0m\n", path);
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
        printf("\033[32mPublishing %s to the registry...\033[0m\n", name);
        fprintf(stderr, "\033[31mError: publishing is not yet implemented\033[0m\n");
        return 1;
    }

    // Not a recognized command, fallback to default behavior
    return -1;
}
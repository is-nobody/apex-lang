#include "commands.h"
#include "execute.h"
#include "platform.h"
#include "build.h"
#include "generate_ir.h"
#include "package_manager.h"
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
        return install_package(argv[2]);
    }

    // --- PUBLISH COMMAND ---
    if (strcmp(argv[1], "publish") == 0) {
        return publish_package();
    }

    // Not a recognized command, fallback to default behavior
    return -1;
}
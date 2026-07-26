#include "commands.h"
#include "execute.h"
#include "platform.h"
#include "build.h"
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

#if defined(__clang__)
#define COMPILER_INFO "Clang " __clang_version__
#elif defined(__GNUC__)
#define COMPILER_INFO "GCC " __VERSION__
#elif defined(_MSC_VER)
#define COMPILER_INFO "MSVC"
#else
#define COMPILER_INFO "Unknown Compiler"
#endif

// dispatches cli commands like 'version' and 'build'
int handle_commands(int argc, char** argv) {
    if (argc < 2) return -1;                                                    // need at least one argument

    if (strcmp(argv[1], "version") == 0) {                                      // version command
        printf("Apex 26.07 [%s] on %s\n", COMPILER_INFO, platform_get_name());  // print version info
        return 0;                                                               // success
    }

    if (strcmp(argv[1], "build") == 0) {   // build command
        return build_command(argc, argv);  // delegate to build handler
    }

    return -1;                             // unknown command
}
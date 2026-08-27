// source/utils/commands.c
// Implementation of Commands for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "commands.h"
#include "execute.h"
#include "platform.h"
#include "build.h"
#include "compile.h"
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
#define COMPILER_NAME "Clang"
#define COMPILER_VERSION_STRING __clang_version__
#elif defined(__GNUC__)
#define COMPILER_NAME "GCC"
#define COMPILER_VERSION_STRING __VERSION__
#else
#define COMPILER_NAME "Unknown Compiler"
#define COMPILER_VERSION_STRING ""
#endif

// extracts major.minor.patch from compiler version string
static void get_compiler_version(char* buffer, size_t size) {
    const char* ver = COMPILER_VERSION_STRING;      // get version string from compiler
    size_t i = 0;                                   // index into output buffer
    
    while (*ver && !isdigit(*ver)) {                // find first digit in string
        ver++;                                      // advance past non-digits
    }
    
    while (*ver && *ver != ' ' && i < size - 1) {   // copy version until space
        buffer[i++] = *ver++;                       // store char and advance
    }
    buffer[i] = '\0';                               // null terminate the string
}

// dispatches cli commands like 'version', 'build', and 'compile'
int handle_commands(int argc, char** argv) {
    if (argc < 2) return -1;                                                    // need at least one argument

    if (strcmp(argv[1], "version") == 0) {                                      // version command
        char compiler_ver[64] = {0};                                            // buffer for clean version
        get_compiler_version(compiler_ver, sizeof(compiler_ver));               // extract major.minor.patch
        
        printf("Apex 26.08 [%s %s] on %s\n", COMPILER_NAME, compiler_ver, platform_get_name());  // print version info
        return 0;                                                               // success
    }

    if (strcmp(argv[1], "build") == 0) {   // build command
        return build_command(argc, argv);  // delegate to build handler
    }

    if (strcmp(argv[1], "compile") == 0) {  // compile command
        return compile_command(argc, argv); // delegate to compile handler
    }

    return -1;                              // unknown command
}
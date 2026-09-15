// source/utils/commands.c
// Implementation of Commands for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "commands.h"
#include "execute.h"
#include "platform.h"
#include "build.h"
#include "compile.h"
#include "emit.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

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

// returns the target architecture as a short lowercase string
static const char* get_arch_string(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86-64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

// dispatches cli commands like 'version', 'build', and 'compile'
int handle_commands(int argc, char** argv) {
    if (argc < 2) return -1;                                                    // need at least one argument

    if (strcmp(argv[1], "version") == 0) {
        char compiler_ver[64] = {0};
        get_compiler_version(compiler_ver, sizeof(compiler_ver));

#if APEX_JIT_ENABLED
        printf("Apex 26.09 JIT [%s %s] on %s %s\n",
            COMPILER_NAME, compiler_ver,
            platform_get_name(), get_arch_string());
#else
        printf("Apex 26.09 [%s %s] on %s %s\n",
            COMPILER_NAME, compiler_ver,
            platform_get_name(), get_arch_string());
#endif
        return 0;
    }

    if (strcmp(argv[1], "build") == 0) {   // build command
        return build_command(argc, argv);  // delegate to build handler
    }

    if (strcmp(argv[1], "compile") == 0) {  // compile command
        return compile_command(argc, argv); // delegate to compile handler
    }

    if (strcmp(argv[1], "emit") == 0) {     // emit (disassemble) command
        return emit_command(argc, argv);    // delegate to emit handler
    }

#if APEX_JIT_ENABLED
    if (strcmp(argv[1], "jit") == 0) {                                          // jit command handler
        if (argc < 4) {                                                         // need <on|off> <filename>
            fprintf(stderr,
                "\033[31mError: Missing arguments.\n"
                "Usage: apex jit <on|off> <filename>\033[0m\n");
            return 1;                                                           // missing args error
        }

        bool want_on;                                                           // parsed jit mode
        if (strcmp(argv[2], "off") == 0) {                                      // "off" -> disable jit
            want_on = false;
        } else if (strcmp(argv[2], "on") == 0) {                                // "on" -> enable jit
            want_on = true;
        } else {                                                                // unknown mode string
            fprintf(stderr,
                "\033[31mError: Invalid jit mode '%s'. Use 'on' or 'off'.\033[0m\n",
                argv[2]);
            return 1;                                                           // invalid mode error
        }

        apex_jit_runtime_enabled = want_on;                                     // set global jit toggle

        const char* filename = argv[3];                                         // script to run

        FILE* f_check = fopen(filename, "rb");                                  // verify file exists
        if (!f_check) {                                                         // open failed
            fprintf(stderr,
                "\033[31mError: Source file '%s' does not exist.\033[0m\n",
                filename);
            return 1;                                                           // missing file error
        }
        fclose(f_check);                                                        // close check handle

        bool ok = execute_source(filename, filename, argc - 3, argv + 3, true); // run with user args
        return ok ? 0 : 1;                                                      // propagate exit code
    }
#else
    if (strcmp(argv[1], "jit") == 0) {
        fprintf(stderr,
            "\033[31mError: This build of Apex was compiled without JIT support.\n"
            "       The 'apex jit' command is unavailable.\033[0m\n");
        return 1;
    }
#endif

    return -1;                              // unknown command
}
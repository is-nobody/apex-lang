#include "commands.h"
#include "execute.h"
#include "platform.h"
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
        if (argc < 3) {
            print_error("Missing filename for 'build' command.");
            return 1;
        }

        const char* filename = argv[2];

        char default_output[4096];
        strncpy(default_output, filename, sizeof(default_output) - 1);
        default_output[sizeof(default_output) - 1] = '\0';

        char* dot = strrchr(default_output, '.');
        if (dot != NULL) {
            *dot = '\0';
        }

        const char* output_name = (argc >= 4) ? argv[3] : default_output;

        printf("\033[32mBuilding %s...\033[0m\n", filename);

        // 1. Read the source file
        FILE* src = fopen(filename, "rb");
        if (!src) {
            print_error("Cannot open source file '%s'", filename);
            return 1;
        }
        fseek(src, 0, SEEK_END);
        long src_size = ftell(src);
        fseek(src, 0, SEEK_SET);
        char* src_code = (char*)malloc(src_size);
        if (!src_code) {
            fclose(src);
            print_error("Memory allocation failed");
            return 1;
        }
        fread(src_code, 1, src_size, src);
        fclose(src);

        // 2. Determine path to self (the interpreter)
        char self_path[4096];
#ifdef _WIN32
        GetModuleFileNameA(NULL, self_path, sizeof(self_path));
#else
        ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
        if (len == -1) {
            char* resolved = realpath(argv[0], self_path);
            if (!resolved) {
                print_error("Cannot resolve executable path");
                free(src_code);
                return 1;
            }
        } else {
            self_path[len] = '\0';
        }
#endif

        // 3. Read self
        FILE* self = fopen(self_path, "rb");
        if (!self) {
            print_error("Cannot open self for reading");
            free(src_code);
            return 1;
        }
        fseek(self, 0, SEEK_END);
        long self_size = ftell(self);
        fseek(self, 0, SEEK_SET);
        char* self_code = (char*)malloc(self_size);
        if (!self_code) {
            fclose(self);
            free(src_code);
            print_error("Memory allocation failed");
            return 1;
        }
        fread(self_code, 1, self_size, self);
        fclose(self);

        // 4. Determine final output name
        char final_output[4096];
        strcpy(final_output, output_name);
#ifdef _WIN32
        size_t out_len = strlen(final_output);
        bool has_exe = false;
        if (out_len >= 4) {
            const char* ext = final_output + out_len - 4;
            if (tolower(ext[0]) == '.' && tolower(ext[1]) == 'e' && 
                tolower(ext[2]) == 'x' && tolower(ext[3]) == 'e') {
                has_exe = true;
            }
        }
        if (!has_exe) {
            strcat(final_output, ".exe");
        }
#endif

        // 5. Create the output binary
        FILE* out = fopen(final_output, "wb");
        if (!out) {
            print_error("Cannot create output file '%s'", final_output);
            free(src_code);
            free(self_code);
            return 1;
        }

        fwrite(self_code, 1, self_size, out);           // Write interpreter
        fwrite(src_code, 1, src_size, out);             // Write payload FIRST
        
        uint32_t size32 = (uint32_t)src_size;
        fwrite(&size32, 4, 1, out);                     // Write payload size AFTER payload
        
        const char* marker = "__APEX_BIN_PAYLOAD__";
        fwrite(marker, 1, 20, out);                     // Write magic marker LAST

        fclose(out);
        free(src_code);
        free(self_code);

#ifndef _WIN32
        chmod(final_output, 0755); // Make executable on POSIX
#endif

        printf("\033[32mBuilding %s completed! Output: %s\033[0m\n", filename, final_output);
        return 0;
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
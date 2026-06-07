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

#define MARKER "__APEX_BIN_PAYLOAD__"

int build_command(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "\033[31mError: Missing filename for 'build' command.\033[0m\n");
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
        fprintf(stderr, "\033[31mError: Cannot open source file '%s'\033[0m\n", filename);
        return 1;
    }
    fseek(src, 0, SEEK_END);
    long src_size = ftell(src);
    fseek(src, 0, SEEK_SET);
    
    char* src_code = (char*)malloc(src_size);
    if (!src_code) {
        fclose(src);
        fprintf(stderr, "\033[31mError: Memory allocation failed\033[0m\n");
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
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
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
        fprintf(stderr, "\033[31mError: Cannot open self for reading\033[0m\n");
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
        fprintf(stderr, "\033[31mError: Memory allocation failed\033[0m\n");
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
        if (tolower((unsigned char)ext[0]) == '.' && tolower((unsigned char)ext[1]) == 'e' &&
            tolower((unsigned char)ext[2]) == 'x' && tolower((unsigned char)ext[3]) == 'e') {
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
        fprintf(stderr, "\033[31mError: Cannot create output file '%s'\033[0m\n", final_output);
        free(src_code);
        free(self_code);
        return 1;
    }
    
    fwrite(self_code, 1, self_size, out);           // Write interpreter
    fwrite(src_code, 1, src_size, out);             // Write payload FIRST
    uint32_t size32 = (uint32_t)src_size;
    fwrite(&size32, 4, 1, out);                     // Write payload size AFTER payload
    fwrite(MARKER, 1, 20, out);                     // Write magic marker LAST
    fclose(out);
    
    free(src_code);
    free(self_code);

#ifndef _WIN32
    chmod(final_output, 0755); // Make executable on POSIX
#endif

    printf("\033[32mBuilding %s completed! Output: %s\033[0m\n", filename, final_output);
    return 0;
}
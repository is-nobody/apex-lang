// source/utils/build.c
// Implementation of Build System for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "build.h"
#include "compile.h"
#include "loader.h"
#include "bytecode.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)  // windows mkdir wrapper
#define F_OK 0                          // file existence flag for access()
#define access _access                  // windows access wrapper
#else
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define MARKER "__APEX_BIN_PAYLOAD__"  // marker to locate payload in binary

// platform descriptor for target builds
typedef struct {
    const char* os;    // operating system name
    const char* arch;  // architecture name
} PlatformInfo;

// detects the current platform at runtime
static PlatformInfo get_current_platform(void) {
    PlatformInfo info = {"unknown", "unknown"};  // default unknown
#if defined(_WIN32)
    info.os = "windows";                         // windows platform
#elif defined(__APPLE__)
    info.os = "macos";                           // macos platform
#elif defined(__linux__)
    info.os = "linux";                           // linux platform
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    info.arch = "x86-64";                        // x86-64 architecture
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
    info.arch = "arm64";                         // arm64 architecture
#endif
    return info;                                 // return detected platform
}

// builds the stub filename for the target platform
static void get_stub_filename(const char* os, const char* arch, char* out_buf, size_t buf_size) {
    snprintf(out_buf, buf_size, "apex_26.09_%s_%s", arch, os);      // format stub name
    if (strcmp(os, "windows") == 0) {                               // check if windows
        strcat(out_buf, ".exe");                                    // add .exe extension
    }
}

// reads a file into memory and returns its size
static char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");                                   // open file in binary mode
    if (!f) return NULL;                                           // file not found
    fseek(f, 0, SEEK_END);                                         // seek to end
    *out_size = ftell(f);                                          // get file size
    fseek(f, 0, SEEK_SET);                                         // seek to start
    char* buf = (char*)malloc(*out_size);                          // allocate buffer
    if (buf) {                                                     // allocation succeeded
        size_t read_count = fread(buf, 1, *out_size, f);           // read file content
        *out_size = (int)read_count;                               // update size with actual read
    }
    fclose(f);                                                     // close file
    return buf;                                                    // return buffer
}

// writes a uint32_t in little-endian order
static void write_u32(FILE* f, uint32_t value) {
    uint8_t buf[4];                                                // byte buffer
    buf[0] = value & 0xFF;                                         // least significant byte
    buf[1] = (value >> 8) & 0xFF;                                  // second byte
    buf[2] = (value >> 16) & 0xFF;                                 // third byte
    buf[3] = (value >> 24) & 0xFF;                                 // most significant byte
    fwrite(buf, 1, 4, f);                                          // write all 4 bytes
}

// main build command that compiles source to bytecode and bundles into executable
int build_command(int argc, char** argv) {
    if (argc < 3) {                                                // check minimum arguments
        fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <filename.apex>\n       apex build <os> <arch> <filename.apex>\033[0m\n");
        return 1;                                                  // error
    }

    const char* target_os = NULL;                                  // target os
    const char* target_arch = NULL;                                // target architecture
    const char* filename = NULL;                                   // source filename

    if (strcmp(argv[2], "windows") == 0 || strcmp(argv[2], "linux") == 0 || strcmp(argv[2], "macos") == 0) {
        if (argc < 5) {                                            // os/arch mode needs 5 args
            fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <os> <arch> <filename.apex>\033[0m\n");
            return 1;                                              // error
        }
        target_os = argv[2];                                       // set target os
        target_arch = argv[3];                                     // set target arch
        filename = argv[4];                                        // set filename
    } else {
        filename = argv[2];                                        // simple mode, filename only
    }

    FILE* f_check = fopen(filename, "rb");                         // check if source exists
    if (!f_check) {                                                // file not found
        fprintf(stderr, "\033[31mError: Source file '%s' does not exist.\033[0m\n", filename);
        return 1;                                                  // error
    }
    fclose(f_check);                                               // close file

    PlatformInfo current = get_current_platform();                 // get current platform
    PlatformInfo target;                                           // target platform
    
    if (target_os) {                                               // target specified
        target.os = target_os;                                     // use specified os
        if (strcmp(target_arch, "x86-64") != 0 && strcmp(target_arch, "arm64") != 0) {  // validate arch
            fprintf(stderr, "\033[31mError: Invalid architecture '%s'. Use 'x86-64' or 'arm64'.\033[0m\n", target_arch);
            return 1;                                              // error
        }
        target.arch = target_arch;                                 // use specified arch
    } else {
        target = current;                                          // use current platform
    }

    printf("\033[36mBuilding for: %s %s\033[0m\n", target.os, target.arch);  // print target info
    printf("\033[32mCompiling %s to bytecode...\033[0m\n", filename);        // print compile start

    // Step 1: Generate bytecode file using compile_command
    char bytecode_path[4096];                                      // bytecode file path
    strncpy(bytecode_path, filename, sizeof(bytecode_path) - 1);   // copy filename
    bytecode_path[sizeof(bytecode_path) - 1] = '\0';               // null terminate
    char* dot = strrchr(bytecode_path, '.');                       // find last dot
    if (dot) *dot = '\0';                                          // strip extension
    strcat(bytecode_path, ".apexc");                               // add .apexc extension

    char* compile_argv[] = { argv[0], "compile", (char*)filename };  // compile command args
    int compile_result = compile_command(3, compile_argv);           // invoke compiler
    
    if (compile_result != 0) {                                     // compilation failed
        fprintf(stderr, "\033[31mError: Compilation failed for '%s'\033[0m\n", filename);
        return 1;                                                  // error
    }

    if (access(bytecode_path, F_OK) != 0) {                        // verify bytecode was created
        fprintf(stderr, "\033[31mError: Bytecode file '%s' was not created.\033[0m\n", bytecode_path);
        return 1;                                                  // error
    }

    // Step 2: Read the generated bytecode file
    long bytecode_size;                                                  // bytecode file size
    char* bytecode_data = read_file(bytecode_path, &bytecode_size);      // read bytecode
    if (!bytecode_data) {                                                // read failed
        fprintf(stderr, "\033[31mError: Cannot read bytecode file '%s'.\033[0m\n", bytecode_path);
        return 1;                                                        // error
    }

    printf("\033[36mBytecode size: %ld bytes\033[0m\n", bytecode_size);  // print bytecode size

    // Step 3: Prepare output executable name
    char base_name[4096];                                          // base name buffer
    strncpy(base_name, filename, sizeof(base_name) - 1);           // copy filename
    base_name[sizeof(base_name) - 1] = '\0';                       // null terminate
    char* dot2 = strrchr(base_name, '.');                          // find last dot
    if (dot2) *dot2 = '\0';                                        // strip extension

    char final_output[4096];                                       // output filename
    snprintf(final_output, sizeof(final_output), "%s_%s_%s", base_name, target.arch, target.os);  // format output
    if (strcmp(target.os, "windows") == 0) {                       // check if windows
        strcat(final_output, ".exe");                              // add .exe extension
    }

    // Step 4: Read stub executable
    char source_to_read[4096];                                     // source stub path
    long self_size;                                                // stub size
    char* self_code;                                               // stub content

    if (target_os) {                                               // explicit os/arch
        char stub_filename[256];                                   // stub filename buffer
        get_stub_filename(target.os, target.arch, stub_filename, sizeof(stub_filename));  // build stub name

        char self_path[4096];                                      // executable path buffer
#ifdef _WIN32
        GetModuleFileNameA(NULL, self_path, sizeof(self_path));    // get exe path on windows
#else
        ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);  // read symlink on linux
        if (len == -1) {                                           // readlink failed
            if (!realpath(argv[0], self_path)) {                   // try realpath
                fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
                free(bytecode_data);                               // free bytecode
                return 1;                                          // error
            }
        } else {
            self_path[len] = '\0';                                 // null terminate
        }
#endif

        char exe_dir[4096];                                            // executable directory
        strncpy(exe_dir, self_path, sizeof(exe_dir));                  // copy exe path
        char* last_slash = strrchr(exe_dir, '/');                      // find last forward slash
        char* last_backslash = strrchr(exe_dir, '\\');                 // find last backslash
        if (last_backslash > last_slash) last_slash = last_backslash;  // use whichever is later
        if (last_slash) *last_slash = '\0';                            // strip filename
        else strcpy(exe_dir, ".");                                     // fallback to current dir

        snprintf(source_to_read, sizeof(source_to_read), "%s/%s", exe_dir, stub_filename);  // build stub path
        self_code = read_file(source_to_read, &self_size);         // read stub
        if (!self_code) {                                          // read failed
            fprintf(stderr, "\033[31mError: Cannot read stub '%s'. Ensure it is compiled and placed next to the apex binary.\033[0m\n", source_to_read);
            free(bytecode_data);                                   // free bytecode
            return 1;                                              // error
        }
    } else {                                                       // no explicit os/arch - use current executable as stub
#ifdef _WIN32
        if (GetModuleFileNameA(NULL, source_to_read, sizeof(source_to_read)) == 0) {  // get exe path
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            free(bytecode_data);                                   // free bytecode
            return 1;                                              // error
        }
#elif __linux__
        ssize_t len = readlink("/proc/self/exe", source_to_read, sizeof(source_to_read) - 1);  // read symlink
        if (len == -1) {                                           // readlink failed
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            free(bytecode_data);                                   // free bytecode
            return 1;                                              // error
        }
        source_to_read[len] = '\0';                                // null terminate
#elif __APPLE__
        uint32_t size = sizeof(source_to_read);                    // buffer size
        if (_NSGetExecutablePath(source_to_read, &size) != 0) {    // get exec path on mac
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            free(bytecode_data);                                   // free bytecode
            return 1;                                              // error
        }
#else
        fprintf(stderr, "\033[31mError: Unsupported platform\033[0m\n");
        free(bytecode_data);                                       // free bytecode
        return 1;                                                  // error
#endif

        self_code = read_file(source_to_read, &self_size);         // read current executable
        if (!self_code) {                                          // read failed
            fprintf(stderr, "\033[31mError: Cannot read current executable '%s'.\033[0m\n", source_to_read);
            free(bytecode_data);                                   // free bytecode
            return 1;                                              // error
        }
    }

    FILE* out = fopen(final_output, "wb");                         // open output file
    if (!out) {                                                    // open failed
        fprintf(stderr, "\033[31mError: Cannot create output file '%s'\033[0m\n", final_output);
        free(self_code);                                           // free stub
        free(bytecode_data);                                       // free bytecode
        return 1;                                                  // error
    }

    fwrite(self_code, 1, self_size, out);                          // write stub
    free(self_code);                                               // free stub

    fwrite(bytecode_data, 1, bytecode_size, out);                  // write bytecode
    free(bytecode_data);                                           // free bytecode

    uint32_t payload_size = (uint32_t)bytecode_size;               // payload size
    write_u32(out, payload_size);                                  // write size

    fwrite(MARKER, 1, 20, out);                                    // write marker
    fclose(out);                                                   // close output
    remove(bytecode_path);                                         // delete temporary bytecode file

#ifndef _WIN32
    chmod(final_output, 0755);                                     // make executable on unix
#endif

    printf("\033[32mBuild completed! Output: %s\033[0m\n", final_output);  // print success
    return 0;                                                      // success
}
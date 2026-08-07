// source/utils/build.c
// Implementation of Build System for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "build.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)  // windows mkdir wrapper
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
    info.os = "windows";
#elif defined(__APPLE__)
    info.os = "macos";
#elif defined(__linux__)
    info.os = "linux";
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    info.arch = "x86-64";
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
    info.arch = "arm64";
#endif
    return info;  // return detected platform
}

// builds the stub filename for the target platform
static void get_stub_filename(const char* os, const char* arch, char* out_buf, size_t buf_size) {
    snprintf(out_buf, buf_size, "apex_26.08_%s_%s", arch, os);      // format stub name
    if (strcmp(os, "windows") == 0) {                               // check if windows
        strcat(out_buf, ".exe");                                    // add .exe extension
    }
}

// converts a dotted module path to a filesystem path
static bool resolve_module_path(const char* source_dir, const char* module_path, char* out_path, int out_size) {
    char relative[1024];                                            // relative path buffer
    size_t len = 0;                                                 // current path length
    relative[0] = '\0';                                             // initialize empty
    char path_copy[1024];                                           // copy of module path
    strncpy(path_copy, module_path, sizeof(path_copy) - 1);         // copy module path
    path_copy[sizeof(path_copy) - 1] = '\0';                        // null terminate
    char* segment = strtok(path_copy, ".");                         // split by dots
    while (segment) {                                               // iterate over segments
        if (len > 0) {                                              // not first segment
            if (len + 1 < sizeof(relative)) relative[len++] = '/';  // add directory separator
        }
        size_t seg_len = strlen(segment);                           // segment length
        if (len + seg_len >= sizeof(relative)) return false;        // buffer overflow
        memcpy(relative + len, segment, seg_len);                   // copy segment
        len += seg_len;                                             // update length
        relative[len] = '\0';                                       // null terminate
        segment = strtok(NULL, ".");                                // next segment
    }
    if (len + 5 >= sizeof(relative)) return false;                                  // check space for .apex
    strcat(relative, ".apex");                                                      // add .apex extension
    if (snprintf(out_path, out_size, "%s/%s", source_dir, relative) >= out_size) {  // build full path
        return false;                                                               // buffer overflow
    }
    return true;                                                                    // success
}

// extracts a relative path from a full path
static void get_relative_path(const char* base_dir, const char* full_path, char* out_rel, int out_size) {
    size_t base_len = strlen(base_dir);                            // base directory length
    if (strncmp(full_path, base_dir, base_len) == 0) {             // path starts with base
        const char* rel = full_path + base_len;                    // skip base directory
        while (*rel == '/' || *rel == '\\' || *rel == '.') rel++;  // skip separators and dots
        strncpy(out_rel, rel, out_size - 1);                       // copy relative path
        out_rel[out_size - 1] = '\0';                              // null terminate
    } else {
        const char* slash = strrchr(full_path, '/');               // find forward slash
        const char* bslash = strrchr(full_path, '\\');             // find backslash
        if (bslash > slash) slash = bslash;                        // use whichever is later
        if (slash) strncpy(out_rel, slash + 1, out_size - 1);      // copy filename
        else strncpy(out_rel, full_path, out_size - 1);            // copy full path
        out_rel[out_size - 1] = '\0';                              // null terminate
    }
}

// recursively scans for import statements and collects dependencies
static void scan_imports(const char* source_dir, const char* filepath,
                         char*** out_paths, int* out_count, int* out_cap) {
    FILE* f = fopen(filepath, "rb");                   // open source file
    if (!f) return;                                    // file not found
    fseek(f, 0, SEEK_END);                             // seek to end
    long size = ftell(f);                              // get file size
    fseek(f, 0, SEEK_SET);                             // seek to start
    char* content = (char*)malloc(size + 1);           // allocate content buffer
    if (!content) { fclose(f); return; }               // allocation failed
    if (fread(content, 1, size, f) != (size_t)size) {  // read file content
        free(content);                                 // free buffer
        fclose(f);                                     // close file
        return;                                        // read failed
    }
    content[size] = '\0';                              // null terminate
    fclose(f);                                         // close file

    char* ptr = content;                                                                // scan pointer
    while (*ptr) {                                                                      // iterate through content
        while (*ptr && isspace(*ptr)) ptr++;                                            // skip whitespace
        if (strncmp(ptr, "import ", 7) == 0) {                                          // found import statement
            ptr += 7;                                                                   // skip "import "
            while (*ptr && isspace(*ptr)) ptr++;                                        // skip whitespace
            char module[256];                                                           // module name buffer
            int i = 0;                                                                  // module name index
            while (*ptr && (isalnum(*ptr) || *ptr == '_' || *ptr == '.') && i < 255) {  // parse module
                module[i++] = *ptr++;                                                   // copy character
            }
            module[i] = '\0';                                                           // null terminate
            if (i > 0 && strcmp(module, "os") != 0 && strcmp(module, "math") != 0 &&    // skip built-in modules
                strcmp(module, "string") != 0 && strcmp(module, "table") != 0 &&
                strcmp(module, "sys") != 0 && strcmp(module, "ffi") != 0 &&
                strcmp(module, "random") != 0 && strcmp(module, "codecs") != 0) {
                char resolved[4096];                                                        // resolved path buffer
                if (resolve_module_path(source_dir, module, resolved, sizeof(resolved))) {  // resolve path
                    bool found = false;                                                     // duplicate check flag
                    for (int j = 0; j < *out_count; j++) {                                  // iterate over collected deps
                        if (strcmp((*out_paths)[j], resolved) == 0) {                       // compare paths
                            found = true;                                                   // already collected
                            break;                                                          // exit loop
                        }
                    }
                    if (!found) {                                                                  // new dependency
                        if (*out_count >= *out_cap) {                                              // need more capacity
                            *out_cap = (*out_cap == 0) ? 16 : (*out_cap * 2);                      // double capacity
                            *out_paths = (char**)realloc(*out_paths, sizeof(char*) * (*out_cap));  // reallocate
                        }
                        (*out_paths)[*out_count] = strdup(resolved);                        // duplicate path
                        (*out_count)++;                                                     // increment count
                        scan_imports(source_dir, resolved, out_paths, out_count, out_cap);  // scan dependency
                    }
                }
            }
        }
        while (*ptr && *ptr != '\n') ptr++;                            // skip to end of line
        if (*ptr == '\n') ptr++;                                       // skip newline
    }
    free(content);                                                     // free content buffer
}

// reads a file into memory and returns its size
static char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");                                       // open file in binary mode
    if (!f) return NULL;                                               // file not found
    fseek(f, 0, SEEK_END);                                             // seek to end
    *out_size = ftell(f);                                              // get file size
    fseek(f, 0, SEEK_SET);                                             // seek to start
    char* buf = (char*)malloc(*out_size);                              // allocate buffer
    if (buf) {                                                         // allocation succeeded
        size_t read_count = fread(buf, 1, *out_size, f);               // read file content
        *out_size = (int)read_count;                                   // update size with actual read
    }
    fclose(f);                                                         // close file
    return buf;                                                        // return buffer
}

// main build command that bundles source files into a standalone executable
int build_command(int argc, char** argv) {
    if (argc < 3) {                                                    // check minimum arguments
        fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <filename.apex>\n       apex build <os> <arch> <filename.apex>\033[0m\n");
        return 1;                                                      // error
    }

    const char* target_os = NULL;                                      // target os
    const char* target_arch = NULL;                                    // target architecture
    const char* filename = NULL;                                       // source filename

    if (strcmp(argv[2], "windows") == 0 || strcmp(argv[2], "linux") == 0 || strcmp(argv[2], "macos") == 0) {
        if (argc < 5) {                                                // os/arch mode needs 5 args
            fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <os> <arch> <filename.apex>\033[0m\n");
            return 1;                                                  // error
        }
        target_os = argv[2];                                           // set target os
        target_arch = argv[3];                                         // set target arch
        filename = argv[4];                                            // set filename
    } else {
        filename = argv[2];                                            // simple mode, filename only
    }

    FILE* f_check = fopen(filename, "rb");                             // check if source exists
    if (!f_check) {                                                    // file not found
        fprintf(stderr, "\033[31mError: Source file '%s' does not exist.\033[0m\n", filename);
        return 1;                                                            // error
    }
    fclose(f_check);                                                         // close file

    PlatformInfo current = get_current_platform();                           // get current platform
    PlatformInfo target;                                                     // target platform
    
    if (target_os) {                                                                    // target specified
        target.os = target_os;                                                          // use specified os
        if (strcmp(target_arch, "x86-64") != 0 && strcmp(target_arch, "arm64") != 0) {  // validate arch
            fprintf(stderr, "\033[31mError: Invalid architecture '%s'. Use 'x86-64' or 'arm64'.\033[0m\n", target_arch);
            return 1;                                                        // error
        }
        target.arch = target_arch;                                           // use specified arch
    } else {
        target = current;                                                    // use current platform
    }

    printf("\033[36mBuilding for: %s %s\033[0m\n", target.os, target.arch);  // print target info
    printf("\033[32mBuilding %s...\033[0m\n", filename);                     // print build start

    char base_name[4096];                                              // base name buffer
    strncpy(base_name, filename, sizeof(base_name) - 1);               // copy filename
    base_name[sizeof(base_name) - 1] = '\0';                           // null terminate
    char* dot = strrchr(base_name, '.');                               // find last dot
    if (dot != NULL) *dot = '\0';                                      // strip extension

    char final_output[4096];                                           // output filename
    snprintf(final_output, sizeof(final_output), "%s_%s_%s", base_name, target.arch, target.os);  // format output
    if (strcmp(target.os, "windows") == 0) {                           // check if windows
        strcat(final_output, ".exe");                                  // add .exe extension
    }

    char source_to_read[4096];                                         // source stub path
    long self_size;                                                    // stub size
    char* self_code;                                                   // stub content

    if (target_os) {
        // explicit os/arch — find stub by name next to the binary (old behavior)
        char stub_filename[256];                                       // stub filename buffer
        get_stub_filename(target.os, target.arch, stub_filename, sizeof(stub_filename));  // build stub name

        char self_path[4096];                                          // executable path buffer
#ifdef _WIN32
        GetModuleFileNameA(NULL, self_path, sizeof(self_path));        // get exe path on windows
#else
        ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);  // read symlink on linux
        if (len == -1) {                                               // readlink failed
            if (!realpath(argv[0], self_path)) {                       // try realpath
                fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
                return 1;                                              // error
            }
        } else {
            self_path[len] = '\0';                                     // null terminate
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

        self_code = read_file(source_to_read, &self_size);       // read stub
        if (!self_code) {                                        // read failed
            fprintf(stderr, "\033[31mError: Cannot read stub '%s'. Ensure it is compiled and placed next to the apex binary.\033[0m\n", source_to_read);
            return 1;                                            // error
        }
    } else {
        // no explicit os/arch — use current executable as stub
#ifdef _WIN32
        if (GetModuleFileNameA(NULL, source_to_read, sizeof(source_to_read)) == 0) { // get exe path
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            return 1;                                            // error
        }
#elif __linux__
        ssize_t len = readlink("/proc/self/exe", source_to_read, sizeof(source_to_read) - 1);  // read symlink
        if (len == -1) {                                         // readlink failed
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            return 1;                                            // error
        }
        source_to_read[len] = '\0';                              // null terminate
#elif __APPLE__
        uint32_t size = sizeof(source_to_read);                  // buffer size
        if (_NSGetExecutablePath(source_to_read, &size) != 0) {  // get exec path on mac
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            return 1;                                            // error
        }
#else
        fprintf(stderr, "\033[31mError: Unsupported platform\033[0m\n");
        return 1;                                                // error
#endif

        self_code = read_file(source_to_read, &self_size);       // read current executable
        if (!self_code) {                                        // read failed
            fprintf(stderr, "\033[31mError: Cannot read current executable '%s'.\033[0m\n", source_to_read);
            return 1;                                            // error
        }
    }

    char source_dir[4096];                                                         // source directory
    strncpy(source_dir, filename, sizeof(source_dir) - 1);                         // copy filename
    source_dir[sizeof(source_dir) - 1] = '\0';                                     // null terminate
    char* last_slash_src = strrchr(source_dir, '/');                               // find last forward slash
    char* last_backslash_src = strrchr(source_dir, '\\');                          // find last backslash
    if (last_backslash_src > last_slash_src) last_slash_src = last_backslash_src;  // use whichever is later
    if (last_slash_src) *last_slash_src = '\0';                                    // strip filename
    else strcpy(source_dir, ".");                                                  // fallback to current dir

    char** dependencies = NULL;                                               // dependency list
    int dep_count = 0;                                                        // dependency count
    int dep_cap = 16;                                                         // dependency capacity
    dependencies = (char**)malloc(sizeof(char*) * dep_cap);                   // allocate dependency array
    dependencies[0] = strdup(filename);                                       // add main file
    dep_count = 1;                                                            // set count
    scan_imports(source_dir, filename, &dependencies, &dep_count, &dep_cap);  // scan for imports

    printf("\033[36mFound %d file(s) to bundle:\033[0m\n", dep_count);        // print file count
    for (int i = 0; i < dep_count; i++) printf("  - %s\n", dependencies[i]);  // print each file

    FILE* out = fopen(final_output, "wb");                             // open output file
    if (!out) {                                                        // open failed
        fprintf(stderr, "\033[31mError: Cannot create output file '%s'\033[0m\n", final_output);
        free(self_code);                                               // free stub
        for(int i = 0; i < dep_count; i++) free(dependencies[i]);      // free dependencies
        free(dependencies);                                            // free dependency array
        return 1;                                                      // error
    }

    fwrite(self_code, 1, self_size, out);                              // write stub
    free(self_code);                                                   // free stub

    uint32_t num_files = (uint32_t)dep_count;                          // number of files
    fwrite(&num_files, 4, 1, out);                                     // write file count
    long total_payload_size = 4;                                       // payload size counter

    for (int i = 0; i < dep_count; i++) {                              // iterate over dependencies
        long file_size;                                                // file size
        char* file_content = read_file(dependencies[i], &file_size);   // read file content
        if (!file_content) {                                           // read failed
            fprintf(stderr, "\033[31mError: Cannot read dependency '%s'\033[0m\n", dependencies[i]);
            fclose(out);                                               // close output
            for(int j = 0; j < dep_count; j++) free(dependencies[j]);  // free dependencies
            free(dependencies);                                        // free dependency array
            return 1;                                                  // error
        }

        char rel_path[4096];                                                         // relative path buffer
        get_relative_path(source_dir, dependencies[i], rel_path, sizeof(rel_path));  // get relative path
        for (char* p = rel_path; *p; p++) if (*p == '\\') *p = '/';                  // normalize path separators

        uint32_t name_len = (uint32_t)strlen(rel_path);        // name length
        uint32_t content_len = (uint32_t)file_size;            // content length
        fwrite(&name_len, 4, 1, out);                          // write name length
        fwrite(rel_path, 1, name_len, out);                    // write name
        fwrite(&content_len, 4, 1, out);                       // write content length
        fwrite(file_content, 1, content_len, out);             // write content
        total_payload_size += 4 + name_len + 4 + content_len;  // update payload size
        free(file_content);                                    // free file content
    }

    uint32_t size32 = (uint32_t)total_payload_size;            // payload size
    fwrite(&size32, 4, 1, out);                                // write payload size
    fwrite(MARKER, 1, 20, out);                                // write marker
    fclose(out);                                               // close output

    for(int i = 0; i < dep_count; i++) free(dependencies[i]);  // free dependencies
    free(dependencies);                                        // free dependency array

#ifndef _WIN32
    chmod(final_output, 0755);  // make executable on unix
#endif

    printf("\033[32mBuilding %s completed! Output: %s\033[0m\n", filename, final_output); // print success
    return 0;  // success
}
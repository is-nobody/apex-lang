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
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define MARKER "__APEX_BIN_PAYLOAD__"

// platform descriptor for target builds
typedef struct {
    const char* os;
    const char* arch;
} PlatformInfo;

// detects the current platform at runtime
static PlatformInfo get_current_platform(void) {
    PlatformInfo info = {"unknown", "unknown"};
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
    return info;
}

// builds the stub filename for the target platform
static void get_stub_filename(const char* os, const char* arch, char* out_buf, size_t buf_size) {
    snprintf(out_buf, buf_size, "apex_26.07_%s_%s", arch, os);
    if (strcmp(os, "windows") == 0) {
        strcat(out_buf, ".exe");
    }
}

// converts a dotted module path to a filesystem path
static bool resolve_module_path(const char* source_dir, const char* module_path, char* out_path, int out_size) {
    char relative[1024];
    size_t len = 0;
    relative[0] = '\0';
    char path_copy[1024];
    strncpy(path_copy, module_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    char* segment = strtok(path_copy, ".");
    while (segment) {
        if (len > 0) {
            if (len + 1 < sizeof(relative)) relative[len++] = '/';
        }
        size_t seg_len = strlen(segment);
        if (len + seg_len >= sizeof(relative)) return false;
        memcpy(relative + len, segment, seg_len);
        len += seg_len;
        relative[len] = '\0';
        segment = strtok(NULL, ".");
    }
    if (len + 5 >= sizeof(relative)) return false;
    strcat(relative, ".apex");
    if (snprintf(out_path, out_size, "%s/%s", source_dir, relative) >= out_size) {
        return false;
    }
    return true;
}

// extracts a relative path from a full path
static void get_relative_path(const char* base_dir, const char* full_path, char* out_rel, int out_size) {
    size_t base_len = strlen(base_dir);
    if (strncmp(full_path, base_dir, base_len) == 0) {
        const char* rel = full_path + base_len;
        while (*rel == '/' || *rel == '\\' || *rel == '.') rel++;
        strncpy(out_rel, rel, out_size - 1);
        out_rel[out_size - 1] = '\0';
    } else {
        const char* slash = strrchr(full_path, '/');
        const char* bslash = strrchr(full_path, '\\');
        if (bslash > slash) slash = bslash;
        if (slash) strncpy(out_rel, slash + 1, out_size - 1);
        else strncpy(out_rel, full_path, out_size - 1);
        out_rel[out_size - 1] = '\0';
    }
}

// recursively scans for import statements and collects dependencies
static void scan_imports(const char* source_dir, const char* filepath,
                         char*** out_paths, int* out_count, int* out_cap) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* content = (char*)malloc(size + 1);
    if (!content) { fclose(f); return; }
    if (fread(content, 1, size, f) != (size_t)size) {
        free(content);
        fclose(f);
        return; 
    }
    content[size] = '\0';
    fclose(f);

    char* ptr = content;
    while (*ptr) {
        while (*ptr && isspace(*ptr)) ptr++;
        if (strncmp(ptr, "import ", 7) == 0) {
            ptr += 7;
            while (*ptr && isspace(*ptr)) ptr++;
            char module[256];
            int i = 0;
            while (*ptr && (isalnum(*ptr) || *ptr == '_' || *ptr == '.') && i < 255) {
                module[i++] = *ptr++;
            }
            module[i] = '\0';
            if (i > 0 && strcmp(module, "os") != 0 && strcmp(module, "math") != 0 &&
                strcmp(module, "string") != 0 && strcmp(module, "table") != 0 &&
                strcmp(module, "sys") != 0 && strcmp(module, "ffi") != 0 &&
                strcmp(module, "random") != 0 && strcmp(module, "codecs") != 0) {
                char resolved[4096];
                if (resolve_module_path(source_dir, module, resolved, sizeof(resolved))) {
                    bool found = false;
                    for (int j = 0; j < *out_count; j++) {
                        if (strcmp((*out_paths)[j], resolved) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        if (*out_count >= *out_cap) {
                            *out_cap = (*out_cap == 0) ? 16 : (*out_cap * 2);
                            *out_paths = (char**)realloc(*out_paths, sizeof(char*) * (*out_cap));
                        }
                        (*out_paths)[*out_count] = strdup(resolved);
                        (*out_count)++;
                        scan_imports(source_dir, resolved, out_paths, out_count, out_cap);
                    }
                }
            }
        }
        while (*ptr && *ptr != '\n') ptr++;
        if (*ptr == '\n') ptr++;
    }
    free(content);
}

// reads a file into memory and returns its size
static char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(*out_size);
    if (buf) {
        size_t read_count = fread(buf, 1, *out_size, f);
        *out_size = (int)read_count;
    }
    fclose(f);
    return buf;
}

// main build command that bundles source files into a standalone executable
int build_command(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <filename.apex>\n       apex build <os> <arch> <filename.apex>\033[0m\n");
        return 1;
    }

    const char* target_os = NULL;
    const char* target_arch = NULL;
    const char* filename = NULL;

    if (strcmp(argv[2], "windows") == 0 || strcmp(argv[2], "linux") == 0 || strcmp(argv[2], "macos") == 0) {
        if (argc < 5) {
            fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <os> <arch> <filename.apex>\033[0m\n");
            return 1;
        }
        target_os = argv[2];
        target_arch = argv[3];
        filename = argv[4];
    } else {
        filename = argv[2];
    }

    FILE* f_check = fopen(filename, "rb");
    if (!f_check) {
        fprintf(stderr, "\033[31mError: Source file '%s' does not exist.\033[0m\n", filename);
        return 1;
    }
    fclose(f_check);

    PlatformInfo current = get_current_platform();
    PlatformInfo target;
    
    if (target_os) {
        target.os = target_os;
        if (strcmp(target_arch, "x86-64") != 0 && strcmp(target_arch, "arm64") != 0) {
            fprintf(stderr, "\033[31mError: Invalid architecture '%s'. Use 'x86-64' or 'arm64'.\033[0m\n", target_arch);
            return 1;
        }
        target.arch = target_arch;
    } else {
        target = current;
    }

    printf("\033[36mBuilding for: %s %s\033[0m\n", target.os, target.arch);
    printf("\033[32mBuilding %s...\033[0m\n", filename);

    char base_name[4096];
    strncpy(base_name, filename, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    char* dot = strrchr(base_name, '.');
    if (dot != NULL) *dot = '\0';

    char final_output[4096];
    snprintf(final_output, sizeof(final_output), "%s_%s_%s", base_name, target.arch, target.os);
    if (strcmp(target.os, "windows") == 0) {
        strcat(final_output, ".exe");
    }

    char source_to_read[4096];
    long self_size;
    char* self_code;

    if (target_os) {
        // explicit os/arch — find stub by name next to the binary (old behavior)
        char stub_filename[256];
        get_stub_filename(target.os, target.arch, stub_filename, sizeof(stub_filename));

        char self_path[4096];
#ifdef _WIN32
        GetModuleFileNameA(NULL, self_path, sizeof(self_path));
#else
        ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
        if (len == -1) {
            if (!realpath(argv[0], self_path)) {
                fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
                return 1;
            }
        } else {
            self_path[len] = '\0';
        }
#endif

        char exe_dir[4096];
        strncpy(exe_dir, self_path, sizeof(exe_dir));
        char* last_slash = strrchr(exe_dir, '/');
        char* last_backslash = strrchr(exe_dir, '\\');
        if (last_backslash > last_slash) last_slash = last_backslash;
        if (last_slash) *last_slash = '\0';
        else strcpy(exe_dir, ".");

        snprintf(source_to_read, sizeof(source_to_read), "%s/%s", exe_dir, stub_filename);

        self_code = read_file(source_to_read, &self_size);
        if (!self_code) {
            fprintf(stderr, "\033[31mError: Cannot read stub '%s'. Ensure it is compiled and placed next to the apex binary.\033[0m\n", source_to_read);
            return 1;
        }
    } else {
        // no explicit os/arch — use current executable as stub
#ifdef _WIN32
        if (GetModuleFileNameA(NULL, source_to_read, sizeof(source_to_read)) == 0) {
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            return 1;
        }
#elif __linux__
        ssize_t len = readlink("/proc/self/exe", source_to_read, sizeof(source_to_read) - 1);
        if (len == -1) {
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            return 1;
        }
        source_to_read[len] = '\0';
#elif __APPLE__
        uint32_t size = sizeof(source_to_read);
        if (_NSGetExecutablePath(source_to_read, &size) != 0) {
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            return 1;
        }
#else
        fprintf(stderr, "\033[31mError: Unsupported platform\033[0m\n");
        return 1;
#endif

        self_code = read_file(source_to_read, &self_size);
        if (!self_code) {
            fprintf(stderr, "\033[31mError: Cannot read current executable '%s'.\033[0m\n", source_to_read);
            return 1;
        }
    }

    char source_dir[4096];
    strncpy(source_dir, filename, sizeof(source_dir) - 1);
    source_dir[sizeof(source_dir) - 1] = '\0';
    char* last_slash_src = strrchr(source_dir, '/');
    char* last_backslash_src = strrchr(source_dir, '\\');
    if (last_backslash_src > last_slash_src) last_slash_src = last_backslash_src;
    if (last_slash_src) *last_slash_src = '\0';
    else strcpy(source_dir, ".");

    char** dependencies = NULL;
    int dep_count = 0;
    int dep_cap = 16;
    dependencies = (char**)malloc(sizeof(char*) * dep_cap);
    dependencies[0] = strdup(filename);
    dep_count = 1;
    scan_imports(source_dir, filename, &dependencies, &dep_count, &dep_cap);

    printf("\033[36mFound %d file(s) to bundle:\033[0m\n", dep_count);
    for (int i = 0; i < dep_count; i++) printf("  - %s\n", dependencies[i]);

    FILE* out = fopen(final_output, "wb");
    if (!out) {
        fprintf(stderr, "\033[31mError: Cannot create output file '%s'\033[0m\n", final_output);
        free(self_code);
        for(int i = 0; i < dep_count; i++) free(dependencies[i]);
        free(dependencies);
        return 1;
    }

    fwrite(self_code, 1, self_size, out);
    free(self_code);

    uint32_t num_files = (uint32_t)dep_count;
    fwrite(&num_files, 4, 1, out);
    long total_payload_size = 4;

    for (int i = 0; i < dep_count; i++) {
        long file_size;
        char* file_content = read_file(dependencies[i], &file_size);
        if (!file_content) {
            fprintf(stderr, "\033[31mError: Cannot read dependency '%s'\033[0m\n", dependencies[i]);
            fclose(out);
            for(int j = 0; j < dep_count; j++) free(dependencies[j]);
            free(dependencies);
            return 1;
        }

        char rel_path[4096];
        get_relative_path(source_dir, dependencies[i], rel_path, sizeof(rel_path));
        for (char* p = rel_path; *p; p++) if (*p == '\\') *p = '/';

        uint32_t name_len = (uint32_t)strlen(rel_path);
        uint32_t content_len = (uint32_t)file_size;
        fwrite(&name_len, 4, 1, out);
        fwrite(rel_path, 1, name_len, out);
        fwrite(&content_len, 4, 1, out);
        fwrite(file_content, 1, content_len, out);
        total_payload_size += 4 + name_len + 4 + content_len;
        free(file_content);
    }

    uint32_t size32 = (uint32_t)total_payload_size;
    fwrite(&size32, 4, 1, out);
    fwrite(MARKER, 1, 20, out);
    fclose(out);

    for(int i = 0; i < dep_count; i++) free(dependencies[i]);
    free(dependencies);

#ifndef _WIN32
    chmod(final_output, 0755);
#endif

    printf("\033[32mBuilding %s completed! Output: %s\033[0m\n", filename, final_output);
    return 0;
}
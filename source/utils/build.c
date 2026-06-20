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

typedef struct {
    const char* os;
    const char* arch;
} PlatformInfo;

static PlatformInfo get_current_platform(void) {
    PlatformInfo info = {"unknown", "unknown"};
#if defined(_WIN32)
    info.os = "windows";
#elif defined(__APPLE__)
    info.os = "macos";
#elif defined(__linux__)
    info.os = "linux";
#endif

    // Simplified: map all x86 variants to "x86" and all ARM variants to "arm"
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    info.arch = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
    info.arch = "arm64";
#endif
    return info;
}

static void get_stub_filename(const char* os, const char* arch, char* out_buf, size_t buf_size) {
    snprintf(out_buf, buf_size, "apex_%s_%s", arch, os);
    if (strcmp(os, "windows") == 0) {
        strcat(out_buf, ".exe");
    }
}

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

static void scan_imports(const char* source_dir, const char* filepath,
                         char*** out_paths, int* out_count, int* out_cap) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* content = (char*)malloc(size + 1);
    if (!content) { fclose(f); return; }
    fread(content, 1, size, f);
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
                strcmp(module, "string") != 0 && strcmp(module, "table") != 0) {
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

static char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(*out_size);
    if (buf) fread(buf, 1, *out_size, f);
    fclose(f);
    return buf;
}

int build_command(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <filename.apex>\n       apex build <os> <arch> <filename.apex>\033[0m\n");
        return 1;
    }

    const char* target_os = NULL;
    const char* target_arch = NULL;
    const char* filename = NULL;

    // Smart parsing: check if argv[2] is a known OS
    if (strcmp(argv[2], "windows") == 0 || strcmp(argv[2], "linux") == 0 || strcmp(argv[2], "macos") == 0) {
        if (argc < 5) {
            fprintf(stderr, "\033[31mError: Missing arguments.\nUsage: apex build <os> <arch> <filename.apex>\033[0m\n");
            return 1;
        }
        target_os = argv[2];
        target_arch = argv[3];
        filename = argv[4];
    } else {
        // Native build syntax
        filename = argv[2];
    }

    // Check if source file exists
    FILE* f_check = fopen(filename, "rb");
    if (!f_check) {
        fprintf(stderr, "\033[31mError: Source file '%s' does not exist.\033[0m\n", filename);
        return 1;
    }
    fclose(f_check);

    // Determine target platform
    PlatformInfo current = get_current_platform();
    PlatformInfo target;
    
    if (target_os) {
        target.os = target_os;
        // Validate architecture: only x86 and arm are allowed now
        if (strcmp(target_arch, "x86_64") != 0 && strcmp(target_arch, "arm64") != 0) {
            fprintf(stderr, "\033[31mError: Invalid architecture '%s'. Use 'x86_64' or 'arm64'.\033[0m\n", target_arch);
            return 1;
        }
        target.arch = target_arch;
    } else {
        target = current;
    }

    printf("\033[36mBuilding for: %s %s\033[0m\n", target.os, target.arch);
    printf("\033[32mBuilding %s...\033[0m\n", filename);

    // Generate strict output name: <filename>_<arch>_<os>[.exe]
    char base_name[4096];
    strncpy(base_name, filename, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    char* dot = strrchr(base_name, '.');
    if (dot != NULL) *dot = '\0'; // Strip .apex

    char final_output[4096];
    snprintf(final_output, sizeof(final_output), "%s_%s_%s", base_name, target.arch, target.os);
    if (strcmp(target.os, "windows") == 0) {
        strcat(final_output, ".exe");
    }

    char stub_filename[256];
    get_stub_filename(target.os, target.arch, stub_filename, sizeof(stub_filename));

    // Resolve directory of the current executable
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

    char stub_path[4096];
    snprintf(stub_path, sizeof(stub_path), "%s/%s", exe_dir, stub_filename);

    long self_size;
    char* self_code = read_file(stub_path, &self_size);
    if (!self_code) {
        fprintf(stderr, "\033[31mError: Cannot read stub '%s'. Ensure it is compiled and placed next to the apex binary.\033[0m\n", stub_path);
        return 1;
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

    // Write VFS Payload
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
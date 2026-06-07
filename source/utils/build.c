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

// Helper to resolve module path (mirrors parser.c logic)
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

// Helper to extract relative path
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

// Recursive import scanner
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
        fprintf(stderr, "\033[31mError: Missing filename for 'build' command.\033[0m\n");
        return 1;
    }
    const char* filename = argv[2];

    // Check if source file exists before proceeding
    FILE* f_check = fopen(filename, "rb");
    if (!f_check) {
        fprintf(stderr, "\033[31mError: Source file '%s' does not exist.\033[0m\n", filename);
        return 1;
    }
    fclose(f_check);

    char default_output[4096];
    strncpy(default_output, filename, sizeof(default_output) - 1);
    default_output[sizeof(default_output) - 1] = '\0';
    char* dot = strrchr(default_output, '.');
    if (dot != NULL) *dot = '\0';
    const char* output_name = (argc >= 4) ? argv[3] : default_output;

    printf("\033[32mBuilding %s...\033[0m\n", filename);

    char source_dir[4096];
    strncpy(source_dir, filename, sizeof(source_dir) - 1);
    source_dir[sizeof(source_dir) - 1] = '\0';
    char* last_slash = strrchr(source_dir, '/');
    char* last_backslash = strrchr(source_dir, '\\');
    if (last_backslash > last_slash) last_slash = last_backslash;
    if (last_slash) *last_slash = '\0';
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

    char self_path[4096];
#ifdef _WIN32
    GetModuleFileNameA(NULL, self_path, sizeof(self_path));
#else
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len == -1) {
        if (!realpath(argv[0], self_path)) {
            fprintf(stderr, "\033[31mError: Cannot resolve executable path\033[0m\n");
            for(int i=0; i<dep_count; i++) free(dependencies[i]);
            free(dependencies);
            return 1;
        }
    } else {
        self_path[len] = '\0';
    }
#endif

    long self_size;
    char* self_code = read_file(self_path, &self_size);
    if (!self_code) {
        fprintf(stderr, "\033[31mError: Cannot open self for reading\033[0m\n");
        for(int i=0; i<dep_count; i++) free(dependencies[i]);
        free(dependencies);
        return 1;
    }

    char final_output[4096];
    strcpy(final_output, output_name);
#ifdef _WIN32
    size_t out_len = strlen(final_output);
    bool has_exe = (out_len >= 4 && strcasecmp(final_output + out_len - 4, ".exe") == 0);
    if (!has_exe) strcat(final_output, ".exe");
#endif

    FILE* out = fopen(final_output, "wb");
    if (!out) {
        fprintf(stderr, "\033[31mError: Cannot create output file '%s'\033[0m\n", final_output);
        free(self_code);
        for(int i=0; i<dep_count; i++) free(dependencies[i]);
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
            for(int j=0; j<dep_count; j++) free(dependencies[j]);
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

    for(int i=0; i<dep_count; i++) free(dependencies[i]);
    free(dependencies);

#ifndef _WIN32
    chmod(final_output, 0755);
#endif
    printf("\033[32mBuilding %s completed! Output: %s\033[0m\n", filename, final_output);
    return 0;
}
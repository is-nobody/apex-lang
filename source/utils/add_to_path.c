#define _GNU_SOURCE
#include "add_to_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define realpath(N,R) _fullpath((R),(N),MAX_PATH)
    #define mkdir(path, mode) _mkdir(path)
    #define unlink _unlink
    #define chmod _chmod
    #define access _access
    #define F_OK 0
    #define setenv(name, value, overwrite) SetEnvironmentVariable(name, value)
    static int symlink(const char* target, const char* linkpath) {
        if (CreateHardLink(linkpath, target, NULL)) return 0;
        if (CreateSymbolicLink(linkpath, target, 0)) return 0;
        return -1;
    }
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

#define APEX_MARKER "# apex-path-managed"

static bool contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    size_t nlen = strlen(needle);
    size_t hlen = strlen(haystack);
    if (nlen > hlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i+j]) != tolower((unsigned char)needle[j])) {
                match = false; break;
            }
        }
        if (match) return true;
    }
    return false;
}

static void get_exe_paths(const char* argv0, char* out_dir, char* out_full, size_t size) {
#ifdef __linux__
    ssize_t len = readlink("/proc/self/exe", out_full, size - 1);
    if (len <= 0) strncpy(out_full, argv0, size - 1);
    out_full[size-1] = '\0';
#elif _WIN32
    GetModuleFileName(NULL, out_full, size);
#else
    realpath(argv0, out_full);
#endif
    char* resolved = realpath(out_full, NULL);
    if (resolved) { strncpy(out_full, resolved, size-1); free(resolved); }
    out_full[size-1] = '\0';

    strncpy(out_dir, out_full, size);
    char* last = strrchr(out_dir, '/');
    #ifdef _WIN32
        char* last2 = strrchr(out_dir, '\\');
        if (last2 > last) last = last2;
    #endif
    if (last) *last = '\0'; else out_dir[0] = '\0';
}

static void update_shell_config(const char* filepath, const char* local_bin_abs) {
    #ifdef _WIN32
    #endif
    
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        f = fopen(filepath, "wb");
        if (f) { fprintf(f, "\n%s\nexport PATH=\"%s:$PATH\"\n", APEX_MARKER, local_bin_abs); fclose(f); }
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* content = (char*)malloc(fsize + 1);
    size_t rd = fread(content, 1, fsize, f);
    content[rd] = '\0';
    fclose(f);

    char** lines = NULL;
    int count = 0;
    char* p = content;
    while (*p) {
        char* start = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') *p++ = '\0';
        
        size_t len = strlen(start);
        if (len > 0 && start[len-1] == '\r') start[len-1] = '\0';
        if (len == 0) continue;

        if (strstr(start, APEX_MARKER)) continue;
        if (contains_ci(start, "apex") && contains_ci(start, "PATH")) continue;

        lines = realloc(lines, sizeof(char*) * (count + 1));
        lines[count++] = strdup(start);
    }
    free(content);

    f = fopen(filepath, "wb");
    if (!f) { for (int i=0; i<count; i++) free(lines[i]); free(lines); return; }
    for (int i=0; i<count; i++) { fprintf(f, "%s\n", lines[i]); free(lines[i]); }
    fprintf(f, "\n%s\nexport PATH=\"%s:$PATH\"\n", APEX_MARKER, local_bin_abs);
    fclose(f);
    free(lines);
}

void ensure_path_updated(const char* argv0) {
    #ifdef _WIN32
    char exe_dir[MAX_PATH] = {0};
    char exe_full[MAX_PATH] = {0};
    #else
    char exe_dir[PATH_MAX] = {0};
    char exe_full[PATH_MAX] = {0};
    #endif
    
    get_exe_paths(argv0, exe_dir, exe_full, sizeof(exe_dir));
    if (!exe_dir[0]) return;

    const char* home = getenv("HOME");
    #ifdef _WIN32
        if (!home) home = getenv("USERPROFILE");
    #endif
    if (!home) return;

    #ifdef _WIN32
    char local_bin[MAX_PATH];
    #else
    char local_bin[PATH_MAX];
    #endif
    
    snprintf(local_bin, sizeof(local_bin), "%s/.local/bin", home);
    mkdir(local_bin, 0755);

    #ifdef _WIN32
    char link_path[MAX_PATH];
    snprintf(link_path, sizeof(link_path), "%s/apex.exe", local_bin);
    #else
    char link_path[PATH_MAX];
    snprintf(link_path, sizeof(link_path), "%s/apex", local_bin);
    #endif
    
    unlink(link_path);
    if (symlink(exe_full, link_path) != 0) {
        FILE* s = fopen(exe_full, "rb");
        FILE* d = fopen(link_path, "wb");
        if (s && d) {
            char buf[4096];
            size_t n;
            while((n = fread(buf, 1, sizeof(buf), s)) > 0) fwrite(buf, 1, n, d);
            fclose(s);
            fclose(d);
            chmod(link_path, 0755);
        }
    }

    #ifndef _WIN32
    const char* shell = getenv("SHELL");
    const char* configs[] = {".bashrc", ".zshrc", ".bash_profile", ".profile"};
    int cnt = 4;
    if (shell) {
        if (strstr(shell, "zsh")) { configs[0] = ".zshrc"; cnt = 1; }
        else if (strstr(shell, "bash")) { configs[0] = ".bashrc"; configs[1] = ".bash_profile"; cnt = 2; }
    }
    for (int i=0; i<cnt; i++) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/%s", home, configs[i]);
        if (access(p, F_OK) == 0 || i == 0) update_shell_config(p, local_bin);
    }

    char* old = getenv("PATH");
    char new_path[PATH_MAX * 2];
    snprintf(new_path, sizeof(new_path), "%s:%s", local_bin, old ? old : "");
    setenv("PATH", new_path, 1);
    #endif
}
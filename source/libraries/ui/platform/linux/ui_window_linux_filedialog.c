// source/libraries/ui/platform/linux/ui_window_linux_filedialog.c
// zenity / kdialog file and folder picker for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef _WIN32

#include "ui_window_linux.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

// appends `s` to `dst` as a single-quoted shell argument
static void append_quoted(char* dst, size_t cap, const char* s) {
    size_t p = strlen(dst);                               // current write position
    if (p + 2 >= cap) return;                             // no room even for the quotes
    dst[p++] = '\'';                                      // opening single quote
    for (const char* c = s; *c && p + 5 < cap; c++) {     // walk the argument bytes
        if (*c == '\'') {                                 // embedded single quote needs escaping
            dst[p++] = '\''; dst[p++] = '\\';             // close, escape,
            dst[p++] = '\''; dst[p++] = '\'';             // reopen, continue
        } else {
            dst[p++] = *c;                                // copy the character verbatim
        }
    }
    if (p < cap - 1) dst[p++] = '\'';                     // closing single quote
    dst[p] = 0;                                           // null terminate
}

// true when `cmd` is an executable found on PATH
static bool command_exists(const char* cmd) {
    const char* path = getenv("PATH");                    // search path from the environment
    if (!path) return false;                              // no PATH: nothing to search
    char full[4096];                                      // scratch buffer for one candidate
    while (*path) {                                       // one PATH entry per iteration
        const char* end = strchr(path, ':');              // find the entry separator
        int len = end ? (int)(end - path) : (int)strlen(path);
        if (len > 0 && len + 1 + (int)strlen(cmd) + 1 < (int)sizeof(full)) {
            memcpy(full, path, (size_t)len);              // copy the directory
            full[len] = '/';                              // append the separator
            strcpy(full + len + 1, cmd);                  // append the command name
            if (access(full, X_OK) == 0) return true;     // executable found
        }
        path = end ? end + 1 : path + strlen(path);       // advance to the next entry
    }
    return false;                                         // not found anywhere on PATH
}

// runs the command through popen and returns the first line of its stdout
static char* run_and_capture(const char* cmd) {
    FILE* p = popen(cmd, "r");                            // spawn and capture stdout
    if (!p) return NULL;                                  // popen failed
    char buf[4096];                                       // line buffer
    size_t n = 0;                                         // captured length
    if (fgets(buf, sizeof(buf), p)) {                     // read the first line
        n = strlen(buf);                                  // its length
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
    }                                                     // trim trailing newline characters
    int status = pclose(p);                               // wait for the child
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return NULL;  // exited abnormally
    if (n == 0) return NULL;                              // no output: treat as cancel
    char* out = (char*)malloc(n + 1);                     // allocate the result copy
    if (out) memcpy(out, buf, n + 1);                     // copy bytes and null
    return out;                                           // caller owns the copy
}

// shows the native "open file" dialog and returns the picked utf-8 path
char* ui_window_open_file(UiWindow* w, const char* title,
                          const char* default_path,
                          const UiFileFilter* filters) {
    (void)w;                                              // pickers are external processes, no parent
    char cmd[8192] = {0};                                 // assembled shell command line

    if (command_exists("zenity")) {                       // prefer zenity when installed
        strcpy(cmd, "zenity --file-selection");           // base invocation
        if (title && *title) {                            // optional dialog caption
            strcat(cmd, " --title=");
            append_quoted(cmd, sizeof(cmd), title);
        }
        if (default_path && *default_path) {              // optional starting path
            strcat(cmd, " --filename=");
            append_quoted(cmd, sizeof(cmd), default_path);
        }
        if (filters) {                                    // optional filter list
            for (int i = 0; filters[i].name; i++) {       // one entry per filter
                char f[512];                              // "name | pattern" pair
                snprintf(f, sizeof(f), "%s | %s", filters[i].name, filters[i].pattern);
                strcat(cmd, " --file-filter=");
                append_quoted(cmd, sizeof(cmd), f);       // append the quoted filter
            }
        }
    } else if (command_exists("kdialog")) {               // fall back to kdialog
        strcpy(cmd, "kdialog --getopenfilename");         // base invocation
        strcat(cmd, " ");                                 // separate the starting path
        append_quoted(cmd, sizeof(cmd),
                      (default_path && *default_path) ? default_path : ".");
        if (filters && filters[0].name && filters[0].pattern) {  // first filter only
            char f[512];                                  // "pattern|name" pair, kdialog order
            snprintf(f, sizeof(f), "%s|%s", filters[0].pattern, filters[0].name);
            strcat(cmd, " ");
            append_quoted(cmd, sizeof(cmd), f);           // append the quoted filter
        }
        if (title && *title) {                            // optional dialog caption
            strcat(cmd, " --title=");
            append_quoted(cmd, sizeof(cmd), title);
        }
    } else {
        return NULL;                                      // no supported picker available
    }

    return run_and_capture(cmd);                          // run it and hand back the path
}

// shows the native "open folder" dialog and returns the picked utf-8 path
char* ui_window_open_folder(UiWindow* w, const char* title,
                            const char* default_path) {
    (void)w;                                              // pickers are external processes, no parent
    char cmd[8192] = {0};                                 // assembled shell command line

    if (command_exists("zenity")) {                       // prefer zenity when installed
        strcpy(cmd, "zenity --file-selection --directory");  // folder mode
        if (title && *title) {                            // optional dialog caption
            strcat(cmd, " --title=");
            append_quoted(cmd, sizeof(cmd), title);
        }
        if (default_path && *default_path) {              // optional starting path
            strcat(cmd, " --filename=");
            append_quoted(cmd, sizeof(cmd), default_path);
        }
    } else if (command_exists("kdialog")) {               // fall back to kdialog
        strcpy(cmd, "kdialog --getexistingdirectory");    // base invocation
        strcat(cmd, " ");                                 // separate the starting path
        append_quoted(cmd, sizeof(cmd),
                      (default_path && *default_path) ? default_path : ".");
        if (title && *title) {                            // optional dialog caption
            strcat(cmd, " --title=");
            append_quoted(cmd, sizeof(cmd), title);
        }
    } else {
        return NULL;                                      // no supported picker available
    }

    return run_and_capture(cmd);                          // run it and hand back the path
}

// shows the native "save file" dialog and returns the picked utf-8 path
char* ui_window_save_file(UiWindow* w, const char* title,
                          const char* default_path,
                          const UiFileFilter* filters) {
    (void)w;                                              // pickers are external processes, no parent
    char cmd[8192] = {0};                                 // assembled shell command line

    if (command_exists("zenity")) {                       // prefer zenity when installed
        strcpy(cmd, "zenity --file-selection --save");
        if (title && *title) {                            // optional dialog caption
            strcat(cmd, " --title=");
            append_quoted(cmd, sizeof(cmd), title);
        }
        if (default_path && *default_path) {              // optional suggested filename
            strcat(cmd, " --filename=");
            append_quoted(cmd, sizeof(cmd), default_path);
        }
        if (filters) {                                    // optional filter list
            for (int i = 0; filters[i].name; i++) {       // one entry per filter
                char f[512];                              // "name | pattern" pair
                snprintf(f, sizeof(f), "%s | %s", filters[i].name, filters[i].pattern);
                strcat(cmd, " --file-filter=");
                append_quoted(cmd, sizeof(cmd), f);       // append the quoted filter
            }
        }
    } else if (command_exists("kdialog")) {               // fall back to kdialog
        strcpy(cmd, "kdialog --getsavefilename");         // base invocation
        strcat(cmd, " ");                                 // separate the starting path
        append_quoted(cmd, sizeof(cmd),
                      (default_path && *default_path) ? default_path : ".");
        if (filters && filters[0].name && filters[0].pattern) {  // first filter only
            char f[512];                                  // "pattern|name" pair, kdialog order
            snprintf(f, sizeof(f), "%s|%s", filters[0].pattern, filters[0].name);
            strcat(cmd, " ");
            append_quoted(cmd, sizeof(cmd), f);           // append the quoted filter
        }
        if (title && *title) {                            // optional dialog caption
            strcat(cmd, " --title=");
            append_quoted(cmd, sizeof(cmd), title);
        }
    } else {
        return NULL;                                      // no supported picker available
    }

    return run_and_capture(cmd);                          // run it and hand back the path
}

#endif
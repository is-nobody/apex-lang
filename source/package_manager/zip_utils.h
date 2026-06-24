#ifndef ZIP_UTILS_H
#define ZIP_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

__attribute__((unused))
static void mkdirs(const char* path) {
    char tmp[1024];
    char* p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

__attribute__((unused))
static bool extract_zip(const char* zip_path, const char* dest_dir) {
    char cmd[2048];
    int ret;

#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
        zip_path, dest_dir);
#else
    mkdirs(dest_dir);
    snprintf(cmd, sizeof(cmd),
        "unzip -o '%s' -d '%s'",
        zip_path, dest_dir);
#endif

    ret = system(cmd);
    return (ret == 0);
}

__attribute__((unused))
static bool create_zip(const char* zip_name, const char* source_dir) {
    char cmd[2048];
    int ret;

#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"Compress-Archive -Path '%s\\*' -DestinationPath '%s' -Force\"",
        source_dir, zip_name);
#else
    snprintf(cmd, sizeof(cmd),
        "cd '%s' && zip -r '%s' . -x '*.zip' '.packages/*' 'node_modules/*'",
        source_dir, zip_name);
#endif

    ret = system(cmd);
    return (ret == 0);
}

#endif // ZIP_UTILS_H
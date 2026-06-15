#include "files_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>
#define chdir _chdir
#define getcwd _getcwd
#define rmdir _rmdir
#define unlink _unlink
#define mkdir _mkdir
#define stat _stat
#define fstat _fstat
#define access _access
#ifndef F_OK
#define F_OK 0
#endif
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/statvfs.h>
#endif

// Helper for recursive directory size
static double calculate_dir_size(const char* path) {
    double total_size = 0;
#ifdef _WIN32
    WIN32_FIND_DATA fd;
    HANDLE hFind;
    char search_path[4096];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);
    hFind = FindFirstFile(search_path, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            char full_path[4096];
            snprintf(full_path, sizeof(full_path), "%s\\%s", path, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                total_size += calculate_dir_size(full_path);
            } else {
                LARGE_INTEGER size;
                size.LowPart = fd.nFileSizeLow;
                size.HighPart = fd.nFileSizeHigh;
                total_size += (double)size.QuadPart;
            }
        } while (FindNextFile(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            char full_path[4096];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    total_size += calculate_dir_size(full_path);
                } else if (S_ISREG(st.st_mode)) {
                    total_size += (double)st.st_size;
                }
            }
        }
        closedir(dir);
    }
#endif
    return total_size;
}

bool files_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;

    // --- File Content ---
    if (strcmp(name, "files.read") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                char* buffer = (char*)malloc(size + 1);
                size_t read_bytes = fread(buffer, 1, size, f);
                (void)read_bytes;
                buffer[size] = '\0';
                fclose(f);
                *result = vm_make_string(buffer);
                free(buffer);
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.write") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "wb");
            if (f) {
                fputs(args[1].string->chars, f);
                fclose(f);
                *result = vm_make_bool(true);
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.append") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "ab");
            if (f) {
                fputs(args[1].string->chars, f);
                fclose(f);
                *result = vm_make_bool(true);
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // --- File/Dir Info ---
    if (strcmp(name, "files.exists") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
#ifdef _WIN32
            *result = vm_make_bool(access(args[0].string->chars, F_OK) == 0);
#else
            *result = vm_make_bool(access(args[0].string->chars, F_OK) == 0);
#endif
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.isfile") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) == 0) {
                *result = vm_make_bool(S_ISREG(st.st_mode) ? true : false);
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.isdir") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) == 0) {
                *result = vm_make_bool(S_ISDIR(st.st_mode) ? true : false);
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.filesize") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) == 0) {
                if (S_ISREG(st.st_mode)) {
                    *result = vm_make_number((double)st.st_size);
                } else {
                    *result = vm_make_bool(false);
                }
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.dirsize") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) == 0 && S_ISDIR(st.st_mode)) {
                *result = vm_make_number(calculate_dir_size(args[0].string->chars));
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.stat") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) == 0) {
                *result = vm_make_table();
                table_set(result->table, "size", vm_make_number(st.st_size));
                table_set(result->table, "mtime", vm_make_number(st.st_mtime));
                table_set(result->table, "ctime", vm_make_number(st.st_ctime));
                table_set(result->table, "isdir", vm_make_bool(S_ISDIR(st.st_mode) ? true : false));
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.filetype") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "rb");
            if (!f) {
                *result = vm_make_bool(false);
                return true;
            }
            unsigned char header[16];
            size_t read_bytes = fread(header, 1, 16, f);
            fclose(f);
            if (read_bytes == 0) {
                *result = vm_make_bool(false);
            } else if (read_bytes >= 4 && header[0] == '%' && header[1] == 'P' && header[2] == 'D' && header[3] == 'F') {
                *result = vm_make_string("PDF document");
            } else if (read_bytes >= 8 && header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G' && header[4] == '\r' && header[5] == '\n' && header[6] == 0x1A && header[7] == '\n') {
                *result = vm_make_string("PNG image");
            } else if (read_bytes >= 3 && header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
                *result = vm_make_string("JPEG image");
            } else if (read_bytes >= 6 && (memcmp(header, "GIF87a", 6) == 0 || memcmp(header, "GIF89a", 6) == 0)) {
                *result = vm_make_string("GIF image");
            } else if (read_bytes >= 4 && header[0] == 'P' && header[1] == 'K' && (header[2] == 0x03 || header[2] == 0x05 || header[2] == 0x07) && header[3] == 0x04) {
                *result = vm_make_string("ZIP archive");
            } else if (read_bytes >= 4 && header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F') {
                *result = vm_make_string("ELF executable");
            } else if (read_bytes >= 2 && header[0] == 'M' && header[1] == 'Z') {
                *result = vm_make_string("Windows executable");
            } else {
                bool is_text = true;
                for (size_t i = 0; i < read_bytes; i++) {
                    if (header[i] < 32 && header[i] != '\n' && header[i] != '\r' && header[i] != '\t') {
                        is_text = false;
                        break;
                    }
                }
                *result = vm_make_string(is_text ? "Plain text" : "Unknown binary");
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // --- File/Dir Operations ---
    if (strcmp(name, "files.mkfile") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "w");
            if (f) {
                fclose(f);
                *result = vm_make_bool(true);
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.mkdir") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
#ifdef _WIN32
            *result = vm_make_bool(_mkdir(args[0].string->chars) == 0);
#else
            *result = vm_make_bool(mkdir(args[0].string->chars, 0755) == 0);
#endif
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.rmfile") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            *result = vm_make_bool(unlink(args[0].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.rmdir") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            *result = vm_make_bool(rmdir(args[0].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.rnfile") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_file = stat(args[0].string->chars, &st) == 0 && S_ISREG(st.st_mode);
            *result = vm_make_bool(is_file && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.rndir") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_dir = stat(args[0].string->chars, &st) == 0 && S_ISDIR(st.st_mode);
            *result = vm_make_bool(is_dir && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.mvfile") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_file = stat(args[0].string->chars, &st) == 0 && S_ISREG(st.st_mode);
            *result = vm_make_bool(is_file && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.mvdir") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_dir = stat(args[0].string->chars, &st) == 0 && S_ISDIR(st.st_mode);
            *result = vm_make_bool(is_dir && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.cpfile") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            FILE* src = fopen(args[0].string->chars, "rb");
            if (!src) { *result = vm_make_bool(false); return true; }
            FILE* dst = fopen(args[1].string->chars, "wb");
            if (!dst) { fclose(src); *result = vm_make_bool(false); return true; }
            char buffer[8192];
            size_t bytes_read;
            bool success = true;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                size_t bytes_written = fwrite(buffer, 1, bytes_read, dst);
                if (bytes_written != bytes_read) { success = false; break; }
            }
            fclose(src);
            fclose(dst);
            *result = vm_make_bool(success);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.cpdir") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) != 0 || !S_ISDIR(st.st_mode)) {
                *result = vm_make_bool(false);
                return true;
            }
#ifdef _WIN32
            if (_mkdir(args[1].string->chars) != 0 && errno != EEXIST) {
                *result = vm_make_bool(false);
                return true;
            }
#else
            if (mkdir(args[1].string->chars, 0755) != 0 && errno != EEXIST) {
                *result = vm_make_bool(false);
                return true;
            }
#endif
            bool success = true;
#ifdef _WIN32
            WIN32_FIND_DATA fd;
            HANDLE hFind;
            char search_path[4096];
            snprintf(search_path, sizeof(search_path), "%s\\*", args[0].string->chars);
            hFind = FindFirstFile(search_path, &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
                    char src_path[4096], dst_path[4096];
                    snprintf(src_path, sizeof(src_path), "%s\\%s", args[0].string->chars, fd.cFileName);
                    snprintf(dst_path, sizeof(dst_path), "%s\\%s", args[1].string->chars, fd.cFileName);
                    Value cp_result;
                    Value cp_args[] = {vm_make_string(src_path), vm_make_string(dst_path)};
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        if (!files_call_builtin(vm, "files.cpdir", 2, cp_args, &cp_result) ||
                            (cp_result.type == VAL_BOOL && !cp_result.boolean)) {
                            success = false;
                            break;
                        }
                    } else {
                        if (!files_call_builtin(vm, "files.cpfile", 2, cp_args, &cp_result) ||
                            (cp_result.type == VAL_BOOL && !cp_result.boolean)) {
                            success = false;
                            break;
                        }
                    }
                } while (FindNextFile(hFind, &fd));
                FindClose(hFind);
            } else {
                success = false;
            }
#else
            DIR* dir = opendir(args[0].string->chars);
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                    char src_path[4096], dst_path[4096];
                    snprintf(src_path, sizeof(src_path), "%s/%s", args[0].string->chars, entry->d_name);
                    snprintf(dst_path, sizeof(dst_path), "%s/%s", args[1].string->chars, entry->d_name);
                    struct stat entry_st;
                    if (stat(src_path, &entry_st) != 0) { success = false; break; }
                    Value cp_result;
                    Value cp_args[] = {vm_make_string(src_path), vm_make_string(dst_path)};
                    if (S_ISDIR(entry_st.st_mode)) {
                        if (!files_call_builtin(vm, "files.cpdir", 2, cp_args, &cp_result) ||
                            (cp_result.type == VAL_BOOL && !cp_result.boolean)) {
                            success = false;
                            break;
                        }
                    } else if (S_ISREG(entry_st.st_mode)) {
                        if (!files_call_builtin(vm, "files.cpfile", 2, cp_args, &cp_result) ||
                            (cp_result.type == VAL_BOOL && !cp_result.boolean)) {
                            success = false;
                            break;
                        }
                    }
                }
                closedir(dir);
            } else {
                success = false;
            }
#endif
            *result = vm_make_bool(success);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.listdir") == 0) {
        const char* path = ".";
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            path = args[0].string->chars;
        }
        *result = vm_make_table();
#ifdef _WIN32
        WIN32_FIND_DATA fd;
        HANDLE hFind;
        char search_path[4096];
        snprintf(search_path, sizeof(search_path), "%s\\*", path);
        hFind = FindFirstFile(search_path, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            int idx = 1;
            do {
                char key[32];
                snprintf(key, sizeof(key), "%d", idx++);
                table_set(result->table, key, vm_make_string(fd.cFileName));
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        } else {
            *result = vm_make_bool(false);
        }
#else
        DIR* dir = opendir(path);
        if (dir) {
            struct dirent* entry;
            int idx = 1;
            while ((entry = readdir(dir)) != NULL) {
                char key[32];
                snprintf(key, sizeof(key), "%d", idx++);
                table_set(result->table, key, vm_make_string(entry->d_name));
            }
            closedir(dir);
        } else {
            *result = vm_make_bool(false);
        }
#endif
        return true;
    }
    if (strcmp(name, "files.parentfolder") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            const char* path = args[0].string->chars;
            char* last_sep = strrchr(path, '/');
#ifdef _WIN32
            char* last_sep_win = strrchr(path, '\\');
            if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
                last_sep = last_sep_win;
            }
#endif
            if (last_sep) {
                int len = last_sep - path;
                if (len == 0) {
                    if (path[0] == '/') {
                        *result = vm_make_string("/");
                    } else {
                        char root[4];
                        snprintf(root, sizeof(root), "%c%c", path[0], path[1]);
                        *result = vm_make_string(root);
                    }
                } else {
                    char* parent = (char*)malloc(len + 1);
                    strncpy(parent, path, len);
                    parent[len] = '\0';
                    *result = vm_make_string(parent);
                    free(parent);
                }
            } else {
                *result = vm_make_string(".");
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    if (strcmp(name, "files.access") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_NUMBER) {
#ifdef _WIN32
            *result = vm_make_bool(_chmod(args[0].string->chars, (int)args[1].number) == 0);
#else
            *result = vm_make_bool(chmod(args[0].string->chars, (mode_t)args[1].number) == 0);
#endif
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    return false;
}
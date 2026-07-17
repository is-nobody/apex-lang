#include "os_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <tlhelp32.h>
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
#ifndef S_ISREG
#define S_ISREG(mode) ((mode) & _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode) & _S_IFDIR)
#endif
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#endif

// recursively calculates the total size of a directory in bytes
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

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    (void)vm;
    int len = (int)strlen(str);
    if (len >= 16 && len <= 64 && vm->intern_table.count < 50000) {
        return MAKE_STRING(string_intern(&vm->intern_table, str, len));
    }
    return MAKE_STRING(string_create(str, len));
}

// dispatcher for operating system built-in functions
bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "os.output") == 0) {
        if (arg_count >= 1) {
            vm_print_value(args[0]);
            printf("\n");
            fflush(stdout);
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "os.input") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            printf("%s", AS_STRING(args[0])->chars);
            fflush(stdout);
        }
        char buffer[4096];
        if (fgets(buffer, sizeof(buffer), stdin)) {
            buffer[strcspn(buffer, "\r\n")] = 0;
            *result = make_string_val(vm, buffer);
        } else {
            *result = make_string_val(vm, "");
        }
        return true;
    }
    
    if (strcmp(name, "os.time") == 0) {
#ifdef _WIN32
        struct _timeb tb;
        _ftime(&tb);
        *result = MAKE_NUMBER((double)tb.time + (double)tb.millitm / 1000.0);
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        *result = MAKE_NUMBER((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);
#endif
        return true;
    }
    
    if (strcmp(name, "os.wait") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            double seconds = AS_NUMBER(args[0]);
            if (seconds < 0) seconds = 0;
#ifdef _WIN32
            Sleep((DWORD)(seconds * 1000));
#else
            struct timespec ts;
            ts.tv_sec = (time_t)seconds;
            ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1000000000);
            nanosleep(&ts, NULL);
#endif
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "os.exit") == 0) {
        int code = 0;
        if (arg_count >= 1 && IS_NUMBER(args[0])) code = (int)AS_NUMBER(args[0]);
        exit(code);
        return true;
    }
    
    if (strcmp(name, "os.get_current_folder") == 0) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) *result = make_string_val(vm, cwd);
        else *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "os.set_current_folder") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            *result = MAKE_BOOL(chdir(AS_STRING(args[0])->chars) == 0);
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.terminate_process") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            int pid = (int)AS_NUMBER(args[0]);
            bool success = false;
#ifdef _WIN32
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
            if (hProcess) {
                success = TerminateProcess(hProcess, 1) != 0;
                CloseHandle(hProcess);
            }
#else
            success = (kill((pid_t)pid, SIGTERM) == 0);
#endif
            *result = MAKE_BOOL(success);
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.execute") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            int exit_code = system(AS_STRING(args[0])->chars);
            *result = MAKE_NUMBER(exit_code);
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }

    if (strcmp(name, "os.read") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            FILE* f = fopen(AS_STRING(args[0])->chars, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                char* buffer = (char*)malloc(size + 1);
                size_t read_bytes = fread(buffer, 1, size, f);
                (void)read_bytes;
                buffer[size] = '\0';
                fclose(f);
                *result = make_string_val(vm, buffer);
                free(buffer);
            } else {
                *result = MAKE_NONE();
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }
    
    if (strcmp(name, "os.write") == 0) {
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
            FILE* f = fopen(AS_STRING(args[0])->chars, "wb");
            if (f) {
                fputs(AS_STRING(args[1])->chars, f);
                fclose(f);
                *result = MAKE_BOOL(true);
            } else {
                *result = MAKE_BOOL(false);
            }
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.append") == 0) {
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
            FILE* f = fopen(AS_STRING(args[0])->chars, "ab");
            if (f) {
                fputs(AS_STRING(args[1])->chars, f);
                fclose(f);
                *result = MAKE_BOOL(true);
            } else {
                *result = MAKE_BOOL(false);
            }
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.exists") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            *result = MAKE_BOOL(access(AS_STRING(args[0])->chars, F_OK) == 0);
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.isfile") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            struct stat st;
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {
                *result = MAKE_BOOL(S_ISREG(st.st_mode) ? true : false);
            } else {
                *result = MAKE_BOOL(false);
            }
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.isfolder") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            struct stat st;
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {
                *result = MAKE_BOOL(S_ISDIR(st.st_mode) ? true : false);
            } else {
                *result = MAKE_BOOL(false);
            }
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.size") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            struct stat st;
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {
                if (S_ISREG(st.st_mode)) {
                    *result = MAKE_NUMBER((double)st.st_size);
                } else if (S_ISDIR(st.st_mode)) {
                    *result = MAKE_NUMBER(calculate_dir_size(AS_STRING(args[0])->chars));
                } else {
                    *result = MAKE_NONE();
                }
            } else {
                *result = MAKE_NONE();
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }
    
    if (strcmp(name, "os.stat") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            struct stat st;
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {
                Table* t = table_create(8);
                *result = MAKE_TABLE(t);
                Value k1 = make_string_val(vm, "size"); table_set(t, k1, MAKE_NUMBER(st.st_size)); value_decref(k1);
                Value k2 = make_string_val(vm, "mtime"); table_set(t, k2, MAKE_NUMBER(st.st_mtime)); value_decref(k2);
                Value k3 = make_string_val(vm, "ctime"); table_set(t, k3, MAKE_NUMBER(st.st_ctime)); value_decref(k3);
                Value k4 = make_string_val(vm, "isdir"); table_set(t, k4, MAKE_BOOL(S_ISDIR(st.st_mode) ? true : false)); value_decref(k4);
            } else {
                *result = MAKE_NONE();
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }
    
    if (strcmp(name, "os.filetype") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            FILE* f = fopen(AS_STRING(args[0])->chars, "rb");
            if (!f) {
                *result = MAKE_NONE();
                return true;
            }
            unsigned char header[16];
            size_t read_bytes = fread(header, 1, 16, f);
            fclose(f);
            if (read_bytes == 0) {
                *result = MAKE_NONE();
            } else if (read_bytes >= 4 && header[0] == '%' && header[1] == 'P' && header[2] == 'D' && header[3] == 'F') {
                *result = make_string_val(vm, "PDF document");
            } else if (read_bytes >= 8 && header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G' && header[4] == '\r' && header[5] == '\n' && header[6] == 0x1A && header[7] == '\n') {
                *result = make_string_val(vm, "PNG image");
            } else if (read_bytes >= 3 && header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
                *result = make_string_val(vm, "JPEG image");
            } else if (read_bytes >= 6 && (memcmp(header, "GIF87a", 6) == 0 || memcmp(header, "GIF89a", 6) == 0)) {
                *result = make_string_val(vm, "GIF image");
            } else if (read_bytes >= 4 && header[0] == 'P' && header[1] == 'K' && (header[2] == 0x03 || header[2] == 0x05 || header[2] == 0x07) && header[3] == 0x04) {
                *result = make_string_val(vm, "ZIP archive");
            } else if (read_bytes >= 4 && header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F') {
                *result = make_string_val(vm, "ELF executable");
            } else if (read_bytes >= 2 && header[0] == 'M' && header[1] == 'Z') {
                *result = make_string_val(vm, "Windows executable");
            } else {
                bool is_text = true;
                for (size_t i = 0; i < read_bytes; i++) {
                    if (header[i] < 32 && header[i] != '\n' && header[i] != '\r' && header[i] != '\t') {
                        is_text = false;
                        break;
                    }
                }
                *result = make_string_val(vm, is_text ? "Plain text" : "Unknown binary");
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }
    
    if (strcmp(name, "os.create_file") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            FILE* f = fopen(AS_STRING(args[0])->chars, "w");
            if (f) {
                fclose(f);
                *result = MAKE_BOOL(true);
            } else {
                *result = MAKE_BOOL(false);
            }
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.create_folder") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
#ifdef _WIN32
            *result = MAKE_BOOL(_mkdir(AS_STRING(args[0])->chars) == 0);
#else
            *result = MAKE_BOOL(mkdir(AS_STRING(args[0])->chars, 0755) == 0);
#endif
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.delete") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            struct stat st;
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {
                bool success = false;
                if (S_ISDIR(st.st_mode)) {
                    success = (rmdir(AS_STRING(args[0])->chars) == 0);
                } else {
                    success = (unlink(AS_STRING(args[0])->chars) == 0);
                }
                *result = MAKE_BOOL(success);
            } else {
                *result = MAKE_BOOL(false);
            }
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.rename") == 0) {
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
            *result = MAKE_BOOL(rename(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars) == 0);
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.move") == 0) {
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
            *result = MAKE_BOOL(rename(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars) == 0);
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.copy") == 0) {
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
            const char* src_path = AS_STRING(args[0])->chars;
            const char* dst_path = AS_STRING(args[1])->chars;
            
            struct stat st;
            if (stat(src_path, &st) != 0) {
                *result = MAKE_BOOL(false);
                return true;
            }

            bool success = true;

            if (S_ISDIR(st.st_mode)) {
#ifdef _WIN32
                if (_mkdir(dst_path) != 0 && errno != EEXIST) {
                    *result = MAKE_BOOL(false);
                    return true;
                }
#else
                if (mkdir(dst_path, 0755) != 0 && errno != EEXIST) {
                    *result = MAKE_BOOL(false);
                    return true;
                }
#endif
#ifdef _WIN32
                WIN32_FIND_DATA fd;
                HANDLE hFind;
                char search_path[4096];
                snprintf(search_path, sizeof(search_path), "%s\\*", src_path);
                hFind = FindFirstFile(search_path, &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
                        char sub_src[4096], sub_dst[4096];
                        snprintf(sub_src, sizeof(sub_src), "%s\\%s", src_path, fd.cFileName);
                        snprintf(sub_dst, sizeof(sub_dst), "%s\\%s", dst_path, fd.cFileName);
                        
                        Value sub_result;
                        Value sub_args[2];
                        sub_args[0] = make_string_val(vm, sub_src);
                        sub_args[1] = make_string_val(vm, sub_dst);
                        if (!os_call_builtin(vm, "os.copy", 2, sub_args, &sub_result) || 
                            (IS_BOOL(sub_result) && !AS_BOOL(sub_result))) {
                            success = false;
                            value_decref(sub_args[0]);
                            value_decref(sub_args[1]);
                            break;
                        }
                        value_decref(sub_args[0]);
                        value_decref(sub_args[1]);
                    } while (FindNextFile(hFind, &fd));
                    FindClose(hFind);
                } else {
                    success = false;
                }
#else
                DIR* dir = opendir(src_path);
                if (dir) {
                    struct dirent* entry;
                    while ((entry = readdir(dir)) != NULL) {
                        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                        char sub_src[4096], sub_dst[4096];
                        snprintf(sub_src, sizeof(sub_src), "%s/%s", src_path, entry->d_name);
                        snprintf(sub_dst, sizeof(sub_dst), "%s/%s", dst_path, entry->d_name);
                        
                        struct stat entry_st;
                        if (stat(sub_src, &entry_st) != 0) { success = false; break; }

                        Value sub_result;
                        Value sub_args[2];
                        sub_args[0] = make_string_val(vm, sub_src);
                        sub_args[1] = make_string_val(vm, sub_dst);
                        if (!os_call_builtin(vm, "os.copy", 2, sub_args, &sub_result) || 
                            (IS_BOOL(sub_result) && !AS_BOOL(sub_result))) {
                            success = false;
                            value_decref(sub_args[0]);
                            value_decref(sub_args[1]);
                            break;
                        }
                        value_decref(sub_args[0]);
                        value_decref(sub_args[1]);
                    }
                    closedir(dir);
                } else {
                    success = false;
                }
#endif
            } else {
                FILE* src = fopen(src_path, "rb");
                if (!src) {
                    *result = MAKE_BOOL(false);
                    return true;
                }
                FILE* dst = fopen(dst_path, "wb");
                if (!dst) {
                    fclose(src);
                    *result = MAKE_BOOL(false);
                    return true;
                }
                char buffer[8192];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                    if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) {
                        success = false;
                        break;
                    }
                }
                fclose(src);
                fclose(dst);
            }
            *result = MAKE_BOOL(success);
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }
    
    if (strcmp(name, "os.items") == 0) {
        const char* path = ".";
        if (arg_count >= 1 && IS_STRING(args[0])) {
            path = AS_STRING(args[0])->chars;
        }
        
    #ifdef _WIN32
        WIN32_FIND_DATA fd;
        HANDLE hFind;
        char search_path[4096];
        snprintf(search_path, sizeof(search_path), "%s\\*", path);
        hFind = FindFirstFile(search_path, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            Table* t = table_create(8);
            *result = MAKE_TABLE(t);
            int idx = 1;
            do {
                Value k = MAKE_NUMBER((double)idx++);
                table_set(t, k, make_string_val(vm, fd.cFileName));
                value_decref(k);
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        } else {
            *result = MAKE_NONE();
        }
#else
        DIR* dir = opendir(path);
        if (dir) {
            Table* t = table_create(8);
            *result = MAKE_TABLE(t);
            struct dirent* entry;
            int idx = 1;
            while ((entry = readdir(dir)) != NULL) {
                Value k = MAKE_NUMBER((double)idx++);
                table_set(t, k, make_string_val(vm, entry->d_name));
                value_decref(k);
            }
            closedir(dir);
        } else {
            *result = MAKE_NONE();
        }
#endif
        return true;
    }
    
    if (strcmp(name, "os.parentfolder") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            const char* path = AS_STRING(args[0])->chars;
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
                        *result = make_string_val(vm, "/");
                    } else {
                        char root[4];
                        snprintf(root, sizeof(root), "%c%c", path[0], path[1]);
                        *result = make_string_val(vm, root);
                    }
                } else {
                    char* parent = (char*)malloc(len + 1);
                    strncpy(parent, path, len);
                    parent[len] = '\0';
                    *result = make_string_val(vm, parent);
                    free(parent);
                }
            } else {
                *result = make_string_val(vm, ".");
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }
    
    if (strcmp(name, "os.access") == 0) {
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_NUMBER(args[1])) {
#ifdef _WIN32
            *result = MAKE_BOOL(_chmod(AS_STRING(args[0])->chars, (int)AS_NUMBER(args[1])) == 0);
#else
            *result = MAKE_BOOL(chmod(AS_STRING(args[0])->chars, (mode_t)AS_NUMBER(args[1])) == 0);
#endif
        } else {
            *result = MAKE_BOOL(false);
        }
        return true;
    }

    return false;
}
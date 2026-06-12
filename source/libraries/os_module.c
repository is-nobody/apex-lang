#include "os_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <sys/stat.h>
    #include <sys/timeb.h>
    #define chdir _chdir
    #define getcwd _getcwd
    #define rmdir _rmdir
    #define unlink _unlink
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <sys/time.h>
    #include <signal.h>
    #include <sys/wait.h>
#endif

bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    
    // os.output — print a value to stdout
    if (strcmp(name, "os.output") == 0) {
        if (arg_count >= 1) {
            vm_print_value(&args[0]);
            printf("\n");
            fflush(stdout);
        }
        *result = vm_make_bool(false);
        return true;
    }
    
    // os.input — read a line from stdin with an optional prompt
    if (strcmp(name, "os.input") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            printf("%s", args[0].string->chars);
            fflush(stdout);
        }
        char buffer[4096];
        if (fgets(buffer, sizeof(buffer), stdin)) {
            buffer[strcspn(buffer, "\r\n")] = 0;
            *result = vm_make_string(buffer);
        } else {
            *result = vm_make_string("");
        }
        return true;
    }
    
    // os.time — return current Unix timestamp as a float
    if (strcmp(name, "os.time") == 0) {
        #ifdef _WIN32
            struct _timeb tb;
            _ftime(&tb);
            *result = vm_make_number((double)tb.time + (double)tb.millitm / 1000.0);
        #else
            struct timeval tv;
            gettimeofday(&tv, NULL);
            *result = vm_make_number((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);
        #endif
        return true;
    }
    
    // os.wait — suspend execution for N seconds
    if (strcmp(name, "os.wait") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double seconds = args[0].number;
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
        *result = vm_make_bool(false);
        return true;
    }
    
    // os.exit — terminate the process with an optional exit code
    if (strcmp(name, "os.exit") == 0) {
        int code = 0;
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            code = (int)args[0].number;
        }
        exit(code);
        return true;
    }
    
    // os.system — execute a shell command, return exit code
    if (strcmp(name, "os.system") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            int exit_code = system(args[0].string->chars);
            *result = vm_make_number(exit_code);
        } else {
            *result = vm_make_number(-1);
        }
        return true;
    }
    
    // os.read — read entire file contents into a string
    if (strcmp(name, "os.read") == 0) {
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
        }
        return true;
    }
    
    // os.write — write a string to a file (overwrites)
    if (strcmp(name, "os.write") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "wb");
            if (f) {
                fputs(args[1].string->chars, f);
                fclose(f);
                *result = vm_make_bool(true);
            } else {
                *result = vm_make_bool(false);
            }
        }
        return true;
    }
    
    // os.exists — check if a file or directory exists
    if (strcmp(name, "os.exists") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "r");
            if (f) {
                fclose(f);
                *result = vm_make_bool(true);
            } else {
                *result = vm_make_bool(false);
            }
        }
        return true;
    }
    
    // os.isfile — check if path is a regular file
    if (strcmp(name, "os.isfile") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) == 0) {
                *result = vm_make_bool(S_ISREG(st.st_mode) ? true : false);
            } else {
                *result = vm_make_bool(false);
            }
        }
        return true;
    }
    
    // os.isdir — check if path is a directory
    if (strcmp(name, "os.isdir") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) == 0) {
                *result = vm_make_bool(S_ISDIR(st.st_mode) ? true : false);
            } else {
                *result = vm_make_bool(false);
            }
        }
        return true;
    }
    
    // os.getcd — get current working directory
    if (strcmp(name, "os.getcd") == 0) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            *result = vm_make_string(cwd);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    
    // os.setcd — change current working directory
    if (strcmp(name, "os.setcd") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            *result = vm_make_bool(chdir(args[0].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    
    // os.mkdir — create a directory
    if (strcmp(name, "os.mkdir") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            #ifdef _WIN32
                *result = vm_make_bool(_mkdir(args[0].string->chars) == 0);
            #else
                *result = vm_make_bool(mkdir(args[0].string->chars, 0755) == 0);
            #endif
        }
        return true;
    }
    
    // os.rmdir — remove an empty directory
    if (strcmp(name, "os.rmdir") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            *result = vm_make_bool(rmdir(args[0].string->chars) == 0);
        }
        return true;
    }
    
    // os.rmfile — delete a file
    if (strcmp(name, "os.rmfile") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            *result = vm_make_bool(unlink(args[0].string->chars) == 0);
        }
        return true;
    }
    
    // os.mkfile — create an empty file
    if (strcmp(name, "os.mkfile") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            FILE* f = fopen(args[0].string->chars, "w");
            if (f) {
                fclose(f);
                *result = vm_make_bool(true);
            } else {
                *result = vm_make_bool(false);
            }
        }
        return true;
    }
    
    // os.rnfile (Rename File)
    if (strcmp(name, "os.rnfile") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_file = stat(args[0].string->chars, &st) == 0 && S_ISREG(st.st_mode);
            *result = vm_make_bool(is_file && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
    
    // os.rndir (Rename Directory)
    if (strcmp(name, "os.rndir") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_dir = stat(args[0].string->chars, &st) == 0 && S_ISDIR(st.st_mode);
            *result = vm_make_bool(is_dir && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.mvfile (Move File)
    if (strcmp(name, "os.mvfile") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_file = stat(args[0].string->chars, &st) == 0 && S_ISREG(st.st_mode);
            *result = vm_make_bool(is_file && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.mvdir (Move Directory)
    if (strcmp(name, "os.mvdir") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            bool is_dir = stat(args[0].string->chars, &st) == 0 && S_ISDIR(st.st_mode);
            *result = vm_make_bool(is_dir && rename(args[0].string->chars, args[1].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.dirsize — calculate total size of directory contents recursively
    if (strcmp(name, "os.dirsize") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            double total_size = 0;
            
            DIR* dir = opendir(args[0].string->chars);
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                        continue;
                    }
                    
                    char full_path[4096];
                    snprintf(full_path, sizeof(full_path), "%s/%s", args[0].string->chars, entry->d_name);
                    
                    struct stat st;
                    if (stat(full_path, &st) == 0) {
                        if (S_ISDIR(st.st_mode)) {
                            Value subdir_result;
                            Value subdir_arg = vm_make_string(full_path);
                            if (os_call_builtin(vm, "os.dirsize", 1, &subdir_arg, &subdir_result)) {
                                if (subdir_result.type == VAL_NUMBER) {
                                    total_size += subdir_result.number;
                                }
                            }
                        } else if (S_ISREG(st.st_mode)) {
                            total_size += (double)st.st_size;
                        }
                    }
                }
                closedir(dir);
                *result = vm_make_number(total_size);
            } else {
                // Директория не существует — возвращаем false
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.cpfile — copy a file from source to destination
    if (strcmp(name, "os.cpfile") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            FILE* src = fopen(args[0].string->chars, "rb");
            if (!src) {
                *result = vm_make_bool(false);
                return true;
            }
            
            FILE* dst = fopen(args[1].string->chars, "wb");
            if (!dst) {
                fclose(src);
                *result = vm_make_bool(false);
                return true;
            }
            
            char buffer[8192];
            size_t bytes_read;
            bool success = true;
            
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                size_t bytes_written = fwrite(buffer, 1, bytes_read, dst);
                if (bytes_written != bytes_read) {
                    success = false;
                    break;
                }
            }
            
            fclose(src);
            fclose(dst);
            
            *result = vm_make_bool(success);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.cpdir — copy a directory recursively from source to destination
    if (strcmp(name, "os.cpdir") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            struct stat st;
            if (stat(args[0].string->chars, &st) != 0 || !S_ISDIR(st.st_mode)) {
                *result = vm_make_bool(false);
                return true;
            }
            
            mkdir(args[1].string->chars, 0755);
            
            bool success = true;
            
            DIR* dir = opendir(args[0].string->chars);
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                        continue;
                    }
                    
                    char src_path[4096], dst_path[4096];
                    snprintf(src_path, sizeof(src_path), "%s/%s", args[0].string->chars, entry->d_name);
                    snprintf(dst_path, sizeof(dst_path), "%s/%s", args[1].string->chars, entry->d_name);
                    
                    struct stat entry_st;
                    if (stat(src_path, &entry_st) != 0) {
                        success = false;
                        break;
                    }
                    
                    Value cp_result;
                    Value cp_args[] = {vm_make_string(src_path), vm_make_string(dst_path)};
                    
                    if (S_ISDIR(entry_st.st_mode)) {
                        if (!os_call_builtin(vm, "os.cpdir", 2, cp_args, &cp_result) ||
                            (cp_result.type == VAL_BOOL && !cp_result.boolean)) {
                            success = false;
                            break;
                        }
                    } else if (S_ISREG(entry_st.st_mode)) {
                        if (!os_call_builtin(vm, "os.cpfile", 2, cp_args, &cp_result) ||
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
            
            *result = vm_make_bool(success);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.filetype (Self-made magic byte detector)
    if (strcmp(name, "os.filetype") == 0) {
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

    // os.listdir — list directory contents, returns a 1-indexed table
    if (strcmp(name, "os.listdir") == 0) {
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
            }
        #endif
        
        return true;
    }
    
    // os.stat — get file/directory metadata as a table
    if (strcmp(name, "os.stat") == 0) {
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
        }
        return true;
    }
    
    // os.platform — return the operating system name
    if (strcmp(name, "os.platform") == 0) {
        #ifdef _WIN32
            *result = vm_make_string("Windows");
        #elif __APPLE__
            *result = vm_make_string("macOS");
        #elif __linux__
            *result = vm_make_string("Linux");
        #else
            *result = vm_make_string("Unknown OS");
        #endif
        return true;
    }
    
    // os.filesize — get file size in bytes
    if (strcmp(name, "os.filesize") == 0) {
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

    // os.kill
    if (strcmp(name, "os.kill") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            int pid = (int)args[0].number;
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
            *result = vm_make_bool(success);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.pid — get current process ID
    if (strcmp(name, "os.pid") == 0) {
        #ifdef _WIN32
            *result = vm_make_number(GetCurrentProcessId());
        #else
            *result = vm_make_number(getpid());
        #endif
        return true;
    }

    // os.getenv — get environment variable
    if (strcmp(name, "os.getenv") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            char* env = getenv(args[0].string->chars);
            if (env) {
                *result = vm_make_string(env);
            } else {
                *result = vm_make_bool(false);
            }
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.setenv — set environment variable
    if (strcmp(name, "os.setenv") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            #ifdef _WIN32
                char env_str[8192];
                snprintf(env_str, sizeof(env_str), "%s=%s", args[0].string->chars, args[1].string->chars);
                *result = vm_make_bool(_putenv(env_str) == 0);
            #else
                *result = vm_make_bool(setenv(args[0].string->chars, args[1].string->chars, 1) == 0);
            #endif
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.env — return all environment variables as a table
    if (strcmp(name, "os.env") == 0) {
        *result = vm_make_table();
        #ifdef _WIN32
            char* env_block = GetEnvironmentStrings();
            if (env_block) {
                char* env = env_block;
                while (*env) {
                    char* eq = strchr(env, '=');
                    if (eq) {
                        *eq = '\0';
                        table_set(result->table, env, vm_make_string(eq + 1));
                        *eq = '=';
                    }
                    env += strlen(env) + 1;
                }
                FreeEnvironmentStrings(env_block);
            }
        #else
            extern char** environ;
            if (environ) {
                for (char** env = environ; *env; env++) {
                    char* eq = strchr(*env, '=');
                    if (eq) {
                        *eq = '\0';
                        table_set(result->table, *env, vm_make_string(eq + 1));
                        *eq = '=';
                    }
                }
            }
        #endif
        return true;
    }

    // os.append — append to file
    if (strcmp(name, "os.append") == 0) {
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

    // os.chmod — change file permissions
    if (strcmp(name, "os.chmod") == 0) {
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

    // os.spawn — create new process, return PID
    if (strcmp(name, "os.spawn") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            #ifdef _WIN32
                STARTUPINFO si = { sizeof(STARTUPINFO) };
                PROCESS_INFORMATION pi = { 0 };
                char cmd[8192];
                snprintf(cmd, sizeof(cmd), "cmd /c %s", args[0].string->chars);
                if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                    CloseHandle(pi.hThread);
                    *result = vm_make_number((double)pi.dwProcessId);
                } else {
                    *result = vm_make_bool(false);
                }
            #else
                pid_t pid = fork();
                if (pid == 0) {
                    execl("/bin/sh", "sh", "-c", args[0].string->chars, (char*)NULL);
                    _exit(127);
                } else if (pid > 0) {
                    *result = vm_make_number((double)pid);
                } else {
                    *result = vm_make_bool(false);
                }
            #endif
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.waitpid — wait for specific process
    if (strcmp(name, "os.waitpid") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            int pid = (int)args[0].number;
            #ifdef _WIN32
                HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
                if (hProcess) {
                    WaitForSingleObject(hProcess, INFINITE);
                    DWORD exit_code;
                    if (GetExitCodeProcess(hProcess, &exit_code)) {
                        *result = vm_make_number((double)exit_code);
                    } else {
                        *result = vm_make_bool(false);
                    }
                    CloseHandle(hProcess);
                } else {
                    *result = vm_make_bool(false);
                }
            #else
                int status;
                if (waitpid(pid, &status, 0) > 0) {
                    if (WIFEXITED(status)) {
                        *result = vm_make_number((double)WEXITSTATUS(status));
                    } else if (WIFSIGNALED(status)) {
                        *result = vm_make_number(-1);
                    } else {
                        *result = vm_make_number(-1);
                    }
                } else {
                    *result = vm_make_bool(false);
                }
            #endif
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.hostname — system hostname
    if (strcmp(name, "os.hostname") == 0) {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            *result = vm_make_string(hostname);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // os.user — current username
    if (strcmp(name, "os.user") == 0) {
        #ifdef _WIN32
            char username[256];
            DWORD size = sizeof(username);
            if (GetUserName(username, &size)) {
                *result = vm_make_string(username);
            } else {
                *result = vm_make_bool(false);
            }
        #else
            char* username = getenv("USER");
            if (!username) username = getenv("LOGNAME");
            if (username) {
                *result = vm_make_string(username);
            } else {
                *result = vm_make_bool(false);
            }
        #endif
        return true;
    }

    // os.homedir — home directory
    if (strcmp(name, "os.homedir") == 0) {
        #ifdef _WIN32
            char* home = getenv("USERPROFILE");
            if (!home) {
                char* drive = getenv("HOMEDRIVE");
                char* path = getenv("HOMEPATH");
                if (drive && path) {
                    char combined[512];
                    snprintf(combined, sizeof(combined), "%s%s", drive, path);
                    home = combined;
                }
            }
            if (home) {
                *result = vm_make_string(home);
            } else {
                *result = vm_make_bool(false);
            }
        #else
            char* home = getenv("HOME");
            if (home) {
                *result = vm_make_string(home);
            } else {
                *result = vm_make_bool(false);
            }
        #endif
        return true;
    }

    return false;
}
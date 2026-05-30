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
    
    // os.getcwd — get current working directory
    if (strcmp(name, "os.getcwd") == 0) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            *result = vm_make_string(cwd);
        } else {
            *result = vm_make_string("");
        }
        return true;
    }
    
    // os.chdir — change current working directory
    if (strcmp(name, "os.chdir") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            *result = vm_make_bool(chdir(args[0].string->chars) == 0);
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
    
    // os.rename — rename a file or directory
    if (strcmp(name, "os.rename") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
            *result = vm_make_bool(rename(args[0].string->chars, args[1].string->chars) == 0);
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
    
    // os.close — stub
    if (strcmp(name, "os.close") == 0) {
        *result = vm_make_bool(true);
        return true;
    }
    
    return false;
}
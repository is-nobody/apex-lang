// source/libraries/os_module.c
// Implementation of OS Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

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
#define chdir _chdir    // windows chdir wrapper
#define getcwd _getcwd  // windows getcwd wrapper
#define rmdir _rmdir    // windows rmdir wrapper
#define unlink _unlink  // windows unlink wrapper
#define mkdir _mkdir    // windows mkdir wrapper
#define stat _stat      // windows stat wrapper
#define fstat _fstat    // windows fstat wrapper
#define access _access  // windows access wrapper
#ifndef F_OK
#define F_OK 0          // file existence check
#endif
#ifndef S_ISREG
#define S_ISREG(mode) ((mode) & _S_IFREG)  // check if regular file
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode) & _S_IFDIR)  // check if directory
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
    double total_size = 0;                                                         // accumulated size
#ifdef _WIN32
    WIN32_FIND_DATA fd;                                                            // file find data
    HANDLE hFind;                                                                  // find handle
    char search_path[4096];                                                        // search path buffer
    snprintf(search_path, sizeof(search_path), "%s\\*", path);                     // build search pattern
    hFind = FindFirstFile(search_path, &fd);                                       // start directory scan
    if (hFind != INVALID_HANDLE_VALUE) {                                           // scan succeeded
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;  // skip special entries
            char full_path[4096];                                                  // full path buffer
            snprintf(full_path, sizeof(full_path), "%s\\%s", path, fd.cFileName);  // build full path
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {                  // is directory
                total_size += calculate_dir_size(full_path);                       // recurse into subdirectory
            } else {
                LARGE_INTEGER size;                                                // file size
                size.LowPart = fd.nFileSizeLow;                                    // low 32 bits
                size.HighPart = fd.nFileSizeHigh;                                  // high 32 bits
                total_size += (double)size.QuadPart;                               // add file size
            }
        } while (FindNextFile(hFind, &fd));                                        // next entry
        FindClose(hFind);                                                          // close find handle
    }
#else
    DIR* dir = opendir(path);                                                      // open directory
    if (dir) {                                                                     // opened successfully
        struct dirent* entry;                                                      // directory entry
        while ((entry = readdir(dir)) != NULL) {                                   // iterate entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;  // skip special
            char full_path[4096];                                                  // full path buffer
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);  // build full path
            struct stat st;                                                        // stat buffer
            if (stat(full_path, &st) == 0) {                                       // get file info
                if (S_ISDIR(st.st_mode)) {                                         // is directory
                    total_size += calculate_dir_size(full_path);                   // recurse
                } else if (S_ISREG(st.st_mode)) {                                  // is regular file
                    total_size += (double)st.st_size;                              // add file size
                }
            }
        }
        closedir(dir);                                                             // close directory
    }
#endif
    return total_size;                                                             // return total size
}

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                                    // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));                // intern and box as value
}

// dispatcher for operating system built-in functions
bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "os.output") == 0) {                             // print to stdout
        if (arg_count >= 1) {
            vm_print_value(args[0]);                                  // print value
            printf("\n");                                             // newline
            fflush(stdout);                                           // flush output
        }
        *result = MAKE_NONE();                                        // return none
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "os.input") == 0) {                              // read from stdin
        if (arg_count >= 1 && IS_STRING(args[0])) {                   // optional prompt
            printf("%s", AS_STRING(args[0])->chars);                  // print prompt
            fflush(stdout);                                           // flush output
        }
        char buffer[4096];                                            // input buffer
        if (fgets(buffer, sizeof(buffer), stdin)) {                   // read line
            buffer[strcspn(buffer, "\r\n")] = 0;                      // strip newline
            *result = make_string_val(vm, buffer);                    // return string
        } else {
            *result = make_string_val(vm, "");                        // empty on eof
        }
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "os.wait") == 0) {                               // sleep for seconds
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                   // validate time
            double seconds = AS_NUMBER(args[0]);                      // extract seconds
            if (seconds < 0) seconds = 0;                             // clamp negative
#ifdef _WIN32
            Sleep((DWORD)(seconds * 1000));                           // windows sleep in ms
#else
            struct timespec ts;                                       // posix timespec
            ts.tv_sec = (time_t)seconds;                              // seconds
            ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1000000000);  // nanoseconds
            nanosleep(&ts, NULL);                                     // sleep
#endif
        }
        *result = MAKE_NONE();                                        // return none
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "os.exit") == 0) {                                                 // exit process
        int code = 0;                                                                   // exit code
        if (arg_count >= 1 && IS_NUMBER(args[0])) code = (int)AS_NUMBER(args[0]);       // extract code
        exit(code);                                                                     // exit with code
        return true;                                                                    // builtin handled
    }
    
    if (strcmp(name, "os.current_folder") == 0) {                             // get current working directory
        char cwd[4096];                                                           // buffer for cwd
        if (getcwd(cwd, sizeof(cwd))) *result = make_string_val(vm, cwd);         // return cwd
        else *result = MAKE_NONE();                                               // failed
        return true;                                                              // builtin handled
    }
    
    if (strcmp(name, "os.change_folder") == 0) {                             // change directory
        if (arg_count >= 1 && IS_STRING(args[0])) {                               // validate path
            *result = MAKE_BOOL(chdir(AS_STRING(args[0])->chars) == 0);           // change and return status
        } else {
            *result = MAKE_BOOL(false);                                           // invalid argument
        }
        return true;                                                              // builtin handled
    }
    
    if (strcmp(name, "os.kill") == 0) {                              // terminate process by pid
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                               // validate pid
            int pid = (int)AS_NUMBER(args[0]);                                    // extract pid
            bool success = false;                                                 // success flag
#ifdef _WIN32
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);  // open process
            if (hProcess) {                                                       // opened
                success = TerminateProcess(hProcess, 1) != 0;                     // terminate
                CloseHandle(hProcess);                                            // close handle
            }
#else
            success = (kill((pid_t)pid, SIGTERM) == 0);         // send SIGTERM
#endif
            *result = MAKE_BOOL(success);                       // return status
        } else {
            *result = MAKE_BOOL(false);                         // invalid argument
        }
        return true;                                            // builtin handled
    }
    
    if (strcmp(name, "os.execute") == 0) {                      // execute shell command
        if (arg_count >= 1 && IS_STRING(args[0])) {             // validate command
            int exit_code = system(AS_STRING(args[0])->chars);  // execute command
            *result = MAKE_NUMBER(exit_code);                   // return exit code
        } else {
            *result = MAKE_NONE();                              // invalid argument
        }
        return true;                                            // builtin handled
    }

    if (strcmp(name, "os.read") == 0) {                         // read file content
        if (arg_count >= 1 && IS_STRING(args[0])) {             // validate path
            FILE* f = fopen(AS_STRING(args[0])->chars, "rb");   // open file binary
            if (f) {                                            // opened
                fseek(f, 0, SEEK_END);                          // seek end
                long size = ftell(f);                           // get size
                fseek(f, 0, SEEK_SET);                          // seek start
                char* buffer = (char*)malloc(size + 1);         // allocate buffer
                size_t read_bytes = fread(buffer, 1, size, f);  // read content
                (void)read_bytes;                               // suppress warning
                buffer[size] = '\0';                            // null terminate
                fclose(f);                                      // close file
                *result = make_string_val(vm, buffer);          // return content
                free(buffer);                                   // free buffer
            } else {
                *result = MAKE_NONE();                          // file not found
            }
        } else {
            *result = MAKE_NONE();                              // invalid argument
        }
        return true;                                            // builtin handled
    }
    
    if (strcmp(name, "os.write") == 0) {                                   // write file content
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {  // validate path and content
            FILE* f = fopen(AS_STRING(args[0])->chars, "wb");              // open file binary write
            if (f) {                                                       // opened
                fputs(AS_STRING(args[1])->chars, f);                       // write content
                fclose(f);                                                 // close file
                *result = MAKE_BOOL(true);                                 // success
            } else {
                *result = MAKE_BOOL(false);                                // failed
            }
        } else {
            *result = MAKE_BOOL(false);                                    // invalid argument
        }
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "os.append") == 0) {                                  // append to file
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {  // validate path and content
            FILE* f = fopen(AS_STRING(args[0])->chars, "ab");              // open file binary append
            if (f) {                                                       // opened
                fputs(AS_STRING(args[1])->chars, f);                       // append content
                fclose(f);                                                 // close file
                *result = MAKE_BOOL(true);                                 // success
            } else {
                *result = MAKE_BOOL(false);                                // failed
            }
        } else {
            *result = MAKE_BOOL(false);                                    // invalid argument
        }
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "os.exists") == 0) {                                       // check if path exists
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
            *result = MAKE_BOOL(access(AS_STRING(args[0])->chars, F_OK) == 0);  // check existence
        } else {
            *result = MAKE_BOOL(false);                                   // invalid argument
        }
        return true;                                                      // builtin handled
    }
    
    if (strcmp(name, "os.isfile") == 0) {                                 // check if path is file
        if (arg_count >= 1 && IS_STRING(args[0])) {                       // validate path
            struct stat st;                                               // stat buffer
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {              // get stats
                *result = MAKE_BOOL(S_ISREG(st.st_mode) ? true : false);  // check regular file
            } else {
                *result = MAKE_BOOL(false);                               // stat failed
            }
        } else {
            *result = MAKE_BOOL(false);                                   // invalid argument
        }
        return true;                                                      // builtin handled
    }
    
    if (strcmp(name, "os.isfolder") == 0) {                               // check if path is directory
        if (arg_count >= 1 && IS_STRING(args[0])) {                       // validate path
            struct stat st;                                               // stat buffer
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {              // get stats
                *result = MAKE_BOOL(S_ISDIR(st.st_mode) ? true : false);  // check directory
            } else {
                *result = MAKE_BOOL(false);                               // stat failed
            }
        } else {
            *result = MAKE_BOOL(false);                                                 // invalid argument
        }
        return true;                                                                    // builtin handled
    }
    
    if (strcmp(name, "os.size") == 0) {                                                    // get file/dir size
        if (arg_count >= 1 && IS_STRING(args[0])) {                                        // validate path
            struct stat st;                                                                // stat buffer
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {                               // get stats
                if (S_ISREG(st.st_mode)) {                                                 // regular file
                    *result = MAKE_NUMBER((double)st.st_size);                             // return file size
                } else if (S_ISDIR(st.st_mode)) {                                          // directory
                    *result = MAKE_NUMBER(calculate_dir_size(AS_STRING(args[0])->chars));  // compute size
                } else {
                    *result = MAKE_NONE();  // unknown type
                }
            } else {
                *result = MAKE_NONE();      // stat failed
            }
        } else {
            *result = MAKE_NONE();          // invalid argument
        }
        return true;                        // builtin handled
    }
    
    if (strcmp(name, "os.create_file") == 0) {                                  // create empty file
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
            FILE* f = fopen(AS_STRING(args[0])->chars, "w");                    // create file
            if (f) {                                                            // created
                fclose(f);                                                      // close
                *result = MAKE_BOOL(true);                                      // success
            } else {
                *result = MAKE_BOOL(false);                                     // failed
            }
        } else {
            *result = MAKE_BOOL(false);                                         // invalid argument
        }
        return true;                                                            // builtin handled
    }
    
    if (strcmp(name, "os.create_folder") == 0) {                                // create directory
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
#ifdef _WIN32
            *result = MAKE_BOOL(_mkdir(AS_STRING(args[0])->chars) == 0);        // create directory
#else
            *result = MAKE_BOOL(mkdir(AS_STRING(args[0])->chars, 0755) == 0);   // create directory with permissions
#endif
        } else {
            *result = MAKE_BOOL(false);                                         // invalid argument
        }
        return true;                                                            // builtin handled
    }
    
    if (strcmp(name, "os.delete") == 0) {                                       // delete file or directory
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
            struct stat st;                                                     // stat buffer
            if (stat(AS_STRING(args[0])->chars, &st) == 0) {                    // get stats
                bool success = false;                                           // success flag
                if (S_ISDIR(st.st_mode)) {                                      // is directory
                    success = (rmdir(AS_STRING(args[0])->chars) == 0);          // remove directory
                } else {
                    success = (unlink(AS_STRING(args[0])->chars) == 0);         // remove file
                }
                *result = MAKE_BOOL(success);                                   // return status
            } else {
                *result = MAKE_BOOL(false);                                     // stat failed
            }
        } else {
            *result = MAKE_BOOL(false);                                         // invalid argument
        }
        return true;                                                            // builtin handled
    }
    
    if (strcmp(name, "os.rename") == 0) {                                       // rename file/dir
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {       // validate paths
            *result = MAKE_BOOL(rename(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars) == 0);  // rename
        } else {
            *result = MAKE_BOOL(false);                                         // invalid argument
        }
        return true;                                                            // builtin handled
    }
    
    if (strcmp(name, "os.move") == 0) {                                         // move file/dir
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {       // validate paths
            *result = MAKE_BOOL(rename(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars) == 0);  // rename
        } else {
            *result = MAKE_BOOL(false);                                         // invalid argument
        }
        return true;                                                            // builtin handled
    }
    
    if (strcmp(name, "os.copy") == 0) {                                         // copy file/directory recursively
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {       // validate paths
            const char* src_path = AS_STRING(args[0])->chars;                   // source path
            const char* dst_path = AS_STRING(args[1])->chars;                   // destination path
            
            struct stat st;                                                     // stat buffer
            if (stat(src_path, &st) != 0) {                                     // source not found
                *result = MAKE_BOOL(false);                                     // return false
                return true;                                                    // builtin handled
            }

            bool success = true;                                                // success flag

            if (S_ISDIR(st.st_mode)) {                                          // source is directory
#ifdef _WIN32
                if (_mkdir(dst_path) != 0 && errno != EEXIST) {                 // create destination
                    *result = MAKE_BOOL(false);                                 // failed
                    return true;                                                // builtin handled
                }
#else
                if (mkdir(dst_path, 0755) != 0 && errno != EEXIST) {            // create destination
                    *result = MAKE_BOOL(false);                                 // failed
                    return true;                                                // builtin handled
                }
#endif
#ifdef _WIN32
                WIN32_FIND_DATA fd;                                             // find data
                HANDLE hFind;                                                   // find handle
                char search_path[4096];                                         // search pattern
                snprintf(search_path, sizeof(search_path), "%s\\*", src_path);  // build pattern
                hFind = FindFirstFile(search_path, &fd);                        // start scan
                if (hFind != INVALID_HANDLE_VALUE) {                            // scan succeeded
                    do {
                        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;  // skip special
                        char sub_src[4096], sub_dst[4096];                                                // sub paths
                        snprintf(sub_src, sizeof(sub_src), "%s\\%s", src_path, fd.cFileName);             // source
                        snprintf(sub_dst, sizeof(sub_dst), "%s\\%s", dst_path, fd.cFileName);             // dest
                        
                        Value sub_result;                                                 // recursive result
                        Value sub_args[2];                                                // recursive args
                        sub_args[0] = make_string_val(vm, sub_src);                       // source arg
                        sub_args[1] = make_string_val(vm, sub_dst);                       // dest arg
                        if (!os_call_builtin(vm, "os.copy", 2, sub_args, &sub_result) ||  // recursive copy
                            (IS_BOOL(sub_result) && !AS_BOOL(sub_result))) {              // failed
                            success = false;                                              // mark failure
                            value_decref(sub_args[0]);                                    // release source
                            value_decref(sub_args[1]);                                    // release dest
                            break;                                                        // exit loop
                        }
                        value_decref(sub_args[0]);                                        // release source
                        value_decref(sub_args[1]);                                        // release dest
                    } while (FindNextFile(hFind, &fd));                                   // next entry
                    FindClose(hFind);                                                     // close find
                } else {
                    success = false;                                                      // scan failed
                }
#else
                DIR* dir = opendir(src_path);                                                               // open directory
                if (dir) {                                                                                  // opened
                    struct dirent* entry;                                                                   // directory entry
                    while ((entry = readdir(dir)) != NULL) {                                                // iterate entries
                        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;  // skip
                        char sub_src[4096], sub_dst[4096];                                                  // sub paths
                        snprintf(sub_src, sizeof(sub_src), "%s/%s", src_path, entry->d_name);               // source
                        snprintf(sub_dst, sizeof(sub_dst), "%s/%s", dst_path, entry->d_name);               // dest
                        
                        struct stat entry_st;                                             // entry stat
                        if (stat(sub_src, &entry_st) != 0) { success = false; break; }    // stat failed

                        Value sub_result;                                                 // recursive result
                        Value sub_args[2];                                                // recursive args
                        sub_args[0] = make_string_val(vm, sub_src);                       // source arg
                        sub_args[1] = make_string_val(vm, sub_dst);                       // dest arg
                        if (!os_call_builtin(vm, "os.copy", 2, sub_args, &sub_result) ||  // recursive copy
                            (IS_BOOL(sub_result) && !AS_BOOL(sub_result))) {              // failed
                            success = false;                                              // mark failure
                            value_decref(sub_args[0]);  // release source
                            value_decref(sub_args[1]);  // release dest
                            break;                      // exit loop
                        }
                        value_decref(sub_args[0]);      // release source
                        value_decref(sub_args[1]);      // release dest
                    }
                    closedir(dir);                      // close directory
                } else {
                    success = false;                    // open failed
                }
#endif
            } else {                                    // source is file
                FILE* src = fopen(src_path, "rb");      // open source
                if (!src) {                             // open failed
                    *result = MAKE_BOOL(false);         // return false
                    return true;                        // builtin handled
                }
                FILE* dst = fopen(dst_path, "wb");      // open dest
                if (!dst) {                             // open failed
                    fclose(src);                        // close source
                    *result = MAKE_BOOL(false);         // return false
                    return true;                        // builtin handled
                }
                char buffer[8192];                                                  // copy buffer
                size_t bytes_read;                                                  // bytes read
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {  // read chunk
                    if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) {         // write failed
                        success = false;                                            // mark failure
                        break;                                                      // exit loop
                    }
                }
                fclose(src);               // close source
                fclose(dst);               // close dest
            }
            *result = MAKE_BOOL(success);  // return status
        } else {
            *result = MAKE_BOOL(false);    // invalid arguments
        }
        return true;                       // builtin handled
    }
    
    if (strcmp(name, "os.items") == 0) {                              // list directory contents
        const char* path = ".";                                       // default to current
        if (arg_count >= 1 && IS_STRING(args[0])) {                   // path provided
            path = AS_STRING(args[0])->chars;                         // use provided path
        }
        
    #ifdef _WIN32
        WIN32_FIND_DATA fd;                                           // find data
        HANDLE hFind;                                                 // find handle
        char search_path[4096];                                       // search pattern
        snprintf(search_path, sizeof(search_path), "%s\\*", path);    // build pattern
        hFind = FindFirstFile(search_path, &fd);                      // start scan
        if (hFind != INVALID_HANDLE_VALUE) {                          // scan succeeded
            Table* t = table_create(8);                               // create result table
            *result = MAKE_TABLE(t);                                  // box table
            int idx = 1;                                              // index counter
            do {
                Value k = MAKE_NUMBER((double)idx++);                 // create index key
                table_set(t, k, make_string_val(vm, fd.cFileName));   // store item name
                value_decref(k);                                      // release key
            } while (FindNextFile(hFind, &fd));                       // next entry
            FindClose(hFind);                                         // close find
        } else {
            *result = MAKE_NONE();                                    // scan failed
        }
#else
        DIR* dir = opendir(path);                                     // open directory
        if (dir) {                                                    // opened
            Table* t = table_create(8);                               // create result table
            *result = MAKE_TABLE(t);                                  // box table
            struct dirent* entry;                                     // directory entry
            int idx = 1;                                              // index counter
            while ((entry = readdir(dir)) != NULL) {                  // iterate entries
                Value k = MAKE_NUMBER((double)idx++);                 // create index key
                table_set(t, k, make_string_val(vm, entry->d_name));  // store item name
                value_decref(k);                                      // release key
            }
            closedir(dir);                                            // close directory
        } else {
            *result = MAKE_NONE();                                    // open failed
        }
#endif
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "os.parentfolder") == 0) {            // get parent directory path
        if (arg_count >= 1 && IS_STRING(args[0])) {        // validate path
            const char* path = AS_STRING(args[0])->chars;  // extract path
            char* last_sep = strrchr(path, '/');           // find forward slash
#ifdef _WIN32
            char* last_sep_win = strrchr(path, '\\');                      // find backslash
            if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {  // use later separator
                last_sep = last_sep_win;                                   // use backslash
            }
#endif
            if (last_sep) {                                                      // found separator
                int len = last_sep - path;                                       // parent length
                if (len == 0) {                                                  // root path
                    if (path[0] == '/') {                                        // unix root
                        *result = make_string_val(vm, "/");                      // return root
                    } else {                                                     // windows root
                        char root[4];                                            // root buffer
                        snprintf(root, sizeof(root), "%c%c", path[0], path[1]);  // drive letter
                        *result = make_string_val(vm, root);                     // return root
                    }
                } else {
                    char* parent = (char*)malloc(len + 1);  // allocate parent
                    strncpy(parent, path, len);             // copy parent
                    parent[len] = '\0';                     // null terminate
                    *result = make_string_val(vm, parent);  // return parent
                    free(parent);                           // free buffer
                }
            } else {
                *result = make_string_val(vm, ".");         // no parent, return current
            }
        } else {
            *result = MAKE_NONE();                          // invalid argument
        }
        return true;                                        // builtin handled
    }
    
    if (strcmp(name, "os.access") == 0) {                                  // change file permissions
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_NUMBER(args[1])) {  // validate path and mode
#ifdef _WIN32
            *result = MAKE_BOOL(_chmod(AS_STRING(args[0])->chars, (int)AS_NUMBER(args[1])) == 0);   // chmod on windows
#else
            *result = MAKE_BOOL(chmod(AS_STRING(args[0])->chars, (mode_t)AS_NUMBER(args[1])) == 0); // chmod on unix
#endif
        } else {
            *result = MAKE_BOOL(false);  // invalid argument
        }
        return true;  // builtin handled
    }

    if (strcmp(name, "os.args") == 0) {
        *result = vm->args_table;  // copy the tagged value
        value_incref(*result);     // bump refcount for caller ownership
        return true;               // builtin handled
    }

    return false;     // not a recognized builtin
}
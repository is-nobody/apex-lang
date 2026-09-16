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
#include <process.h>
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

// normalize path separators for windows filesystem
static void normalize_path(char* path) {
#ifdef _WIN32
    for (char* p = path; *p; p++) {          // iterate all characters
        if (*p == '/') *p = '\\';            // replace forward slash
    }
#else
    (void)path;                              // suppress unused warning
#endif
}

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

// helper to create an interned string value (VM-thread only; never from a worker)
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                                    // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));                // intern and box as value
}

// helper to create a fresh non-interned string value (safe from worker threads)
static Value make_owned_string(const char* str, int len) {
    return MAKE_STRING(string_create(str, len));                                   // fresh refcounted string, no VM access
}

// create a leaf future ready to be resolved by a background worker
static FutureObject* os_make_leaf_future(void) {
    FutureObject* fut = (FutureObject*)calloc(1, sizeof(FutureObject));  // zero-init for safety on failure paths
    fut->header.ref_count = 1;                   // caller holds one reference
    fut->header.type = VAL_FUTURE;               // mark type as future
    fut->result = MAKE_NONE();                   // filled in when resolved
    fut->state = 0;                              // pending
    fut->func_idx = -1;                          // leaf: no coroutine body
    fut->arg_count = 0;                          // no captured args
    fut->args = NULL;                            // no args array
    fut->frame_idx = -1;                         // not started
    fut->owns_frame = false;                     // no frame owned
    fut->saved_ip = 0;                           // unused for leaf
    fut->saved_dest_reg = -1;                    // unused for leaf
    fut->awaiting = MAKE_NONE();                 // not awaiting
    fut->waiters = NULL;                         // no waiters yet
    fut->waiter_count = 0;                       // empty
    fut->waiter_capacity = 0;                    // no capacity
    fut->saved_iter_depth = -1;                  // no active loops
    fut->saved_table_iter_depth = -1;            // no table iterators
    return fut;                                  // return fresh future
}

// global stdin mutex, initialized once via platform once-primitive
static ApexMutex g_stdin_mutex;

#ifdef _WIN32
static INIT_ONCE g_stdin_once = INIT_ONCE_STATIC_INIT;    // windows one-time init token
static BOOL CALLBACK os_stdin_once_init(PINIT_ONCE o, PVOID p, PVOID* c) {
    (void)o; (void)p; (void)c;                            // unused parameters
    InitializeCriticalSection(&g_stdin_mutex);            // create the stdin critical section
    return TRUE;                                          // success
}
static void ensure_stdin_mutex(void) {
    InitOnceExecuteOnce(&g_stdin_once, os_stdin_once_init, NULL, NULL);  // run init exactly once
}
#else
static pthread_once_t g_stdin_once = PTHREAD_ONCE_INIT;   // posix one-time init token
static void os_stdin_once_init(void) {
    pthread_mutex_init(&g_stdin_mutex, NULL);             // create the stdin mutex
}
static void ensure_stdin_mutex(void) {
    pthread_once(&g_stdin_once, os_stdin_once_init);      // run init exactly once
}
#endif

// generic task descriptor handed to a worker thread
typedef struct {
    VM* vm;                  // vm pointer for completion push
    FutureObject* fut;       // future to resolve with the task result
    Value (*fn)(void*);      // blocking function executed on the worker
    void (*free_fn)(void*);  // releases the packed argument struct
    void* arg;               // packed argument struct
} OsAsyncTask;

// releases an async task descriptor and its packed arguments
static void os_async_task_destroy(void* p) {
    OsAsyncTask* t = (OsAsyncTask*)p;                 // unpack task descriptor
    if (t->free_fn) t->free_fn(t->arg);               // release argument struct
    free(t);                                          // free descriptor itself
}

// worker thread entry: runs the blocking function and posts the result
static void* os_generic_thread(void* p) {
    OsAsyncTask* t = (OsAsyncTask*)p;                 // unpack task descriptor
    Value result = t->fn(t->arg);                     // run blocking function off the event loop
    vm_push_completion(t->vm, t->fut, result);        // hand off to scheduler
    os_async_task_destroy(t);                         // release descriptor and args
    return NULL;                                      // thread exit
}

// spawn a detached worker thread that runs the blocking task off the main loop
static void os_spawn_worker(VM* vm, FutureObject* fut, void* (*fn)(void*),
                            void* arg, void (*arg_free)(void*)) {
    APEX_MUTEX_LOCK(&vm->completion_mutex);           // reserve a worker slot
    vm->pending_workers++;                            // count pending worker
    APEX_MUTEX_UNLOCK(&vm->completion_mutex);         // release lock

    bool ok = false;                                  // spawn success flag
#ifdef _WIN32
    uintptr_t h = _beginthreadex(NULL, 0,               // windows thread
                                 (unsigned __stdcall (*)(void*))fn,
                                 arg, 0, NULL);
    if (h) { CloseHandle((HANDLE)h); ok = true; }     // detach handle
#else
    pthread_t tid;                                    // posix thread handle
    if (pthread_create(&tid, NULL, fn, arg) == 0) {   // start thread
        pthread_detach(tid);                          // detach
        ok = true;                                    // mark success
    }
#endif

    if (!ok) {                                        // spawn failed
        vm_push_completion(vm, fut, MAKE_NONE());     // complete future with none
        if (arg_free) arg_free(arg);                  // release unused argument
    }
}

// generic packed arguments for the OS async worker tasks
typedef struct {
    char* path;              // primary path, command, or prompt text
    char* path2;             // secondary path (copy/rename/move target)
    char* content;           // string content for write/append operations
    int int_val;             // integer argument (pid)
    double num_val;          // numeric argument (mode bits)
} OsArgs;

// allocates a zero-initialized OsArgs
static OsArgs* os_args_new(void) {
    return (OsArgs*)calloc(1, sizeof(OsArgs));        // zero-init all fields
}

// releases an OsArgs and any owned strings
static void os_args_free(void* p) {
    OsArgs* a = (OsArgs*)p;                           // unpack argument struct
    free(a->path);                                    // release primary path
    free(a->path2);                                   // release secondary path
    free(a->content);                                 // release string content
    free(a);                                          // release struct itself
}

// runs fn asynchronously inside a coroutine, synchronously otherwise
static bool os_run_async_or_sync(VM* vm, Value (*fn)(void*),
                                 void (*free_fn)(void*), void* arg,
                                 Value* result) {
    if (vm->builtin_async) {                          // caller used `await` on this builtin
        FutureObject* fut = os_make_leaf_future();    // fresh pending future
        value_incref(MAKE_FUTURE(fut));               // worker holds one reference

        OsAsyncTask* t = (OsAsyncTask*)malloc(sizeof(OsAsyncTask));  // pack task descriptor
        t->vm = vm;                                   // store vm pointer
        t->fut = fut;                                 // store future
        t->fn = fn;                                   // store blocking function
        t->free_fn = free_fn;                         // store cleanup function
        t->arg = arg;                                 // store argument struct

        os_spawn_worker(vm, fut, os_generic_thread, t, os_async_task_destroy);  // offload
        *result = MAKE_FUTURE(fut);                   // return pending future
    } else {                                          // top level: nothing else is runnable
        *result = fn(arg);                            // run blocking function inline
        if (free_fn) free_fn(arg);                    // release argument struct
    }
    return true;                                      // builtin handled
}

// recursively copies a file or directory tree without touching the VM
static bool os_copy_recursive(const char* src_path, const char* dst_path) {
    struct stat st;                                                             // stat buffer
    if (stat(src_path, &st) != 0) return false;                                 // source missing

    if (S_ISDIR(st.st_mode)) {                                                  // source is a directory
#ifdef _WIN32
        if (_mkdir(dst_path) != 0 && errno != EEXIST) return false;             // create destination
#else
        if (mkdir(dst_path, 0755) != 0 && errno != EEXIST) return false;        // create destination
#endif
#ifdef _WIN32
        WIN32_FIND_DATA fd;                                                     // find data
        char search_path[4096];                                                 // search pattern
        snprintf(search_path, sizeof(search_path), "%s\\*", src_path);          // build pattern
        HANDLE hFind = FindFirstFile(search_path, &fd);                         // start scan
        if (hFind == INVALID_HANDLE_VALUE) return false;                        // scan failed
        bool ok = true;                                                         // overall status
        do {
            if (strcmp(fd.cFileName, ".") == 0 ||
                strcmp(fd.cFileName, "..") == 0) continue;                      // skip special entries
            char sub_src[4096], sub_dst[4096];                                  // sub paths
            snprintf(sub_src, sizeof(sub_src), "%s\\%s", src_path, fd.cFileName);
            snprintf(sub_dst, sizeof(sub_dst), "%s\\%s", dst_path, fd.cFileName);
            if (!os_copy_recursive(sub_src, sub_dst)) { ok = false; break; }    // recurse
        } while (FindNextFile(hFind, &fd));                                     // next entry
        FindClose(hFind);                                                       // close find
        return ok;                                                              // return status
#else
        DIR* dir = opendir(src_path);                                           // open directory
        if (!dir) return false;                                                 // open failed
        bool ok = true;                                                         // overall status
        struct dirent* entry;                                                   // directory entry
        while ((entry = readdir(dir)) != NULL) {                                // iterate entries
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) continue;                    // skip special entries
            char sub_src[4096], sub_dst[4096];                                  // sub paths
            snprintf(sub_src, sizeof(sub_src), "%s/%s", src_path, entry->d_name);
            snprintf(sub_dst, sizeof(sub_dst), "%s/%s", dst_path, entry->d_name);
            if (!os_copy_recursive(sub_src, sub_dst)) { ok = false; break; }    // recurse
        }
        closedir(dir);                                                          // close directory
        return ok;                                                              // return status
#endif
    }

    FILE* src = fopen(src_path, "rb");                                          // open source file
    if (!src) return false;                                                     // open failed
    FILE* dst = fopen(dst_path, "wb");                                          // open destination
    if (!dst) { fclose(src); return false; }                                    // open failed
    char buffer[8192];                                                          // copy buffer
    size_t n;                                                                   // bytes moved
    bool ok = true;                                                             // overall status
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {                   // read chunk
        if (fwrite(buffer, 1, n, dst) != n) { ok = false; break; }              // write chunk
    }
    fclose(src);                                                                // close source
    fclose(dst);                                                                // close destination
    return ok;                                                                  // return status
}

// reads an entire file into a freshly allocated string
static Value os_read_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    FILE* f = fopen(a->path, "rb");                     // open file binary
    if (!f) return MAKE_NONE();                         // file not found
    fseek(f, 0, SEEK_END);                              // seek end
    long size = ftell(f);                               // get size
    fseek(f, 0, SEEK_SET);                              // seek start
    char* buffer = (char*)malloc(size + 1);             // allocate buffer
    if (!buffer) { fclose(f); return MAKE_NONE(); }     // allocation failed
    size_t n = fread(buffer, 1, size, f);               // read content
    buffer[n] = '\0';                                   // null terminate
    fclose(f);                                          // close file
    StringObject* str = string_create(buffer, (int)n);  // fresh non-interned string
    free(buffer);                                       // free temp buffer
    return MAKE_STRING(str);                            // return boxed string
}

// writes content to a file, replacing existing data
static Value os_write_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    FILE* f = fopen(a->path, "wb");                     // open file binary write
    if (!f) return MAKE_BOOL(false);                    // open failed
    fputs(a->content, f);                               // write content
    fclose(f);                                          // close file
    return MAKE_BOOL(true);                             // success
}

// appends content to a file
static Value os_append_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    FILE* f = fopen(a->path, "ab");                     // open file binary append
    if (!f) return MAKE_BOOL(false);                    // open failed
    fputs(a->content, f);                               // append content
    fclose(f);                                          // close file
    return MAKE_BOOL(true);                             // success
}

// runs a shell command and returns the process exit code
static Value os_exec_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    int code = system(a->path);                         // run command (blocks this thread only)
    return MAKE_NUMBER((double)code);                   // return exit code
}

// reads a line from stdin under a global mutex so prompts never interleave
static Value os_input_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    ensure_stdin_mutex();                               // one-time init of the stdin mutex
    APEX_MUTEX_LOCK(&g_stdin_mutex);                    // serialize stdin access across workers
    printf("%s", a->content ? a->content : "");         // print prompt
    fflush(stdout);                                     // flush output
    char buffer[4096];                                  // input buffer
    if (fgets(buffer, sizeof(buffer), stdin)) {         // read line
        buffer[strcspn(buffer, "\r\n")] = 0;            // strip newline
    } else {
        buffer[0] = '\0';                               // empty on eof
    }
    APEX_MUTEX_UNLOCK(&g_stdin_mutex);                  // release stdin mutex
    return make_owned_string(buffer, (int)strlen(buffer));  // fresh non-interned string
}

// returns file or recursive directory size in bytes
static Value os_size_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    struct stat st;                                     // stat buffer
    if (stat(a->path, &st) != 0) return MAKE_NONE();    // stat failed
    if (S_ISREG(st.st_mode)) return MAKE_NUMBER((double)st.st_size);      // regular file size
    if (S_ISDIR(st.st_mode)) return MAKE_NUMBER(calculate_dir_size(a->path));  // directory size
    return MAKE_NONE();                                 // unknown type
}

// recursively copies a file or directory tree
static Value os_copy_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    return MAKE_BOOL(os_copy_recursive(a->path, a->path2));  // return copy status
}

// lists a directory into a 1-indexed table of entry names
static Value os_list_folder_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    const char* path = a->path ? a->path : ".";         // default to current directory
    Table* t = table_create(8);                         // result table
    Value result = MAKE_TABLE(t);                       // box table
    int idx = 1;                                        // 1-based index counter
#ifdef _WIN32
    WIN32_FIND_DATA fd;                                 // find data
    char search_path[4096];                             // search pattern
    snprintf(search_path, sizeof(search_path), "%s\\*", path);  // build pattern
    HANDLE hFind = FindFirstFile(search_path, &fd);     // start scan
    if (hFind == INVALID_HANDLE_VALUE) {                // scan failed
        value_decref(result);                           // destroy the empty table
        return MAKE_NONE();                             // no such directory
    }
    do {
        Value k = MAKE_NUMBER((double)idx++);           // create index key
        Value v = make_owned_string(fd.cFileName, (int)strlen(fd.cFileName));  // fresh entry name
        table_set(t, k, v);                             // store item name
        value_decref(v);                                // table holds its own reference
    } while (FindNextFile(hFind, &fd));                 // next entry
    FindClose(hFind);                                   // close find
#else
    DIR* dir = opendir(path);                           // open directory
    if (!dir) {                                         // open failed
        value_decref(result);                           // destroy the empty table
        return MAKE_NONE();                             // no such directory
    }
    struct dirent* entry;                               // directory entry
    while ((entry = readdir(dir)) != NULL) {            // iterate entries
        Value k = MAKE_NUMBER((double)idx++);           // create index key
        Value v = make_owned_string(entry->d_name, (int)strlen(entry->d_name));  // fresh entry name
        table_set(t, k, v);                             // store item name
        value_decref(v);                                // table holds its own reference
    }
    closedir(dir);                                      // close directory
#endif
    return result;                                      // return populated table
}

// tests whether a path exists
static Value os_exists_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    return MAKE_BOOL(access(a->path, F_OK) == 0);       // return existence status
}

// tests whether a path is a regular file
static Value os_is_file_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    struct stat st;                                     // stat buffer
    if (stat(a->path, &st) != 0) return MAKE_BOOL(false);  // stat failed
    return MAKE_BOOL(S_ISREG(st.st_mode) ? true : false);  // regular file check
}

// tests whether a path is a directory
static Value os_is_folder_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    struct stat st;                                     // stat buffer
    if (stat(a->path, &st) != 0) return MAKE_BOOL(false);  // stat failed
    return MAKE_BOOL(S_ISDIR(st.st_mode) ? true : false);  // directory check
}

// creates an empty file, truncating any existing content
static Value os_create_file_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    FILE* f = fopen(a->path, "w");                      // create or truncate file
    if (!f) return MAKE_BOOL(false);                    // open failed
    fclose(f);                                          // close file
    return MAKE_BOOL(true);                             // success
}

// creates a directory
static Value os_create_folder_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
#ifdef _WIN32
    return MAKE_BOOL(_mkdir(a->path) == 0);             // create directory on windows
#else
    return MAKE_BOOL(mkdir(a->path, 0755) == 0);        // create directory with permissions
#endif
}

// deletes a file or empty directory
static Value os_delete_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    struct stat st;                                     // stat buffer
    if (stat(a->path, &st) != 0) return MAKE_BOOL(false);  // stat failed
    bool success = false;                               // success flag
    if (S_ISDIR(st.st_mode)) success = (rmdir(a->path) == 0);   // remove directory
    else success = (unlink(a->path) == 0);              // remove file
    return MAKE_BOOL(success);                          // return status
}

// renames a file or directory
static Value os_rename_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    return MAKE_BOOL(rename(a->path, a->path2) == 0);   // rename
}

// moves a file or directory
static Value os_move_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
#ifdef _WIN32
    return MAKE_BOOL(MoveFile(a->path, a->path2) != 0); // windows move api
#else
    return MAKE_BOOL(rename(a->path, a->path2) == 0);   // unix rename
#endif
}

// changes the process working directory
static Value os_change_folder_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    return MAKE_BOOL(chdir(a->path) == 0);              // change directory
}

// terminates a process by pid
static Value os_terminate_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
    int pid = a->int_val;                               // extract pid
    bool success = false;                               // success flag
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);  // open process
    if (hProcess) {                                     // opened
        success = TerminateProcess(hProcess, 1) != 0;   // terminate
        CloseHandle(hProcess);                          // close handle
    }
#else
    success = (kill((pid_t)pid, SIGTERM) == 0);         // send SIGTERM
#endif
    return MAKE_BOOL(success);                          // return status
}

// changes permissions on a file
static Value os_access_sync(void* p) {
    OsArgs* a = (OsArgs*)p;                             // unpack argument struct
#ifdef _WIN32
    int unix_mode = a->int_val;                         // extract unix mode bits
    int win_mode = 0;                                   // windows mode flags
    if (unix_mode & 4) win_mode |= _S_IREAD;            // unix read -> windows read
    if (unix_mode & 2) win_mode |= _S_IWRITE;           // unix write -> windows write
    return MAKE_BOOL(_chmod(a->path, win_mode) == 0);   // chmod on windows
#else
    return MAKE_BOOL(chmod(a->path, (mode_t)a->int_val) == 0);  // chmod on unix
#endif
}

// dispatcher for operating system built-in functions
bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "os.output") == 0) {                         // print to stdout
        if (arg_count >= 1) {                                     // require a value argument
            vm_print_value(args[0]);                              // print value

            bool needs_newline = true;                            // append newline by default
            if (IS_STRING(args[0])) {                             // string payload may end with '\n'
                StringObject* s = AS_STRING(args[0]);             // unwrap string object
                if (s->length > 0 && s->chars[s->length - 1] == '\n') {
                    needs_newline = false;                        // already ends with line break
                }
            }
            if (needs_newline) {
                fputc('\n', stdout);                              // newline
            }
            fflush(stdout);                                       // flush output
        }
        *result = MAKE_NONE();                                    // return none
        return true;                                              // builtin handled
    }

    if (strcmp(name, "os.input") == 0) {                          // read from stdin
        OsArgs* a = os_args_new();                                // pack argument struct
        a->content = strdup(arg_count >= 1 && IS_STRING(args[0])
                            ? AS_STRING(args[0])->chars
                            : "");                                // copy prompt text

        // stdin is inherently blocking and shared with the main thread:
        // never offload to a worker, always read synchronously
        *result = os_input_sync(a);                               // synchronous read
        os_args_free(a);                                          // release argument struct
        return true;                                              // builtin handled
    }

    if (strcmp(name, "os.wait") == 0) {                               // sleep for seconds
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                   // validate time
            double seconds = AS_NUMBER(args[0]);                      // extract seconds
            if (seconds < 0) seconds = 0;                             // clamp negative

            if (vm->builtin_async) {                                  // await os.wait(...)
                FutureObject* fut = os_make_leaf_future();            // fresh pending future
                vm_schedule_timer(vm, seconds, fut);                  // timer resolves it later
                *result = MAKE_FUTURE(fut);                           // hand back pending future
                return true;                                          // builtin handled
            }

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
            OsArgs* a = os_args_new();                                            // pack argument struct
            char path[4096];                                                      // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars);        // copy path
            normalize_path(path);                                                 // normalize separators
            a->path = strdup(path);                                               // store normalized path
            return os_run_async_or_sync(vm, os_change_folder_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                               // invalid argument
        return true;                                                              // builtin handled
    }

    if (strcmp(name, "os.terminate") == 0) {                              // terminate process by pid
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                               // validate pid
            OsArgs* a = os_args_new();                                            // pack argument struct
            a->int_val = (int)AS_NUMBER(args[0]);                                 // store pid
            return os_run_async_or_sync(vm, os_terminate_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                               // invalid argument
        return true;                                                              // builtin handled
    }

    if (strcmp(name, "os.execute") == 0) {                      // execute shell command
        if (arg_count >= 1 && IS_STRING(args[0])) {             // validate command
            OsArgs* a = os_args_new();                          // pack argument struct
            a->path = strdup(AS_STRING(args[0])->chars);        // copy command
            return os_run_async_or_sync(vm, os_exec_sync, os_args_free, a, result);
        }
        *result = MAKE_NONE();                                  // invalid argument
        return true;                                            // builtin handled
    }

    if (strcmp(name, "os.read") == 0) {                         // read file content
        if (arg_count >= 1 && IS_STRING(args[0])) {             // validate path
            OsArgs* a = os_args_new();                          // pack argument struct
            char path[4096];                                    // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars);  // copy path
            normalize_path(path);                               // normalize separators
            a->path = strdup(path);                             // store normalized path
            return os_run_async_or_sync(vm, os_read_sync, os_args_free, a, result);
        }
        *result = MAKE_NONE();                                  // invalid argument
        return true;                                            // builtin handled
    }

    if (strcmp(name, "os.write") == 0) {                                   // write file content
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {  // validate path and content
            OsArgs* a = os_args_new();                                     // pack argument struct
            char path[4096];                                               // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars); // copy path
            normalize_path(path);                                          // normalize separators
            a->path = strdup(path);                                        // store normalized path
            a->content = strdup(AS_STRING(args[1])->chars);                // copy content
            return os_run_async_or_sync(vm, os_write_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                        // invalid argument
        return true;                                                       // builtin handled
    }

    if (strcmp(name, "os.append") == 0) {                                  // append to file
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {  // validate path and content
            OsArgs* a = os_args_new();                                     // pack argument struct
            char path[4096];                                               // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars); // copy path
            normalize_path(path);                                          // normalize separators
            a->path = strdup(path);                                        // store normalized path
            a->content = strdup(AS_STRING(args[1])->chars);                // copy content
            return os_run_async_or_sync(vm, os_append_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                        // invalid argument
        return true;                                                       // builtin handled
    }

    if (strcmp(name, "os.exists") == 0) {                                       // check if path exists
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
            OsArgs* a = os_args_new();                                          // pack argument struct
            char path[4096];                                                    // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars);      // copy path
            normalize_path(path);                                               // normalize separators
            a->path = strdup(path);                                             // store normalized path
            return os_run_async_or_sync(vm, os_exists_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                             // invalid argument
        return true;                                                            // builtin handled
    }

    if (strcmp(name, "os.is_file") == 0) {                                 // check if path is file
        if (arg_count >= 1 && IS_STRING(args[0])) {                       // validate path
            OsArgs* a = os_args_new();                                    // pack argument struct
            char path[4096];                                              // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars); // copy path
            normalize_path(path);                                         // normalize separators
            a->path = strdup(path);                                       // store normalized path
            return os_run_async_or_sync(vm, os_is_file_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                       // invalid argument
        return true;                                                      // builtin handled
    }

    if (strcmp(name, "os.is_folder") == 0) {                               // check if path is directory
        if (arg_count >= 1 && IS_STRING(args[0])) {                       // validate path
            OsArgs* a = os_args_new();                                    // pack argument struct
            char path[4096];                                              // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars); // copy path
            normalize_path(path);                                         // normalize separators
            a->path = strdup(path);                                       // store normalized path
            return os_run_async_or_sync(vm, os_is_folder_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                       // invalid argument
        return true;                                                      // builtin handled
    }

    if (strcmp(name, "os.size") == 0) {                                            // get file/dir size
        if (arg_count >= 1 && IS_STRING(args[0])) {                                // validate path
            OsArgs* a = os_args_new();                                             // pack argument struct
            char path[4096];                                                       // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars);         // copy path
            normalize_path(path);                                                  // normalize separators
            a->path = strdup(path);                                                // store normalized path
            return os_run_async_or_sync(vm, os_size_sync, os_args_free, a, result);
        }
        *result = MAKE_NONE();                                                     // invalid argument
        return true;                                                               // builtin handled
    }

    if (strcmp(name, "os.create_file") == 0) {                                  // create empty file
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
            OsArgs* a = os_args_new();                                          // pack argument struct
            char path[4096];                                                    // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars);      // copy path
            normalize_path(path);                                               // normalize separators
            a->path = strdup(path);                                             // store normalized path
            return os_run_async_or_sync(vm, os_create_file_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                             // invalid argument
        return true;                                                            // builtin handled
    }

    if (strcmp(name, "os.create_folder") == 0) {                                // create directory
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
            OsArgs* a = os_args_new();                                          // pack argument struct
            char path[4096];                                                    // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars);      // copy path
            normalize_path(path);                                               // normalize separators
            a->path = strdup(path);                                             // store normalized path
            return os_run_async_or_sync(vm, os_create_folder_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                             // invalid argument
        return true;                                                            // builtin handled
    }

    if (strcmp(name, "os.delete") == 0) {                                       // delete file or directory
        if (arg_count >= 1 && IS_STRING(args[0])) {                             // validate path
            OsArgs* a = os_args_new();                                          // pack argument struct
            char path[4096];                                                    // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars);      // copy path
            normalize_path(path);                                               // normalize separators
            a->path = strdup(path);                                             // store normalized path
            return os_run_async_or_sync(vm, os_delete_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                             // invalid argument
        return true;                                                            // builtin handled
    }

    if (strcmp(name, "os.rename") == 0) {                                       // rename file/dir
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {       // validate paths
            OsArgs* a = os_args_new();                                          // pack argument struct
            char src[4096], dst[4096];                                          // normalized path buffers
            snprintf(src, sizeof(src), "%s", AS_STRING(args[0])->chars);        // copy source
            snprintf(dst, sizeof(dst), "%s", AS_STRING(args[1])->chars);        // copy destination
            normalize_path(src);                                                // normalize source
            normalize_path(dst);                                                // normalize destination
            a->path = strdup(src);                                              // store source
            a->path2 = strdup(dst);                                             // store destination
            return os_run_async_or_sync(vm, os_rename_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                             // invalid argument
        return true;                                                            // builtin handled
    }

    if (strcmp(name, "os.move") == 0) {                                         // move file/dir
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {       // validate paths
            OsArgs* a = os_args_new();                                          // pack argument struct
            char src[4096], dst[4096];                                          // normalized path buffers
            snprintf(src, sizeof(src), "%s", AS_STRING(args[0])->chars);        // copy source
            snprintf(dst, sizeof(dst), "%s", AS_STRING(args[1])->chars);        // copy destination
            normalize_path(src);                                                // normalize source
            normalize_path(dst);                                                // normalize destination
            a->path = strdup(src);                                              // store source
            a->path2 = strdup(dst);                                             // store destination
            return os_run_async_or_sync(vm, os_move_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                             // invalid argument
        return true;                                                            // builtin handled
    }

    if (strcmp(name, "os.copy") == 0) {                                         // copy file/directory recursively
        if (arg_count >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {       // validate paths
            OsArgs* a = os_args_new();                                          // pack argument struct
            char src_path[4096], dst_path[4096];                                // normalized path buffers
            snprintf(src_path, sizeof(src_path), "%s", AS_STRING(args[0])->chars);  // copy source
            snprintf(dst_path, sizeof(dst_path), "%s", AS_STRING(args[1])->chars);  // copy destination
            normalize_path(src_path);                                           // normalize source
            normalize_path(dst_path);                                           // normalize destination
            a->path = strdup(src_path);                                         // store source
            a->path2 = strdup(dst_path);                                        // store destination
            return os_run_async_or_sync(vm, os_copy_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                             // invalid argument
        return true;                                                            // builtin handled
    }

    if (strcmp(name, "os.list_folder") == 0) {                              // list directory contents
        OsArgs* a = os_args_new();                                          // pack argument struct
        if (arg_count >= 1 && IS_STRING(args[0])) {                         // path provided
            char normalized[4096];                                          // normalized path buffer
            snprintf(normalized, sizeof(normalized), "%s", AS_STRING(args[0])->chars);  // copy path
            normalize_path(normalized);                                     // normalize separators
            a->path = strdup(normalized);                                   // store normalized path
        }
        return os_run_async_or_sync(vm, os_list_folder_sync, os_args_free, a, result);
    }

    if (strcmp(name, "os.parent_folder") == 0) {            // get parent directory path
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
            OsArgs* a = os_args_new();                                     // pack argument struct
            char path[4096];                                               // normalized path buffer
            snprintf(path, sizeof(path), "%s", AS_STRING(args[0])->chars); // copy path
            normalize_path(path);                                          // normalize separators
            a->path = strdup(path);                                        // store normalized path
            a->int_val = (int)AS_NUMBER(args[1]);                          // store mode bits
            return os_run_async_or_sync(vm, os_access_sync, os_args_free, a, result);
        }
        *result = MAKE_BOOL(false);                                        // invalid argument
        return true;                                                       // builtin handled
    }

    if (strcmp(name, "os.args") == 0) {                        // command line arguments
        *result = vm->args_table;                              // copy the tagged value
        value_incref(*result);                                 // bump refcount for caller ownership
        return true;                                           // builtin handled
    }

    return false;     // not a recognized builtin
}
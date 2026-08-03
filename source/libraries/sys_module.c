#include "sys_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <sys/timeb.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                         // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));     // intern and box as value
}

// dispatcher for system information built-in functions
bool sys_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "sys.environment") == 0) {                         // get all environment variables
        Table* t = table_create(32);                                    // create table for env vars
        *result = MAKE_TABLE(t);                                        // box table as result
#ifdef _WIN32
        char* env_block = GetEnvironmentStrings();                      // get windows environment block
        if (env_block) {                                                // check if env block exists
                char* env = env_block;                                  // start at beginning
                while (*env) {                                          // iterate through null-terminated strings
                char* eq = strchr(env, '=');                            // find separator
                if (eq) {                                               // valid key=value pair
                        size_t name_len = eq - env;                     // compute key length
                        char* name = (char*)malloc(name_len + 1);       // allocate key buffer
                        if (name) {                                     // check allocation
                        memcpy(name, env, name_len);                    // copy key
                        name[name_len] = '\0';                          // null terminate
                        Value k = make_string_val(vm, name);            // create key value
                        table_set(t, k, make_string_val(vm, eq + 1));   // store key-value pair
                        value_decref(k);                                // release key reference
                        free(name);                                     // free key buffer
                        }
                }
                env += strlen(env) + 1;                                 // move to next variable
                }
                FreeEnvironmentStrings(env_block);                      // free env block
        }
#else
        extern char** environ;                                          // posix environment array
        if (environ) {                                                  // check if environ exists
                for (char** env = environ; *env; env++) {               // iterate over env array
                char* eq = strchr(*env, '=');                           // find separator
                if (eq) {                                               // valid key=value pair
                        size_t name_len = eq - *env;                    // compute key length
                        char* name = (char*)malloc(name_len + 1);       // allocate key buffer
                        if (name) {                                     // check allocation
                        memcpy(name, *env, name_len);                   // copy key
                        name[name_len] = '\0';                          // null terminate
                        Value k = make_string_val(vm, name);            // create key value
                        table_set(t, k, make_string_val(vm, eq + 1));   // store key-value pair
                        value_decref(k);                                // release key reference
                        free(name);                                     // free key buffer
                        }
                }
                }
        }
#endif
        return true;                                                    // builtin handled
    }

    if (strcmp(name, "sys.process_id") == 0) {                          // get current process id
#ifdef _WIN32
        *result = MAKE_NUMBER(GetCurrentProcessId());                   // windows process id
#else
        *result = MAKE_NUMBER(getpid());                                // posix process id
#endif
        return true;                                                    // builtin handled
    }

    if (strcmp(name, "sys.platform") == 0) {                            // get operating system name
        const char* platform = NULL;                                    // platform string
#ifdef _WIN32
        platform = "Windows";                                           // windows platform
#elif __ANDROID__
        platform = "Android";                                           // android platform
#elif __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IOS
        platform = "iOS";                                               // ios platform
#elif TARGET_OS_TV
        platform = "tvOS";                                              // tvos platform
#elif TARGET_OS_WATCH
        platform = "watchOS";                                           // watchos platform
#elif TARGET_OS_MAC
        platform = "macOS";                                             // macos platform
#else
        platform = "Apple";                                             // generic apple
#endif
#elif __linux__
        platform = "Linux";                                             // linux platform
#elif __FreeBSD__
        platform = "FreeBSD";                                           // freebsd platform
#elif __OpenBSD__
        platform = "OpenBSD";                                           // openbsd platform
#elif __NetBSD__
        platform = "NetBSD";                                            // netbsd platform
#elif defined(__QNX__)
        platform = "QNX";                                               // qnx platform
#elif __unix__
        platform = "Unix";                                              // generic unix
#endif
        if (platform) *result = make_string_val(vm, platform);          // return platform string
        else *result = MAKE_NONE();                                     // unknown platform
        return true;                                                    // builtin handled
    }

    if (strcmp(name, "sys.architecture") == 0) {                        // get system architecture
        const char* arch = NULL;                                        // architecture string
#ifdef _WIN32
        SYSTEM_INFO si;                                                 // windows system info
        GetNativeSystemInfo(&si);                                       // get system info
        switch (si.wProcessorArchitecture) {                            // check processor type
            case PROCESSOR_ARCHITECTURE_AMD64: arch = "x86-64"; break;  // 64-bit x86
            case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;     // 32-bit x86
            case PROCESSOR_ARCHITECTURE_ARM64: arch = "arm64"; break;   // 64-bit arm
            case PROCESSOR_ARCHITECTURE_ARM:   arch = "arm"; break;     // 32-bit arm
            default:                           break;                   // unknown
        }
#else
        struct utsname buffer;                                                // posix system info
        if (uname(&buffer) == 0) {                                            // get system info
            if (strcmp(buffer.machine, "aarch64") == 0) arch = "arm64";       // 64-bit arm
            else if (strncmp(buffer.machine, "armv7", 5) == 0) arch = "arm";  // 32-bit arm
            else if (strcmp(buffer.machine, "x86_64") == 0) arch = "x86-64";  // 64-bit x86
            else if (strncmp(buffer.machine, "i686", 4) == 0 || strncmp(buffer.machine, "i386", 4) == 0) arch = "x86";  // 32-bit x86
            else if (strlen(buffer.machine) > 0) arch = buffer.machine;  // use kernel reported
        }
#endif
        if (arch) *result = make_string_val(vm, arch);                   // return architecture string
        else *result = MAKE_NONE();                                      // unknown architecture
        return true;                                                     // builtin handled
    }

    if (strcmp(name, "sys.hostname") == 0) {                             // get system hostname
        char hostname[256];                                              // buffer for hostname
#ifdef _WIN32
        DWORD size = sizeof(hostname);                                   // buffer size
        if (GetComputerName(hostname, &size)) *result = make_string_val(vm, hostname);  // get and return
        else *result = MAKE_NONE();                                                     // failed
#else
        if (gethostname(hostname, sizeof(hostname)) == 0) *result = make_string_val(vm, hostname);  // get and return
        else *result = MAKE_NONE();                                                                 // failed
#endif
        return true;                                            // builtin handled
    }

    if (strcmp(name, "sys.user") == 0) {                        // get current username
#ifdef _WIN32
        char username[256];                                                         // buffer for username
        DWORD size = sizeof(username);                                              // buffer size
        if (GetUserName(username, &size)) *result = make_string_val(vm, username);  // get and return
        else *result = MAKE_NONE();                                                 // failed
#else
        char* username = getenv("USER");                        // try USER env var
        if (!username) username = getenv("LOGNAME");            // try LOGNAME env var
        if (username) *result = make_string_val(vm, username);  // return username
        else *result = MAKE_NONE();                             // not found
#endif
        return true;                                            // builtin handled
    }

    if (strcmp(name, "sys.homedir") == 0) {                     // get user home directory
#ifdef _WIN32
        char* home = getenv("USERPROFILE");                                 // try USERPROFILE
        if (!home) {                                                        // if not set
            char* drive = getenv("HOMEDRIVE");                              // try HOMEDRIVE
            char* path = getenv("HOMEPATH");                                // try HOMEPATH
            if (drive && path) {                                            // both exist
                static char combined[512];                                  // static buffer
                snprintf(combined, sizeof(combined), "%s%s", drive, path);  // combine
                home = combined;                                            // use combined
            }
        }
        if (home) *result = make_string_val(vm, home);  // return home path
        else *result = MAKE_NONE();                     // not found
#else
        char* home = getenv("HOME");                    // try HOME env var
        if (home) *result = make_string_val(vm, home);  // return home path
        else *result = MAKE_NONE();                     // not found
#endif
        return true;                                    // builtin handled
    }

    if (strcmp(name, "sys.apex_version") == 0) {        // get interpreter version
        *result = make_string_val(vm, "26.08");         // return version string
        return true;                                    // builtin handled
    }

    if (strcmp(name, "sys.executable") == 0) {          // get executable path
        char path[4096];                                // buffer for path
#ifdef _WIN32
        if (GetModuleFileName(NULL, path, sizeof(path)) != 0) *result = make_string_val(vm, path);  // get executable path
        else *result = MAKE_NONE();                                                                 // failed
#elif __linux__
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);          // read symlink
        if (len != -1) { path[len] = '\0'; *result = make_string_val(vm, path); }  // return path
        else *result = MAKE_NONE();                                                // failed
#elif __APPLE__
        uint32_t size = sizeof(path);                                                     // buffer size
        if (_NSGetExecutablePath(path, &size) == 0) *result = make_string_val(vm, path);  // get exec path
        else *result = MAKE_NONE();                                                       // failed
#else
        *result = MAKE_NONE();                       // unsupported platform
#endif
        return true;                                 // builtin handled
    }

    if (strcmp(name, "sys.disksize") == 0) {         // get disk space info
        const char* path = ".";                      // default to current dir
        if (arg_count >= 1 && IS_STRING(args[0])) {  // check if path provided
            path = AS_STRING(args[0])->chars;        // use provided path
        }

#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;     // windows disk info
        if (GetDiskFreeSpaceEx(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {  // get disk info
            double total_mb = (double)totalNumberOfBytes.QuadPart / (1024.0 * 1024.0);     // total in mb
            double free_mb = (double)totalNumberOfFreeBytes.QuadPart / (1024.0 * 1024.0);  // free in mb
            double used_mb = total_mb - free_mb;                         // used in mb

            Table* t = table_create(8);                                  // create result table
            Value k1 = make_string_val(vm, "total"); table_set(t, k1, MAKE_NUMBER(total_mb)); value_decref(k1);
            Value k2 = make_string_val(vm, "used"); table_set(t, k2, MAKE_NUMBER(used_mb)); value_decref(k2);
            Value k3 = make_string_val(vm, "free"); table_set(t, k3, MAKE_NUMBER(free_mb)); value_decref(k3);
            
            *result = MAKE_TABLE(t);                                     // return table
        } else {
            *result = MAKE_NONE();                                       // failed
        }
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
        struct statvfs buf;                                              // posix filesystem info
        if (statvfs(path, &buf) == 0) {                                  // get filesystem info
            double total = (double)buf.f_blocks * (double)buf.f_frsize;  // total bytes
            double free = (double)buf.f_bavail * (double)buf.f_frsize;   // free bytes
            
            double total_mb = total / (1024.0 * 1024.0);                 // total in mb
            double free_mb = free / (1024.0 * 1024.0);                   // free in mb
            double used_mb = total_mb - free_mb;                         // used in mb

            Table* t = table_create(8);                                  // create result table
            Value k1 = make_string_val(vm, "total"); table_set(t, k1, MAKE_NUMBER(total_mb)); value_decref(k1);
            Value k2 = make_string_val(vm, "used"); table_set(t, k2, MAKE_NUMBER(used_mb)); value_decref(k2);
            Value k3 = make_string_val(vm, "free"); table_set(t, k3, MAKE_NUMBER(free_mb)); value_decref(k3);
            
            *result = MAKE_TABLE(t);                 // return table
        } else {
            *result = MAKE_NONE();                   // failed
        }
#else
        *result = MAKE_NONE();                       // unsupported platform
#endif
        return true;                                 // builtin handled
    }

    if (strcmp(name, "sys.isterminal") == 0) {       // check if fd is terminal
        int fd = 1;                                  // default to stdout
        if (arg_count >= 1 && IS_NUMBER(args[0])) {  // check if fd provided
            fd = (int)AS_NUMBER(args[0]);            // use provided fd
        }
        *result = MAKE_BOOL(isatty(fd));             // check terminal status
        return true;                                 // builtin handled
    }

    if (strcmp(name, "sys.tempdir") == 0) {          // get temporary directory
#ifdef _WIN32
        char path[4096];                             // buffer for temp path
        if (GetTempPath(sizeof(path), path) != 0) {  // get temp path
            *result = make_string_val(vm, path);     // return temp path
        } else {
            *result = MAKE_NONE();                   // failed
        }
#else
        const char* tmp = getenv("TMPDIR");          // try TMPDIR
        if (!tmp) tmp = getenv("TMP");               // try TMP
        if (!tmp) tmp = getenv("TEMP");              // try TEMP
        if (!tmp) tmp = "/tmp";                      // fallback to /tmp
        *result = make_string_val(vm, tmp);          // return temp path
#endif
        return true;                                 // builtin handled
    }

    if (strcmp(name, "sys.time") == 0) {                                            // get current time
#ifdef _WIN32
        struct _timeb tb;                                                           // windows time struct
        _ftime(&tb);                                                                // get time
        *result = MAKE_NUMBER((double)tb.time + (double)tb.millitm / 1000.0);       // seconds with milliseconds
#else
        struct timeval tv;                                                          // posix time struct
        gettimeofday(&tv, NULL);                                                    // get time
        *result = MAKE_NUMBER((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);  // seconds with microseconds
#endif
        return true;                                                                // builtin handled
    }

    if (strcmp(name, "sys.date") == 0) {                                        // get current date/time
        Table* t = table_create(16);                                            // create result table
        *result = MAKE_TABLE(t);                                                // box table as result
#ifdef _WIN32
        SYSTEMTIME st;                                                          // windows system time
        GetSystemTime(&st);                                                     // get system time
        
        table_set(t, make_string_val(vm, "year"), MAKE_NUMBER(st.wYear));       // store year
        table_set(t, make_string_val(vm, "month"), MAKE_NUMBER(st.wMonth));     // store month
        table_set(t, make_string_val(vm, "week"), MAKE_NUMBER(st.wDayOfWeek));  // store day of week
        table_set(t, make_string_val(vm, "day"), MAKE_NUMBER(st.wDay));         // store day
        table_set(t, make_string_val(vm, "hour"), MAKE_NUMBER(st.wHour));       // store hour
        table_set(t, make_string_val(vm, "minute"), MAKE_NUMBER(st.wMinute));   // store minute
        table_set(t, make_string_val(vm, "second"), MAKE_NUMBER(st.wSecond));   // store second
        table_set(t, make_string_val(vm, "millisecond"), MAKE_NUMBER(st.wMilliseconds));  // store millisecond
#else
        struct timeval tv;                        // posix time
        gettimeofday(&tv, NULL);                  // get time
        struct tm* tm_info = gmtime(&tv.tv_sec);  // convert to gmt struct
        
        if (tm_info) {                            // check conversion
                table_set(t, make_string_val(vm, "year"), MAKE_NUMBER(tm_info->tm_year + 1900));   // store year
                table_set(t, make_string_val(vm, "month"), MAKE_NUMBER(tm_info->tm_mon + 1));      // store month
                table_set(t, make_string_val(vm, "week"), MAKE_NUMBER(tm_info->tm_wday));          // store day of week
                table_set(t, make_string_val(vm, "day"), MAKE_NUMBER(tm_info->tm_mday));           // store day
                table_set(t, make_string_val(vm, "hour"), MAKE_NUMBER(tm_info->tm_hour));          // store hour
                table_set(t, make_string_val(vm, "minute"), MAKE_NUMBER(tm_info->tm_min));         // store minute
                table_set(t, make_string_val(vm, "second"), MAKE_NUMBER(tm_info->tm_sec));         // store second
                table_set(t, make_string_val(vm, "millisecond"), MAKE_NUMBER(tv.tv_usec / 1000));  // store millisecond
        }
#endif
        return true;   // builtin handled
    }

    return false;      // not a recognized builtin
}
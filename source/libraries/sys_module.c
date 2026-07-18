#include "sys_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    int len = (int)strlen(str);
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));
}

// dispatcher for system information built-in functions
bool sys_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "sys.environment") == 0) {
        Table* t = table_create(32);
        *result = MAKE_TABLE(t);
#ifdef _WIN32
        char* env_block = GetEnvironmentStrings();
        if (env_block) {
                char* env = env_block;
                while (*env) {
                char* eq = strchr(env, '=');
                if (eq) {
                        size_t name_len = eq - env;
                        char* name = (char*)malloc(name_len + 1);
                        if (name) {
                        memcpy(name, env, name_len);
                        name[name_len] = '\0';
                        Value k = make_string_val(vm, name);
                        table_set(t, k, make_string_val(vm, eq + 1));
                        value_decref(k);
                        free(name);
                        }
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
                        size_t name_len = eq - *env;
                        char* name = (char*)malloc(name_len + 1);
                        if (name) {
                        memcpy(name, *env, name_len);
                        name[name_len] = '\0';
                        Value k = make_string_val(vm, name);
                        table_set(t, k, make_string_val(vm, eq + 1));
                        value_decref(k);
                        free(name);
                        }
                }
                }
        }
#endif
        return true;
    }

    if (strcmp(name, "sys.process_id") == 0) {
#ifdef _WIN32
        *result = MAKE_NUMBER(GetCurrentProcessId());
#else
        *result = MAKE_NUMBER(getpid());
#endif
        return true;
    }

    if (strcmp(name, "sys.platform") == 0) {
        const char* platform = NULL;
#ifdef _WIN32
        platform = "Windows";
#elif __ANDROID__
        platform = "Android";
#elif __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IOS
        platform = "iOS";
#elif TARGET_OS_TV
        platform = "tvOS";
#elif TARGET_OS_WATCH
        platform = "watchOS";
#elif TARGET_OS_MAC
        platform = "macOS";
#else
        platform = "Apple";
#endif
#elif __linux__
        platform = "Linux";
#elif __FreeBSD__
        platform = "FreeBSD";
#elif __OpenBSD__
        platform = "OpenBSD";
#elif __NetBSD__
        platform = "NetBSD";
#elif defined(__QNX__)
        platform = "QNX";
#elif __unix__
        platform = "Unix";
#endif
        if (platform) *result = make_string_val(vm, platform);
        else *result = MAKE_NONE();
        return true;
    }

    if (strcmp(name, "sys.architecture") == 0) {
        const char* arch = NULL;
#ifdef _WIN32
        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: arch = "x86-64"; break;
            case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
            case PROCESSOR_ARCHITECTURE_ARM64: arch = "arm64"; break;
            case PROCESSOR_ARCHITECTURE_ARM:   arch = "arm"; break;
            default:                           break;
        }
#else
        struct utsname buffer;
        if (uname(&buffer) == 0) {
            if (strcmp(buffer.machine, "aarch64") == 0) arch = "arm64";
            else if (strncmp(buffer.machine, "armv7", 5) == 0) arch = "arm";
            else if (strncmp(buffer.machine, "i686", 4) == 0 || strncmp(buffer.machine, "i386", 4) == 0) arch = "x86";
            else if (strlen(buffer.machine) > 0) arch = buffer.machine;
        }
#endif
        if (arch) *result = make_string_val(vm, arch);
        else *result = MAKE_NONE();
        return true;
    }

    if (strcmp(name, "sys.hostname") == 0) {
        char hostname[256];
#ifdef _WIN32
        DWORD size = sizeof(hostname);
        if (GetComputerName(hostname, &size)) *result = make_string_val(vm, hostname);
        else *result = MAKE_NONE();
#else
        if (gethostname(hostname, sizeof(hostname)) == 0) *result = make_string_val(vm, hostname);
        else *result = MAKE_NONE();
#endif
        return true;
    }

    if (strcmp(name, "sys.user") == 0) {
#ifdef _WIN32
        char username[256];
        DWORD size = sizeof(username);
        if (GetUserName(username, &size)) *result = make_string_val(vm, username);
        else *result = MAKE_NONE();
#else
        char* username = getenv("USER");
        if (!username) username = getenv("LOGNAME");
        if (username) *result = make_string_val(vm, username);
        else *result = MAKE_NONE();
#endif
        return true;
    }

    if (strcmp(name, "sys.homedir") == 0) {
#ifdef _WIN32
        char* home = getenv("USERPROFILE");
        if (!home) {
            char* drive = getenv("HOMEDRIVE");
            char* path = getenv("HOMEPATH");
            if (drive && path) {
                static char combined[512];
                snprintf(combined, sizeof(combined), "%s%s", drive, path);
                home = combined;
            }
        }
        if (home) *result = make_string_val(vm, home);
        else *result = MAKE_NONE();
#else
        char* home = getenv("HOME");
        if (home) *result = make_string_val(vm, home);
        else *result = MAKE_NONE();
#endif
        return true;
    }

    if (strcmp(name, "sys.apex_version") == 0) {
        *result = make_string_val(vm, "26.07");
        return true;
    }

    if (strcmp(name, "sys.executable") == 0) {
        char path[4096];
#ifdef _WIN32
        if (GetModuleFileName(NULL, path, sizeof(path)) != 0) *result = make_string_val(vm, path);
        else *result = MAKE_NONE();
#elif __linux__
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) { path[len] = '\0'; *result = make_string_val(vm, path); }
        else *result = MAKE_NONE();
#elif __APPLE__
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) *result = make_string_val(vm, path);
        else *result = MAKE_NONE();
#else
        *result = MAKE_NONE();
#endif
        return true;
    }

    if (strcmp(name, "sys.disksize") == 0) {
        const char* path = ".";
        if (arg_count >= 1 && IS_STRING(args[0])) {
            path = AS_STRING(args[0])->chars;
        }

#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
        if (GetDiskFreeSpaceEx(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
            double total_mb = (double)totalNumberOfBytes.QuadPart / (1024.0 * 1024.0);
            double free_mb = (double)totalNumberOfFreeBytes.QuadPart / (1024.0 * 1024.0);
            double used_mb = total_mb - free_mb;

            Table* t = table_create(8);
            Value k1 = make_string_val(vm, "total"); table_set(t, k1, MAKE_NUMBER(total_mb)); value_decref(k1);
            Value k2 = make_string_val(vm, "used"); table_set(t, k2, MAKE_NUMBER(used_mb)); value_decref(k2);
            Value k3 = make_string_val(vm, "free"); table_set(t, k3, MAKE_NUMBER(free_mb)); value_decref(k3);
            
            *result = MAKE_TABLE(t);
        } else {
            *result = MAKE_NONE();
        }
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
        struct statvfs buf;
        if (statvfs(path, &buf) == 0) {
            double total = (double)buf.f_blocks * (double)buf.f_frsize;
            double free = (double)buf.f_bavail * (double)buf.f_frsize;
            
            double total_mb = total / (1024.0 * 1024.0);
            double free_mb = free / (1024.0 * 1024.0);
            double used_mb = total_mb - free_mb;

            Table* t = table_create(8);
            Value k1 = make_string_val(vm, "total"); table_set(t, k1, MAKE_NUMBER(total_mb)); value_decref(k1);
            Value k2 = make_string_val(vm, "used"); table_set(t, k2, MAKE_NUMBER(used_mb)); value_decref(k2);
            Value k3 = make_string_val(vm, "free"); table_set(t, k3, MAKE_NUMBER(free_mb)); value_decref(k3);
            
            *result = MAKE_TABLE(t);
        } else {
            *result = MAKE_NONE();
        }
#else
        *result = MAKE_NONE();
#endif
        return true;
    }

    if (strcmp(name, "sys.isterminal") == 0) {
        int fd = 1;
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            fd = (int)AS_NUMBER(args[0]);
        }
        *result = MAKE_BOOL(isatty(fd));
        return true;
    }

    if (strcmp(name, "sys.tempdir") == 0) {
#ifdef _WIN32
        char path[4096];
        if (GetTempPath(sizeof(path), path) != 0) {
            *result = make_string_val(vm, path);
        } else {
            *result = MAKE_NONE();
        }
#else
        const char* tmp = getenv("TMPDIR");
        if (!tmp) tmp = getenv("TMP");
        if (!tmp) tmp = getenv("TEMP");
        if (!tmp) tmp = "/tmp";
        *result = make_string_val(vm, tmp);
#endif
        return true;
    }

    if (strcmp(name, "sys.time") == 0) {
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
    
    return false;
}
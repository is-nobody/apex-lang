#include "sys_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

bool sys_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    (void)arg_count;
    (void)args;

    // --- System Info ---
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
        if (platform) *result = vm_make_string(platform);
        else *result = vm_make_bool(false);
        return true;
    }

    if (strcmp(name, "sys.architecture") == 0) {
        const char* arch = NULL;
#ifdef _WIN32
        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: arch = "x86_64"; break;
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
        if (arch) *result = vm_make_string(arch);
        else *result = vm_make_bool(false);
        return true;
    }

    if (strcmp(name, "sys.hostname") == 0) {
        char hostname[256];
#ifdef _WIN32
        DWORD size = sizeof(hostname);
        if (GetComputerName(hostname, &size)) *result = vm_make_string(hostname);
        else *result = vm_make_bool(false);
#else
        if (gethostname(hostname, sizeof(hostname)) == 0) *result = vm_make_string(hostname);
        else *result = vm_make_bool(false);
#endif
        return true;
    }

    if (strcmp(name, "sys.user") == 0) {
#ifdef _WIN32
        char username[256];
        DWORD size = sizeof(username);
        if (GetUserName(username, &size)) *result = vm_make_string(username);
        else *result = vm_make_bool(false);
#else
        char* username = getenv("USER");
        if (!username) username = getenv("LOGNAME");
        if (username) *result = vm_make_string(username);
        else *result = vm_make_bool(false);
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
                char combined[512];
                snprintf(combined, sizeof(combined), "%s%s", drive, path);
                home = combined;
            }
        }
        if (home) *result = vm_make_string(home);
        else *result = vm_make_bool(false);
#else
        char* home = getenv("HOME");
        if (home) *result = vm_make_string(home);
        else *result = vm_make_bool(false);
#endif
        return true;
    }

    if (strcmp(name, "sys.apex_version") == 0) {
        *result = vm_make_string("26.6");
        return true;
    }

    if (strcmp(name, "sys.executable") == 0) {
        char path[4096];
#ifdef _WIN32
        if (GetModuleFileName(NULL, path, sizeof(path)) != 0) *result = vm_make_string(path);
        else *result = vm_make_bool(false);
#elif __linux__
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) { path[len] = '\0'; *result = vm_make_string(path); }
        else *result = vm_make_bool(false);
#elif __APPLE__
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) *result = vm_make_string(path);
        else *result = vm_make_bool(false);
#else
        *result = vm_make_bool(false);
#endif
        return true;
    }

    if (strcmp(name, "sys.disksize") == 0) {
        const char* path = ".";
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            path = args[0].string->chars;
        }

#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
        if (GetDiskFreeSpaceEx(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
            double total_mb = (double)totalNumberOfBytes.QuadPart / (1024.0 * 1024.0);
            double free_mb = (double)totalNumberOfFreeBytes.QuadPart / (1024.0 * 1024.0);
            double used_mb = total_mb - free_mb;

            Table* t = table_create(8);
            table_set(t, "total", vm_make_number(total_mb));
            table_set(t, "used", vm_make_number(used_mb));
            table_set(t, "free", vm_make_number(free_mb));
            
            result->type = VAL_TABLE;
            result->table = t;
        } else {
            *result = vm_make_bool(false);
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
            table_set(t, "total", vm_make_number(total_mb));
            table_set(t, "used", vm_make_number(used_mb));
            table_set(t, "free", vm_make_number(free_mb));
            
            result->type = VAL_TABLE;
            result->table = t;
        } else {
            *result = vm_make_bool(false);
        }
#else
        *result = vm_make_bool(false);
#endif
        return true;
    }

    if (strcmp(name, "sys.isterminal") == 0) {
        int fd = 1;
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            fd = (int)args[0].number;
        }
        *result = vm_make_bool(isatty(fd));
        return true;
    }

    if (strcmp(name, "sys.tempdir") == 0) {
        char path[4096];
#ifdef _WIN32
        if (GetTempPath(sizeof(path), path) != 0) {
            *result = vm_make_string(path);
        } else {
            *result = vm_make_bool(false);
        }
#else
        const char* tmp = getenv("TMPDIR");
        if (!tmp) tmp = getenv("TMP");
        if (!tmp) tmp = getenv("TEMP");
        if (!tmp) tmp = "/tmp";
        *result = vm_make_string(tmp);
#endif
        return true;
    }

    return false;
}
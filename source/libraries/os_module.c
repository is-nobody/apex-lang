#include "os_module.h"
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
#define chdir _chdir
#define getcwd _getcwd
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#endif

bool os_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;

    // --- I/O ---
    if (strcmp(name, "os.output") == 0) {
        if (arg_count >= 1) {
            vm_print_value(&args[0]);
            printf("\n");
            fflush(stdout);
        }
        *result = vm_make_bool(false);
        return true;
    }
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

    // --- Environment ---
    if (strcmp(name, "os.getenv") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            char* env = getenv(args[0].string->chars);
            if (env) *result = vm_make_string(env);
            else *result = vm_make_bool(false);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
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

    // --- Time & Process Control ---
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
    if (strcmp(name, "os.exit") == 0) {
        int code = 0;
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) code = (int)args[0].number;
        exit(code);
        return true;
    }

    // --- Directory Navigation ---
    if (strcmp(name, "os.getcd") == 0) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) *result = vm_make_string(cwd);
        else *result = vm_make_bool(false);
        return true;
    }
    if (strcmp(name, "os.setcd") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            *result = vm_make_bool(chdir(args[0].string->chars) == 0);
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }

    // --- Process Management ---
    if (strcmp(name, "os.pid") == 0) {
#ifdef _WIN32
        *result = vm_make_number(GetCurrentProcessId());
#else
        *result = vm_make_number(getpid());
#endif
        return true;
    }
    if (strcmp(name, "os.getparentpid") == 0) {
#ifdef _WIN32
        DWORD ppid = 0;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);
            DWORD current_pid = GetCurrentProcessId();
            if (Process32First(hSnapshot, &pe32)) {
                do {
                    if (pe32.th32ProcessID == current_pid) {
                        ppid = pe32.th32ParentProcessID;
                        break;
                    }
                } while (Process32Next(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
        *result = vm_make_number((double)ppid);
#else
        *result = vm_make_number((double)getppid());
#endif
        return true;
    }
    if (strcmp(name, "os.spawn") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
#ifdef _WIN32
            STARTUPINFO si;
            memset(&si, 0, sizeof(si));
            si.cb = sizeof(si);
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
                if (WIFEXITED(status)) *result = vm_make_number((double)WEXITSTATUS(status));
                else *result = vm_make_number(-1);
            } else {
                *result = vm_make_bool(false);
            }
#endif
        } else {
            *result = vm_make_bool(false);
        }
        return true;
    }
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
    if (strcmp(name, "os.system") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            int exit_code = system(args[0].string->chars);
            *result = vm_make_number(exit_code);
        } else {
            *result = vm_make_number(-1);
        }
        return true;
    }

    return false;
}
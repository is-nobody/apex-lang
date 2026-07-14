#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE LibHandle;
    #define dlopen(path, flags) LoadLibraryA(path)
    #define dlsym(handle, name) GetProcAddress(handle, name)
    #define dlclose(handle) FreeLibrary(handle)
#else
    #include <dlfcn.h>
    typedef void* LibHandle;
#endif

// dispatcher for foreign function interface built-ins
bool ffi_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;

    if (strcmp(name, "ffi.open") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STRING) {
            const char* path = args[0].string->chars;
            char full_path[4096];
            
            if (strchr(path, '/') == NULL && strchr(path, '\\') == NULL) {
                snprintf(full_path, sizeof(full_path), "./%s", path);
                path = full_path;
            }

            void* handle = dlopen(path, RTLD_LAZY);
            if (!handle) {
                *result = vm_make_none();
            } else {
                *result = vm_make_table();
                Value k1 = vm_make_string("_handle"); table_set(result->table, k1, vm_make_number((double)(uintptr_t)handle)); value_decref(&k1);
                Value k2 = vm_make_string("path"); table_set(result->table, k2, vm_make_string(args[0].string->chars)); value_decref(&k2);
            }
        } else {
            *result = vm_make_none();
        }
        return true;
    }

    if (strcmp(name, "ffi.call") == 0) {
        if (arg_count < 2) {
            *result = vm_make_none();
            return true;
        }

        Value* lib_val = &args[0];
        Value* name_val = &args[1];

        if (lib_val->type != VAL_TABLE || name_val->type != VAL_STRING) {
            *result = vm_make_none();
            return true;
        }

        Value k_handle = vm_make_string("_handle");
        Value handle_val;
        bool has_handle = table_get(lib_val->table, k_handle, &handle_val);
        value_decref(&k_handle);
        if (!has_handle || handle_val.type != VAL_NUMBER) {
            *result = vm_make_none();
            return true;
        }

        LibHandle handle = (LibHandle)(uintptr_t)handle_val.number;
        const char* func_name = name_val->string->chars;
        
        void* func_ptr = dlsym(handle, func_name);
        if (!func_ptr) {
            *result = vm_make_none();
            return true;
        }

        typedef long (*generic_func_0)();
        typedef long (*generic_func_1)(long);
        typedef long (*generic_func_2)(long, long);
        typedef long (*generic_func_3)(long, long, long);
        typedef long (*generic_func_4)(long, long, long, long);

        long res = 0;
        int actual_args = arg_count - 2;

        switch (actual_args) {
            case 0:
                res = ((generic_func_0)func_ptr)();
                break;
            case 1:
                res = ((generic_func_1)func_ptr)((long)args[2].number);
                break;
            case 2:
                res = ((generic_func_2)func_ptr)((long)args[2].number, (long)args[3].number);
                break;
            case 3:
                res = ((generic_func_3)func_ptr)((long)args[2].number, (long)args[3].number, (long)args[4].number);
                break;
            case 4:
                res = ((generic_func_4)func_ptr)((long)args[2].number, (long)args[3].number, (long)args[4].number, (long)args[5].number);
                break;
            default:
                *result = vm_make_none();
                return true;
        }

        *result = vm_make_number((double)res);
        return true;
    }

    if (strcmp(name, "ffi.errno") == 0) {
        *result = vm_make_number(errno);
        return true;
    }

    if (strcmp(name, "ffi.malloc") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            size_t size = (size_t)args[0].number;
            void* ptr = malloc(size);
            if (ptr) {
                *result = vm_make_number((double)(uintptr_t)ptr);
            } else {
                *result = vm_make_none();
            }
        } else {
            *result = vm_make_none();
        }
        return true;
    }

    if (strcmp(name, "ffi.free") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            void* ptr = (void*)(uintptr_t)args[0].number;
            free(ptr);  // safe for NULL
            *result = vm_make_bool(true);
        } else {
            *result = vm_make_none();
        }
        return true;
    }

    if (strcmp(name, "ffi.strerror") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_string(strerror((int)args[0].number));
        } else {
            *result = vm_make_string(strerror(errno));
        }
        return true;
    }

    return false;
}
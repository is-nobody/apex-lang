#include "ffi_module.h"
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

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));
}

// dispatcher for foreign function interface built-ins
bool ffi_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "ffi.open") == 0) {
        if (arg_count >= 1 && IS_STRING(args[0])) {
            const char* path = AS_STRING(args[0])->chars;
            char full_path[4096];
            
            if (strchr(path, '/') == NULL && strchr(path, '\\') == NULL) {
                snprintf(full_path, sizeof(full_path), "./%s", path);
                path = full_path;
            }

            void* handle = dlopen(path, RTLD_LAZY);
            if (!handle) {
                *result = MAKE_NONE();
            } else {
                Table* t = table_create(8);
                *result = MAKE_TABLE(t);
                Value k1 = make_string_val(vm, "_handle"); table_set(t, k1, MAKE_NUMBER((double)(uintptr_t)handle)); value_decref(k1);
                Value k2 = make_string_val(vm, "path"); table_set(t, k2, MAKE_STRING(string_intern(&vm->intern_table, AS_STRING(args[0])->chars, AS_STRING(args[0])->length))); value_decref(k2);
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }

    if (strcmp(name, "ffi.call") == 0) {
        if (arg_count < 2) {
            *result = MAKE_NONE();
            return true;
        }

        Value lib_val = args[0];
        Value name_val = args[1];

        if (!IS_TABLE(lib_val) || !IS_STRING(name_val)) {
            *result = MAKE_NONE();
            return true;
        }

        Value k_handle = make_string_val(vm, "_handle");
        Value handle_val;
        bool has_handle = table_get(AS_TABLE(lib_val), k_handle, &handle_val);
        value_decref(k_handle);
        if (!has_handle || !IS_NUMBER(handle_val)) {
            *result = MAKE_NONE();
            return true;
        }

        LibHandle handle = (LibHandle)(uintptr_t)AS_NUMBER(handle_val);
        value_decref(handle_val);
        const char* func_name = AS_STRING(name_val)->chars;
        
        void* func_ptr = dlsym(handle, func_name);
        if (!func_ptr) {
            *result = MAKE_NONE();
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
                res = ((generic_func_1)func_ptr)((long)AS_NUMBER(args[2]));
                break;
            case 2:
                res = ((generic_func_2)func_ptr)((long)AS_NUMBER(args[2]), (long)AS_NUMBER(args[3]));
                break;
            case 3:
                res = ((generic_func_3)func_ptr)((long)AS_NUMBER(args[2]), (long)AS_NUMBER(args[3]), (long)AS_NUMBER(args[4]));
                break;
            case 4:
                res = ((generic_func_4)func_ptr)((long)AS_NUMBER(args[2]), (long)AS_NUMBER(args[3]), (long)AS_NUMBER(args[4]), (long)AS_NUMBER(args[5]));
                break;
            default:
                *result = MAKE_NONE();
                return true;
        }

        *result = MAKE_NUMBER((double)res);
        return true;
    }

    if (strcmp(name, "ffi.errno") == 0) {
        *result = MAKE_NUMBER(errno);
        return true;
    }

    if (strcmp(name, "ffi.malloc") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            size_t size = (size_t)AS_NUMBER(args[0]);
            void* ptr = malloc(size);
            if (ptr) {
                *result = MAKE_NUMBER((double)(uintptr_t)ptr);
            } else {
                *result = MAKE_NONE();
            }
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }

    if (strcmp(name, "ffi.free") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            void* ptr = (void*)(uintptr_t)AS_NUMBER(args[0]);
            free(ptr);
            *result = MAKE_BOOL(true);
        } else {
            *result = MAKE_NONE();
        }
        return true;
    }

    if (strcmp(name, "ffi.strerror") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = make_string_val(vm, strerror((int)AS_NUMBER(args[0])));
        } else {
            *result = make_string_val(vm, strerror(errno));
        }
        return true;
    }

    return false;
}
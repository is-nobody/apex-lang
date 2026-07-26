#include "ffi_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE LibHandle;                                // windows library handle type
    #define dlopen(path, flags) LoadLibraryA(path)            // wrap windows LoadLibrary
    #define dlsym(handle, name) GetProcAddress(handle, name)  // wrap windows GetProcAddress
    #define dlclose(handle) FreeLibrary(handle)               // wrap windows FreeLibrary
#else
    #include <dlfcn.h>
    typedef void* LibHandle;                                  // posix library handle type
#endif

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                      // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));  // intern and box as value
}

// dispatcher for foreign function interface built-ins
bool ffi_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "ffi.open") == 0) {                                    // open dynamic library
        if (arg_count >= 1 && IS_STRING(args[0])) {                         // validate library path argument
            const char* path = AS_STRING(args[0])->chars;                   // extract path string
            char full_path[4096];                                           // buffer for absolute path
            
            if (strchr(path, '/') == NULL && strchr(path, '\\') == NULL) {  // check if path is relative
                snprintf(full_path, sizeof(full_path), "./%s", path);       // prepend ./ to relative path
                path = full_path;                                           // use constructed path
            }

            void* handle = dlopen(path, RTLD_LAZY);                         // load library with lazy binding
            if (!handle) {                                                  // check if load failed
                *result = MAKE_NONE();                                      // return none on failure
            } else {
                Table* t = table_create(8);                                 // create table to store library info
                *result = MAKE_TABLE(t);                                    // box table as result
                Value k1 = make_string_val(vm, "_handle");                  // create handle key
                table_set(t, k1, MAKE_NUMBER((double)(uintptr_t)handle));   // store handle as number
                value_decref(k1);                                           // release key reference
                Value k2 = make_string_val(vm, "path");                     // create path key
                table_set(t, k2, MAKE_STRING(string_intern(&vm->intern_table, AS_STRING(args[0])->chars, AS_STRING(args[0])->length)));  // store original path
                value_decref(k2);                                           // release key reference
            }
        } else {
            *result = MAKE_NONE();                                          // invalid arguments, return none
        }
        return true;                                                        // builtin handled
    }

    if (strcmp(name, "ffi.call") == 0) {                   // call function from library
        if (arg_count < 2) {                               // need at least library and function name
            *result = MAKE_NONE();                         // insufficient args
            return true;                                   // builtin handled
        }

        Value lib_val = args[0];                           // library table
        Value name_val = args[1];                          // function name string

        if (!IS_TABLE(lib_val) || !IS_STRING(name_val)) {  // validate argument types
            *result = MAKE_NONE();                         // invalid types
            return true;                                   // builtin handled
        }

        Value k_handle = make_string_val(vm, "_handle");   // create handle lookup key
        Value handle_val;
        bool has_handle = table_get(AS_TABLE(lib_val), k_handle, &handle_val);  // extract handle from table
        value_decref(k_handle);                                                 // release key reference
        if (!has_handle || !IS_NUMBER(handle_val)) {                            // validate handle exists
            *result = MAKE_NONE();                                              // no handle found
            return true;                                                        // builtin handled
        }

        LibHandle handle = (LibHandle)(uintptr_t)AS_NUMBER(handle_val);  // cast handle to library pointer
        value_decref(handle_val);                                        // release handle value
        const char* func_name = AS_STRING(name_val)->chars;              // extract function name
        
        void* func_ptr = dlsym(handle, func_name);                       // lookup function by name
        if (!func_ptr) {                                                 // check if function found
            *result = MAKE_NONE();                                       // function not found
            return true;                                                 // builtin handled
        }

        // generic function pointer types for different arities
        typedef long (*generic_func_0)();                        // 0-arg function type
        typedef long (*generic_func_1)(long);                    // 1-arg function type
        typedef long (*generic_func_2)(long, long);              // 2-arg function type
        typedef long (*generic_func_3)(long, long, long);        // 3-arg function type
        typedef long (*generic_func_4)(long, long, long, long);  // 4-arg function type

        long res = 0;                                            // result storage
        int actual_args = arg_count - 2;                         // number of function arguments

        switch (actual_args) {                                   // dispatch based on arity
            // call with 0 args
            case 0:
                res = ((generic_func_0)func_ptr)();
                break;
            // call with 1 arg
            case 1:
                res = ((generic_func_1)func_ptr)((long)AS_NUMBER(args[2]));
                break;
            // call with 2 args
            case 2:
                res = ((generic_func_2)func_ptr)((long)AS_NUMBER(args[2]), (long)AS_NUMBER(args[3]));
                break;
            // call with 3 args
            case 3:
                res = ((generic_func_3)func_ptr)((long)AS_NUMBER(args[2]), (long)AS_NUMBER(args[3]), (long)AS_NUMBER(args[4]));
                break;
            // call with 4 args
            case 4:
                res = ((generic_func_4)func_ptr)((long)AS_NUMBER(args[2]), (long)AS_NUMBER(args[3]), (long)AS_NUMBER(args[4]), (long)AS_NUMBER(args[5]));
                break;
            default:
                *result = MAKE_NONE();                                  // unsupported arity
                return true;                                            // builtin handled
        }

        *result = MAKE_NUMBER((double)res);                             // return result as number
        return true;                                                    // builtin handled
    }

    if (strcmp(name, "ffi.errno") == 0) {                               // get current errno
        *result = MAKE_NUMBER(errno);                                   // return errno as number
        return true;                                                    // builtin handled
    }

    if (strcmp(name, "ffi.malloc") == 0) {                              // allocate memory
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                     // validate size argument
            size_t size = (size_t)AS_NUMBER(args[0]);                   // extract size in bytes
            void* ptr = malloc(size);                                   // allocate memory
            if (ptr) {                                                  // check allocation success
                *result = MAKE_NUMBER((double)(uintptr_t)ptr);          // return pointer as number
            } else {
                *result = MAKE_NONE();                                  // allocation failed
            }
        } else {
            *result = MAKE_NONE();                                      // invalid argument
        }
        return true;                                                    // builtin handled
    }

    if (strcmp(name, "ffi.free") == 0) {                                // free allocated memory
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                     // validate pointer argument
            void* ptr = (void*)(uintptr_t)AS_NUMBER(args[0]);           // extract pointer
            free(ptr);                                                  // free memory
            *result = MAKE_BOOL(true);                                  // return success
        } else {
            *result = MAKE_NONE();                                      // invalid argument
        }
        return true;                                                    // builtin handled
    }

    if (strcmp(name, "ffi.strerror") == 0) {                            // get error string
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                     // validate error number argument
            *result = make_string_val(vm, strerror((int)AS_NUMBER(args[0])));  // convert error to string
        } else {
            *result = make_string_val(vm, strerror(errno));             // use current errno if no arg
        }
        return true;                                                    // builtin handled
    }

    return false;                                                       // not a recognized builtin
}
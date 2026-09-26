// source/compiler/codegen_modules.c
// Module-name resolution helpers
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// checks if a name is a known built-in module root using first-char switch
bool is_known_builtin_module(const char* name) {
    switch (name[0]) {
        case 'o':
            return strcmp(name, "os") == 0;        // os module
        case 's':
            return strcmp(name, "sys") == 0 ||     // sys module
                   strcmp(name, "string") == 0;    // string module
        case 'm':
            return strcmp(name, "math") == 0;      // math module
        case 't':
            return strcmp(name, "table") == 0;     // table module
        case 'r':
            return strcmp(name, "random") == 0 ||  // random module
                   strcmp(name, "regex") == 0;     // regex module
        case 'c':
            return strcmp(name, "csv") == 0 ||     // csv module
                   strcmp(name, "crypto") == 0;    // crypto module
        case 'j':
            return strcmp(name, "json") == 0;      // json module
        case 'x':
            return strcmp(name, "xml") == 0;       // xml module
        case 'b':
            return strcmp(name, "base") == 0;      // base module
        case 'z':
            return strcmp(name, "zip") == 0;       // zip module
        case 'd':
            return strcmp(name, "datetime") == 0;  // datetime module
        default:
            return false;                          // no builtin module matches
    }
}

// registers a module-scoped global variable for name resolution
void add_module_global(CodeGenerator* cg, const char* full_name) {
    for (int i = 0; i < cg->module_globals_count; i++) {                          // check existing
        if (strcmp(cg->module_globals[i], full_name) == 0) return;                // already exists
    }
    if (cg->module_globals_count >= cg->module_globals_capacity) {                // need more space
        cg->module_globals_capacity = cg->module_globals_capacity == 0 ? 16 : cg->module_globals_capacity * 2;  // double
        cg->module_globals = (char**)realloc(cg->module_globals, sizeof(char*) * cg->module_globals_capacity);  // reallocate
    }
    cg->module_globals[cg->module_globals_count++] = strdup(full_name);           // add name
}
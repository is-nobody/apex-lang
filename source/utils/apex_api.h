// source/utils/apex_api.h
// Implementation of Apex API for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef APEX_API_H
#define APEX_API_H

#include <stdbool.h>

// initializes the apex runtime environment, call once at application start
void apex_init(void);

// shuts down the apex runtime and frees global resources
void apex_shutdown(void);

// executes apex code from a file, writes os.exit() code into out_exit_code (-1 if none)
bool apex_execute_file(const char* filepath, int* out_exit_code);

// executes apex code from a source string, writes os.exit() code into out_exit_code (-1 if none)
bool apex_execute_string(const char* source_code, const char* filename, int* out_exit_code);

#endif // APEX_API_H
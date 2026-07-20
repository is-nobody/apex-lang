#ifndef APEX_API_H
#define APEX_API_H

#include <stdbool.h>

// initializes the apex runtime environment, call once at application start
void apex_init(void);

// shuts down the apex runtime and frees global resources
void apex_shutdown(void);

// executes apex code from a file
bool apex_execute_file(const char* filepath);

// executes apex code from a source string with the given filename for error context
bool apex_execute_string(const char* source_code, const char* filename);

#endif // APEX_API_H
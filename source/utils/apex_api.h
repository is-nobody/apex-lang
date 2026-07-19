#ifndef APEX_API_H
#define APEX_API_H

#include <stdbool.h>

// initializes the apex runtime environment, call once at application start
void apex_init(void);

// shuts down the apex runtime and frees global resources
void apex_shutdown(void);

// executes apex code from a file (if filename is provided) or from a source string
bool apex_execute(const char* filename, const char* source_code);

#endif // APEX_API_H
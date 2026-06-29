#ifndef APEX_API_H
#define APEX_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// initializes the apex runtime environment, call once at application start
void apex_init(void);

// shuts down the apex runtime and frees global resources
void apex_shutdown(void);

// executes apex source code from a string with a filename for error context
bool apex_execute(const char* source_code, const char* filename);

// executes an apex source file from the given filesystem path
bool apex_execute_file(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // APEX_API_H
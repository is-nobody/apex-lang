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

// execute from file or from string
bool apex_execute(const char* filename, const char* source_code);

#ifdef __cplusplus
}
#endif

#endif // APEX_API_H
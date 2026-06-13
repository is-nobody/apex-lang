#ifndef APEX_API_H
#define APEX_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the Apex runtime environment.
 * Call this once at the start of your application.
 */
void apex_init(void);

/**
 * Shuts down the Apex runtime and frees global resources.
 * Call this before your application exits.
 */
void apex_shutdown(void);

/**
 * Executes Apex source code from a string.
 * 
 * @param source_code The Apex code to execute.
 * @param filename    A name for the script (used in error messages).
 * @return true if execution was successful, false otherwise.
 */
bool apex_execute(const char* source_code, const char* filename);

/**
 * Executes Apex code from a file.
 * 
 * @param filepath The path to the .apex file.
 * @return true if execution was successful, false otherwise.
 */
bool apex_execute_file(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // APEX_API_H
#ifndef EXECUTE_H
#define EXECUTE_H

#include <stdbool.h>
#include <stdarg.h>

// prints a red-colored error message with formatting
void print_error(const char* format, ...);

// executes source code from a string with the given filename for error context
bool execute_source_string(const char* source_code, const char* filename);

// executes source code from a file path
bool execute_source(const char* source, const char* filename);

#endif // EXECUTE_H
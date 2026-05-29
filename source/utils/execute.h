#ifndef EXECUTE_H
#define EXECUTE_H
#include <stdbool.h>

/**
 * Executes Apex source code.
 * @param source   Source code string
 * @param filename Source file name or identifier
 * @return true on success, false on failure
 */
bool execute_source(const char* source, const char* filename);

/** Enables or disables safe mode for REPL */
void set_repl_mode(int active);

/** Called from tokenizer.c / parser.c instead of exit(1) */
void throw_repl_error(void);
#endif
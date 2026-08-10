// source/utils/loader.h
// Implementation of Bytecode Loader for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef LOADER_H
#define LOADER_H

#include "bytecode.h"
#include <stdbool.h>

// loads a bytecode chunk from a .apexc file, restoring all sections
BytecodeChunk* bytecode_load(const char* path);

// executes a bytecode file directly, bypassing tokenization and parsing
bool execute_bytecode_file(const char* filepath, int argc, char** argv, bool skip_script_name);

#endif // LOADER_H
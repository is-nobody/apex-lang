#ifndef EXECUTE_H
#define EXECUTE_H

#include "tokenizer.h"
#include "parser.h"
#include "ast.h"
#include "bytecode.h"
#include "codegen.h"
#include "vm.h"
#include <stdbool.h>
#include <stdarg.h>

// prints a red-colored error message with formatting
void print_error(const char* format, ...);

// executes source code from a file path
bool execute_source(const char* source, const char* filename);

// cleans up all allocated resources in reverse order
void cleanup_all(Tokenizer* tok, Parser* par, ASTNode* ast,
                 CodeGenerator* cg, BytecodeChunk* chunk, VM* vm,
                 char* source);

#endif // EXECUTE_H
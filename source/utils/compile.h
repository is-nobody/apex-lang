// source/utils/compile.h
// Implementation of Compiler for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef COMPILE_H
#define COMPILE_H

// compiles an apex source file to bytecode (.apexc), returns 0 on success
int compile_command(int argc, char** argv);

#endif // COMPILE_H
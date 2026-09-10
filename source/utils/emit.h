// source/utils/emit.h
// Implementation of Bytecode Disassembler for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef EMIT_H
#define EMIT_H

// disassembles an apex source file and prints bytecode to stdout, returns 0 on success
int emit_command(int argc, char** argv);

#endif // EMIT_H
// source/utils/build.h
// Implementation of Build System for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef BUILD_H
#define BUILD_H

// builds a standalone executable from an apex source file, returns 0 on success
int build_command(int argc, char** argv);

#endif // BUILD_H
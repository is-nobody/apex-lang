#ifndef ERROR_H
#define ERROR_H

void print_error_with_context(const char* filename, const char* source, 
                              int line, int col, int len, 
                              const char* type, const char* message);

#endif
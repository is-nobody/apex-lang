#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

void platform_init(void);
void terminal_enable_raw_mode(void);
void terminal_disable_raw_mode(void);
int terminal_read_char(void);
ssize_t terminal_read_blocking(char* c);
bool terminal_has_input(void);
const char* platform_get_name(void);

char* platform_create_temp_file(const char* data, size_t len);

void platform_delete_temp_file(const char* path);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_H
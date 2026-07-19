#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

#ifdef _WIN32
  #include <windows.h>
  typedef SSIZE_T ssize_t;
#else
  #include <unistd.h>
#endif

// initializes platform-specific features (terminal, paths, etc.)
void platform_init(void);

// enables raw terminal mode for character-by-character input
void terminal_enable_raw_mode(void);

// disables raw terminal mode and restores normal settings
void terminal_disable_raw_mode(void);

// reads a single character from the terminal (raw mode)
int terminal_read_char(void);

// reads a single character with blocking, returns the character read
ssize_t terminal_read_blocking(char* c);

// checks if there is input available on the terminal
bool terminal_has_input(void);

// returns the name of the current platform (e.g., "Windows", "Linux")
const char* platform_get_name(void);

// creates a temporary file with the given data and returns its path
char* platform_create_temp_file(const char* data, size_t len);

// deletes a temporary file by its path
void platform_delete_temp_file(const char* path);

#endif // PLATFORM_H
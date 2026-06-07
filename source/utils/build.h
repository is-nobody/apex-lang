#ifndef BUILD_H
#define BUILD_H

/**
 * Handles the 'build' CLI command to create a standalone executable.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on error.
 */
int build_command(int argc, char** argv);

#endif // BUILD_H
#ifndef COMMANDS_H
#define COMMANDS_H

/**
 * Handles specific CLI commands like 'version' and 'build'.
 * 
 * @return Returns the exit code if a command was handled (0 for success, 1 for error).
 *         Returns -1 if no known command was found (fallback to default behavior).
 */
int handle_commands(int argc, char** argv);

#endif // COMMANDS_H
#ifndef COMMANDS_H
#define COMMANDS_H

// handles cli commands like 'version' and 'build', returns 0/1 if handled or -1 if none
int handle_commands(int argc, char** argv);

#endif // COMMANDS_H
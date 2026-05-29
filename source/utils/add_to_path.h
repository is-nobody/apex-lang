#ifndef ADD_TO_PATH_H
#define ADD_TO_PATH_H

/**
 * Adds the executable's directory to PATH.
 * Updates the current session and persistent configs (Windows registry / shell rc).
 * @param argv0 argv[0] from main()
 */
void ensure_path_updated(const char* argv0);
#endif
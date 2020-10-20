#ifndef zzftp__path_utils_h
#define zzftp__path_utils_h

#include <stdbool.h>

// Calculates the real path from a working directory and a relative path
// Returns NULL on invalid input
char *path_cat(const char *wd, const char *rel);

// The same, but in-place
// If successful, true is returned and the original string is freed
// Otherwise false is returned and the original string is not touched
bool path_change(char **wd, const char *rel);

#endif

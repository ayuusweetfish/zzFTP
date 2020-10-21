#ifndef zzftp__auth_h
#define zzftp__auth_h

#include <stdbool.h>
#include <string.h>

static inline bool user_auth(const char *user, const char *pass)
{
  return strcmp(user, "qwq") == 0 && strcmp(pass, "quq") == 0;
}

#endif

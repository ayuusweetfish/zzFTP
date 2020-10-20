#include "path_utils.h"

#include <stdlib.h>
#include <string.h>

char *path_cat(const char *wd, const char *rel)
{
  if (wd[0] != '/') return NULL;
  char *buf = malloc(strlen(wd) + strlen(rel));
  if (buf == NULL) return NULL;
#define meh (free(buf), NULL)

  strcpy(buf, rel[0] == '/' ? "/" : wd);
  size_t len = strlen(buf);

  for (const char *c = rel, *d = c; *d != '\0'; c = d + 1) {
    // Find the current item
    d = c;
    while (*d != '/' && *d != '\0') {
      if (!((*d >= '0' && *d <= '9') ||
          (*d >= 'A' && *d <= 'Z') ||
          (*d >= 'a' && *d <= 'z') ||
          (*d == '-' || *d == '.' || *d == '_')))
        return meh;
      d++;
    }
    // Process
    if (d - c == 0 || (d - c == 1 && c[0] == '.')) {
      // No-op
    } else if (d - c == 2 && c[0] == '.' && c[1] == '.') {
      // Go up
      while (buf[len - 1] != '/') len--;
      if (len > 1) len--; // Remove the extra trailing slash
    } else {
      if (len > 1) buf[len++] = '/';
      for (; c < d; c++) buf[len++] = *c;
    }
  }

  buf[len] = '\0';
  return buf;
#undef meh
}

bool path_cd(char **wd, const char *rel)
{
  char *p = path_cat(*wd, rel);
  if (p != NULL) {
    free(*wd);
    *wd = p;
    return true;
  } else {
    return false;
  }
}

#ifdef PATH_UTILS_TEST
#include <stdio.h>
#include <string.h>

static void test(const char *a, const char *b, const char *o)
{
  char *v = path_cat(a, b);
  printf("%s | %s + %s -> %s\n",
    strcmp(v, o) == 0 ? "passed" : "failed", a, b, v);
  free(v);
}

int main()
{
  test("/quq/qvq/qwq/qxq", "qaq/qnq", "/quq/qvq/qwq/qxq/qaq/qnq");
  test("/quq/qvq", "qwq/../../qxq/", "/quq/qxq");
  test("/quq/qvq", "../../../../../..", "/");
  test("/", "quq", "/quq");
  test("/quq", "qvq", "/quq/qvq");
  test("/", "a//b", "/a/b");
  test("/", "..", "/");
  test("/", "../a", "/a");
  test("/quq/qvq", "/qwq/qxq/..", "/qwq");
  test("/quq/qvq", "//qwq/qxq/..", "/qwq");
  test("/quq", "qvq/.//.../././///./qwq", "/quq/qvq/.../qwq");
  return 0;
}
#endif

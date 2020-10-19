#include "io_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void panic(const char *msg)
{
  fprintf(stderr, "panic | %s: errno = %d (%s)\n", msg, errno, strerror(errno));
  exit(1);
}

void warn(const char *msg)
{
  fprintf(stderr, "warn  | %s: errno = %d (%s)\n", msg, errno, strerror(errno));
}

size_t read_all(int fd, void *buf, size_t len)
{
  size_t result, tot = 0;
  while (tot < len) {
    result = read(fd, buf, len - tot);
    if (result == -1) {
      warn("write() failed");
      return tot;
    } else if (result == 0) {
      break;
    }
    buf += result;
    tot += result;
  }
  return tot;
}

size_t write_all(int fd, const void *buf, size_t len)
{
  ssize_t result;
  while (len != 0) {
    result = write(fd, buf, len);
    if (result == -1) {
      warn("write() failed");
      return len;
    }
    buf += result;
    len -= result;
  }
  return 0;
}

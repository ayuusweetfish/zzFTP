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

size_t write_all(int fd, const void *buf, size_t len)
{
  ssize_t ret;
  while (len != 0) {
    ret = write(fd, buf, len);
    if (ret == -1) {
      warn("write() failed");
      return len;
    }
    buf += ret;
    len -= ret;
  }
  return 0;
}

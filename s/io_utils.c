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

void info(const char *msg)
{
  fprintf(stderr, "info  | %s\n", msg);
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

#define RLBUF_BUFSIZE   1024

void rlb_init(rlb *b, int fd)
{
  b->fd = fd;
  b->buf = malloc(RLBUF_BUFSIZE);
  b->head = b->tail = b->buf;
  b->no_more = false;
}

static inline char rlb_get_char(rlb *b)
{
  char c;
  do {
    // Check whether buffer has been all consumed
    if (b->tail <= b->head) {
      // Refill buffer
      ssize_t result = read(b->fd, b->buf, RLBUF_BUFSIZE);
      if (result == -1) {
        warn("read() failed");
        b->no_more = true;
        return '\n';
      } else if (result == 0) {
        info("connection closed");
        b->no_more = true;
        return '\n';
      }
      b->head = b->buf;
      b->tail = b->buf + result;
    }
    c = *(b->head++);
  } while (c == '\r');
  return c;
}

size_t rlb_read_line(rlb *b, char *buf, size_t size)
{
  if (size == 0) return 0;
  if (size == 1) { buf[0] = '\0'; return 0; }

  size_t i;
  for (i = 0; i < size; i++)
    if ((buf[i] = rlb_get_char(b)) == '\n') { i++; break; }

  // Truncate LF
  if (buf[i - 1] == '\n') i--;

  if (i == size) {
    // Truncate extraneous content
    char ch;
    while ((ch = rlb_get_char(b)) != '\n') { }
    // Return `size`
    buf[i - 1] = '\0';
    return size;
  } else {
    buf[i] = '\0';
    // Return -1 on closed socket or error
    if (i == 0 && b->no_more) return -1;
    return i;
  }
}

void rlb_deinit(rlb *b)
{
  free(b->buf);
}

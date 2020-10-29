#include "io_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

void panic(const char *msg)
{
  fprintf(stderr, "panic | %s: errno = %d (%s)\n", msg, errno, strerror(errno));
  exit(1);
}

void warn(const char *msg)
{
#ifdef LOG_WARN
  fprintf(stderr, "warn  | %s: errno = %d (%s)\n", msg, errno, strerror(errno));
#endif
}

void info(const char *msg)
{
#ifdef LOG_INFO
  fprintf(stderr, "info  | %s\n", msg);
#endif
}

size_t read_all(int fd, void *buf, size_t len)
{
  size_t result, tot = 0;
  while (tot < len) {
    result = read(fd, buf, len - tot);
    if (result == -1) {
      if (errno == EAGAIN) {
        usleep(1000);
        continue;
      }
      warn("read() failed");
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
      if (errno == EAGAIN) {
        usleep(1000);
        continue;
      }
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

void send_mark(int fd, int code, const char *msg)
{
  char pfx[4];
  pfx[0] = '0' + (code / 100) % 10;
  pfx[1] = '0' + (code / 10) % 10;
  pfx[2] = '0' + code % 10;

  while (1) {
    const char *p = strchr(msg, '\n');
    // No more LF's, or terminating LF
    bool last_line = (p == NULL || *(p + 1) == '\0');
    size_t line_len = (p == NULL ? strlen(msg) : (p - msg));

    pfx[3] = (last_line ? ' ' : '-');
    write_all(fd, pfx, 4);        // Prefix
    write_all(fd, msg, line_len); // Line
    write_all(fd, "\r\n", 2);     // EOL

    msg = p + 1;
    if (last_line) break;
  }
}

int sock_ephemeral()
{
  int sock_eph = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_eph == -1) return -1;

  struct sockaddr_in addr = { 0 };
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock_eph, (struct sockaddr *)&addr, sizeof addr) == -1) {
    close(sock_eph);
    return -2;
  }

  return sock_eph;
}

int ephemeral(int sock_conn, uint8_t o_addr[6], int *o_sock_fd)
{
  int sock_eph = sock_ephemeral();
  if (sock_eph < 0) return sock_eph;

  struct sockaddr_in addr;
  socklen_t addr_len;

  // Port
  addr_len = sizeof addr;
  getsockname(sock_eph, (struct sockaddr *)&addr, &addr_len);
  memcpy(o_addr + 4, &addr.sin_port, 2);

  // Address
  addr_len = sizeof addr;
  getsockname(sock_conn, (struct sockaddr *)&addr, &addr_len);
  memcpy(o_addr + 0, &addr.sin_addr.s_addr, 4);

  *o_sock_fd = sock_eph;
  return 0;
}

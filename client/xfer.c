#include "xfer.h"

#include "io_utils.h"

#include "libui/ui.h"

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define queue(_fn, _arg) uiQueueMain((void *)(_fn), (void *)(_arg));

struct conn_thr_args {
  xfer *x;
  struct sockaddr_in addr;
  void (*next)(int);
};

static void conn_thr(struct conn_thr_args *p_args)
{
  struct conn_thr_args args = *p_args;
  free(p_args);

  bool successful;
  if (args.addr.sin_addr.s_addr == 0) {
    // Listen
    successful = (listen(args.x->pasv_fd, 0) == 0 &&
      (args.x->fd = accept(args.x->pasv_fd, NULL, NULL)) > 0);
  } else {
    // Connect
    successful = (connect(args.x->fd,
      (struct sockaddr *)&args.addr, sizeof args.addr) == 0);
  }

  if (successful) {
    rlb_init(&args.x->b, args.x->fd);
    queue(args.next, 0);
  } else {
    close(args.x->fd);
    queue(args.next, XFER_ERR_CONNECT);
  }
}

void xfer_init(xfer *x, const char *host, int port, void (*next)(int))
{
  int fd = sock_ephemeral();
  if (fd < 0) { (*next)(XFER_ERR_SOCKET); return; }

  struct conn_thr_args *args = malloc(sizeof(struct conn_thr_args));

  x->fd = fd;
  args->x = x;
  memset(&args->addr, 0, sizeof args->addr);
  args->addr.sin_family = AF_INET;
  args->addr.sin_addr.s_addr = inet_addr(host);
  args->addr.sin_port = htons(port);
  args->next = next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)conn_thr, args) != 0) {
    close(fd);
    (*next)(XFER_ERR_THREAD);
  }
}

int xfer_init_listen_1(xfer *x, const xfer *y, uint8_t local_addr[6])
{
  if (ephemeral(y->fd, local_addr, &x->pasv_fd) != 0) return XFER_ERR_SOCKET;
  return 0;
}

void xfer_init_listen_2(xfer *x, void (*next)(int))
{
  struct conn_thr_args *args = malloc(sizeof(struct conn_thr_args));

  args->x = x;
  args->addr.sin_family = AF_INET;
  args->addr.sin_addr.s_addr = 0;
  args->next = next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)conn_thr, args) != 0) {
    close(x->pasv_fd);
    (*next)(XFER_ERR_THREAD);
  }
}

void xfer_deinit(xfer *x)
{
  close(x->fd);
  rlb_deinit(&x->b);
}

struct write_args {
  xfer *x;
  char *str;
  void (*next)(int);
};

static void write_thr(struct write_args *p_args)
{
  struct write_args args = *p_args;
  free(p_args);

  if (write_all(args.x->fd, args.str, strlen(args.str)) != 0) {
    if (args.next) queue(args.next, XFER_ERR_IO);
  } else {
    if (args.next) queue(args.next, 0);
  }
  free(args.str);
}

void xfer_write(xfer *x, const char *str, void (*next)(int))
{
  xfer_write_all(x, str, strlen(str), next);
}

void xfer_write_all(xfer *x, const void *buf, size_t len, void (*next)(int))
{
  struct write_args *args = malloc(sizeof(struct write_args));
  args->x = x;
  args->str = strdup(buf);
  args->next = next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)write_thr, args) != 0) {
    (*next)(XFER_ERR_THREAD);
  }
}

struct read_mark_args {
  xfer *x;
  void (*next)(int, char *);
};

struct read_mark_cb_args {
  void (*next)(int, char *);
  int code;
  char *buf;
};

static void read_mark_cb(struct read_mark_cb_args *args)
{
  (*args->next)(args->code, args->buf);
  free(args->buf);
  free(args);
}

#define READLINE_LINE_BUF_SIZE 1024
static void read_mark_thr(struct read_mark_args *p_args)
{
  struct read_mark_args args = *p_args;
  free(p_args);

  size_t cap = 64, len = 0;
  char *buf = malloc(cap);
  int code = 0;

  char *line = malloc(READLINE_LINE_BUF_SIZE);

  while (1) {
    size_t lnsz = rlb_read_line(&args.x->b, line, READLINE_LINE_BUF_SIZE);
    if (lnsz >= 4) {
      while (len + lnsz - 3 >= cap) buf = realloc(buf, cap <<= 1);
      strcpy(buf + len, line + 4);
      len += (lnsz - 4);
      buf[len++] = '\n';
      if (isdigit(line[0]) && isdigit(line[1]) &&
          isdigit(line[2]) && line[3] == ' ') {
        code = (line[0] - '0') * 100 +
          (line[1] - '0') * 10 + (line[2] - '0');
        break;
      }
    }
  }
  free(line);
  buf[len] = '\0';

  if (args.next) {
    struct read_mark_cb_args *cb_args
      = malloc(sizeof(struct read_mark_cb_args));
    cb_args->next = args.next;
    cb_args->code = code;
    cb_args->buf = buf;
    queue(read_mark_cb, cb_args);
  }
}

void xfer_read_mark(xfer *x, void (*next)(int, char *))
{
  struct read_mark_args *args = malloc(sizeof(struct read_mark_args));
  args->x = x;
  args->next = (void *)next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)read_mark_thr, args) != 0) {
    (*next)(XFER_ERR_THREAD, NULL);
  }
}

struct read_all_args {
  xfer *x;
  void (*next)(size_t, char *);
};

struct read_all_cb_args {
  void (*next)(size_t, char *);
  size_t len;
  char *buf;
};

static void read_all_cb(struct read_all_cb_args *args)
{
  (*args->next)(args->len, args->buf);
  free(args->buf);
  free(args);
}

#define READALL_BLOCK_BUF_SIZE 65536
static void read_all_thr(struct read_all_args *p_args)
{
  struct read_all_args args = *p_args;
  free(p_args);

  size_t cap = 64, len = 0;
  char *buf = malloc(cap);

  char *block = malloc(READALL_BLOCK_BUF_SIZE);
  ssize_t read_ret;

  while ((read_ret = read(args.x->fd, block, READALL_BLOCK_BUF_SIZE)) > 0) {
    while (len + read_ret >= cap) buf = realloc(buf, cap <<= 1);
    memcpy(buf + len, block, read_ret);
    len += read_ret;
  }
  free(block);
  buf[len] = '\0';

  if (args.next) {
    struct read_all_cb_args *cb_args
      = malloc(sizeof(struct read_all_cb_args));
    cb_args->next = args.next;
    cb_args->len = len;
    cb_args->buf = buf;
    queue(read_all_cb, cb_args);
  }
}

void xfer_read_all(xfer *x, void (*next)(size_t, char *))
{
  struct read_all_args *args = malloc(sizeof(struct read_all_args));
  args->x = x;
  args->next = (void *)next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)read_all_thr, args) != 0) {
    (*next)(XFER_ERR_THREAD, NULL);
  }
}

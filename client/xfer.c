#include "xfer.h"

#include "io_utils.h"

#include "libui/ui.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define queue(_fn, _arg) uiQueueMain((_fn), (void *)(_arg));

struct xfer_conn_thr_args {
  xfer *x;
  struct sockaddr_in addr;
  void (*next)(void *);
};

void xfer_conn_thr(struct xfer_conn_thr_args *p_args)
{
  struct xfer_conn_thr_args args = *p_args;
  free(p_args);

  if (connect(args.x->fd, (struct sockaddr *)&args.addr, sizeof args.addr) == -1) {
    close(args.x->fd);
    queue(args.next, XFER_ERR_CONNECT);
  } else {
    queue(args.next, 0);
  }
}

void xfer_init(xfer *x, const char *host, int port, void (*next)(int))
{
  int fd = sock_ephemeral();
  if (fd < 0) { (*next)(XFER_ERR_SOCKET); return; }

  struct xfer_conn_thr_args *args = malloc(sizeof(struct xfer_conn_thr_args));

  x->fd = fd;
  args->x = x;
  memset(&args->addr, 0, sizeof args->addr);
  args->addr.sin_family = AF_INET;
  args->addr.sin_addr.s_addr = inet_addr(host);
  args->addr.sin_port = htons(port);
  args->next = (void *)next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)xfer_conn_thr, args) != 0) {
    close(fd);
    (*next)(XFER_ERR_THREAD);
  }
}

struct xfer_io_args {
  xfer *x;
  char *str;
  void (*next)(void *);
};

void xfer_write_thr(struct xfer_io_args *p_args)
{
  struct xfer_io_args args = *p_args;
  free(p_args);

  puts(args.str);
  if (write_all(args.x->fd, args.str, strlen(args.str)) != 0) {
    if (args.next) queue(args.next, XFER_ERR_IO);
  } else {
    if (args.next) queue(args.next, 0);
  }
  free(args.str);
}

void xfer_write(xfer *x, const char *str, void (*next)(int))
{
  struct xfer_io_args *args = malloc(sizeof(struct xfer_io_args));
  args->x = x;
  args->str = strdup(str);
  args->next = (void *)next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)xfer_write_thr, args) != 0) {
    (*next)(XFER_ERR_THREAD);
  }
}

void xfer_read_line(xfer *x, void (*next)(char *))
{
}

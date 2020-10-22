#include "xfer.h"

#include "io_utils.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

struct xfer_conn_thr_args {
  int fd;
  struct sockaddr_in addr;
  void (*next)(int);
};

void xfer_conn_thr(struct xfer_conn_thr_args *p_args)
{
  struct xfer_conn_thr_args args = *p_args;
  free(p_args);

  if (connect(args.fd, (struct sockaddr *)&args.addr, sizeof args.addr) == -1) {
    close(args.fd);
    (*args.next)(XFER_INIT_ERR_CONNECT);
  } else {
    (*args.next)(0);
  }
}

void xfer_init(xfer *x, const char *host, int port, void (*next)(int))
{
  int fd = sock_ephemeral();
  if (fd < 0) { (*next)(XFER_INIT_ERR_SOCKET); return; }

  struct xfer_conn_thr_args *args = malloc(sizeof(struct xfer_conn_thr_args));

  args->fd = fd;
  memset(&args->addr, 0, sizeof args->addr);
  args->addr.sin_family = AF_INET;
  args->addr.sin_addr.s_addr = inet_addr(host);
  args->addr.sin_port = htons(port);
  args->next = next;

  pthread_t thr;
  if (pthread_create(&thr, NULL, (void *)xfer_conn_thr, args) != 0) {
    close(fd);
    (*next)(XFER_INIT_ERR_THREAD); return;
  }
}

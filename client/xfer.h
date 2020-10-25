#ifndef zzftp_client__xfer_h
#define zzftp_client__xfer_h

#include "io_utils.h"

#include <stdint.h>

typedef struct xfer_s {
  int fd, pasv_fd;
  rlb b;
} xfer;

#define XFER_ERR_SOCKET   (-1)
#define XFER_ERR_THREAD   (-2)
#define XFER_ERR_CONNECT  (-3)
#define XFER_ERR_IO       (-4)

// Initializes the transfer controller by connecting to a host
// `next` is called when the connection is established
// or immediately with an error code
void xfer_init(xfer *x, const char *host, int port, void (*next)(int));
// Initializes the transfer controller by listening on a port
// The transfer is discard-after-use
// An already established transfer `y` helps determine the local address
// and populate the `local_addr` field
// `next` is called when one connection is established
// or immediately with an error code
int xfer_init_listen_1(xfer *x, const xfer *y, uint8_t local_addr[6]);
void xfer_init_listen_2(xfer *x, void (*next)(int));
// Closes the transfer controller
void xfer_deinit(xfer *x);

void xfer_write(xfer *x, const char *str, void (*next)(int));
void xfer_write_all(xfer *x, const void *buf, size_t len, void (*next)(int));
void xfer_read_mark(xfer *x, void (*next)(int, char *));
void xfer_read_all(xfer *x, void (*next)(size_t, char *));
void xfer_read_all_to(xfer *x, int fd, void (*next)(size_t));

#endif

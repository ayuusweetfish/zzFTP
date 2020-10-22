#ifndef zzftp_client__xfer_h
#define zzftp_client__xfer_h

#include <stdint.h>
#include <pthread.h>

typedef struct xfer_s {
  uint8_t local_addr[6];
} xfer;

#define XFER_INIT_ERR_SOCKET    (-1)
#define XFER_INIT_ERR_THREAD    (-2)
#define XFER_INIT_ERR_CONNECT   (-3)
void xfer_init(xfer *x, const char *host, int port, void (*next)(int));

#endif

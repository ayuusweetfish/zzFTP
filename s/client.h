#ifndef zzftp__client_h
#define zzftp__client_h

#include "io_utils.h"

#include <stdint.h>

typedef struct client_s {
  int sock_ctl;   // Socket for the control connection
  rlb buf_ctl;    // Buffer for the control connection

  int sock_dat_p; // Socket for the data connection (passive)
  int sock_dat;   // Socket for the data connection

  enum client_state_t {
    CLST_CONN = 0,    // Connected
    CLST_WAIT_PASS,   // USER entered, waiting for password
    CLST_READY,       // Ready
    CLST_PORT = 16,   // Port mode
    CLST_PASV = 32,   // Passive mode
    CLST_XFER = 64,   // Transfer in progress
  } state;

  char *username;
  int xferred_files_bytes;
  int xferred_files_num;

  // Port mode: client address and port
  // Passive mode: local address and port
  uint8_t addr[4];
  uint16_t port;
} client;

client *client_create(int sock_ctl);
void client_close(client *c);

void client_run_loop(client *c);

void process_command(client *c, const char *verb, const char *arg);

#endif

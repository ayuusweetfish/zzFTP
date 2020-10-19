#ifndef zzftp__client_h
#define zzftp__client_h

#include "io_utils.h"

typedef struct client_s {
  int sock_ctl;   // Socket for the control connection
  rlb buf_ctl;    // Buffer for the control connection

  int sock_dat_p; // Socket for the data connection (passive)
  int sock_dat;   // Socket for the data connection

  char *username;
  int xferred_files_bytes;
  int xferred_files_num;
} client;

client *client_create(int sock_ctl);
void client_close(client *c);

void client_run_loop(client *c);

void process_command(client *c, const char *verb, const char *arg);

#endif

#ifndef zzftp__client_h
#define zzftp__client_h

#include "io_utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct client_s {
  int sock_ctl;   // Socket for the control connection
  rlb buf_ctl;    // Buffer for the control connection

  enum client_state_t {
    CLST_CONN = 0,    // Connected
    CLST_WAIT_PASS,   // USER entered, waiting for password
    CLST_READY,       // Ready
    CLST_PORT = 16,   // Port mode
    CLST_PASV = 32,   // Passive mode
  } state;

  char *username;
  int xferred_files_bytes;
  int xferred_files_num;

  char *wd;
  char *rnfr;
  size_t rest_offs;

  // Port mode: client address and port
  // Passive mode: local address and port
  uint8_t addr[4];
  uint16_t port;

  pthread_mutex_t mutex_ctl;

  pthread_mutex_t mutex_dat;
  pthread_cond_t cond_dat;
  pthread_t thr_dat;
  bool thr_dat_running;

  FILE *dat_fp;
  enum dat_type_t {
    DATA_UNDEFINED,
    DATA_SEND_FILE,
    DATA_RECV_FILE,
    DATA_SEND_PIPE,
  } dat_type;
} client;

client *client_create(int sock_ctl);
void client_close(client *c);

void client_run_loop(client *c);

bool client_xfer_in_progress(client *c);
void client_close_threads(client *c);

typedef enum cmd_result_e {
  CMD_RESULT_DONE,
  CMD_RESULT_SHUTDOWN,
} cmd_result;

cmd_result process_command(client *c, const char *verb, const char *arg);

#define mark(_code, _str) do { \
  pthread_mutex_lock(&c->mutex_ctl); \
  send_mark(c->sock_ctl, _code, _str); \
  pthread_mutex_unlock(&c->mutex_ctl); \
} while (0)
#define markf(_code, ...) do { \
  char s[256]; \
  snprintf(s, sizeof s, __VA_ARGS__); \
  mark(_code, s); \
} while (0)

#define crit(_block) do { \
  pthread_mutex_lock(&c->mutex_dat); \
  _block \
  pthread_mutex_unlock(&c->mutex_dat); \
} while (0)

#endif

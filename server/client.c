#include "client.h"

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>

client *client_create(int sock_ctl)
{
  client *c = malloc(sizeof(client));

  c->sock_ctl = sock_ctl;
  rlb_init(&c->buf_ctl, sock_ctl);

  c->state = CLST_CONN;

  c->username = NULL;
  c->xferred_files_bytes = 0;
  c->xferred_files_num = 0;

  c->wd = strdup("/");
  c->rnfr = NULL;

  pthread_mutex_init(&c->mutex_dat, NULL);
  pthread_cond_init(&c->cond_dat, NULL);
  c->thr_dat_running = false;
  c->dat_fp = NULL;

  return c;
}

void client_close(client *c)
{
  client_close_threads(c);

  rlb_deinit(&c->buf_ctl);
  shutdown(c->sock_ctl, SHUT_RDWR);
  close(c->sock_ctl);

  if (c->username != NULL) free(c->username);
  free(c->wd);
  if (c->rnfr != NULL) free(c->rnfr);

  pthread_mutex_destroy(&c->mutex_dat);

  free(c);
}

static const char *WELCOME_MSG =
  "                Welcome to zzftp.kawa.moe FTP archives                \n"
  "\n"
  "This site is provided as a public service.  Use in violation of any\n"
  "applicable laws is strictly prohibited.  No guarantees, explicit or\n"
  "implicit, about the contents of this site.  Use at your own risk.\n"
  "\n"
  "FTP server ready.\n"
;

static const char *GOODBYE_MSG =
  "Goodbye";

void client_run_loop(client *c)
{
  send_mark(c->sock_ctl, 220, WELCOME_MSG);

  char cmd[1024];
  while (1) {
    // Read a command
    ssize_t cmd_len = rlb_read_line(&c->buf_ctl, cmd, sizeof cmd);
    if (cmd_len < 0) break;
    if (cmd_len == sizeof cmd) {
      send_mark(c->sock_ctl, 500,
        "Command line too long, the limit is 1023 characters.");
      continue;
    }

    // Parse the verb and the argument
    const char *verb = cmd, *arg = "";
    char *p = cmd;
    while ((*p = toupper(*p)) >= 'A' && *p <= 'Z') p++;
    if (*p != ' ' && *p != '\0') {
      send_mark(c->sock_ctl, 500,
        "The verb should be terminate the command line, "
        "or be followed by a space.");
      continue;
    }
    if (*p != '\0') {
      *p = '\0';
      arg = p + 1;
    }

    if (process_command(c, verb, arg) == CMD_RESULT_SHUTDOWN)
      break;
  }

  send_mark(c->sock_ctl, 221, GOODBYE_MSG);
}

bool client_xfer_in_progress(client *c)
{
  FILE *f;
  crit({ f = c->dat_fp; });
  if (f != NULL) return true;
  return false;
}

void client_close_threads(client *c)
{
  bool running;
  crit({ running = c->thr_dat_running; });

  if (running) {
    crit({ c->thr_dat_running = false; });
    pthread_join(c->thr_dat, NULL);
    c->state = CLST_READY;
  }
}

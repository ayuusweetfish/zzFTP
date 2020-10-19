#include "client.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

client *client_create(int sock_ctl)
{
  client *c = malloc(sizeof(client));

  c->sock_ctl = sock_ctl;
  rlb_init(&c->buf_ctl, sock_ctl);

  c->sock_dat_p = c->sock_dat = -1;

  c->state = CLST_CONN;

  c->username = NULL;
  c->xferred_files_bytes = 0;
  c->xferred_files_num = 0;

  return c;
}

void client_close(client *c)
{
  rlb_deinit(&c->buf_ctl);
  close(c->sock_ctl);

  if (c->sock_dat_p >= 0) close(c->sock_dat_p);
  if (c->sock_dat >= 0) close(c->sock_dat);

  if (c->username != NULL) free(c->username);

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
    while (*p >= 'A' && *p <= 'Z') p++;
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

    if (strcmp(verb, "QUIT") == 0) break;
    process_command(c, verb, arg);
  }

  send_mark(c->sock_ctl, 221, GOODBYE_MSG);
}

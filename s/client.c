#include "client.h"

#include "io_utils.h"

#include <stdlib.h>
#include <unistd.h>

client *client_create(int sock_ctl)
{
  client *c = malloc(sizeof(client));

  c->sock_ctl = sock_ctl;
  rlb_init(&c->buf_ctl, sock_ctl);

  c->sock_dat_p = c->sock_dat = -1;

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

void client_process(client *c)
{
  char s[8], t[64];
  size_t l;
  do {
    l = rlb_read_line(&c->buf_ctl, s, sizeof s);
    snprintf(t, sizeof t, "%d \"%s\"\n", (int)l, s);
    write_all(c->sock_ctl, t, strlen(t));
  } while (l != -1);
}

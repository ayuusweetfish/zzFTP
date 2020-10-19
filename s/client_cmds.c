#include "client.h"

#include <stdio.h>
#include <string.h>

#define mark(_code, _str) send_mark(c->sock_ctl, _code, _str)

static void handler_SYST(client *c, const char *arg)
{
  mark(215, "UNIX Type: L8");
}

static void handler_TYPE(client *c, const char *arg)
{
  if (strcmp(arg, "I") == 0) mark(200, "Type set to I.");
  else mark(504, "Only binary mode is supported.");
}

// Process

void process_command(client *c, const char *verb, const char *arg)
{
#define def_cmd(_verb) \
  if (strcmp(verb, #_verb) == 0) { handler_##_verb(c, arg); return; }

  def_cmd(SYST)
  def_cmd(TYPE)

#undef def_cmd

  char t[64];
  snprintf(t, sizeof t, "Unknown command \"%s\"", verb);
  send_mark(c->sock_ctl, 202, t);
}

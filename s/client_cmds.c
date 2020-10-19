#include "client.h"

#include <stdio.h>
#include <stdlib.h>
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

static void handler_USER(client *c, const char *arg)
{
  if (strcmp(arg, "anonymous") == 0) {
    c->state = CLST_WAIT_PASS;
    c->username = NULL;
    mark(331, "Logging in anonymously. "
      "Send your complete e-mail address as password.");
  } else {
    // TODO
    mark(504, "This server allows anonymous access only.");
  }
}

static void handler_PASS(client *c, const char *arg)
{
  if (c->state != CLST_WAIT_PASS) {
    mark(503, "Specify your username first.");
    return;
  }
  if (c->username == NULL) {
    // Anonymous login
    // Can be replaced with asprintf
    int len = snprintf(NULL, 0, "anon/%s", arg);
    char *username = malloc(len + 1);
    snprintf(username, len + 1, "anon/%s", arg);
    c->username = username;
    char s[256];
    snprintf(s, sizeof s, "Logged in. Welcome, %s.", username);
    mark(230, s);
  }
}

// Process

void process_command(client *c, const char *verb, const char *arg)
{
#define def_cmd(_verb) \
  if (strcmp(verb, #_verb) == 0) { handler_##_verb(c, arg); return; }

  def_cmd(SYST)
  def_cmd(TYPE)
  def_cmd(USER)
  def_cmd(PASS)

#undef def_cmd

  char t[64];
  snprintf(t, sizeof t, "Unknown command \"%s\"", verb);
  send_mark(c->sock_ctl, 202, t);
}

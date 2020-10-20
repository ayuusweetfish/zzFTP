#include "client.h"
#include "auth.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define mark(_code, _str) send_mark(c->sock_ctl, _code, _str)
#define markf(_code, ...) do { \
  char s[256]; \
  snprintf(s, sizeof s, __VA_ARGS__); \
  mark(_code, s); \
} while (0)

#define auth() do { \
  if (c->state < CLST_READY) { \
    mark(530, "Log in first."); \
    return; \
  } \
} while (0)

// Reference: RFC 954, 5.4, Command-Reply Sequences (pp. 48-52)

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
  if (c->state >= CLST_READY) {
    mark(503, "Already logged in.");
    return;
  }

  c->state = CLST_WAIT_PASS;
  if (c->username != NULL) free(c->username);

  if (strcmp(arg, "anonymous") == 0) {
    c->username = NULL;
    mark(331, "Logging in anonymously. "
      "Send your complete e-mail address as password.");
  } else {
    c->username = strdup(arg);
    mark(331, "Please specify the password.");
  }
}

static void handler_PASS(client *c, const char *arg)
{
  if (c->state < CLST_WAIT_PASS) {
    mark(503, "Specify your username first.");
    return;
  } else if (c->state >= CLST_READY) {
    mark(503, "Already logged in.");
    return;
  }

  if (c->username == NULL) {
    // Anonymous login
    if (strlen(arg) > 64) {
      mark(530, "Password too long (more than 64 characters).");
      return;
    }
    // Create anonymous username (can be replaced with asprintf)
    int len = snprintf(NULL, 0, "anonymous/%s", arg);
    char *username = malloc(len + 1);
    snprintf(username, len + 1, "anonymous/%s", arg);
    c->username = username;
  } else {
    // Existing user
    if (!user_auth(c->username, arg)) {
      usleep(1000000);  // Delay before replying
      mark(530, "Incorrect username/password.");
      return;
    }
    // Log in
  }

  c->state = CLST_READY;
  markf(230, "Logged in. Welcome, %s.", c->username);
}

static void handler_PORT(client *c, const char *arg)
{
  auth();

  unsigned x[6];
  if (sscanf(arg, "%u,%u,%u,%u,%u,%u",
      &x[0], &x[1], &x[2], &x[3], &x[4], &x[5]) != 6) {
    mark(501, "Incorrect address format.");
    return;
  }
  for (int i = 0; i < 6; i++) if (x[0] >= 256) {
    mark(501, "Incorrect address format.");
    return;
  }

  c->state = CLST_PORT;
  for (int i = 0; i < 4; i++) c->addr[i] = x[i];
  c->port = x[4] * 256 + x[5];
  markf(200, "Will connect to %u.%u.%u.%u:%u\n",
    x[0], x[1], x[2], x[3], c->port);
}

static void handler_PASV(client *c, const char *arg)
{
  auth();

  uint8_t addr[6];
  int fd;
  ephemeral(c->sock_ctl, addr, &fd);

  markf(227, "Entering Passive Mode ("
    "%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8
  ")", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
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
  def_cmd(PORT)
  def_cmd(PASV)

#undef def_cmd

  char t[64];
  snprintf(t, sizeof t, "Unknown command \"%s\"", verb);
  send_mark(c->sock_ctl, 202, t);
}

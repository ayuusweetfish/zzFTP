#include "client.h"
#include "auth.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>

#define mark(_code, _str) send_mark(c->sock_ctl, _code, _str)
#define markf(_code, ...) do { \
  char s[256]; \
  snprintf(s, sizeof s, __VA_ARGS__); \
  mark(_code, s); \
} while (0)

#define auth() do { \
  if (c->state < CLST_READY) { \
    mark(530, "Log in first."); \
    return CMD_RESULT_DONE; \
  } \
} while (0)

#define shutdown(_str) do { \
  mark(421, _str " Shutting down connection."); \
  return CMD_RESULT_SHUTDOWN; \
} while (0)

// Reference: RFC 954, 5.4, Command-Reply Sequences (pp. 48-52)

static cmd_result handler_QUIT(client *c, const char *arg)
{
  return CMD_RESULT_SHUTDOWN;
}

static cmd_result handler_SYST(client *c, const char *arg)
{
  mark(215, "UNIX Type: L8");
  return CMD_RESULT_DONE;
}

static cmd_result handler_TYPE(client *c, const char *arg)
{
  if (strcmp(arg, "I") == 0) mark(200, "Type set to I.");
  else mark(504, "Only binary mode is supported.");
  return CMD_RESULT_DONE;
}

static cmd_result handler_USER(client *c, const char *arg)
{
  if (c->state >= CLST_READY) {
    mark(503, "Already logged in.");
    return CMD_RESULT_DONE;
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
  return CMD_RESULT_DONE;
}

static cmd_result handler_PASS(client *c, const char *arg)
{
  if (c->state < CLST_WAIT_PASS) {
    mark(503, "Specify your username first.");
    return CMD_RESULT_DONE;
  } else if (c->state >= CLST_READY) {
    mark(503, "Already logged in.");
    return CMD_RESULT_DONE;
  }

  if (c->username == NULL) {
    // Anonymous login
    if (strlen(arg) > 64) {
      mark(530, "Password too long (more than 64 characters).");
      return CMD_RESULT_DONE;
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
      return CMD_RESULT_DONE;
    }
    // Log in
  }

  c->state = CLST_READY;
  markf(230, "Logged in. Welcome, %s.", c->username);
  return CMD_RESULT_DONE;
}

static cmd_result handler_PORT(client *c, const char *arg)
{
  auth();

  unsigned x[6];
  if (sscanf(arg, "%u,%u,%u,%u,%u,%u",
      &x[0], &x[1], &x[2], &x[3], &x[4], &x[5]) != 6) {
    mark(501, "Incorrect address format.");
    return CMD_RESULT_DONE;
  }
  for (int i = 0; i < 6; i++) if (x[0] >= 256) {
    mark(501, "Incorrect address format.");
    return CMD_RESULT_DONE;
  }

  c->state = CLST_PORT;
  for (int i = 0; i < 4; i++) c->addr[i] = x[i];
  c->port = x[4] * 256 + x[5];
  markf(200, "Will connect to %u.%u.%u.%u:%u\n",
    x[0], x[1], x[2], x[3], c->port);
  return CMD_RESULT_DONE;
}

static cmd_result handler_PASV(client *c, const char *arg)
{
  auth();

  uint8_t addr[6];
  int fd;
  if (ephemeral(c->sock_ctl, addr, &fd) != 0 ||
      listen(fd, 8) != 0)
    shutdown("Cannot enter passive mode.");

  c->state = CLST_PASV;
  if (c->sock_dat_p != -1) close(c->sock_dat_p);
  c->sock_dat_p = fd;

  markf(227, "Entering Passive Mode ("
    "%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8
  ")", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return CMD_RESULT_DONE;
}

// Process

cmd_result process_command(client *c, const char *verb, const char *arg)
{
#define def_cmd(_verb) \
  if (strcmp(verb, #_verb) == 0) return handler_##_verb(c, arg);

  def_cmd(QUIT)
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
  return CMD_RESULT_DONE;
}

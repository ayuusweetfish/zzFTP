#include "client.h"
#include "auth.h"
#include "path_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef NO_AUTH
#define auth() do { \
  if (c->state < CLST_READY) { \
    mark(530, "Log in first."); \
    return CMD_RESULT_DONE; \
  } \
} while (0)
#else
#define auth() do { } while (0)
#endif

#define data() do { \
  if (c->state != CLST_PORT && c->state != CLST_PASV) { \
    mark(425, "Use PORT or PASV first."); \
    return CMD_RESULT_DONE; \
  } \
} while (0)

#define disconnect(_str) do { \
  mark(421, _str " Shutting down connection."); \
  return CMD_RESULT_SHUTDOWN; \
} while (0)

#define ignore_if_xfer() \
  if (client_xfer_in_progress(c)) return CMD_RESULT_DONE

// Reference: RFC 954, 5.4, Command-Reply Sequences (pp. 48-52)

static cmd_result handler_QUIT(client *c, const char *arg)
{
  client_close_threads(c);
  return CMD_RESULT_SHUTDOWN;
}

static cmd_result handler_SYST(client *c, const char *arg)
{
  mark(215, "UNIX Type: L8");
  return CMD_RESULT_DONE;
}

static cmd_result handler_TYPE(client *c, const char *arg)
{
  ignore_if_xfer();
  if (strcmp(arg, "I") == 0) mark(200, "Type set to I.");
  else mark(504, "Only binary mode is supported.");
  return CMD_RESULT_DONE;
}

static cmd_result handler_USER(client *c, const char *arg)
{
  ignore_if_xfer();
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
  ignore_if_xfer();
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

#include "client_xfer_thr.h"

static cmd_result handler_PORT(client *c, const char *arg)
{
  ignore_if_xfer();
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

  // Create thread
  crit({ c->thr_dat_running = true; });
  if (pthread_create(&c->thr_dat, NULL, &active_data, c) != 0)
    disconnect("Cannot enter port mode: pthread_create() failed.");

  c->state = CLST_PORT;
  for (int i = 0; i < 4; i++) c->addr[i] = x[i];
  c->port = x[4] * 256 + x[5];
  markf(200, "Will connect to %u.%u.%u.%u:%u\n",
    x[0], x[1], x[2], x[3], c->port);
  return CMD_RESULT_DONE;
}

static cmd_result handler_PASV(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();
  client_close_threads(c);

  uint8_t addr[6];
  int fd;
  if (ephemeral(c->sock_ctl, addr, &fd) != 0 ||
      fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1 ||
      listen(fd, 0) == -1)
    disconnect("Cannot enter passive mode: cannot start data connection.");

  c->state = CLST_PASV;

  // Create thread
  struct passive_data_arg *thr_arg = malloc(sizeof(struct passive_data_arg));
  thr_arg->sock_fd = fd;
  thr_arg->c = c;
  crit({ c->thr_dat_running = true; });
  if (pthread_create(&c->thr_dat, NULL, &passive_data, thr_arg) != 0)
    disconnect("Cannot enter passive mode: pthread_create() failed.");

  markf(227, "Entering Passive Mode ("
    "%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8
  ")", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return CMD_RESULT_DONE;
}

#define full_path(_d) do { \
  _d = path_cat(c->wd, arg); \
  if (_d == NULL) { \
    mark(501, "Argument is not a valid path."); \
    return CMD_RESULT_DONE; \
  } \
} while (0)

static cmd_result handler_CWD(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();

  char *d; full_path(d);
  if (!path_exists(d, PATH_REQUIREMENT_DIR)) {
    markf(550, "Directory <%s> does not exist.", d);
    return (free(d), CMD_RESULT_DONE);
  }

  free(c->wd);
  c->wd = d;
  markf(250, "Working directory changed to <%s>.", d);
  return CMD_RESULT_DONE;
}

static cmd_result handler_PWD(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();
  markf(257, "Working directory is <%s>.", c->wd);
  return CMD_RESULT_DONE;
}

static cmd_result handler_MKD(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();

  char *d; full_path(d);
  if (mkdir(d + 1, 0755) != 0) {
    markf(550, "Cannot create directory <%s> (%s).", d, strerror(errno));
    return (free(d), CMD_RESULT_DONE);
  }

  markf(250, "Directory <%s> created.", d);
  return (free(d), CMD_RESULT_DONE);
}

#define mark_dir(_msg, _cmp, ...) do { \
  bool changes_wd = (strcmp(c->wd, _cmp) == 0); \
  if (changes_wd) { \
    char *d_up = path_cat(d, ".."); \
    free(c->wd); \
    c->wd = d_up; \
    markf(250, _msg " Working directory changed to <%s>.", \
      __VA_ARGS__, c->wd); \
  } else { \
    markf(250, _msg, __VA_ARGS__); \
  } \
} while (0)

static cmd_result handler_RMD(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();

  char *d; full_path(d);
  if (strlen(d) == 1) {
    mark(550, "Cannot remove root directory.");
    return (free(d), CMD_RESULT_DONE);
  }
  if (rmdir(d + 1) != 0) {
    markf(550, "Cannot remove directory <%s> (%s).", d, strerror(errno));
    return (free(d), CMD_RESULT_DONE);
  }

  mark_dir("Directory <%s> removed.", d, d);
  return (free(d), CMD_RESULT_DONE);
}

static cmd_result handler_RNFR(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();

  char *d; full_path(d);
  if (strlen(d) == 1) {
    mark(550, "Cannot rename root directory.");
    return (free(d), CMD_RESULT_DONE);
  }
  if (!path_exists(d, PATH_REQUIREMENT_NONE)) {
    markf(550, "Path <%s> does not exist.", d);
    return (free(d), CMD_RESULT_DONE);
  }

  if (c->rnfr != NULL) free(c->rnfr);
  c->rnfr = d;
  markf(250, "Renaming <%s>.", d);
  return CMD_RESULT_DONE;
}

static cmd_result handler_RNTO(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();

  if (c->rnfr == NULL) {
    mark(503, "Use RNFR first.");
    return CMD_RESULT_DONE;
  }

  char *rnfr = c->rnfr;
  c->rnfr = NULL;

  char *d; full_path(d);
  if (strlen(d) == 1) {
    mark(550, "Cannot rename to root directory.");
    return (free(rnfr), free(d), CMD_RESULT_DONE);
  }
  if (rename(rnfr + 1, d + 1) != 0) {
    markf(550, "Cannot rename <%s> to <%s> (%s).",
      rnfr, d, strerror(errno));
    return (free(rnfr), free(d), CMD_RESULT_DONE);
  }

  mark_dir("Renamed <%s> to <%s>.", rnfr, rnfr, d);
  return (free(rnfr), free(d), CMD_RESULT_DONE);
}

static cmd_result handler_LIST(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();
  data();

  FILE *f = popen("ls -lH", "r");
  if (f == NULL) {
    mark(550, "Internal error. Cannot list.");
    return CMD_RESULT_DONE;
  }

  crit({ c->dat_fp = f; c->dat_type = DATA_SEND_PIPE; });

  mark(150, "Directory listing is being sent over the data connection.");
  return CMD_RESULT_DONE;
}

static cmd_result handler_RETR(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();
  data();

  char *d; full_path(d);
  if (!path_exists(d, PATH_REQUIREMENT_REGULAR)) {
    markf(550, "File <%s> does not exist.", d);
    return (free(d), CMD_RESULT_DONE);
  }

  FILE *f = fopen(d + 1, "r");
  if (f == NULL) {
    mark(550, "Internal error. Cannot retrieve file.");
    return (free(d), CMD_RESULT_DONE);
  }

  crit({ c->dat_fp = f; c->dat_type = DATA_SEND_FILE; });

  mark(150, "File contents are being sent over the data connection.");
  return (free(d), CMD_RESULT_DONE);
}

static cmd_result handler_STOR(client *c, const char *arg)
{
  ignore_if_xfer();
  auth();
  data();

  char *d; full_path(d);

  FILE *f = fopen(d + 1, "w");
  if (f == NULL) {
    mark(550, "Cannot write to file.");
    return (free(d), CMD_RESULT_DONE);
  }

  crit({ c->dat_fp = f; c->dat_type = DATA_RECV_FILE; });

  mark(150, "Send file contents over the data connection.");
  return (free(d), CMD_RESULT_DONE);
}

static cmd_result handler_ABOR(client *c, const char *arg)
{
  if (client_xfer_in_progress(c)) {
    enum dat_type_t dat_type = c->dat_type;
    client_close_threads(c);
    mark(226, dat_type == DATA_RECV_FILE ?
      "Transfer aborted. Partial data written." :
      "Transfer aborted.");
  } else {
    mark(225, "No transfer in progress.");
  }
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
  def_cmd(CWD)
  def_cmd(PWD)
  def_cmd(MKD)
  def_cmd(RMD)
  def_cmd(RNFR)
  def_cmd(RNTO)
  def_cmd(LIST)
  def_cmd(RETR)
  def_cmd(STOR)
  def_cmd(ABOR)

#undef def_cmd

  char t[64];
  snprintf(t, sizeof t, "Unknown command \"%s\"", verb);
  send_mark(c->sock_ctl, 202, t);
  return CMD_RESULT_DONE;
}

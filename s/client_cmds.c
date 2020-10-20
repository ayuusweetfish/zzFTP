#include "client.h"
#include "auth.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define mark(_code, _str) send_mark(c->sock_ctl, _code, _str)
#define markf(_code, ...) do { \
  char s[256]; \
  snprintf(s, sizeof s, __VA_ARGS__); \
  mark(_code, s); \
} while (0)

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

#define shutdown(_str) do { \
  mark(421, _str " Shutting down connection."); \
  return CMD_RESULT_SHUTDOWN; \
} while (0)

#define crit(_block) do { \
  pthread_mutex_lock(&c->mutex_dat); \
  _block \
  pthread_mutex_unlock(&c->mutex_dat); \
} while (0)

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

struct passive_data_arg {
  int sock_fd;
  client *c;
};

static void *passive_data(void *arg)
{
  int sock_fd = ((struct passive_data_arg *)arg)->sock_fd;
  client *c = ((struct passive_data_arg *)arg)->c;
  free(arg);

  int conn_fd = -1;
  FILE *fp = NULL;
  enum dat_type_t dat_type = DATA_UNDEFINED;

  const int BUF_SIZE = 1024;
  void *buf = malloc(BUF_SIZE);

  while (1) {
    bool running;
    crit({ running = c->thr_dat_running; });
    if (!running) break;

    if (conn_fd == -1) {
      if ((conn_fd = accept(sock_fd, NULL, NULL)) == -1) {
        if (errno == EAGAIN) {
          usleep(1000000);
          continue;
        } else {
          panic("accept() failed");
        }
      }
    }

    if (conn_fd != -1) {
      if (fp == NULL)
        crit({ fp = c->dat_fp; dat_type = c->dat_type; });
      if (fp != NULL) {
        if (dat_type == DATA_SEND_FILE || dat_type == DATA_SEND_PIPE) {
          size_t bytes_read = fread(buf, 1, BUF_SIZE, fp);
          if (bytes_read > 0)
            write_all(conn_fd, buf, bytes_read);
        } else {
          puts("TODO");
        }
        if (feof(fp)) {
          break;  // Transmission complete
        } else if (ferror(fp) != 0) {
          warn("error reading file");
          break;  // TODO: Inform the client about the error
        }
      }
    }
  }

  free(buf);
  if (fp != NULL) {
    if (dat_type == DATA_SEND_PIPE) pclose(fp);
    else fclose(fp);
  }
  if (conn_fd != -1) close(conn_fd);
  close(sock_fd);

  crit({ c->dat_fp = NULL; });
  c->state = CLST_READY;

  info("data thread terminated");
  return NULL;
}

static cmd_result handler_PASV(client *c, const char *arg)
{
  auth();
  client_close_threads(c);

  uint8_t addr[6];
  int fd;
  if (ephemeral(c->sock_ctl, addr, &fd) != 0 ||
      fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1 ||
      listen(fd, 0) == -1)
    shutdown("Cannot enter passive mode: cannot start data connection.");

  c->state = CLST_PASV;

  // Create thread
  struct passive_data_arg *thr_arg = malloc(sizeof(struct passive_data_arg));
  thr_arg->sock_fd = fd;
  thr_arg->c = c;
  crit({ c->thr_dat_running = true; });
  if (pthread_create(&c->thr_dat, NULL, &passive_data, thr_arg) != 0)
    shutdown("Cannot enter passive mode: pthread_create() failed.");

  markf(227, "Entering Passive Mode ("
    "%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8
  ")", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return CMD_RESULT_DONE;
}

static cmd_result handler_LIST(client *c, const char *arg)
{
  auth();
  if (c->state != CLST_PORT && c->state != CLST_PASV) {
    mark(425, "Use PORT or PASV first.");
    return CMD_RESULT_DONE;
  }

  FILE *f;
  crit({ f = c->dat_fp; });
  if (f != NULL) {
    mark(425, "Data transfer already in progress. Ignoring.");
    return CMD_RESULT_DONE;
  }

  f = popen("ls /tmp", "r");
  if (f == NULL) {
    mark(550, "Internal error. Cannot list.");
    return CMD_RESULT_DONE;
  }

  crit({ c->dat_fp = f; c->dat_type = DATA_SEND_PIPE; });

  mark(150, "Directory listing is being sent over the data connection.");
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
  def_cmd(LIST)

#undef def_cmd

  char t[64];
  snprintf(t, sizeof t, "Unknown command \"%s\"", verb);
  send_mark(c->sock_ctl, 202, t);
  return CMD_RESULT_DONE;
}

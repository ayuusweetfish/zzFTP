#ifndef SLOW_DATA
  #define BUF_SIZE 1024
#else
  #define BUF_SIZE 8
#endif

static inline bool write_block(
  client *c,
  int conn_fd, enum dat_type_t dat_type, FILE *fp, void *buf)
{
  if (dat_type == DATA_SEND_FILE || dat_type == DATA_SEND_PIPE) {
    size_t bytes_read = fread(buf, 1, BUF_SIZE, fp);
    if (bytes_read > 0) {
      write_all(conn_fd, buf, bytes_read);
    #ifdef SLOW_DATA
      usleep(300000);
    #endif
    }
  } else {
    puts("TODO");
  }
  if (feof(fp)) {
    mark(226, "Transfer complete.");
    return false;
  } else if (ferror(fp) != 0) {
    mark(451, "Transfer aborted by internal I/O error.");
    return false;
  }
  return true;
}

static inline void cleanup(
  client *c,
  int conn_fd, enum dat_type_t dat_type, FILE *fp, void *buf)
{
  if (conn_fd != -1) close(conn_fd);

  free(buf);
  if (fp != NULL) {
    if (dat_type == DATA_SEND_PIPE) pclose(fp);
    else fclose(fp);
  }

  crit({ c->dat_fp = NULL; });
  c->state = CLST_READY;

  info("data thread terminated");
}

// Active mode

static void *active_data(void *arg)
{
  client *c = (client *)arg;

  int conn_fd = -1;
  FILE *fp = NULL;
  enum dat_type_t dat_type = DATA_UNDEFINED;
  void *buf = malloc(BUF_SIZE);

  while (1) {
    bool running;
    crit({ running = c->thr_dat_running; });
    if (!running) break;

    if (fp == NULL) {
      crit({ fp = c->dat_fp; dat_type = c->dat_type; });
      if (fp != NULL) {
        // Establish connection
        conn_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (conn_fd == -1) {
          mark(425, "Cannot establish connection: socket() failed.");
          break;
        }
        // fcntl(conn_fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1 ||
        // Fill in the address from client record
        struct sockaddr_in addr = { 0 };
        addr.sin_family = AF_INET;
        memcpy(&addr.sin_addr.s_addr, c->addr, 4);
        addr.sin_port = htons(c->port);
        // TODO: Connect with a timeout
        if (connect(conn_fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
          mark(425, "Cannot establish connection.");
          break;
        }
      } else {
        usleep(1000000);  // TODO: Use pthread_cond
        continue;
      }
    }

    if (fp != NULL)
      if (!write_block(c, conn_fd, dat_type, fp, buf)) break;
  }

  cleanup(c, conn_fd, dat_type, fp, buf);
  return NULL;
}

// Passive mode

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
      if (fp != NULL)
        if (!write_block(c, conn_fd, dat_type, fp, buf)) break;
    }
  }

  close(sock_fd);
  cleanup(c, conn_fd, dat_type, fp, buf);
  return NULL;
}

#undef BUF_SIZE

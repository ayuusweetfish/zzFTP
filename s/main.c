#include "io_utils.h"
#include "client.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

void *serve_client(void *arg)
{
  int conn_fd = *(int *)arg;
  free(arg);

  client *c = client_create(conn_fd);
  client_run_loop(c);
  client_close(c);
  return NULL;
}

void print_usage(char *argv0, int exit_code)
{
  printf("usage: %s [-port <n>] [-root <path>]\n", argv0);
  exit(exit_code);
}

int main(int argc, char *argv[])
{
  // Parse arguments
  int port = 21;
  const char *root = "/tmp";

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
      print_usage(argv[0], 0);
    } else if (strcmp(argv[i], "-port") == 0) {
      if (i + 1 >= argc) print_usage(argv[0], 1);
      if (sscanf(argv[i + 1], "%d", &port) != 1 ||
          port < 0 || port >= 65535)
        print_usage(argv[0], 1);
    } else if (strcmp(argv[i], "-root") == 0) {
      if (i + 1 >= argc) print_usage(argv[0], 1);
      root = argv[i + 1];
    }
  }

  if (chdir(root) != 0)
    panic("chdir() failed");

  signal(SIGPIPE, SIG_IGN);

  // Allocate socket
  int sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_fd == -1)
    panic("socket() failed");

  // Allow successive runs without waiting
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
      &(int){1}, sizeof(int)) == -1)
    panic("setsockopt() failed");

  // Bind to address and start listening
  struct sockaddr_in addr = { 0 };
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock_fd, (struct sockaddr *)&addr, sizeof addr) == -1)
    panic("bind() failed");
  if (listen(sock_fd, 1024) == -1)
    panic("listen() failed");

  // Accept connections
  struct sockaddr_in cli_addr;
  socklen_t cli_addr_len;
  while (1) {
    cli_addr_len = sizeof cli_addr;
    int conn_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &cli_addr_len);
    if (conn_fd == -1)
      panic("accept() failed");

    pthread_t thr;
    int *conn_fd_container = malloc(sizeof(int));
    *conn_fd_container = conn_fd;
    if (pthread_create(&thr, NULL, &serve_client, conn_fd_container) != 0) {
      warn("cannot serve more clients");
      close(conn_fd);
      free(conn_fd_container);
      continue;
    }
    pthread_detach(thr);
  }

  return 0;
}

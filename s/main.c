#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

static inline void panic(const char *msg)
{
  fprintf(stderr, "panic | %s: errno = %d (%s)\n", msg, errno, strerror(errno));
  exit(1);
}

int main(int argc, char *argv[])
{
  int sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_fd == -1)
    panic("socket() failed");

  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
      &(int){1}, sizeof(int)) == -1)
    panic("setsockopt() failed");

  struct sockaddr_in addr = { 0 };
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1800);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock_fd, (struct sockaddr *)&addr, sizeof addr) == -1)
    panic("bind() failed");

  if (listen(sock_fd, 1024) == -1)
    panic("listen() failed");

  struct sockaddr_in cli_addr;
  socklen_t cli_addr_len;
  while (1) {
    cli_addr_len = sizeof cli_addr;
    int conn_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &cli_addr_len);
    if (conn_fd == -1)
      panic("accept() failed");

    write(conn_fd, "qwq\n", 4);
    close(conn_fd);
  }

  return 0;
}

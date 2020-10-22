#ifndef zzftp__io_utils_h
#define zzftp__io_utils_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Prints errno with a message to stderr and exits the program
void panic(const char *msg);
// Prints errno with a message to stderr
void warn(const char *msg);
// Prints a message to stderr
void info(const char *msg);

// Reads up to `len` bytes of data, stopping if read() returns 0
// Returns the number of bytes read
size_t read_all(int fd, void *buf, size_t len);
// Writes `len` bytes of data
// Returns the number of bytes remaining (0 if no errors occurred)
size_t write_all(int fd, const void *buf, size_t len);

// A read-line buffer
typedef struct rlb_s {
  int fd;
  char *buf, *head, *tail;
  bool no_more;
} rlb;

// Prepares for reading from a file descriptor
void rlb_init(rlb *b, int fd);
// Reads a line from the descriptor
size_t rlb_read_line(rlb *b, char *buf, size_t size);
// Releases the resources used, does not touch the descriptor
void rlb_deinit(rlb *b);

// Sends a multiline mark, replacing all LF characters with CR-LF diagraphs
// The message may or may not be terminated with an LF
// Either way, the sent mark is terminated with CR-LF
void send_mark(int fd, int code, const char *msg);

// Creates a new socket and bind it to an ephemeral port.
// Returns a non-negative descriptor on success and a negative integer on errors
int sock_ephemeral();

// Creates a new socket and bind it to an ephemeral port,
// writing the address accessible from the client of `sock_conn` to `o_addr`
// Returns 0 on success and a negative integer on errors
int ephemeral(int sock_conn, uint8_t o_addr[6], int *o_sock_fd);

#endif

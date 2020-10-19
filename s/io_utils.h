#ifndef zzftp__io_utils_h
#define zzftp__io_utils_h

#include <stddef.h>

// Prints errno with a message to stderr and exits the program
void panic(const char *msg);

// Prints errno with a message to stderr
void warn(const char *msg);

// Reads up to `len` bytes of data, stopping if read() returns 0
// Returns the number of bytes read
size_t read_all(int fd, void *buf, size_t len);

// Writes `len` bytes of data
// Returns the number of bytes remaining (0 if no errors occurred)
size_t write_all(int fd, const void *buf, size_t len);

// A read-line buffer
typedef struct rlbuf_s {
  int fd;
  size_t ptr;
  char *buf;
} rlbuf;

// Reads a line into buffer
void read_line(rlbuf b, char *buf, size_t len);

#endif

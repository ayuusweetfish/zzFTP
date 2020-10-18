#ifndef zzftp__io_utils_h
#define zzftp__io_utils_h

#include <stddef.h>

void panic(const char *msg);
size_t write_all(int fd, const void *buf, size_t len);

#endif

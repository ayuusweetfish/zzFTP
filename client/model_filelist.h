#ifndef zzftp_client__model_filelist_h
#define zzftp_client__model_filelist_h

#include "libui/ui.h"

#include <stdio.h>
#include <stdbool.h>

uiTable *file_list_table();

typedef struct file_rec_s {
  int is_dir; // 0 - file; 1 - regular dir; 2 - parent dir
  char *name;
  int size;
  char *date;
} file_rec;

void file_list_clear();
void file_list_set_enabled(bool enabled);
void file_list_reset(int n, file_rec *recs);

// Externally implemented
void file_list_rename(const char *from, const char *to);
void file_list_download(const struct file_rec_s *r);
void file_list_mkdir(const char *name);
void file_list_delete(bool is_dir, const char *name);

// Helper function
static inline void get_size_str(char s[16], size_t x)
{
  if ((x >> 30) > 0)
    snprintf(s, 16, "%.2f GiB", (float)x / (1LL << 30));
  else if ((x >> 20) > 0)
    snprintf(s, 16, "%.2f MiB", (float)x / (1LL << 20));
  else if ((x >> 10) > 0)
    snprintf(s, 16, "%.2f KiB", (float)x / (1LL << 10));
  else
    snprintf(s, 16, "%zd Byte%s", x, x == 1 ? "" : "s");
}

#endif

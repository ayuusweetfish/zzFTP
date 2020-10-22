#ifndef zzftp_client__model_filelist_h
#define zzftp_client__model_filelist_h

#include "libui/ui.h"

#include <stdbool.h>

uiTable *file_list_table();

typedef struct file_rec_s {
  int is_dir; // 0 - file; 1 - regular dir; 2 - parent dir
  char *name;
  int size;
  char *date;
} file_rec;

void file_list_disable();
void file_list_reset(int n, file_rec *recs);

#endif

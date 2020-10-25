#include "model_filelist.h"

#include "res.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uiImage *imgDir, *imgFile, *imgPlus;
static uiTableModel *model;
static uiTableModelHandler modelHandler;
static uiTable *table = NULL;

static bool has_contents = false;
static bool enabled = false;
static file_rec *files = NULL;
static int n_files = 0;

static int modelNumRows(uiTableModelHandler *mh, uiTableModel *m);

void file_list_clear()
{
  for (int i = modelNumRows(&modelHandler, model) - 1; i >= 0; i--)
    uiTableModelRowDeleted(model, i);
  has_contents = false;
}

void file_list_set_enabled(bool _enabled)
{
  enabled = _enabled;
  for (int i = modelNumRows(&modelHandler, model) - 1; i >= 0; i--)
    uiTableModelRowChanged(model, i);
}

static int file_rec_cmp(const void *_a, const void *_b)
{
  const file_rec *a = _a, *b = _b;
  if (a->is_dir != b->is_dir) return b->is_dir - a->is_dir;
  return strcmp(a->name, b->name);
}

void file_list_reset(int n, file_rec *recs)
{
  for (int i = modelNumRows(&modelHandler, model) - 1; i >= 0; i--)
    uiTableModelRowDeleted(model, i);
  if (files != NULL) {
    for (int i = 0; i < n_files; i++) {
      free(files[i].name);
      free(files[i].date);
    }
    free(files);
  }
  files = recs;
  n_files = n;
  has_contents = enabled = true;
  qsort(recs, n, sizeof(file_rec), file_rec_cmp);
  for (int i = 0; i < modelNumRows(&modelHandler, model); i++)
    uiTableModelRowInserted(model, i);
}

enum modelColumn {
  COL_ENABLED = 0,
  COL_ICON,
  COL_NAME,
  COL_SIZE,
  COL_DATE,
  COL_DOWNLOAD,
  COL_RENAMABLE,
  COL_DELETE,
  COL_DELETABLE,
  COL_TEXTCOLOUR,
  COL_EMPTY,
  COL_COUNT
};

static int modelNumColumns(uiTableModelHandler *mh, uiTableModel *m)
{
  return COL_COUNT;
}

static uiTableValueType modelColumnType(
  uiTableModelHandler *mh, uiTableModel *m, int column)
{
  switch (column) {
    case COL_ICON: return uiTableValueTypeImage;
    case COL_ENABLED:
    case COL_RENAMABLE:
    case COL_DELETABLE:
      return uiTableValueTypeInt;
    case COL_TEXTCOLOUR: return uiTableValueTypeColor;
    default: return uiTableValueTypeString;
  }
}

static int modelNumRows(uiTableModelHandler *mh, uiTableModel *m)
{
  return (has_contents ? n_files + 1 : 0);
}

static uiTableValue *modelCellValue(
  uiTableModelHandler *mh, uiTableModel *m, int row, int col)
{
  if (row == n_files) {
    // The last row
    switch (col) {
      case COL_ICON: return uiNewTableValueImage(imgPlus);
      case COL_DOWNLOAD: return uiNewTableValueString("Upload");
      case COL_DELETE: return uiNewTableValueString("Make dir");
      case COL_ENABLED: return uiNewTableValueInt(enabled);
      case COL_RENAMABLE: return uiNewTableValueInt(0);
      case COL_DELETABLE: return uiNewTableValueInt(enabled);
      case COL_TEXTCOLOUR: return uiNewTableValueColor(0, 0, 0, 1);
      default: return uiNewTableValueString("");
    }
  }

  switch (col) {
    case COL_ICON:
      return uiNewTableValueImage(files[row].is_dir ? imgDir : imgFile);
    case COL_NAME: return uiNewTableValueString(files[row].name);
    case COL_SIZE:
      if (files[row].is_dir) {
        return uiNewTableValueString("");
      } else {
        char s[16];
        get_size_str(s, files[row].size);
        return uiNewTableValueString(s);
      }
    case COL_DATE: return uiNewTableValueString(files[row].date);
    case COL_DOWNLOAD:
      return uiNewTableValueString(
        files[row].is_dir == 2 ? "Go up" :
        files[row].is_dir == 1 ? "Open" : "Download");
    case COL_DELETE:
      return uiNewTableValueString(files[row].is_dir != 2 ? "Delete" : "");
    case COL_ENABLED: return uiNewTableValueInt(enabled);
    case COL_RENAMABLE:
    case COL_DELETABLE:
      return uiNewTableValueInt(enabled && files[row].is_dir != 2);
    case COL_TEXTCOLOUR:
      return (files[row].is_dir == 1 ?
        uiNewTableValueColor(0, 0.3, 0.8, 1) :
        uiNewTableValueColor(0, 0, 0, 1));
    case COL_EMPTY: return uiNewTableValueString("");
  }
  return uiNewTableValueString("qwqwq");
}

static void modelSetCellValue(
  uiTableModelHandler *mh, uiTableModel *m, int row, int col, const uiTableValue *val)
{
  printf("%d %d\n", row, col);
  if (col == COL_DOWNLOAD) {
    file_list_download(&files[row]);
  } else if (col == COL_NAME) {
    const char *s = uiTableValueString(val);
    file_list_rename(files[row].name, s);
  }
}

uiTable *file_list_table()
{
  if (table != NULL) return table;

  imgDir = uiNewImage(32, 32);
  uiImageAppend(imgDir, (void *)res_folder_png, 32, 32, 32 * 4);
  imgFile = uiNewImage(32, 32);
  uiImageAppend(imgFile, (void *)res_text_x_generic_png, 32, 32, 32 * 4);
  imgPlus = uiNewImage(32, 32);
  uiImageAppend(imgPlus, (void *)res_list_add_png, 32, 32, 32 * 4);

  modelHandler = (uiTableModelHandler) {
    .NumColumns = modelNumColumns,
    .ColumnType = modelColumnType,
    .NumRows = modelNumRows,
    .CellValue = modelCellValue,
    .SetCellValue = modelSetCellValue,
  };
  model = uiNewTableModel(&modelHandler);
  table = uiNewTable(&(uiTableParams) {
    .Model = model,
    .RowBackgroundColorModelColumn = -1
  });

  uiTableTextColumnOptionalParams p = {
    .ColorModelColumn = COL_TEXTCOLOUR
  };

  uiTableAppendImageTextColumn(table, "Name",
    COL_ICON, COL_NAME, COL_RENAMABLE, &p);
  uiTableAppendTextColumn(table, "Size",
    COL_SIZE, uiTableModelColumnNeverEditable, &p);
  uiTableAppendTextColumn(table, "Modified at",
    COL_DATE, uiTableModelColumnNeverEditable, &p);
  uiTableAppendButtonColumn(table, "",
    COL_DOWNLOAD, COL_ENABLED);
  uiTableAppendButtonColumn(table, "",
    COL_DELETE, COL_DELETABLE);
  uiTableAppendTextColumn(table, "",
    COL_EMPTY, uiTableModelColumnNeverEditable, &p);

  file_rec *fs = malloc(6 * sizeof(file_rec));
  fs[0] = (file_rec) {2, strdup(".."), 0, strdup("")};
  fs[1] = (file_rec) {1, strdup("dir_1"), 0, strdup("quq")};
  fs[2] = (file_rec) {1, strdup("dir_2"), 0, strdup("qvq")};
  fs[3] = (file_rec) {0, strdup("file_1"), 123, strdup("qwq")};
  fs[4] = (file_rec) {0, strdup("file_2"), 456, strdup("qxq")};
  fs[5] = (file_rec) {0, strdup("file_3"), 1023, strdup("uwu")};
  file_list_reset(6, fs);

  return table;
}

#include "model_filelist.h"

#include "res.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uiImage *imgDir, *imgFile, *imgPlus;
static uiTableModel *model;
static uiTableModelHandler modelHandler;
static uiTable *table = NULL;

static file_rec *files = NULL;
static int n_files = 0;

void file_list_reset(int n, file_rec *recs)
{
  for (int i = 0; i <= n_files; i++)
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
  for (int i = 0; i <= n; i++)
    uiTableModelRowInserted(model, i);
}

enum modelColumn {
  COL_ICON = 0,
  COL_NAME,
  COL_SIZE,
  COL_DATE,
  COL_DOWNLOAD,
  COL_RENAME,
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
    case COL_RENAMABLE:
    case COL_DELETABLE:
      return uiTableValueTypeInt;
    case COL_TEXTCOLOUR: return uiTableValueTypeColor;
    default: return uiTableValueTypeString;
  }
}

static int modelNumRows(uiTableModelHandler *mh, uiTableModel *m)
{
  return n_files + 1;
}

static uiTableValue *modelCellValue(
  uiTableModelHandler *mh, uiTableModel *m, int row, int col)
{
  if (row == n_files) {
    // The last row
    switch (col) {
      case COL_ICON: return uiNewTableValueImage(imgPlus);
      case COL_DOWNLOAD: return uiNewTableValueString("Upload");
      case COL_RENAME: return uiNewTableValueString("Make dir");
      case COL_RENAMABLE: return uiNewTableValueInt(1);
      case COL_DELETABLE: return uiNewTableValueInt(0);
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
        char s[32];
        snprintf(s, sizeof s, "%d", files[row].size);
        return uiNewTableValueString(s);
      }
    case COL_DATE: return uiNewTableValueString(files[row].date);
    case COL_DOWNLOAD:
      return uiNewTableValueString(
        files[row].is_dir == 2 ? "Go up" :
        files[row].is_dir == 1 ? "Open" : "Download");
    case COL_RENAME:
      return uiNewTableValueString(files[row].is_dir != 2 ? "Rename" : "");
    case COL_DELETE:
      return uiNewTableValueString(files[row].is_dir != 2 ? "Delete" : "");
    case COL_RENAMABLE: return uiNewTableValueInt(files[row].is_dir != 2);
    case COL_DELETABLE: return uiNewTableValueInt(files[row].is_dir != 2);
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
    COL_ICON, COL_NAME, uiTableModelColumnNeverEditable, &p);
  uiTableAppendTextColumn(table, "Size",
    COL_SIZE, uiTableModelColumnNeverEditable, &p);
  uiTableAppendTextColumn(table, "Modified at",
    COL_DATE, uiTableModelColumnNeverEditable, &p);
  uiTableAppendButtonColumn(table, "",
    COL_DOWNLOAD, uiTableModelColumnAlwaysEditable);
  uiTableAppendButtonColumn(table, "",
    COL_RENAME, COL_RENAMABLE);
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

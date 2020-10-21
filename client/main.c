#include "libui/ui.h"

#include <stdio.h>
#include <stdlib.h>

static int windowOnClosing(uiWindow *w, void *_unused)
{
  uiQuit();
  return 1;
}

static inline uiBox *inputHorizBox(const char *text, uiControl *ent)
{
  uiLabel *lbl = uiNewLabel(text);
  uiBox *box = uiNewHorizontalBox();
  uiBoxSetPadded(box, 1);
  uiBoxAppend(box, uiControl(lbl), 0);
  uiBoxAppend(box, ent, 1);
  return box;
}

int main()
{
  // Initialize
  uiInitOptions o = { 0 };
  const char *err = uiInit(&o);
  if (err != NULL) {
    fprintf(stderr, "Error initializing libui: %s\n", err);
    uiFreeInitError(err);
    exit(1);
  }

  // Create window
  uiWindow *w = uiNewWindow("zzFTP Client", 320, 400, 0);
  uiWindowSetMargined(w, 1);

  // Layout
  uiGrid *grMain = uiNewGrid();
  uiGridSetPadded(grMain, 1);
  uiWindowSetChild(w, uiControl(grMain));

  // Connection bar
  uiGrid *grConn = uiNewGrid();
  uiGridSetPadded(grConn, 1);
  {
    uiEntry *entHost = uiNewEntry();
    uiEntrySetText(entHost, "127.0.0.1");
    uiGridAppend(grConn,
      uiControl(inputHorizBox("Host", uiControl(entHost))),
      0, 0, 1, 1, 1, uiAlignFill, 1, uiAlignFill);

    uiSpinbox *entPort = uiNewSpinbox(1, 65535);
    uiSpinboxSetValue(entPort, 21);
    uiGridAppend(grConn,
      uiControl(inputHorizBox("Port", uiControl(entPort))),
      1, 0, 1, 1, 1, uiAlignFill, 1, uiAlignFill);

    uiEntry *entUser = uiNewEntry();
    uiEntrySetText(entUser, "anonymous");
    uiGridAppend(grConn,
      uiControl(inputHorizBox("User", uiControl(entUser))),
      0, 1, 1, 1, 1, uiAlignFill, 1, uiAlignFill);

    uiEntry *entPass = uiNewPasswordEntry();
    uiGridAppend(grConn,
      uiControl(inputHorizBox("Pass", uiControl(entPass))),
      1, 1, 1, 1, 1, uiAlignFill, 1, uiAlignFill);

    uiButton *btnConn = uiNewButton("Connect");
    uiGridAppend(grConn, uiControl(btnConn),
      0, 2, 2, 1, 1, uiAlignFill, 1, uiAlignFill);
  }
  uiGridAppend(grMain, uiControl(grConn), 0, 0, 3, 1,
    1, uiAlignFill, 0, uiAlignStart);

  // Run
  uiWindowOnClosing(w, windowOnClosing, NULL);
  uiControlShow(uiControl(w));

  uiMain();
  return 0;
}

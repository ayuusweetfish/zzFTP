#include "libui/ui.h"

#include "model_filelist.h"
#include "xfer.h"

#include <stdio.h>
#include <stdlib.h>

static int onShouldQuit(void *_unused)
{
  return 1;
}

static int windowOnClosing(uiWindow *w, void *_unused)
{
  uiQuit();
  return 1;
}

static uiWindow *w;

static uiBox *boxConn;
static uiEntry *entHost, *entUser, *entPass;
static uiSpinbox *entPort;
static uiButton *btnConn;

static uiLabel *lblStatus;
static uiProgressBar *pbar;

static uiBox *boxOp;
static uiButton *btnMode;

static xfer x;
static bool connected = false;

static inline void loading() { uiProgressBarSetValue(pbar, -1); }
static inline void done() { uiProgressBarSetValue(pbar, 0); }
static inline void status(const char *s)
{
  puts(s);
  uiLabelSetText(lblStatus, s);
}
#define statusf(...) do { \
  char s[128]; \
  snprintf(s, 128, __VA_ARGS__); \
  status(s); \
} while (0)

// LIST command
static void auth_read(int code, char *s);
void do_auth()
{
  xfer_write(&x, "USER qwq\r\n", NULL);
  xfer_read_mark(&x, auth_read);
}
static void auth_read(int code, char *s)
{
  // printf("%d [%s]\n", code, s);
  if (code == 220) {
    puts("Logged in");
  }
}

void xfer_done(int code)
{
  done();
  uiControlEnable(uiControl(btnConn));
  if (code == 0) {
    status("Connected!");
    connected = true;
    uiButtonSetText(btnConn, "Disconnect");
    uiControlEnable(uiControl(boxOp));
    do_auth();
  } else {
    status(code == XFER_ERR_CONNECT ?
      "Cannot connect to host" :
      "Internal error during connection");
    uiControlEnable(uiControl(boxConn));
  }
}

void btnConnClick(uiButton *_b, void *_u)
{
  if (!connected) {
    // Connect
    char *host = uiEntryText(entHost);
    int port = uiSpinboxValue(entPort);
    char *user = uiEntryText(entUser);
    char *pass = uiEntryText(entPass);

    loading();
    statusf("Connecting to %s", host);
    uiControlDisable(uiControl(boxConn));
    uiControlDisable(uiControl(btnConn));
    xfer_init(&x, host, port, &xfer_done);

    free(host);
    free(user);
    free(pass);
  } else {
    // Disconnect
    xfer_deinit(&x);
    status("Disconnected");
    connected = false;
    uiControlEnable(uiControl(boxConn));
    uiControlDisable(uiControl(boxOp));
    uiButtonSetText(btnConn, "Connect");
  }
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

  // Workaround for quit menu for macOS
#if __APPLE__
  uiMenu *menuDefault = uiNewMenu("");
  uiMenuAppendQuitItem(menuDefault);
#endif

  // Create window
  w = uiNewWindow("zzFTP Client", 480, 600, 0);
  uiWindowSetMargined(w, 1);

  // Layout
  uiBox *boxMain = uiNewVerticalBox();
  uiBoxSetPadded(boxMain, 1);
  uiWindowSetChild(w, uiControl(boxMain));

  // Connection bar
  boxConn = uiNewVerticalBox();
  uiBoxSetPadded(boxConn, 1);
  {
    uiBox *boxConnR1 = uiNewHorizontalBox();
    uiBoxSetPadded(boxConnR1, 1);
    {
      uiLabel *lblHost = uiNewLabel("Host");
      uiBoxAppend(boxConnR1, uiControl(lblHost), 0);
      entHost = uiNewEntry();
      uiEntrySetText(entHost, "127.0.0.1");
      uiBoxAppend(boxConnR1, uiControl(entHost), 1);

      uiLabel *lblPort = uiNewLabel("Port");
      uiBoxAppend(boxConnR1, uiControl(lblPort), 0);
      entPort = uiNewSpinbox(1, 65535);
      uiSpinboxSetValue(entPort, 21);
      uiBoxAppend(boxConnR1, uiControl(entPort), 0);
    }
    uiBoxAppend(boxConn, uiControl(boxConnR1), 1);

    uiBox *boxConnR2 = uiNewHorizontalBox();
    uiBoxSetPadded(boxConnR2, 1);
    {
      uiLabel *lblUser = uiNewLabel("User");
      uiBoxAppend(boxConnR2, uiControl(lblUser), 0);
      entUser = uiNewEntry();
      uiEntrySetText(entUser, "anonymous");
      uiBoxAppend(boxConnR2, uiControl(entUser), 1);

      uiLabel *lblPass = uiNewLabel("Password");
      uiBoxAppend(boxConnR2, uiControl(lblPass), 0);
      entPass = uiNewPasswordEntry();
      uiBoxAppend(boxConnR2, uiControl(entPass), 1);
    }
    uiBoxAppend(boxConn, uiControl(boxConnR2), 1);
  }
  uiBoxAppend(boxMain, uiControl(boxConn), 0);

  btnConn = uiNewButton("Connect");
  uiButtonOnClicked(btnConn, btnConnClick, NULL);
  uiBoxAppend(boxMain, uiControl(btnConn), 0);

  pbar = uiNewProgressBar();
  uiProgressBarSetValue(pbar, 0);
  uiBoxAppend(boxMain, uiControl(pbar), 0);

  lblStatus = uiNewLabel("Disconnected");
  uiBoxAppend(boxMain, uiControl(lblStatus), 0);

  // Box for operations
  boxOp = uiNewVerticalBox();
  uiBoxSetPadded(boxOp, 1);
  {
    uiBoxAppend(boxOp, uiControl(file_list_table()), 1);

    btnMode = uiNewButton("Mode: Passive");
    uiBoxAppend(boxOp, uiControl(btnMode), 0);
  }
  uiBoxAppend(boxMain, uiControl(boxOp), 1);

  file_list_disable();
  uiControlDisable(uiControl(boxOp));

  // Run
  uiWindowOnClosing(w, windowOnClosing, NULL);
  uiOnShouldQuit(onShouldQuit, NULL);
  uiControlShow(uiControl(w));

  uiMain();
  return 0;
}

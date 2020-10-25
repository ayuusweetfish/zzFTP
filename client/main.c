#include "libui/ui.h"

#include "model_filelist.h"
#include "xfer.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static uiButton *btnMode;

static char *cwd = NULL;
static bool passive_mode = true;

// x - Control connection
// y - Data connection
static xfer x, y;
static bool connected = false;

static inline void loading()
{
  uiProgressBarSetValue(pbar, -1);
  uiControlDisable(uiControl(btnConn));
  file_list_set_enabled(false);
}
static inline void progress(float p)
{
  uiProgressBarSetValue(pbar, (int)(p * 100 + 0.5f));
}
static inline void done()
{
  uiProgressBarSetValue(pbar, 0);
  uiControlEnable(uiControl(btnConn));
  file_list_set_enabled(true);
}
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

// Send/receive data
// If `send_len` is non-negative, the data is sent
// and `next` will be called with an error code and NULL;
// Otherwise the data is received, and `next` will be called with the
// length and the received data
// Note: the function is not re-entrant
static ssize_t data_send_len;
static char *data_send_buf;
static void (*data_inter)();
static void (*data_next)(size_t, char *);
static void data_pasv_1(int code, char *s);
static void data_port_1(int code, char *s);
static void data_2(int code);
static void data_3(size_t len);
void do_data(ssize_t send_len, char *send_buf,
  void (*inter)(), void (*next)(size_t, char *))
{
  loading();
  data_send_len = send_len;
  data_send_buf = send_buf;
  data_inter = inter;
  data_next = next;
  if (passive_mode) {
    // Passive mode
    status("Entering passive mode");
    xfer_write(&x, "PASV\r\n", NULL);
    xfer_read_mark(&x, data_pasv_1);
  } else {
    // Active (port) mode
    status("Entering active (port) mode");
    uint8_t addr[6];
    if (xfer_init_listen_1(&y, &x, addr) != 0) {
      done();
      status("Cannot listen on a local port, try passive mode instead");
      return;
    }
    char s[64];
    snprintf(s, sizeof s, "PORT %d,%d,%d,%d,%d,%d\r\n",
      (int)addr[0], (int)addr[1], (int)addr[2], (int)addr[3],
      (int)addr[4], (int)addr[5]);
    xfer_write(&x, s, NULL);
    xfer_read_mark(&x, data_port_1);
  }
}
static void data_pasv_1(int code, char *s)
{
  if (code != 227) goto exception;

  char *p = s;
  while (*p != '\0' && !isdigit(*p)) p++;
  if (*p == '\0') goto exception;

  unsigned x[6];
  if (sscanf(p, "%u,%u,%u,%u,%u,%u",
        &x[0], &x[1], &x[2], &x[3], &x[4], &x[5]) != 6)
    goto exception;
  for (int i = 0; i < 6; i++)
    if (x[i] >= 256) goto exception;

  char host[16];
  snprintf(host, sizeof host, "%u.%u.%u.%u", x[0], x[1], x[2], x[3]);
  int port = x[4] * 256 + x[5];
  statusf("Passive mode: connecting to %s:%d", host, port);
  xfer_init(&y, host, port, data_2);

  if (data_inter != NULL) (*data_inter)();

  return;
exception:
  done();
  status("Did not see a valid passive mode response");
}
static void data_port_1(int code, char *s)
{
  if (code != 200) goto exception;

  if (data_inter != NULL) (*data_inter)();

  statusf("Port mode: listening");
  xfer_init_listen_2(&y, data_2);

  return;
exception:
  done();
  status("Did not see a valid port mode response");
}
static void data_2(int code)
{
  if (code == 0) {
    status("Data connection established, starting transfer");
    if (data_send_len >= 0) {
      if (data_send_buf != NULL) {
        // Send
        // TODO
      } else {
        // Receive
        xfer_read_all(&y, data_next);
      }
    } else {
      // Receive to file
      xfer_read_all_to(&y, -(int)data_send_len, data_3);
    }
  } else {
    done();
    status("Cannot establish data connection");
  }
}
static void data_3(size_t len)
{
  // Progress
  (*data_next)(len, NULL);
}

// List directory
static void list_1(int code, char *s);
static void list_2();
static void list_3(size_t len, char *data);
void do_list()
{
  loading();
  status("Querying current working directory");
  xfer_write(&x, "PWD\r\n", NULL);
  xfer_read_mark(&x, list_1);
}
static void list_1(int code, char *s)
{
  if (code == 257) {
    if (cwd != NULL) free(cwd);
    // Find the directory enclosed by double quotes
    char *p = strchr(s, '"'),
         *q = strrchr(s, '"');
    if (p != NULL) {
      p++;
      cwd = malloc(q - p + 1);
      memcpy(cwd, p, q - p);
      cwd[q - p] = '\0';
      // Update status
      char *s;
      asprintf(&s, "Directory: %s - retrieving file list", cwd);
      status(s);
      free(s);
      // Retrieve list
      do_data(0, NULL, list_2, list_3);
    } else {
      cwd = NULL;
      done();
      status("Cannot understand server's working directory response");
    }
  } else {
    done();
    status("Cannot query current working directory");
  }
}
static void list_2()
{
  xfer_write(&x, "LIST\r\n", NULL);
  xfer_read_mark(&x, NULL);   // Code 150
}
static void list_3(size_t len, char *data)
{
  xfer_read_mark(&x, NULL);   // Code 226

  done();
  xfer_deinit(&y);
  statusf("Directory: %s", cwd);

  int count = 1;
  for (char *p = data; *p != '\0'; p++) if (*p == '\n') count++;
  file_rec *recs = malloc(count * sizeof(file_rec));

  int actual_count = 0;
  // Parent
  if (strcmp(cwd, "/") != 0)
    recs[actual_count++] = (file_rec) {
      .is_dir = 2,
      .name = strdup(".."),
      .size = 0,
      .date = strdup(""),
    };
  // Entries
  for (char *p = data, *q = p; *p != '\0'; p = q + 1) {
    for (q = p; *q != '\n'; q++) { }
    *q = '\0';
    char attr[16];
    int size;
    char d1[8], d2[8], d3[8];
    char name[64];
    if (sscanf(p, "%15s%*s%*s%*s%d%7s%7s%7s%63s",
          attr, &size, d1, d2, d3, name) == 6) {
      char date[32];
      snprintf(date, sizeof date, "%s %s %s", d1, d2, d3);
      recs[actual_count++] = (file_rec) {
        .is_dir = (attr[0] == 'd'),
        .name = strdup(name),
        .size = size,
        .date = strdup(date),
      };
    }
  }

  file_list_reset(actual_count, recs);
}

// Change directory
static void cwd_1(int code, char *s);
void do_cwd(const char *name)
{
  loading();
  status("Changing working directory");
  char *s;
  asprintf(&s, "CWD %s\r\n", name);
  xfer_write(&x, s, NULL);
  free(s);
  xfer_read_mark(&x, cwd_1);
}
static void cwd_1(int code, char *s)
{
  if (code == 250) {
    do_list();
  } else {
    done();
    status((code == 450 || code == 550) ?
      "Cannot change directory: not available or no access" :
      "Did not see a valid working directory change result");
  }
}

// Rename
static void rename_1(int code, char *s);
static void rename_2(int code, char *s);
static char *rename_from = NULL;
static char *rename_to = NULL;
void do_rename(const char *from, const char *to)
{
  loading();
  rename_from = strdup(from);
  rename_to = strdup(to);
  statusf("Renaming %s to %s (RNFR)", rename_from, rename_to);

  char *s;
  asprintf(&s, "RNFR %s\r\n", rename_from);
  xfer_write(&x, s, NULL);
  free(s);
  xfer_read_mark(&x, rename_1);
}
static void rename_1(int code, char *s)
{
  if (code == 250) {
    char *s;
    asprintf(&s, "RNTO %s\r\n", rename_to);
    xfer_write(&x, s, NULL);
    free(s);
    xfer_read_mark(&x, rename_2);
  } else {
    done();
    status((code == 450 || code == 550) ?
      "Cannot rename file: not available or no access" :
      "Did not see a valid rename-from response");
    free(rename_from);
    free(rename_to);
  }
}
static void rename_2(int code, char *s)
{
  if (code == 250) {
    statusf("Successfully renamed \"%s\" to \"%s\"", rename_from, rename_to);
    do_list();
  } else {
    done();
    status((code == 450 || code == 550) ?
      "Cannot rename file: not available or no access" :
      "Did not see a valid rename-to response");
  }
  free(rename_from);
  free(rename_to);
}

// Rename
static void mkd_1(int code, char *s);
void do_mkd(const char *name)
{
  loading();
  statusf("Making directory \"%s\"", name);
  char *s;
  asprintf(&s, "MKD %s\r\n", name);
  xfer_write(&x, s, NULL);
  free(s);
  xfer_read_mark(&x, mkd_1);
}
static void mkd_1(int code, char *s)
{
  done();
  if (code == 250) {
    status("Successfully created new directory");
    do_list();
  } else {
    status((code == 450 || code == 550) ?
      "Cannot create directory: not available or no access" :
      "Did not see a valid directory creation result");
  }
}

// Delete
static void rm_1(int code, char *s);
void do_rm(bool is_dir, const char *name)
{
  loading();
  statusf("Removing \"%s\"", name);
  char *s;
  asprintf(&s, "%s %s\r\n", is_dir ? "RMD" : "DELE", name);
  xfer_write(&x, s, NULL);
  free(s);
  xfer_read_mark(&x, rm_1);
}
static void rm_1(int code, char *s)
{
  done();
  if (code == 250) {
    status("Successfully removed");
    do_list();
  } else {
    status((code == 450 || code == 550) ?
      "Cannot delete: not available, no access, or directory non-empty" :
      "Did not see a valid entry deletion result");
  }
}

// Retrieve file
// Non-reentrant
static void retr_1();
static void retr_2(size_t len, char *data);
static const char *retr_name = NULL;
static int retr_size = 0;
static char retr_size_str[16];
static int retr_out_fd = -1;
void do_retr(const char *name, int size, const char *local_path)
{
  loading();
  statusf("Requesting file %s", name);

  int out_fd = open(local_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out_fd == -1) {
    done();
    statusf("Cannot create local file: %s", strerror(errno));
    return;
  }

  retr_name = name;
  retr_size = size;
  get_size_str(retr_size_str, size);
  retr_out_fd = out_fd;
  do_data(-out_fd, NULL, retr_1, retr_2);
}
static void retr_1()
{
  char *s;
  asprintf(&s, "RETR %s\r\n", retr_name);
  xfer_write(&x, s, NULL);
  xfer_read_mark(&x, NULL);   // Code 150
}
static void retr_2(size_t len, char *data)
{
  if (len == (size_t)-1) {
    // Finish
    xfer_read_mark(&x, NULL); // Code 226
    close(retr_out_fd);

    done();
    statusf("Successfully downloaded \"%s\" (%s)", retr_name, retr_size_str);
  } else {
    char size_xferred[16];
    get_size_str(size_xferred, len);
    statusf("Downloading \"%s\" (%s / %s)",
      retr_name, size_xferred, retr_size_str);
    progress((float)len / retr_size);
  }
}

// Connect to the server and log in
static void auth_1(int code, char *s);
static void auth_2(int code, char *s);
static void auth_3(int code, char *s);
void do_auth()
{
  status("Connected, waiting for welcome message");
  xfer_read_mark(&x, auth_1);
}
static void auth_1(int code, char *s)
{
  if (code == 220) {
    status("Welcome message received, logging in");
    char *user = uiEntryText(entUser);
    char *s;
    asprintf(&s, "USER %s\r\n", user);
    xfer_write(&x, s, NULL);
    free(s);
    free(user);
    xfer_read_mark(&x, auth_2);
  } else {
    done();
    status("Did not see a valid welcome mark");
    uiControlEnable(uiControl(boxConn));
  }
}
static void auth_2(int code, char *s)
{
  if (code == 331) {
    char *pass = uiEntryText(entPass);
    asprintf(&s, "PASS %s\r\n", pass);
    xfer_write(&x, s, NULL);
    free(s);
    free(pass);
    xfer_read_mark(&x, auth_3);
  } else {
    done();
    status("Did not see a valid password request mark");
    uiControlEnable(uiControl(boxConn));
  }
}
static void auth_3(int code, char *s)
{
  done();
  if (code == 230) {
    status("Logged in");
    connected = true;
    uiButtonSetText(btnConn, "Disconnect");
    do_list();
  } else {
    status(code == 530 ?
      "Incorrect credentials" :
      "Did not see a valid log in mark");
    uiControlEnable(uiControl(boxConn));
  }
}

// Callback on connected
void connection_setup(int code)
{
  if (code == 0) {
    do_auth();
  } else {
    done();
    status(code == XFER_ERR_CONNECT ?
      "Cannot connect to the server" :
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

    loading();
    statusf("Connecting to %s", host);
    uiControlDisable(uiControl(boxConn));
    xfer_init(&x, host, port, &connection_setup);

    free(host);
  } else {
    // Disconnect
    xfer_deinit(&x);
    done();
    status("Disconnected");
    connected = false;
    uiControlEnable(uiControl(boxConn));
    uiButtonSetText(btnConn, "Connect");
    file_list_clear();
  }
}

void btnModeClick(uiButton *b, void *_u)
{
  uiButtonSetText(b, (passive_mode ^= 1) ?
    "Current mode: Passive" : "Current mode: Active");
}

void file_list_rename(const char *from, const char *to)
{
  do_rename(from, to);
}

void file_list_download(const struct file_rec_s *r)
{
  if (r->is_dir) {
    do_cwd(r->name);
  } else {
    char *local_path = uiSaveFile(w);
    if (local_path != NULL)
      do_retr(r->name, r->size, local_path);
  }
}

void file_list_mkdir(const char *name)
{
  do_mkd(name);
}

void file_list_delete(bool is_dir, const char *name)
{
  do_rm(is_dir, name);
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
  uiBox *boxOp = uiNewVerticalBox();
  uiBoxSetPadded(boxOp, 1);
  {
    uiBoxAppend(boxOp, uiControl(file_list_table()), 1);
  }
  uiBoxAppend(boxMain, uiControl(boxOp), 1);

  file_list_clear();
  uiControlDisable(uiControl(boxOp));

  btnMode = uiNewButton("Current mode: Passive");
  uiBoxAppend(boxMain, uiControl(btnMode), 0);
  uiButtonOnClicked(btnMode, btnModeClick, NULL);

  // Run
  uiWindowOnClosing(w, windowOnClosing, NULL);
  uiOnShouldQuit(onShouldQuit, NULL);
  uiControlShow(uiControl(w));

  uiMain();
  return 0;
}

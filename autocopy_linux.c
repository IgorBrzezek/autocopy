#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/record.h>
#include <X11/extensions/Xfixes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>

#define APP_VERSION "0.0.5-linux"
#define APP_AUTHOR "Igor Brzezek"
#define MAX_PATH 260

// Global configuration
bool bRequireAlt = false;
bool bRequireCtrl = false;
bool bBatch = false;
bool bShowText = false;
bool bCtrl1 = false;
bool bCtrl2 = false;
bool bTUI = false;
int nRequiredClicks = 1;
int nCurrentClicks = 0;
Time lastClickTime = 0;
int maxDoubleClickTime = 500;
int minTime = 0;
int maxTime = 500;

// Statistics and info
int nTotalTexts = 0;
long long nTotalChars = 0;
char szStartTime[64] = {0};
char szLogFile[MAX_PATH] = {0};
char szArgsInfo[512] = "Arguments: ";

// TUI state
#define MAX_TUI_LINES 10000
char *tuiLogBuffer[MAX_TUI_LINES] = {0};
int tuiLogCount = 0;
int tuiScrollOffset = 0;
int tuiSelectedLine = -1;
int tuiMaxLogLines = 200;
int tuiLineSizeLimit = 4096;
int terminalWidth = 80;
int terminalHeight = 24;
bool bShouldRedrawLogs = false;

// Clipboard for copy to clipboard feature
char *copyClipboardText = NULL;
Window clipboardWindow = None;
Display *clipboardDisplay = NULL;
pthread_mutex_t clipboardMutex = PTHREAD_MUTEX_INITIALIZER;
bool clipboardShouldExit = false;

// Global Ctrl key state for TUI mode
bool bCtrlKeyPressed = false;
pthread_mutex_t ctrlKeyMutex = PTHREAD_MUTEX_INITIALIZER;

// Global terminal settings for cleanup
struct termios g_original_termios;
bool g_termios_saved = false;

Display *ctrl_display = NULL;

void cleanup_and_exit(int sig) {
  // Restore terminal settings
  if (g_termios_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
  }
  
  // Restore cursor visibility
  printf("\033[?25h");
  printf("\033[0m");
  printf("\033[J");
  fflush(stdout);
  
  exit(0);
}

void GetTerminalSize();
void DrawTUIHeader();
void RedrawTUILogs();
void AddTUILogMessage(const char *text);
void CopyToClipboard(const char *text);
void *clipboard_handler_thread(void *arg);
void ShowLongHelp(const char *name);
void ShowShortHelp(const char *name);

void GetTerminalSize() {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    terminalWidth = w.ws_col;
    terminalHeight = w.ws_row;
  } else {
    terminalWidth = 80;
    terminalHeight = 24;
  }
}

void DrawTUIHeader() {
  GetTerminalSize();

  printf("\033[H");
  printf("\033[2J");

  printf("\033[1;1H");
  printf("\033[44m\033[97m");
  char line1[512];
  snprintf(line1, sizeof(line1), " autocopy v%s | Started: %s (CTRL-C to stop)",
           APP_VERSION, szStartTime);
  printf("%-*s", terminalWidth, line1);
  printf("\033[0m");

  printf("\033[2;1H");
  printf("\033[42m\033[97m");
  printf("%-*s", terminalWidth, szArgsInfo);
  printf("\033[0m");

  printf("\033[3;1H");
  printf("\033[42m\033[97m");
  double avg = (nTotalTexts > 0) ? (double)nTotalChars / nTotalTexts : 0.0;
  char line3[512];
  snprintf(line3, sizeof(line3),
           " Copied: %d | Total Chars: %lld | Avg Len: %.2f",
           nTotalTexts, nTotalChars, avg);
  printf("%-*s", terminalWidth, line3);
  printf("\033[0m");

  printf("\033[?25l");

  fflush(stdout);
}

void RedrawTUILogs() {
  GetTerminalSize();

  int logAreaHeight = terminalHeight - 3;
  int startLine = tuiScrollOffset;

  if (startLine < 0) startLine = 0;
  if (startLine > tuiLogCount - logAreaHeight) startLine = tuiLogCount - logAreaHeight;
  if (startLine < 0) startLine = 0;

  tuiScrollOffset = startLine;

  for (int i = 0; i < logAreaHeight; i++) {
    printf("\033[%d;1H", 4 + i);
    printf("\033[K");
    int logIndex = startLine + i;
    if (logIndex < tuiLogCount && tuiLogBuffer[logIndex]) {
      char logLine[1024];
      snprintf(logLine, sizeof(logLine), "[%d]: %s", logIndex + 1, tuiLogBuffer[logIndex]);
      if (logIndex == tuiSelectedLine) {
        printf("\033[47m\033[30m");
        printf("%-*.*s", terminalWidth, terminalWidth, logLine);
        printf("\033[0m");
      } else {
        printf("%-*.*s", terminalWidth, terminalWidth, logLine);
      }
    }
  }

  fflush(stdout);
}

void AddTUILogMessage(const char *text) {
  char truncatedText[10000];
  snprintf(truncatedText, sizeof(truncatedText), "%.*s", tuiLineSizeLimit, text);

  if (tuiLogCount < tuiMaxLogLines) {
    tuiLogBuffer[tuiLogCount] = strdup(truncatedText);
    tuiLogCount++;
  } else {
    free(tuiLogBuffer[0]);
    for (int i = 0; i < tuiMaxLogLines - 1; i++) {
      tuiLogBuffer[i] = tuiLogBuffer[i + 1];
    }
    tuiLogBuffer[tuiMaxLogLines - 1] = strdup(truncatedText);
  }

  int logAreaHeight = terminalHeight - 3;
  if (tuiLogCount > logAreaHeight) {
    tuiScrollOffset = tuiLogCount - logAreaHeight;
  }

  tuiSelectedLine = tuiLogCount - 1;

  DrawTUIHeader();
  RedrawTUILogs();
}

void *keyboard_input_thread(void *arg) {
  struct termios newt;
  tcgetattr(STDIN_FILENO, &g_original_termios);
  g_termios_saved = true;
  newt = g_original_termios;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);

  // Get keycodes for Ctrl keys
  static KeyCode ctrl_l_code = 0, ctrl_r_code = 0;
  static bool ctrl_key_codes_initialized = false;

  if (!ctrl_key_codes_initialized && ctrl_display) {
    ctrl_l_code = XKeysymToKeycode(ctrl_display, XK_Control_L);
    ctrl_r_code = XKeysymToKeycode(ctrl_display, XK_Control_R);
    ctrl_key_codes_initialized = true;
  }

  int ch;
  while (1) {
    ch = getchar();
    if (ch == 27) {
      int ch2 = getchar();
      int ch3 = getchar();
      if (ch2 == 91) {
        if (ch3 == 65) {
          if (tuiSelectedLine > 0) {
            tuiSelectedLine--;
            if (tuiSelectedLine < tuiScrollOffset) {
              tuiScrollOffset = tuiSelectedLine;
            }
          }
          DrawTUIHeader();
          RedrawTUILogs();
        } else if (ch3 == 66) {
          int logAreaHeight = terminalHeight - 3;
          if (tuiSelectedLine < tuiLogCount - 1) {
            tuiSelectedLine++;
            if (tuiSelectedLine >= tuiScrollOffset + logAreaHeight) {
              tuiScrollOffset = tuiSelectedLine - logAreaHeight + 1;
            }
          }
          DrawTUIHeader();
          RedrawTUILogs();
        }
      }
    } else if (ch == 10 || ch == 13) {
      // Check if Ctrl is pressed using XQueryKeymap
      bool ctrl_pressed = false;
      if (ctrl_display && (ctrl_l_code || ctrl_r_code)) {
        char keys_return[32];
        XQueryKeymap(ctrl_display, keys_return);
        if (ctrl_l_code && (keys_return[ctrl_l_code >> 3] & (1 << (ctrl_l_code & 7)))) {
          ctrl_pressed = true;
        }
        if (ctrl_r_code && (keys_return[ctrl_r_code >> 3] & (1 << (ctrl_r_code & 7)))) {
          ctrl_pressed = true;
        }
      }
      
      if (ctrl_pressed && tuiSelectedLine >= 0 && tuiSelectedLine < tuiLogCount && tuiLogBuffer[tuiSelectedLine]) {
        CopyToClipboard(tuiLogBuffer[tuiSelectedLine]);
      }
    } else if (ch == 'u' || ch == 'U') {
      tuiScrollOffset--;
      DrawTUIHeader();
      RedrawTUILogs();
    } else if (ch == 'd' || ch == 'D') {
      tuiScrollOffset++;
      DrawTUIHeader();
      RedrawTUILogs();
    }
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
  return NULL;
}


void WriteToLog(const char *text) {
  if (szLogFile[0] == '\0')
    return;
  FILE *f = fopen(szLogFile, "a");
  if (f) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec, text);
    fclose(f);
  }
}

void send_ctrl_c() {
  if (!ctrl_display)
    return;

  // Get keycodes
  KeyCode control = XKeysymToKeycode(ctrl_display, XK_Control_L);
  KeyCode c = XKeysymToKeycode(ctrl_display, XK_c);

  // Press Ctrl
  XTestFakeKeyEvent(ctrl_display, control, True, CurrentTime);
  // Press C
  XTestFakeKeyEvent(ctrl_display, c, True, CurrentTime);
  // Release C
  XTestFakeKeyEvent(ctrl_display, c, False, CurrentTime);
  // Release Ctrl
  XTestFakeKeyEvent(ctrl_display, control, False, CurrentTime);

  XFlush(ctrl_display);
}

char *GetClipboardText() {
  Display *disp = XOpenDisplay(NULL);
  if (!disp)
    return NULL;

  Atom clipboard = XInternAtom(disp, "CLIPBOARD", False);
  Atom utf8 = XInternAtom(disp, "UTF8_STRING", False);
  Atom string_atom = XInternAtom(disp, "STRING", False);
  XEvent event;

  Window owner = XGetSelectionOwner(disp, clipboard);
  if (owner == None) {
    XCloseDisplay(disp);
    return NULL;
  }

  Window win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 1, 1, 0, 0, 0);

  XSelectInput(disp, win, PropertyChangeMask);

  XConvertSelection(disp, clipboard, utf8, utf8, win, CurrentTime);
  XFlush(disp);

  char *result = NULL;
  for (int i = 0; i < 20; i++) {
    if (XCheckTypedEvent(disp, SelectionNotify, &event)) {
      XSelectionEvent *se = (XSelectionEvent *)&event.xselection;
      if (se->property != None) {
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *prop;

        if (XGetWindowProperty(disp, win, se->property,
                               0, (1024 * 1024) / 4, False, AnyPropertyType,
                               &type, &format, &nitems, &bytes_after, &prop) == Success) {
          if (prop && nitems > 0) {
            if (type == string_atom || type == utf8 || format == 8) {
              result = strdup((char *)prop);
            }
            XFree(prop);
          }
          XDeleteProperty(disp, win, se->property);
        }
      }
      break;
    }
    usleep(50000);
  }

  XDestroyWindow(disp, win);
  XCloseDisplay(disp);

  return result;
}

void CopyToClipboard(const char *text) {
  if (!text)
    return;

  pthread_mutex_lock(&clipboardMutex);
  free(copyClipboardText);
  copyClipboardText = strdup(text);
  pthread_mutex_unlock(&clipboardMutex);

  if (!clipboardDisplay)
    return;

  Atom clipboard = XInternAtom(clipboardDisplay, "CLIPBOARD", False);
  XSetSelectionOwner(clipboardDisplay, clipboard, clipboardWindow, CurrentTime);
  XFlush(clipboardDisplay);
}

void *clipboard_handler_thread(void *arg) {
  clipboardDisplay = XOpenDisplay(NULL);
  if (!clipboardDisplay)
    return NULL;

  Atom utf8 = XInternAtom(clipboardDisplay, "UTF8_STRING", False);
  Atom string_atom = XInternAtom(clipboardDisplay, "STRING", False);
  Atom targets_atom = XInternAtom(clipboardDisplay, "TARGETS", False);
  Atom atom_atom = XInternAtom(clipboardDisplay, "ATOM", False);

  clipboardWindow = XCreateSimpleWindow(clipboardDisplay, DefaultRootWindow(clipboardDisplay),
                                        0, 0, 10, 10, 0, 0, 0);

  XSelectInput(clipboardDisplay, clipboardWindow, PropertyChangeMask);
  XFlush(clipboardDisplay);

  XEvent event;
  while (!clipboardShouldExit) {
    if (XCheckTypedEvent(clipboardDisplay, SelectionRequest, &event)) {
      if (event.type == SelectionRequest) {
        XSelectionRequestEvent *req = (XSelectionRequestEvent *)&event;
        XEvent response;
        response.xselection.type = SelectionNotify;
        response.xselection.requestor = req->requestor;
        response.xselection.selection = req->selection;
        response.xselection.target = req->target;
        response.xselection.time = req->time;

        pthread_mutex_lock(&clipboardMutex);
        if (req->target == targets_atom) {
          Atom supported_targets[] = {utf8, string_atom};
          XChangeProperty(clipboardDisplay, req->requestor, req->property,
                         atom_atom, 32, PropModeReplace,
                         (unsigned char *)supported_targets, 2);
          response.xselection.property = req->property;
        } else if (req->target == utf8 || req->target == string_atom) {
          if (copyClipboardText) {
            XChangeProperty(clipboardDisplay, req->requestor, req->property,
                           string_atom, 8, PropModeReplace,
                           (unsigned char *)copyClipboardText,
                           strlen(copyClipboardText));
            response.xselection.property = req->property;
          } else {
            response.xselection.property = None;
          }
        } else {
          response.xselection.property = None;
        }
        pthread_mutex_unlock(&clipboardMutex);

        XSendEvent(clipboardDisplay, req->requestor, False, 0, &response);
        XFlush(clipboardDisplay);
      }
    }
    usleep(10000);
  }

  if (clipboardWindow != None) {
    XDestroyWindow(clipboardDisplay, clipboardWindow);
    clipboardWindow = None;
  }
  XCloseDisplay(clipboardDisplay);
  clipboardDisplay = NULL;

  return NULL;
}

void PrintClipboardText() {
  char *text = GetClipboardText();
  if (text) {
    WriteToLog(text);

    if (bTUI) {
      nTotalTexts++;
      nTotalChars += (long long)strlen(text);
      AddTUILogMessage(text);
    } else if (bShowText && !bBatch) {
      printf("[Clipboard]: %s\n", text);
    }

    free(text);
  }
}

void event_callback(XPointer ptr, XRecordInterceptData *data) {
  if (data->category != XRecordFromServer) {
    XRecordFreeData(data);
    return;
  }

  unsigned char *xdata = (unsigned char *)data->data;
  int type = xdata[0];

  if (type == KeyPress || type == KeyRelease) {
    int keycode = xdata[1];
    // Use XKeysymToKeycode to get the correct keycodes for this system
    static int ctrl_l_code = 0;
    static int ctrl_r_code = 0;
    static bool initialized = false;
    
    if (!initialized && ctrl_display) {
      ctrl_l_code = XKeysymToKeycode(ctrl_display, XK_Control_L);
      ctrl_r_code = XKeysymToKeycode(ctrl_display, XK_Control_R);
      initialized = true;
    }
    
    if ((ctrl_l_code && keycode == ctrl_l_code) || 
        (ctrl_r_code && keycode == ctrl_r_code)) {
      pthread_mutex_lock(&ctrlKeyMutex);
      bCtrlKeyPressed = (type == KeyPress);
      pthread_mutex_unlock(&ctrlKeyMutex);
    }
  }

  if (type == ButtonRelease) {
    int button = xdata[1];
    if (button == 1) {
      Time now = data->server_time;
      Time diff = now - lastClickTime;

      if (nCurrentClicks > 0 && (diff < minTime || diff > maxTime)) {
        nCurrentClicks = 1;
      } else {
        nCurrentClicks++;
      }
      lastClickTime = now;

      Window root, child;
      int root_x, root_y, win_x, win_y;
      unsigned int mask;
      XQueryPointer(ctrl_display, DefaultRootWindow(ctrl_display), &root,
                    &child, &root_x, &root_y, &win_x, &win_y, &mask);

      bool alt_pressed = (mask & Mod1Mask);
      bool ctrl_pressed = (mask & ControlMask);

      bool trigger = false;
      if (bRequireAlt && alt_pressed && nCurrentClicks == nRequiredClicks)
        trigger = true;
      else if (bRequireCtrl && ctrl_pressed &&
               nCurrentClicks == nRequiredClicks)
        trigger = true;
      else if (!bRequireAlt && !bRequireCtrl &&
               nCurrentClicks == nRequiredClicks)
        trigger = true;

      bool ctrl_short_trigger = false;
      if (ctrl_pressed) {
        if (bCtrl1 && nCurrentClicks == 1)
          ctrl_short_trigger = true;
        else if (bCtrl2 && nCurrentClicks == 2)
          ctrl_short_trigger = true;
      }

      if (trigger || ctrl_short_trigger) {
        usleep(200000);
        send_ctrl_c();
        nCurrentClicks = 0;

        if (bShowText || bTUI || szLogFile[0] != '\0') {
          usleep(100000);
          PrintClipboardText();
        }
      }
    }
  }

  XRecordFreeData(data);
}

void *record_thread(void *arg) {
  XRecordRange *range;
  XRecordContext context;
  Display *data_display = XOpenDisplay(NULL);

  if (!data_display) {
    fprintf(stderr, "Error: Could not open data display.\n");
    return NULL;
  }

  XRecordRange *range_mouse = XRecordAllocRange();
  range_mouse->device_events.first = ButtonPress;
  range_mouse->device_events.last = ButtonRelease;

  XRecordRange *range_key = XRecordAllocRange();
  range_key->device_events.first = KeyPress;
  range_key->device_events.last = KeyRelease;

  XRecordRange *ranges[2] = {range_mouse, range_key};
  XRecordClientSpec spec = XRecordAllClients;
  context = XRecordCreateContext(data_display, 0, &spec, 1, ranges, 2);
  XFree(range_mouse);
  XFree(range_key);

  if (!context) {
    fprintf(stderr, "Error: Could not create XRecord context.\n");
    return NULL;
  }

  XRecordEnableContext(data_display, context, event_callback, NULL);

  return NULL;
}

void ShowShortHelp(const char *name) {
  printf("autocopy v%s (Linux/X11)\n", APP_VERSION);
  printf("Author: %s\n", APP_AUTHOR);
  printf("Exit: Press Ctrl+C in terminal to exit\n\n");
  printf("Usage: %s [options]\n", name);
  printf("Options: -h --help --version --showtext --1click --2click --3click --alt --ctrl --ctrl1 --ctrl2 --tui --log <file> --logbuffer N --linesize M --mintime <ms> --maxtime <ms> -b --batch\n");
}


void ShowLongHelp(const char *name) {
  printf("autocopy v%s (Linux/X11)\n", APP_VERSION);
  printf("Author: %s\n", APP_AUTHOR);
  printf("Exit: Press Ctrl+C in terminal to exit\n\n");
  printf("Usage: %s [options]\n", name);

  printf("\nGeneral Options:\n");
  printf("  -h                Show short help message\n");
  printf("  --help            Show this detailed help message\n");
  printf("  --version         Show version information\n");
  printf("  --showtext        Show the text copied to clipboard (on by default if not in TUI or batch mode)\n");
  printf("  -b, --batch       Run in batch mode (no output to console, useful for background operation)\n");

  printf("\nClick Options:\n");
  printf("  --1click          Copy after 1 click (default behavior)\n");
  printf("  --2click          Copy after 2 clicks (double click)\n");
  printf("  --3click          Copy after 3 clicks (triple click)\n");

  printf("\nModifier Options:\n");
  printf("  --alt             Only copy if Alt is held down\n");
  printf("  --ctrl            Only copy if Ctrl is held down\n");
  printf("  --ctrl1           Always allow single-click + Ctrl to copy, overriding other click/modifier options\n");
  printf("  --ctrl2           Always allow double-click + Ctrl to copy, overriding other click/modifier options\n");

  printf("\nTUI (Terminal User Interface) Options:\n");
  printf("  --tui             Enable Terminal User Interface mode.\n");
  printf("                    - In TUI mode, use arrow keys to navigate logs.\n");
  printf("                    - Press Ctrl+Enter to copy the selected log line to the system clipboard.\n");
  printf("                    - 'u'/'U': Scroll up.\n");
  printf("                    - 'd'/'D': Scroll down.\n");
  printf("  --logbuffer N     Maximum number of log lines to keep in memory in TUI mode (default: 200).\n");
  printf("  --linesize M      Maximum size of text (in characters) to store per log line (default: 4096).\n");

  printf("\nLogging Options:\n");
  printf("  --log <file>      Log all copied text to the specified file.\n");

  printf("\nTiming Options:\n");
  printf("  --mintime <ms>    Minimum time in milliseconds between clicks to be considered part of a multi-click sequence (default: 0ms).\n");
  printf("  --maxtime <ms>    Maximum time in milliseconds between clicks to be considered part of a multi-click sequence (default: 500ms).\n");
}

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      ShowShortHelp(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--help") == 0) {
      ShowLongHelp(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--version") == 0) {
      printf("autocopy version %s\n", APP_VERSION);
      printf("Author: %s\n", APP_AUTHOR);
      return 0;
    } else if (strcmp(argv[i], "--showtext") == 0) {
      bShowText = true;
    } else if (strcmp(argv[i], "--1click") == 0) {
      nRequiredClicks = 1;
    } else if (strcmp(argv[i], "--2click") == 0) {
      nRequiredClicks = 2;
    } else if (strcmp(argv[i], "--3click") == 0) {
      nRequiredClicks = 3;
    } else if (strcmp(argv[i], "--alt") == 0) {
      bRequireAlt = true;
      bRequireCtrl = false;
    } else if (strcmp(argv[i], "--ctrl") == 0) {
      bRequireCtrl = true;
      bRequireAlt = false;
    } else if (strcmp(argv[i], "--ctrl1") == 0) {
      bCtrl1 = true;
    } else if (strcmp(argv[i], "--ctrl2") == 0) {
      bCtrl2 = true;
    } else if (strcmp(argv[i], "--tui") == 0) {
      bTUI = true;
    } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
      strncpy(szLogFile, argv[++i], MAX_PATH - 1);
    } else if (strcmp(argv[i], "--logbuffer") == 0 && i + 1 < argc) {
      tuiMaxLogLines = atoi(argv[++i]);
      if (tuiMaxLogLines < 1) tuiMaxLogLines = 1;
      if (tuiMaxLogLines > MAX_TUI_LINES) tuiMaxLogLines = MAX_TUI_LINES;
    } else if (strcmp(argv[i], "--libebuffer") == 0 && i + 1 < argc) {
      tuiMaxLogLines = atoi(argv[++i]);
      if (tuiMaxLogLines < 1) tuiMaxLogLines = 1;
      if (tuiMaxLogLines > MAX_TUI_LINES) tuiMaxLogLines = MAX_TUI_LINES;
    } else if (strcmp(argv[i], "--linesize") == 0 && i + 1 < argc) {
      tuiLineSizeLimit = atoi(argv[++i]);
      if (tuiLineSizeLimit < 1) tuiLineSizeLimit = 1;
    } else if (strcmp(argv[i], "--mintime") == 0 && i + 1 < argc) {
      minTime = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--maxtime") == 0 && i + 1 < argc) {
      maxTime = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--batch") == 0) {
      bBatch = true;
    } else {
      printf("Unknown option: %s\n", argv[i]);
      ShowLongHelp(argv[0]);
      return 1;
    }
  }

  signal(SIGINT, cleanup_and_exit);

  ctrl_display = XOpenDisplay(NULL);
  if (!ctrl_display) {
    fprintf(stderr, "Error: Cannot open display. Are you on X11? (ctrl_display is NULL)\n");
    return 1;
  }


  if (bTUI) {
    for (int i = 1; i < argc; i++) {
      strncat(szArgsInfo, argv[i], sizeof(szArgsInfo) - strlen(szArgsInfo) - 2);
      strcat(szArgsInfo, " ");
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(szStartTime, sizeof(szStartTime),
             "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    DrawTUIHeader();
  } else if (!bBatch) {
    printf("autocopy linux started (X11). Press Ctrl+C in terminal to exit.\n");
    printf("Settings: %d click(s)%s%s\n", nRequiredClicks,
           bRequireAlt ? " + Alt" : "", bRequireCtrl ? " + Ctrl" : "");
    printf("Timing: Min %d ms, Max %d ms\n", minTime, maxTime);
    fflush(stdout);
  }

  pthread_t record_thread_id;
  pthread_t keyboard_thread_id;
  pthread_t clipboard_thread_id;

  pthread_create(&record_thread_id, NULL, record_thread, NULL);
  pthread_create(&clipboard_thread_id, NULL, clipboard_handler_thread, NULL);
  usleep(100000);
  if (bTUI) {
    pthread_create(&keyboard_thread_id, NULL, keyboard_input_thread, NULL);
  }

  pthread_join(record_thread_id, NULL);
  clipboardShouldExit = true;
  pthread_join(clipboard_thread_id, NULL);
  if (bTUI) {
    pthread_cancel(keyboard_thread_id);
  }

  if (bTUI) {
    printf("\033[?25h");
    printf("\033[0m");
    printf("\033[J");
    fflush(stdout);
  }

  for (int i = 0; i < tuiLogCount; i++) {
    if (tuiLogBuffer[i]) {
      free(tuiLogBuffer[i]);
    }
  }

  XCloseDisplay(ctrl_display);
  return 0;
}

// Compile with: gcc autocopy_linux.c -o autocopy_linux -lX11 -lXtst -lXfixes -lpthread

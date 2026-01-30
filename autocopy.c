#define _WIN32_WINNT 0x0501
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define APP_AUTHOR "Igor Brzezek"
#define APP_VERSION "0.0.4"
#define APP_DATE "29.01.2026"
#define APP_GITHUB "https://github.com/IgorBrzezek/Auto-Copy"

// Global hook handles
HHOOK hMouseHook;
HHOOK hKeyHook;
// PID of this process
DWORD dwMyPID;

// Configuration flags
BOOL bShowText = TRUE;
BOOL bTUI = FALSE;
BOOL bRequireAlt = FALSE;
BOOL bRequireCtrl = FALSE;
BOOL bCtrl2 = FALSE;
BOOL bCtrl1 = FALSE;
int nRequiredClicks = 1;

// Statistics and info
int nTotalTexts = 0;
long long nTotalChars = 0;
char szStartTime[64] = {0};
char szArgsInfo[256] = "Arguments: ";
char szLogFile[MAX_PATH] = {0};

// TUI Log Buffer
#define MAX_LOG_LINES 100
char *logBuffer[MAX_LOG_LINES] = {0};
int logPos = 0;
int logCount = 0;

// Function to write text to a log file
void WriteToLog(const char *text) {
  if (szLogFile[0] == '\0')
    return;
  FILE *f = fopen(szLogFile, "a");
  if (f) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n", st.wYear, st.wMonth,
            st.wDay, st.wHour, st.wMinute, st.wSecond, text);
    fclose(f);
  }
}

// Timing constraints
DWORD dwMinTime = 0; // ms
DWORD dwMaxTime = 0; // Will be set to GetDoubleClickTime() if 0

// Click tracking
int nClickCount = 0;
DWORD dwLastClickTime = 0;

// Function to draw or update the TUI header
void DrawTUIHeader() {
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    return;

  int width = (int)csbi.dwSize.X;
  COORD coord = {
      0, csbi.srWindow.Top}; // Keep at the very top of the visible window

  // Save current attributes
  WORD oldAttrs = csbi.wAttributes;

  // Line 1: Blue Header (Name, Version, Start Time)
  SetConsoleCursorPosition(hConsole, coord);
  SetConsoleTextAttribute(hConsole, BACKGROUND_BLUE | BACKGROUND_INTENSITY |
                                        FOREGROUND_RED | FOREGROUND_GREEN |
                                        FOREGROUND_BLUE | FOREGROUND_INTENSITY);
  char line1[512];
  snprintf(line1, sizeof(line1), " %s v%s | Started: %s (CTRL-Z to stop)",
           "autocopy", APP_VERSION, szStartTime);
  printf("%-*s", width, line1);

  // Line 2: Green Info Bar (Arguments)
  coord.Y = csbi.srWindow.Top + 1;
  SetConsoleCursorPosition(hConsole, coord);
  SetConsoleTextAttribute(hConsole, BACKGROUND_GREEN | FOREGROUND_RED |
                                        FOREGROUND_GREEN | FOREGROUND_BLUE |
                                        FOREGROUND_INTENSITY);
  printf("%-*s", width, szArgsInfo);

  // Line 3: Green Stats Bar (Count, Total Chars, Avg)
  coord.Y = csbi.srWindow.Top + 2;
  SetConsoleCursorPosition(hConsole, coord);
  double avg = (nTotalTexts > 0) ? (double)nTotalChars / nTotalTexts : 0.0;
  char line3[512];
  snprintf(line3, sizeof(line3),
           " Copied: %d | Total Chars: %I64d | Avg Len: %.2f", nTotalTexts,
           nTotalChars, avg);
  printf("%-*s", width, line3);

  // Reset color
  SetConsoleTextAttribute(hConsole, oldAttrs);
  // Hide cursor to prevent flickering in TUI
  CONSOLE_CURSOR_INFO cursorInfo;
  GetConsoleCursorInfo(hConsole, &cursorInfo);
  cursorInfo.bVisible = FALSE;
  SetConsoleCursorInfo(hConsole, &cursorInfo);
}

// Function to add a message to the TUI log and handle scrolling
void AddLogMessage(const char *text) {
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    return;

  int windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  int logAreaTop = 3;
  int logAreaHeight = windowHeight - logAreaTop;

  if (logAreaHeight <= 0)
    return;

  // If we've reached the bottom of the visible area, scroll it up
  if (logCount >= logAreaHeight) {
    SMALL_RECT scrollRect;
    scrollRect.Left = 0;
    scrollRect.Top = csbi.srWindow.Top + logAreaTop + 1;
    scrollRect.Right = csbi.dwSize.X - 1;
    scrollRect.Bottom = csbi.srWindow.Bottom;

    COORD destOrigin = {0, (SHORT)(csbi.srWindow.Top + logAreaTop)};

    CHAR_INFO fill;
    fill.Char.UnicodeChar = L' ';
    fill.Attributes = csbi.wAttributes;

    ScrollConsoleScreenBuffer(hConsole, &scrollRect, NULL, destOrigin, &fill);
    SetConsoleCursorPosition(hConsole, (COORD){0, (SHORT)csbi.srWindow.Bottom});
  } else {
    SetConsoleCursorPosition(
        hConsole,
        (COORD){0, (SHORT)(csbi.srWindow.Top + logAreaTop + logCount)});
  }

  // Truncate text and pad to clear the line
  int width = (int)csbi.dwSize.X - 1;
  char logLine[1024];
  snprintf(logLine, sizeof(logLine), "[%d]: %s", nTotalTexts, text);
  printf("%-*.*s", width, width, logLine);

  logCount++;
  if (logCount > logAreaHeight)
    logCount = logAreaHeight;
}

// Function to print current clipboard text content
void PrintClipboardText() {
  if (!OpenClipboard(NULL))
    return;

  HANDLE hData = GetClipboardData(CF_TEXT);
  if (hData != NULL) {
    char *pszText = (char *)GlobalLock(hData);
    if (pszText != NULL) {
      // 1. Write to log if enabled
      WriteToLog(pszText);

      // 2. Handle TUI or Console output
      if (bTUI) {
        nTotalTexts++;
        nTotalChars += (long long)strlen(pszText);
        DrawTUIHeader();
        AddLogMessage(pszText);
      } else if (bShowText) {
        printf("[Clipboard]: %s\n", pszText);
      }

      GlobalUnlock(hData);
    }
  }
  CloseClipboard();
}

// Safe handler for console Control signals
BOOL WINAPI ConsoleHandler(DWORD dwType) {
  if (dwType == CTRL_C_EVENT) {
    // Return TRUE to indicate we handled/ignored the event
    return TRUE;
  }
  return FALSE;
}

// Function to simulate Ctrl+C key press
void SendCtrlC() {
  INPUT inputs[4] = {0};

  // Press Ctrl
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_CONTROL;

  // Press C
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = 'C';

  // Release C
  inputs[2].type = INPUT_KEYBOARD;
  inputs[2].ki.wVk = 'C';
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

  // Release Ctrl
  inputs[3].type = INPUT_KEYBOARD;
  inputs[3].ki.wVk = VK_CONTROL;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

  SendInput(4, inputs, sizeof(INPUT));
}

// Thread wrapper for delayed copy execution
DWORD WINAPI CopyThread(LPVOID lpParam) {
  // Short delay to allow the system/app to process button release
  Sleep(200);

  // SAFETY CHECK:
  // Get the process ID of the currently active window
  HWND hForeground = GetForegroundWindow();
  DWORD dwForegroundPID;
  GetWindowThreadProcessId(hForeground, &dwForegroundPID);

  // If active window belongs to this process, do NOT send Ctrl+C
  if (dwForegroundPID == dwMyPID) {
    return 0;
  }

  SendCtrlC();

  // Process clipboard if any output mode is enabled
  if (bShowText || bTUI || szLogFile[0] != '\0') {
    Sleep(100); // Give system time to update clipboard
    PrintClipboardText();
  }

  return 0;
}

// Low-level mouse hook procedure
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION) {
    if (wParam == WM_LBUTTONUP) {
      DWORD dwCurrentTime = GetTickCount();
      DWORD dwDiff = dwCurrentTime - dwLastClickTime;

      // Check timing constraints
      if (nClickCount > 0 && (dwDiff < dwMinTime || dwDiff > dwMaxTime)) {
        nClickCount = 1;
      } else {
        nClickCount++;
      }
      dwLastClickTime = dwCurrentTime;

      // Primary trigger check (Current settings)
      BOOL bModifierPressed = TRUE;
      if (bRequireAlt) {
        bModifierPressed = (GetAsyncKeyState(VK_LMENU) & 0x8000);
      } else if (bRequireCtrl) {
        bModifierPressed = (GetAsyncKeyState(VK_LCONTROL) & 0x8000);
      }

      BOOL bPrimaryTrigger =
          (bModifierPressed && nClickCount == nRequiredClicks);

      // Secondary trigger check (--ctrl1/--ctrl2)
      BOOL bCtrlShortTrigger = FALSE;
      if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        if (bCtrl1 && nClickCount == 1)
          bCtrlShortTrigger = TRUE;
        else if (bCtrl2 && nClickCount == 2)
          bCtrlShortTrigger = TRUE;
      }

      if (bPrimaryTrigger || bCtrlShortTrigger) {
        // Trigger copy thread
        CreateThread(NULL, 0, CopyThread, NULL, 0, NULL);
        nClickCount = 0; // Reset after triggering
      }
    }
  }
  return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

// Low-level keyboard hook procedure to catch CTRL+Z for exit (only if active)
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION) {
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      KBDLLHOOKSTRUCT *pKbdStruct = (KBDLLHOOKSTRUCT *)lParam;
      if (pKbdStruct->vkCode == 'Z') {
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
          if (GetForegroundWindow() == GetConsoleWindow()) {
            PostQuitMessage(0);
          }
        }
      }
    }
  }
  return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
}

void ShowHelp(const char *progName) {
  printf("autocopy v%s\n", APP_VERSION);
  printf("Author: %s\n", APP_AUTHOR);
  printf("Date: %s\n", APP_DATE);
  printf(
      "Exit: CTRL+Z (captured only when window is active) or close window\n\n");
  printf("Usage: %s [options]\n", progName);
  printf("Options:\n");
  printf("  -h, --help        Show this help message\n");
  printf("  --version         Show version information\n");
  printf("  --showtext        Show the text copied to clipboard (on by "
         "default)\n");
  printf("  --1click          Copy after 1 click (default)\n");
  printf("  --2click          Copy after 2 clicks (double click)\n");
  printf("  --3click          Copy after 3 clicks (triple click)\n");
  printf("  --alt             Only copy if Left Alt is held down\n");
  printf("  --ctrl            Only copy if Left Control is held down\n");
  printf("  --ctrl1           Always allow single-click + Ctrl to copy\n");
  printf("  --ctrl2           Always allow double-click + Ctrl to copy\n");
  printf("  --tui             Enable Terminal User Interface mode\n");
  printf("  --log <file>      Log all copied text to specified file\n");
  printf("  --mintime <ms>    Minimum time between clicks (default: 0ms)\n");
  printf("  --maxtime <ms>    Maximum time between clicks (default: system "
         "double-click time)\n");
}

int main(int argc, char *argv[]) {
  // Ensure only one instance is running
  // Using a unique name without "Global\" to avoid permission issues
  HANDLE hMutex = CreateMutex(NULL, FALSE, "AutoCopy2_Instance_Mutex_Igor");
  DWORD dwLastError = GetLastError();

  if (hMutex == NULL) {
    fprintf(stderr, "Error: Could not create instance mutex (error %lu).\n",
            dwLastError);
    Sleep(2000);
    return 1;
  }

  if (dwLastError == ERROR_ALREADY_EXISTS) {
    fprintf(stderr,
            "Error: Another instance of Auto-Copy is already running!\n");
    CloseHandle(hMutex);
    Sleep(2000); // Give user time to see the error
    return 1;
  }

  // Initialize default max time
  dwMaxTime = GetDoubleClickTime();

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      ShowHelp(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--version") == 0) {
      printf("autocopy version %s\n", APP_VERSION);
      printf("Author: %s\n", APP_AUTHOR);
      return 0;
    } else if (strcmp(argv[i], "--showtext") == 0) {
      bShowText = TRUE;
    } else if (strcmp(argv[i], "--1click") == 0) {
      nRequiredClicks = 1;
    } else if (strcmp(argv[i], "--2click") == 0) {
      nRequiredClicks = 2;
    } else if (strcmp(argv[i], "--3click") == 0) {
      nRequiredClicks = 3;
    } else if (strcmp(argv[i], "--alt") == 0) {
      bRequireAlt = TRUE;
      bRequireCtrl = FALSE;
    } else if (strcmp(argv[i], "--ctrl") == 0) {
      bRequireCtrl = TRUE;
      bRequireAlt = FALSE;
    } else if (strcmp(argv[i], "--ctrl1") == 0) {
      bCtrl1 = TRUE;
    } else if (strcmp(argv[i], "--ctrl2") == 0) {
      bCtrl2 = TRUE;
    } else if (strcmp(argv[i], "--tui") == 0) {
      bTUI = TRUE;
    } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
      strncpy(szLogFile, argv[++i], MAX_PATH - 1);
    } else if (strcmp(argv[i], "--mintime") == 0 && i + 1 < argc) {
      dwMinTime = (DWORD)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(argv[i], "--maxtime") == 0 && i + 1 < argc) {
      dwMaxTime = (DWORD)strtoul(argv[++i], NULL, 10);
    } else {
      printf("Unknown option: %s\n", argv[i]);
      ShowHelp(argv[0]);
      return 1;
    }
  }

  // Get our own PID for window detection safety
  dwMyPID = GetCurrentProcessId();

  // Register control handler to prevent closing on Ctrl+C
  if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
    printf("Warning: Could not set control handler.\n");
  }

  printf("autocopy started.\n");
  printf("Settings: %d click(s)%s%s\n", nRequiredClicks,
         bRequireAlt ? " + Left Alt" : "", bRequireCtrl ? " + Left Ctrl" : "");
  printf("Timing: Min %lu ms, Max %lu ms\n", dwMinTime, dwMaxTime);
  printf("Select text with mouse in OTHER windows to copy automatically "
         "(Ctrl+C).\n");
  printf("Clicking this window is safe.\n");
  printf("Close this window to exit.\n");
  if (bShowText) {
    printf("Clipboard monitoring enabled (--showtext).\n");
  }

  if (bTUI) {
    // Collect args info
    for (int i = 1; i < argc; i++) {
      strncat(szArgsInfo, argv[i], sizeof(szArgsInfo) - strlen(szArgsInfo) - 2);
      strcat(szArgsInfo, " ");
    }
    // Get start time
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(szStartTime, sizeof(szStartTime), "%04d-%02d-%02d %02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    system("cls"); // Clear screen for TUI
    DrawTUIHeader();
  }

  // Install low-level mouse hook
  hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc,
                                GetModuleHandle(NULL), 0);

  // Install low-level keyboard hook
  hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                              GetModuleHandle(NULL), 0);

  if (hMouseHook == NULL || hKeyHook == NULL) {
    printf("Error: Failed to install hooks. Error codes: M:%lu K:%lu\n",
           GetLastError(), GetLastError());
    return 1;
  }

  // Message loop
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  UnhookWindowsHookEx(hMouseHook);
  UnhookWindowsHookEx(hKeyHook);

  // Restore cursor visibility
  HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_CURSOR_INFO cursorInfo;
  GetConsoleCursorInfo(hStdout, &cursorInfo);
  cursorInfo.bVisible = TRUE;
  SetConsoleCursorInfo(hStdout, &cursorInfo);

  CloseHandle(hMutex);
  return 0;
}

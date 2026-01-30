#define _WIN32_WINNT 0x0501
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define APP_AUTHOR "Igor Brzezek"
#define APP_VERSION "0.0.5"
#define APP_DATE "30.01.2026"
#define APP_GITHUB "https://github.com/IgorBrzezek/Auto-Copy"

// Global hook handles
HHOOK hMouseHook;
HHOOK hKeyHook;
// PID of this process
DWORD dwMyPID;

// Configuration flags
BOOL bShowText = FALSE;
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
char szArgsInfo[256] = " Arguments: ";
char szLogFile[MAX_PATH] = {0};

// TUI Log Buffer configuration
int maxLines = 100;  // Default maximum lines in buffer (changeable with --maxline)
int maxLineBuffer = 65536;  // Default maximum bytes per line (64KB, changeable with --maxlinebuffer)
char **logBuffer = NULL;  // Dynamically allocated buffer
int logPos = 0;  // Current position in buffer
int logCount = 0;  // Visible count

// Scrolling state for TUI
int scrollOffset = 0;
// Current selected position in TUI log
int selectedIndex = -1;

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
      0, 0};

  // Save current attributes
  WORD oldAttrs = csbi.wAttributes;

  // Line 1: Blue Header (Name, Version, Start Time)
  SetConsoleCursorPosition(hConsole, coord);
  SetConsoleTextAttribute(hConsole, BACKGROUND_BLUE | BACKGROUND_INTENSITY |
                                        FOREGROUND_RED | FOREGROUND_GREEN |
                                        FOREGROUND_BLUE | FOREGROUND_INTENSITY);
  char line1[512];
  snprintf(line1, sizeof(line1), " %s v%s | Started: %s (CTRL-Z to stop, CTRL-ENTER to copy)",
           "autocopy", APP_VERSION, szStartTime);
  printf("%-*.*s", width, width, line1);

  // Line 2: Green Info Bar (Arguments)
  coord.Y = 1;
  SetConsoleCursorPosition(hConsole, coord);
  SetConsoleTextAttribute(hConsole, BACKGROUND_GREEN | FOREGROUND_RED |
                                        FOREGROUND_GREEN | FOREGROUND_BLUE |
                                        FOREGROUND_INTENSITY);
  printf("%-*.*s", width, width, szArgsInfo);

  // Line 3: Green Stats Bar (Count, Total Chars, Avg)
  coord.Y = 2;
  SetConsoleCursorPosition(hConsole, coord);
  double avg = (nTotalTexts > 0) ? (double)nTotalChars / nTotalTexts : 0.0;
  char line3[512];
  snprintf(line3, sizeof(line3),
           " Copied: %d | Total Chars: %I64d | Avg Len: %.2f", nTotalTexts,
           nTotalChars, avg);
  printf("%-*.*s", width, width, line3);

  // Reset color
  SetConsoleTextAttribute(hConsole, oldAttrs);
  // Hide cursor to prevent flickering in TUI
  CONSOLE_CURSOR_INFO cursorInfo;
  GetConsoleCursorInfo(hConsole, &cursorInfo);
  cursorInfo.bVisible = FALSE;
  SetConsoleCursorInfo(hConsole, &cursorInfo);
}

// Function to redraw the log area with scrolling support
void RedrawLogArea() {
  DrawTUIHeader();
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    return;

  int windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  int logAreaTop = 3;
  int logAreaHeight = windowHeight - logAreaTop;

  if (logAreaHeight <= 0)
    return;

  int width = (int)csbi.dwSize.X;

  int maxScroll = logPos - logAreaHeight;
  if (maxScroll < 0)
    maxScroll = 0;
  if (scrollOffset > maxScroll)
    scrollOffset = maxScroll;

  int startIdx = logPos - logAreaHeight - scrollOffset;
  if (startIdx < 0)
    startIdx = 0;

  for (int i = 0; i < logAreaHeight; i++) {
    COORD coord = {0, (SHORT)(logAreaTop + i)};
    SetConsoleCursorPosition(hConsole, coord);

    int logIdx = startIdx + i;
    if (logIdx >= 0 && logIdx < logPos && logBuffer[logIdx] != NULL) {
      char logLine[1024];
      snprintf(logLine, sizeof(logLine), "[%d]: %s", logIdx + 1, logBuffer[logIdx]);

       // Check if this is the selected item and apply white highlight bar
       if (logIdx == selectedIndex) {
         // White background with black text
         SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
       }
       printf("%-*.*s", width, width, logLine);
       // Reset to default colors
       SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    } else {
      printf("%*s", width, "");
    }
  }
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

   // Store text in buffer, respecting maxLineBuffer size limit
   char *textToStore = (char *)text;
   char truncatedText[65536] = {0};  // Local buffer for truncation
   
   // Truncate text if it exceeds maxLineBuffer
   if ((int)strlen(text) > maxLineBuffer) {
     strncpy(truncatedText, text, maxLineBuffer - 1);
     truncatedText[maxLineBuffer - 1] = '\0';
     textToStore = truncatedText;
   }
   
   if (logPos < maxLines) {
     logBuffer[logPos] = strdup(textToStore);
   } else {
     // Buffer is full, remove oldest and shift
     free(logBuffer[0]);
     for (int i = 0; i < maxLines - 1; i++) {
       logBuffer[i] = logBuffer[i + 1];
     }
     logBuffer[maxLines - 1] = strdup(textToStore);
   }
   logPos++;

   // Reset scroll offset when new message arrives
   scrollOffset = 0;

   // Auto-select the first (most recent) item if nothing is selected
   if (selectedIndex < 0) {
     selectedIndex = logPos - 1;
   }

   // Update visible count
   logCount++;
   if (logCount > logAreaHeight)
     logCount = logAreaHeight;

   // Redraw the entire log area
   RedrawLogArea();
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

  // Clear clipboard before copying to avoid mixing old and new content
  if (OpenClipboard(NULL)) {
    EmptyClipboard();
    CloseClipboard();
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
// Also handles scrolling keys for TUI mode
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION) {
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      KBDLLHOOKSTRUCT *pKbdStruct = (KBDLLHOOKSTRUCT *)lParam;
      
      // Handle CTRL+Z for exit
      if (pKbdStruct->vkCode == 'Z') {
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
          if (GetForegroundWindow() == GetConsoleWindow()) {
            PostQuitMessage(0);
          }
        }
      }
      
       // Handle TUI mode controls (scrolling, selection, copy)
       if (bTUI && GetForegroundWindow() == GetConsoleWindow()) {
         HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
         CONSOLE_SCREEN_BUFFER_INFO csbi;
         if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
           int windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
           int logAreaTop = 3;
           int logAreaHeight = windowHeight - logAreaTop;
           
           // Calculate max scroll offset
           int maxScroll = (logPos > logAreaHeight) ? (logPos - logAreaHeight) : 0;
           
           // Handle Up arrow key - move selection up
           if (pKbdStruct->vkCode == VK_UP) {
             if (selectedIndex < 0) {
               selectedIndex = logPos - 1; // Select last item if nothing selected
             } else if (selectedIndex > 0) {
               selectedIndex--;
               // Auto-scroll if needed
               int visibleTop = logPos - logAreaHeight - scrollOffset;
               if (visibleTop < 0) visibleTop = 0;
               if (selectedIndex < visibleTop && scrollOffset < maxScroll) {
                 scrollOffset++;
               }
             }
             RedrawLogArea();
             return 0;
           }
           
           // Handle Down arrow key - move selection down
           if (pKbdStruct->vkCode == VK_DOWN) {
             if (selectedIndex < 0) {
               selectedIndex = 0; // Select first item if nothing selected
             } else if (selectedIndex < logPos - 1) {
               selectedIndex++;
               // Auto-scroll if needed
               int visibleBottom = logPos - scrollOffset - 1;
               if (selectedIndex > visibleBottom && scrollOffset > 0) {
                 scrollOffset--;
               }
             }
             RedrawLogArea();
             return 0;
           }
           
           // Handle CTRL+ENTER to copy selected item to clipboard
           if (pKbdStruct->vkCode == VK_RETURN) {
             if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
               if (selectedIndex >= 0 && selectedIndex < logPos && logBuffer[selectedIndex] != NULL) {
                 // Clear clipboard and copy selected text
                 if (OpenClipboard(NULL)) {
                   EmptyClipboard();
                   
                   size_t textLen = strlen(logBuffer[selectedIndex]);
                   HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, textLen + 1);
                   if (hMem) {
                     char *pMem = (char *)GlobalLock(hMem);
                     if (pMem) {
                       strcpy(pMem, logBuffer[selectedIndex]);
                       GlobalUnlock(hMem);
                       SetClipboardData(CF_TEXT, hMem);
                     }
                   }
                   CloseClipboard();
                 }
               }
               return 0;
             }
           }
         }
       }
    }
  }
  return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
}

// Short help - program info and options in one line
void ShowShortHelp(const char *progName) {
   printf("autocopy v%s | Author: %s | Date: %s | Usage: %s [--tui|--showtext] [--1click|--2click|--3click] [--alt|--ctrl] [--ctrl1] [--ctrl2] [--mintime MS] [--maxtime MS] [--maxline N] [--maxlinebuffer M] [--log FILE] [--help] [--version] [--force]\n",
           APP_VERSION, APP_AUTHOR, APP_DATE, progName);
}

// Detailed help - organized by category
void ShowDetailedHelp(const char *progName) {
   printf("autocopy v%s\n", APP_VERSION);
   printf("Author: %s\n", APP_AUTHOR);
   printf("Date: %s\n", APP_DATE);
   printf("Exit: CTRL+Z (captured only when window is active) or close window\n\n");
   printf("\nUsage: %s [options]\n\n", progName);
   
   // Display modes section
   printf("DISPLAY MODES:\n");
   printf("  --tui                Enable Terminal User Interface mode (key u/d or arrows up/dn to move active line)\n");
   printf("  --showtext           Show copied text in console output\n\n");
   
   // Click triggering section
   printf("CLICK TRIGGERING:\n");
   printf("  --1click             Trigger on single click (default)\n");
   printf("  --2click             Trigger on double click\n");
   printf("  --3click             Trigger on triple click\n\n");
   
   // Modifier keys section
   printf("MODIFIER KEYS:\n");
   printf("  --alt                Require Left Alt to be held down\n");
   printf("  --ctrl               Require Left Control to be held down\n");
   printf("  --ctrl1              Always allow single-click + Ctrl to copy\n");
   printf("  --ctrl2              Always allow double-click + Ctrl to copy\n\n");
   
   // Timing section
   printf("TIMING:\n");
   printf("  --mintime <ms>       Minimum time between clicks (default: 0ms)\n");
   printf("  --maxtime <ms>       Maximum time between clicks (default: system double-click time)\n\n");
   
   // Buffer management section
   printf("BUFFER MANAGEMENT:\n");
   printf("  --maxline <N>        Maximum lines in buffer (default: 100)\n");
   printf("  --maxlinebuffer <M>  Maximum bytes per line (default: 65536 = 64KB)\n\n");
   
   // File and logging section
   printf("FILE AND LOGGING:\n");
   printf("  --log <file>         Log all copied text to specified file\n\n");
   
   // Help and info section
   printf("HELP AND INFORMATION:\n");
   printf("  -h                   Show short help (program info only)\n");
   printf("  --help               Show this detailed help with all options\n");
   printf("  --version            Show version information\n");
}

int main(int argc, char *argv[]) {
   // Check for --force option to ignore mutex check
   BOOL bForce = FALSE;
   for (int i = 1; i < argc; i++) {
     if (strcmp(argv[i], "--force") == 0) {
       bForce = TRUE;
       break;
     }
   }

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

   if (dwLastError == ERROR_ALREADY_EXISTS && !bForce) {
     fprintf(stderr,
             "Error: Another instance of Auto-Copy is already running!\n");
     CloseHandle(hMutex);
     Sleep(2000); // Give user time to see the error
     return 1;
   }

   // Initialize default max time
   dwMaxTime = GetDoubleClickTime();

    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-h") == 0) {
        ShowShortHelp(argv[0]);
        return 0;
      } else if (strcmp(argv[i], "--help") == 0) {
        ShowDetailedHelp(argv[0]);
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
      } else if (strcmp(argv[i], "--maxline") == 0 && i + 1 < argc) {
        maxLines = atoi(argv[++i]);
        if (maxLines < 10) maxLines = 10;  // Enforce minimum of 10 lines
        if (maxLines > 100000) maxLines = 100000;  // Enforce maximum of 100k lines
      } else if (strcmp(argv[i], "--maxlinebuffer") == 0 && i + 1 < argc) {
        maxLineBuffer = atoi(argv[++i]);
        if (maxLineBuffer < 256) maxLineBuffer = 256;  // Enforce minimum of 256 bytes
        if (maxLineBuffer > 1048576) maxLineBuffer = 1048576;  // Enforce maximum of 1MB
      } else if (strcmp(argv[i], "--force") == 0) {
        // --force option already handled at the beginning
        // Silently ignore it here
      } else {
        printf("Unknown option: %s\n", argv[i]);
        ShowShortHelp(argv[0]);
        return 1;
      }
    }

   // Allocate log buffer with dynamic size
   logBuffer = (char **)malloc(maxLines * sizeof(char *));
   if (logBuffer == NULL) {
     fprintf(stderr, "Error: Failed to allocate log buffer memory.\n");
     Sleep(2000);
     return 1;
   }
   memset(logBuffer, 0, maxLines * sizeof(char *));

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
   printf("Buffer: Max %d lines, %d bytes per line\n", maxLines, maxLineBuffer);
   printf("Select text with mouse in OTHER windows to copy automatically "
          "(Ctrl+C).\n");
   printf("Clicking this window is safe.\n");
   printf("Close this window to exit. (CTRL-Z)\n");
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

   // Clean up log buffer memory
   if (logBuffer != NULL) {
     for (int i = 0; i < logPos; i++) {
       if (logBuffer[i] != NULL) {
         free(logBuffer[i]);
       }
     }
     free(logBuffer);
   }

   // Restore cursor visibility
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   CONSOLE_CURSOR_INFO cursorInfo;
   GetConsoleCursorInfo(hStdout, &cursorInfo);
   cursorInfo.bVisible = TRUE;
   SetConsoleCursorInfo(hStdout, &cursorInfo);

   CloseHandle(hMutex);
   return 0;
}

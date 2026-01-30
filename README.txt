autocopy v0.0.5
================

Author: Igor Brzezek
Date: 30.01.2026
GitHub: https://github.com/IgorBrzezek/Auto-Copy

DESCRIPTION
===========
autocopy is a lightweight Windows utility designed to streamline your workflow by automatically copying selected text to the clipboard. It monitors mouse clicks and simulates a "Ctrl+C" command when you release the mouse button, effectively copying whatever text you have just highlighted.

The program includes advanced features such as:
- Flexible click detection (single, double, triple clicks)
- Modifier key requirements (Alt or Ctrl)
- Terminal User Interface (TUI) with live statistics and scrollable log
- File logging capabilities
- Configurable timing constraints
- Dynamic buffer management

HOW IT WORKS
============
The program installs a low-level mouse hook to detect when you finish selecting text (Left Button Up).
To avoid accidental copies or interference with the program itself, several safety measures are included:
1. It does NOT trigger when you click inside its own console window.
2. It includes a smart "Single Instance" check to prevent multiple copies of the program from running simultaneously.
3. It has a built-in signal handler to ensure the program doesn't close accidentally when a "Ctrl+C" event is triggered while it is the active window.
4. It validates the active window's process ID before sending the Ctrl+C command.

Additionally:
- When clicking in other windows (like a browser or text editor), the program waits 200ms before sending the Copy command.
- A global keyboard hook monitors for Ctrl+Z to allow graceful exit when the console is active.
- TUI mode provides real-time statistics: number of copies, total characters, and average text length.

USAGE
=====
Run the executable from the command line:
autocopy.exe [options]

COMMAND-LINE OPTIONS
====================

DISPLAY MODES:
  --tui                Enable Terminal User Interface mode with live statistics and scrollable log
                       TUI Controls: Arrow Up/Down to navigate, Ctrl+Enter to copy selected item
  --showtext           Show copied text in console output (disabled by default)

CLICK TRIGGERING:
  --1click             Trigger on single click / mouse release (default behavior)
  --2click             Trigger on double click
  --3click             Trigger on triple click

MODIFIER KEYS:
  --alt                Only copy if the Left Alt key is held down
  --ctrl               Only copy if the Left Control key is held down
  --ctrl1              Always allow single-click + Ctrl to copy (works independently)
  --ctrl2              Always allow double-click + Ctrl to copy (works independently)

TIMING CONSTRAINTS:
  --mintime <ms>       Minimum time between clicks in milliseconds (default: 0ms)
  --maxtime <ms>       Maximum time between clicks in milliseconds (default: system double-click time ~500ms)

BUFFER MANAGEMENT:
  --maxline <N>        Maximum lines to keep in TUI buffer (default: 100, range: 10-100000)
  --maxlinebuffer <M>  Maximum bytes per line in buffer (default: 65536 = 64KB, range: 256-1048576 = 1MB)

FILE AND LOGGING:
  --log <file>         Log all copied text to specified file with timestamps

HELP AND INFORMATION:
  -h                   Show short help message (program info and all options in one line)
  --help               Show this detailed help with complete documentation
  --version            Show version information
  --force              Ignore instance check (allows multiple instances to run)

EXIT
====
Press Ctrl+Z (when console is active) or close the console window to exit gracefully.

EXAMPLES
========

Example 1: Basic usage with text display
  autocopy.exe --showtext
  Triggers on single click and shows copied text in console.

Example 2: Double-click with Alt modifier
  autocopy.exe --2click --alt
  Triggers only when you double-click while holding Left Alt.

Example 3: TUI mode with file logging
  autocopy.exe --tui --log clipboard_log.txt
  Displays live statistics and logs all copied text to clipboard_log.txt.

Example 4: Triple-click with timing constraints
  autocopy.exe --3click --mintime 200 --maxtime 800 --showtext
  Requires triple-click with 200-800ms timing between clicks, shows text output.

Example 5: Ctrl-based shortcuts with custom buffer
  autocopy.exe --ctrl1 --ctrl2 --maxline 200 --maxlinebuffer 131072 --tui
  Allows both single+Ctrl and double+Ctrl triggers with larger buffer and TUI.

Example 6: Triple-click with Ctrl, file logging and statistics
  autocopy.exe --3click --ctrl --log my_clipboard.log --tui --maxline 50

Example 7: Maximum configuration
  autocopy.exe --tui --showtext --1click --ctrl --ctrl1 --ctrl2 --mintime 100 --maxtime 1000 --maxline 500 --maxlinebuffer 524288 --log output.txt

COMPILATION AND PORTABILITY
============================

Prerequisites:
- MinGW-w64 (GCC for Windows)
  Download from: https://winlibs.com/ or MSYS2
- Make (Optional, for automated builds)

Why Static Linking?
By default, GCC links some libraries dynamically. This can cause errors like 
"The code execution cannot proceed because libgcc_s_dw2-1.dll was not found" 
on machines without MinGW installed. Using the -static flag bundles these 
dependencies into the .exe file, making it truly standalone.

Required Windows API Libraries:
- kernel32      (Core Windows API functions, process/memory management)
- user32        (User interface functions, window/hook management)
- gdi32         (Graphics Device Interface, window rendering)

COMPILATION METHODS
===================

Method 1: Using Makefile (Simplest)
If you have 'make' installed, simply run:
    make

The included Makefile is configured with the -static flag and optimization.

Method 2: Manual Static Compilation (Recommended for Distribution)
Use this command for a fully portable executable:
    gcc autocopy.c -o autocopy.exe -lkernel32 -luser32 -lgdi32 -Wall -O2 -static

Command breakdown:
- gcc autocopy.c                 : Compile the source file
- -o autocopy.exe               : Output executable name
- -lkernel32 -luser32 -lgdi32  : Link Windows API libraries
- -Wall                         : Enable all compiler warnings
- -O2                           : Level 2 optimization for performance
- -static                       : CRITICAL - statically link all libraries for portability

Result: autocopy.exe (~100-200 KB) that runs on any Windows PC without external dependencies.

Method 3: Dynamic Linking (Not Recommended for Distribution)
For local development only:
    gcc autocopy.c -o autocopy.exe -lkernel32 -luser32 -lgdi32 -Wall -O2

This creates a smaller executable but requires MinGW runtime DLLs on target systems.

VERIFYING PORTABILITY
=====================
1. After compilation with -static, the .exe file will be ~100-200 KB
2. Test on a clean Windows system without development tools installed
3. The program should run without errors
4. If you see "dll not found" errors, recompile with -static flag

SAFETY AND BEHAVIOR
===================
- Program waits 200ms after click detection before sending Ctrl+C to allow system processing
- In terminal applications (CMD, PowerShell), you may see a new line appear after click (normal Windows behavior)
- Single Instance Enforcement: Only one autocopy instance can run at a time (use --force to override)
- Process ID Validation: The program checks if the active window belongs to autocopy before sending Ctrl+C
- Safe Ctrl+C Handling: Console handler prevents accidental program closure

TECHNICAL DETAILS
=================

Mouse Hook:
- Uses WH_MOUSE_LL (Low-Level Mouse Hook) for global monitoring
- Detects WM_LBUTTONUP events system-wide
- Validates against self-trigger by checking window process ID

Keyboard Hook:
- Uses WH_KEYBOARD_LL (Low-Level Keyboard Hook)
- Monitors for Ctrl+Z to allow graceful exit
- In TUI mode: handles arrow keys for navigation and Ctrl+Enter for clipboard copy

Instance Control:
- Uses named mutex "AutoCopy2_Instance_Mutex_Igor"
- Prevents simultaneous instances (use --force to bypass)

TUI Features:
- Dynamic log buffer with configurable line limits
- Real-time statistics: copy count, total characters, average length
- Colored output: Blue header, green info bar
- Scrollable log with selection highlighting
- Keyboard navigation and clipboard integration

TROUBLESHOOTING
===============

Issue: "Another instance of Auto-Copy is already running!"
Solution: Close any existing autocopy.exe window or use --force flag

Issue: "libgcc_s_dw2-1.dll was not found"
Solution: Recompile with -static flag: gcc autocopy.c -o autocopy.exe -lkernel32 -luser32 -lgdi32 -static

Issue: Program doesn't copy on some applications
Solution: Try increasing --mintime or --maxtime values, or use --showtext to verify triggering

Issue: TUI display issues
Solution: Ensure your terminal window is at least 40 columns wide and 10 rows tall


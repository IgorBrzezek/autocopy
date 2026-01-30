autocopy v0.0.4
================

Author: Igor Brzezek
Date: 29.01.2026
GitHub: https://github.com/IgorBrzezek/Auto-Copy

Description:
------------
autocopy is a lightweight Windows utility designed to streamline your workflow by automatically copying selected text to the clipboard. It monitors mouse clicks and simulates a "Ctrl+C" command when you release the mouse button, effectively copying whatever text you have just highlighted.

How it works:
-------------
The program installs a low-level mouse hook to detect when you finish selecting text (Left Button Up). 
To avoid accidental copies or interference with the program itself, several safety measures are included:
1. It does NOT trigger when you click inside its own console window.
2. It includes a smart "Single Instance" check to prevent multiple copies of the program from running simultaneously.
3. It has a built-in signal handler to ensure the program doesn't close accidentally when a "Ctrl+C" event is triggered while it is the active window.

Usage:
------
Run the executable from the command line:
autocopy.exe [options]

Options:
--------
  -h, --help        Show the help message
  --version         Show version information
  --showtext        Show the text copied (on by default)
  --1click          Copy after 1 click / mouse release (default behavior)
  --2click          Copy after a double click
  --3click          Copy after a triple click
  --alt             Only copy if the Left Alt key is held down
  --ctrl            Only copy if the Left Control key is held down
  --ctrl1           Always allow single-click + Ctrl to copy
  --ctrl2           Always allow double-click + Ctrl to copy
  --tui             Enable Terminal User Interface mode (Statistics & Log)
  --log <file>      Log all copied text to specified file
  --mintime <ms>    Minimum time between clicks (default: 0ms)
  --maxtime <ms>    Maximum time between clicks (default: system double-click time)

Compilation and Portability:
-----------------------------
To ensure autocopy runs on any Windows machine without requiring external 
dependencies (like MinGW runtime DLLs), the project should be compiled 
with static linking.

Prerequisites:
- MinGW-w64 (GCC for Windows)
- Make (Optional, for automated builds)

Why Static Linking?
By default, GCC links some libraries dynamically. This can cause errors like 
"The code execution cannot proceed because libgcc_s_dw2-1.dll was not found" 
on machines without MinGW installed. Using the -static flag bundles these 
dependencies into the .exe file, making it truly standalone.

Option 1: Using Makefile (Simplest)
If you have 'make' installed, simply run:
    make

The included Makefile is already configured with the -static flag.

Option 2: Manual Compilation (Detailed)
If you prefer to compile manually, use the following command:
    gcc autocopy.c -o autocopy.exe -luser32 -lgdi32 -Wall -O2 -static

Command breakdown:
- gcc autocopy.c: Compiles the source file.
- -o autocopy.exe: Names the output executable.
- -luser32 -lgdi32: Essential Windows API libraries.
- -Wall: Enables all compiler warnings.
- -O2: Enables optimization for better performance.
- -static: CRITICAL for portability. Links all libraries statically.

Verifying Portability:
The generated autocopy.exe will be slightly larger but self-contained. 
You can run it on any Windows PC without installing any extra software.

Safety & Behavior:
------------------
- When clicking in other windows (like a browser or text editor), the program will wait 200ms before sending the Copy command to ensure the system processes the selection first.
- If you are using a terminal (CMD, PowerShell), you might see a new line appearing after a click. This is a side effect of the "Ctrl+C" signal being sent to the console.
- **Exit**: Press **CTRL+Z** (when console is active) or close the console window to exit.

Technical Note:
---------------
This program uses a global mutex named "AutoCopy2_Instance_Mutex_Igor" to ensure single-instance enforcement. If you try to run the program while another instance is active, it will display an error and exit.

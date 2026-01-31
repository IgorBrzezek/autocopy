# autocopy (Linux/X11)

## Purpose
`autocopy` is a utility for Linux (X11) that automates the process of copying selected text to the clipboard. It monitors mouse clicks and keyboard modifier keys (Alt, Ctrl) to automatically trigger a "copy" action (simulating Ctrl+C) on selected text, making it quicker to grab text from applications without explicit selection and copy commands. It also features a Terminal User Interface (TUI) for logging copied text.

## How it Works
The program operates by listening for specific mouse click patterns and optional modifier key presses (Alt or Ctrl). When a configured pattern is detected, it simulates a `Ctrl+C` key press, which typically copies the currently selected text in most X11 applications. The copied text is then stored in the system clipboard. If running in TUI mode, the copied text is also displayed and logged within the terminal.

## Features
-   **Automatic Copying**: Triggers copy action based on mouse clicks (single, double, or triple).
-   **Modifier Key Support**: Optionally requires `Alt` or `Ctrl` to be held down during clicks for copying.
-   **TUI Mode**: Provides an interactive Terminal User Interface to view a log of all copied text.
-   **Log to File**: Option to log all copied text to a specified file.
-   **Clipboard Management**: Manages system clipboard to provide the copied text.

## Usage
Run the program from your terminal with various options:

```bash
./autocopy_linux [options]
```

### Options:
-   `-h`, `--help`: Show this help message.
-   `--version`: Show version information.
-   `--showtext`: Show the text copied to clipboard (on by default if not in TUI or batch mode).
-   `--1click`: Copy after 1 click (default behavior).
-   `--2click`: Copy after 2 clicks (double click).
-   `--3click`: Copy after 3 clicks (triple click).
-   `--alt`: Only copy if `Alt` is held down.
-   `--ctrl`: Only copy if `Ctrl` is held down.
-   `--ctrl1`: Always allow single-click + `Ctrl` to copy, overriding `--1click`, `--2click`, `--3click`, `--alt`, `--ctrl` if specified.
-   `--ctrl2`: Always allow double-click + `Ctrl` to copy, overriding other click/modifier options if specified.
-   `--tui`: Enable Terminal User Interface mode.
    -   In TUI mode, use arrow keys to navigate logs.
    -   Press `Ctrl+Enter` to copy the currently selected log line to the system clipboard.
        -   The selected text will be copied and ready to paste elsewhere.
        -   Useful for quickly retrieving previously copied text from the log.
    -   Press `Ctrl+Shift+Enter` to copy all log entries from the current TUI session to the clipboard.
    -   `u`/`U`: Scroll up one page.
    -   `d`/`D`: Scroll down one page.
    -   `Home`: Jump to the first log entry.
    -   `End`: Jump to the last log entry.
    -   `q`: Quit TUI mode and return to normal operation.
    -   `Ctrl+C`: Exit the entire program.
-   `--log <file>`: Log all copied text to the specified file.
-   `--logbuffer N`: Maximum number of log lines to keep in memory in TUI mode (default: 200).
    -   When the buffer is full, oldest entries are automatically removed (FIFO).
    -   Higher values use more memory but preserve more history.
    -   Set to 0 for unlimited buffer (memory permitting).
-   `--linesize M`: Maximum size of text (in characters) to store per log line (default: 4096).
    -   Text exceeding this limit will be truncated.
    -   Useful for preventing memory issues with very long selections.
-   `--mintime <ms>`: Minimum time in milliseconds between clicks to be considered part of a multi-click sequence (default: 0ms).
-   `--maxtime <ms>`: Maximum time in milliseconds between clicks to be considered part of a multi-click sequence (default: 500ms).
-   `-b`, `--batch`: Run in batch mode (no output to console, useful for background operation).

### Examples:
1.  **Run with default settings (single click copy, console output):**
    ```bash
    ./autocopy_linux
    ```
2.  **Copy on double-click with Alt held down:**
    ```bash
    ./autocopy_linux --2click --alt
    ```
3.  **Run in TUI mode and log to a file:**
    ```bash
    ./autocopy_linux --tui --log ~/autocopy_log.txt
    ```
4.  **Run in batch mode (no console output):**
    ```bash
    ./autocopy_linux -b
    ```
5.  **Copy with single click and Ctrl key**
    ```bash
    ./autocopy_linux --ctrl --1click
    ```
    or simply
    ```bash
    ./autocopy_linux --ctrl1
    ```

## Compilation on Linux (X11)

`autocopy` is a C program that uses X11 libraries for monitoring input events and managing the clipboard. To compile it, you need a C compiler (like GCC) and the development headers for X11, XTest, XFixes, and XRecord extensions.

### Prerequisites:
Make sure you have the necessary development packages installed. On Debian/Ubuntu-based systems, you can install them using:

```bash
sudo apt-get update
sudo apt-get install build-essential libx11-dev libxtst-dev libxfixes-dev libxrecord-dev libxcb-dev libxau-dev libxdmcp-dev
```
On Fedora/RHEL-based systems:
```bash
sudo dnf install gcc make libX11-devel libXtst-devel libXfixes-devel libXrandr-devel libXext-devel libxcb-devel libXau-devel libXdmcp-devel
```

### Compiling:
Navigate to the directory containing `autocopy_linux.c` and run the following command:

#### Dynamic Linking (recommended for most systems):
```bash
gcc autocopy_linux.c -o autocopy_linux -lX11 -lXtst -lXfixes -lpthread
```
This will create a small executable (~38KB) that requires X11 libraries to be installed on the target system.

#### Static Linking (for systems without X11 libraries):
```bash
gcc -static autocopy_linux.c -o autocopy_linux -Wl,--start-group -lX11 -lXtst -lXfixes -lXext -lxcb -lXau -lXdmcp -lpthread -ldl -lrt -lresolv -Wl,--end-group
```
This will create a larger executable (~2.7MB) that contains all required libraries and can run on any Linux system with X11 server, even without development libraries installed.

### Running:
After compilation, you can run the program:

```bash
./autocopy_linux
```

To exit the program, press `Ctrl+C` in the terminal where it's running.

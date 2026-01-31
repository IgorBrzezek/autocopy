# autocopy v0.0.5


## Purpose
**autocopy** is a lightweight Windows utility designed to streamline your workflow by automatically copying selected text to the clipboard. It eliminates the need for manual `Ctrl+C` by detecting when you finish highlighting text with your mouse and simulating the copy command automatically.

---

## 🚀 Features

- **Automatic Clipboard Update**: Simulates `Ctrl+C` immediately after text selection
- **Flexible Click Detection**: Configure to trigger on single, double, or triple clicks
- **Modifier Key Support**: Optionally restrict copying to only when `Alt` or `Ctrl` is held down
- **Ctrl Shortcuts**: Independent `Ctrl+Click` and `Ctrl+DoubleClick` triggers
- **Terminal User Interface (TUI)**: Visual mode with live statistics and scrollable log
- **File Logging**: Log all copied text with timestamps to a file
- **Timing Control**: Set minimum and maximum delays between clicks
- **Buffer Management**: Configurable buffer size for TUI mode
- **Safety Features**:
  - Does not trigger inside its own console window
  - Single Instance Enforcement: Prevents running multiple copies
  - Signal Handling: Prevents accidental closure
  - Process ID Validation: Checks active window ownership

---

## 📋 Requirements

- **Operating System**: Windows (XP SP3 and later with Win32 API support)
- **Architecture**: x86 or x64
- **Compilation**: MinGW-w64 (GCC for Windows)
- **No Runtime Dependencies**: Binary includes all required libraries when compiled with `-static`

---

## 🛠 Usage

Run the executable from your terminal:

```powershell
autocopy.exe [options]
```

### Quick Start

```powershell
# Basic usage with text display
autocopy.exe --showtext

# Double-click with Alt modifier
autocopy.exe --2click --alt

# TUI mode with logging
autocopy.exe --tui --log clipboard.txt

# Show help
autocopy.exe -h

# Most useful
The most useful option is:
 autocopy.exe --2click --tui --showtext
because it "catches" double and triple clicks.
```

### Complete Option Reference

#### Display Modes
| Option | Description |
| :--- | :--- |
| `--tui` | Enable Terminal User Interface with live statistics and scrollable log |
| `--showtext` | Display copied text in console output (disabled by default) |

#### Click Triggering
| Option | Description |
| :--- | :--- |
| `--1click` | Trigger on single click (default) |
| `--2click` | Trigger on double-click |
| `--3click` | Trigger on triple-click |

#### Modifier Keys
| Option | Description |
| :--- | :--- |
| `--alt` | Require **Left Alt** key to be held down |
| `--ctrl` | Require **Left Control** key to be held down |
| `--ctrl1` | Always allow single-click + Ctrl combination |
| `--ctrl2` | Always allow double-click + Ctrl combination |

#### Timing Configuration
| Option | Description |
| :--- | :--- |
| `--mintime <ms>` | Minimum delay between clicks (default: 0ms) |
| `--maxtime <ms>` | Maximum delay between clicks (default: system double-click time ~500ms) |

#### Buffer Management
| Option | Description |
| :--- | :--- |
| `--maxline <N>` | Maximum lines in TUI buffer (default: 100, range: 10-100000) |
| `--maxlinebuffer <M>` | Maximum bytes per line (default: 65536, range: 256-1048576) |

#### File and Logging
| Option | Description |
| :--- | :--- |
| `--log <file>` | Log all copied text to specified file with timestamps |

#### Help and Information
| Option | Description |
| :--- | :--- |
| `-h` | Show short help (program info and options in one line) |
| `--help` | Show detailed help documentation |
| `--version` | Show version information |
| `--force` | Ignore instance check (allow multiple instances) |

### TUI Mode Controls

When using `--tui` option, the following keyboard controls are available:

| Key | Action |
| :--- | :--- |
| **Arrow Up** | Move selection up in log |
| **Arrow Down** | Move selection down in log |
| **Ctrl+Enter** | Copy selected item to clipboard |
| **Ctrl+Z** | Exit program |

---

## 📖 Usage Examples

### Example 1: Basic Text Display
```powershell
autocopy.exe --showtext
```
Triggers on single click and displays all copied text in the console window.

### Example 2: Double-Click with Alt Modifier
```powershell
autocopy.exe --2click --alt
```
Only triggers when you double-click while holding the **Left Alt** key.

### Example 3: TUI Mode with File Logging
```powershell
autocopy.exe --tui --log clipboard_log.txt
```
Displays live statistics and logs all copied text to `clipboard_log.txt` with timestamps.

### Example 4: Triple-Click with Timing
```powershell
autocopy.exe --3click --mintime 200 --maxtime 800 --showtext
```
Requires triple-click with 200-800ms timing between clicks; displays copied text.

### Example 5: Ctrl-Based Shortcuts with Custom Buffer
```powershell
autocopy.exe --ctrl1 --ctrl2 --maxline 200 --maxlinebuffer 131072 --tui
```
Allows both `Ctrl+SingleClick` and `Ctrl+DoubleClick` triggers with larger buffer and TUI.

### Example 6: Comprehensive Configuration
```powershell
autocopy.exe --tui --showtext --2click --ctrl --mintime 100 --maxtime 600 --maxline 500 --log output.txt
```
Multiple options combined: TUI, text display, double-click trigger with Ctrl, timing constraints, larger buffer, and logging.

---

## 🏗 Compilation

### Prerequisites

1. **MinGW-w64**: GCC compiler for Windows
   - Download from [WinLibs](https://winlibs.com/) (Recommended)
   - Or install via [MSYS2](https://www.msys2.org/)
2. **Make** (Optional): For automated builds

### Required Libraries

The program links against these standard Windows API libraries:

- `kernel32` - Core Windows API functions
- `user32` - Window and input management
- `gdi32` - Graphics rendering

### Why Static Linking?

By default, GCC creates executables that depend on runtime DLLs like `libgcc_s_dw2-1.dll`. Using `-static` bundles these into the executable, creating a truly standalone binary that works on any Windows PC.

### Compilation Methods

#### Method 1: Using Makefile (Simplest)

```bash
make
```

The included `Makefile` is configured with optimal flags and static linking.

#### Method 2: Manual Compilation with Static Linking (Recommended)

```bash
gcc autocopy.c -o autocopy.exe -lkernel32 -luser32 -lgdi32 -Wall -O2 -static
```

**Command breakdown:**
- `gcc autocopy.c` - Compile source file
- `-o autocopy.exe` - Output executable name
- `-lkernel32 -luser32 -lgdi32` - Link Windows API libraries
- `-Wall` - Enable all compiler warnings
- `-O2` - Optimization level 2 (balance speed and size)
- `-static` - **Critical**: statically link all libraries for portability

**Result**: A fully standalone `autocopy.exe` (~100-200 KB) that runs on any Windows system.

#### Method 3: Dynamic Linking (Development Only)

```bash
gcc autocopy.c -o autocopy.exe -lkernel32 -luser32 -lgdi32 -Wall -O2
```

Creates smaller executable but requires MinGW runtime DLLs on target systems. **Not recommended for distribution.**

### Verifying Portability

1. Compile with `-static` flag
2. Check file size: Should be ~100-200 KB (not a few KB)
3. Test on a clean Windows system without any development tools
4. Program should run without "DLL not found" errors

### Installation

1. Place the compiled `autocopy.exe` in a folder of your choice
2. (Optional) Add the folder to your system **PATH** for command-line access
3. Run with desired options

---

## 🔍 How It Works

### Mouse Hooking

The program installs a **low-level mouse hook** (`WH_MOUSE_LL`) to monitor `WM_LBUTTONUP` events globally:

1. **Detection**: Monitors mouse button release events across all windows
2. **Validation**: Checks if active window is NOT the autocopy console itself
3. **Execution**: Waits 200ms (to allow system processing), then simulates `Ctrl+C`
4. **Clipboard Read**: Updates internal buffer if logging or TUI is enabled

### Keyboard Hooking

A second hook (`WH_KEYBOARD_LL`) manages keyboard input:

- **Ctrl+Z**: Graceful exit when console is active
- **Arrow Keys** (TUI mode): Navigate through log entries
- **Ctrl+Enter** (TUI mode): Copy selected item to clipboard

### Instance Control

- Uses named **Mutex**: `AutoCopy2_Instance_Mutex_Igor`
- Ensures only one instance runs at a time
- Use `--force` to bypass and allow multiple instances

### Process ID Validation

Before sending `Ctrl+C`, the program:
1. Gets the active window's process ID
2. Compares it with its own process ID
3. **Only sends the command** if they don't match (safety feature)

### TUI Statistics

Real-time display shows:
- **Copy Count**: Total number of successful copies
- **Total Characters**: Sum of all copied text lengths
- **Average Length**: Average characters per copy

---

## 🔐 Safety Features

- ✅ **Self-Detection**: Does not trigger when clicking in its own window
- ✅ **Single Instance**: Only one copy can run simultaneously (configurable)
- ✅ **Signal Handling**: Prevents accidental closure on `Ctrl+C`
- ✅ **Process Validation**: Checks window ownership before sending commands
- ✅ **Graceful Exit**: `Ctrl+Z` or window close
- ⚠️ **Terminal Behavior**: In CMD/PowerShell, `Ctrl+C` creates a new line (normal Windows behavior)

---

## 🐛 Troubleshooting

### "Another instance of Auto-Copy is already running!"

**Cause**: Another autocopy window is open  
**Solution**: Close the existing window or use `--force` flag

### "libgcc_s_dw2-1.dll was not found"

**Cause**: Compiled without `-static` flag  
**Solution**: Recompile with:
```bash
gcc autocopy.c -o autocopy.exe -lkernel32 -luser32 -lgdi32 -static
```

### Program doesn't copy on some applications

**Causes**: Timing issues or click detection problems  
**Solutions**:
- Try increasing `--mintime` or `--maxtime`
- Use `--showtext` to verify triggering is working
- Check if application requires specific modifiers

### TUI display corruption

**Cause**: Terminal too small  
**Solution**: Resize terminal to at least 40 columns × 10 rows

### Selection not detected reliably

**Cause**: Timing configuration doesn't match application behavior  
**Solution**: Adjust `--mintime` and `--maxtime` values based on your click speed

---

## 📝 Behavior Notes

- **Timing**: Program waits 200ms after detecting mouse button release before sending `Ctrl+C`
- **Console Output**: When using text display or logging, timestamps are automatically added
- **Buffer Overflow**: In TUI mode, oldest entries are removed when buffer reaches maximum
- **Logging**: File is opened and closed for each entry (safe but slower)

---

## 📄 License & Information

- **Author**: Igor Brzezek
- **Version**: 0.0.5
- **Date**: 30.01.2026
- **Target OS**: Windows (XP SP3+)
- **Source Code**: [GitHub - IgorBrzezek/Auto-Copy](https://github.com/IgorBrzezek/Auto-Copy)
- **Exit**: Press **Ctrl+Z** (when window is active) or close the console window

---

## 🤝 Support

For issues, questions, or feature requests, please visit the [GitHub repository](https://github.com/IgorBrzezek/Auto-Copy).


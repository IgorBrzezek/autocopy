# autocopy v0.0.4

**autocopy** is a lightweight Windows utility designed to streamline your workflow by automatically copying selected text to the clipboard. It eliminates the need for manual `Ctrl+C` by detecting when you finish highlighting text with your mouse.

---

## 🚀 Features

- **Automatic Clipboard Update**: Simulates `Ctrl+C` immediately after text selection.
- **Modifier Key Support**: Optionally restrict copying to only when `Alt` or `Ctrl` is held down.
- **Click Counting**: Configure the tool to trigger after single, double, or triple clicks.
- **TUI Mode**: A visual Terminal User Interface with Statistics and live log.
- **Safety Measures**:
  - **Self-Detection**: Does not trigger inside its own console window.
  - **Single Instance Enforcement**: Prevents running multiple copies of the program.
  - **Signal Handling**: Prevents accidental closure when `Ctrl+C` is triggered.
  - **Quick Exit**: Press **Ctrl+Z** when the console window is active to close the program.

---

## 🛠 Usage

Run the executable from your favorite terminal:

```powershell
./autocopy.exe [options]
```

### Options

| Option | Description |
| :--- | :--- |
| `-h, --help` | Show the help message |
| `--version` | Show version information |
| `--showtext` | Display copied text in the console (on by default) |
| `--1click` | Trigger on 1 click / mouse release (Default) |
| `--2click` | Trigger on double-click |
| `--3click` | Trigger on triple-click |
| `--alt` | Require **Left Alt** to be held down |
| `--ctrl` | Require **Left Control** to be held down |
| `--ctrl1` | Always allow single-click + Ctrl to copy |
| `--ctrl2` | Always allow double-click + Ctrl to copy |
| `--tui` | Enable Terminal User Interface mode |
| `--log <file>` | Log all copied text to specified file |
| `--mintime <ms>` | Minimum delay between clicks (Default: 0ms) |
| `--maxtime <ms>` | Maximum delay between clicks (Default: System double-click time) |

---

## 🏗 Compilation and Portability

To ensure **autocopy** runs on any Windows machine without requiring external dependencies (like MinGW runtime DLLs), the project should be compiled with **static linking**.

### Prerequisites
1. **MinGW-w64**: A distribution of GCC for Windows. We recommend using [WinLibs](https://winlibs.com/) or the version provided via MSYS2.
2. **Make** (Optional): Useful for automated builds.

### Why Static Linking?
By default, GCC links some libraries dynamically. This can cause errors like `The code execution cannot proceed because libgcc_s_dw2-1.dll was not found` on machines without MinGW installed. Using the `-static` flag bundles these dependencies into the `.exe` file, making it truly standalone.

### Option 1: Using Makefile (Simplest)
If you have `make` installed, simply run:
```bash
make
```
The included `Makefile` is already configured with the `-static` flag and optimization settings.

### Option 2: Manual Compilation (Detailed)
If you prefer to compile manually, use the following command in your terminal:

```bash
gcc autocopy.c -o autocopy.exe -luser32 -lgdi32 -Wall -O2 -static
```

**Breakdown of the command:**
- `gcc autocopy.c`: Compiles the source file.
- `-o autocopy.exe`: Names the output executable.
- `-luser32 -lgdi32`: Links essential Windows API libraries for mouse hooks and UI.
- `-Wall`: Enables all compiler warnings (best practice).
- `-O2`: Enables level 2 optimizations for better performance.
- `-static`: **Crucial for portability.** Tells the compiler to link all libraries statically so the `.exe` is self-contained.

### Verifying Portability
After compilation, the `autocopy.exe` file size should be slightly larger (typically around 100-200 KB). You can test it by moving it to a computer that doesn't have any development tools installed—it should run perfectly.

### Installation
1. Move the generated `autocopy.exe` to a folder of your choice.
2. (Optional) Add the folder to your system **PATH** variable.
3. Run the executable with your desired flags.

---

## 🔍 How It Works

The program uses a **Low-Level Mouse Hook** (`WH_MOUSE_LL`) to monitor `WM_LBUTTONUP` events globally across Windows. 

1. **Detection**: When you release the left mouse button, the program checks if the current active window is NOT its own console.
2. **Execution**: If valid, it waits for `200ms` and then uses the `SendInput` API to simulate the `Ctrl+C` keystroke.
3. **Prevention**: It uses a named system **Mutex** (`AutoCopy2_Instance_Mutex_Igor`) to ensure that only one instance of the application is running at any given time.

> [!IMPORTANT]
> When using terminal-based applications (CMD, PowerShell), you may notice a new line appearing after a click. This is a standard Windows behavior when the active console receives a `^C` interrupt signal.

---

## 📄 License & Safety

- **Author**: Igor Brzezek
- **Target OS**: Windows (Win32 API)
- **Exit**: Press **Ctrl+Z** (when window is active) or close the console window.

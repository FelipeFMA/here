# Here

A Windows utility that makes new windows spawn on the monitor where your cursor is located.

## Features

- **Automatic window positioning**: New windows appear on the monitor where your cursor is
- **Smart filtering**: Ignores system windows, tooltips, and other non-application windows
- **Lightweight**: Minimal CPU and memory usage
- **No dependencies**: Standalone executable

## Building

### Prerequisites

- Windows 10/11
- A C++ compiler (MSVC, MinGW, or Clang)

### Build with g++ (MinGW)

```powershell
g++ -o here.exe main.cpp -luser32 -static
```

### Build with MSVC (Visual Studio Developer Command Prompt)

```cmd
cl /EHsc /O2 main.cpp user32.lib /Fe:here.exe
```

## Usage

1. Run `here.exe`
2. A console window will appear confirming the utility is running
3. Open any new application - it will appear on the monitor where your cursor is
4. To stop, press `Ctrl+C` or close the console window

## How It Works

The utility uses Windows Event Hooks (`SetWinEventHook`) to detect when new windows are created. When a new top-level application window appears:

1. Gets the current cursor position
2. Determines which monitor the cursor is on
3. Moves the new window to that monitor, preserving its relative position

## Filtered Windows

The utility automatically ignores:

- Child windows and dialogs (they follow their parent)
- Tool windows (tooltips, floating toolbars)
- System windows (taskbar, desktop, etc.)
- Windows without a title bar
- Very small windows (smaller than 50x50 pixels)

## Running at Startup

To run automatically when Windows starts:

1. Press `Win + R`, type `shell:startup`, press Enter
2. Create a shortcut to `here.exe` in this folder

## License

MIT License - Feel free to use and modify as needed.

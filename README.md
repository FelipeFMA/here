# Here

A Windows utility that makes new windows spawn on the monitor where your cursor is located.

## Features

- **Automatic window positioning**: New windows appear on the monitor where your cursor is.
- **System Tray Integration**: Runs quietly in the background; access settings via the tray icon.
- **GUI Logging**: Built-in log window to monitor window movements.
- **HiDPI Aware**: Properly scales the interface on high-density displays.
- **Smart filtering**: Ignores system windows, tooltips, and other non-application windows.
- **Lightweight**: Minimal CPU and memory usage.

## Building

### Prerequisites

- Windows 10/11
- MinGW-w64 (g++) installed and in your PATH.

### Build with compile.bat

The easiest way to build is to use the provided batch file:

1. Open a terminal in the project directory.
2. Run `.\compile.bat`.

This will compile the project with the necessary linker flags:
`-lgdi32 -ldwmapi -lshcore -lcomctl32 -luxtheme`

### Manual Build (MinGW)

```powershell
g++ -o here.exe .\main.cpp -lgdi32 -ldwmapi -lshcore -lcomctl32 -luxtheme
```

## Usage

1. Run `here.exe`.
2. The application starts **minimized to the system tray** (near the clock).
3. **Left-click** the tray icon to show/hide the Log window.
4. **Right-click** the tray icon for the menu (Show/Hide, Exit).
5. Open any new application - it will appear on the monitor where your cursor is.

## How It Works

The utility uses Windows Event Hooks (`SetWinEventHook`) to detect when new windows are created or brought to the foreground. When a valid top-level application window appears:

1. Gets the current cursor position.
2. Determines which monitor the cursor is on.
3. If the window is on a different monitor, it moves it to the cursor's monitor while maintaining its relative position within the work area.

## License

MIT License - Feel free to use and modify as needed.


#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <set>
#include <string>

// Global set to track windows we've already processed
std::set<HWND> processedWindows;

// Check if a window is a valid top-level window that should be repositioned
bool IsValidWindow(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    // Get window style
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    // Skip child windows
    if (style & WS_CHILD) {
        return false;
    }

    // Skip tool windows (tooltips, floating toolbars, etc.)
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }

    // Skip windows without a title bar (likely system windows)
    if (!(style & WS_CAPTION)) {
        return false;
    }

    // Get window class name to filter out system windows
    char className[256];
    if (GetClassNameA(hwnd, className, sizeof(className))) {
        std::string classStr(className);
        
        // Skip common system window classes
        if (classStr == "tooltips_class32" ||
            classStr == "Shell_TrayWnd" ||
            classStr == "Progman" ||
            classStr == "WorkerW" ||
            classStr == "NotifyIconOverflowWindow" ||
            classStr == "Windows.UI.Core.CoreWindow" ||
            classStr.find("Xaml") != std::string::npos) {
            return false;
        }
    }

    // Check if window has a reasonable size (not minimized or tiny)
    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        if (width < 50 || height < 50) {
            return false;
        }
    }

    return true;
}

// Move a window to the monitor where the cursor is located
void MoveWindowToCursorMonitor(HWND hwnd) {
    // Get cursor position
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) {
        return;
    }

    // Get the monitor at cursor position
    HMONITOR cursorMonitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
    
    // Get current window position
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        return;
    }

    // Get the monitor where the window currently is
    HMONITOR windowMonitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);

    // If window is already on the cursor's monitor, don't move it
    if (cursorMonitor == windowMonitor) {
        return;
    }

    // Get monitor info for the cursor's monitor
    MONITORINFO cursorMonitorInfo = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(cursorMonitor, &cursorMonitorInfo)) {
        return;
    }

    // Get monitor info for the window's current monitor
    MONITORINFO windowMonitorInfo = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(windowMonitor, &windowMonitorInfo)) {
        return;
    }

    // Calculate window dimensions
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Calculate the relative position of the window on its current monitor
    RECT& srcWork = windowMonitorInfo.rcWork;
    RECT& dstWork = cursorMonitorInfo.rcWork;

    int srcWorkWidth = srcWork.right - srcWork.left;
    int srcWorkHeight = srcWork.bottom - srcWork.top;
    int dstWorkWidth = dstWork.right - dstWork.left;
    int dstWorkHeight = dstWork.bottom - dstWork.top;

    // Calculate relative position (0.0 to 1.0) on source monitor
    float relX = (float)(windowRect.left - srcWork.left) / (float)srcWorkWidth;
    float relY = (float)(windowRect.top - srcWork.top) / (float)srcWorkHeight;

    // Apply same relative position to destination monitor
    int newX = dstWork.left + (int)(relX * dstWorkWidth);
    int newY = dstWork.top + (int)(relY * dstWorkHeight);

    // Ensure window fits within the destination monitor
    if (newX + windowWidth > dstWork.right) {
        newX = dstWork.right - windowWidth;
    }
    if (newY + windowHeight > dstWork.bottom) {
        newY = dstWork.bottom - windowHeight;
    }
    if (newX < dstWork.left) {
        newX = dstWork.left;
    }
    if (newY < dstWork.top) {
        newY = dstWork.top;
    }

    // Move the window
    SetWindowPos(hwnd, NULL, newX, newY, 0, 0, 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Get window title for logging
    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    std::cout << "Moved window \"" << title << "\" to cursor monitor" << std::endl;
}

// WinEvent callback function
void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime)
{
    // Only process window-level events
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) {
        return;
    }

    if (!hwnd) {
        return;
    }

    // Check if we've already processed this window
    if (processedWindows.find(hwnd) != processedWindows.end()) {
        return;
    }

    // Validate the window
    if (!IsValidWindow(hwnd)) {
        return;
    }

    // Mark as processed and move the window
    processedWindows.insert(hwnd);
    MoveWindowToCursorMonitor(hwnd);
}

// Cleanup destroyed windows from our tracking set
void CALLBACK CleanupProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime)
{
    if (hwnd && event == EVENT_OBJECT_DESTROY) {
        processedWindows.erase(hwnd);
    }
}

// Tray icon globals
#define WM_TRAYICON (WM_USER + 1)
#define IDM_EXIT 1001
#define IDM_HIDE 1002
#define IDM_SHOW 1003

NOTIFYICONDATA nid = {};
HWND hHiddenWnd = NULL;
bool isConsoleVisible = true;

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow(hwnd); // Fixes menu not disappearing behavior
            HMENU hMenu = CreatePopupMenu();
            if (isConsoleVisible) {
                AppendMenu(hMenu, MF_STRING, IDM_HIDE, "Hide Console");
            } else {
                AppendMenu(hMenu, MF_STRING, IDM_SHOW, "Show Console");
            }
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, IDM_EXIT, "Exit");
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDM_HIDE:
            ShowWindow(GetConsoleWindow(), SW_HIDE);
            isConsoleVisible = false;
            break;
        case IDM_SHOW:
            ShowWindow(GetConsoleWindow(), SW_SHOW);
            isConsoleVisible = true;
            break;
        }
        break;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Window Position Utility v1.1" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "New windows will spawn on the monitor" << std::endl;
    std::cout << "where your cursor is located." << std::endl;
    std::cout << std::endl;
    std::cout << "Use the generic tray icon to Hide/Show this console." << std::endl;
    std::cout << "Right-click the tray icon and select Exit to quit." << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // Register hidden window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "WindowPosTrayClass";
    RegisterClassEx(&wc);

    // Create hidden window to handle tray messages
    hHiddenWnd = CreateWindowEx(0, "WindowPosTrayClass", "Window Pos Tray", 
                                0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hHiddenWnd) {
        std::cerr << "Failed to create tray window" << std::endl;
        return 1;
    }

    // Initialize Tray Icon
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hHiddenWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Use default application icon
    strcpy(nid.szTip, "Window Position Utility");
    Shell_NotifyIcon(NIM_ADD, &nid);

    // Set up hook for window show events
    HWINEVENTHOOK showHook = SetWinEventHook(
        EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,
        NULL,
        WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (!showHook) {
        std::cerr << "Error: Failed to set up window show hook. Error code: " 
                  << GetLastError() << std::endl;
        return 1;
    }

    // Set up hook for foreground window changes (catches some windows that don't trigger SHOW)
    HWINEVENTHOOK foregroundHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        NULL,
        WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (!foregroundHook) {
        std::cerr << "Warning: Failed to set up foreground hook." << std::endl;
    }

    // Set up hook for window destruction (cleanup)
    HWINEVENTHOOK destroyHook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
        NULL,
        CleanupProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    std::cout << "Hooks installed successfully. Monitoring for new windows..." << std::endl;

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup hooks
    if (showHook) UnhookWinEvent(showHook);
    if (foregroundHook) UnhookWinEvent(foregroundHook);
    if (destroyHook) UnhookWinEvent(destroyHook);

    std::cout << "Window Position Utility stopped." << std::endl;
    return 0;
}

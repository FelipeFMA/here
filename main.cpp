#define _CRT_SECURE_NO_WARNINGS
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shcore.lib")

#include <windows.h>
#include <shellscalingapi.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <set>
#include <string>
#include <vector>
#include <sstream>

// Dark theme colors
#define CLR_BG       RGB(30, 30, 30)
#define CLR_BG_LOG   RGB(20, 20, 20)
#define CLR_TEXT     RGB(220, 220, 220)
#define CLR_ACCENT   RGB(100, 149, 237)  // Cornflower blue
#define CLR_DIM      RGB(140, 140, 140)

// Global set to track windows we've already processed
std::set<HWND> processedWindows;

// Log storage
std::vector<std::wstring> logMessages;
HWND hLogListBox = NULL;

// Forward declarations
void AddLog(const std::wstring& msg);
void AddLog(const std::string& msg);

// Check if a window is a valid top-level window that should be repositioned
bool IsValidWindow(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    if (style & WS_CHILD) return false;
    if (exStyle & WS_EX_TOOLWINDOW) return false;
    if (!(style & WS_CAPTION)) return false;

    char className[256];
    if (GetClassNameA(hwnd, className, sizeof(className))) {
        std::string classStr(className);
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

    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        if (width < 50 || height < 50) return false;
    }

    return true;
}

// Move a window to the monitor where the cursor is located
void MoveWindowToCursorMonitor(HWND hwnd) {
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;

    HMONITOR cursorMonitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
    
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) return;

    HMONITOR windowMonitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);

    if (cursorMonitor == windowMonitor) return;

    MONITORINFO cursorMonitorInfo = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(cursorMonitor, &cursorMonitorInfo)) return;

    MONITORINFO windowMonitorInfo = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(windowMonitor, &windowMonitorInfo)) return;

    // Handle maximized windows specifically
    if (IsZoomed(hwnd)) {
        // Restore to allow moving, then move to center of new monitor
        ShowWindow(hwnd, SW_RESTORE);
        
        RECT& dstWork = cursorMonitorInfo.rcWork;
        int dstW = dstWork.right - dstWork.left;
        int dstH = dstWork.bottom - dstWork.top;
        
        // Target a reasonable size in the center of the destination monitor
        int targetW = dstW > 1200 ? 1200 : dstW - 100;
        int targetH = dstH > 900 ? 900 : dstH - 100;
        int targetX = dstWork.left + (dstW - targetW) / 2;
        int targetY = dstWork.top + (dstH - targetH) / 2;

        SetWindowPos(hwnd, NULL, targetX, targetY, targetW, targetH, SWP_NOZORDER | SWP_NOACTIVATE);
        
        // Re-maximize on the new monitor
        ShowWindow(hwnd, SW_MAXIMIZE);

        char title[256];
        GetWindowTextA(hwnd, title, sizeof(title));
        std::string msg = "Moved (Maximized): \"";
        msg += title;
        msg += "\"";
        AddLog(msg);
        return;
    }

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    RECT& srcWork = windowMonitorInfo.rcWork;
    RECT& dstWork = cursorMonitorInfo.rcWork;

    int srcWorkWidth = srcWork.right - srcWork.left;
    int srcWorkHeight = srcWork.bottom - srcWork.top;
    int dstWorkWidth = dstWork.right - dstWork.left;
    int dstWorkHeight = dstWork.bottom - dstWork.top;

    float relX = (float)(windowRect.left - srcWork.left) / (float)srcWorkWidth;
    float relY = (float)(windowRect.top - srcWork.top) / (float)srcWorkHeight;

    int newX = dstWork.left + (int)(relX * dstWorkWidth);
    int newY = dstWork.top + (int)(relY * dstWorkHeight);

    if (newX + windowWidth > dstWork.right) newX = dstWork.right - windowWidth;
    if (newY + windowHeight > dstWork.bottom) newY = dstWork.bottom - windowHeight;
    if (newX < dstWork.left) newX = dstWork.left;
    if (newY < dstWork.top) newY = dstWork.top;

    SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    std::string msg = "Moved: \"";
    msg += title;
    msg += "\"";
    AddLog(msg);
}

// WinEvent callback function
void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!hwnd) return;
    if (processedWindows.find(hwnd) != processedWindows.end()) return;
    if (!IsValidWindow(hwnd)) return;

    processedWindows.insert(hwnd);
    MoveWindowToCursorMonitor(hwnd);
}

// Cleanup destroyed windows from our tracking set
void CALLBACK CleanupProc(
    HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (hwnd && event == EVENT_OBJECT_DESTROY) {
        processedWindows.erase(hwnd);
    }
}

// Tray icon definitions
#define WM_TRAYICON (WM_USER + 1)
#define IDM_EXIT 1001
#define IDM_TOGGLE 1002

NOTIFYICONDATAW nid = {};
HWND hMainWnd = NULL;
HWND hTrayWnd = NULL;
bool isWindowVisible = false;
HBRUSH hBgBrush = NULL;
HBRUSH hLogBgBrush = NULL;
HFONT hFont = NULL;
HFONT hTitleFont = NULL;
UINT g_dpi = 96;  // Current system DPI (96 = 100% scaling)

// Helper to scale a value by DPI
inline int Scale(int value) {
    return MulDiv(value, g_dpi, 96);
}

void AddLog(const std::wstring& msg) {
    logMessages.push_back(msg);
    if (hLogListBox) {
        SendMessageW(hLogListBox, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
        SendMessageW(hLogListBox, LB_SETTOPINDEX, logMessages.size() - 1, 0);
    }
}

void AddLog(const std::string& msg) {
    int size = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, &wstr[0], size);
    wstr.resize(size - 1);
    AddLog(wstr);
}

// Custom draw for listbox
LRESULT HandleListBoxDraw(DRAWITEMSTRUCT* dis) {
    if (dis->itemID == (UINT)-1) return TRUE;

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    // Background
    HBRUSH bgBrush = CreateSolidBrush(CLR_BG_LOG);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Get text
    wchar_t text[512];
    SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);

    // Draw text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT);
    rc.left += Scale(12);
    rc.top += Scale(4);
    SelectObject(hdc, hFont);
    DrawTextW(hdc, text, -1, &rc, DT_LEFT | DT_SINGLELINE);

    return TRUE;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Enable dark title bar (Windows 10 1809+)
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));  // DWMWA_USE_IMMERSIVE_DARK_MODE

        // Create title label
        HWND hTitle = CreateWindowW(L"STATIC", L"Here",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(20), Scale(16), Scale(360), Scale(28), hwnd, NULL, GetModuleHandle(NULL), NULL);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

        // Create subtitle
        HWND hSubtitle = CreateWindowW(L"STATIC", L"New windows spawn on cursor's monitor",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(20), Scale(44), Scale(360), Scale(20), hwnd, NULL, GetModuleHandle(NULL), NULL);
        SendMessage(hSubtitle, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Create log listbox
        hLogListBox = CreateWindowExW(0, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            Scale(20), Scale(80), Scale(360), Scale(260), hwnd, (HMENU)101, GetModuleHandle(NULL), NULL);
        SendMessage(hLogListBox, LB_SETITEMHEIGHT, 0, Scale(28));

        // Add initial messages
        AddLog(L"Monitoring started");
        AddLog(L"Hooks installed successfully");
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, CLR_BG);
        SetTextColor(hdc, CLR_TEXT);
        return (LRESULT)hBgBrush;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, CLR_BG_LOG);
        SetTextColor(hdc, CLR_TEXT);
        return (LRESULT)hLogBgBrush;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlID == 101) {
            return HandleListBoxDraw(dis);
        }
        break;
    }

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        mis->itemHeight = 28;
        return TRUE;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, hBgBrush);
        return 1;
    }

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        isWindowVisible = false;
        return 0;

    case WM_DESTROY:
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP) {
            // Left click toggles window
            if (isWindowVisible) {
                ShowWindow(hMainWnd, SW_HIDE);
                isWindowVisible = false;
            } else {
                ShowWindow(hMainWnd, SW_SHOW);
                SetForegroundWindow(hMainWnd);
                isWindowVisible = true;
            }
        } else if (lParam == WM_RBUTTONUP) {
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow(hwnd);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE, isWindowVisible ? L"Hide" : L"Show");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_EXIT:
            Shell_NotifyIconW(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        case IDM_TOGGLE:
            if (isWindowVisible) {
                ShowWindow(hMainWnd, SW_HIDE);
                isWindowVisible = false;
            } else {
                ShowWindow(hMainWnd, SW_SHOW);
                SetForegroundWindow(hMainWnd);
                isWindowVisible = true;
            }
            break;
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    // Enable Per-Monitor DPI Awareness for proper HiDPI scaling
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    // Get system DPI
    HDC hdc = GetDC(NULL);
    g_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);

    // Hide any attached console
    if (GetConsoleWindow()) {
        FreeConsole();
    }

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    // Create brushes
    hBgBrush = CreateSolidBrush(CLR_BG);
    hLogBgBrush = CreateSolidBrush(CLR_BG_LOG);

    // Create DPI-scaled fonts
    hFont = CreateFontW(Scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    hTitleFont = CreateFontW(Scale(22), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Register main window class
    WNDCLASSEXW wcMain = { sizeof(WNDCLASSEXW) };
    wcMain.lpfnWndProc = MainWndProc;
    wcMain.hInstance = GetModuleHandle(NULL);
    wcMain.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcMain.lpszClassName = L"HereMainClass";
    wcMain.hbrBackground = hBgBrush;
    RegisterClassExW(&wcMain);

    // Register tray window class
    WNDCLASSEXW wcTray = { sizeof(WNDCLASSEXW) };
    wcTray.lpfnWndProc = TrayWndProc;
    wcTray.hInstance = GetModuleHandle(NULL);
    wcTray.lpszClassName = L"HereTrayClass";
    RegisterClassExW(&wcTray);

    // Create main window (hidden initially)
    hMainWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"HereMainClass", L"Here",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, Scale(420), Scale(380),
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hMainWnd) return 1;

    // Create hidden tray message window
    hTrayWnd = CreateWindowExW(0, L"HereTrayClass", L"", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!hTrayWnd) return 1;

    // Initialize tray icon
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hTrayWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(nid.szTip, L"Here - Window Position Utility");
    Shell_NotifyIconW(NIM_ADD, &nid);

    // Set up hooks
    HWINEVENTHOOK showHook = SetWinEventHook(
        EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, NULL, WinEventProc,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    HWINEVENTHOOK foregroundHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    HWINEVENTHOOK destroyHook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, NULL, CleanupProc,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    if (!showHook) {
        MessageBoxW(NULL, L"Failed to set up window hook", L"Error", MB_ICONERROR);
        return 1;
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    if (showHook) UnhookWinEvent(showHook);
    if (foregroundHook) UnhookWinEvent(foregroundHook);
    if (destroyHook) UnhookWinEvent(destroyHook);
    DeleteObject(hBgBrush);
    DeleteObject(hLogBgBrush);
    DeleteObject(hFont);
    DeleteObject(hTitleFont);

    return 0;
}

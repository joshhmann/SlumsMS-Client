#include "pch.h"
#include "settings_window.h"
#include "settings_values.h"
#include "constants.h"

namespace {
constexpr UINT command = 0x1F80;
constexpr int firstCheck = 1000, saveButton = 2000, cancelButton = 2001, statusLabel = 2002;
constexpr const char* panelClass = "SlumsMSSettingsPanel";
constexpr const char* iniPath = ".\\" CONSTANTS_CONFIG_NAME;
constexpr const char* previousProc = "SlumsMS.Settings.PreviousProc";
#ifdef MAPLEROOT_WEATHER
constexpr std::size_t count = 9;
#else
constexpr std::size_t count = 7;
#endif
HWND panel = nullptr;
slums_settings::Values values{};

std::optional<std::string> readValue(const char* section, const char* key) {
    char value[128]{};
    GetPrivateProfileStringA(section, key, "", value, sizeof(value), iniPath);
    return value[0] ? std::optional<std::string>(value) : std::nullopt;
}
bool enabled() { return slums_settings::boolean(readValue("clienthax", "settingswindow"), true); }
LRESULT CALLBACK controlProc(HWND wnd, UINT message, WPARAM key, LPARAM detail) {
    const auto original = reinterpret_cast<WNDPROC>(GetPropA(wnd, previousProc));
    if (message == WM_KEYDOWN) {
        if (key == VK_TAB) {
            HWND next = GetNextDlgTabItem(GetParent(wnd), wnd, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            if (next) SetFocus(next);
            return 0;
        }
        if (key == VK_ESCAPE) { PostMessageA(GetParent(wnd), WM_CLOSE, 0, 0); return 0; }
        if (key == VK_RETURN) { SendMessageA(GetParent(wnd), WM_COMMAND, saveButton, 0); return 0; }
    }
    if (message == WM_CHAR && (key == VK_TAB || key == VK_ESCAPE || key == VK_RETURN)) return 0;
    if (message == WM_NCDESTROY) RemovePropA(wnd, previousProc);
    return original ? CallWindowProcA(original, wnd, message, key, detail) : DefWindowProcA(wnd, message, key, detail);
}
HWND control(HWND parent, const char* type, const char* text, DWORD style, int x, int y, int width, int height, int id) {
    HWND wnd = CreateWindowExA(0, type, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleA(nullptr), nullptr);
    if (wnd) {
        SendMessageA(wnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        if (style & WS_TABSTOP) {
            auto original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(controlProc)));
            SetPropA(wnd, previousProc, reinterpret_cast<HANDLE>(original));
        }
    }
    return wnd;
}
LRESULT CALLBACK panelProc(HWND wnd, UINT message, WPARAM key, LPARAM detail) {
    if (message == WM_CREATE) {
        values = slums_settings::load(readValue);
        control(wnd, "STATIC", "Changes apply after restarting the game.\r\nResolution and sound are in the game's Options.",
                0, 18, 14, 400, 38, 0);
        int y = 58;
        for (std::size_t i = 0; i < count; ++i, y += 25) {
            HWND check = control(wnd, "BUTTON", slums_settings::preferences[i].label,
                    BS_AUTOCHECKBOX | WS_TABSTOP, 18, y, 390, 23, firstCheck + static_cast<int>(i));
            SendMessageA(check, BM_SETCHECK, values[i] ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        control(wnd, "STATIC", "", 0, 18, y + 5, 400, 32, statusLabel);
        control(wnd, "BUTTON", "Save", BS_DEFPUSHBUTTON | WS_TABSTOP, 230, y + 40, 80, 27, saveButton);
        control(wnd, "BUTTON", "Cancel", BS_PUSHBUTTON | WS_TABSTOP, 320, y + 40, 80, 27, cancelButton);
        return 0;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(key)) {
        case saveButton: {
            for (std::size_t i = 0; i < count; ++i)
                values[i] = SendDlgItemMessageA(wnd, firstCheck + static_cast<int>(i), BM_GETCHECK, 0, 0) == BST_CHECKED;
            bool saved = slums_settings::save(values, readValue,
                    [](const char* section, const char* name, const char* value) {
                        return WritePrivateProfileStringA(section, name, value, iniPath) != FALSE;
                    }, count);
            SetDlgItemTextA(wnd, statusLabel, saved ? "Saved. Restart the game to apply your settings."
                    : "Settings could not be saved. Check config.ini permissions, then try again.");
            if (saved) SetDlgItemTextA(wnd, cancelButton, "Close");
            return 0;
        }
        case cancelButton: DestroyWindow(wnd); return 0;
        }
    }
    if (message == WM_CLOSE) { DestroyWindow(wnd); return 0; }
    if (message == WM_DESTROY) { panel = nullptr; return 0; }
    return DefWindowProcA(wnd, message, key, detail);
}
void show(HWND owner) {
    if (!enabled()) return;
    if (panel && IsWindow(panel)) { ShowWindow(panel, SW_SHOW); SetForegroundWindow(panel); return; }
    WNDCLASSA cls{};
    cls.lpfnWndProc = panelProc;
    cls.hInstance = GetModuleHandleA(nullptr);
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    cls.lpszClassName = panelClass;
    if (!RegisterClassA(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    RECT ownerRect{}; GetWindowRect(owner, &ownerRect);
    const int height = 58 + static_cast<int>(count) * 25 + 110;
    MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
    GetMonitorInfoA(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
    int x = ownerRect.left + 30, y = ownerRect.top + 40;
    if (x + 440 > monitor.rcWork.right) x = monitor.rcWork.right - 440;
    if (y + height > monitor.rcWork.bottom) y = monitor.rcWork.bottom - height;
    if (x < monitor.rcWork.left) x = monitor.rcWork.left;
    if (y < monitor.rcWork.top) y = monitor.rcWork.top;
    panel = CreateWindowExA(WS_EX_CONTROLPARENT | WS_EX_DLGMODALFRAME, panelClass, "SlumsMS settings",
            WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, 440, height, owner, nullptr, cls.hInstance, nullptr);
    if (panel) { ShowWindow(panel, SW_SHOW); SetForegroundWindow(panel); SetFocus(GetDlgItem(panel, firstCheck)); }
}
}
void AddSlumsSettingsMenu(HWND owner) {
    if (!enabled()) return;
    HMENU menu = GetSystemMenu(owner, FALSE);
    if (menu) { AppendMenuA(menu, MF_SEPARATOR, 0, nullptr); AppendMenuA(menu, MF_STRING, command, "SlumsMS settings...\tCtrl+F10"); }
}
bool HandleSlumsSettingsMessage(HWND owner, UINT message, WPARAM key, LPARAM detail) {
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && key == VK_F10
            && (GetKeyState(VK_CONTROL) & 0x8000) && !(detail & (1L << 30)) && enabled()) { show(owner); return true; }
    if (message == WM_SYSCOMMAND && (key & 0xFFF0) == command && enabled()) { show(owner); return true; }
    if (message == WM_DESTROY && panel && IsWindow(panel)) DestroyWindow(panel);
    return false;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../src/settings_window.h"
#include <cassert>
#include <string>

int main() {
    char cwd[MAX_PATH]{};
    assert(GetCurrentDirectoryA(MAX_PATH, cwd));
    std::string temporary = std::string(cwd) + "\\settings-ui-" + std::to_string(GetCurrentProcessId())
            + "-" + std::to_string(GetTickCount64());
    assert(CreateDirectoryA(temporary.c_str(), nullptr));
    assert(SetCurrentDirectoryA(temporary.c_str()));
    assert(WritePrivateProfileStringA("config", "host", "example.invalid", ".\\config.ini"));
    assert(WritePrivateProfileStringA("clienthax", "attackwalk", "false", ".\\config.ini"));
    assert(WritePrivateProfileStringA("clienthax", "moveattack", "false", ".\\config.ini"));
    WNDCLASSA cls{};
    cls.lpfnWndProc = DefWindowProcA; cls.hInstance = GetModuleHandleA(nullptr); cls.lpszClassName = "SlumsSettingsTestOwner";
    assert(RegisterClassA(&cls));
    HWND owner = CreateWindowA(cls.lpszClassName, "test owner", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600,
            nullptr, nullptr, cls.hInstance, nullptr);
    assert(owner);
    AddSlumsSettingsMenu(owner);
    assert(HandleSlumsSettingsMessage(owner, WM_SYSCOMMAND, 0x1F80, 0));
    HWND panel = FindWindowA("SlumsMSSettingsPanel", nullptr);
    assert(panel && GetWindow(panel, GW_OWNER) == owner); // only ever send commands to this test's owned window
    assert(SendDlgItemMessageA(panel, 1000, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    assert(SendDlgItemMessageA(panel, 1001, BM_GETCHECK, 0, 0) == BST_UNCHECKED); // legacy fallback
    SendDlgItemMessageA(panel, 1000, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageA(panel, WM_COMMAND, 2001, 0); // Cancel
    char value[128]{};
    GetPrivateProfileStringA("clienthax", "attackwalk", "", value, sizeof(value), ".\\config.ini");
    assert(std::string(value) == "false");
    assert(!IsWindow(panel));
    assert(HandleSlumsSettingsMessage(owner, WM_SYSCOMMAND, 0x1F80, 0));
    panel = FindWindowA("SlumsMSSettingsPanel", nullptr);
    assert(panel && GetWindow(panel, GW_OWNER) == owner);
    SendDlgItemMessageA(panel, 1000, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageA(panel, WM_COMMAND, 2000, 0); // Save through actual Windows controls and INI API
    GetPrivateProfileStringA("clienthax", "attackwalk", "", value, sizeof(value), ".\\config.ini");
    assert(std::string(value) == "true");
    GetPrivateProfileStringA("config", "host", "", value, sizeof(value), ".\\config.ini");
    assert(std::string(value) == "example.invalid");
    char status[160]{}; GetDlgItemTextA(panel, 2002, status, sizeof(status));
    assert(std::string(status).find("Saved. Restart") == 0);
    HandleSlumsSettingsMessage(owner, WM_DESTROY, 0, 0);
    assert(!IsWindow(panel));
    assert(WritePrivateProfileStringA("clienthax", "settingswindow", "false", ".\\config.ini"));
    assert(!HandleSlumsSettingsMessage(owner, WM_SYSCOMMAND, 0x1F80, 0));
    DestroyWindow(owner);
    assert(SetCurrentDirectoryA(cwd));
    return 0;
}

#pragma once
#include <windows.h>
void AddSlumsSettingsMenu(HWND owner);
bool HandleSlumsSettingsMessage(HWND owner, UINT message, WPARAM key, LPARAM detail);

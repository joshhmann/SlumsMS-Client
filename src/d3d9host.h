#pragma once
#include <windows.h>

// d3d9host.cpp: D3D9 device diagnostics and adapter fallback (Parsec), and the ImGui overlay host.
void AttachD3D9Host();

// From the game window procedure (system.cpp): returns true when the overlay consumed the message.
bool D3D9Host_HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// settings_overlay.cpp: the in-game settings window, drawn by the overlay while it is open.
void SettingsOverlay_Draw(bool* pOpen);

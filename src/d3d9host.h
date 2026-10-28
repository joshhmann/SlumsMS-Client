#pragma once
#include <windows.h>

// d3d9host.cpp: D3D9 device diagnostics and adapter fallback (Parsec), and the ImGui overlay host.
void AttachD3D9Host();

// From the game window procedure (system.cpp): returns true when the overlay consumed the message.
bool D3D9Host_HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// settings_overlay.cpp: the in-game HUD window, drawn by the overlay while it is open.
void SettingsOverlay_Draw(bool* pOpen);

// overlay.cpp: the HUD framework (theme, font scale, widget layout, freeze).
void Overlay_Setup();            // after ImGui's context exists, before the first frame
bool Overlay_HasHud();           // any widget switched on
void Overlay_DrawHud();
void Overlay_DrawSettings();     // the HUD section of the settings window
bool Overlay_IsFrozen();
bool Overlay_BeforeFrame();    // rebakes the font when its drawn size changed; true when it did
bool Overlay_SnapOn();         // Edit UI snaps to OverlayPolicy::kGridFrac
void Overlay_ToggleFreeze();

#ifdef MAPLEROOT_DAMAGE_FORMAT
void DamageFmt_DrawSettings();   // damagefmt.cpp: the "Short damage numbers" switch (ticket 119)
#endif

// hud_vitals.cpp: the HP, MP and EXP modules (one bar each).
void HudVitals_DrawHP();
void HudVitals_DrawMP();
void HudVitals_DrawEXP();

// uilayout.cpp: grips over the stock status bar and quick slot while the HUD is unfrozen, and their reset.
void UiLayout_DrawGrips();
void UiLayout_Reset();
#ifdef MAPLEROOT_INV_RESIZE
void InvResize_ResetSize();   // invresize.cpp: back to the stock 4x6 for this character
#endif

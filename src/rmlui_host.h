#pragma once
// ============================================================
// rmlui_host.h -- the RmlUi HUD host on the D3D9 overlay (1.7.0, HUD skin step 1).
//
// RmlUi draws the skinned HUD modules; ImGui stays for tools (HUD window, Edit UI grips) and draws on top. The host
// owns RmlUi's lifetime, one context ("hud") sized to the back buffer, Tahoma from the Windows fonts, the DX9 render
// interface (rmlui_renderer_dx9.h) and input next to ImGui's. config.ini [clienthax] hudskin picks the mode:
//   off  (default) RmlUi is never initialised; the 1.6.x ImGui modules as before.
//   skin           the host runs and the skinned panels (registered through OnReady) draw in the field.
//   test           as skin, plus a test document (top centre) that shows the host draws, takes clicks and reflows.
// Documents and art are loaded relative to the game folder, from SlumsHUD\ (kRoot), so a skin change is a data change.
//
// Everything runs on the game thread (the overlay's EndScene and the window procedure).
// ============================================================

#include <windows.h>
#include <d3d9.h>

#include <string>

namespace Rml {
class Context;
class ElementDocument;
}  // namespace Rml

namespace RmlHost {

enum class Mode { Off, Skin, Test };

constexpr const char* kRoot = "SlumsHUD/";

Mode GetMode();
// The context, or nullptr before the first frame in the field and when the host is off or failed.
Rml::Context* Context();

// Panels register here (from their Attach* function, before the first frame). OnReady runs once, right after the
// context exists: create data models (Context()->CreateDataModel) and load documents there. OnFrame runs every
// frame in the field before Context::Update: refresh model values and call DirtyVariable for the ones that changed.
using Callback = void (*)();
void OnReady(Callback fn);
void OnFrame(Callback fn);

// A document from SlumsHUD\<file> (or from memory), hidden until Show. nullptr on failure (logged).
Rml::ElementDocument* LoadDocument(const std::string& file);
Rml::ElementDocument* LoadDocumentFromMemory(const std::string& rml, const std::string& name);
void Show(Rml::ElementDocument* doc, bool visible);

// Places a document's body at x, y with size w x h in back-buffer pixels (Edit UI stores screen fractions:
// OverlayPolicy::FromFractions turns them into these). h <= 0 lets the content decide the height.
void SetRect(Rml::ElementDocument* doc, float x, float y, float w, float h);

// True while the mouse is over a visible RmlUi element (or one is being dragged): the overlay keeps such input
// from the game, both the window messages and the DirectInput clicks (D3D9Host_PassGameInput, bypass.cpp).
bool WantsMouse();

// True when back-buffer pixel (x, y) is over a visible RmlUi panel element (the root/background does not count).
// For hooks that decide whether the stock UI under a panel should take a click (e.g. CUIStatusBar::HitTest).
bool IsOverPanel(int x, int y);

}  // namespace RmlHost

// d3d9host.cpp's side.
void RmlHost_Setup();                                    // AttachD3D9Host: reads [clienthax] hudskin
bool RmlHost_Wants();                                    // the overlay must draw this frame
void RmlHost_Render(IDirect3DDevice9* dev, HWND hWnd);   // EndScene, before ImGui
// The window procedure, before ImGui. imguiOnTop: an ImGui window has the mouse or keyboard, so clicks are its.
bool RmlHost_HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool imguiOnTop);

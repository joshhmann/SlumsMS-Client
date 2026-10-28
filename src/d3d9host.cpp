// ============================================================
// d3d9host.cpp: the D3D9 host (1.6.0, tickets 39 and 90): device diagnostics, adapter fallback, ImGui overlay.
//
// The client draws through d3d8to9 (d3d8.dll -> d3d9!Direct3DCreate9), so the D3D9 objects it uses are ordinary
// IDirect3D9 / IDirect3DDevice9 instances. This detours Direct3DCreate9 and, on the first IDirect3D9, the methods
// the client's start-up uses:
//   - every failing adapter query is logged with its HRESULT, and the adapters (name, display, monitor, primary)
//     are logged once, so a failure with Parsec or another virtual display as the primary can be pinned down;
//   - when CreateDevice fails it is retried on the other adapters, then with software vertex processing
//     (d3d9host_policy.h), and every attempt is logged.
// On the created device, EndScene and Reset are detoured to host Dear ImGui (the 1.91.4 build the MapleRoot
// companion already links, with its DX9 and Win32 backends). Ctrl+F10 opens the HUD window; while it is open
// the overlay takes the mouse and keyboard messages ImGui wants. Nothing is drawn or allocated until it opens.
//
// config.ini [graphics] d3d9host=false turns all of it off; [graphics] adapterfallback=false keeps the logging
// without retries; [clienthax] settingswindow=false disables the window.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "constants.h"
#include "attackstop_patch.h"
#include "d3d9host.h"
#include "d3d9host_policy.h"
#ifdef MAPLEROOT_RMLUI
#include "rmlui_host.h"
#endif

#include <d3d9.h>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d9.lib")
// A real reference, so d3d9.dll is a static import and loaded before DllMain runs (no LoadLibrary under the loader
// lock). Its value is our import thunk; the detour goes on the export GetProcAddress returns.
extern "C" IDirect3D9* (WINAPI* const g_pForceD3D9Import)(UINT) = &Direct3DCreate9;

namespace {

bool ReadIniBool(const char* section, const char* key, bool fallback) {
    char value[32] = {};
    GetPrivateProfileStringA(section, key, "", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    return integration::ParseIniBool(value).value_or(fallback);
}

const char* Hr(HRESULT hr) { return D3D9Host::HrName(static_cast<std::uint32_t>(hr)); }

// ---- IDirect3D9 (vtable slots: 8 GetAdapterDisplayMode, 9 CheckDeviceType, 10 CheckDeviceFormat, 14 GetDeviceCaps,
// 16 CreateDevice) ------------------------------------------------------------------------------------------------
using Direct3DCreate9_t = IDirect3D9*(WINAPI*)(UINT);
using GetAdapterDisplayMode_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDISPLAYMODE*);
using CheckDeviceType_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL);
using CheckDeviceFormat_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, DWORD,
    D3DRESOURCETYPE, D3DFORMAT);
using GetDeviceCaps_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, D3DCAPS9*);
using CreateDevice_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
// ---- IDirect3DDevice9 (vtable slots: 16 Reset, 42 EndScene) ------------------------------------------------------
using Reset_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using EndScene_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);

Direct3DCreate9_t g_Direct3DCreate9 = nullptr;
GetAdapterDisplayMode_t g_GetAdapterDisplayMode = nullptr;
CheckDeviceType_t g_CheckDeviceType = nullptr;
CheckDeviceFormat_t g_CheckDeviceFormat = nullptr;
GetDeviceCaps_t g_GetDeviceCaps = nullptr;
CreateDevice_t g_CreateDevice = nullptr;
Reset_t g_Reset = nullptr;
EndScene_t g_EndScene = nullptr;

bool g_bFallback = true;
bool g_bSettingsWindow = true;
bool g_bD3DHooked = false;
bool g_bDeviceHooked = false;
IDirect3DDevice9* g_pDevice = nullptr;
HWND g_hWnd = nullptr;

bool g_bImGuiReady = false;
bool g_bVisible = false;

template <typename T>
T VtableSlot(void* pObject, int slot) {
    return reinterpret_cast<T>((*reinterpret_cast<void***>(pObject))[slot]);
}

// Log a failing query once per (call, adapter, HRESULT): the client may repeat them every start-up step.
void LogFailureOnce(const char* call, UINT adapter, HRESULT hr) {
    static std::vector<std::tuple<const char*, UINT, HRESULT>> s_seen;
    for (const auto& s : s_seen) {
        if (std::get<0>(s) == call && std::get<1>(s) == adapter && std::get<2>(s) == hr) return;
    }
    s_seen.emplace_back(call, adapter, hr);
    DebugMessage("D3D9: %s(adapter %u) failed 0x%08lX %s", call, adapter, static_cast<unsigned long>(hr), Hr(hr));
}

HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode_hook(IDirect3D9* self, UINT adapter, D3DDISPLAYMODE* mode) {
    const HRESULT hr = g_GetAdapterDisplayMode(self, adapter, mode);
    if (FAILED(hr)) LogFailureOnce("GetAdapterDisplayMode", adapter, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CheckDeviceType_hook(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, D3DFORMAT display,
    D3DFORMAT back, BOOL windowed) {
    const HRESULT hr = g_CheckDeviceType(self, adapter, type, display, back, windowed);
    if (FAILED(hr)) LogFailureOnce("CheckDeviceType", adapter, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CheckDeviceFormat_hook(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, D3DFORMAT fmt,
    DWORD usage, D3DRESOURCETYPE rtype, D3DFORMAT check) {
    const HRESULT hr = g_CheckDeviceFormat(self, adapter, type, fmt, usage, rtype, check);
    if (FAILED(hr) && hr != D3DERR_NOTAVAILABLE) LogFailureOnce("CheckDeviceFormat", adapter, hr);   // probing is normal
    return hr;
}

HRESULT STDMETHODCALLTYPE GetDeviceCaps_hook(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, D3DCAPS9* caps) {
    const HRESULT hr = g_GetDeviceCaps(self, adapter, type, caps);
    if (FAILED(hr)) LogFailureOnce("GetDeviceCaps", adapter, hr);
    return hr;
}

// One line per adapter: what Windows calls it, the display it drives, its mode, whether it is the primary and
// whether a HAL device is available on it.
void LogAdapters(IDirect3D9* d3d) {
    const UINT count = d3d->GetAdapterCount();
    DebugMessage("D3D9: %u adapter(s)", count);
    for (UINT a = 0; a < count; ++a) {
        D3DADAPTER_IDENTIFIER9 id = {};
        d3d->GetAdapterIdentifier(a, 0, &id);
        D3DDISPLAYMODE mode = {};
        const HRESULT hrMode = g_GetAdapterDisplayMode(d3d, a, &mode);
        D3DCAPS9 caps = {};
        const HRESULT hrCaps = g_GetDeviceCaps(d3d, a, D3DDEVTYPE_HAL, &caps);
        MONITORINFOEXA mi = {};
        mi.cbSize = sizeof(mi);
        const HMONITOR mon = d3d->GetAdapterMonitor(a);
        const bool bPrimary = mon && GetMonitorInfoA(mon, &mi) && (mi.dwFlags & MONITORINFOF_PRIMARY);
        DebugMessage("D3D9: adapter %u: %s on %s%s, mode %ux%u fmt %d (%s), HAL caps %s", a, id.Description,
            id.DeviceName, bPrimary ? " [primary]" : "", mode.Width, mode.Height, static_cast<int>(mode.Format),
            Hr(hrMode), Hr(hrCaps));
    }
}

// ---- the device: ImGui ------------------------------------------------------------------------------------------
void InitImGui(IDirect3DDevice9* dev) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // no imgui.ini next to the game
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    Overlay_Setup();   // theme, font, saved HUD layout
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX9_Init(dev);
    g_bImGuiReady = true;
    DebugMessage("D3D9: ImGui %s ready on window %p", IMGUI_VERSION, g_hWnd);
}

void DrawOverlay(IDirect3DDevice9* dev) {
    // Drawn while the HUD window is open, a widget is on, Edit UI is on (grips over the stock bar), or the first-run
    // tour needs it.
    const bool bImGui = g_bVisible || (g_bSettingsWindow && (Overlay_HasHud() || !Overlay_IsFrozen() || UiTour_Wants()));
#ifdef MAPLEROOT_RMLUI
    // The RmlUi HUD skin first, under ImGui, so the tools (HUD window, Edit UI grips) stay on top. It catches its
    // own failures and turns itself off.
    if (RmlHost_Wants()) RmlHost_Render(dev, g_hWnd);
#endif
    if (!bImGui) return;
    try {
        if (!g_bImGuiReady) InitImGui(dev);
        ImGuiIO& io = ImGui::GetIO();
        // Only over an overlay window: elsewhere the game's own cursor is the one on top.
        io.MouseDrawCursor = io.WantCaptureMouse;
        if (Overlay_BeforeFrame()) ImGui_ImplDX9_InvalidateDeviceObjects();   // re-upload the rebaked font
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        Overlay_DrawHud();
        if (g_bVisible) SettingsOverlay_Draw(&g_bVisible);
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    } catch (...) {
        g_bVisible = false;
        LOG_ONCE("D3D9: the overlay failed to draw and was closed");
    }
}

HRESULT STDMETHODCALLTYPE EndScene_hook(IDirect3DDevice9* dev) {
    if (dev == g_pDevice) DrawOverlay(dev);
    return g_EndScene(dev);
}

HRESULT STDMETHODCALLTYPE Reset_hook(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    const bool bOurs = dev == g_pDevice && g_bImGuiReady;
    if (bOurs) ImGui_ImplDX9_InvalidateDeviceObjects();
    const HRESULT hr = g_Reset(dev, pp);
    if (FAILED(hr)) {
        DebugMessage("D3D9: Reset(%ux%u, windowed %d) failed 0x%08lX %s", pp ? pp->BackBufferWidth : 0,
            pp ? pp->BackBufferHeight : 0, pp ? pp->Windowed : -1, static_cast<unsigned long>(hr), Hr(hr));
    } else if (bOurs) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return hr;
}

void HookDevice(IDirect3DDevice9* dev, HWND hWnd) {
    g_pDevice = dev;
    if (hWnd) g_hWnd = hWnd;
    if (g_bDeviceHooked) return;
    g_Reset = VtableSlot<Reset_t>(dev, 16);
    g_EndScene = VtableSlot<EndScene_t>(dev, 42);
    const bool okReset = ATTACH_HOOK(g_Reset, Reset_hook);
    const bool okEnd = ATTACH_HOOK(g_EndScene, EndScene_hook);
    g_bDeviceHooked = okReset && okEnd;
    DebugMessage("D3D9: overlay %s (Ctrl+F10 settings, Ctrl+F11 freezes the HUD)", g_bDeviceHooked ? "ready" : "unavailable: hook failed");
}

// ---- CreateDevice -----------------------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE CreateDevice_hook(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND focus,
    DWORD behavior, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** ppDevice) {
    // A failed call may change *pp, so retries start from the parameters the client passed.
    const D3DPRESENT_PARAMETERS asked = pp ? *pp : D3DPRESENT_PARAMETERS{};
    HRESULT hr = g_CreateDevice(self, adapter, type, focus, behavior, pp, ppDevice);
    DebugMessage("D3D9: CreateDevice(adapter %u, type %d, window %p, flags 0x%lX, %ux%u windowed %d fmt %d) = "
        "0x%08lX %s", adapter, static_cast<int>(type), focus, static_cast<unsigned long>(behavior),
        asked.BackBufferWidth, asked.BackBufferHeight, asked.Windowed, static_cast<int>(asked.BackBufferFormat),
        static_cast<unsigned long>(hr), Hr(hr));

    if (FAILED(hr) && g_bFallback && pp) {
        for (const auto& at : D3D9Host::FallbackAttempts(adapter, self->GetAdapterCount(), behavior)) {
            D3DPRESENT_PARAMETERS tryPp = asked;
            const HRESULT hr2 = g_CreateDevice(self, at.adapter, type, focus, at.behavior, &tryPp, ppDevice);
            DebugMessage("D3D9: retry CreateDevice(adapter %u, flags 0x%lX) = 0x%08lX %s", at.adapter,
                static_cast<unsigned long>(at.behavior), static_cast<unsigned long>(hr2), Hr(hr2));
            if (SUCCEEDED(hr2)) {
                *pp = tryPp;
                hr = hr2;
                break;
            }
        }
    }
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) {
        HookDevice(*ppDevice, asked.hDeviceWindow ? asked.hDeviceWindow : focus);
    }
    return hr;
}

void HookDirect3D9(IDirect3D9* d3d) {
    if (g_bD3DHooked) return;
    g_bD3DHooked = true;
    g_GetAdapterDisplayMode = VtableSlot<GetAdapterDisplayMode_t>(d3d, 8);
    g_CheckDeviceType = VtableSlot<CheckDeviceType_t>(d3d, 9);
    g_CheckDeviceFormat = VtableSlot<CheckDeviceFormat_t>(d3d, 10);
    g_GetDeviceCaps = VtableSlot<GetDeviceCaps_t>(d3d, 14);
    g_CreateDevice = VtableSlot<CreateDevice_t>(d3d, 16);
    LogAdapters(d3d);   // before the hooks, through the originals
    ATTACH_HOOK(g_GetAdapterDisplayMode, GetAdapterDisplayMode_hook);
    ATTACH_HOOK(g_CheckDeviceType, CheckDeviceType_hook);
    ATTACH_HOOK(g_CheckDeviceFormat, CheckDeviceFormat_hook);
    ATTACH_HOOK(g_GetDeviceCaps, GetDeviceCaps_hook);
    ATTACH_HOOK(g_CreateDevice, CreateDevice_hook);
}

IDirect3D9* WINAPI Direct3DCreate9_hook(UINT sdk) {
    IDirect3D9* d3d = g_Direct3DCreate9(sdk);
    DebugMessage("D3D9: Direct3DCreate9(%u) = %p", sdk, d3d);
    if (d3d) HookDirect3D9(d3d);
    return d3d;
}

}  // namespace

bool D3D9Host_HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_bDeviceHooked) return false;
    const bool bKeyDown = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;   // F10 arrives as WM_SYSKEYDOWN
    const bool bCtrlFresh = (GetKeyState(VK_CONTROL) & 0x8000) && !(lParam & (1 << 30));
    if (g_bSettingsWindow && bKeyDown && wParam == VK_F10 && bCtrlFresh) {
        g_bVisible = !g_bVisible;
        return true;
    }
    if (g_bSettingsWindow && bKeyDown && wParam == VK_F11 && bCtrlFresh) {
        Overlay_ToggleFreeze();
        return true;
    }
#ifdef MAPLEROOT_RMLUI
    // RmlUi before ImGui, unless an ImGui window (drawn above the skin) has the mouse or keyboard.
    {
        const bool bImGuiOnTop = g_bImGuiReady &&
            (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard);
        if (RmlHost_HandleMessage(hWnd, msg, wParam, lParam, bImGuiOnTop)) return true;
    }
#endif
    if (!g_bSettingsWindow || !g_bImGuiReady) return false;
    if (g_bVisible && bKeyDown && wParam == VK_ESCAPE) {
        g_bVisible = false;
        return true;
    }
    if (bKeyDown && wParam == VK_ESCAPE && UiTour_HandleEscape()) return true;
    // ImGui sees every message (hover, drags); the game loses only what an overlay window wants. A frozen HUD
    // takes no input, so then nothing is held back.
    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    const ImGuiIO& io = ImGui::GetIO();
    if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && io.WantCaptureMouse) return true;
    if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR) && io.WantCaptureKeyboard) return true;
    return false;
}

void AttachD3D9Host() {
    if (!ReadIniBool("graphics", "d3d9host", true)) {
        DebugMessage("D3D9: host off ([graphics] d3d9host=false)");
        return;
    }
    g_bFallback = ReadIniBool("graphics", "adapterfallback", true);
    g_bSettingsWindow = ReadIniBool("clienthax", "settingswindow", true);
#ifdef MAPLEROOT_RMLUI
    RmlHost_Setup();
#endif
    UiTour_Setup(g_bSettingsWindow && ReadIniBool("clienthax", "uitour", true));
    HMODULE hD3D9 = g_pForceD3D9Import ? GetModuleHandleA("d3d9.dll") : nullptr;   // loaded as our import
    g_Direct3DCreate9 = hD3D9 ? reinterpret_cast<Direct3DCreate9_t>(GetProcAddress(hD3D9, "Direct3DCreate9")) : nullptr;
    if (!g_Direct3DCreate9) {
        DebugMessage("D3D9: d3d9.dll is not loaded; host off");
        return;
    }
    if (ATTACH_HOOK(g_Direct3DCreate9, Direct3DCreate9_hook)) {
        DebugMessage("D3D9: host on (adapter fallback %s, HUD %s)", g_bFallback ? "on" : "off",
            g_bSettingsWindow ? "on" : "off");
    }
}

// ============================================================
// rmlui_host.cpp: the RmlUi HUD host (see rmlui_host.h).
//
// Lifetime: nothing happens until the first frame in the field with [clienthax] hudskin=skin or test. Then RmlUi is
// initialised once (render, system and text-input interfaces, Tahoma), the "hud" context is created at the back
// buffer's size, and the OnReady callbacks run. RmlUi is never shut down: the game process ends instead, as for
// ImGui. Any exception turns the host off for the session (logged) and restores the device state first.
// ============================================================

#include "pch.h"
#include "debug.h"
#include "constants.h"
#include "attackstop_patch.h"
#include "rmlui_host.h"
#include "rmlui_renderer_dx9.h"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_Win32.h>

namespace {

constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;   // CUserLocal*: set only while a character is in the field

// The game draws its own cursor and owns the OS cursor; RmlUi only logs through us.
class SystemInterface_Slums : public SystemInterface_Win32 {
public:
    void SetMouseCursor(const Rml::String&) override {}
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if (type <= Rml::Log::LT_WARNING) DebugMessage("RmlUi: %s", message.c_str());
        return true;
    }
};

RmlHost::Mode g_mode = RmlHost::Mode::Off;
bool g_bReady = false;
bool g_bFailed = false;
RenderInterface_DX9 g_render;
SystemInterface_Slums* g_pSystem = nullptr;        // created on first use: its constructor loads cursors
TextInputMethodEditor_Win32* g_pIme = nullptr;
Rml::Context* g_pContext = nullptr;
int g_nWidth = 0, g_nHeight = 0;
std::vector<RmlHost::Callback> g_onReady, g_onFrame;

bool InField() { return *reinterpret_cast<void* const*>(kAddr_UserLocal) != nullptr; }

// ---- the test document (hudskin=test) --------------------------------------------------------------------------
// Draws, takes clicks and reflows: a cream panel in the 1.6.0 colours with a live frame rate, the back buffer size
// and a click counter, bound through a data model.
const char* const kTestRml = R"RML(<rml>
<head>
<title>SlumsMS HUD host test</title>
<style>
body { position: absolute; top: 12px; left: 50%; margin-left: -170px; width: 340px; font-family: Tahoma;
       font-size: 14px; color: #333333; }
div, p { display: block; }
.panel { background-color: #EEEEDDF2; border: 2px #557799; border-radius: 10px; padding: 10px 12px; }
.title { font-weight: bold; font-size: 16px; color: #557799; margin-bottom: 6px; }
.row { margin: 3px 0; }
.bar { height: 14px; border-radius: 7px; background-color: #FFFFFF; border: 1px #99BBCC; margin: 6px 0; }
.fill { height: 100%; border-radius: 6px; background-color: #99BBCC; }
button { display: inline-block; padding: 4px 14px; margin-top: 6px; border-radius: 6px; background-color: #99BBCC;
         color: #FFFFFF; font-weight: bold; cursor: pointer; }
button:hover { background-color: #AACCDD; }
button:active { background-color: #557799; }
</style>
</head>
<body data-model="hosttest">
<div class="panel">
  <div class="title">SlumsMS HUD skin: host test</div>
  <p class="row">{{ fps }} fps at {{ size }}</p>
  <div class="bar"><div class="fill" data-style-width="pulse + '%'"></div></div>
  <p class="row">Clicks: {{ clicks }}</p>
  <button data-event-click="clicks = clicks + 1">Click me</button>
</div>
</body>
</rml>)RML";

int g_nFps = 0, g_nClicks = 0, g_nPulse = 0;
Rml::String g_sSize;
Rml::DataModelHandle g_testModel;
DWORD g_dwFpsTick = 0;
int g_nFrames = 0;

void TestReady() {
    Rml::DataModelConstructor c = g_pContext->CreateDataModel("hosttest");
    if (!c) return;
    c.Bind("fps", &g_nFps);
    c.Bind("size", &g_sSize);
    c.Bind("clicks", &g_nClicks);
    c.Bind("pulse", &g_nPulse);
    g_testModel = c.GetModelHandle();
    if (Rml::ElementDocument* doc = RmlHost::LoadDocumentFromMemory(kTestRml, "hosttest.rml")) RmlHost::Show(doc, true);
}

void TestFrame() {
    ++g_nFrames;
    const DWORD now = GetTickCount();
    if (now - g_dwFpsTick >= 1000) {
        g_nFps = g_nFrames;
        g_nFrames = 0;
        g_dwFpsTick = now;
        g_sSize = std::to_string(g_nWidth) + "x" + std::to_string(g_nHeight);
        g_testModel.DirtyVariable("fps");
        g_testModel.DirtyVariable("size");
    }
    g_nPulse = static_cast<int>((now / 20) % 101);
    g_testModel.DirtyVariable("pulse");
}

// ---- lifetime ---------------------------------------------------------------------------------------------------
bool Initialise(IDirect3DDevice9* dev, HWND hWnd, int w, int h) {
    g_pSystem = new SystemInterface_Slums();
    g_pSystem->SetWindow(hWnd);
    g_pIme = new TextInputMethodEditor_Win32();
    g_render.SetDevice(dev);
    Rml::SetRenderInterface(&g_render);
    Rml::SetSystemInterface(g_pSystem);
    if (!Rml::Initialise()) {
        DebugMessage("RmlUi: Initialise failed");
        return false;
    }
    char fonts[MAX_PATH];
    const UINT n = GetWindowsDirectoryA(fonts, MAX_PATH);
    if (!n || n > MAX_PATH - 32) return false;
    const std::string dir = std::string(fonts) + "\\Fonts\\";
    const bool regular = Rml::LoadFontFace(dir + "tahoma.ttf", true);
    Rml::LoadFontFace(dir + "tahomabd.ttf", false, Rml::Style::FontWeight::Bold);
    if (!regular) DebugMessage("RmlUi: tahoma.ttf could not be loaded; text will not draw");
    g_pContext = Rml::CreateContext("hud", Rml::Vector2i(w, h), nullptr, g_pIme);
    if (!g_pContext) {
        DebugMessage("RmlUi: CreateContext failed");
        return false;
    }
    g_nWidth = w;
    g_nHeight = h;
    if (g_mode == RmlHost::Mode::Test) {
        g_onReady.push_back(&TestReady);
        g_onFrame.push_back(&TestFrame);
    }
    for (RmlHost::Callback fn : g_onReady) fn();
    DebugMessage("RmlUi: %s host ready (%dx%d, %s)", Rml::GetVersion().c_str(), w, h,
        g_mode == RmlHost::Mode::Test ? "test document" : "skin");
    return true;
}

bool BackBufferSize(IDirect3DDevice9* dev, int& w, int& h) {
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &rt)) || !rt) return false;
    D3DSURFACE_DESC d = {};
    const bool ok = SUCCEEDED(rt->GetDesc(&d));
    rt->Release();
    w = static_cast<int>(d.Width);
    h = static_cast<int>(d.Height);
    return ok && w > 0 && h > 0;
}

void Fail(const char* what) {
    g_bFailed = true;
    g_render.EndFrame();   // no-op unless a frame was open
    DebugMessage("RmlUi: %s; the HUD skin is off for this session", what);
}

}  // namespace

// ---- the panel API ----------------------------------------------------------------------------------------------
namespace RmlHost {

Mode GetMode() { return g_mode; }
Rml::Context* Context() { return g_bReady && !g_bFailed ? g_pContext : nullptr; }
void OnReady(Callback fn) { if (fn) g_onReady.push_back(fn); }
void OnFrame(Callback fn) { if (fn) g_onFrame.push_back(fn); }

Rml::ElementDocument* LoadDocument(const std::string& file) {
    if (!g_pContext) return nullptr;
    Rml::ElementDocument* doc = g_pContext->LoadDocument(std::string(kRoot) + file);
    if (!doc) DebugMessage("RmlUi: could not load %s%s", kRoot, file.c_str());
    return doc;
}

Rml::ElementDocument* LoadDocumentFromMemory(const std::string& rml, const std::string& name) {
    if (!g_pContext) return nullptr;
    Rml::ElementDocument* doc = g_pContext->LoadDocumentFromMemory(rml, std::string(kRoot) + name);
    if (!doc) DebugMessage("RmlUi: could not parse %s", name.c_str());
    return doc;
}

void Show(Rml::ElementDocument* doc, bool visible) {
    if (!doc) return;
    if (visible) doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    else doc->Hide();
}

void SetRect(Rml::ElementDocument* doc, float x, float y, float w, float h) {
    if (!doc) return;
    doc->SetProperty(Rml::PropertyId::Left, Rml::Property(x, Rml::Unit::PX));
    doc->SetProperty(Rml::PropertyId::Top, Rml::Property(y, Rml::Unit::PX));
    doc->SetProperty(Rml::PropertyId::MarginLeft, Rml::Property(0.0f, Rml::Unit::PX));
    doc->SetProperty(Rml::PropertyId::Width, Rml::Property(w, Rml::Unit::PX));
    if (h > 0) doc->SetProperty(Rml::PropertyId::Height, Rml::Property(h, Rml::Unit::PX));
    else doc->RemoveProperty(Rml::PropertyId::Height);
}

bool WantsMouse() { return Context() && g_pContext->IsMouseInteracting(); }

bool IsOverPanel(int x, int y) {
    Rml::Context* c = Context();
    if (!c) return false;
    Rml::Element* e = c->GetElementAtPoint(Rml::Vector2f(static_cast<float>(x), static_cast<float>(y)));
    return e && e != c->GetRootElement();
}

}  // namespace RmlHost

// ---- d3d9host.cpp's side ----------------------------------------------------------------------------------------
void RmlHost_Setup() {
    char value[16] = {};
    GetPrivateProfileStringA("clienthax", "hudskin", "off", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    const std::string v = value;
    g_mode = v == "skin" ? RmlHost::Mode::Skin : v == "test" ? RmlHost::Mode::Test : RmlHost::Mode::Off;
    DebugMessage("RmlUi: hudskin=%s", g_mode == RmlHost::Mode::Off ? "off" : v.c_str());
}

bool RmlHost_Wants() { return g_mode != RmlHost::Mode::Off && !g_bFailed; }

void RmlHost_Render(IDirect3DDevice9* dev, HWND hWnd) {
    if (!RmlHost_Wants() || !dev || !InField()) return;
    try {
        int w = 0, h = 0;
        if (!BackBufferSize(dev, w, h)) return;
        if (!g_bReady) {
            if (!Initialise(dev, hWnd, w, h)) {
                Fail("initialisation failed");
                return;
            }
            g_bReady = true;
        }
        if (dev != g_render.Device()) {   // a new device: the old one's textures go, RmlUi regenerates them
            Rml::ReleaseTextures(&g_render);
            g_render.SetDevice(dev);
        }
        if (w != g_nWidth || h != g_nHeight) {
            g_nWidth = w;
            g_nHeight = h;
            g_pContext->SetDimensions(Rml::Vector2i(w, h));
        }
        for (RmlHost::Callback fn : g_onFrame) fn();
        g_pContext->Update();
        if (g_render.BeginFrame(w, h)) {
            g_pContext->Render();
            g_render.EndFrame();
        }
    } catch (...) {
        Fail("a frame failed");
    }
}

bool RmlHost_HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool imguiOnTop) {
    if (!g_bReady || g_bFailed || !InField()) return false;
    const bool mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
    if (imguiOnTop && msg != WM_MOUSEMOVE) return false;   // an ImGui tool window is above ours: its input
    try {
        const bool propagate = RmlWin32::WindowProcedure(g_pContext, *g_pIme, hWnd, msg, wParam, lParam);
        // Moves always go on (ImGui hover, the game); a click, wheel or key RmlUi handled stays with it.
        if (msg == WM_MOUSEMOVE) return false;
        if (mouse) return !propagate || g_pContext->IsMouseInteracting();
        return !propagate;
    } catch (...) {
        Fail("input handling failed");
        return false;
    }
}

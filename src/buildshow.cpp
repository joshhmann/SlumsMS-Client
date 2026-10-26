// ============================================================
// buildshow.cpp: the release and build in the login screen's bottom-right corner (1.6.0).
//
// "SlumsMS 1.6.0 <middle dot> build 10600" (buildshow_text.h), white Dotum 12 with a dark shadow, on its own layer
// above the login UI. BuildShow_Tick (CWvsApp::CallUpdate) shows it while the stage is a CLogin (vftable 0x00AF6B24, stored by
// CLogin::CLogin 0x005F3C59) and drops it on any other stage. It is rebuilt when the resolution changes.
// ============================================================

#include "pch.h"
#include "debug.h"
#include "constants.h"
#include "buildshow_text.h"
#include "buildstamp_proto.h"
#include "wvs/util.h"
#include "wvs/wndman.h"
#include "ztl/ztl.h"

int get_screen_width();
int get_screen_height();

namespace {

constexpr uintptr_t kAddr_CurrentStage = 0x00BEDED4;   // the stage slot (get_stage)
constexpr uintptr_t kVft_CLogin = 0x00AF6B24;
constexpr uintptr_t kAddr_SetFont = 0x0046341A;        // IWzFont::SetFont(face, size, colour, ...)
constexpr int kMargin = 6;
constexpr int kLayerZ = static_cast<int>(0xC0061800u);  // the stock overlay band (chat balloons 0xC006156C, field clock 0xC00616FC)
constexpr DWORD kColText = 0xFFFFFFFF;
constexpr DWORD kColShadow = 0xC0101010;
constexpr DWORD kColPlate = 0x90000000;                // translucent dark plate behind the text
constexpr DWORD kClear = 0x00FFFFFF;                   // Canvas.dll's "zero the pixels" fill (see weathercanvas.cpp)

IWzGr2DLayer* g_pLayer = nullptr;
int g_nBuiltW = 0, g_nBuiltH = 0;
bool g_bFailed = false;

bool OnLoginStage() {
    void* stage = *reinterpret_cast<void**>(kAddr_CurrentStage);
    return stage && *reinterpret_cast<uintptr_t*>(stage) == kVft_CLogin;
}

IWzFontPtr MakeFont(DWORD col) {
    IWzFontPtr f;
    PcCreateObject<IWzFontPtr>(L"Canvas#Font", f, nullptr);
    if (f) {
        HRESULT hr = reinterpret_cast<HRESULT(__thiscall*)(IWzFont*, Ztl_bstr_t, unsigned long, unsigned long,
            const Ztl_variant_t&)>(kAddr_SetFont)(f, L"Dotum", 12, col, Ztl_variant_t(L""));
        if (FAILED(hr)) f = nullptr;
    }
    return f;
}

void Release() {
    if (!g_pLayer) return;
    try { g_pLayer->visible = 0; } catch (...) {}
    g_pLayer->Release();
    g_pLayer = nullptr;
}

void Build(int sw, int sh) {
    wchar_t text[96];
    BuildShow::LoginLabel(text, 96, kSlumsRelease, kSlumsBuildNumber);
    try {
        IWzFontPtr fText = MakeFont(kColText);
        IWzFontPtr fShadow = MakeFont(kColShadow);
        if (!fText || !fShadow) throw 0;
        const int w = static_cast<int>(fText->CalcTextWidth(Ztl_bstr_t(text), Ztl_variant_t())) + 8;
        const int h = 18;
        IWzCanvasPtr canvas;
        PcCreateObject<IWzCanvasPtr>(L"Canvas", canvas, nullptr);
        if (!canvas) throw 0;
        canvas->Create(w, h, 0, CANVAS_PIXFORMAT::CP_A8R8G8B8);
        canvas->DrawRectangle(0, 0, w, h, kClear);
        canvas->DrawRectangle(0, 0, w, h, kColPlate);
        canvas->DrawTextA(4, 3, Ztl_bstr_t(text), fShadow, Ztl_variant_t(), Ztl_variant_t());
        canvas->DrawTextA(3, 2, Ztl_bstr_t(text), fText, Ztl_variant_t(), Ztl_variant_t());
        // The canvas goes in through CreateLayer (InsertCanvas on a fresh layer drew nothing). The origin is the window
        // manager's top-left (wvs/wndman.h ResetOrgWindow), so the position is in screen pixels.
        IWzGr2DLayerPtr made = get_gr()->CreateLayer(0, 0, w, h, kLayerZ, static_cast<IUnknown*>(canvas),
            Ztl_variant_t());
        if (!made) throw 0;
        made->origin = static_cast<IUnknown*>(CWndMan::GetInstance()->m_pOrgWindow);
        made->RelMove(sw - w - kMargin, sh - h - kMargin, Ztl_variant_t(), Ztl_variant_t());
        made->color = 0xFFFFFFFF;   // a new layer's colour is 0x00FFFFFF: alpha 0, nothing drawn
        made->visible = 1;
        g_pLayer = made;
        g_pLayer->AddRef();
        g_nBuiltW = sw;
        g_nBuiltH = sh;
        DebugMessage("Build label: shown at %d,%d on a %dx%d screen", sw - w - kMargin, sh - h - kMargin, sw, sh);
    } catch (...) {
        Release();
        g_bFailed = true;   // not worth retrying every frame; the title still shows the build
        DebugMessage("Build label: could not draw the login-screen text");
    }
}

}  // namespace

void BuildShow_Tick() {
    if (!OnLoginStage()) {
        Release();
        return;
    }
    if (g_bFailed || !CWndMan::IsInstantiated()) return;
    const int sw = get_screen_width(), sh = get_screen_height();
    if (g_pLayer && (sw != g_nBuiltW || sh != g_nBuiltH)) Release();
    if (!g_pLayer) Build(sw, sh);
}

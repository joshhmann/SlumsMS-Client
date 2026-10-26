// ============================================================
// hud_native.cpp: HP, MP and EXP as separate stock-art modules (1.6.0, the owner's split of the status bar).
//
// The stock bar draws its three gauges as one strip (CUIStatusBar +0xCD4, UI/StatusBar.img/gauge/bar, 340x31), with
// a gray cover per gauge for the missing part (+0xBB8 HP, +0xBBC MP, +0xBC0/+0xBC4 EXP, gauge/gray stretched) and the
// numbers drawn on the bar. Each module here is our own game layers built from those same sprites: its slice of the
// strip, its own gray cover, and its numbers from UI/StatusBar.img/number. They move like the other Edit UI elements
// (uilayout.cpp: UiLayout_NativeRect / kNativeHp..kNativeExp); the stock strip and covers are hidden while they show.
// Measured on TEST (1600x900): the strip sits at bar origin + (218, 567); covers start at +60 (HP), +168 (MP),
// +223 (EXP) for the values then, which puts the gauge interiors at x 2-107, 109-215 and 223-338, rows 14-30.
// ============================================================

#include "pch.h"
#include "debug.h"
#include "constants.h"
#include "mapleroot.h"
#include "uilayout_policy.h"
#include "wvs/util.h"
#include "wvs/wndman.h"
#include "wvs/statusbar.h"
#include "ztl/ztl.h"

#include <algorithm>
#include <climits>
#include <cstdio>

int get_screen_width();
int get_screen_height();
bool HudVitals_Read(long& hp, long& maxHp, long& mp, long& maxMp);   // hud_vitals.cpp
void UiLayout_NativeRect(int e, int sliceW, int sliceH, int& x, int& y);   // uilayout.cpp

namespace {

constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;
constexpr int kOff_StripLayer = 0xCD4;
constexpr int kOff_Covers[] = {0xBB8, 0xBBC, 0xBC0, 0xBC4};
constexpr int kLayerZ = 3;   // above the bar's own layers (z 1-2), under windows

struct Gauge {
    int elem;          // UiLayout element
    int sliceL, sliceR;   // the module's slice of gauge/bar
    int inL, inR;      // the gauge interior inside the strip
    const wchar_t* label;
};
const Gauge kGauges[3] = {
    {UiLayout::kNativeHp, 0, 109, 2, 107, L"HP"},
    {UiLayout::kNativeMp, 109, 219, 109, 215, L"MP"},
    {UiLayout::kNativeExp, 219, 340, 223, 338, L"EXP"},
};
constexpr int kInTop = 14, kInH = 16, kStripH = 31;

struct Module {
    IWzGr2DLayer* slice = nullptr;
    IWzGr2DLayer* cover = nullptr;
    IWzGr2DLayer* text = nullptr;
    long long shownValue = -1, shownMax = -1;
    int x = INT_MIN, y = INT_MIN;
};
Module g_mod[3];
bool g_bBuilt = false, g_bFailed = false;
void* g_pBar = nullptr;

IWzCanvasPtr Sprite(const wchar_t* path) {
    try {
        return IWzCanvasPtr(get_unknown(get_rm()->GetObjectA(path)));
    } catch (...) {
        return nullptr;
    }
}

IWzCanvasPtr NewCanvas(int w, int h) {
    IWzCanvasPtr c;
    PcCreateObject<IWzCanvasPtr>(L"Canvas", c, nullptr);
    if (!c) return nullptr;
    c->Create(w, h, 0, CANVAS_PIXFORMAT::CP_A8R8G8B8);
    c->DrawRectangle(0, 0, w, h, 0x00FFFFFF);   // Canvas.dll's "zero the pixels" fill
    return c;
}

IWzGr2DLayer* NewLayer(IWzCanvasPtr canvas, int w, int h) {
    IWzGr2DLayerPtr made = get_gr()->CreateLayer(0, 0, w, h, kLayerZ, static_cast<IUnknown*>(canvas), Ztl_variant_t());
    if (!made) return nullptr;
    made->origin = static_cast<IUnknown*>(CWndMan::GetInstance()->m_pOrgWindow);
    made->color = 0xFFFFFFFF;   // a new layer's colour is 0x00FFFFFF: alpha 0, nothing drawn
    made->visible = 1;
    IWzGr2DLayer* p = made;
    p->AddRef();
    return p;
}

void Drop(IWzGr2DLayer*& p) {
    if (!p) return;
    try { p->visible = 0; } catch (...) {}
    p->Release();
    p = nullptr;
}

// Replaces a layer's single canvas.
void SetCanvas(IWzGr2DLayer* layer, IWzCanvasPtr canvas) {
    layer->RemoveCanvas(-2);
    layer->InsertCanvas(canvas, 1000, 255, 255);
    layer->color = 0xFFFFFFFF;
}

// "[12345/67890]" or "12345[45.67%]" in the bar's own number sprites; returns the drawn width.
int DrawNumbers(IWzCanvasPtr c, int x, int y, const char* s) {
    for (; *s; ++s) {
        const wchar_t* name = nullptr;
        wchar_t digit[48];
        switch (*s) {
        case '[': name = L"UI/StatusBar.img/number/Lbracket"; break;
        case ']': name = L"UI/StatusBar.img/number/Rbracket"; break;
        case '/': name = L"UI/StatusBar.img/number/slash"; break;
        case '%': name = L"UI/StatusBar.img/number/percent"; break;
        case '.':
            c->DrawRectangle(x, y + 6, 1, 1, 0xFFFFFFFF);
            x += 2;
            continue;
        default:
            if (*s < '0' || *s > '9') continue;
            swprintf_s(digit, L"UI/StatusBar.img/number/%c", static_cast<wchar_t>(*s));
            name = digit;
        }
        IWzCanvasPtr sp = Sprite(name);
        if (!sp) continue;
        const int top = (*s == '[' || *s == ']') ? y : y + 1;
        c->Copy(x, top, sp);
        x += static_cast<int>(sp->width) + 1;
    }
    return x;
}

bool Build() {
    IWzCanvasPtr bar = Sprite(L"UI/StatusBar.img/gauge/bar");
    if (!bar || !CWndMan::IsInstantiated()) return false;
    for (int i = 0; i < 3; ++i) {
        const Gauge& g = kGauges[i];
        const int w = g.sliceR - g.sliceL;
        IWzCanvasPtr slice = NewCanvas(w, kStripH);
        if (!slice) return false;
        slice->CopyEx(0, 0, bar, CANVAS_ALPHATYPE::CA_OVERWRITE, w, kStripH, g.sliceL, 0, w, kStripH);
        g_mod[i].slice = NewLayer(slice, w, kStripH);
        g_mod[i].cover = NewLayer(NewCanvas(1, kInH), 1, kInH);
        g_mod[i].text = NewLayer(NewCanvas(1, 11), 1, 11);
        if (!g_mod[i].slice || !g_mod[i].cover || !g_mod[i].text) return false;
    }
    return true;
}

void SetStockGaugesVisible(bool on) {
    if (!CUIStatusBar::IsInstantiated()) return;
    char* b = reinterpret_cast<char*>(CUIStatusBar::GetInstance());
    auto set = [&](int off) {
        IWzGr2DLayer* l = *reinterpret_cast<IWzGr2DLayer**>(b + off);
        if (l && l->visible != (on ? 1 : 0)) l->visible = on ? 1 : 0;
    };
    try {
        set(kOff_StripLayer);
        for (int off : kOff_Covers) set(off);
    } catch (...) {
    }
}

// The stock HP/MP/EXP numbers are drawn on the bar window's own canvas (+0x18, 1920x578, layer at origin y + 22), in
// the strip's label row: canvas x 218-558, y 545-558. Cleared every frame while the modules show (the bar redraws
// them whenever a value changes, after this tick).
void ClearStockNumbers() {
    if (!CUIStatusBar::IsInstantiated()) return;
    IWzCanvasPtr c = CUIStatusBar::GetInstance()->m_pLayer->Getcanvas(Ztl_variant_t(0L));
    if (c) c->DrawRectangle(218, 545, 340, 13, 0x00FFFFFF);
}

void Update(int i, long long value, long long max, bool exp) {
    Module& m = g_mod[i];
    const Gauge& g = kGauges[i];
    const int w = g.sliceR - g.sliceL;
    int x = 0, y = 0;
    UiLayout_NativeRect(g.elem, w, kStripH, x, y);
    const bool moved = x != m.x || y != m.y;
    if (value != m.shownValue || max != m.shownMax) {
        m.shownValue = value;
        m.shownMax = max;
        const int inW = g.inR - g.inL;
        const long long v = value < 0 ? 0 : (value > max ? max : value);
        const int filled = max > 0 ? static_cast<int>(inW * v / max) : 0;
        const int missing = inW - filled;
        IWzCanvasPtr gray = Sprite(L"UI/StatusBar.img/gauge/gray");
        IWzCanvasPtr cover = NewCanvas((std::max)(missing, 1), kInH);
        if (cover && gray && missing > 0) {
            cover->CopyEx(0, 0, gray, CANVAS_ALPHATYPE::CA_OVERWRITE, missing, kInH, 0, 0, 1, kInH);
        }
        if (cover) SetCanvas(m.cover, cover);
        char s[64];
        if (exp) {
            const double pct = max > 0 ? 100.0 * static_cast<double>(v) / static_cast<double>(max) : 0.0;
            sprintf_s(s, "%lld[%.2f%%]", value, pct);
        } else {
            sprintf_s(s, "[%lld/%lld]", value, max);
        }
        IWzCanvasPtr text = NewCanvas(140, 11);
        if (text) {
            DrawNumbers(text, 0, 0, s);
            SetCanvas(m.text, text);
        }
        m.x = INT_MIN;   // re-place the cover for its new width
    }
    if (moved || m.x == INT_MIN) {
        m.x = x;
        m.y = y;
        const int inW = g.inR - g.inL;
        const long long v = m.shownValue < 0 ? 0 : (m.shownValue > m.shownMax ? m.shownMax : m.shownValue);
        const int filled = m.shownMax > 0 ? static_cast<int>(inW * v / m.shownMax) : 0;
        m.slice->RelMove(x, y, Ztl_variant_t(), Ztl_variant_t());
        m.cover->RelMove(x + (g.inL - g.sliceL) + filled, y + kInTop, Ztl_variant_t(), Ztl_variant_t());
        m.text->RelMove(x + (exp ? 24 : 17), y + 2, Ztl_variant_t(), Ztl_variant_t());
        m.cover->visible = filled < inW ? 1 : 0;
    }
}

void DropAll() {
    for (Module& m : g_mod) {
        Drop(m.slice);
        Drop(m.cover);
        Drop(m.text);
        m.shownValue = m.shownMax = -1;
        m.x = m.y = INT_MIN;
    }
    g_bBuilt = false;
}

bool NativeHudEnabled() {
    static int s_on = -1;
    if (s_on < 0) s_on = GetPrivateProfileIntA("clienthax", "nativehud", 1, ".\\" CONSTANTS_CONFIG_NAME) ? 1 : 0;
    return s_on == 1;
}

}  // namespace

// From CWvsApp::CallUpdate (via UiLayout_Tick), every frame on the game thread.
void HudNative_Tick() {
    if (!NativeHudEnabled() || g_bFailed) return;
    const bool field = *reinterpret_cast<void* const*>(kAddr_UserLocal) != nullptr && CUIStatusBar::IsInstantiated();
    void* bar = field ? CUIStatusBar::GetInstance() : nullptr;
    if (!field || bar != g_pBar) {
        if (g_bBuilt) DropAll();
        g_pBar = bar;
        if (!field) return;
    }
    try {
        if (!g_bBuilt) {
            if (!Build()) {
                DropAll();
                g_bFailed = true;
                DebugMessage("Native HUD: could not build the HP/MP/EXP modules; the stock gauges stay");
                return;
            }
            g_bBuilt = true;
            DebugMessage("Native HUD: HP, MP and EXP modules up");
        }
        long hp = 0, maxHp = 0, mp = 0, maxMp = 0;
        if (!HudVitals_Read(hp, maxHp, mp, maxMp)) return;
        Update(0, hp, maxHp, false);
        Update(1, mp, maxMp, false);
        Update(2, MapleRoot_CurrentExp(), MapleRoot_NextLevelExp(), true);
        SetStockGaugesVisible(false);
        ClearStockNumbers();
    } catch (...) {
        DropAll();
        g_bFailed = true;
        SetStockGaugesVisible(true);
        LOG_ONCE("Native HUD: drawing failed; the stock gauges are back");
    }
}

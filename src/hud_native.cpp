// ============================================================
// hud_native.cpp: HP, MP, EXP and level/job/name as separate stock-art modules (1.6.0, the owner's split of the
// status bar).
//
// The stock bar paints these pieces across shared canvases:
//   plate art     base/backgrnd2 on the 800x578 canvas of CUIStatusBar +0xCD0 (x 0-209 level plate, 209-563 gauge
//                 plate, rows 36-69, from canvas y 507)
//   gauge strip   gauge/bar on +0xCD4 (340x31, at plate x 218, 2 px below the plate top)
//   gauge covers  gauge/gray stretched on +0xBB8 HP, +0xBBC MP, +0xBC0/+0xBC4 EXP
//   text          on the bar window's canvas (+0x18): HP/MP/EXP numbers in the strip's label row, the level digits
//                 (UI/Basic.img/LevelNo, x 50, y 554), job (x 87, y 545) and name (x 87, y 560), by
//                 CUIStatusBar::SetStatusValue (0x008D82B8) and SetNumberValue
// Each module here is our own game layers built from the same sprites (its plate slice, its gauge slice and cover or
// its level digits and text), so it looks like the stock piece and moves on its own (uilayout.cpp: kNativeHp ..
// kNativeLevel). While they show, the stock strip and covers are hidden and the stock plate art and text are cleared.
// Gauge interiors, measured on TEST from the stock covers: x 2-107 (HP), 109-215 (MP), 223-338 (EXP), rows 14-30.
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
#include <cstring>

int get_screen_width();
int get_screen_height();
bool HudVitals_Read(long& hp, long& maxHp, long& mp, long& maxMp);   // hud_vitals.cpp
void UiLayout_NativeRect(int e, int sliceW, int sliceH, int& x, int& y);   // uilayout.cpp
bool UiLayout_GroupOffset(int e, int& dx, int& dy, int& winX, int& winY);   // uilayout.cpp

namespace {

constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;
constexpr uintptr_t kAddr_CWvsContext = 0x00BE7918;
constexpr uintptr_t kAddr_GetCharacterData = 0x00425D0B;
constexpr uintptr_t kAddr_ZRefRelease = 0x00428C44;
constexpr uintptr_t kAddr_FuseShort = 0x004746DD;   // returns int with hpmp.cpp's detour
constexpr uintptr_t kAddr_FuseByte = 0x0047465D;
constexpr uintptr_t kAddr_GetJobName = 0x004A77EF;  // const char* get_job_name(int)
constexpr uintptr_t kAddr_SetFont = 0x0046341A;     // IWzFont::SetFont(face, size, colour, ...)
constexpr int kOff_PlateCanvasLayer = 0xCD0;
constexpr int kOff_StripLayer = 0xCD4;
constexpr int kOff_Covers[] = {0xBB8, 0xBBC, 0xBC0, 0xBC4};
constexpr int kZPlate = 3, kZTop = 4;   // above the bar's own layers (z 1-2), under windows

constexpr int kPlateTop = 36, kPlateH = 34;   // plate rows in base/backgrnd2
constexpr int kStripDy = 2;                   // the gauge strip starts 2 px below the plate top
constexpr int kStripX = 218;                  // the gauge strip's x on the plate art
constexpr int kInTop = 14, kInH = 16, kStripH = 31;

struct Gauge {
    int elem;             // UiLayout element
    int plateL, plateR;   // its slice of base/backgrnd2
    int sliceL, sliceR;   // its slice of gauge/bar (strip coordinates)
    int inL, inR;         // the gauge interior (strip coordinates)
};
const Gauge kGauges[3] = {
    {UiLayout::kNativeHp, 209, 327, 0, 109, 2, 107},
    {UiLayout::kNativeMp, 327, 437, 109, 219, 109, 215},
    {UiLayout::kNativeExp, 437, 563, 219, 340, 223, 338},
};
constexpr int kLevelPlateL = 0, kLevelPlateR = 209;

struct Module {
    IWzGr2DLayer* plate = nullptr;
    IWzGr2DLayer* slice = nullptr;   // gauges: gauge/bar slice
    IWzGr2DLayer* cover = nullptr;   // gauges: gray cover
    IWzGr2DLayer* text = nullptr;    // numbers, or level digits + job + name
    long long shownValue = -1, shownMax = -1;
    char shownText[64] = {};
    int x = INT_MIN, y = INT_MIN;
};
Module g_mod[4];   // HP, MP, EXP, level

// The chat input row's art (base/backgrnd2 x 0-563, rows 7-28; on the bar canvas at y 514), carried along when the
// chat input group moves: the stock row is cleared and this copy follows the box.
constexpr int kChatRowTop = 7, kChatRowH = 22, kChatRowW = 563;
IWzGr2DLayer* g_pChatRow = nullptr;
int g_nChatX = INT_MIN, g_nChatY = INT_MIN;
bool g_bBuilt = false, g_bFailed = false;
void* g_pBar = nullptr;
IWzFontPtr g_fontJob, g_fontName;

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

IWzGr2DLayer* NewLayer(IWzCanvasPtr canvas, int w, int h, int z) {
    IWzGr2DLayerPtr made = get_gr()->CreateLayer(0, 0, w, h, z, static_cast<IUnknown*>(canvas), Ztl_variant_t());
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

void SetCanvas(IWzGr2DLayer* layer, IWzCanvasPtr canvas) {
    layer->RemoveCanvas(-2);
    layer->InsertCanvas(canvas, 1000, 255, 255);
    layer->color = 0xFFFFFFFF;
}

IWzFontPtr MakeFont(int size, DWORD col) {
    IWzFontPtr f;
    PcCreateObject<IWzFontPtr>(L"Canvas#Font", f, nullptr);
    if (f) {
        HRESULT hr = reinterpret_cast<HRESULT(__thiscall*)(IWzFont*, Ztl_bstr_t, unsigned long, unsigned long,
            const Ztl_variant_t&)>(kAddr_SetFont)(f, L"Dotum", size, col, Ztl_variant_t(L""));
        if (FAILED(hr)) f = nullptr;
    }
    return f;
}

// Digits and signs from a sprite folder ("UI/StatusBar.img/number" or "UI/Basic.img/LevelNo"); returns the end x.
int DrawSpriteNumber(IWzCanvasPtr c, int x, int y, const char* s, const wchar_t* folder) {
    for (; *s; ++s) {
        wchar_t path[96];
        const wchar_t* leaf = nullptr;
        switch (*s) {
        case '[': leaf = L"Lbracket"; break;
        case ']': leaf = L"Rbracket"; break;
        case '/': leaf = L"slash"; break;
        case '%': leaf = L"percent"; break;
        case '.':
            c->DrawRectangle(x, y + 6, 1, 1, 0xFFFFFFFF);
            x += 2;
            continue;
        default:
            break;
        }
        if (leaf) swprintf_s(path, L"%s/%s", folder, leaf);
        else if (*s >= '0' && *s <= '9') swprintf_s(path, L"%s/%c", folder, static_cast<wchar_t>(*s));
        else continue;
        IWzCanvasPtr sp = Sprite(path);
        if (!sp) continue;
        const int top = (*s == '[' || *s == ']') ? y : y + 1;
        c->Copy(x, top, sp);
        x += static_cast<int>(sp->width) + 1;
    }
    return x;
}

// A slice of a sprite as its own canvas.
IWzCanvasPtr Slice(IWzCanvasPtr src, int l, int t, int w, int h) {
    IWzCanvasPtr c = NewCanvas(w, h);
    if (c && src) c->CopyEx(0, 0, src, CANVAS_ALPHATYPE::CA_OVERWRITE, w, h, l, t, w, h);
    return c;
}

bool Build() {
    IWzCanvasPtr bar = Sprite(L"UI/StatusBar.img/gauge/bar");
    IWzCanvasPtr plate = Sprite(L"UI/StatusBar.img/base/backgrnd2");
    if (!bar || !plate || !CWndMan::IsInstantiated()) return false;
    for (int i = 0; i < 3; ++i) {
        const Gauge& g = kGauges[i];
        const int pw = g.plateR - g.plateL, sw = g.sliceR - g.sliceL;
        g_mod[i].plate = NewLayer(Slice(plate, g.plateL, kPlateTop, pw, kPlateH), pw, kPlateH, kZPlate);
        g_mod[i].slice = NewLayer(Slice(bar, g.sliceL, 0, sw, kStripH), sw, kStripH, kZTop);
        g_mod[i].cover = NewLayer(NewCanvas(1, kInH), 1, kInH, kZTop);
        g_mod[i].text = NewLayer(NewCanvas(1, 11), 1, 11, kZTop);
        if (!g_mod[i].plate || !g_mod[i].slice || !g_mod[i].cover || !g_mod[i].text) return false;
    }
    const int lw = kLevelPlateR - kLevelPlateL;
    g_mod[3].plate = NewLayer(Slice(plate, kLevelPlateL, kPlateTop, lw, kPlateH), lw, kPlateH, kZPlate);
    g_mod[3].text = NewLayer(NewCanvas(1, 1), 1, 1, kZTop);
    g_pChatRow = NewLayer(Slice(plate, 0, kChatRowTop, kChatRowW, kChatRowH), kChatRowW, kChatRowH, kZPlate);
    if (g_pChatRow) g_pChatRow->visible = 0;
    g_fontJob = MakeFont(11, 0xFFFFFFFF);
    g_fontName = MakeFont(11, 0xFFFFFFFF);
    return g_mod[3].plate && g_mod[3].text;
}

void SetStockPiecesVisible(bool on) {
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

// The stock plate art (on +0xCD0's canvas) and text (on the window canvas) under the modules, cleared every frame:
// the bar repaints them on stat changes, after this tick.
void ClearStockArt() {
    if (!CUIStatusBar::IsInstantiated()) return;
    char* b = reinterpret_cast<char*>(CUIStatusBar::GetInstance());
    IWzCanvasPtr win = CUIStatusBar::GetInstance()->m_pLayer->Getcanvas(Ztl_variant_t(0L));
    if (win) win->DrawRectangle(0, 507 + kPlateTop - 2, 563, kPlateH + 4, 0x00FFFFFF);
    IWzGr2DLayer* plateLayer = *reinterpret_cast<IWzGr2DLayer**>(b + kOff_PlateCanvasLayer);
    if (plateLayer) {
        IWzCanvasPtr pc = plateLayer->Getcanvas(Ztl_variant_t(0L));
        if (pc) pc->DrawRectangle(0, 507 + kPlateTop, 563, kPlateH, 0x00FFFFFF);
    }
}

void Place(IWzGr2DLayer* l, int x, int y) {
    l->RelMove(x, y, Ztl_variant_t(), Ztl_variant_t());
}

void UpdateGauge(int i, long long value, long long max, bool exp) {
    Module& m = g_mod[i];
    const Gauge& g = kGauges[i];
    int x = 0, y = 0;
    UiLayout_NativeRect(g.elem, g.plateR - g.plateL, kPlateH, x, y);
    bool place = x != m.x || y != m.y;
    if (value != m.shownValue || max != m.shownMax) {
        m.shownValue = value;
        m.shownMax = max;
        const int inW = g.inR - g.inL;
        const long long v = value < 0 ? 0 : (value > max ? max : value);
        const int missing = inW - (max > 0 ? static_cast<int>(inW * v / max) : 0);
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
            DrawSpriteNumber(text, 0, 0, s, L"UI/StatusBar.img/number");
            SetCanvas(m.text, text);
        }
        place = true;
    }
    if (!place) return;
    m.x = x;
    m.y = y;
    // The strip slice sits where it sits on the stock plate: plate x of the slice = kStripX + sliceL.
    const int sx = x + (kStripX + g.sliceL - g.plateL), sy = y + kStripDy;
    const int inW = g.inR - g.inL;
    const long long v = m.shownValue < 0 ? 0 : (m.shownValue > m.shownMax ? m.shownMax : m.shownValue);
    const int filled = m.shownMax > 0 ? static_cast<int>(inW * v / m.shownMax) : 0;
    Place(m.plate, x, y);
    Place(m.slice, sx, sy);
    Place(m.cover, sx + (g.inL - g.sliceL) + filled, sy + kInTop);
    Place(m.text, sx + (exp ? 24 : 17), sy + 2);
    m.cover->visible = filled < inW ? 1 : 0;
}

struct Who {
    bool ok;
    int level;
    int job;
    char name[16];
};

Who ReadWho() {
    Who w = {};
    __try {
        char* ctx = *reinterpret_cast<char**>(kAddr_CWvsContext);
        if (!ctx) return w;
        void* out[2] = {nullptr, nullptr};
        void* ret = reinterpret_cast<void*(__thiscall*)(void*, void**)>(kAddr_GetCharacterData)(ctx, out);
        char* cs = *reinterpret_cast<char**>(reinterpret_cast<char*>(ret) + 4);
        if (out[1]) reinterpret_cast<void(__thiscall*)(void**, int)>(kAddr_ZRefRelease)(out, 0);
        if (!cs) return w;
        w.level = static_cast<unsigned char>(reinterpret_cast<unsigned char(__cdecl*)(const void*, unsigned)>(
            kAddr_FuseByte)(cs + 0x33, *reinterpret_cast<unsigned*>(cs + 0x35)));
        w.job = static_cast<short>(reinterpret_cast<int(__cdecl*)(const void*, unsigned)>(kAddr_FuseShort)(
            cs + 0x39, *reinterpret_cast<unsigned*>(cs + 0x3D)));
        memcpy(w.name, cs + 4, 13);
        w.name[13] = 0;
        w.ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        w.ok = false;
    }
    return w;
}

void UpdateLevel() {
    Module& m = g_mod[3];
    const Who w = ReadWho();
    if (!w.ok) return;
    int x = 0, y = 0;
    UiLayout_NativeRect(UiLayout::kNativeLevel, kLevelPlateR - kLevelPlateL, kPlateH, x, y);
    char key[64];
    sprintf_s(key, "%d|%d|%s", w.level, w.job, w.name);
    if (strcmp(key, m.shownText) != 0) {
        strcpy_s(m.shownText, key);
        IWzCanvasPtr c = NewCanvas(kLevelPlateR - kLevelPlateL, kPlateH);
        if (c) {
            char lv[8];
            sprintf_s(lv, "%d", w.level);
            const int adjust = w.level > 99 ? 12 : (w.level > 9 ? 6 : 0);
            // Stock places (window canvas): digits x 50, y 554; job x 87, y 545; name x 87, y 560. Plate top = 543.
            DrawSpriteNumber(c, 50 - adjust, 554 - 543 - 1, lv, L"UI/Basic.img/LevelNo");
            const char* job = reinterpret_cast<const char*(__cdecl*)(int)>(kAddr_GetJobName)(w.job);
            try {
                if (g_fontJob && job) c->DrawTextA(87, 545 - 543, Ztl_bstr_t(job), g_fontJob, Ztl_variant_t(), Ztl_variant_t());
                if (g_fontName) c->DrawTextA(87, 560 - 543, Ztl_bstr_t(w.name), g_fontName, Ztl_variant_t(), Ztl_variant_t());
            } catch (...) {
            }
            SetCanvas(m.text, c);
        }
        m.x = INT_MIN;
    }
    if (x != m.x || y != m.y) {
        m.x = x;
        m.y = y;
        Place(m.plate, x, y);
        Place(m.text, x, y);
    }
}

void UpdateChatRow() {
    if (!g_pChatRow || !CUIStatusBar::IsInstantiated()) return;
    int dx = 0, dy = 0, wx = 0, wy = 0;
    if (!UiLayout_GroupOffset(UiLayout::kChatInput, dx, dy, wx, wy)) return;
    const bool moved = dx || dy;
    if (moved) {
        // Stock row: cleared on the plate art canvas (+0xCD0) every frame, as the other stock pieces.
        char* b = reinterpret_cast<char*>(CUIStatusBar::GetInstance());
        IWzGr2DLayer* plateLayer = *reinterpret_cast<IWzGr2DLayer**>(b + kOff_PlateCanvasLayer);
        if (plateLayer) {
            IWzCanvasPtr pc = plateLayer->Getcanvas(Ztl_variant_t(0L));
            if (pc) pc->DrawRectangle(0, 507 + kChatRowTop, kChatRowW, kChatRowH, 0x00FFFFFF);
        }
        const int x = wx + dx, y = wy + 507 + kChatRowTop + dy;
        if (x != g_nChatX || y != g_nChatY) {
            g_nChatX = x;
            g_nChatY = y;
            Place(g_pChatRow, x, y);
        }
    }
    if (g_pChatRow->visible != (moved ? 1 : 0)) g_pChatRow->visible = moved ? 1 : 0;
}

void DropAll() {
    Drop(g_pChatRow);
    g_nChatX = g_nChatY = INT_MIN;
    for (Module& m : g_mod) {
        Drop(m.plate);
        Drop(m.slice);
        Drop(m.cover);
        Drop(m.text);
        m.shownValue = m.shownMax = -1;
        m.shownText[0] = 0;
        m.x = m.y = INT_MIN;
    }
    g_fontJob = nullptr;
    g_fontName = nullptr;
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
                DebugMessage("Native HUD: could not build the modules; the stock bar stays");
                return;
            }
            g_bBuilt = true;
            DebugMessage("Native HUD: HP, MP, EXP and level modules up");
        }
        long hp = 0, maxHp = 0, mp = 0, maxMp = 0;
        if (!HudVitals_Read(hp, maxHp, mp, maxMp)) return;
        UpdateGauge(0, hp, maxHp, false);
        UpdateGauge(1, mp, maxMp, false);
        UpdateGauge(2, MapleRoot_CurrentExp(), MapleRoot_NextLevelExp(), true);
        UpdateLevel();
        UpdateChatRow();
        SetStockPiecesVisible(false);
        ClearStockArt();
    } catch (...) {
        DropAll();
        g_bFailed = true;
        SetStockPiecesVisible(true);
        LOG_ONCE("Native HUD: drawing failed; the stock bar is back");
    }
}

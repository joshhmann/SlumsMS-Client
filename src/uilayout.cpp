// ============================================================
// uilayout.cpp: the stock bottom bar and quick slot, moved where the player puts them (1.6.0, ticket 91).
//
// The stock HUD hangs off origin vectors that resolution.cpp creates and wvs/wndman.h ResetOrgWindow places:
// CUIStatusBar and everything its OnCreate builds on CWndMan::ms_pOrgStatusBar, the quick slot on ms_pOrgQuickSlot
// (its clicks already go through GetShortCutIndexByPos_hook, which works from the two origins). ResetOrgWindow asks
// UiLayout_Offset for each element's offset, so a move is one RelMove of that origin. ResetOrgWindow runs at start-up
// and on every resolution change, and this file calls it again on character load and while dragging.
//
// Editing uses the HUD's Ctrl+F11 freeze switch: unfrozen (in the field), each element gets a grip from the D3D9
// overlay (UiLayout_DrawGrips, from overlay.cpp) that drags it live. Offsets are per character in
// HKCU\Software\SlumsMS\Layout (uilayout_policy.h), loaded from CConfig::LoadCharacter like the inventory size.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "constants.h"
#include "uilayout_policy.h"
#include <climits>
#include <cstring>
#include "wvs/wndman.h"
#include "wvs/statusbar.h"

#ifdef MAPLEROOT_INTEGRATION
#include "d3d9host.h"
#include "overlay_policy.h"
#include "imgui.h"
#endif

int get_screen_width();
int get_screen_height();

namespace {

using namespace UiLayout;

constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;      // CUserLocal singleton: set only while in the field
constexpr int kOff_QuickSlotLayer = 0xCC4;             // CUIStatusBar: the quick slot layer OnCreate makes (0x008D156E)
// CUIStatusBar's layer (and window) is 1920x578: it reserves the room its chat log expands into. The bar a player
// sees and grabs is the bottom strip: the chat input row plus the gauges and buttons.
constexpr int kBarGripH = 66;
constexpr const char* kStoreKey = "Software\\SlumsMS\\Layout";
constexpr const char* kSuffix[kElemCount] = {"bar", "qs", "btn", "icons", "chat"};
constexpr const char* kLabel[kElemCount] = {"Status bar", "Quick slots", "Buttons", "Icon buttons", "Chat input"};

// The status bar's own child controls (CWnd::m_lpChildren, CCtrlWnd per MapleRoot's API header: id +0x14, position
// vector m_pLTCtrl +0x18, width +0x1C, height +0x20). Listed on TEST 2026-10-26: Shop 1000, Trade 1001, Menu 1002,
// Shortcut 1007 (54x34 at y 543); the icon row 2000-2003, 1005, 1006, 1009 (y 515). Each group moves as one; a
// control keeps its own click handling (CWnd::HitTest asks each child at its own position).
// The chat input group: the edit box 1011 (85,520 440x12), the chat-type selector 1012 and the small buttons 1004
// beside the box.
constexpr int kGroupCount = kElemCount - kFirstGroup;
constexpr unsigned kGroupIds[kGroupCount][8] = {
    {1000, 1001, 1002, 1007, 0, 0, 0, 0},
    {2000, 2001, 2002, 2003, 1005, 1006, 1009, 0},
    {1011, 1012, 1004, 0, 0, 0, 0, 0},
};
constexpr int kOff_Children = 0x60;                    // CWnd::m_lpChildren head
constexpr int kOff_CtrlId = 0x14, kOff_CtrlLT = 0x18, kOff_CtrlW = 0x1C, kOff_CtrlH = 0x20;
constexpr uintptr_t kAddr_WndInvalidateRect = 0x009E04C9;   // CWnd::InvalidateRect(const RECT*), null = all

Offset g_off[kElemCount];
int g_nWorld = -1;
unsigned g_dwChar = 0;
int g_nDragging = -1;
int g_nDragDx = 0, g_nDragDy = 0;   // the dragged element's pixel offset

// A grouped control and where the client put it (bar coordinates), seen before we ever move it.
// Keyed by control id, never by pointer: the client destroys and recreates controls (the chat edit box on every
// chat open/close), so a kept pointer dangles. Every instance of an id sits at the same stock place (the three 1004s
// are all at 536,519), so one entry per id covers them.
struct StockCtl {
    unsigned id;
    int group;
    int x, y, w, h;
};
StockCtl g_stock[32];
int g_nStock = 0;
void* g_pStockBar = nullptr;   // the CUIStatusBar the stock positions belong to

void ValueName(char (&name)[48], int e) {
    sprintf_s(name, "w%d_c%u_%s", g_nWorld, g_dwChar, kSuffix[e]);
}

void Save(int e) {
    if (g_nWorld < 0) return;
    char name[48];
    ValueName(name, e);
    LSTATUS st;
    if (g_off[e].fx == 0 && g_off[e].fy == 0) {
        st = RegDeleteKeyValueA(HKEY_CURRENT_USER, kStoreKey, name);
    } else {
        const DWORD v = Pack(g_off[e]);
        st = RegSetKeyValueA(HKEY_CURRENT_USER, kStoreKey, name, REG_DWORD, &v, sizeof(v));
    }
    DebugMessage("UI layout: saved %s %d,%d (1/10000 screen) for w%d_c%u (%ld)", kSuffix[e], g_off[e].fx, g_off[e].fy,
        g_nWorld, g_dwChar, st);
}

void ApplyGroups();

void Reapply() {
    if (CWndMan::IsInstantiated()) CWndMan::GetInstance()->ResetOrgWindow();
    ApplyGroups();
}

int GroupOf(unsigned id) {
    for (int g = 0; g < kGroupCount; ++g) {
        for (unsigned gid : kGroupIds[g]) {
            if (gid && gid == id) return kFirstGroup + g;
        }
    }
    return -1;
}

// Walks the bar's children the way CWnd::HitTest (0x009E01E7) does; f(control) for each.
template <typename F>
void ForEachBarControl(char* bar, F f) {
    char* node = *reinterpret_cast<char**>(bar + kOff_Children);
    for (int n = 0; node && n < 64; ++n) {
        char* c = *reinterpret_cast<char**>(node + 4);   // ZRef<CCtrlWnd>::p
        if (c) f(c);
        char* next = *reinterpret_cast<char**>(node - 0x10 + 8);
        node = next ? next + 0x10 : nullptr;
    }
}

IWzVector2D* CtlLT(char* c) {
    return *reinterpret_cast<IWzVector2D**>(c + kOff_CtrlLT);
}

// The stock entry for a live control, or null.
const StockCtl* StockOf(unsigned id) {
    for (int i = 0; i < g_nStock; ++i) {
        if (g_stock[i].id == id) return &g_stock[i];
    }
    return nullptr;
}

// Calls f(control, stock) for each live grouped control, every time from the live list.
template <typename F>
void ForEachGrouped(char* bar, F f) {
    ForEachBarControl(bar, [&](char* c) {
        if (const StockCtl* sc = StockOf(*reinterpret_cast<unsigned*>(c + kOff_CtrlId))) f(c, *sc);
    });
}

// Remembers each grouped control's stock place the first time it is seen (and again for a new status bar).
void CaptureStock(char* bar) {
    if (bar != g_pStockBar) {
        g_pStockBar = bar;
        g_nStock = 0;
    }
    ForEachBarControl(bar, [](char* c) {
        const unsigned id = *reinterpret_cast<unsigned*>(c + kOff_CtrlId);
        const int g = GroupOf(id);
        if (g < 0 || StockOf(id) || g_nStock >= 32) return;   // not grouped, or its stock place is already kept
        IWzVector2D* lt = CtlLT(c);
        if (!lt) return;
        g_stock[g_nStock++] = StockCtl{id, g, lt->rx, lt->ry, *reinterpret_cast<int*>(c + kOff_CtrlW),
            *reinterpret_cast<int*>(c + kOff_CtrlH)};
    });
    LOG_ONCE_PER_ID(g_nStock, "UI layout: %d status bar buttons in movable groups", g_nStock);
}

// The bar window on screen (its layer's top-left, the window's full size).
bool BarWindow(Rect& r) {
    if (!CUIStatusBar::IsInstantiated() || !CWndMan::IsInstantiated()) return false;
    CUIStatusBar* bar = CUIStatusBar::GetInstance();
    IWzGr2DLayer* layer = bar->m_pLayer.GetInterfacePtr();
    IWzVector2DPtr org = CWndMan::GetInstance()->m_pOrgWindow;
    if (!layer || !org) return false;
    r = Rect{layer->x - org->x, layer->y - org->y, bar->m_width, bar->m_height};
    return r.w > 0 && r.h > 0;
}

// A group's stock rect on screen (the bar where it is now) and where it may go.
bool GroupHome(int e, Rect& home, Rect& bounds) {
    Rect win;
    if (!BarWindow(win)) return false;
    int l = INT_MAX, t = INT_MAX, r = INT_MIN, b = INT_MIN;
    for (int i = 0; i < g_nStock; ++i) {
        const StockCtl& sc = g_stock[i];
        if (sc.group != e) continue;
        l = (std::min)(l, sc.x); t = (std::min)(t, sc.y);
        r = (std::max)(r, sc.x + sc.w); b = (std::max)(b, sc.y + sc.h);
    }
    if (l == INT_MAX) return false;
    home = Rect{win.left + l, win.top + t, r - l, b - t};
    bounds = Intersect(win, Rect{0, 0, get_screen_width(), get_screen_height()});
    return true;
}

void GroupOffsetPx(int e, int& dx, int& dy) {
    dx = dy = 0;
    if (e == g_nDragging) {
        dx = g_nDragDx;
        dy = g_nDragDy;
        return;
    }
    Rect home, bounds;
    if (!GroupHome(e, home, bounds)) return;
    PixelOffsetWithin(g_off[e], home, bounds, get_screen_width(), get_screen_height(), dx, dy);
}

// Puts every grouped control where its group's offset says; repaints the bar if anything moved.
void ApplyGroups() {
    if (!CUIStatusBar::IsInstantiated()) return;
    char* bar = reinterpret_cast<char*>(CUIStatusBar::GetInstance());
    try {
        CaptureStock(bar);
        int off[kElemCount][2] = {};
        for (int e = kFirstGroup; e < kElemCount; ++e) GroupOffsetPx(e, off[e][0], off[e][1]);
        bool moved = false;
        IWzCanvasPtr canvas;
        ForEachGrouped(bar, [&](char* c, const StockCtl& sc) {
            IWzVector2D* lt = CtlLT(c);
            if (!lt) return;
            const int x = sc.x + off[sc.group][0], y = sc.y + off[sc.group][1];
            const int ox = lt->rx, oy = lt->ry;
            if (ox != x || oy != y) {
                // Controls paint into the bar's canvas, and nothing clears its empty (chat log) area: wipe where the
                // control was, or every position it passed through stays drawn. The repaint below restores the
                // bar's own art where there is some. 0x00FFFFFF is Canvas.dll's "zero these pixels" fill.
                if (!canvas) canvas = CUIStatusBar::GetInstance()->m_pLayer->Getcanvas(Ztl_variant_t(0L));
                if (canvas) canvas->DrawRectangle(ox, oy, sc.w, sc.h, 0x00FFFFFF);
                lt->RelMove(x, y, Ztl_variant_t(), Ztl_variant_t());
                moved = true;
            }
        });
        if (moved) reinterpret_cast<void(__thiscall*)(void*, const RECT*)>(kAddr_WndInvalidateRect)(bar, nullptr);
    } catch (...) {
        LOG_ONCE("UI layout: could not move the status bar's buttons");
    }
}

// The element's layer, or null (no status bar outside the field).
IWzGr2DLayer* LayerOf(int e) {
    if (!CUIStatusBar::IsInstantiated()) return nullptr;
    CUIStatusBar* bar = CUIStatusBar::GetInstance();
    if (e == kStatusBar) return bar->m_pLayer.GetInterfacePtr();
    return reinterpret_cast<IWzGr2DLayerPtr*>(reinterpret_cast<char*>(bar) + kOff_QuickSlotLayer)->GetInterfacePtr();
}

// The quick slot layer's canvas is wider than the slots drawn on it (1008 px for about 460 px of slots): the drawn
// width is where its last non-transparent column is. Scanned once per canvas, three rows, every second column.
int DrawnWidth(IWzGr2DLayer* layer, int fallback) {
    static IWzCanvas* s_canvas = nullptr;
    static int s_width = 0;
    try {
        IWzCanvasPtr c = layer->Getcanvas(Ztl_variant_t(0L));
        if (!c) return fallback;
        if (c.GetInterfacePtr() == s_canvas) return s_width;
        const int w = c->width, h = c->height;
        int drawn = 0;
        for (int x = w - 1; x >= 0 && !drawn; x -= 2) {
            for (int y : {h / 4, h / 2, h * 3 / 4}) {
                if (static_cast<unsigned>(c->Getpixel(x, y)) >> 24) {
                    drawn = x + 2;
                    break;
                }
            }
        }
        s_canvas = c.GetInterfacePtr();
        s_width = drawn > 0 ? (std::min)(drawn, w) : fallback;
        DebugMessage("UI layout: quick slot canvas %dx%d, drawn width %d", w, h, s_width);
        return s_width;
    } catch (...) {
        return fallback;
    }
}

// Where the element sits at offset 0, in screen pixels: worked out from the fixed anchors (the bottom-left origin
// ResetOrgWindow places, the quick slot's stock (152,68) from it, and each layer's own offset from its origin),
// never from where the element is now. Learning it from the live position fed the current offset back into the
// clamp and walked the quick slot off screen.
bool HomeRect(int e, Rect& r) {
    if (e >= kFirstGroup) return false;   // groups: GroupHome
    try {
        IWzGr2DLayer* layer = LayerOf(e);
        if (!layer || !CWndMan::IsInstantiated()) return false;
        IWzVector2DPtr org = CWndMan::GetInstance()->m_pOrgWindow;
        IWzVector2DPtr lb = CWndMan::ms_pOrgWindowEx[CWnd::UIOrigin::Origin_LB];
        if (!org || !lb) return false;
        const int sw = get_screen_width(), sh = get_screen_height();
        const int lbX = lb->x - org->x, lbY = lb->y - org->y;
        r.w = layer->Getwidth();
        r.h = layer->Getheight();
        if (e == kStatusBar) {
            r.left = lbX + layer->rx;
            r.top = lbY + layer->ry + r.h - kBarGripH;
            r.h = kBarGripH;
            r.w = (std::min)(r.w, sw - r.left);
        } else {
            const bool wide = sw > 800;   // as ResetOrgWindow places the quick slot origin
            r.left = lbX + (wide ? 152 : 0) + layer->rx;
            r.top = lbY + (wide ? 68 : 0) + layer->ry;
            r.w = DrawnWidth(layer, r.w);
        }
        // Never trust a rect that is not plausibly on screen: a bad read must not move anything.
        return r.w > 0 && r.h > 0 && r.left > -sw && r.left < sw && r.top > -sh && r.top < sh;
    } catch (...) {
        return false;
    }
}

}  // namespace

// From ResetOrgWindow (wvs/wndman.h): the pixel offset for an element on the current screen.
void UiLayout_Offset(int e, int& dx, int& dy) {
    dx = dy = 0;
    if (e < 0 || e >= kElemCount) return;
    const int sw = get_screen_width(), sh = get_screen_height();
    if (e == g_nDragging) {
        dx = g_nDragDx;
        dy = g_nDragDy;
        return;
    }
    Rect home;
    if (!HomeRect(e, home)) home = Rect{};   // no layer yet: only kept within one screen
    PixelOffset(g_off[e], home, sw, sh, dx, dy);
}

namespace {

// QA state for the client-mcp driver ([debug] uistate=1 in config.ini; never on for players): the stage, whether the
// chat input has focus, and where the moved UI is, in game pixels. The driver trusts this instead of guessing from a
// downscaled screenshot. Written twice a second to slums-ui-state.json next to the exe (temp file + rename).
constexpr uintptr_t kAddr_CurrentStage = 0x00BEDED4;
constexpr uintptr_t kVft_CLogin = 0x00AF6B24;
constexpr uintptr_t kAddr_CtrlIsFocused = 0x004E0101;   // CCtrlWnd::IsFocused: CWndMan+0x88 == the control
constexpr unsigned kId_ChatInput = 1011;

bool UiStateEnabled() {
    static int s_on = -1;
    if (s_on < 0) s_on = GetPrivateProfileIntA("debug", "uistate", 0, ".\\" CONSTANTS_CONFIG_NAME) ? 1 : 0;
    return s_on == 1;
}

void AppendRect(char* out, size_t n, const char* name, const Rect& r, bool ok) {
    char item[96];
    if (ok) sprintf_s(item, ",\"%s\":[%d,%d,%d,%d]", name, r.left, r.top, r.w, r.h);
    else sprintf_s(item, ",\"%s\":null", name);
    strcat_s(out, n, item);
}

void WriteUiState() {
    static ULONGLONG s_next = 0;
    const ULONGLONG now = GetTickCount64();
    if (now < s_next) return;
    s_next = now + 500;
    const char* stage = "other";
    void* st = *reinterpret_cast<void**>(kAddr_CurrentStage);
    if (*reinterpret_cast<void* const*>(kAddr_UserLocal)) stage = "field";
    else if (st && *reinterpret_cast<uintptr_t*>(st) == kVft_CLogin) stage = "login";
    bool chatOpen = false;
    Rect chat{}, bar{}, qs{}, groups[2]{};
    bool chatOk = false, barOk = false, qsOk = false, groupOk[2] = {};
    try {
        if (CUIStatusBar::IsInstantiated() && CWndMan::IsInstantiated()) {
            char* b = reinterpret_cast<char*>(CUIStatusBar::GetInstance());
            Rect win{};
            const bool winOk = BarWindow(win);
            ForEachBarControl(b, [&](char* c) {
                if (*reinterpret_cast<unsigned*>(c + kOff_CtrlId) != kId_ChatInput) return;
                chatOpen = reinterpret_cast<int(__thiscall*)(char*)>(kAddr_CtrlIsFocused)(c) != 0;
                IWzVector2D* lt = CtlLT(c);
                if (winOk && lt) {
                    chat = Rect{win.left + lt->rx, win.top + lt->ry, *reinterpret_cast<int*>(c + kOff_CtrlW),
                        *reinterpret_cast<int*>(c + kOff_CtrlH)};
                    chatOk = true;
                }
            });
            int dx = 0, dy = 0;
            if (HomeRect(kStatusBar, bar)) { UiLayout_Offset(kStatusBar, dx, dy); bar.left += dx; bar.top += dy; barOk = true; }
            if (HomeRect(kQuickSlot, qs)) { UiLayout_Offset(kQuickSlot, dx, dy); qs.left += dx; qs.top += dy; qsOk = true; }
            for (int g = 0; g < 2; ++g) {
                Rect bounds;
                if (GroupHome(kFirstGroup + g, groups[g], bounds)) {
                    GroupOffsetPx(kFirstGroup + g, dx, dy);
                    groups[g].left += dx;
                    groups[g].top += dy;
                    groupOk[g] = true;
                }
            }
        }
    } catch (...) {
    }
    char json[768];
    sprintf_s(json, "{\"pid\":%lu,\"t_ms\":%llu,\"stage\":\"%s\",\"screen\":[%d,%d],\"chat_open\":%s",
        GetCurrentProcessId(), now, stage, get_screen_width(), get_screen_height(), chatOpen ? "true" : "false");
    AppendRect(json, sizeof(json), "status_bar", bar, barOk);
    AppendRect(json, sizeof(json), "chat_input", chat, chatOk);
    AppendRect(json, sizeof(json), "quick_slot", qs, qsOk);
    AppendRect(json, sizeof(json), "buttons", groups[0], groupOk[0]);
    AppendRect(json, sizeof(json), "icon_buttons", groups[1], groupOk[1]);
    strcat_s(json, "}\n");
    HANDLE h = CreateFileA("slums-ui-state.json.tmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, json, static_cast<DWORD>(strlen(json)), &written, nullptr);
    CloseHandle(h);
    MoveFileExA("slums-ui-state.json.tmp", "slums-ui-state.json", MOVEFILE_REPLACE_EXISTING);
}

// CUIStatusBar::HitTest (0x008DE7DA) runs CWnd::HitTest, which finds the control under the point, then throws the
// hit away (result 0: the click goes to the map) everywhere above the bar's strip: the window is 1920x578 to make room
// for the chat log, and only its bottom (local y >= 507, or >= 427 right of x 647 with the quick slot up) is the bar.
// A button group moved up into that area must keep its clicks: if the point is on one of our moved controls, the
// hit stands (result 2, ppCtrl = the control), as it would at the control's stock place.
auto g_pBarHitTest = reinterpret_cast<int(__thiscall*)(void*, int, int, void**)>(0x008DE7DA);

int __fastcall BarHitTest_hook(void* bar, void*, int x, int y, void** ppCtrl) {
    const int r = g_pBarHitTest(bar, x, y, ppCtrl);
    if (r != 0 || bar != g_pStockBar) return r;
    int hit = 0;
    try {
        ForEachGrouped(static_cast<char*>(bar), [&](char* c, const StockCtl& sc) {
            if (hit) return;
            if (!g_off[sc.group].fx && !g_off[sc.group].fy && sc.group != g_nDragging) return;   // at its stock place
            IWzVector2D* lt = CtlLT(c);
            if (!lt) return;
            const int cx = lt->rx, cy = lt->ry;
            if (x >= cx && x < cx + sc.w && y >= cy && y < cy + sc.h) {
                if (ppCtrl) *ppCtrl = c;
                hit = 2;
            }
        });
    } catch (...) {
    }
    return hit ? hit : r;
}

}  // namespace

void AttachUiLayoutMod() {
    // push ebp / mov ebp,esp / mov eax,[CWndMan singleton]
    const unsigned char stock[] = {0x55, 0x8B, 0xEC, 0xA1, 0x0C, 0xC2, 0xBE, 0x00};
    if (std::memcmp(reinterpret_cast<const void*>(0x008DE7DA), stock, sizeof(stock)) != 0) {
        DebugMessage("UI layout: CUIStatusBar::HitTest has unexpected bytes; moved buttons click only near the bar");
        return;
    }
    if (!(ATTACH_HOOK(g_pBarHitTest, BarHitTest_hook))) DebugMessage("UI layout: could not hook CUIStatusBar::HitTest");
}

// From CWvsApp::CallUpdate (bypass.cpp), every frame: keeps the button groups where the player put them, also
// when the client rebuilds the status bar, and writes the QA state when it is switched on.
void UiLayout_Tick() {
    if (*reinterpret_cast<void* const*>(kAddr_UserLocal)) ApplyGroups();
    if (UiStateEnabled()) WriteUiState();
}

// From CConfig::LoadCharacter_hook (resolution.cpp).
void UiLayout_OnLoadCharacter(int nWorldID, unsigned int dwCharacterId) {
    g_nWorld = nWorldID;
    g_dwChar = dwCharacterId;
    g_nDragging = -1;
    for (int e = 0; e < kElemCount; ++e) {
        char name[48];
        ValueName(name, e);
        DWORD v = 0, cb = sizeof(v);
        g_off[e] = RegGetValueA(HKEY_CURRENT_USER, kStoreKey, name, RRF_RT_REG_DWORD, nullptr, &v, &cb) == ERROR_SUCCESS
            ? Unpack(v) : Offset{};
    }
    DebugMessage("UI layout: bar %d,%d quick slots %d,%d buttons %d,%d icons %d,%d for w%d_c%u",
        g_off[kStatusBar].fx, g_off[kStatusBar].fy, g_off[kQuickSlot].fx, g_off[kQuickSlot].fy,
        g_off[kMainButtons].fx, g_off[kMainButtons].fy, g_off[kIconRow].fx, g_off[kIconRow].fy, g_nWorld, g_dwChar);
    Reapply();
}

void UiLayout_Reset() {
    g_nDragging = -1;
    for (int e = 0; e < kElemCount; ++e) {
        g_off[e] = Offset{};
        Save(e);
    }
    Reapply();
}

#ifdef MAPLEROOT_INTEGRATION
// From overlay.cpp while the HUD is unfrozen: a grip over each element; dragging one moves it live.
void UiLayout_DrawGrips() {
    if (!*reinterpret_cast<void* const*>(kAddr_UserLocal)) return;   // the field only
    const int sw = get_screen_width(), sh = get_screen_height();
    ImDrawList* fg = ImGui::GetForegroundDrawList();

    // Every element's rect now, its stock rect and where it may go.
    struct Grip { bool ok; Rect home, bounds, r; int dx, dy; };
    Grip grips[kElemCount] = {};
    for (int e = 0; e < kElemCount; ++e) {
        Grip& g = grips[e];
        g.bounds = Rect{0, 0, sw, sh};
        if (e >= kFirstGroup) {
            g.ok = GroupHome(e, g.home, g.bounds);
            if (g.ok) GroupOffsetPx(e, g.dx, g.dy);
        } else {
            g.ok = HomeRect(e, g.home);
            if (g.ok) UiLayout_Offset(e, g.dx, g.dy);
        }
        g.r = Rect{g.home.left + g.dx, g.home.top + g.dy, g.home.w, g.home.h};
    }
    // The buttons and icon row sit inside the status bar's strip: decide who is under the cursor ourselves (smallest
    // first) and let only that grip take the mouse, instead of trusting ImGui's window order.
    static const int kPriority[kElemCount] = {kMainButtons, kIconRow, kChatInput, kQuickSlot, kStatusBar};
    int hot = g_nDragging;
    if (hot < 0) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        for (int e : kPriority) {
            const Rect& r = grips[e].r;
            if (grips[e].ok && m.x >= r.left && m.x < r.left + r.w && m.y >= r.top && m.y < r.top + r.h) {
                hot = e;
                break;
            }
        }
    }

    for (int e = 0; e < kElemCount; ++e) {
        Grip& g = grips[e];
        if (!g.ok) continue;
        const Rect& r = g.r;
        char id[32];
        sprintf_s(id, "##uigrip%d", e);
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(r.left), static_cast<float>(r.top)), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (e != hot) flags |= ImGuiWindowFlags_NoInputs;
        ImGui::Begin(id, nullptr, flags);
        ImGui::InvisibleButton("grip", ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)));
        if (e == hot && ImGui::IsItemActivated()) {
            g_nDragging = e;
            g_nDragDx = g.dx;
            g_nDragDy = g.dy;
        }
        if (ImGui::IsItemActive() && g_nDragging == e) {
            const ImVec2 d = ImGui::GetIO().MouseDelta;
            if (d.x != 0 || d.y != 0) {
                g_nDragDx += static_cast<int>(d.x);
                g_nDragDy += static_cast<int>(d.y);
                ClampWithin(g.home, g.bounds, g_nDragDx, g_nDragDy);
                Reapply();
            }
        }
        if (g_nDragging == e && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            g_off[e] = Offset{ToFrac(g_nDragDx, sw), ToFrac(g_nDragDy, sh)};
            if (Overlay_SnapOn()) g_off[e] = Offset{OverlayPolicy::Snap(g_off[e].fx), OverlayPolicy::Snap(g_off[e].fy)};
            g_nDragging = -1;
            Save(e);
            Reapply();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        const ImVec2 a(static_cast<float>(r.left), static_cast<float>(r.top));
        const ImVec2 b(static_cast<float>(r.left + r.w), static_cast<float>(r.top + r.h));
        // Idle: a faint 1 px outline, so the bar stays readable. Under the cursor: fill, bold outline and the name.
        if (e == hot) {
            fg->AddRectFilled(a, b, IM_COL32(90, 150, 255, 60));
            fg->AddRect(a, b, IM_COL32(120, 175, 255, 230), 0.0f, 0, 2.0f);
            fg->AddText(ImVec2(a.x + 6, a.y + 4), IM_COL32(255, 255, 255, 240), kLabel[e]);
        } else {
            fg->AddRect(a, b, IM_COL32(120, 175, 255, 110), 0.0f, 0, 1.0f);
        }
    }
}
#endif  // MAPLEROOT_INTEGRATION

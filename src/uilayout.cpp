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
#include "debug.h"
#include "constants.h"
#include "uilayout_policy.h"
#include "wvs/wndman.h"
#include "wvs/statusbar.h"

#ifdef MAPLEROOT_INTEGRATION
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
constexpr const char* kSuffix[kElemCount] = {"bar", "qs"};
constexpr const char* kLabel[kElemCount] = {"Status bar", "Quick slots"};

Offset g_off[kElemCount];
// Where each element sits at offset 0, relative to the bottom-left 800x600 origin (Origin_LB), learned from its
// layer while the grips are up; used to keep it on screen when ResetOrgWindow applies an offset. w == 0: unknown.
Rect g_home[kElemCount];
int g_nWorld = -1;
unsigned g_dwChar = 0;
int g_nDragging = -1;
int g_nDragDx = 0, g_nDragDy = 0;   // the dragged element's pixel offset

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

void Reapply() {
    if (CWndMan::IsInstantiated()) CWndMan::GetInstance()->ResetOrgWindow();
}

// The element's layer, or null (no status bar outside the field).
IWzGr2DLayer* LayerOf(int e) {
    if (!CUIStatusBar::IsInstantiated()) return nullptr;
    CUIStatusBar* bar = CUIStatusBar::GetInstance();
    if (e == kStatusBar) return bar->m_pLayer.GetInterfacePtr();
    return reinterpret_cast<IWzGr2DLayerPtr*>(reinterpret_cast<char*>(bar) + kOff_QuickSlotLayer)->GetInterfacePtr();
}

// The element's rect on screen now, from its layer (positions are relative to the screen's top-left origin).
bool ScreenRect(int e, Rect& r) {
    try {
        IWzGr2DLayer* layer = LayerOf(e);
        if (!layer || !CWndMan::IsInstantiated()) return false;
        IWzVector2DPtr org = CWndMan::GetInstance()->m_pOrgWindow;
        r.left = layer->x - org->x;
        r.top = layer->y - org->y;
        r.w = layer->Getwidth();
        r.h = layer->Getheight();
        if (e == kStatusBar) {
            r.top += r.h - kBarGripH;
            r.h = kBarGripH;
            r.w = (std::min)(r.w, get_screen_width() - r.left);
        }
        return r.w > 0 && r.h > 0;
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
    Rect home = g_home[e];
    home.top += sh - 600;   // stored relative to Origin_LB, which sits at (0, sh - 600)
    PixelOffset(g_off[e], home, sw, sh, dx, dy);
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
    DebugMessage("UI layout: bar %d,%d quick slots %d,%d for w%d_c%u", g_off[kStatusBar].fx, g_off[kStatusBar].fy,
        g_off[kQuickSlot].fx, g_off[kQuickSlot].fy, g_nWorld, g_dwChar);
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
    for (int e = 0; e < kElemCount; ++e) {
        Rect r;
        if (!ScreenRect(e, r)) continue;
        int dx = 0, dy = 0;
        UiLayout_Offset(e, dx, dy);
        g_home[e] = Rect{r.left - dx, r.top - dy - (sh - 600), r.w, r.h};

        char id[32];
        sprintf_s(id, "##uigrip%d", e);
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(r.left), static_cast<float>(r.top)), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::InvisibleButton("grip", ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)));
        const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (ImGui::IsItemActivated()) {
            g_nDragging = e;
            g_nDragDx = dx;
            g_nDragDy = dy;
        }
        if (ImGui::IsItemActive() && g_nDragging == e) {
            const ImVec2 d = ImGui::GetIO().MouseDelta;
            if (d.x != 0 || d.y != 0) {
                Rect home = g_home[e];
                home.top += sh - 600;
                g_nDragDx += static_cast<int>(d.x);
                g_nDragDy += static_cast<int>(d.y);
                Clamp(home, sw, sh, g_nDragDx, g_nDragDy);
                Reapply();
            }
        }
        if (ImGui::IsItemDeactivated() && g_nDragging == e) {
            g_off[e] = Offset{ToFrac(g_nDragDx, sw), ToFrac(g_nDragDy, sh)};
            g_nDragging = -1;
            Save(e);
            Reapply();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        const ImVec2 a(static_cast<float>(r.left), static_cast<float>(r.top));
        const ImVec2 b(static_cast<float>(r.left + r.w), static_cast<float>(r.top + r.h));
        if (hot) fg->AddRectFilled(a, b, IM_COL32(90, 150, 255, 60));   // idle: outline only, the bar stays readable
        fg->AddRect(a, b, IM_COL32(120, 175, 255, 230), 0.0f, 0, 2.0f);
        fg->AddText(ImVec2(a.x + 6, a.y + 4), IM_COL32(255, 255, 255, 240), kLabel[e]);
    }
}
#endif  // MAPLEROOT_INTEGRATION

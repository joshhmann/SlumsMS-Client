// ============================================================
// display.cpp: display toggles in the HUD window (ticket 120, MAPLEROOT_DISPLAY_TOGGLES).
//
// First toggle: hide other players' cash and set effects. CUser::LoadLayer (0x00941417, thiscall: bsUOL, bLeft,
// USERLAYER&, int*) loads every layer attached to a character; after it returns, a layer from Effect/ItemEff.img or
// Effect/SetEff.img on a character that is not this one goes into a small registry and gets `visible` from the
// toggle, so flipping it applies at once. The rules are display_policy.h (tested natively). Map back/tile/object,
// opacity and the animation cap follow (research note docs/research-119-120.md).
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "d3d9host.h"
#include "display_policy.h"
#include "wvs/avatar.h"
#include "ztl/ztl.h"

#include "imgui.h"

#include <cstring>
#include <cwchar>
#include <deque>

namespace {

constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;   // CUserLocal* (a CUser at offset 0)
constexpr uintptr_t kAddr_LoadLayer = 0x00941417;
constexpr unsigned char kLoadLayerStock[] = {0xB8, 0x73, 0xD7, 0xAD, 0x00, 0xE8, 0x77, 0xF7, 0x11, 0x00};
constexpr const char* kPrefKey = "Software\\SlumsMS\\Overlay";
constexpr size_t kMaxTracked = 512;                 // the oldest drop off; they stay as they were last set

Display::Prefs g_prefs;
bool g_loaded = false;
std::deque<IWzGr2DLayerPtr> g_othersCash;           // other characters' ItemEff/SetEff layers

void LoadPrefs() {
    if (g_loaded) return;
    g_loaded = true;
    DWORD v = 0, cb = sizeof(v);
    if (RegGetValueA(HKEY_CURRENT_USER, kPrefKey, "hide_others_cash", RRF_RT_REG_DWORD, nullptr, &v, &cb) == ERROR_SUCCESS)
        g_prefs.hideOthersCash = v != 0;
}

void SavePrefs() {
    DWORD v = g_prefs.hideOthersCash ? 1 : 0;
    RegSetKeyValueA(HKEY_CURRENT_USER, kPrefKey, "hide_others_cash", REG_DWORD, &v, sizeof(v));
}

void Apply(IWzGr2DLayerPtr& layer) {
    try {
        const Display::LayerState s = Display::Decide(Display::Category::CashEffect, Display::Owner::Others, g_prefs);
        layer->visible = s.visible ? 1 : 0;
    } catch (...) {
    }
}

bool IsCashEffect(const wchar_t* uol) {
    return uol && (!wcsncmp(uol, L"Effect/ItemEff.img", 18) || !wcsncmp(uol, L"Effect/SetEff.img", 17));
}

class CUser;
using LoadLayer_t = int(__thiscall*)(CUser*, Ztl_bstr_t, int, USERLAYER&, int*);
auto CUser_LoadLayer = reinterpret_cast<LoadLayer_t>(kAddr_LoadLayer);

int __fastcall CUser_LoadLayer_hook(CUser* pThis, void* /*edx*/, Ztl_bstr_t bsUOL, int bLeft, USERLAYER& l, int* pnRepeat) {
    const bool cash = IsCashEffect(bsUOL.GetBSTR());
    const int r = CUser_LoadLayer(pThis, bsUOL, bLeft, l, pnRepeat);
    if (r && cash && l.pLayer && pThis != *reinterpret_cast<CUser* const*>(kAddr_UserLocal)) {
        LoadPrefs();
        g_othersCash.push_back(l.pLayer);
        if (g_othersCash.size() > kMaxTracked) g_othersCash.pop_front();
        Apply(g_othersCash.back());
    }
    return r;
}

}  // namespace

// The "Display" rows of the HUD window (Ctrl+F10).
void Display_DrawSettings() {
    LoadPrefs();
    ImGui::SeparatorText("Display");
    if (ImGui::Checkbox("Hide other players' cash and set effects", &g_prefs.hideOthersCash)) {
        SavePrefs();
        for (auto& layer : g_othersCash) Apply(layer);
    }
}

void AttachDisplayMod() {
    if (memcmp(reinterpret_cast<const void*>(kAddr_LoadLayer), kLoadLayerStock, sizeof(kLoadLayerStock)) != 0) {
        DebugMessage("Display: CUser::LoadLayer is not the stock code; display toggles stay off");
        return;
    }
    ATTACH_HOOK(CUser_LoadLayer, CUser_LoadLayer_hook);
    DebugMessage("Display: toggles attached");
}

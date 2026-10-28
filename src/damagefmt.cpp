// ============================================================
// damagefmt.cpp: short damage numbers (ticket 119, MAPLEROOT_DAMAGE_FORMAT).
//
// CAnimationDisplayer::Effect_HP (0x00437D0F) formats the number with ZXString<char>::Format(&str, "%d", nDamage)
// (cdecl call at 0x00437DFB) and then picks each digit's sprite by c - '0'. The call is redirected here: with the
// switch on, numbers of 10,000 and up become "12.3K" / "1.23M" / "2.14B" (damagefmt_policy.h) written with ':', ';',
// '<', '=' so the stock lookup reaches sprites "10".."13", which tools/wz/add_damage_glyphs.py bakes into Custom.wz
// for every digit set. A missing glyph would make Effect_HP throw, so the glyphs are checked once and the stock "%d"
// is kept if any set lacks them. The switch is "Short damage numbers" in the HUD window (per PC, default off).
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "damagefmt_policy.h"
#include "ztl/ztl.h"

#include "imgui.h"

#include <cstring>

namespace {

constexpr uintptr_t kAddr_ZXStringFormat = 0x00445B4B;   // ZXString<char>::Format(ZXString*, const char*, ...)
constexpr const char* kPrefKey = "Software\\SlumsMS\\Overlay";

bool g_short = false;
bool g_loaded = false;
int g_glyphs = -1;   // -1 not checked, 0 missing, 1 present

using Format_t = void*(__cdecl*)(void*, const char*, ...);

bool GlyphsPresent() {
    if (g_glyphs >= 0) return g_glyphs == 1;
    g_glyphs = 1;
    static const wchar_t* kSets[] = {L"NoRed0", L"NoRed1", L"NoBlue0", L"NoBlue1", L"NoViolet0", L"NoViolet1", L"NoCri0", L"NoCri1"};
    for (const wchar_t* set : kSets) {
        for (int g = 10; g <= 13; ++g) {
            wchar_t path[64];
            swprintf_s(path, L"Effect/BasicEff.img/%s/%d", set, g);
            try {
                Ztl_variant_t v = get_rm()->GetObjectA(path);
                if (v.vt != VT_UNKNOWN && v.vt != VT_DISPATCH) g_glyphs = 0;
            } catch (...) {
                g_glyphs = 0;
            }
            if (!g_glyphs) {
                DebugMessage("Damage numbers: glyph %ls missing in Custom.wz; short numbers stay off", path);
                return false;
            }
        }
    }
    return true;
}

void LoadPrefs() {
    if (g_loaded) return;
    g_loaded = true;
    DWORD v = 0, cb = sizeof(v);
    if (RegGetValueA(HKEY_CURRENT_USER, kPrefKey, "damage_short", RRF_RT_REG_DWORD, nullptr, &v, &cb) == ERROR_SUCCESS) g_short = v != 0;
}

// Replaces the "%d" Format call inside Effect_HP (same cdecl signature for this call site: out, fmt, value).
void* __cdecl DamageFormat(void* out, const char* fmt, int damage) {
    const auto format = reinterpret_cast<Format_t>(kAddr_ZXStringFormat);
    LoadPrefs();
    if (!g_short || damage < 10000 || !GlyphsPresent()) return format(out, fmt, damage);
    char buf[32];
    DamageFmt::ShortDamage(damage, buf, sizeof(buf));
    DamageFmt::EncodeGlyphs(buf);
    return format(out, "%s", buf);
}

}  // namespace

// The "Damage numbers" row of the HUD window (Ctrl+F10).
void DamageFmt_DrawSettings() {
    LoadPrefs();
    if (ImGui::Checkbox("Short damage numbers (12.3K, 1.23M)", &g_short)) {
        DWORD v = g_short ? 1 : 0;
        RegSetKeyValueA(HKEY_CURRENT_USER, kPrefKey, "damage_short", REG_DWORD, &v, sizeof(v));
    }
}

void AttachDamageFormatMod() {
    const auto& site = DamageFmt::kFormatCall;
    if (memcmp(reinterpret_cast<const void*>(site.addr), site.stock, sizeof(site.stock)) != 0) {
        DebugMessage("Damage numbers: Effect_HP's format call is not the stock code; short numbers stay off");
        return;
    }
    PatchCall(site.addr, &DamageFormat);
    DebugMessage("Damage numbers: format call redirected");
}

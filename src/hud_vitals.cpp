// ============================================================
// hud_vitals.cpp: the HP, MP and EXP modules of the modular HUD (1.6.0, ticket 91).
//
// Reads exactly what the stock status bar shows. CUIStatusBar::Draw (0x008D80A4) passes these to SetNumberValue
// (0x008D850B), which MapleRoot already detours, so this reads them the way Draw does instead of stacking a hook:
//   HP / MP     GW_CharacterStat +0x61 / +0x71 through _ZtlSecureFuse<short> 0x004746DD, which hpmp.cpp detours to
//               return the whole 4-byte value (stored raw, checksum 0), so it is called as returning int
//   max HP / MP CWvsContext +0x211C / +0x2128, ZtlSecure<long> (checksum +8), _ZtlSecureFuse<long> 0x00416563
//   EXP         MapleRoot's 64-bit EXP and its EXP table (MapleRoot_CurrentExp / MapleRoot_NextLevelExp); the stock
//               bar gets a scaled-down copy that only fits its int gauge
// The character stat is GetCharacterData's ZRef payload, as in Draw (0x008D80C3).
// ============================================================

#include "pch.h"
#include "debug.h"
#include "mapleroot.h"
#include "d3d9host.h"
#include "overlay_policy.h"

#include "imgui.h"

namespace {

constexpr uintptr_t kAddr_CWvsContext = 0x00BE7918;
constexpr uintptr_t kAddr_GetCharacterData = 0x00425D0B;
constexpr uintptr_t kAddr_ZRefRelease = 0x00428C44;
constexpr uintptr_t kAddr_FuseShort = 0x004746DD;
constexpr uintptr_t kAddr_FuseLong = 0x00416563;
constexpr int kOff_HP = 0x61, kOff_MP = 0x71;               // in GW_CharacterStat
constexpr int kOff_MaxHP = 0x211C, kOff_MaxMP = 0x2128;     // in CWvsContext

using FuseShort_t = int(__cdecl*)(const void*, unsigned int);   // int: hpmp.cpp's detour (4-byte HP/MP)
using FuseLong_t = long(__cdecl*)(const long*, unsigned int);

struct Vitals {
    bool ok;
    long hp, maxHp, mp, maxMp;
};

// Stock entry bytes, or a detour's jmp: MapleRoot detours _ZtlSecureFuse<long> (its pass-through ztlfuse), and
// calling through a detour still reaches the function.
bool StockOrDetoured(uintptr_t addr, const unsigned char* stock, size_t n) {
    const auto* p = reinterpret_cast<const unsigned char*>(addr);
    return p[0] == 0xE9 || memcmp(p, stock, n) == 0;
}

// The entry bytes of the client functions called here (MapleStory.exe 86e4e6be...ac90); checked once.
bool ClientMatches() {
    static int s_state = -1;
    if (s_state < 0) {
        const unsigned char fuseShort[] = {0x55, 0x8B, 0xEC, 0x51, 0x51, 0x8B, 0x45, 0x08};
        const unsigned char fuseLong[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x53, 0x8B};
        const unsigned char getCd[] = {0x55, 0x8B, 0xEC, 0x51, 0x8B, 0x81, 0xB8, 0x20};
        s_state = StockOrDetoured(kAddr_FuseShort, fuseShort, sizeof(fuseShort)) &&
            StockOrDetoured(kAddr_FuseLong, fuseLong, sizeof(fuseLong)) &&
            StockOrDetoured(kAddr_GetCharacterData, getCd, sizeof(getCd));
        if (!s_state) DebugMessage("HUD vitals: unexpected client bytes; the widget stays empty");
    }
    return s_state == 1;
}

// SEH leaf: no unwinding objects in scope.
Vitals SehReadVitals() {
    Vitals v = {};
    __try {
        char* ctx = *reinterpret_cast<char**>(kAddr_CWvsContext);
        if (!ctx) return v;
        void* out[2] = {nullptr, nullptr};
        void* ret = reinterpret_cast<void*(__thiscall*)(void*, void**)>(kAddr_GetCharacterData)(ctx, out);
        char* cs = *reinterpret_cast<char**>(reinterpret_cast<char*>(ret) + 4);
        if (out[1]) {
            reinterpret_cast<void(__thiscall*)(void**, int)>(kAddr_ZRefRelease)(out, 0);
            out[1] = nullptr;
        }
        if (!cs) return v;   // CWvsContext holds the master reference, so the stat stays valid
        const auto fuseShort = reinterpret_cast<FuseShort_t>(kAddr_FuseShort);
        const auto fuseLong = reinterpret_cast<FuseLong_t>(kAddr_FuseLong);
        v.hp = fuseShort(cs + kOff_HP, *reinterpret_cast<unsigned*>(cs + kOff_HP + 4));
        v.mp = fuseShort(cs + kOff_MP, *reinterpret_cast<unsigned*>(cs + kOff_MP + 4));
        v.maxHp = fuseLong(reinterpret_cast<long*>(ctx + kOff_MaxHP), *reinterpret_cast<unsigned*>(ctx + kOff_MaxHP + 8));
        v.maxMp = fuseLong(reinterpret_cast<long*>(ctx + kOff_MaxMP), *reinterpret_cast<unsigned*>(ctx + kOff_MaxMP + 8));
        v.ok = v.maxHp > 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        v.ok = false;
    }
    return v;
}

// One module's bar: the whole width, the label inside ("HP 25000 / 50073 (49%)").
void ModuleBar(const char* label, long long value, long long max, bool twoDecimals, const ImVec4& color) {
    char numbers[96], text[112];
    OverlayPolicy::BarLabel(numbers, sizeof(numbers), value, max, twoDecimals);
    sprintf_s(text, "%s %s", label, numbers);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(OverlayPolicy::Ratio(value, max), ImVec2(-1.0f, 0.0f), text);
    ImGui::PopStyleColor();
}

// Read once per frame for all three modules.
const Vitals& FrameVitals() {
    static Vitals s_v = {};
    static int s_frame = -1;
    if (s_frame != ImGui::GetFrameCount()) {
        s_frame = ImGui::GetFrameCount();
        s_v = ClientMatches() ? SehReadVitals() : Vitals{};
    }
    return s_v;
}

bool Waiting(const Vitals& v) {
    if (v.ok) return false;
    ImGui::TextDisabled("Waiting for a character...");
    return true;
}

}  // namespace

void HudVitals_DrawHP() {
    const Vitals& v = FrameVitals();
    if (!Waiting(v)) ModuleBar("HP", v.hp, v.maxHp, false, ImVec4(0.86f, 0.22f, 0.22f, 1.0f));
}

void HudVitals_DrawMP() {
    const Vitals& v = FrameVitals();
    if (!Waiting(v)) ModuleBar("MP", v.mp, v.maxMp, false, ImVec4(0.22f, 0.45f, 0.90f, 1.0f));
}

void HudVitals_DrawEXP() {
    const Vitals& v = FrameVitals();
    if (!Waiting(v)) {
        ModuleBar("EXP", MapleRoot_CurrentExp(), MapleRoot_NextLevelExp(), true, ImVec4(0.93f, 0.74f, 0.18f, 1.0f));
    }
}

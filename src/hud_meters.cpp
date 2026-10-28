// ============================================================
// hud_meters.cpp: the EXP-rate and DPS modules of the modular HUD (ticket 118, MAPLEROOT_HUD_METERS).
//
// EXP: MapleRoot's 64-bit EXP and its EXP table each frame (as hud_vitals.cpp). A level-up shows as the table value
// changing while the EXP drops, so the tracker counts the rest of the old level plus the new EXP.
// DPS: CMob::OnHit (0x00668B83, thiscall, 13 stack args, ret 0x34) runs once per damage number the client draws; a1
// is the attacker's character id (compared with CUserLocal+0x11A8 at 0x00668BF5), a2 the skill id, a5 the damage.
// Only hits whose attacker is this character count. The maths is hud_meters_policy.h (tested natively).
// Party ranks: the server sends LP 0x3739 about once a second to party members hitting a boss (or a mob with 5M+ HP);
// the module lists them under this character's own DPS for 15 s after the last one. Solo, it shows this character only.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "mapleroot.h"
#include "d3d9host.h"
#include "hud_meters_policy.h"
#include "wvs/packet.h"

#include "imgui.h"

#include <cstring>
#include <mutex>

namespace {

constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;   // CUserLocal*: set only while a character is in the field
constexpr int kOff_CharacterId = 0x11A8;            // CUserLocal: this character's id (0x00668BF5)

HudMeters::ExpTracker g_exp;
HudMeters::DpsMeter g_dps;
uint32_t g_lastHitMs = 0;
int g_levelCounter = 0;                // stands in for the level: +1 when the EXP table moves on and the EXP drops
long long g_lastExp = -1, g_lastNext = -1;
std::mutex g_rankLock;
HudMeters::DamageRank g_rank;          // the latest party rank (LP 0x3739)
uint32_t g_rankMs = 0;                 // when it came; 0 = none yet
constexpr uint32_t kRankShowMs = 15000;

int MyCharacterId() {
    __try {
        char* user = *reinterpret_cast<char**>(kAddr_UserLocal);
        return user ? *reinterpret_cast<int*>(user + kOff_CharacterId) : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

using OnHit_t = int(__thiscall*)(void*, int, int, int, int, int, int, int, int, int, int, int, int, int);
auto CMob_OnHit = reinterpret_cast<OnHit_t>(HudMeters::kAddr_CMob_OnHit);

int __fastcall CMob_OnHit_hook(void* pThis, void* /*edx*/, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
    int a9, int a10, int a11, int a12, int a13) {
    if (a5 > 0 && a1 != 0 && a1 == MyCharacterId()) {
        g_lastHitMs = GetTickCount();
        g_dps.Hit(g_lastHitMs, a2, a5);
    }
    return CMob_OnHit(pThis, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
}

// 12345 -> "12.3K" (as the damage numbers will read, ticket 119), for the HUD's own text.
void Short(char* out, size_t cap, long long v) {
    if (v < 10000) { sprintf_s(out, cap, "%lld", v); return; }
    const char* sfx = "K";
    double d = static_cast<double>(v) / 1e3;
    if (v >= 1000000000LL) { sfx = "B"; d = static_cast<double>(v) / 1e9; }
    else if (v >= 1000000LL) { sfx = "M"; d = static_cast<double>(v) / 1e6; }
    sprintf_s(out, cap, d >= 100 ? "%.0f%s" : d >= 10 ? "%.1f%s" : "%.2f%s", d, sfx);
}

void Duration(char* out, size_t cap, long long s) {
    if (s < 0) { strcpy_s(out, cap, "--"); return; }
    if (s >= 3600) sprintf_s(out, cap, "%lldh %02lldm", s / 3600, (s / 60) % 60);
    else sprintf_s(out, cap, "%lldm %02llds", s / 60, s % 60);
}

}  // namespace

void HudMeters_DrawExpRate() {
    const long long exp = MapleRoot_CurrentExp(), next = MapleRoot_NextLevelExp();
    if (next <= 0) {
        ImGui::TextDisabled("Waiting for a character...");
        return;
    }
    if (g_lastNext >= 0 && next != g_lastNext && exp < g_lastExp) ++g_levelCounter;   // levelled up
    g_lastExp = exp;
    g_lastNext = next;
    g_exp.Sample(GetTickCount(), g_levelCounter, exp, next);
    char rate[32], session[32], eta[32];
    Short(rate, sizeof(rate), g_exp.PerHour());
    Short(session, sizeof(session), g_exp.Session());
    Duration(eta, sizeof(eta), g_exp.SecondsToLevel());
    if (g_exp.PerHour() <= 0) ImGui::Text("EXP/h: measuring...   session +%s", session);
    else ImGui::Text("EXP/h %s   level in %s   session +%s", rate, eta, session);
}

void HudMeters_DrawDps() {
    const uint32_t now = GetTickCount();
    if (g_lastHitMs && now - g_lastHitMs > 60000) {   // a minute without hitting: start a fresh fight
        g_dps.Reset();
        g_lastHitMs = 0;
    }
    char dps[32], total[32];
    Short(dps, sizeof(dps), g_dps.Dps(now));
    Short(total, sizeof(total), g_dps.Total());
    if (!g_dps.Total()) ImGui::TextDisabled("DPS: hit something");
    else ImGui::Text("DPS %s   fight total %s", dps, total);

    HudMeters::DamageRank rank;
    {
        std::lock_guard<std::mutex> lock(g_rankLock);
        if (!g_rankMs || now - g_rankMs > kRankShowMs) return;
        rank = g_rank;
    }
    const int me = MyCharacterId();
    for (size_t i = 0; i < rank.rows.size(); ++i) {
        const auto& row = rank.rows[i];
        char dmg[32];
        Short(dmg, sizeof(dmg), row.damage);
        const int pct = HudMeters::SharePct(rank, i);
        if (row.charId == me) ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.3f, 1.0f), "%zu. %s  %s  %d%%", i + 1, row.name.c_str(), dmg, pct);
        else ImGui::Text("%zu. %s  %s  %d%%", i + 1, row.name.c_str(), dmg, pct);
    }
}

// LP 0x3739 (packetdispatcher.cpp). The dispatcher only peeked the opcode, so the body starts 2 bytes in.
void HudMeters_HandleDamageRank(CInPacket* pPacket) {
    const unsigned char* p = pPacket ? pPacket->CurrentPublic() : nullptr;
    if (!p) return;
    const size_t left = pPacket->LengthPublic() - pPacket->GetOffset();
    if (left < 2) return;
    HudMeters::DamageRank r;
    if (!HudMeters::ParseDamageRank(p + 2, left - 2, r)) {
        DebugMessage("HUD meters: damage rank packet did not parse (%zu bytes)", left);
        return;
    }
    std::lock_guard<std::mutex> lock(g_rankLock);
    g_rank = std::move(r);
    g_rankMs = GetTickCount();
    if (!g_rankMs) g_rankMs = 1;
}

void AttachHudMetersMod() {
    if (memcmp(reinterpret_cast<const void*>(HudMeters::kAddr_CMob_OnHit), HudMeters::kOnHitStock,
            sizeof(HudMeters::kOnHitStock)) != 0) {
        DebugMessage("HUD meters: CMob::OnHit is not the stock code; the DPS module stays empty");
        return;
    }
    ATTACH_HOOK(CMob_OnHit, CMob_OnHit_hook);
    DebugMessage("HUD meters: DPS feed attached");
}

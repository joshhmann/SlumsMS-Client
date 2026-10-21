// ============================================================
// setstatbonus.cpp — SetEquip addon (ticket 18): show the server's set bonus in the client's own stat totals.
//
// Ported from the addon's setstatbonus.cpp (kaentakedev, same v83 client.exe as Kaentake; every address below was
// re-checked by disassembling the standalone client's MapleStory.exe, sha256 86e4e6be...ac90).
//
// The server folds set bonuses into the character (Character#recalcEquipStats). The v83 client recomputes its
// "equip-inclusive" STR/DEX/INT/LUK/MaxHP/MaxMP itself from each worn item (sub_77EC9F, called by
// CWvsContext::ValidateStat at 0x00A0858F with ecx = CWvsContext + 0x20BC), so a bonus attached to no item is
// invisible to it. The server sends the folded bonus in every pieces packet (setitem.cpp keeps it):
//
// Part 1, STR/DEX/INT/LUK: after the real ValidateStat, decode each cached total on the CWvsContext singleton,
//   add the bonus and re-encode (ZtlSecure<long>: data, then checksum 8 bytes later), then CUIStat::ResetInfo
//   as ValidateStat itself does at 0x00A085BF. Nothing after sub_77EC9F's item loops changes these four.
//     STR +0x20E0/+0x20E8  DEX +0x20EC/+0x20F4  INT +0x20F8/+0x2100  LUK +0x2104/+0x210C
//
// Part 2, MaxHP/MaxMP: sub_77EC9F applies the Hyper Body percentage after the item loops
//   (0x0077F14A: call Fuse<long>(esi+0x60) -> imul (100 + pct) -> idiv 100 -> Tear; MaxMP at 0x0077F173), then
//   clamps to 30000. The server adds the set HP before Hyper Body, so the bonus is added to the value those two
//   Fuse calls return (call-site redirects, not a hook on Fuse itself), which keeps the client equal to the server
//   with and without Hyper Body. The HUD bar reads GW_CharacterStat directly and is untouched.
//
// Weapon/magic attack are applied by the server only (bots, summons and the damage ceiling); they do not change
// the client's own damage range.
// ============================================================
#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "wvs/secure.h"
#include "wvs/field.h"
#include "setitem.h"

#include <cstdint>

namespace {

class CWvsContext;
class CUIStat;

constexpr uintptr_t kAddr_ValidateStat       = 0x00A0843C;  // CWvsContext::ValidateStat(void), thiscall, plain ret
constexpr uintptr_t kAddr_CUIStat_ResetInfo  = 0x008C59D4;  // CUIStat::ResetInfo(void)
constexpr uintptr_t kAddr_CWvsContext        = 0x00BE7918;  // CWvsContext* singleton
constexpr uintptr_t kAddr_CUIStat            = 0x00BF10C4;  // CUIStat* singleton (null until first opened)

constexpr uintptr_t kStrData = 0x20E0, kStrCs = 0x20E8;
constexpr uintptr_t kDexData = 0x20EC, kDexCs = 0x20F4;
constexpr uintptr_t kIntData = 0x20F8, kIntCs = 0x2100;
constexpr uintptr_t kLukData = 0x2104, kLukCs = 0x210C;

auto CWvsContext__ValidateStat = reinterpret_cast<void(__thiscall*)(CWvsContext*)>(kAddr_ValidateStat);
auto CUIStat__ResetInfo = reinterpret_cast<void(__thiscall*)(CUIStat*)>(kAddr_CUIStat_ResetInfo);

void AddToCachedTotal(uint8_t* base, uintptr_t dataOffset, uintptr_t csOffset, int bonus) {
    if (bonus == 0) return;
    long* at = reinterpret_cast<long*>(base + dataOffset);
    unsigned int& cs = *reinterpret_cast<unsigned int*>(base + csOffset);
    long current = ZtlSecureFuse<long>(at, cs);
    cs = ZtlSecureTear<long>(at, current + bonus);
}

void __fastcall CWvsContext__ValidateStat_hook(CWvsContext* pThis, void* /*edx*/) {
    CWvsContext__ValidateStat(pThis);   // Part 2 runs inside, through the redirected Fuse calls
    const setitem::Bonus b = SetItem_GetBonus();
    if (b.str == 0 && b.dex == 0 && b.int_ == 0 && b.luk == 0) return;
    auto base = reinterpret_cast<uint8_t*>(pThis);
    AddToCachedTotal(base, kStrData, kStrCs, b.str);
    AddToCachedTotal(base, kDexData, kDexCs, b.dex);
    AddToCachedTotal(base, kIntData, kIntCs, b.int_);
    AddToCachedTotal(base, kLukData, kLukCs, b.luk);
    if (void* stat = *reinterpret_cast<void**>(kAddr_CUIStat)) {
        CUIStat__ResetInfo(reinterpret_cast<CUIStat*>(stat));
    }
}

// _ZtlSecureFuse<long>(const long*, unsigned) is __cdecl (both sites: push cs; push ptr; call; pop ecx; pop ecx).
using t_Fuse_long = long(__cdecl*)(const long*, unsigned int);
constexpr uintptr_t kAddr_Fuse_long = 0x00416563;
constexpr uintptr_t kAddr_MaxHpPctFuseSite = 0x0077F14A;
constexpr uintptr_t kAddr_MaxMpPctFuseSite = 0x0077F173;
auto Fuse_long = reinterpret_cast<t_Fuse_long>(kAddr_Fuse_long);

long __cdecl MaxHpPctFuse_hook(const long* at, unsigned int cs) {
    return Fuse_long(at, cs) + SetItem_GetBonus().maxHp;
}

long __cdecl MaxMpPctFuse_hook(const long* at, unsigned int cs) {
    return Fuse_long(at, cs) + SetItem_GetBonus().maxMp;
}

// The exact bytes at each call site, so a different client build is refused instead of corrupted.
bool IsFuseCall(uintptr_t site) {
    auto p = reinterpret_cast<const unsigned char*>(site);
    if (p[0] != 0xE8) return false;
    int32_t rel;
    memcpy(&rel, p + 1, sizeof(rel));
    return site + 5 + rel == kAddr_Fuse_long;
}

}  // namespace

void SetStatBonus_Revalidate() {
    // Main thread only (SetItem_Tick); in a field the character data ValidateStat reads exists.
    if (!get_field()) return;
    auto ctx = *reinterpret_cast<CWvsContext**>(kAddr_CWvsContext);
    if (ctx) CWvsContext__ValidateStat_hook(ctx, nullptr);
}

void AttachSetStatBonusMod() {
    if (!IsFuseCall(kAddr_MaxHpPctFuseSite) || !IsFuseCall(kAddr_MaxMpPctFuseSite)) {
        DebugMessage("setitem: MaxHP/MaxMP Fuse call sites do not match this client; set HP/MP not shown");
    } else {
        PatchCall(kAddr_MaxHpPctFuseSite, &MaxHpPctFuse_hook);
        PatchCall(kAddr_MaxMpPctFuseSite, &MaxMpPctFuse_hook);
    }
    ATTACH_HOOK(CWvsContext__ValidateStat, CWvsContext__ValidateStat_hook);
}

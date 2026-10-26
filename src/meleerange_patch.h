#pragma once
// ============================================================
// meleerange_patch.h -- another character's melee attack in a pose its weapon has no range for no longer closes
// the client (1.6.0).
//
// Header-only so tests/meleerange_patch_test.cpp can check the bytes without the client; meleerange.cpp installs it.
//
// THE CRASH. CUserRemote::OnMeleeAttack (0x00980C90) gets the attack's range from the skill when the skill has one,
// else from the weapon's afterimage: CActionMan::GetMeleeAttackRange(Ztl_bstr_t sAfterImage, int nAction, RECT&)
// 0x00414440. That looks up the afterimage (GetWeaponAfterImage 0x0040AC78: a ZArray of 0xA2 action slots of 0x30
// bytes, only the first 0xA20 bytes = 54 slots zeroed, the rest left as allocated) and reads the slot for nAction
// as a ZtlSecure rect (0x004144D4, two _ZtlSecureFuse<long> 0x00416563 per corner). A pose the afterimage has no
// data for is a zeroed or uninitialised slot, its checksum never matches, and the Fuse throws a ZException out of
// packet processing: the session ends (SESSION_CLEANUP, no crash dialog). A real player's client never sends such
// a pose for its weapon; a server bot can (seen on TEST 2026-10-26: QA rm-2's staged partner, twice, crash.log
// stack 0x004165AA <- 0x004144E3 <- 0x00980DD2 <- CUserRemote::OnAttack).
//
// THE FIX. GetMeleeAttackRange is detoured: the stock function runs inside __try, and only the client's own C++
// throw (0xE06D7363) is caught. The stock frame's unwind releases the afterimage name as usual; the rect comes back
// empty, so the attack is drawn without an afterimage range instead of closing the client. Every other path is
// untouched: a slot with data fuses exactly as before. The entry bytes are checked first; on a mismatch nothing is
// hooked.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MeleeRange {

constexpr std::uint32_t kAddr_GetMeleeAttackRange = 0x00414440;   // __thiscall, ret 0xC
constexpr std::uint32_t kAddr_EhProlog            = 0x00A60B98;   // the client's _EH_prolog
constexpr std::uint32_t kAddr_OnMeleeAttackCall   = 0x00980DCD;   // CUserRemote::OnMeleeAttack: call 0x00414440
constexpr std::uint32_t kCxxException             = 0xE06D7363;   // MSVC throw (ZException)
constexpr std::size_t kEntryLength = 10;

inline const std::array<unsigned char, kEntryLength>& OriginalBytes() {
    static const std::array<unsigned char, kEntryLength> bytes = {{
        0xB8, 0x3C, 0x78, 0xA7, 0x00,       // mov eax, 0x00A7783C   (the frame's EH table)
        0xE8, 0x4E, 0xC7, 0x64, 0x00,       // call 0x00A60B98      (_EH_prolog)
    }};
    return bytes;
}

enum class SiteState { Original, Detoured, Unknown };

// Detoured: a jmp at the entry (this mod after a restart of the hook, or another detour we would chain onto).
inline SiteState Classify(const unsigned char* current) {
    if (std::memcmp(current, OriginalBytes().data(), kEntryLength) == 0) return SiteState::Original;
    if (current[0] == 0xE9) return SiteState::Detoured;
    return SiteState::Unknown;
}

}  // namespace MeleeRange

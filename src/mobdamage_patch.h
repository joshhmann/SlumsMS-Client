#pragma once
// ============================================================
// mobdamage_patch.h -- low defence never adds damage when a mob touches you (1.6.0).
//
// Header-only so tests/mobdamage_patch_test.cpp can check the bytes without the client; mobdamage.cpp installs it.
//
// THE FORMULA. Mob physical damage to the player is computed by the client (CalcDamage mob PDamage, 0x0079309F,
// named in Angel.idb) and the server applies what it is sent. It reads the job's standard defence for your level
// from StandardPDD.img (0x00792F75: table at 0x00BEDF58, 6 job classes x levels 0..200, filled at 0x00792D21 and
// carried forward past the last entry), then branches at 0x0079328D:
//   PDD >= std: reduction = (s/900 + lv/1300 + 0.28) * (PDD - std) * 0.7 + (s*0.00125 + 0.28) * PDD
//   PDD <  std: reduction = (s*0.00125 + lv/550 + 0.28) * (PDD - std) * {1.3, or 13/(lvdiff+13)} + (...) * PDD
// where s is a weighted sum of STR/DEX/INT/LUK. Below the standard, (PDD - std) is negative, so the "reduction"
// turns into extra damage, scaled by the stats: a level 200 admin with 32767 stats and 7 defence took ~900 from a
// snail (PADamage 12) after !job moved them from job 900 (class 9, no table row, std 0) into a real class.
//
// THE FIX. At 0x0079328D the cmp / mov / jl becomes a jmp to a cave that does the mov and, when PDD < std, sets
// std = PDD before taking the PDD >= std branch. That branch then reduces by (s*0.00125 + 0.28) * PDD, which is
// never negative. Characters at or above their standard are untouched. Every byte around the site is checked
// first; on any mismatch nothing is patched. config.ini [clienthax] lowdefensepenalty=true keeps the stock rule.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MobDamage {

constexpr std::uint32_t kAddr_Function   = 0x0079309F;   // CalcDamage mob physical damage, ret 0x1C
constexpr std::uint32_t kAddr_StdLookup  = 0x00792F75;   // (jobClass, level) -> standard PDD, 0 outside the table
constexpr std::uint32_t kAddr_StdCall    = 0x007931B4;   // call kAddr_StdLookup; result -> [ebp+0x1C]
constexpr std::uint32_t kAddr_PddStore   = 0x007931A8;   // mov [ebp+0x18], edi  (edi = the character's PDD)
constexpr std::uint32_t kAddr_Site       = 0x0079328D;   // cmp edi,[ebp+0x1C] / mov [ebp+0x10],eax / jl below
constexpr std::uint32_t kAddr_Resume     = 0x00793295;   // the PDD >= std branch (the jl's fall-through)
constexpr std::uint32_t kAddr_BelowStd   = 0x007932F3;   // the jl's target: the two below-standard branches
constexpr std::size_t kSiteLength = 8;

inline const std::array<unsigned char, kSiteLength>& OriginalBytes() {
    static const std::array<unsigned char, kSiteLength> bytes = {{
        0x3B, 0x7D, 0x1C,          // cmp edi, [ebp+0x1C]
        0x89, 0x45, 0x10,          // mov [ebp+0x10], eax   (the stat term)
        0x7C, 0x5E,                // jl 0x007932F3
    }};
    return bytes;
}

// jmp rel32 to the cave, then 3 nops that are never reached.
inline std::array<unsigned char, kSiteLength> PatchedBytes(std::uint32_t uCave) {
    const std::uint32_t rel = uCave - (kAddr_Site + 5);
    return {{0xE9, static_cast<unsigned char>(rel), static_cast<unsigned char>(rel >> 8),
             static_cast<unsigned char>(rel >> 16), static_cast<unsigned char>(rel >> 24), 0x90, 0x90, 0x90}};
}

enum class SiteState { Original, Patched, Unknown };

inline SiteState Classify(const unsigned char* current) {
    if (std::memcmp(current, OriginalBytes().data(), kSiteLength) == 0) return SiteState::Original;
    if (current[0] == 0xE9 && current[5] == 0x90 && current[6] == 0x90 && current[7] == 0x90) return SiteState::Patched;
    return SiteState::Unknown;
}

// The defence term of the formula above, as a reduction (positive = less damage). With the fix, a defence below
// the standard counts as if the standard were the defence itself.
inline double Reduction(int statTerm, int level, int mobLevel, int pdd, int std, bool fixed) {
    if (fixed && pdd < std) std = pdd;
    const double b = statTerm * 0.00125 + 0.28;
    if (pdd >= std) return (statTerm / 900.0 + level / 1300.0 + 0.28) * (pdd - std) * 0.7 + b * pdd;
    const double a = statTerm * 0.00125 + level / 550.0 + 0.28;
    const double k = level < mobLevel ? 1.3 : 13.0 / (level - mobLevel + 13);
    return a * (pdd - std) * k + b * pdd;
}

}  // namespace MobDamage

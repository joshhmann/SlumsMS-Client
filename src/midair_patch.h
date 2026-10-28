#pragma once
// ============================================================
// midair_patch.h -- casting in the air (ticket 126). SCAFFOLD.
//
// No Windows, no client: tests/midair_patch_test.cpp checks it. MapleRoot already lifts two in-air gates from
// config.ini (YourStuff.h:643-653, both default true, shown in the launcher): [clienthax] jumpshoot (bows and
// crossbows) and jumpmagic (magic attacks). Still stock: starting a keydown attack (Hurricane, Piercing Arrow,
// Big Bang), Teleport and summons. This header names those gates (bytes verified against MapleStory.exe
// 86e4e6be...) and the per-skill policy for a future [clienthax] jumpkeydown; nothing patches them yet
// (research note: docs/research-126-midair.md).
// ============================================================

#include <array>
#include <cstdint>

namespace MidAir {

struct Site {
    std::uint32_t addr;
    std::array<std::uint8_t, 5> stock;   // the first five bytes there
    std::uint32_t allowTarget;           // where "allowed in the air" continues
    const char* what;
};

// Existing MapleRoot writes (E9 to allowTarget) and the new keydown gate.
inline constexpr Site kShoot   {0x009539FA, {0x75, 0x0F, 0x8B, 0xCE, 0xE8}, 0x00953A11, "TryDoingShootAttack foothold gate"};
inline constexpr Site kMagic   {0x009559E5, {0x75, 0x2E, 0x8B, 0x45, 0xEC}, 0x00955A20, "TryDoingMagicAttack foothold gate"};
inline constexpr Site kKeydown {0x0096A942, {0x83, 0xBF, 0x10, 0x01, 0x00}, 0x0096A968, "DoActiveSkill_Prepare foothold gate (cmp [edi+0x110],0 / jne)"};

// E9 rel32 from `site` to `target`.
inline constexpr std::array<std::uint8_t, 5> JmpTo(std::uint32_t site, std::uint32_t target) {
    const std::uint32_t rel = target - (site + 5);
    return {0xE9, static_cast<std::uint8_t>(rel), static_cast<std::uint8_t>(rel >> 8), static_cast<std::uint8_t>(rel >> 16),
        static_cast<std::uint8_t>(rel >> 24)};
}

enum class AirCast { Allow, Stock };

// v1 for TEST (owner 2026-10-28): the "not on a foothold -> refuse" exit of the keydown start, `je 0x0096AEAE` at
// 0x0096A954 (after `call 0x007046AC; test eax,eax`), NOPed when [clienthax] jumpkeydown=true. The rope/ladder
// test that follows (0x0096A968) still refuses. It covers every keydown skill; the per-skill list below is the plan
// for after TEST.
inline constexpr std::uint32_t kKeydownAirExit = 0x0096A954;
inline constexpr std::array<std::uint8_t, 6> kKeydownAirExitStock = {0x0F, 0x84, 0x54, 0x05, 0x00, 0x00};

// Keydown attacks that may start in the air once jumpkeydown is on: Hurricane, Piercing Arrow, Wind Archer's
// Hurricane, the three Big Bangs.
inline constexpr std::array<int, 6> kAirKeydown = {3121004, 3221001, 13111002, 2121001, 2221001, 2321001};

inline constexpr bool IsExplorerOrCygnusMageArcher(int job) {
    return (job >= 200 && job <= 232) || (job >= 300 && job <= 322) || (job >= 1200 && job <= 1212) ||
        (job >= 1300 && job <= 1312);
}

// Everything not listed stays stock: Rapid Fire, Monster Magnet, summons and Teleport are not part of this ticket.
inline constexpr AirCast KeydownPolicy(int skillId, int job, bool jumpkeydown) {
    if (!jumpkeydown || !IsExplorerOrCygnusMageArcher(job)) return AirCast::Stock;
    for (int s : kAirKeydown) {
        if (s == skillId) return AirCast::Allow;
    }
    return AirCast::Stock;
}

}  // namespace MidAir

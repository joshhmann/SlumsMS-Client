#pragma once
// ============================================================
// evan_patch.h -- let the v83 client use Evan skills (MAPLEROOT_EVAN, ticket 128 / Evan research).
//
// No Windows, no client: tests/evan_patch_test.cpp checks it. The v83 exe already has Evan (job names Evan0-10, per-
// tier SP, the Mir dragon on 0xB5/0xB6, the SkillEx window) but two skill paths refuse any skill whose job is 22xx or
// 2001. Both checks are `(skill / 10000) / 100 == 22` and `job == 2001` (0x7D1); NOPing them is the community fix
// (RageZone "Evan for v83"), checked here byte for byte against MapleStory.exe 86e4e6be...
// Needs the Evan data (Skill 2001/2200-2218 + Skill/Dragon, the dragon, body actions, String, UI SkillEx) and a
// server on the real Evan ids (not MapleRoot's 9900-9918 remap). Research note: docs/research-evan.md.
// ============================================================

#include <cstddef>
#include <cstdint>

namespace EvanPatch {

struct Site {
    std::uintptr_t addr;
    const unsigned char* stock;
    std::size_t size;
    const char* what;
};

// 0x0075C783: `xor eax,eax / jmp 0x75C7B5` -- the early "not usable" return after the two checks above it.
inline constexpr unsigned char kGate1[] = {0x33, 0xC0, 0xEB, 0x2E};
// 0x00761714: `cmp eax,0x16 / je +d7 / cmp esi,0x7d1 / je +cb` -- both checks, jumping past the skill.
inline constexpr unsigned char kGate2[] = {0x83, 0xF8, 0x16, 0x0F, 0x84, 0xD7, 0x00, 0x00, 0x00,
                                           0x81, 0xFE, 0xD1, 0x07, 0x00, 0x00, 0x0F, 0x84, 0xCB, 0x00, 0x00, 0x00};

inline constexpr Site kSites[] = {
    {0x0075C783, kGate1, sizeof(kGate1), "skill gate 1: early return for Evan skills"},
    {0x00761714, kGate2, sizeof(kGate2), "skill gate 2: Evan job / 2001 checks"},
};

// The exe's Evan action names (StringPool 5490-5507, around "magicmissile") are an older list than the Evan skill
// data (v84, reworked for v83), so some actions in the data have no code and can't play. Nine slots hold names no
// Evan skill uses; renaming them gives those codes the data's names (the body frames are merged in from the same
// data). The two *_prepare slots, and the names the data does use, stay. Still without a slot: Awakening,
// OnixBlessing, darkFog, ghostLettering, mapleHero, slow, soulStone.
struct ActionRename {
    int index;
    const char* stock;
    const char* evan;
};

inline constexpr ActionRename kActionRenames[] = {
    {5491, "dragonSpark", "lightingBolt"},
    {5496, "infinityExplosion", "Earthquake"},
    {5497, "superMagicmissile", "fireCircle"},
    {5501, "dragonShield", "magicShield"},
    {5502, "dragonFury", "dragonThrust"},
    {5503, "dragonFly", "killingWing"},
    {5505, "elementalRegistance", "magicRegistance"},
    {5506, "dragonAura", "recoveryAura"},
    {5507, "dragonSkin", "flameWheel"},
};

}  // namespace EvanPatch

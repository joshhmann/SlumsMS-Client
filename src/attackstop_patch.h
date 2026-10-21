#pragma once

// ============================================================
// attackstop_patch.h -- MapleRoot's [clienthax] moveattack sites.
//
// MapleRoot's ijl15 (mapleroot/ijl15/YourStuff.h, doini) applies three byte
// patches when config.ini [clienthax] moveattack is true (its default):
//
//   0x009CBFB0 CVecCtrlUser::WorkUpdateActive  7E (jle) -> EB (jmp)
//       Vanilla only reads VK_LEFT/VK_RIGHT when the avatar has no one-time
//       action (GetOneTimeAction() <= -1). While an attack animation plays the
//       walk input stays zero, so ground friction stops the character. The jmp
//       always reads the keys, so holding a direction keeps walking (and
//       accelerating) through the attack: the "momentum" slide.
//   0x0094C3BB CUserLocal::Jump  0F 8F 12 01 00 00 (jg ret) -> 6x NOP
//       Vanilla refuses to jump during a one-time action; NOP allows it.
//   0x0095F97A keydown-skill movable check (called by CUserLocal::IsImmovable
//       at 0x0095F914)  7F 2C (jg) -> EB 59 (jmp to "return 1"), so every
//       keydown skill is treated as movable.
//
// Vanilla attack stop = leave (or put back) the original bytes at all three.
// This header is pure data + decisions so it can be tested natively.
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace integration {

struct AttackStopSite {
    std::uint32_t address;
    std::size_t length;
    std::array<unsigned char, 6> original;   // vanilla v83 bytes
    std::array<unsigned char, 6> moveAttack; // bytes MapleRoot's moveattack writes
    const char* name;
};

inline const std::array<AttackStopSite, 3>& AttackStopSites() {
    static const std::array<AttackStopSite, 3> sites = {{
        {0x0095F97A, 2, {{0x7F, 0x2C}}, {{0xEB, 0x59}}, "keydown skill movable check"},
        {0x009CBFB0, 1, {{0x7E}}, {{0xEB}}, "CVecCtrlUser::WorkUpdateActive walk input gate"},
        {0x0094C3BB, 6, {{0x0F, 0x8F, 0x12, 0x01, 0x00, 0x00}},
                        {{0x90, 0x90, 0x90, 0x90, 0x90, 0x90}}, "CUserLocal::Jump one-time-action gate"},
    }};
    return sites;
}

enum class AttackStopState { Original, MoveAttack, Unknown };

inline AttackStopState ClassifyAttackStopSite(const AttackStopSite& site, const unsigned char* current) {
    if (std::memcmp(current, site.original.data(), site.length) == 0) return AttackStopState::Original;
    if (std::memcmp(current, site.moveAttack.data(), site.length) == 0) return AttackStopState::MoveAttack;
    return AttackStopState::Unknown;
}

// Default for config.ini [clienthax] vanillaattackstop when the key is absent.
#ifdef MAPLEROOT_VANILLA_ATTACK_STOP
constexpr bool kVanillaAttackStopDefault = true;
#else
constexpr bool kVanillaAttackStopDefault = false;
#endif

// moveattack is applied only when asked for and vanilla attack stop is off.
// vanillaattackstop wins so installs whose config.ini already says
// moveattack=true (the MapleRoot default we shipped) still get the fix.
inline bool ShouldApplyMoveAttack(bool iniMoveAttack, bool vanillaAttackStop) {
    return iniMoveAttack && !vanillaAttackStop;
}

// Restore one site: write the original bytes only over the exact moveattack
// bytes. Anything else (already vanilla, or some other patch) is left alone.
inline bool ShouldRestoreAttackStopSite(const AttackStopSite& site, const unsigned char* current) {
    return ClassifyAttackStopSite(site, current) == AttackStopState::MoveAttack;
}

} // namespace integration

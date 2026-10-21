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
// SlumsMS splits the single switch into one config.ini key per site (see
// ResolveMoveAttackSettings for precedence) and applies them from here.
// Vanilla = leave (or put back) the original bytes at a site.
// Decisions are pure functions so they can be tested natively.
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <optional>
#include <string>

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

enum class MoveAttackSite : std::size_t { ChargeMove = 0, AttackWalk = 1, AttackJump = 2 };

// config.ini [clienthax] key that owns each site, indexed like AttackStopSites().
inline const char* MoveAttackSiteKey(MoveAttackSite site) {
    static const char* const keys[] = {"chargemove", "attackwalk", "attackjump"};
    return keys[static_cast<std::size_t>(site)];
}

// Parse an INI boolean the way MapleRoot's INIReader::GetBoolean does
// (true/yes/on/1, false/no/off/0, any case). Absent ("") or anything else is
// "not set" so it falls back like a missing key.
inline std::optional<bool> ParseIniBool(std::string value) {
    for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (value == "true" || value == "yes" || value == "on" || value == "1") return true;
    if (value == "false" || value == "no" || value == "off" || value == "0") return false;
    return std::nullopt;
}

struct MoveAttackIni {
    std::optional<bool> moveAttack;  // legacy MapleRoot switch for all three
    std::optional<bool> attackWalk;  // 0x009CBFB0, the slide
    std::optional<bool> attackJump;  // 0x0094C3BB
    std::optional<bool> chargeMove;  // 0x0095F97A
};

struct MoveAttackSettings {
    bool attackWalk;
    bool attackJump;
    bool chargeMove;
    bool Enabled(MoveAttackSite site) const {
        switch (site) {
        case MoveAttackSite::ChargeMove: return chargeMove;
        case MoveAttackSite::AttackWalk: return attackWalk;
        default: return attackJump;
        }
    }
};

// Precedence, per site (each key controls only its own site):
//   1. The site's own key (attackwalk / attackjump / chargemove), when set.
//   2. Otherwise attackjump and chargemove follow the legacy moveattack
//      (default true, MapleRoot's default), so old config.ini files keep
//      jumping and moving while charging; moveattack=false turns them off.
//   3. attackwalk never follows moveattack=true: the slide is off unless
//      attackwalk=true (owner's default, 2026-10-21: vanilla attack weight).
//      moveattack=false leaves it off too.
inline MoveAttackSettings ResolveMoveAttackSettings(const MoveAttackIni& ini) {
    const bool legacy = ini.moveAttack.value_or(true);
    return MoveAttackSettings{
        ini.attackWalk.value_or(false),
        ini.attackJump.value_or(legacy),
        ini.chargeMove.value_or(legacy),
    };
}

enum class MoveAttackWrite { None, WriteMoveAttack, WriteOriginal };

// What to write at one site. Enabled: patch only over the vanilla bytes.
// Disabled: restore only over the exact moveattack bytes (e.g. a stray
// MapleRoot ijl15.dll applied it). Unknown bytes belong to some other patch
// and are left alone either way.
inline MoveAttackWrite DecideMoveAttackWrite(const AttackStopSite& site, const unsigned char* current, bool enabled) {
    const AttackStopState state = ClassifyAttackStopSite(site, current);
    if (enabled && state == AttackStopState::Original) return MoveAttackWrite::WriteMoveAttack;
    if (!enabled && state == AttackStopState::MoveAttack) return MoveAttackWrite::WriteOriginal;
    return MoveAttackWrite::None;
}

// Apply the resolved settings. Reader is const unsigned char*(std::uint32_t
// address) giving the current bytes; Writer is void(std::uint32_t address,
// const unsigned char* bytes, std::size_t length) and owns page protection.
template <typename Reader, typename Writer>
void ApplyMoveAttackSettings(const MoveAttackSettings& settings, Reader&& read, Writer&& write) {
    const auto& sites = AttackStopSites();
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto& site = sites[i];
        switch (DecideMoveAttackWrite(site, read(site.address), settings.Enabled(static_cast<MoveAttackSite>(i)))) {
        case MoveAttackWrite::WriteMoveAttack: write(site.address, site.moveAttack.data(), site.length); break;
        case MoveAttackWrite::WriteOriginal: write(site.address, site.original.data(), site.length); break;
        default: break;
        }
    }
}

// In-process form: read the live client image at the site address.
template <typename Writer>
void ApplyMoveAttackSettings(const MoveAttackSettings& settings, Writer&& write) {
    ApplyMoveAttackSettings(settings,
        [](std::uint32_t address) {
            return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(address));
        },
        write);
}

} // namespace integration

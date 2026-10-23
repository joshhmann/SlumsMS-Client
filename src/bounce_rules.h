#pragma once

// ============================================================
// bounce_rules.h -- double-tap jump and the bounce block zones.
//
// How MapleRoot's universal bounces work (mapleroot/ijl15, unchanged here):
//   * The server grants every character Ninja Bounce 1050 / 10001050 / 20001050
//     ("Jump Around!") and Vertical Bounce 1054 / 10001054 / 20001054 ("Jump
//     Upwards!") at login. A player hotkeys one and presses it.
//   * CUserLocal::DoActiveSkill 0x00966F7A is detoured (SkillEdits/Activeskill.h
//     CUserLocal__DoActiveSkill_t). For those ids the detour rewrites the jg at
//     0x0096792A into a jmp to its doActiveSkills cave, which sends the bounce
//     ids to the stock bound-jump call at 0x0096897A, i.e.
//     CUserLocal::DoActiveSkill_BoundJump 0x0096BE38 (the thieves' Flash Jump).
//   * Inside that function a cave at 0x0096BF0B (FlashJumpAll) accepts the
//     bounce ids next to 14101004 and, per use, rewrites the impulse immediates
//     at 0x0096C00A / 0x0096C021 / 0x0096C031 (sideways for 1050, straight up
//     for 1054). 0x0096C073 is NOPed so every non-Hermit id takes the Night
//     Walker effect path. YourStuff.h "PROPER FJ" NOPs the one-time-action,
//     foothold and skill-request gates (0x0096BED2/BEE2/BF59/BF86/BFAE).
//
// SlumsMS adds two things on top, and touches none of those bytes:
//   1. Double-tap jump ([clienthax] doubletapjump, default false): the jump
//      key's call to CUserLocal::Jump in CUserLocal::OnKeyDown (0x0094F5A7,
//      E8 rel32 -> 0x0094C383) is redirected once at startup. A FRESH press
//      (lParam bit 30 clear: not keyboard auto-repeat) while the character is
//      airborne by the client's own physics state -- no foothold, not on a
//      ladder or rope, not swimming or in a fly map -- also asks
//      DoActiveSkill for the Ninja Bounce of its job family: the same call a
//      Ninja Bounce hotkey makes, so the same impulse (never real Flash Jump).
//      Holding jump never bounces; the stock "one bound jump per airtime" flag
//      (CUserLocal+0x2B58) still limits it to one per jump.
//   2. Bounce block zones: on a blocked map, DoActiveSkill refuses the six
//      universal bounce ids before anything moves or is sent (hotkey, macro or
//      double-tap alike). Flash Jump (4111006 / 14101004) is refused only where
//      the zone's category says block_flash_jump -- the jump quests, mirroring
//      stock v83: DoActiveSkill_BoundJump itself refuses any bound jump when
//      CField+0x144 (fieldLimit) has MOVEMENTSKILLS 0x2 (0x0096BE5E..0x0096BE71,
//      string 0x374), and the server's stock Map.wz sets 0x2 on every vanilla
//      JQ stage. Our list does not depend on the client's own Map.wz copy.
//      The sets come from the server (LP BOUNCE_ZONES_SYNC 0x3737) at login;
//      until they arrive the built-in jump-quest list below is used for both.
//
// Everything here is pure so tests/bounce_rules_test.cpp runs it natively.
// ============================================================

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace bounce {

// ---- skills ----
constexpr int kFlashJumpHermit = 4111006;
constexpr int kFlashJumpNightWalker = 14101004;
constexpr std::array<int, 3> kNinjaBounce = {{1050, 10001050, 20001050}};
constexpr std::array<int, 3> kVerticalBounce = {{1054, 10001054, 20001054}};

inline bool IsNinjaBounce(int id) {
    return std::find(kNinjaBounce.begin(), kNinjaBounce.end(), id) != kNinjaBounce.end();
}
inline bool IsVerticalBounce(int id) {
    return std::find(kVerticalBounce.begin(), kVerticalBounce.end(), id) != kVerticalBounce.end();
}
// The MapleRoot bounces every character has; never the thieves' real Flash Jump.
inline bool IsUniversalBounce(int id) { return IsNinjaBounce(id) || IsVerticalBounce(id); }
inline bool IsFlashJump(int id) { return id == kFlashJumpHermit || id == kFlashJumpNightWalker; }

// ---- map zones ----
struct MapRange {
    std::int32_t first;
    std::int32_t last; // inclusive
    bool operator==(const MapRange& o) const { return first == o.first && last == o.last; }
};

// Sorted, merged, inclusive ranges; Contains is a binary search.
class ZoneSet {
public:
    ZoneSet() = default;
    static ZoneSet FromRanges(std::vector<MapRange> ranges) {
        ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
                         [](const MapRange& r) { return r.first > r.last; }),
            ranges.end());
        std::sort(ranges.begin(), ranges.end(),
            [](const MapRange& a, const MapRange& b) { return a.first < b.first; });
        ZoneSet z;
        for (const auto& r : ranges) {
            if (!z.ranges_.empty() && static_cast<std::int64_t>(r.first) <= static_cast<std::int64_t>(z.ranges_.back().last) + 1) {
                z.ranges_.back().last = (std::max)(z.ranges_.back().last, r.last);
            } else {
                z.ranges_.push_back(r);
            }
        }
        return z;
    }
    bool Contains(int mapId) const {
        auto it = std::upper_bound(ranges_.begin(), ranges_.end(), mapId,
            [](int id, const MapRange& r) { return id < r.first; });
        if (it == ranges_.begin()) return false;
        --it;
        return mapId >= it->first && mapId <= it->last;
    }
    const std::vector<MapRange>& Ranges() const { return ranges_; }
    std::size_t MapCount() const {
        std::size_t n = 0;
        for (const auto& r : ranges_) n += static_cast<std::size_t>(r.last - r.first) + 1;
        return n;
    }

private:
    std::vector<MapRange> ranges_;
};

// The jump-quest category of the server's bounce-zones.yaml, generated by the server's
// tools/bounce/gen_client_zones.py. tests/bounce_rules_test.cpp checks it against the server's
// golden BOUNCE_ZONES_SYNC packet for the shipped yaml, so the two lists cannot drift silently.
inline const std::vector<MapRange>& BuiltinJumpQuestRanges() {
    static const std::vector<MapRange> ranges = {
#include "bounce_zones_builtin.inc"
    };
    return ranges;
}

// ---- LP BOUNCE_ZONES_SYNC 0x3737 ----
// After the opcode, little-endian: version(1) = 1, nativeBounce(4) = the Ninja Bounce id of the
// character's job family, n(2), n x (first(4), last(4)) inclusive map-id ranges where the universal
// bounces are refused, m(2), m x (first(4), last(4)) where Flash Jump is refused too. The server's
// server.bounce.BounceZonePackets writes it; the sets replace the built-in list.
constexpr std::uint16_t kBounceZonesSyncOpcode = 0x3737;
constexpr std::uint8_t kBounceZonesVersion = 1;
constexpr std::size_t kMaxZoneRanges = 4096;

struct ZonesSync {
    int nativeBounce = 0;
    std::vector<MapRange> bounce;
    std::vector<MapRange> flashJump;
};

inline std::int32_t ReadLe32(const unsigned char* p) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
        | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24));
}

// Decodes the bytes after the opcode. False (and out untouched) on a short, long or unknown packet.
inline bool DecodeZonesSync(const unsigned char* body, std::size_t size, ZonesSync& out) {
    if (!body || size < 9 || body[0] != kBounceZonesVersion) return false;
    ZonesSync z;
    z.nativeBounce = ReadLe32(body + 1);
    if (z.nativeBounce != 0 && !IsNinjaBounce(z.nativeBounce)) return false;
    std::size_t at = 5;
    for (auto* list : {&z.bounce, &z.flashJump}) {
        if (size < at + 2) return false;
        const std::size_t count = static_cast<std::size_t>(body[at]) | (static_cast<std::size_t>(body[at + 1]) << 8);
        at += 2;
        if (count > kMaxZoneRanges || size < at + count * 8) return false;
        list->reserve(count);
        for (std::size_t i = 0; i < count; ++i, at += 8) list->push_back({ReadLe32(body + at), ReadLe32(body + at + 4)});
    }
    if (at != size) return false;
    out = std::move(z);
    return true;
}

// ---- bounce impulses (LP BOUNCE_ZONES_SYNC version 2) ----
// The bound-jump impulse is three immediates inside CUserLocal::DoActiveSkill_BoundJump that
// MapleRoot's FlashJumpAll cave rewrites per use (SkillEdits/Activeskill.h):
//   0x0096C00A+1  mov edx, imm32          sideways speed facing left   (stock -350)
//   0x0096C021+3  lea ecx, [ecx*8+imm32]  sideways speed facing right  (stock +350)
//   0x0096C031+1  mov ecx, imm32          vertical speed               (stock -250)
// The thieves keep MapleRoot's strong values; everyone else gets stock Flash Jump strength
// for Ninja Bounce and a short hop for Vertical Bounce. The server sends all four sets in a
// SECOND 0x3737 packet with version 2, after the version 1 zones packet: a client that only
// knows version 1 rejects it as unknown and keeps its zones; a server that never sends it
// leaves today's MapleRoot values everywhere.
constexpr std::uint32_t kImpulseLeftVx = 0x0096C00A + 1;
constexpr std::uint32_t kImpulseRightVx = 0x0096C021 + 3;
constexpr std::uint32_t kImpulseVy = 0x0096C031 + 1;
constexpr std::uint8_t kBounceImpulsesVersion = 2;
constexpr std::int32_t kMaxImpulse = 5000;

struct Impulse {
    std::int32_t leftVx = 0;
    std::int32_t rightVx = 0;
    std::int32_t vy = 0;
    bool operator==(const Impulse& o) const { return leftVx == o.leftVx && rightVx == o.rightVx && vy == o.vy; }
};

// MapleRoot as shipped (YourStuff.h startup bytes and Activeskill.h applyVelocityChange).
constexpr Impulse kMapleRootNinja = {-683, 606, -688};
constexpr Impulse kMapleRootVertical = {-1, 0, -1200};
constexpr Impulse kStockFlashJump = {-350, 350, -250};

struct BounceImpulses {
    Impulse ninjaNative = kMapleRootNinja;      // thieves (4xx) and Night Walkers (14xx)
    Impulse ninjaOther = kMapleRootNinja;       // everyone else
    Impulse verticalNative = kMapleRootVertical;
    Impulse verticalOther = kMapleRootVertical;
};

// The families whose own skill book has Flash Jump: Thief (400, 41x, 42x) and Night Walker (14xx).
inline bool IsFlashJumpFamily(int job) {
    return job / 100 == 4 || job / 100 == 14;
}

// The immediates for this bound jump, or false for an id the cave does not handle. Real Flash Jump
// keeps the native Ninja Bounce values, which is what MapleRoot gives it today.
inline bool ImpulseFor(int skillId, int job, const BounceImpulses& cfg, Impulse& out) {
    const bool native = IsFlashJumpFamily(job);
    if (IsFlashJump(skillId)) { out = cfg.ninjaNative; return true; }
    if (IsNinjaBounce(skillId)) { out = native ? cfg.ninjaNative : cfg.ninjaOther; return true; }
    if (IsVerticalBounce(skillId)) { out = native ? cfg.verticalNative : cfg.verticalOther; return true; }
    return false;
}

// Version 2 body after the opcode: version(1) = 2, count(1) = 4, then count x (leftVx(4),
// rightVx(4), vy(4)) little-endian, in the order ninjaNative, ninjaOther, verticalNative,
// verticalOther. False (out untouched) on anything else or an impulse beyond +-kMaxImpulse.
inline bool DecodeImpulsesSync(const unsigned char* body, std::size_t size, BounceImpulses& out) {
    if (!body || size != 2 + 4 * 12 || body[0] != kBounceImpulsesVersion || body[1] != 4) return false;
    Impulse v[4];
    for (int i = 0; i < 4; ++i) {
        const unsigned char* p = body + 2 + i * 12;
        v[i] = {ReadLe32(p), ReadLe32(p + 4), ReadLe32(p + 8)};
        for (std::int32_t x : {v[i].leftVx, v[i].rightVx, v[i].vy}) {
            if (x > kMaxImpulse || x < -kMaxImpulse) return false;
        }
    }
    out.ninjaNative = v[0];
    out.ninjaOther = v[1];
    out.verticalNative = v[2];
    out.verticalOther = v[3];
    return true;
}

// ---- decisions ----

// The client's own movement state of the local user's CVecCtrl.
struct AirState {
    bool hasVecCtrl = false;
    bool onFoothold = false; // CVecCtrl+0x110 non-null
    bool onLadder = false;   // 0x00705309
    bool onRope = false;     // 0x00705343
    bool swimOrFly = false;  // 0x007046AC: swim area or fly map
};

inline bool IsAirborne(const AirState& s) {
    return s.hasVecCtrl && !s.onFoothold && !s.onLadder && !s.onRope && !s.swimOrFly;
}

// WM_KEYDOWN lParam: bit 30 = the key was already down (auto-repeat), bit 31 = key up.
inline bool IsFreshPress(std::uint32_t lParam) { return (lParam & 0xC0000000u) == 0; }

// The bounce a double-tap uses: EXACTLY the Ninja Bounce a hotkey would use -- the job family's
// (nativeBounce from the server; 0 = unknown), else any Ninja Bounce the character has. 0 = none.
//
// Never real Flash Jump, and never Vertical Bounce. It used to prefer Flash Jump 4111006 / 14101004
// when learned, which made a thief's double-tap a different jump from their Ninja Bounce hotkey:
// DoActiveSkill_BoundJump doubles the skill level into the impulse only for ids that are NOT
// 4111006 / 14101004 (0x0096BFCA..0x0096BFE9), and Hermit Flash Jump branches to the jump at
// 0x0096BF02, before MapleRoot's FlashJumpAll cave at 0x0096BF0B, so it never had its impulse
// chosen at all (it flew with whatever the last bounce wrote). Flash Jump stays on its own hotkey.
template <typename LevelOf>
int ChooseDoubleTapSkill(LevelOf levelOf, int nativeBounce) {
    if (IsNinjaBounce(nativeBounce) && levelOf(nativeBounce) > 0) return nativeBounce;
    for (int id : kNinjaBounce) {
        if (levelOf(id) > 0) return id;
    }
    return 0;
}

// Whether the current map refuses bounces / Flash Jump.
struct MapBlock {
    bool bounce = false;
    bool flashJump = false;
};

// Ninja Bounce only in the air, like stock Flash Jump. MapleRoot NOPed the bound jump's foothold
// gate (0x0096BF59) for every id, so a Ninja Bounce hotkey also fired from the ground: a weak hop
// with no jump under it. Vertical Bounce keeps working from the ground (that gate stays NOPed);
// real Flash Jump is untouched. An unreadable movement state (no CVecCtrl) allows the skill, so a
// bad read can never take the bounce away.
inline bool RefuseOnGround(int skillId, const AirState& air) {
    return IsNinjaBounce(skillId) && air.hasVecCtrl && !IsAirborne(air);
}

// Whether DoActiveSkill must refuse this skill on this map.
inline bool ShouldBlockSkill(int skillId, const MapBlock& here) {
    return (IsUniversalBounce(skillId) && here.bounce) || (IsFlashJump(skillId) && here.flashJump);
}

struct DoubleTapInput {
    bool enabled = true;      // [clienthax] doubletapjump
    std::uint32_t lParam = 0; // the jump key's WM_KEYDOWN lParam
    AirState air;
    MapBlock here;
};

// The skill a jump-key press should also use, or 0 for "just the stock jump".
template <typename LevelOf>
int DecideDoubleTap(const DoubleTapInput& in, LevelOf levelOf, int nativeBounce) {
    if (!in.enabled || !IsFreshPress(in.lParam) || !IsAirborne(in.air)) return 0;
    const int skill = ChooseDoubleTapSkill(levelOf, nativeBounce);
    if (skill == 0 || ShouldBlockSkill(skill, in.here)) return 0;
    return skill;
}

// ---- the one static patch ----
// CUserLocal::OnKeyDown 0x0094F2ED, jump function key (53):
//   0094F5A4  57              push edi            ; bEnforced = 0
//   0094F5A5  8B CB           mov ecx, ebx        ; CUserLocal*
//   0094F5A7  E8 D7 CD FF FF  call 0x0094C383     ; CUserLocal::Jump
// Only the rel32 of that call changes. MapleRoot's attackjump patch (0x0094C3BB, inside Jump)
// is untouched; Jump itself is not detoured.
constexpr std::uint32_t kJumpKeyCallSite = 0x0094F5A7;
constexpr std::uint32_t kCUserLocalJump = 0x0094C383;
constexpr std::uint32_t kJumpKeyContext = 0x0094F5A4;
constexpr std::array<unsigned char, 8> kJumpKeyOriginal = {{0x57, 0x8B, 0xCB, 0xE8, 0xD7, 0xCD, 0xFF, 0xFF}};

inline std::array<unsigned char, 5> CallRel32(std::uint32_t site, std::uint32_t target) {
    const std::uint32_t rel = target - (site + 5);
    return {{0xE8, static_cast<unsigned char>(rel), static_cast<unsigned char>(rel >> 8),
        static_cast<unsigned char>(rel >> 16), static_cast<unsigned char>(rel >> 24)}};
}

enum class JumpSiteState { Original, Patched, Unknown };

// current = 8 bytes read from kJumpKeyContext.
inline JumpSiteState ClassifyJumpSite(const unsigned char* current, std::uint32_t thunk) {
    if (std::memcmp(current, kJumpKeyOriginal.data(), kJumpKeyOriginal.size()) == 0) return JumpSiteState::Original;
    const auto call = CallRel32(kJumpKeyCallSite, thunk);
    if (std::memcmp(current, kJumpKeyOriginal.data(), 3) == 0 && std::memcmp(current + 3, call.data(), call.size()) == 0)
        return JumpSiteState::Patched;
    return JumpSiteState::Unknown;
}

} // namespace bounce

#pragma once
// Bot attack diagnostic (ticket 32): the pure part - decoding another character's attack packet the way
// CUserRemote::OnAttack (0x009803AB) reads it, and the reasons the client drops or misdraws one. The hooks in
// attackdiag.cpp feed it the live facts (the attacker's weapon, what the client's own checks returned).
// The rules and their addresses are in the server repo's docs/real-bots/client-remote-attack-rules.md.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace attackdiag {

constexpr int kCloseRange = 0xBA, kRanged = 0xBB, kMagic = 0xBC, kBody = 0xBD;
constexpr int kActionCount = 162; // the body-action table 0x004A38E7 fills at 0x00BEC620
constexpr int kMesoExplosion = 4211006;

struct Attack {
    int type = 0;
    int mobs = 0, lines = 0;
    int skillId = 0, skillLevel = 0;
    int option = 0, action = 0;
    bool left = false;
    int speed = 0;
    int bullet = 0;
    bool truncated = false;
};

// The keydown int of Big Bang and the Evan breaths (0x0098066E).
inline bool HasKeydownInt(int skillId) {
    return skillId == 2121001 || skillId == 2221001 || skillId == 2321001 || skillId == 22121000 || skillId == 22151001;
}

// Reads one remote attack body (after the attacker id) exactly as OnAttack does.
inline Attack Decode(int type, const std::uint8_t* p, std::size_t n) {
    Attack a;
    a.type = type;
    std::size_t at = 0;
    auto u8 = [&](int& v) { if (at + 1 > n) { a.truncated = true; v = 0; return; } v = p[at]; at += 1; };
    auto u16 = [&](int& v) {
        if (at + 2 > n) { a.truncated = true; v = 0; return; }
        v = p[at] | (p[at + 1] << 8); at += 2;
    };
    auto u32 = [&](int& v) {
        if (at + 4 > n) { a.truncated = true; v = 0; return; }
        std::uint32_t x; std::memcpy(&x, p + at, 4); v = static_cast<int>(x); at += 4;
    };
    int info = 0, level = 0, word = 0, mastery = 0;
    u8(info);
    a.mobs = info >> 4;
    a.lines = info & 0xF;
    u8(level);
    u8(a.skillLevel);
    if (a.skillLevel > 0) u32(a.skillId);
    u8(a.option);
    u16(word);
    a.action = word & 0x7FFF;
    a.left = (word >> 15) != 0;
    u8(a.speed);
    u8(mastery);
    u32(a.bullet);
    for (int m = 0; m < a.mobs && !a.truncated; ++m) {
        int mob = 0, hit = 0, count = a.lines, line = 0;
        u32(mob);
        if (mob == 0) continue;
        u8(hit);
        if (a.skillId == kMesoExplosion) u8(count);
        for (int i = 0; i < count && !a.truncated; ++i) u32(line);
    }
    if (type == kRanged) { int x = 0; u16(x); u16(x); }
    if (HasKeydownInt(a.skillId)) { int x = 0; u32(x); }
    return a;
}

// What the hooks know about the attacker when the attack arrives.
struct Facts {
    bool skillKnown = true;          // CSkillInfo::GetSkill (0x0075C755) found the skill
    int weaponType = 0;              // get_weapon_type (0x00460AA0) of the attacker's weapon
    int attackType = 0;              // the weapon's Character.wz "attack" (CUser+0x4F8)
    bool hasAfterImage = true;       // the weapon's "afterImage" (CUser+0x508) is set
    bool shootsWithoutWeapon = false;// 0x0076681D
    bool keepsKeydownPose = false;   // OnAttack's list at 0x00980452
    bool inKeydown = false;          // the attacker's keydown state (CUserRemote+0x37CC) holds this skill
    bool checkSkipped = false;       // OnAttack's list at 0x00980722 (no CheckAttackAction)
    int checkResult = -1;            // CUser::CheckAttackAction (0x0092EDB2): 1 drawn, 0 dropped, -1 not called
    bool weaponFitsSkill = true;     // SKILLENTRY::IsCorrectWeaponType (0x00766157)
};

// Why the client drops or misdraws the attack; empty when it draws all of it.
inline std::vector<std::string> Reasons(const Attack& a, const Facts& f) {
    std::vector<std::string> r;
    if (a.truncated) r.push_back("packet shorter than OnAttack reads (CInPacket throws)");
    if (a.action >= kActionCount) r.push_back("pose outside the action table (0x009807CF reads past it)");
    if (a.skillId != 0 && !f.skillKnown) r.push_back("skill not in this client's Skill.wz");
    if (f.checkResult == 0) {
        if (f.attackType < 1 || f.attackType > 9) r.push_back("weapon attack type outside 1..9: no pose is valid");
        else if (a.skillId != 0 && f.skillKnown && !f.weaponFitsSkill) r.push_back("weapon cannot use the skill");
        else r.push_back("pose not allowed for the skill / weapon (CheckAttackAction)");
    }
    switch (a.type) {
        case kCloseRange:
            if (!f.hasAfterImage) r.push_back("melee with a weapon without afterImage: no hits drawn (0x00936F34)");
            break;
        case kRanged: {
            bool shooting = f.weaponType == 45 || f.weaponType == 46 || f.weaponType == 47 || f.weaponType == 49;
            if (!shooting && !f.shootsWithoutWeapon) r.push_back("shot from a non-shooting weapon: nothing drawn (0x009812B4)");
            break;
        }
        case kMagic:
            if (a.skillId == 0 || !f.skillKnown) r.push_back("spell without a known skill: nothing drawn (0x0098252A)");
            break;
        default:
            break;
    }
    if (f.keepsKeydownPose && !f.inKeydown) r.push_back("keydown skill without a keydown packet: drawn without its pose");
    return r;
}

} // namespace attackdiag

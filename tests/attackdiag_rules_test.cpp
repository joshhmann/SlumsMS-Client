#include "../src/attackdiag_rules.h"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace attackdiag;

static void require(bool value, const char* what) {
    if (!value) throw std::runtime_error(what);
}

// A remote close-range attack body as the server writes it (PacketCreator.addAttackBody, after the attacker id).
static std::vector<std::uint8_t> Body(int skill, int level, int pose, bool left, int mobs, int lines) {
    std::vector<std::uint8_t> b;
    auto u8 = [&](int v) { b.push_back(static_cast<std::uint8_t>(v)); };
    auto u32 = [&](int v) { for (int i = 0; i < 4; ++i) u8((v >> (8 * i)) & 0xFF); };
    u8((mobs << 4) | lines);
    u8(0x5B);
    u8(level);
    if (level > 0) u32(skill);
    u8(0);                       // option
    u8(pose);                    // pose low byte
    u8(left ? 0x80 : 0x00);      // pose high byte: bit 15 = facing left
    u8(6);                       // speed
    u8(0x0A);                    // mastery
    u32(0);                      // bullet
    for (int m = 0; m < mobs; ++m) {
        u32(1000 + m);
        u8(0);
        for (int i = 0; i < lines; ++i) u32(123);
    }
    return b;
}

int main() {
    auto body = Body(21110006, 20, 131, true, 3, 4);
    Attack a = Decode(kCloseRange, body.data(), body.size());
    require(!a.truncated && a.skillId == 21110006 && a.skillLevel == 20, "skill decoded");
    require(a.action == 131 && a.left && a.mobs == 3 && a.lines == 4 && a.speed == 6, "pose and counts decoded");
    Facts ok;
    ok.attackType = 2; ok.weaponType = 44; ok.checkResult = 1;
    require(Reasons(a, ok).empty(), "a drawn attack has no reasons");

    Attack cut = Decode(kCloseRange, body.data(), body.size() - 3);
    require(cut.truncated && !Reasons(cut, ok).empty(), "a short packet is reported");

    Facts dropped = ok;
    dropped.checkResult = 0;
    require(Reasons(a, dropped)[0].find("pose not allowed") != std::string::npos, "a refused pose");
    dropped.weaponFitsSkill = false;
    require(Reasons(a, dropped)[0].find("weapon cannot use") != std::string::npos, "a refused weapon");

    auto shot = Body(0, 0, 5, false, 1, 1);
    shot.push_back(0); shot.push_back(0); shot.push_back(0); shot.push_back(0); // the ranged tail
    Attack s = Decode(kRanged, shot.data(), shot.size());
    Facts sword = ok;
    sword.weaponType = 30;
    require(!s.truncated && !Reasons(s, sword).empty(), "a shot from a sword is dropped");

    Facts hurricane = ok;
    hurricane.keepsKeydownPose = true;
    require(!Reasons(a, hurricane).empty(), "a keydown skill without its keydown is reported");
    hurricane.inKeydown = true;
    require(Reasons(a, hurricane).empty(), "with its keydown it is drawn");

    Facts noImage = ok;
    noImage.hasAfterImage = false;
    require(!Reasons(a, noImage).empty(), "melee without an afterImage draws no hits");
    std::cout << "PASS: decode, drop reasons (pose, weapon, shot weapon, keydown, afterimage, short packet)\n";
}

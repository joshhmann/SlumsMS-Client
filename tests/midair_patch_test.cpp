// Ticket 126: the in-air gates (midair_patch.h): jump encodings and the keydown policy.
#include "../src/midair_patch.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace MidAir;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        // The writes MapleRoot makes today: E9 12 00 00 00 and E9 36 00 00 00.
        Check(JmpTo(kShoot.addr, kShoot.allowTarget) == std::array<std::uint8_t, 5>{0xE9, 0x12, 0x00, 0x00, 0x00}, "jumpshoot write");
        Check(JmpTo(kMagic.addr, kMagic.allowTarget) == std::array<std::uint8_t, 5>{0xE9, 0x36, 0x00, 0x00, 0x00}, "jumpmagic write");
        Check(kKeydown.allowTarget > kKeydown.addr && kKeydown.allowTarget - kKeydown.addr == 0x26, "keydown continues at the rope test");

        Check(KeydownPolicy(3121004, 312, true) == AirCast::Allow, "Bowmaster Hurricane");
        Check(KeydownPolicy(13111002, 1311, true) == AirCast::Allow, "Wind Archer Hurricane");
        Check(KeydownPolicy(2121001, 212, true) == AirCast::Allow, "F/P Big Bang");
        Check(KeydownPolicy(3121004, 312, false) == AirCast::Stock, "off by default");
        Check(KeydownPolicy(5221004, 522, true) == AirCast::Stock, "Rapid Fire stays stock");
        Check(KeydownPolicy(3121004, 412, true) == AirCast::Stock, "only mages and archers");
        Check(IsExplorerOrCygnusMageArcher(232) && IsExplorerOrCygnusMageArcher(1312) && !IsExplorerOrCygnusMageArcher(1400), "jobs");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "midair: ok\n";
    return 0;
}

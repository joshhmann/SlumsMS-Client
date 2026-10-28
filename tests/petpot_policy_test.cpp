// Ticket 122: pet potion window rules (petpot_policy.h).
#include "../src/petpot_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace PetPot;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        Check(SnapThreshold(-3) == 0 && SnapThreshold(0) == 0, "off");
        Check(SnapThreshold(47) == 45 && SnapThreshold(48) == 50 && SnapThreshold(50) == 50, "5% steps");
        Check(SnapThreshold(100) == 95, "stock max");
        Check(MaySet(2000005, 3), "a held potion");
        Check(!MaySet(2000005, 0), "not held: the server would disconnect");
        Check(!MaySet(4000000, 5), "not a potion");
        Check(MaySet(0, 0), "clearing is always fine");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "petpot: ok\n";
    return 0;
}

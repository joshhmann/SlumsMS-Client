// Ticket 124: controller and mouse-button input (input_policy.h).
#include "../src/input_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Input;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        Dirs none{};
        Dirs d = StickToDirs(3000, -2000, none);
        Check(!d.left && !d.right && !d.up && !d.down, "rest is still");
        d = StickToDirs(20000, 0, none);
        Check(d.right && !d.left, "push right");
        d = StickToDirs(7000, 0, d);
        Check(d.right, "hysteresis keeps holding at 7000");
        d = StickToDirs(7000, 0, none);
        Check(!d.right, "but 7000 doesn't start a press");
        d = StickToDirs(0, 30000, none);
        Check(d.up && !d.down, "up is positive");
        Check(CursorSpeed(5000) == 0.0f && CursorSpeed(32767) > 17.9f && CursorSpeed(-32767) < -17.9f, "cursor curve");
        Check(CursorSpeed(12000) < CursorSpeed(20000), "faster with more tilt");
        MouseMap m; m.x1 = 0x1D;
        Check(ScanForXButton(1, m) == 0x1D && ScanForXButton(2, m) == 0, "side buttons");
        Check(kLeft == 0xCB && kRight == 0xCD, "scan codes");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "input: ok\n";
    return 0;
}

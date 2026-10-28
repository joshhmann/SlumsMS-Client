// Borderless placement and the config switch (window_policy.h).
#include "../src/window_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace WindowPolicy;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        int x, y;
        Place({0, 0, 1920, 1080}, 1920, 1080, x, y);
        Check(x == 0 && y == 0, "native resolution covers the screen");
        Place({0, 0, 1920, 1080}, 1366, 768, x, y);
        Check(x == 277 && y == 156, "smaller: centred");
        Place({1920, 0, 4480, 1440}, 2560, 1440, x, y);
        Check(x == 1920 && y == 0, "second monitor");
        Place({0, 0, 1280, 720}, 1920, 1080, x, y);
        Check(x == 0 && y == 0, "bigger than the monitor: top-left stays visible");
        Check(IniTrue("true") && IniTrue("1") && IniTrue("ON") && !IniTrue("false") && !IniTrue("") && !IniTrue(nullptr), "switch values");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "window: ok\n";
    return 0;
}

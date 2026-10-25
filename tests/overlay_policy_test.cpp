// ImGui HUD layout maths (overlay_policy.h): fractions round-trip, widgets stay on screen across resolutions,
// font size follows the screen, bars and labels.
#include "../src/overlay_policy.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace OverlayPolicy;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.6f; }
}  // namespace

int main() {
    try {
        // A widget at (40, 60) 240x90 on 800x600 keeps its relative place at 1920x1080.
        const int fx = ToFraction(40, 800), fy = ToFraction(60, 600), fw = ToFraction(240, 800), fh = ToFraction(90, 600);
        const Rect a = FromFractions(fx, fy, fw, fh, 800, 600, 120, 40);
        Check(Near(a.x, 40) && Near(a.y, 60) && Near(a.w, 240) && Near(a.h, 90), "round trip at 800x600");
        const Rect b = FromFractions(fx, fy, fw, fh, 1920, 1080, 120, 40);
        Check(Near(b.x, 96) && Near(b.y, 108) && Near(b.w, 576) && Near(b.h, 162), "scales to 1920x1080");

        // Anything stored off-screen or oversized is pulled back fully on screen.
        const Rect c = FromFractions(9900, 9900, 3000, 3000, 800, 600, 120, 40);
        Check(c.x + c.w <= 800.01f && c.y + c.h <= 600.01f && c.x >= 0 && c.y >= 0, "kept on screen");
        const Rect d = FromFractions(0, 0, 50, 50, 800, 600, 120, 40);
        Check(Near(d.w, 120) && Near(d.h, 40), "minimum size");
        Check(ToFraction(900, 800) == kFrac && ToFraction(-5, 800) == 0, "fractions clamp");

        Check(Near(FontPixels(600, 100), 15) && Near(FontPixels(1080, 100), 27), "font follows the screen");
        Check(Near(FontPixels(600, 150), 22.5f) && FontPixels(600, 10) >= 11 && FontPixels(2160, 200) <= 40, "scale and limits");

        Check(Ratio(50, 200) == 0.25f && Ratio(5, 0) == 0.0f && Ratio(300, 200) == 1.0f && Ratio(-1, 9) == 0.0f, "ratios");
        char buf[64];
        BarLabel(buf, sizeof(buf), 17956, 30000, false);
        Check(std::strcmp(buf, "17956 / 30000 (59%)") == 0, std::string("HP label: ") + buf);
        BarLabel(buf, sizeof(buf), 1234, 5678, true);
        Check(std::strcmp(buf, "1234 / 5678 (21.73%)") == 0, std::string("EXP label: ") + buf);

        std::cout << "overlay policy OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "overlay_policy_test FAILED: " << e.what() << "\n";
        return 1;
    }
}

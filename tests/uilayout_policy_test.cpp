// Movable stock UI (uilayout_policy.h): storage packing, fraction <-> pixel conversion and on-screen clamping.
#include "../src/uilayout_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace UiLayout;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        // Packing keeps signed values and rejects nothing it can store.
        for (Offset o : {Offset{0, 0}, Offset{-2500, 1234}, Offset{kFrac, -kFrac}, Offset{-1, -1}}) {
            const Offset back = Unpack(Pack(o));
            Check(back.fx == o.fx && back.fy == o.fy, "pack round trip");
        }
        Check(Unpack(Pack(Offset{0, 0})).fx == 0 && Pack(Offset{0, 0}) == 0, "zero is the stock place");
        const Offset wild = Unpack(0x7FFF8000u);   // out of range values from a hand-edited registry
        Check(wild.fx == kFrac && wild.fy == -kFrac, "unpack clamps to one screen");

        // Fractions follow the screen: 1/4 of the width is 200 px at 800 and 400 px at 1600.
        Check(ToPx(2500, 800) == 200 && ToPx(2500, 1600) == 400, "fraction to pixels");
        Check(ToPx(-2500, 800) == -200, "negative fraction");
        Check(ToFrac(200, 800) == 2500 && ToFrac(-200, 800) == -2500, "pixels to fraction");
        Check(ToFrac(5, 0) == 0, "no screen, no offset");

        // The bottom bar at 800x600: home 0,540 800x60. Moved up 300 it stays on screen; moved down it cannot.
        const Rect bar{0, 540, 800, 60};
        int dx = 0, dy = -300;
        Clamp(bar, 800, 600, dx, dy);
        Check(dx == 0 && dy == -300, "inside the screen is kept");
        dx = 50; dy = 40;
        Clamp(bar, 800, 600, dx, dy);
        Check(dx == 0 && dy == 0, "a full-width bar cannot move sideways or below the bottom");
        dx = 0; dy = -900;
        Clamp(bar, 800, 600, dx, dy);
        Check(dy == -540, "and stops at the top");

        // The quick slot at 1600x900: home 1200,820 232x80; a stored offset from 800x600 scales and clamps.
        const Rect qs{1200, 820, 232, 80};
        PixelOffset(Offset{ToFrac(300, 800), ToFrac(-100, 600)}, qs, 1600, 900, dx, dy);
        Check(dx == 168 && dy == -150, "scaled to 1600x900, clamped to the right edge (1600 - 232 - 1200)");

        // Unknown size: only kept within one screen.
        dx = 5000; dy = -5000;
        Clamp(Rect{}, 800, 600, dx, dy);
        Check(dx == 800 && dy == -600, "unknown size limits to one screen");

        // An element wider than the screen pins to the left/top edge.
        dx = 30; dy = 0;
        Clamp(Rect{10, 0, 900, 50}, 800, 600, dx, dy);
        Check(dx == -10, "too wide pins left");

        // A control group stays inside its window: buttons at 573,543 (bar coords) in a 1920x578 window on screen
        // at 0,322 of a 1600x900 screen may go up to the window's top, not above it.
        const Rect window{0, 322, 1920, 578};
        const Rect bounds = Intersect(window, Rect{0, 0, 1600, 900});
        Check(bounds.left == 0 && bounds.top == 322 && bounds.w == 1600 && bounds.h == 578, "window clipped to screen");
        const Rect btn{573, 322 + 543, 222, 34};
        dx = -100; dy = -900;
        ClampWithin(btn, bounds, dx, dy);
        Check(dx == -100 && dy == -543, "stops at the window top");
        dx = 2000; dy = 0;
        ClampWithin(btn, bounds, dx, dy);
        Check(dx == 1600 - 222 - 573, "and at the screen edge");
        Check(Intersect(Rect{0, 0, 10, 10}, Rect{20, 20, 5, 5}).w == 0, "disjoint rects intersect empty");

        std::cout << "uilayout: packing, scaling and clamping OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "uilayout_policy_test FAILED: " << e.what() << "\n";
        return 1;
    }
}

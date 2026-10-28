#pragma once
// ============================================================
// input_policy.h -- controller and mouse-button input (ticket 124). SCAFFOLD.
//
// No Windows, no client: tests/input_policy_test.cpp checks it. The game reads the keyboard through DirectInput
// scan codes; movementkeys.cpp already patches the raw keyboard snapshot at 0x0059B00A, so a controller's directions
// and buttons become real key-down/hold/repeat there. Mouse side buttons (WM_XBUTTONDOWN, ignored by the stock
// client) map to key slots from the window procedure. The right stick moves the cursor (SendInput first).
// Research note: docs/research-123-124.md.
// ============================================================

#include <cmath>
#include <cstdint>

namespace Input {

// DirectInput scan codes the movement keys use.
constexpr uint8_t kLeft = 0xCB, kRight = 0xCD, kUp = 0xC8, kDown = 0xD0;

struct Dirs { bool left, right, up, down; };

// Left stick (-32768..32767) to arrow keys with a dead zone and a little hysteresis, so a resting stick never walks
// and a held direction doesn't flicker at the edge.
inline Dirs StickToDirs(int x, int y, Dirs held, int deadzone = 8000, int release = 6500) {
    auto axis = [](int v, bool posHeld, bool negHeld, int on, int off, bool& pos, bool& neg) {
        pos = v > (posHeld ? off : on);
        neg = v < -(negHeld ? off : on);
    };
    Dirs d{};
    axis(x, held.right, held.left, deadzone, release, d.right, d.left);
    axis(y, held.up, held.down, deadzone, release, d.up, d.down);   // XInput y is up-positive
    return d;
}

// Right stick to cursor speed in pixels per frame: nothing inside the dead zone, then a curve so small tilts aim.
inline float CursorSpeed(int v, int deadzone = 7000, float maxPx = 18.0f) {
    const int a = v < 0 ? -v : v;
    if (a <= deadzone) return 0.0f;
    const float t = static_cast<float>(a - deadzone) / static_cast<float>(32767 - deadzone);
    const float s = maxPx * t * t;
    return v < 0 ? -s : s;
}

// Mouse X1/X2 to a scan code chosen by the player (0 = not mapped).
struct MouseMap { uint8_t x1 = 0, x2 = 0; };
inline uint8_t ScanForXButton(int button, const MouseMap& m) { return button == 1 ? m.x1 : button == 2 ? m.x2 : 0; }

}  // namespace Input

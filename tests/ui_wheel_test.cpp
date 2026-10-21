// The v83 client hands OnMouseWheel GET_WHEEL_DELTA / -120 (CWndMan::ProcessMouse at
// 0x009E3DE9), so wheel UP is negative. Pin that a list scrolls to earlier rows on it.
#include "../src/ui_wheel.h"
#include <cstdio>

static int ClientWheel(int nWindowsDelta) { return nWindowsDelta / -120; }   // as the client does

int main() {
    int fail = 0;
    const int up = ClientWheel(120), down = ClientWheel(-120), touch = ClientWheel(60);
    if (up != -1 || down != 1) { std::puts("FAIL: client sign model"); ++fail; }
    if (WheelRowStep(up) != -1) { std::puts("FAIL: wheel up must go to earlier rows"); ++fail; }
    if (WheelRowStep(down) != 1) { std::puts("FAIL: wheel down must go to later rows"); ++fail; }
    if (WheelRowStep(touch) != 0) { std::puts("FAIL: a sub-notch delta must not scroll"); ++fail; }
    if (WheelRowStep(ClientWheel(360)) != -1) { std::puts("FAIL: a fast flick up is still one row up"); ++fail; }
    if (!fail) std::puts("PASS: wheel up -> earlier rows, down -> later, sub-notch -> none");
    return fail;
}

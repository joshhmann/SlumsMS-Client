// First-run Edit UI tour (uitour_policy.h): when the card comes up on its own, and the "Show me" pulse.
#include "../src/uitour_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace UiTour;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        // Once per PC, in the field, after the map has had a moment to draw.
        Check(ShouldOffer(true, 0, true, kDelayMs), "first run in the field");
        Check(!ShouldOffer(true, 0, true, kDelayMs - 1), "not before the delay");
        Check(!ShouldOffer(true, 0, false, kDelayMs * 10), "never outside the field");
        Check(!ShouldOffer(true, kVersion, true, kDelayMs), "seen: never again");
        Check(!ShouldOffer(true, kVersion + 3, true, kDelayMs), "seen by a newer client");
        Check(!ShouldOffer(false, 0, true, kDelayMs), "uitour=false (QA)");
        Check(ShouldOffer(true, kVersion - 1, true, kDelayMs), "an older tour seen: the new one shows once");

        // The pulse stays visible while it runs, never exceeds full strength, and ends.
        for (unsigned ms = 0; ms < kPulseMs; ms += 37) {
            const float p = Pulse(ms);
            Check(p > 0.1f && p <= 1.0f, "pulse in range at " + std::to_string(ms));
        }
        Check(Pulse(kPulseMs) == 0.0f && Pulse(kPulseMs * 3) == 0.0f, "pulse ends");
        Check(Pulse(500) > Pulse(0), "it pulses");
        Check(Pulse(500) > Pulse(5500), "and fades");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "uitour policy: ok\n";
    return 0;
}

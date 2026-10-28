// Ticket 118: EXP per hour and DPS (hud_meters_policy.h).
#include "../src/hud_meters_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace HudMeters;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        ExpTracker t;
        t.Sample(0, 100, 0, 1000000);
        Check(t.PerHour() == 0, "nothing yet");
        t.Sample(30000, 100, 50000, 1000000);
        Check(t.PerHour() == 0, "under a minute: no rate");
        t.Sample(60000, 100, 100000, 1000000);
        Check(t.PerHour() == 6000000, "100k in a minute = 6M/h: " + std::to_string(t.PerHour()));
        Check(t.SecondsToLevel() == 540, "900k left at 6M/h = 9 min: " + std::to_string(t.SecondsToLevel()));
        t.Sample(120000, 101, 50000, 2000000);   // level up: 900k to finish + 50k
        Check(t.Session() == 1050000, "gain across the level-up: " + std::to_string(t.Session()));
        t.Sample(130000, 101, 40000, 2000000);   // EXP lost on death: not a gain
        Check(t.Session() == 1050000, "losses ignored");

        DpsMeter d;
        d.Hit(0, 2121006, 500000);
        d.Hit(1000, 2121006, 500000);
        d.Hit(2000, 2121007, 2000000);
        d.Hit(2500, 0, -1);
        Check(d.Total() == 3000000, "total");
        Check(d.Dps(5000) == 300000, "3M over a 10 s window: " + std::to_string(d.Dps(5000)));
        Check(d.Dps(10500) == 250000, "only the first hit left the window: " + std::to_string(d.Dps(10500)));
        auto top = d.Top(1);
        Check(top.size() == 1 && top[0].first == 2121007, "top skill");
        Check(kOnHitStock[0] == 0xB8 && kAddr_CMob_OnHit == 0x00668B83, "site");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "hud meters: ok\n";
    return 0;
}

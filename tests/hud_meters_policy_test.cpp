// Ticket 118: EXP per hour and DPS (hud_meters_policy.h).
#include "../src/hud_meters_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace HudMeters;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
void Put(std::vector<uint8_t>& b, const void* p, size_t n) {
    const auto* c = static_cast<const uint8_t*>(p);
    b.insert(b.end(), c, c + n);
}
void Row(std::vector<uint8_t>& b, int id, const std::string& name, long long dmg) {
    Put(b, &id, 4);
    const uint16_t n = static_cast<uint16_t>(name.size());
    Put(b, &n, 2);
    Put(b, name.data(), name.size());
    Put(b, &dmg, 8);
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

        // LP 0x3739 body, as the server's DamageRank.java writes it.
        std::vector<uint8_t> b;
        const int mob = 8800000;
        Put(b, &mob, 4);
        b.push_back(3);
        Row(b, 7, "Slums", 3000000000LL);   // past 32 bits on purpose
        Row(b, 9, "Penguin", 1000000000LL);
        Row(b, 4, "", 0);
        DamageRank r;
        Check(ParseDamageRank(b.data(), b.size(), r), "parse");
        Check(r.mobId == 8800000 && r.rows.size() == 3, "mob and rows");
        Check(r.rows[0].name == "Slums" && r.rows[0].damage == 3000000000LL && r.rows[1].charId == 9, "row values");
        Check(SharePct(r, 0) == 75 && SharePct(r, 1) == 25 && SharePct(r, 2) == 0 && SharePct(r, 9) == 0, "shares");
        DamageRank keep = r;
        Check(!ParseDamageRank(b.data(), b.size() - 1, keep) && keep.rows.size() == 3, "short packet refused, old rank kept");
        std::vector<uint8_t> many(b.begin(), b.begin() + 4);
        many.push_back(7);
        Check(!ParseDamageRank(many.data(), many.size(), keep), "more than 6 rows refused");
        std::vector<uint8_t> longName(b.begin(), b.begin() + 4);
        longName.push_back(1);
        Row(longName, 1, std::string(40, 'x'), 1);
        Check(!ParseDamageRank(longName.data(), longName.size(), keep), "over-long name refused");
        Check(!ParseDamageRank(nullptr, 0, keep) && kDamageRankOpcode == 0x3739, "empty; opcode");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "hud meters: ok\n";
    return 0;
}

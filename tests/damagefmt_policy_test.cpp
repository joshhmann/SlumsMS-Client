// Ticket 119: short damage numbers (damagefmt_policy.h).
#include "../src/damagefmt_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace DamageFmt;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
std::string S(long long v) {
    char b[32];
    ShortDamage(v, b, sizeof(b));
    return b;
}
}  // namespace

int main() {
    try {
        Check(S(0) == "0" && S(9999) == "9999" && S(-5) == "-5", "small numbers stay stock");
        Check(S(10000) == "10K", S(10000));
        Check(S(12345) == "12.3K", S(12345));
        Check(S(999999) == "999K", S(999999));
        Check(S(1000000) == "1M", S(1000000));
        Check(S(1234567) == "1.23M", S(1234567));
        Check(S(1990000) == "1.99M", S(1990000));
        Check(S(2147483647) == "2.14B", S(2147483647));
        Check(S(15000) == "15K", "trailing zero dropped: " + S(15000));
        char g[] = "1.23M";
        EncodeGlyphs(g);
        Check(std::string(g) == "1:23<" && g[1] - '0' == 10 && g[4] - '0' == 12, "glyph indexes");
        Check(ClampPct(10) == 60 && ClampPct(100) == 100 && ClampPct(400) == 150, "size clamp");
        Check(kFormatCall.stock[0] == 0xE8, "a call");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "damagefmt: ok\n";
    return 0;
}

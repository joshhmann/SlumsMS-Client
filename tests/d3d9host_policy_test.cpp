// D3D9 host (d3d9host_policy.h): the CreateDevice retries and the HRESULT names in the log.
#include "../src/d3d9host_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D9Host;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        // Two adapters, the client asks for adapter 0 with hardware vertex processing (d3d8to9's usual 0x40).
        const auto two = FallbackAttempts(0, 2, kHardwareVP);
        const std::vector<Attempt> wantTwo = {{1, kHardwareVP}, {0, kSoftwareVP}, {1, kSoftwareVP}};
        Check(two == wantTwo, "other adapter first, then software VP on the requested one, then the rest");

        // One adapter: only the software-VP retry.
        const auto one = FallbackAttempts(0, 1, kHardwareVP | 0x2);
        Check(one.size() == 1 && one[0].adapter == 0 && one[0].behavior == (kSoftwareVP | 0x2),
            "single adapter: software VP, other flags kept");

        // Already software: no repeated call, only the other adapters.
        const auto sw = FallbackAttempts(1, 3, kSoftwareVP);
        Check(sw.size() == 2 && sw[0].adapter == 0 && sw[1].adapter == 2, "software already: other adapters only");
        for (const auto& a : sw) Check(!(a.adapter == 1 && a.behavior == kSoftwareVP), "never repeats the failed call");

        // Mixed vertex processing becomes software too.
        Check(WithSoftwareVP(kMixedVP | 0x4) == (kSoftwareVP | 0x4), "mixed -> software");

        Check(std::string(HrName(0x80004005)) == "E_FAIL", "E_FAIL named (-2147467259, the Parsec start-up error)");
        Check(std::string(HrName(0x8876086A)) == "D3DERR_NOTAVAILABLE", "NOTAVAILABLE named");
        Check(std::string(HrName(0x12345678)) == "?", "unknown codes");

        std::cout << "d3d9host policy OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "d3d9host_policy_test FAILED: " << e.what() << "\n";
        return 1;
    }
}

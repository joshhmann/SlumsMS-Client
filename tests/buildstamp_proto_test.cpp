// Build stamp (CP 0x3738): the client writes exactly the bytes the server's ClientBuildStampHandler reads.
// kGolden is ClientVersionGateTest's golden body in the SlumsMS server: change both together.
#include "../src/buildstamp_proto.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Recorder {
    std::vector<unsigned char> bytes;
    void Encode2(unsigned short n) { bytes.push_back(n & 0xFF); bytes.push_back(n >> 8); }
    void Encode4(unsigned int n) { for (int i = 0; i < 4; ++i) bytes.push_back((n >> (8 * i)) & 0xFF); }
    void EncodeStr(const char* s) {
        size_t n = std::strlen(s);
        Encode2(static_cast<unsigned short>(n));
        bytes.insert(bytes.end(), s, s + n);
    }
};

std::string Hex(const std::vector<unsigned char>& b) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (unsigned char c : b) { out += digits[c >> 4]; out += digits[c & 15]; }
    return out;
}

void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

// format 1, build 10402 (1.4.2), release "1.4.2"
const char* kGolden = "0100a228000005003" "12e342e32";

}  // namespace

int main() {
    try {
        Recorder golden;
        BuildStamp_EncodeValues(golden, 10402, "1.4.2");
        Check(Hex(golden.bytes) == kGolden, "golden stamp bytes: " + Hex(golden.bytes));

        Recorder built;
        BuildStamp_Encode(built);
        Check(built.bytes.size() == 2 + 4 + 2 + std::strlen(SLUMS_RELEASE), "stamp length");
        Check(built.bytes[0] == 1 && built.bytes[1] == 0, "format 1");
        unsigned int build = built.bytes[2] | (built.bytes[3] << 8) | (built.bytes[4] << 16) | (static_cast<unsigned int>(built.bytes[5]) << 24);
        Check(build == static_cast<unsigned int>(SLUMS_BUILD_NUMBER) && build > 0, "build number");
        Check(std::strlen(SLUMS_RELEASE) > 0 && std::strlen(SLUMS_RELEASE) <= 40, "release length the server accepts");
        Check(kBuildStampOpcode == 0x3738, "opcode");
        std::cout << "build stamp " << SLUMS_RELEASE << " (" << SLUMS_BUILD_NUMBER << "): " << Hex(built.bytes) << "\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

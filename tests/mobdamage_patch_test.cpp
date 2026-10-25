// Mob touch damage (mobdamage_patch.h): the low-defence branch site, its patch, and the defence term with and
// without the fix. With a MapleStory.exe argument it also checks the stock bytes and the surrounding instructions
// the cave relies on.
#include "../src/mobdamage_patch.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace MobDamage;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

void CheckFormula() {
    // The report: level 200, 32767 stats, 7 defence, warrior standard 494, snail level 1.
    const int s = static_cast<int>((32767 + 32767) * 0.25 + 32767 / 9.0 + 32767 * 2 / 7.0);
    Check(Reduction(s, 200, 1, 7, 494, false) < -500, "stock: below the standard the term adds hundreds of damage");
    Check(Reduction(s, 200, 1, 7, 494, true) > 0, "fixed: the term only ever reduces damage");
    // Fixed below-standard equals the stock formula with the standard at the character's own defence.
    Check(Reduction(300, 120, 50, 100, 400, true) == Reduction(300, 120, 50, 100, 100, false), "std lowered to PDD");
    // At or above the standard nothing changes.
    for (int pdd : {494, 600, 1000}) {
        Check(Reduction(300, 200, 1, pdd, 494, true) == Reduction(300, 200, 1, pdd, 494, false), "PDD >= std unchanged");
    }
    // Low-level character below a higher-level mob: the 1.3 branch is covered too.
    Check(Reduction(50, 10, 30, 5, 54, false) < Reduction(50, 10, 30, 5, 54, true), "level < mob level also fixed");
    Check(Reduction(50, 10, 30, 5, 54, true) >= 0, "and never negative");
}

void CheckBytes() {
    const auto& orig = OriginalBytes();
    Check(kAddr_Site + 6 + 2 + static_cast<std::int8_t>(orig[7]) == kAddr_BelowStd, "jl goes to the below branches");
    Check(kAddr_Site + kSiteLength == kAddr_Resume, "the jl falls through to the PDD >= std branch");
    const auto patched = PatchedBytes(0x10001000);
    Check(patched[0] == 0xE9 && kAddr_Site + 5 + Le32(&patched[1]) == 0x10001000, "jmp rel32 to the cave");
    Check(Classify(orig.data()) == SiteState::Original, "stock classifies as original");
    Check(Classify(patched.data()) == SiteState::Patched, "patched classifies as patched");
    unsigned char other[kSiteLength] = {0x3B, 0x7D, 0x1C, 0x89, 0x45, 0x10, 0x7D, 0x5E};
    Check(Classify(other) == SiteState::Unknown, "anything else is left alone");
}

struct Exe {
    std::vector<unsigned char> d;
    explicit Exe(const char* path) {
        std::ifstream f(path, std::ios::binary);
        Check(f.good(), "cannot open the exe");
        d.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    // .text of this client has raw offset == VA - image base.
    const unsigned char* At(std::uint32_t va, std::size_t n) const {
        Check(va >= 0x401000 && va - 0x400000 + n <= d.size(), "address out of range");
        return d.data() + (va - 0x400000);
    }
};

void CheckClient(const char* path) {
    const Exe exe(path);
    Check(std::memcmp(exe.At(kAddr_Site, kSiteLength), OriginalBytes().data(), kSiteLength) == 0, "stock site bytes");
    // The cave relies on: [ebp+0x1C] = the standard (stored after the lookup), edi = PDD, and the >= branch reading
    // the defence from [ebp+0x18] and the standard from [ebp+0x1C] only through the (PDD - std) difference.
    const unsigned char* call = exe.At(kAddr_StdCall, 5);
    Check(call[0] == 0xE8 && kAddr_StdCall + 5 + Le32(call + 1) == kAddr_StdLookup, "call to the standard-PDD lookup");
    Check(std::memcmp(exe.At(0x007931C3, 3), "\x89\x45\x1C", 3) == 0, "lookup result stored to [ebp+0x1C]");
    Check(std::memcmp(exe.At(kAddr_PddStore, 3), "\x89\x7D\x18", 3) == 0, "PDD kept in edi and [ebp+0x18]");
    Check(std::memcmp(exe.At(0x007932A4, 3), "\x2B\x7D\x1C", 3) == 0, ">= branch: sub edi,[ebp+0x1C]");
    Check(std::memcmp(exe.At(0x007932E3, 3), "\xDA\x4D\x18", 3) == 0, ">= branch: fimul [ebp+0x18] (PDD)");
    Check(exe.At(kAddr_Function, 1)[0] == 0x55, "function prologue");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        CheckFormula();
        CheckBytes();
        if (argc > 1) {
            CheckClient(argv[1]);
            std::cout << "mobdamage: formula, patch and client bytes OK\n";
        } else {
            std::cout << "mobdamage: formula and patch OK (no exe given)\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "mobdamage_patch_test FAILED: " << e.what() << "\n";
        return 1;
    }
}

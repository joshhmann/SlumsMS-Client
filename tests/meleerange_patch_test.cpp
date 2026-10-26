// Remote melee range (meleerange_patch.h): the GetMeleeAttackRange entry guard. With a MapleStory.exe argument it
// also checks the stock bytes and the call path the analysis rests on.
#include "../src/meleerange_patch.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace MeleeRange;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

std::uint32_t CallTarget(std::uint32_t va, const unsigned char* call) {
    return va + 5 + Le32(call + 1);
}

void CheckBytes() {
    const auto& orig = OriginalBytes();
    Check(CallTarget(kAddr_GetMeleeAttackRange + 5, &orig[5]) == kAddr_EhProlog, "entry calls _EH_prolog");
    Check(Classify(orig.data()) == SiteState::Original, "stock classifies as original");
    unsigned char jmp[kEntryLength] = {0xE9, 0x00, 0x10, 0x00, 0x10, 0xE8, 0x4E, 0xC7, 0x64, 0x00};
    Check(Classify(jmp) == SiteState::Detoured, "a jmp at the entry is someone's detour");
    unsigned char other[kEntryLength] = {0xB8, 0x3C, 0x78, 0xA7, 0x00, 0xE8, 0x4E, 0xC7, 0x64, 0x01};
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
    Check(std::memcmp(exe.At(kAddr_GetMeleeAttackRange, kEntryLength), OriginalBytes().data(), kEntryLength) == 0,
          "stock entry bytes");
    const unsigned char* call = exe.At(kAddr_OnMeleeAttackCall, 5);
    Check(call[0] == 0xE8 && CallTarget(kAddr_OnMeleeAttackCall, call) == kAddr_GetMeleeAttackRange,
          "CUserRemote::OnMeleeAttack calls GetMeleeAttackRange");
    // The slot array: 0xA2 actions of 0x30 bytes, only 0xA20 bytes zeroed (GetWeaponAfterImage 0x0040AC78).
    Check(std::memcmp(exe.At(0x0040ACDA, 5), "\xBE\xA2\x00\x00\x00", 5) == 0, "0xA2 action slots");
    Check(std::memcmp(exe.At(0x0040ACFE, 5), "\x68\x20\x0A\x00\x00", 5) == 0, "0xA20 bytes zeroed");
    // The stock function is __thiscall with three stack arguments: it returns with ret 0xC.
    Check(std::memcmp(exe.At(0x004144D1, 3), "\xC2\x0C\x00", 3) == 0, "ret 0xC");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        CheckBytes();
        if (argc > 1) {
            CheckClient(argv[1]);
            std::cout << "meleerange: guard and client bytes OK\n";
        } else {
            std::cout << "meleerange: guard OK (no exe given)\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "meleerange_patch_test FAILED: " << e.what() << "\n";
        return 1;
    }
}

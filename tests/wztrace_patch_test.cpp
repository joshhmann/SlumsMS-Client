// WZ trace stock bytes (wztrace_patch.h): the two sites the QA trace detours at wztrace=2 must be the stock code before
// anything is hooked. With a MapleStory.exe argument the test also reads the bytes out of the client and compares them,
// including the ret of each function -- that ret is what proves the argument counts the hooks declare.
#include "../src/wztrace_patch.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace WzTracePatch;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::uint32_t Read32(const std::vector<unsigned char>& d, std::size_t at) {
    Check(at + 4 <= d.size(), "pe read out of range");
    return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | (static_cast<std::uint32_t>(d[at + 3]) << 24);
}

std::size_t FileOffset(const std::vector<unsigned char>& d, std::uint32_t va) {
    const std::uint32_t pe = Read32(d, 0x3C);
    Check(Read32(d, pe) == 0x00004550, "not a PE file");
    const unsigned sections = d[pe + 6] | (d[pe + 7] << 8);
    const unsigned optSize = d[pe + 20] | (d[pe + 21] << 8);
    const std::uint32_t imageBase = Read32(d, pe + 24 + 28);
    const std::uint32_t rva = va - imageBase;
    for (unsigned i = 0; i < sections; ++i) {
        const std::size_t sh = pe + 24 + optSize + i * 40;
        const std::uint32_t vsize = Read32(d, sh + 8), vaddr = Read32(d, sh + 12);
        const std::uint32_t rawSize = Read32(d, sh + 16), rawPtr = Read32(d, sh + 20);
        if (rva >= vaddr && rva < vaddr + (vsize > rawSize ? vsize : rawSize)) return rawPtr + (rva - vaddr);
    }
    throw std::runtime_error("address outside every section");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const auto& throwBytes = CxxThrowExceptionBytes();
        const auto& tileBytes = MakeTileBytes();
        Check(throwBytes.size() == kCxxThrowExceptionLength && tileBytes.size() == kMakeTileLength, "a full site each");

        // _CxxThrowException: push ebp / mov ebp, esp / sub esp, 0x20 / mov eax, [ebp+8].
        Check(throwBytes[0] == 0x55 && throwBytes[1] == 0x8B && throwBytes[2] == 0xEC, "_CxxThrowException: push ebp; mov ebp, esp");
        Check(throwBytes[3] == 0x83 && throwBytes[4] == 0xEC && throwBytes[5] == 0x20, "_CxxThrowException: sub esp, 0x20");
        Check(throwBytes[6] == 0x8B && throwBytes[7] == 0x45 && throwBytes[8] == 0x08, "_CxxThrowException reads the object at [ebp+8]");
        // MakeTile: mov eax, imm32 / call SEH setup / sub esp, 0x5C / push ebx; push esi; push edi.
        Check(tileBytes[0] == 0xB8 && tileBytes[5] == 0xE8, "MakeTile: mov eax, imm32; call");
        Check(tileBytes[10] == 0x83 && tileBytes[11] == 0xEC && tileBytes[12] == 0x5C, "MakeTile: sub esp, 0x5C");
        Check(tileBytes[13] == 0x53 && tileBytes[14] == 0x56 && tileBytes[15] == 0x57, "MakeTile: push ebx; push esi; push edi");

        // The guard's decision: stock bytes pass, one changed byte anywhere in the site fails (wztrace.cpp then leaves
        // that detour off and says so in integration.log).
        {
            unsigned char live[kCxxThrowExceptionLength];
            std::memcpy(live, throwBytes.data(), kCxxThrowExceptionLength);
            Check(Matches(live, throwBytes.data(), kCxxThrowExceptionLength), "stock throw bytes match");
            for (std::size_t i = 0; i < kCxxThrowExceptionLength; ++i) {
                live[i] ^= 0xFF;
                Check(!Matches(live, throwBytes.data(), kCxxThrowExceptionLength),
                    "one changed throw byte at " + std::to_string(i) + " fails the check");
                live[i] ^= 0xFF;
            }
            Check(!Matches(nullptr, throwBytes.data(), kCxxThrowExceptionLength), "a missing site fails the check");
        }
        {
            unsigned char live[kMakeTileLength];
            std::memcpy(live, tileBytes.data(), kMakeTileLength);
            Check(Matches(live, tileBytes.data(), kMakeTileLength), "stock MakeTile bytes match");
            for (std::size_t i = 0; i < kMakeTileLength; ++i) {
                live[i] ^= 0xFF;
                Check(!Matches(live, tileBytes.data(), kMakeTileLength),
                    "one changed MakeTile byte at " + std::to_string(i) + " fails the check");
                live[i] ^= 0xFF;
            }
            Check(!Matches(nullptr, tileBytes.data(), kMakeTileLength), "a missing site fails the check");
        }

        if (argc > 1) {
            std::ifstream f(argv[1], std::ios::binary);
            Check(f.good(), "cannot open client exe");
            std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            const std::size_t throwOff = FileOffset(d, kAddr_CxxThrowException);
            const std::size_t tileOff = FileOffset(d, kAddr_MakeTile);
            Check(throwOff + kCxxThrowExceptionLength <= d.size() && tileOff + kMakeTileLength <= d.size(),
                "site past end of file");
            Check(Matches(d.data() + throwOff, throwBytes.data(), kCxxThrowExceptionLength),
                "client exe holds the stock _CxxThrowException prologue");
            Check(Matches(d.data() + tileOff, tileBytes.data(), kMakeTileLength),
                "client exe holds the stock MakeTile prologue");

            // ret 8 closes _CxxThrowException (object, ThrowInfo) and ret 0xC closes MakeTile (this + three dwords).
            const unsigned char throwRet[3] = {0xC2, 0x08, 0x00};
            const unsigned char tileRet[3] = {0xC2, 0x0C, 0x00};
            const std::size_t throwRetOff = FileOffset(d, kAddr_CxxThrowExceptionRet);
            const std::size_t tileRetOff = FileOffset(d, kAddr_MakeTileRet);
            Check(throwRetOff + 3 <= d.size() && tileRetOff + 3 <= d.size(), "ret past end of file");
            Check(Matches(d.data() + throwRetOff, throwRet, 3), "_CxxThrowException ends in ret 8: two arguments");
            Check(Matches(d.data() + tileRetOff, tileRet, 3), "MakeTile ends in ret 0xC: this plus three arguments");
            std::cout << "wztrace_patch: client exe holds both stock sites and both ret shapes\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "wztrace_patch: " << e.what() << '\n';
        return 1;
    }
    std::cout << "wztrace_patch: ok\n";
    return 0;
}

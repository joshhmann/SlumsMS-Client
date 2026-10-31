// QA module stock bytes (qamodule_patch.h): the two addresses the QA client stands on must be the stock code before
// anything is reported from them. With a MapleStory.exe argument the test reads those bytes out of the client and
// compares them with the expected table; without one it checks the table itself and the guards' decisions.
#include "../src/qamodule_patch.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace QaPatch;

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
        const auto& dispatch = ProcessPacketBytes();
        const auto& field = GetCurFieldIDBytes();
        Check(dispatch.size() == kProcessPacketLength && field.size() == kGetCurFieldIDLength, "a full site each");

        // The dispatch prologue: mov eax, imm32 then the SEH call, then the current stage is loaded.
        Check(dispatch[0] == 0xB8 && dispatch[5] == 0xE8, "ProcessPacket: mov eax, imm32; call");
        Check(dispatch[12] == 0xA1, "ProcessPacket loads the current stage through an absolute mov");
        // GetCurFieldID: call get_field, test eax, je out, lea ecx, [eax+0x138].
        Check(field[0] == 0xE8 && field[5] == 0x85 && field[6] == 0xC0, "GetCurFieldID: call; test eax, eax");
        Check(field[7] == 0x74 && field[9] == 0x8D && field[10] == 0x88, "GetCurFieldID: je; lea ecx, [eax+disp32]");

        // The guard's decision: stock bytes pass, a changed byte anywhere in the site fails (the QA module then
        // switches that part off and emits an error event naming it).
        {
            unsigned char live[kProcessPacketLength];
            std::memcpy(live, dispatch.data(), kProcessPacketLength);
            Check(Matches(live, dispatch.data(), kProcessPacketLength), "stock dispatch bytes match");
            for (std::size_t i = 0; i < kProcessPacketLength; ++i) {
                live[i] ^= 0xFF;
                Check(!Matches(live, dispatch.data(), kProcessPacketLength),
                    "one changed byte at " + std::to_string(i) + " fails the check");
                live[i] ^= 0xFF;
            }
            Check(!Matches(nullptr, dispatch.data(), kProcessPacketLength), "a missing site fails the check");
        }

        if (argc > 1) {
            std::ifstream f(argv[1], std::ios::binary);
            Check(f.good(), "cannot open client exe");
            std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            const std::size_t dispatchOff = FileOffset(d, kAddr_ProcessPacket);
            const std::size_t fieldOff = FileOffset(d, kAddr_GetCurFieldID);
            Check(dispatchOff + kProcessPacketLength <= d.size() && fieldOff + kGetCurFieldIDLength <= d.size(),
                "site past end of file");
            Check(Matches(d.data() + dispatchOff, dispatch.data(), kProcessPacketLength),
                "client exe holds the stock ProcessPacket prologue");
            Check(Matches(d.data() + fieldOff, field.data(), kGetCurFieldIDLength), "client exe holds the stock GetCurFieldID");
            std::cout << "qamodule_patch: client exe holds both stock sites\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "qamodule_patch: " << e.what() << '\n';
        return 1;
    }
    std::cout << "qamodule_patch: ok\n";
    return 0;
}

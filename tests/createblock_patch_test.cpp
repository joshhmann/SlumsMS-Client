// Character-creation block (createblock_patch.h): the "high population" je and its jmp.
// With a MapleStory.exe argument it also checks the vanilla bytes in that client, and that the je skips
// exactly the notice call (so the jmp only removes the notice).
#include "../src/createblock_patch.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace CreateBlock;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

std::uint32_t Read32(const std::vector<unsigned char>& d, std::size_t at) {
    Check(at + 4 <= d.size(), "pe read out of range");
    return Le32(d.data() + at);
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

std::uint32_t ShortJumpTarget(std::uint32_t at, const unsigned char* b) {
    return at + 2 + static_cast<std::int8_t>(b[1]);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const auto& orig = OriginalBytes();
        const auto& patched = PatchedBytes();
        const unsigned char* je = orig.data() + kWriteOffset;

        // Layout: the context and world-list loads, the indexed flag compare, the je over the notice.
        Check(orig[0] == 0xA1 && Le32(&orig[1]) == kWvsContext, "mov eax, [CWvsContext]");
        Check(orig[5] == 0x8B && orig[6] == 0x90 && Le32(&orig[7]) == kWorldIdOffset, "mov edx, [eax+0x2054]");
        Check(orig[12] == 0x8B && orig[13] == 0xB1 && Le32(&orig[14]) == kWorldListOffset, "mov esi, [ecx+0x18C]");
        Check(orig[18] == 0xC1 && orig[19] == 0xE2 && orig[20] == 0x05, "world ID * 0x20");
        Check(orig[23] == 0x39 && orig[24] == 0x44 && orig[25] == 0x32 && orig[26] == 0x18, "cmp [edx+esi+0x18], eax");
        Check(kAddr_Je == kAddr_Site + 27 && je[0] == 0x74, "je right after the cmp");
        Check(ShortJumpTarget(kAddr_Je, je) == kAddr_SlotCheck, "je goes to the slot check");
        Check(kAddr_SlotCheck == kAddr_Site + kSiteLength, "je skips to the end of the site");
        Check(orig[29] == 0x50 && orig[30] == 0x6A && orig[31] == kNoticeHighPopulation, "push 0, push notice 36");
        const std::uint32_t callAt = kAddr_Site + 32;
        Check(orig[32] == 0xE8 && callAt + 5 + Le32(&orig[33]) == kAddr_Notice, "call the OK notice");

        // The patch writes exactly one byte: je -> jmp, same displacement.
        Check(kWriteLength == 1, "one byte written");
        for (std::size_t i = 0; i < kSiteLength; ++i) {
            if (i == kWriteOffset) Check(patched[i] == 0xEB, "je became jmp");
            else Check(patched[i] == orig[i], "every other byte untouched");
        }
        Check(ShortJumpTarget(kAddr_Je, patched.data() + kWriteOffset) == kAddr_SlotCheck, "jmp still goes to the slot check");

        // Decisions.
        Check(Classify(orig.data()) == SiteState::Original && ShouldPatch(orig.data()), "patch vanilla bytes");
        Check(Classify(patched.data()) == SiteState::Patched && !ShouldPatch(patched.data()), "already patched");
        unsigned char other[kSiteLength];
        for (auto& b : other) b = 0xCC;
        Check(Classify(other) == SiteState::Unknown && !ShouldPatch(other), "foreign bytes left alone");
        unsigned char hooked[kSiteLength];
        std::memcpy(hooked, orig.data(), kSiteLength);
        hooked[0] = 0xE9;  // someone else's detour at the routine start
        Check(!ShouldPatch(hooked), "a detour at the routine start is left alone");

        if (argc > 1) {
            std::ifstream f(argv[1], std::ios::binary);
            Check(f.good(), "cannot open client exe");
            std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            const std::size_t off = FileOffset(d, kAddr_Site);
            Check(off + kSiteLength <= d.size(), "site past end of file");
            Check(Classify(d.data() + off) == SiteState::Original, "client exe holds the vanilla bytes");
            std::cout << "createblock_patch: client exe holds the vanilla world-flag check\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "createblock_patch: " << e.what() << '\n';
        return 1;
    }
    std::cout << "createblock_patch: ok\n";
    return 0;
}

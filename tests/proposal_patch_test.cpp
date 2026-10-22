// Same-gender marriage (proposal_patch.h): the engagement-box proposal's "male only" jne and its NOP.
// With a MapleStory.exe argument it also checks the vanilla bytes in that client, and that the jne goes
// to the routine's epilogue (so NOPing it only removes the silent return).
#include "../src/proposal_patch.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ProposalGender;

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

std::uint32_t NearJccTarget(std::uint32_t at, const unsigned char* b) {
    Check(b[0] == 0x0F && b[1] >= 0x80 && b[1] <= 0x8F, "not a near jcc");
    const std::int32_t rel = static_cast<std::int32_t>(b[2] | (b[3] << 8) | (b[4] << 16) | (static_cast<std::uint32_t>(b[5]) << 24));
    return at + 6 + rel;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const auto& orig = OriginalBytes();
        const auto& patched = PatchedBytes();

        // Layout: cmp [ebx+0x202C], esi at the site, the jne right after it.
        Check(kAddr_GenderJne == kAddr_GenderCmp + kWriteOffset, "jne follows the 6-byte cmp");
        Check(orig[0] == 0x39 && orig[1] == 0xB3, "cmp r/m32, esi with [ebx+disp32]");
        const std::uint32_t disp = orig[2] | (orig[3] << 8) | (orig[4] << 16) | (static_cast<std::uint32_t>(orig[5]) << 24);
        Check(disp == kGenderOffset, "the cmp reads CWvsContext+0x202C");
        Check(orig[6] == 0x0F && orig[7] == 0x85, "jne rel32");
        Check(NearJccTarget(kAddr_GenderJne, orig.data() + kWriteOffset) == kAddr_Epilogue, "jne goes to the epilogue");
        Check(kAddr_GenderCmp > kAddr_Proposal && kAddr_GenderCmp - kAddr_Proposal < 0x80, "site inside the routine");

        // The patch keeps the cmp and NOPs exactly the jne.
        for (std::size_t i = 0; i < kWriteOffset; ++i) Check(patched[i] == orig[i], "cmp untouched");
        for (std::size_t i = kWriteOffset; i < kSiteLength; ++i) Check(patched[i] == 0x90, "jne NOP'd");
        Check(kWriteOffset + kWriteLength == kSiteLength, "write covers the jne only");

        // Decisions.
        Check(Classify(orig.data()) == SiteState::Original && ShouldPatch(orig.data()), "patch vanilla bytes");
        Check(Classify(patched.data()) == SiteState::Patched && !ShouldPatch(patched.data()), "already patched");
        unsigned char other[kSiteLength];
        for (auto& b : other) b = 0xCC;
        Check(Classify(other) == SiteState::Unknown && !ShouldPatch(other), "foreign bytes left alone");
        unsigned char hooked[kSiteLength];
        std::memcpy(hooked, orig.data(), kSiteLength);
        hooked[0] = 0xE9;  // someone else's jmp over the cmp
        Check(!ShouldPatch(hooked), "a detour over the cmp is left alone");

        if (argc > 1) {
            std::ifstream f(argv[1], std::ios::binary);
            Check(f.good(), "cannot open client exe");
            std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            const std::size_t off = FileOffset(d, kAddr_GenderCmp);
            Check(off + kSiteLength <= d.size(), "site past end of file");
            Check(Classify(d.data() + off) == SiteState::Original, "client exe holds the vanilla bytes");
            std::cout << "proposal_patch: client exe holds the vanilla male-only check\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "proposal_patch: " << e.what() << '\n';
        return 1;
    }
    std::cout << "proposal_patch: ok\n";
    return 0;
}

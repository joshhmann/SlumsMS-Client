// Evan skill gates (evan_patch.h): the stock bytes, and against MapleStory.exe when one is given.
#include "../src/evan_patch.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
// File offset of a virtual address in a PE (section table walk).
long FileOffset(const std::vector<unsigned char>& pe, std::uint32_t va) {
    auto u32 = [&](size_t o) { return pe[o] | pe[o + 1] << 8 | pe[o + 2] << 16 | static_cast<std::uint32_t>(pe[o + 3]) << 24; };
    auto u16 = [&](size_t o) { return static_cast<unsigned>(pe[o] | pe[o + 1] << 8); };
    const size_t nt = u32(0x3C), nsec = u16(nt + 6), opt = u16(nt + 20), sec = nt + 24 + opt;
    const std::uint32_t rva = va - u32(nt + 24 + 28);
    for (size_t i = 0; i < nsec; ++i) {
        const size_t s = sec + i * 40;
        const std::uint32_t vaddr = u32(s + 12), vsize = u32(s + 8), raw = u32(s + 20);
        if (rva >= vaddr && rva < vaddr + vsize) return static_cast<long>(raw + (rva - vaddr));
    }
    return -1;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        Check(sizeof(EvanPatch::kGate1) == 4 && sizeof(EvanPatch::kGate2) == 21, "patch sizes as in the guide");
        Check(EvanPatch::kGate2[11] == 0xD1 && EvanPatch::kGate2[12] == 0x07, "job 2001 check");
        for (const auto& r : EvanPatch::kActionRenames) {
            Check(r.index >= 5490 && r.index <= 5507 && std::strlen(r.evan) < 23, std::string("rename in range: ") + r.evan);
            for (const auto& q : EvanPatch::kActionRenames) Check(&r == &q || (r.index != q.index && std::strcmp(r.evan, q.evan)), "unique");
        }
        if (argc > 1) {
            std::ifstream f(argv[1], std::ios::binary);
            std::vector<unsigned char> pe((std::istreambuf_iterator<char>(f)), {});
            for (const auto& s : EvanPatch::kSites) {
                const long o = FileOffset(pe, static_cast<std::uint32_t>(s.addr));
                Check(o > 0 && std::memcmp(&pe[o], s.stock, s.size) == 0, std::string("stock bytes: ") + s.what);
            }
            std::cout << "evan: sites match " << argv[1] << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "evan: ok\n";
    return 0;
}

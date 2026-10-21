#include "../src/attackstop_patch.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* what) { if (!value) throw std::runtime_error(what); }

std::uint32_t Read32(const std::vector<unsigned char>& d, std::size_t at) {
    require(at + 4 <= d.size(), "pe read out of range");
    return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | (static_cast<std::uint32_t>(d[at + 3]) << 24);
}

// Map a VA to a file offset using the PE section table (unpacked v83 client).
std::size_t FileOffset(const std::vector<unsigned char>& d, std::uint32_t va) {
    const std::uint32_t pe = Read32(d, 0x3C);
    require(Read32(d, pe) == 0x00004550, "not a PE file");
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

// Rel8 / rel32 jump target of a short or near conditional/unconditional jump.
std::uint32_t JumpTarget(std::uint32_t at, const unsigned char* b) {
    if (b[0] == 0xEB || (b[0] >= 0x70 && b[0] <= 0x7F)) return at + 2 + static_cast<std::int8_t>(b[1]);
    if (b[0] == 0x0F && b[1] >= 0x80 && b[1] <= 0x8F) {
        std::int32_t rel = b[2] | (b[3] << 8) | (b[4] << 16) | (b[5] << 24);
        return at + 6 + rel;
    }
    throw std::runtime_error("not a jump");
}
}

int main(int argc, char** argv) {
    using namespace integration;
    const auto& sites = AttackStopSites();
    require(sites.size() == 3, "three moveattack sites");

    // Encodings: the vanilla gates and what MapleRoot's doini writes over them.
    require(JumpTarget(0x0095F97A, sites[0].original.data()) == 0x0095F9A8, "keydown check vanilla jg");
    require(JumpTarget(0x0095F97A, sites[0].moveAttack.data()) == 0x0095F9D5, "keydown check patched to return 1");
    require(sites[1].original[0] == 0x7E && sites[1].moveAttack[0] == 0xEB, "WorkUpdateActive jle -> jmp");
    require(JumpTarget(0x0094C3BB, sites[2].original.data()) == 0x0094C4D3, "Jump vanilla jg to ret path");
    for (std::size_t i = 0; i < 6; ++i) require(sites[2].moveAttack[i] == 0x90, "Jump gate NOP'd");

    // Classification and restore decisions.
    for (const auto& s : sites) {
        require(ClassifyAttackStopSite(s, s.original.data()) == AttackStopState::Original, "original classified");
        require(ClassifyAttackStopSite(s, s.moveAttack.data()) == AttackStopState::MoveAttack, "patched classified");
        require(!ShouldRestoreAttackStopSite(s, s.original.data()), "no write over vanilla");
        require(ShouldRestoreAttackStopSite(s, s.moveAttack.data()), "restore over moveattack");
        const unsigned char other[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
        require(ClassifyAttackStopSite(s, other) == AttackStopState::Unknown, "unknown classified");
        require(!ShouldRestoreAttackStopSite(s, other), "foreign patch left alone");
    }
    require(ShouldApplyMoveAttack(true, false), "moveattack kept when stop off");
    require(!ShouldApplyMoveAttack(true, true), "vanillaattackstop beats moveattack=true");
    require(!ShouldApplyMoveAttack(false, false) && !ShouldApplyMoveAttack(false, true), "moveattack=false");

    // Optional: check the original bytes against an unpacked client exe.
    if (argc > 1) {
        std::ifstream f(argv[1], std::ios::binary);
        require(f.good(), "cannot open client exe");
        std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        for (const auto& s : sites) {
            const std::size_t off = FileOffset(d, s.address);
            require(off + s.length <= d.size(), "site past end of file");
            require(ClassifyAttackStopSite(s, d.data() + off) == AttackStopState::Original, s.name);
        }
        std::cout << "PASS: client exe holds the vanilla bytes at all three sites\n";
    }
    std::cout << "PASS: moveattack site encodings, restore decisions and vanillaattackstop precedence\n";
}

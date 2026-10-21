#include "../src/attackstop_patch.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
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

    // Classification.
    for (const auto& s : sites) {
        require(ClassifyAttackStopSite(s, s.original.data()) == AttackStopState::Original, "original classified");
        require(ClassifyAttackStopSite(s, s.moveAttack.data()) == AttackStopState::MoveAttack, "patched classified");
        const unsigned char other[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
        require(ClassifyAttackStopSite(s, other) == AttackStopState::Unknown, "unknown classified");
    }
    // Site order matches the key table.
    require(sites[static_cast<std::size_t>(MoveAttackSite::ChargeMove)].address == 0x0095F97A, "chargemove site");
    require(sites[static_cast<std::size_t>(MoveAttackSite::AttackWalk)].address == 0x009CBFB0, "attackwalk site");
    require(sites[static_cast<std::size_t>(MoveAttackSite::AttackJump)].address == 0x0094C3BB, "attackjump site");
    require(std::string(MoveAttackSiteKey(MoveAttackSite::AttackWalk)) == "attackwalk", "attackwalk key");
    require(std::string(MoveAttackSiteKey(MoveAttackSite::AttackJump)) == "attackjump", "attackjump key");
    require(std::string(MoveAttackSiteKey(MoveAttackSite::ChargeMove)) == "chargemove", "chargemove key");

    // INI boolean parsing (INIReader::GetBoolean spellings; junk = not set).
    for (const char* t : {"true", "TRUE", "yes", "On", "1"}) require(ParseIniBool(t) == std::optional<bool>(true), t);
    for (const char* f : {"false", "No", "off", "0"}) require(ParseIniBool(f) == std::optional<bool>(false), f);
    require(!ParseIniBool("").has_value() && !ParseIniBool("maybe").has_value(), "absent/junk not set");

    // Precedence table. ini = {moveattack, attackwalk, attackjump, chargemove};
    // expected = {walk, jump, charge}.
    using O = std::optional<bool>;
    const O _ = std::nullopt, T = true, F = false;
    struct Row { MoveAttackIni ini; bool walk, jump, charge; const char* what; };
    const Row table[] = {
        {{_, _, _, _}, false, true, true, "empty [clienthax]: slide off, jump+charge on"},
        {{T, _, _, _}, false, true, true, "legacy moveattack=true: slide off, rest on"},
        {{F, _, _, _}, false, false, false, "legacy moveattack=false: all off"},
        {{F, T, _, _}, true, false, false, "moveattack=false, attackwalk=true"},
        {{F, _, T, _}, false, true, false, "moveattack=false, attackjump=true"},
        {{F, _, _, T}, false, false, true, "moveattack=false, chargemove=true"},
        {{T, _, F, _}, false, false, true, "moveattack=true, attackjump=false"},
        {{T, _, _, F}, false, true, false, "moveattack=true, chargemove=false"},
        {{_, T, T, T}, true, true, true, "all new keys on"},
        {{_, F, F, F}, false, false, false, "all new keys off"},
        {{T, F, T, T}, false, true, true, "shipped config.ini"},
        {{F, T, T, T}, true, true, true, "new keys beat moveattack=false"},
        {{T, F, F, F}, false, false, false, "new keys beat moveattack=true"},
    };
    for (const auto& row : table) {
        const MoveAttackSettings got = ResolveMoveAttackSettings(row.ini);
        require(got.attackWalk == row.walk && got.attackJump == row.jump && got.chargeMove == row.charge, row.what);
        require(got.Enabled(MoveAttackSite::AttackWalk) == row.walk, row.what);
        require(got.Enabled(MoveAttackSite::AttackJump) == row.jump, row.what);
        require(got.Enabled(MoveAttackSite::ChargeMove) == row.charge, row.what);
    }

    // Per-site write decisions.
    for (const auto& s : sites) {
        const unsigned char other[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
        require(DecideMoveAttackWrite(s, s.original.data(), true) == MoveAttackWrite::WriteMoveAttack, "enable vanilla");
        require(DecideMoveAttackWrite(s, s.moveAttack.data(), true) == MoveAttackWrite::None, "already enabled");
        require(DecideMoveAttackWrite(s, s.original.data(), false) == MoveAttackWrite::None, "already vanilla");
        require(DecideMoveAttackWrite(s, s.moveAttack.data(), false) == MoveAttackWrite::WriteOriginal, "restore");
        require(DecideMoveAttackWrite(s, other, true) == MoveAttackWrite::None, "foreign patch kept (on)");
        require(DecideMoveAttackWrite(s, other, false) == MoveAttackWrite::None, "foreign patch kept (off)");
    }

    // End to end on a fake image: each setting touches only its own site.
    for (int mask = 0; mask < 8; ++mask) {
        for (int start = 0; start < 2; ++start) { // all-vanilla, then all-patched (stray ijl15)
            std::map<std::uint32_t, std::vector<unsigned char>> image;
            for (const auto& s : sites) {
                const auto& from = start ? s.moveAttack : s.original;
                image[s.address].assign(from.begin(), from.begin() + s.length);
            }
            const MoveAttackSettings set{(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0};
            ApplyMoveAttackSettings(set,
                [&](std::uint32_t a) { return image.at(a).data(); },
                [&](std::uint32_t a, const unsigned char* b, std::size_t n) { image.at(a).assign(b, b + n); });
            for (std::size_t i = 0; i < sites.size(); ++i) {
                const bool on = set.Enabled(static_cast<MoveAttackSite>(i));
                require(ClassifyAttackStopSite(sites[i], image.at(sites[i].address).data()) ==
                            (on ? AttackStopState::MoveAttack : AttackStopState::Original), sites[i].name);
            }
        }
    }

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
    std::cout << "PASS: moveattack site encodings, per-site write decisions and attackwalk/attackjump/chargemove precedence\n";
}

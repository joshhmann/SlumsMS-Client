// Double-tap jump and bounce block zones (src/bounce_rules.h): decisions, the one static patch's bytes, and the
// LP 0x3737 wire format. The hex strings are the server's BounceZonePacketsTest golden packets: change both together.
#include "../src/bounce_rules.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

std::vector<unsigned char> Hex(const std::string& h) {
    std::vector<unsigned char> out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) out.push_back(static_cast<unsigned char>(std::stoi(h.substr(i, 2), nullptr, 16)));
    return out;
}

// The shipped bounce-zones.yaml (only jump_quests on), native bounce 1050.
const char* kShipped = "3737011a0400001000cae1f505cae1f505a4230506a823050644ab23064dab2306b6c94206bcc9420671aa7f0603ac7f0680d17f0684d17f0606ef1c0d06ef1c0d20c4b01021c4b0101448453614484536809af436809af4360044f8361344f836283d6a3a953d6a3a10416a3a7d416a3af8446a3a65456a3ae0486a3a48496a3af8dc3d3bf9dc3d3b1000cae1f505cae1f505a4230506a823050644ab23064dab2306b6c94206bcc9420671aa7f0603ac7f0680d17f0684d17f0606ef1c0d06ef1c0d20c4b01021c4b0101448453614484536809af436809af4360044f8361344f836283d6a3a953d6a3a10416a3a7d416a3af8446a3a65456a3ae0486a3a48496a3af8dc3d3bf9dc3d3b";
// Two bounce ranges, one Flash Jump range, a Cygnus (10001050).
const char* kSmall = "3737019a9a98000200a4230506a823050620c4b01021c4b0100100a4230506a8230506";

bounce::ZonesSync Decode(const std::string& hex) {
    const auto b = Hex(hex);
    Check(b.size() >= 2 && b[0] == 0x37 && b[1] == 0x37, "opcode 0x3737");
    bounce::ZonesSync z;
    Check(bounce::DecodeZonesSync(b.data() + 2, b.size() - 2, z), "decodes");
    return z;
}

struct Levels {
    std::map<int, int> lv;
    int operator()(int id) const { auto it = lv.find(id); return it == lv.end() ? 0 : it->second; }
};

std::uint32_t Read32(const std::vector<unsigned char>& d, std::size_t at) {
    Check(at + 4 <= d.size(), "read past end");
    return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | (static_cast<std::uint32_t>(d[at + 3]) << 24);
}

std::size_t FileOffset(const std::vector<unsigned char>& d, std::uint32_t va) {
    const std::uint32_t pe = Read32(d, 0x3C);
    Check(Read32(d, pe) == 0x00004550, "not a PE file");
    const unsigned sections = d[pe + 6] | (d[pe + 7] << 8);
    const unsigned optSize = d[pe + 20] | (d[pe + 21] << 8);
    const std::uint32_t rva = va - Read32(d, pe + 24 + 28);
    for (unsigned i = 0; i < sections; ++i) {
        const std::size_t sh = pe + 24 + optSize + i * 40;
        const std::uint32_t vsize = Read32(d, sh + 8), vaddr = Read32(d, sh + 12);
        const std::uint32_t rawSize = Read32(d, sh + 16), rawPtr = Read32(d, sh + 20);
        if (rva >= vaddr && rva < vaddr + (vsize > rawSize ? vsize : rawSize)) return rawPtr + (rva - vaddr);
    }
    throw std::runtime_error("address outside every section");
}

bounce::AirState Air() {
    bounce::AirState a;
    a.hasVecCtrl = true;
    return a;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        using namespace bounce;
        Check(kBounceZonesSyncOpcode == 0x3737, "opcode");

        // ---- wire format ----
        const auto small = Decode(kSmall);
        Check(small.nativeBounce == 10001050, "native bounce");
        Check(small.bounce.size() == 2 && small.bounce[0] == MapRange{101000100, 101000104}
                  && small.bounce[1] == MapRange{280020000, 280020001}, "bounce ranges");
        Check(small.flashJump.size() == 1 && small.flashJump[0] == MapRange{101000100, 101000104}, "fj ranges");
        {
            auto b = Hex(kSmall);
            ZonesSync z;
            z.nativeBounce = 7;
            Check(!DecodeZonesSync(b.data() + 2, b.size() - 3, z) && z.nativeBounce == 7, "short packet refused, out untouched");
            b.push_back(0);
            Check(!DecodeZonesSync(b.data() + 2, b.size() - 2, z), "trailing byte refused");
            b.pop_back();
            b[2] = 2;
            Check(!DecodeZonesSync(b.data() + 2, b.size() - 2, z), "unknown version refused");
            b[2] = 1;
            b[3] = 0x41;  // native bounce no longer a Ninja Bounce id
            Check(!DecodeZonesSync(b.data() + 2, b.size() - 2, z), "bad native bounce refused");
            const auto empty = Hex("3737011a04000000000000");
            Check(DecodeZonesSync(empty.data() + 2, empty.size() - 2, z) && z.bounce.empty() && z.flashJump.empty(), "empty");
        }

        // ---- the built-in list is the shipped jump_quests category ----
        const auto shipped = Decode(kShipped);
        Check(shipped.nativeBounce == 1050, "shipped native");
        const auto builtin = ZoneSet::FromRanges(BuiltinJumpQuestRanges());
        Check(builtin.Ranges() == ZoneSet::FromRanges(shipped.bounce).Ranges(), "built-in list == server bounce set");
        Check(builtin.Ranges() == ZoneSet::FromRanges(shipped.flashJump).Ranges(), "built-in list == server FJ set");
        for (int jq : {100000202, 220000006, 101000100, 101000104, 105040310, 105040316, 103000900, 103000909, 280020000,
                 280020001, 922000000, 910510100, 109030001, 109040004, 980041000, 922240019, 993910008, 993910009}) {
            Check(builtin.Contains(jq), "jump quest in built-in list");
        }
        for (int open : {100000000, 101000000, 103000000, 105040300, 280030000, 922010100, 100000201, 0, -1}) {
            Check(!builtin.Contains(open), "town/boss/PQ not in built-in list");
        }

        // ---- ZoneSet ----
        const auto z = ZoneSet::FromRanges({{30, 40}, {10, 20}, {21, 22}, {50, 49}, {35, 45}});
        Check(z.Ranges().size() == 2 && z.Ranges()[0] == MapRange{10, 22} && z.Ranges()[1] == MapRange{30, 45}, "merge");
        Check(z.Contains(10) && z.Contains(22) && !z.Contains(23) && !z.Contains(9) && z.Contains(45) && !z.Contains(46), "ends");
        Check(z.MapCount() == 13 + 16, "count");

        // ---- skills ----
        for (int id : {1050, 10001050, 20001050, 1054, 10001054, 20001054}) Check(IsUniversalBounce(id), "bounce id");
        Check(!IsUniversalBounce(4111006) && !IsUniversalBounce(14101004) && !IsUniversalBounce(1051), "not bounce");
        MapBlock jq{true, true}, bounceOnly{true, false}, none{};
        Check(ShouldBlockSkill(1050, jq) && ShouldBlockSkill(20001054, jq) && ShouldBlockSkill(4111006, jq), "jq blocks");
        Check(ShouldBlockSkill(1054, bounceOnly) && !ShouldBlockSkill(14101004, bounceOnly), "fj per category");
        Check(!ShouldBlockSkill(1050, none) && !ShouldBlockSkill(2001002, jq), "open map / other skill");

        // ---- airborne ----
        Check(IsAirborne(Air()), "airborne");
        auto a = Air(); a.onFoothold = true; Check(!IsAirborne(a), "on the ground");
        a = Air(); a.onLadder = true; Check(!IsAirborne(a), "ladder");
        a = Air(); a.onRope = true; Check(!IsAirborne(a), "rope");
        a = Air(); a.swimOrFly = true; Check(!IsAirborne(a), "swimming / fly map");
        Check(!IsAirborne(AirState{}), "no CVecCtrl");

        // ---- fresh press vs auto-repeat ----
        Check(IsFreshPress(0x00390001), "first WM_KEYDOWN (space)");
        Check(!IsFreshPress(0x40390001), "auto-repeat");
        Check(!IsFreshPress(0xC0390001), "key up");

        // ---- skill choice ----
        Levels beginner{{{1050, 1}, {10001050, 1}, {20001050, 1}}};
        Check(ChooseDoubleTapSkill(beginner, 1050) == 1050, "adventurer");
        Check(ChooseDoubleTapSkill(beginner, 10001050) == 10001050, "cygnus");
        Check(ChooseDoubleTapSkill(beginner, 20001050) == 20001050, "aran");
        Check(ChooseDoubleTapSkill(beginner, 0) == 1050, "before the server packet");
        Levels hermit{{{1050, 1}, {4111006, 20}}};
        Check(ChooseDoubleTapSkill(hermit, 1050) == 4111006, "real FJ first");
        Levels nw{{{10001050, 1}, {14101004, 10}}};
        Check(ChooseDoubleTapSkill(nw, 10001050) == 14101004, "night walker FJ");
        Levels cygOnly{{{10001050, 1}}};
        Check(ChooseDoubleTapSkill(cygOnly, 1050) == 10001050, "native missing -> any bounce owned");
        Check(ChooseDoubleTapSkill(Levels{}, 1050) == 0, "nothing learned");

        // ---- the decision ----
        DoubleTapInput in;
        in.lParam = 0x00390001;
        in.air = Air();
        Check(DecideDoubleTap(in, beginner, 1050) == 1050, "fresh press in the air bounces");
        in.enabled = false;
        Check(DecideDoubleTap(in, beginner, 1050) == 0, "toggle off");
        in.enabled = true;
        in.lParam = 0x40390001;
        Check(DecideDoubleTap(in, beginner, 1050) == 0, "holding jump never bounces");
        in.lParam = 0x00390001;
        in.air.onFoothold = true;
        Check(DecideDoubleTap(in, beginner, 1050) == 0, "first press on the ground is just a jump");
        in.air = Air();
        in.air.onRope = true;
        Check(DecideDoubleTap(in, beginner, 1050) == 0, "rope");
        in.air = Air();
        in.here = jq;
        Check(DecideDoubleTap(in, beginner, 1050) == 0, "jump quest: no bounce");
        Check(DecideDoubleTap(in, hermit, 1050) == 0, "jump quest: no Flash Jump either");
        in.here = bounceOnly;
        Check(DecideDoubleTap(in, hermit, 1050) == 4111006, "FJ where only bounces are blocked");
        Check(DecideDoubleTap(in, beginner, 1050) == 0, "bounce blocked there");

        // ---- the static patch ----
        Check(kJumpKeyContext + 3 == kJumpKeyCallSite, "context");
        const auto orig = CallRel32(kJumpKeyCallSite, kCUserLocalJump);
        Check(orig[0] == 0xE8 && orig[1] == 0xD7 && orig[2] == 0xCD && orig[3] == 0xFF && orig[4] == 0xFF,
            "rel32 of the stock call reproduces E8 D7 CD FF FF");
        Check(std::equal(orig.begin(), orig.end(), kJumpKeyOriginal.begin() + 3), "original bytes");
        const std::uint32_t thunk = 0x10012340;
        const auto patch = CallRel32(kJumpKeyCallSite, thunk);
        Check(patch[0] == 0xE8 && static_cast<std::uint32_t>(patch[1] | (patch[2] << 8) | (patch[3] << 16)
                  | (static_cast<std::uint32_t>(patch[4]) << 24)) == thunk - (kJumpKeyCallSite + 5), "patched rel32");
        std::array<unsigned char, 8> site = kJumpKeyOriginal;
        Check(ClassifyJumpSite(site.data(), thunk) == JumpSiteState::Original, "classify original");
        std::copy(patch.begin(), patch.end(), site.begin() + 3);
        Check(ClassifyJumpSite(site.data(), thunk) == JumpSiteState::Patched, "classify patched");
        site[0] = 0x90;
        Check(ClassifyJumpSite(site.data(), thunk) == JumpSiteState::Unknown, "classify unknown");
        // MapleRoot's attackjump bytes (0x0094C3BB, inside CUserLocal::Jump) are not ours to touch.
        Check(kJumpKeyCallSite + 5 <= 0x0094C3BB || kJumpKeyCallSite >= 0x0094C3BB + 6, "no overlap with attackjump");

        // Optional: the stock bytes this relies on, in an unpacked client exe (MapleRoot patches it only in memory).
        if (argc > 1) {
            std::ifstream f(argv[1], std::ios::binary);
            Check(f.good(), "cannot open client exe");
            std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            const struct { std::uint32_t va; const char* hex; const char* what; } stock[] = {
                {kJumpKeyContext, "578bcbe8d7cdffff", "OnKeyDown: push edi; mov ecx,ebx; call CUserLocal::Jump"},
                {0x0096BE5E, "e8a9bbacff8b8044010000d1e883e001", "BoundJump: fieldLimit & 0x2 gate"},
                {0x00705309, "5657ffb12c010000", "CVecCtrl ladder test"},
                {0x00705343, "5657ffb12c010000", "CVecCtrl rope test"},
                {0x007046AC, "558bec515157", "CVecCtrl swim/fly test"},
                {0x00A1238B, "e87c56a2ff85c0", "CWvsContext::GetCurFieldID"},
                {0x007616F6, "558bec51894dfc", "CSkillInfo::GetSkillLevel"},
            };
            for (const auto& s : stock) {
                const auto want = Hex(s.hex);
                const std::size_t off = FileOffset(d, s.va);
                Check(off + want.size() <= d.size() && std::equal(want.begin(), want.end(), d.begin() + off), s.what);
            }
            std::cout << "PASS: client exe holds the stock bytes at the jump key call and the helpers used\n";
        }

        std::cout << "PASS: wire format, built-in list == server golden, airborne/fresh-press decisions, skill choice, patch bytes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}

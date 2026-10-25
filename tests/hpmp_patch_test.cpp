// 4-byte HP/MP (hpmp_patch.h): the site table, the patched forms, and with a MapleStory.exe argument every site's
// stock bytes plus the instructions around the decode/tear pairs the patch relies on.
#include "../src/hpmp_patch.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace HpMp;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::string Hex(std::uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof(b), "0x%08X", v);
    return b;
}

const Site* Find(std::uint32_t va) {
    for (const Site& s : kSites) {
        if (s.va == va) return &s;
    }
    return nullptr;
}

void CheckTable() {
    int counts[4] = {};
    for (const Site& s : kSites) {
        ++counts[static_cast<int>(s.kind)];
        Check(s.len >= 3 && s.len <= 5, "site length " + Hex(s.va));
        for (const Site& o : kSites) {
            if (&o != &s) Check(o.va + o.len <= s.va || s.va + s.len <= o.va, "overlapping sites at " + Hex(s.va));
        }
        if (s.kind == Kind::Widen) {
            const bool movsx = s.orig[0] == 0x0F && s.orig[1] == 0xBF;
            const bool prefixed = s.orig[0] == 0x66 && (s.orig[1] == 0x85 || s.orig[1] == 0x3B) && s.orig[2] >> 6 == 3;
            Check(movsx || prefixed, "widen site form " + Hex(s.va));
            if (movsx) {
                const int mod = s.orig[2] >> 6;
                Check((mod == 3 && s.len == 3) || (mod == 1 && (s.orig[2] & 7) == 5 && s.len == 4),
                    "movsx is reg or [ebp+disp8] " + Hex(s.va));
            } else {
                Check(s.len == 3, "66 op modrm " + Hex(s.va));
            }
        }
    }
    Check(counts[static_cast<int>(Kind::Decode)] == 9, "9 decode sites (the wire contract)");
    Check(counts[static_cast<int>(Kind::Tear)] == 17, "17 tear sites");
    Check(counts[static_cast<int>(Kind::Cap)] == 4, "4 caps");
    // The community mod's list patches these, but they truncate other stats: job (+0x39), LUK/INT in GetString,
    // AP (+0x81), equip incHP/incMP in CUIStatDetail. They must stay stock.
    for (std::uint32_t va : {0x008CBF94u, 0x008CC015u, 0x008CC1CFu, 0x008CC24Fu, 0x008CC9DCu, 0x008CCA8Au, 0x008CD9CEu,
             0x008CDAE5u, 0x008CDF25u, 0x008CDFC0u, 0x004E337Bu, 0x004E3380u, 0x00A23C4Au, 0x008C4139u, 0x008C4141u,
             0x008C418Eu, 0x008C419Fu, 0x007656FEu, 0x00A3ECF5u, 0x00A3ED02u, 0x00980656u, 0x00980663u}) {
        Check(Find(va) == nullptr, "not an HP/MP site: " + Hex(va));
    }
}

void CheckForms() {
    unsigned char b[5];
    const Site reg = {0x1000, Kind::Widen, 3, {0x0F, 0xBF, 0xC8}, ""};
    PatchedBytes(reg, 0, b);
    Check(std::memcmp(b, "\x8B\xC8\x90", 3) == 0, "movsx ecx,ax -> mov ecx,eax; nop");
    const Site mem = {0x1000, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xDC}, ""};
    PatchedBytes(mem, 0, b);
    Check(std::memcmp(b, "\x8B\x4D\xDC\x90", 4) == 0, "movsx ecx,word [ebp-0x24] -> mov ecx,dword [ebp-0x24]; nop");
    const Site test = {0x1000, Kind::Widen, 3, {0x66, 0x85, 0xC0}, ""};
    PatchedBytes(test, 0, b);
    Check(std::memcmp(b, "\x85\xC0\x90", 3) == 0, "test ax,ax -> test eax,eax; nop");
    const Site cap = {0x1000, Kind::Cap, 4, {0x30, 0x75, 0x00, 0x00}, ""};
    PatchedBytes(cap, 0, b);
    Check(Le32(b) == 99999, "cap 99,999");
    const Site dec = {0x004E2B9A, Kind::Decode, 5, {}, ""};
    PatchedBytes(dec, 0, b);
    Check(b[0] == 0xE8 && dec.va + 5 + Le32(b + 1) == kAddr_Decode4, "decode -> call Decode4");
    const Site tear = {0x004E2BA4, Kind::Tear, 5, {}, ""};
    PatchedBytes(tear, 0x10001000, b);
    Check(b[0] == 0xE8 && tear.va + 5 + Le32(b + 1) == 0x10001000, "tear -> call FakeTear");
    for (const Site& s : kSites) {
        unsigned char o[5], p[5];
        OriginalBytes(s, o);
        PatchedBytes(s, 0x10001000, p);
        Check(Classify(s, o, 0x10001000) == SiteState::Original, "stock classifies as original " + Hex(s.va));
        Check(Classify(s, p, 0x10001000) == SiteState::Patched, "patched classifies as patched " + Hex(s.va));
        p[0] ^= 0xFF;
        Check(Classify(s, p, 0x10001000) == SiteState::Unknown, "anything else is unknown " + Hex(s.va));
    }
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
    Check(std::memcmp(exe.At(kAddr_FuseShort, kFuseEntryLength), kFuseEntry, kFuseEntryLength) == 0, "Fuse<short> entry");
    Check(std::memcmp(exe.At(kAddr_TearShort, 3), "\x55\x8B\xEC", 3) == 0, "Tear<short> entry");
    Check(exe.At(0x004E8151, 1)[0] == 0xC3, "Tear<short> returns with a plain ret (fastcall, no stack args)");
    for (const Site& s : kSites) {
        unsigned char o[5];
        OriginalBytes(s, o);
        Check(std::memcmp(exe.At(s.va, s.len), o, s.len) == 0, std::string("stock bytes at ") + Hex(s.va) + " " + s.what);
    }
    // Each decode feeds the next tear directly: lea edx,[reg+slot] / mov ecx,eax / call Tear.
    const std::uint32_t pairs[][3] = {{0x004E2B9A, 0x004E2BA4, 0x61}, {0x004E2BAE, 0x004E2BB8, 0x69},
        {0x004E2BC2, 0x004E2BCC, 0x71}, {0x004E2BD6, 0x004E2BE0, 0x79}, {0x0077621A, 0x00776224, 0x61},
        {0x004E30DA, 0x004E30E4, 0x61}, {0x004E30F4, 0x004E30FE, 0x69}, {0x004E310E, 0x004E3118, 0x71},
        {0x004E3128, 0x004E3132, 0x79}};
    for (const auto& p : pairs) {
        const unsigned char* b = exe.At(p[0] + 5, 5);
        Check(b[0] == 0x8D && (b[1] == 0x56 || b[1] == 0x57) && b[2] == p[2] && b[3] == 0x8B && b[4] == 0xC8 &&
                  p[0] + 10 == p[1],
            "decode " + Hex(p[0]) + " feeds the tear of slot +" + Hex(p[2]));
    }
    // The four caps are the 30,000 immediates of mov esi / mov ebx / cmp eax.
    Check(exe.At(0x0078D8D1, 1)[0] == 0xBE && exe.At(0x0077F19F, 1)[0] == 0xBB, "mov esi/ebx, 30000");
    Check(exe.At(0x008CD656, 1)[0] == 0x3D && exe.At(0x008CD6EA, 1)[0] == 0x3D, "cmp eax, 30000");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        CheckTable();
        CheckForms();
        if (argc > 1) {
            CheckClient(argv[1]);
            std::cout << "hpmp: " << kSiteCount << " sites, forms and client bytes OK\n";
        } else {
            std::cout << "hpmp: " << kSiteCount << " sites and forms OK (no exe given)\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "hpmp_patch_test FAILED: " << e.what() << "\n";
        return 1;
    }
}

// ============================================================
// hpmp.cpp: 4-byte HP / MaxHP / MP / MaxMP up to 99,999 (1.6.0). Always on: the 1.6.0 server sends them as 4 bytes.
//
// hpmp_patch.h has the analysis and the site table. AttachHpMpMod checks every site, then patches; HpMp_Verify
// runs after every other mod is installed and checks nothing overwrote a site. Either failure shows a message box
// and the client exits: a client that half-reads 4-byte stats would desync on the first stat packet.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "hpmp_patch.h"

namespace {

auto g_fuseShort = reinterpret_cast<short(__cdecl*)(const void*, unsigned int)>(HpMp::kAddr_FuseShort);

// Replaces _ZtlSecureTear<short> at the HP/MP write sites: the raw value in the slot, checksum 0 for the caller to
// store at +4.
int __fastcall FakeTear(int value, int* slot) {
    *slot = value;
    return 0;
}

// Checksum 0: a raw value from FakeTear. Anything else: the stock short, sign-extended (stock leaves eax's upper
// half undefined, and the widened sites now read all of eax).
int __cdecl FuseShort_hook(const void* slot, unsigned int checksum) {
    if (checksum == 0) return *static_cast<const int*>(slot);
    return g_fuseShort(slot, checksum);
}

std::uint32_t FakeTearAddress() {
    return static_cast<std::uint32_t>(reinterpret_cast<uintptr_t>(&FakeTear));
}

const unsigned char* At(std::uint32_t va) {
    return reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(va));
}

[[noreturn]] void RefuseToStart(const char* problem, std::uint32_t va, const char* what) {
    char text[512];
    sprintf_s(text,
        "SlumsMS cannot start: the 4-byte HP/MP patch %s at 0x%08X (%s).\n\n"
        "This client would misread HP and MP from the server. Repair or reinstall the game with the launcher.",
        problem, va, what);
    DebugMessage("HP/MP: %s at 0x%08X (%s); refusing to start", problem, va, what);
    MessageBoxA(nullptr, text, "SlumsMS", MB_OK | MB_ICONERROR | MB_TOPMOST);
    ExitProcess(1);
}

}  // namespace

void AttachHpMpMod() {
    using namespace HpMp;
    // Check everything first so a mismatch patches nothing.
    if (std::memcmp(At(kAddr_FuseShort), kFuseEntry, kFuseEntryLength) != 0) {
        RefuseToStart("found unexpected bytes (another hook?)", kAddr_FuseShort, "_ZtlSecureFuse<short>");
    }
    for (const Site& s : kSites) {
        if (Classify(s, At(s.va), FakeTearAddress()) != SiteState::Original) {
            RefuseToStart("found unexpected bytes", s.va, s.what);
        }
    }
    if (!ATTACH_HOOK(g_fuseShort, FuseShort_hook)) {
        RefuseToStart("could not hook", kAddr_FuseShort, "_ZtlSecureFuse<short>");
    }
    for (const Site& s : kSites) {
        unsigned char b[5];
        PatchedBytes(s, FakeTearAddress(), b);
        PatchMemory(reinterpret_cast<void*>(static_cast<uintptr_t>(s.va)), b, s.len);
    }
    DebugMessage("HP/MP: 4-byte HP/MP up to %d (%u sites)", kMaxValue, static_cast<unsigned>(kSiteCount));
}

void HpMp_Verify() {
    using namespace HpMp;
    if (At(kAddr_FuseShort)[0] != 0xE9) RefuseToStart("lost its hook", kAddr_FuseShort, "_ZtlSecureFuse<short>");
    for (const Site& s : kSites) {
        if (Classify(s, At(s.va), FakeTearAddress()) != SiteState::Patched) {
            RefuseToStart("was overwritten by another patch", s.va, s.what);
        }
    }
    DebugMessage("HP/MP: all sites still patched after the other mods");
}

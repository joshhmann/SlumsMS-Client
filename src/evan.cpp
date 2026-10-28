// ============================================================
// evan.cpp -- turn on the v83 client's own Evan skill paths (MAPLEROOT_EVAN). See evan_patch.h.
// Each site is checked before it is written; a mismatch leaves the client stock and says so.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "evan_patch.h"

#include <cstring>
#include <iterator>
#include <vector>

void EncodeString(int nIdx, const char* sSource, char* sDestination);   // stringpool.cpp

void AttachEvanMod() {
    for (const auto& s : EvanPatch::kSites) {
        if (std::memcmp(reinterpret_cast<const void*>(s.addr), s.stock, s.size) != 0) {
            DebugMessage("Evan: %s at %08X does not match the stock bytes; Evan skills stay off", s.what,
                static_cast<unsigned>(s.addr));
            return;
        }
    }
    for (const auto& s : EvanPatch::kSites) {
        std::vector<unsigned char> nops(s.size, 0x90);
        PatchMemory(reinterpret_cast<void*>(s.addr), nops.data(), nops.size());
    }
    DebugMessage("Evan: skill gates opened (%u sites)", static_cast<unsigned>(std::size(EvanPatch::kSites)));
    // Action names the Evan data uses (evan_patch.h kActionRenames). The encoded strings must outlive the client.
    static char s_encoded[std::size(EvanPatch::kActionRenames)][24];
    size_t i = 0;
    for (const auto& r : EvanPatch::kActionRenames) EncodeString(r.index, r.evan, s_encoded[i++]);
    DebugMessage("Evan: %u action names set from the Evan data", static_cast<unsigned>(i));
}

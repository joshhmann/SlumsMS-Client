// ============================================================
// midair.cpp: start keydown attacks in the air ([clienthax] jumpkeydown, default off; TEST only for now, ticket 126).
// See midair_patch.h. MapleRoot's jumpshoot/jumpmagic keep handling bows/crossbows and magic attacks.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "constants.h"
#include "midair_patch.h"

#include <cstring>

void AttachMidAirMod() {
    char value[16] = {};
    GetPrivateProfileStringA("clienthax", "jumpkeydown", "", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    if (_stricmp(value, "true") != 0 && strcmp(value, "1") != 0) return;
    const auto& stock = MidAir::kKeydownAirExitStock;
    if (memcmp(reinterpret_cast<const void*>(MidAir::kKeydownAirExit), stock.data(), stock.size()) != 0) {
        DebugMessage("Mid-air: the keydown gate is not the stock code; jumpkeydown stays off");
        return;
    }
    unsigned char nops[6];
    memset(nops, 0x90, sizeof(nops));
    PatchMemory(reinterpret_cast<void*>(MidAir::kKeydownAirExit), nops, sizeof(nops));
    DebugMessage("Mid-air: keydown attacks may start in the air ([clienthax] jumpkeydown)");
}

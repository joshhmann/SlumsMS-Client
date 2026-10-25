// ============================================================
// mobdamage.cpp: low defence never adds damage when a mob touches you (1.6.0).
//
// Stock v83 adds damage when your defence is below StandardPDD.img's value for your job and level, scaled by your
// stats; admins with huge stats took ~900 from snails after !job. mobdamage_patch.h has the analysis. This routes the
// branch at 0x0079328D through a cave that treats a below-standard defence as the standard.
// config.ini [clienthax] lowdefensepenalty=true keeps the stock rule.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "constants.h"
#include "attackstop_patch.h"
#include "mobdamage_patch.h"

namespace {

uintptr_t g_resume = MobDamage::kAddr_Resume;   // not const: the cave jumps through it

// Replaces cmp edi,[ebp+0x1C] / mov [ebp+0x10],eax / jl: edi = the character's PDD, [ebp+0x1C] = the standard PDD,
// eax = the stat term. Falls through to the PDD >= std branch with std lowered to PDD when it was above.
__declspec(naked) void BelowStandardCave() {
    __asm {
        mov dword ptr [ebp + 0x10], eax
        cmp edi, dword ptr [ebp + 0x1C]
        jge resume
        mov dword ptr [ebp + 0x1C], edi
    resume:
        jmp [g_resume]
    }
}

bool KeepStockRule() {
    char value[32] = {};
    GetPrivateProfileStringA("clienthax", "lowdefensepenalty", "", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    return integration::ParseIniBool(value).value_or(false);
}

}  // namespace

void AttachMobDamageMod() {
    using namespace MobDamage;
    if (KeepStockRule()) {
        DebugMessage("Mob damage: stock low-defence penalty kept ([clienthax] lowdefensepenalty=true)");
        return;
    }
    const auto* site = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(kAddr_Site));
    switch (Classify(site)) {
    case SiteState::Patched:
        DebugMessage("Mob damage: already patched");
        return;
    case SiteState::Unknown:
        DebugMessage("Mob damage: unexpected client bytes at 0x%08X; stock rule kept", kAddr_Site);
        return;
    default:
        break;
    }
    auto bytes = PatchedBytes(static_cast<std::uint32_t>(reinterpret_cast<uintptr_t>(&BelowStandardCave)));
    PatchMemory(reinterpret_cast<void*>(static_cast<uintptr_t>(kAddr_Site)), bytes.data(), bytes.size());
    DebugMessage("Mob damage: defence below the job standard no longer adds damage");
}

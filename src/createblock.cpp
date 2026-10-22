// ============================================================
// createblock.cpp: never show the random "high population" block on character creation (1.5.0).
//
// Stock v83 reads the selected world's bBlockCharCreation by using the world ID as a list position;
// with live's single world ID 1 that reads stale heap memory. createblock_patch.h has the analysis.
// This turns that one je into a jmp after checking the bytes around it.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "createblock_patch.h"

void AttachCreateBlockMod() {
    using namespace CreateBlock;
    const auto* site = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(kAddr_Site));
    switch (Classify(site)) {
    case SiteState::Patched:
        DebugMessage("Create block: already skipped");
        return;
    case SiteState::Unknown:
        DebugMessage("Create block: unexpected client bytes at 0x%08X; stock check kept", kAddr_Site);
        return;
    default:
        break;
    }
    PatchMemory(reinterpret_cast<void*>(static_cast<uintptr_t>(kAddr_Site + kWriteOffset)),
        const_cast<unsigned char*>(PatchedBytes().data() + kWriteOffset), kWriteLength);
    DebugMessage("Create block: character creation is never blocked by the world-list flag");
}

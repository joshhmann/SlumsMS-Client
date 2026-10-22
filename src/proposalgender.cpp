// ============================================================
// proposalgender.cpp: let any character open the engagement proposal (1.5.0, same-gender marriage).
//
// Stock v83 returns silently from the engagement-box proposal unless the character is male;
// proposal_patch.h has the analysis. This NOPs that one jne after checking the bytes around it.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "proposal_patch.h"

void AttachProposalGenderMod() {
    using namespace ProposalGender;
    const auto* site = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(kAddr_GenderCmp));
    switch (Classify(site)) {
    case SiteState::Patched:
        DebugMessage("Proposal gender: already open to every gender");
        return;
    case SiteState::Unknown:
        DebugMessage("Proposal gender: unexpected client bytes at 0x%08X; only men can propose", kAddr_GenderCmp);
        return;
    default:
        break;
    }
    PatchMemory(reinterpret_cast<void*>(static_cast<uintptr_t>(kAddr_GenderCmp + kWriteOffset)),
        const_cast<unsigned char*>(PatchedBytes().data() + kWriteOffset), kWriteLength);
    DebugMessage("Proposal gender: any character can use an engagement ring box");
}

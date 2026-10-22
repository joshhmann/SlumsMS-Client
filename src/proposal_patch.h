#pragma once
// ============================================================
// proposal_patch.h -- let a female character propose (1.5.0, ticket 58: same-gender marriage).
//
// Header-only so tests/proposal_patch_test.cpp can check the bytes without the client;
// proposalgender.cpp installs it.
//
// THE CHECK. Using an engagement ring box (2240000-2240003, item type 224) goes through the
// consume-item dispatcher (call at 0x004F07AC) to the proposal routine at 0x00A0FC6C, which:
//   1. refuses with "Your ETC slot is full." (StringPool 0x109F) when the Etc tab has no free slot;
//   2. 0x00A0FCBB  cmp [CWvsContext+0x202C], esi(0)
//      0x00A0FCC1  jne 0x00A0FE76 (the epilogue)      <- returns SILENTLY unless the character is male
//   3. refuses with "You are already married/engaged." (0x1095 / 0x1094) from the marriage record;
//   4. asks "Please enter your partner's name." (0x10A0) and sends RING_ACTION (0x89) mode 0
//      (name, item id).
// CWvsContext+0x202C is the character's gender: SET_GENDER (0x3A, handler 0x00A23D79, which Cosmic sends
// on every channel login) stores the decoded byte there. So stock v83 only lets a man propose: a woman
// using the box gets no window and no message. That blocks any couple of two women outright, and a woman
// proposing to a man. Nothing else on the proposal path reads a gender: the partner is a typed name that
// the client never resolves, and the receiving side (MARRIAGE_REQUEST 0x48, handler 0x00A29886) only
// shows "%s has requested engagement" and answers mode 2.
//
// THE FIX. NOP the 6-byte jne, so step 2 falls through for every gender. The server (RingActionHandler)
// already accepts a proposal from anyone. Every byte around the site is checked first; on any mismatch
// nothing is patched and the client keeps the stock behaviour.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ProposalGender {

constexpr std::uint32_t kAddr_Proposal = 0x00A0FC6C;     // the engagement-box proposal routine
constexpr std::uint32_t kAddr_GenderCmp = 0x00A0FCBB;    // cmp dword ptr [ebx+0x202C], esi
constexpr std::uint32_t kAddr_GenderJne = 0x00A0FCC1;    // jne 0x00A0FE76
constexpr std::uint32_t kAddr_Epilogue = 0x00A0FE76;     // where the jne goes: return, nothing shown
constexpr std::uint32_t kGenderOffset = 0x202C;          // CWvsContext gender, set by SET_GENDER (0x3A)
constexpr std::size_t kSiteLength = 12;                  // cmp (6) + jne (6)

// The vanilla bytes at kAddr_GenderCmp, and what the patch leaves there.
inline const std::array<unsigned char, kSiteLength>& OriginalBytes() {
    static const std::array<unsigned char, kSiteLength> bytes = {{
        0x39, 0xB3, 0x2C, 0x20, 0x00, 0x00,  // cmp dword ptr [ebx+0x202C], esi
        0x0F, 0x85, 0xAF, 0x01, 0x00, 0x00,  // jne 0x00A0FE76
    }};
    return bytes;
}

inline const std::array<unsigned char, kSiteLength>& PatchedBytes() {
    static const std::array<unsigned char, kSiteLength> bytes = {{
        0x39, 0xB3, 0x2C, 0x20, 0x00, 0x00,  // cmp kept (flags are dead: the next instruction re-tests)
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    }};
    return bytes;
}

// Only the jne is written.
constexpr std::size_t kWriteOffset = 6;
constexpr std::size_t kWriteLength = 6;

enum class SiteState { Original, Patched, Unknown };

inline SiteState Classify(const unsigned char* current) {
    if (std::memcmp(current, OriginalBytes().data(), kSiteLength) == 0) return SiteState::Original;
    if (std::memcmp(current, PatchedBytes().data(), kSiteLength) == 0) return SiteState::Patched;
    return SiteState::Unknown;
}

// Patch only over the exact vanilla bytes; leave anything else alone.
inline bool ShouldPatch(const unsigned char* current) { return Classify(current) == SiteState::Original; }

}  // namespace ProposalGender

#pragma once
// ============================================================
// createblock_patch.h -- stop the random "high population" block on character creation (1.5.0).
//
// Header-only so tests/createblock_patch_test.cpp can check the bytes without the client;
// createblock.cpp installs it.
//
// THE CHECK. Clicking "create character" runs 0x005F6425 (called from 0x006016A3):
//   0x005F6425  mov eax, [0x00BE7918]          ; CWvsContext
//   0x005F642A  mov edx, [eax+0x2054]          ; the selected world's ID (set at 0x00A02DDE from 0x005F6E86)
//   0x005F6430  push esi
//   0x005F6431  mov esi, [ecx+0x18C]           ; CLogin world list, 0x20 bytes per world
//   0x005F6437  shl edx, 5
//   0x005F643A  xor eax, eax
//   0x005F643C  cmp [edx+esi+0x18], eax        ; that world's bBlockCharCreation
//   0x005F6440  je  0x005F644C                 ; 0: go on to the slot check
//   0x005F6442  push eax / push 0x24 / call 0x0060F024   ; Notice/text/36: "Due to the high population,
//               character creation has temporarily been disabled in this world. ..."
// The world list is filled by the SERVERLIST handler (0x005F95B7) in packet order, and every other lookup
// searches it for the world's ID. This one uses the ID as a list position. Live runs a single world with
// ID 1 (Bera), so the list has one entry and the check reads past it: the last 4 bytes of the 0x40-byte
// ZAllocEx block, which the world entry never writes. Whatever was left there decides it, so creation is
// blocked at random. Channel loads play no part. The server always sends bBlockCharCreation = 0.
//
// THE FIX. Turn the je into a jmp (74 0A -> EB 0A), so creation always goes on to the slot check, which is
// what the server's 0 asks for anyway. Every byte around the site is checked first; on any mismatch
// nothing is patched and the client keeps the stock behaviour.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace CreateBlock {

constexpr std::uint32_t kAddr_Site = 0x005F6425;          // start of the "create character" click routine
constexpr std::uint32_t kAddr_Je = 0x005F6440;            // je 0x005F644C over the notice
constexpr std::uint32_t kAddr_SlotCheck = 0x005F644C;     // where the je goes: the character-slot check
constexpr std::uint32_t kAddr_Notice = 0x0060F024;        // the OK notice (Notice/text/N)
constexpr std::uint32_t kWvsContext = 0x00BE7918;
constexpr std::uint32_t kWorldIdOffset = 0x2054;
constexpr std::uint32_t kWorldListOffset = 0x18C;
constexpr unsigned char kNoticeHighPopulation = 0x24;     // Notice/text/36
constexpr std::size_t kSiteLength = 39;                   // 0x005F6425 .. 0x005F644C

// The vanilla bytes at kAddr_Site, and what the patch leaves there.
inline const std::array<unsigned char, kSiteLength>& OriginalBytes() {
    static const std::array<unsigned char, kSiteLength> bytes = {{
        0xA1, 0x18, 0x79, 0xBE, 0x00,              // mov eax, [0x00BE7918]
        0x8B, 0x90, 0x54, 0x20, 0x00, 0x00,        // mov edx, [eax+0x2054]
        0x56,                                      // push esi
        0x8B, 0xB1, 0x8C, 0x01, 0x00, 0x00,        // mov esi, [ecx+0x18C]
        0xC1, 0xE2, 0x05,                          // shl edx, 5
        0x33, 0xC0,                                // xor eax, eax
        0x39, 0x44, 0x32, 0x18,                    // cmp [edx+esi+0x18], eax
        0x74, 0x0A,                                // je 0x005F644C
        0x50,                                      // push eax
        0x6A, 0x24,                                // push 0x24
        0xE8, 0xDA, 0x8B, 0x01, 0x00,              // call 0x0060F024
        0xEB, 0x29,                                // jmp 0x005F6475
    }};
    return bytes;
}

inline const std::array<unsigned char, kSiteLength>& PatchedBytes() {
    static const std::array<unsigned char, kSiteLength> bytes = [] {
        std::array<unsigned char, kSiteLength> b = OriginalBytes();
        b[kAddr_Je - kAddr_Site] = 0xEB;          // jmp 0x005F644C
        return b;
    }();
    return bytes;
}

// Only the je's opcode byte is written.
constexpr std::size_t kWriteOffset = kAddr_Je - kAddr_Site;
constexpr std::size_t kWriteLength = 1;

enum class SiteState { Original, Patched, Unknown };

inline SiteState Classify(const unsigned char* current) {
    if (std::memcmp(current, OriginalBytes().data(), kSiteLength) == 0) return SiteState::Original;
    if (std::memcmp(current, PatchedBytes().data(), kSiteLength) == 0) return SiteState::Patched;
    return SiteState::Unknown;
}

// Patch only over the exact vanilla bytes; leave anything else alone.
inline bool ShouldPatch(const unsigned char* current) { return Classify(current) == SiteState::Original; }

}  // namespace CreateBlock

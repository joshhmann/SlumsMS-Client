#pragma once
// ============================================================
// qamodule_patch.h -- the stock bytes the QA module (ticket 170) stands on. Header-only, no Windows, so
// tests/qamodule_patch_test.cpp can check them against a copy of the client without a build.
//
// The QA module writes no patches of its own: it rides the existing packet detour and reads the client's own
// window list. Two addresses carry the whole feature, and both are checked before anything is used:
//   ProcessPacket        0x004965F1  every server packet passes here; the chat lines and ready signals come from it
//   GetCurFieldID        0x00A1238B  the map the client is in, read when field.ready fires
// qamodule.cpp compares the live bytes at both addresses with OriginalBytes() at startup; on any mismatch it
// leaves that part switched off and emits one "error" event naming the site, so a different client build gives
// you a quiet, readable failure instead of wrong signals.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace QaPatch {

constexpr std::uint32_t kAddr_ProcessPacket = 0x004965F1;   // CClientSocket::ProcessPacket (the packet dispatcher)
constexpr std::uint32_t kAddr_GetCurFieldID = 0x00A1238B;   // CWvsContext::GetCurFieldID thunk (ignores this)

constexpr std::size_t kProcessPacketLength = 17;            // mov eax,imm32 / call rel32 / push ecx; push ecx / mov eax,[abs32]
constexpr std::size_t kGetCurFieldIDLength = 16;            // call rel32 / test eax,eax / je / lea ecx,[eax+disp32]

// mov eax, 0xA812B0 / call SEH: the dispatcher's prologue.
inline const std::array<unsigned char, kProcessPacketLength>& ProcessPacketBytes() {
    static const std::array<unsigned char, kProcessPacketLength> bytes = {{
        0xB8, 0xB0, 0x12, 0xA8, 0x00,  // mov eax, 0xA812B0
        0xE8, 0x9D, 0xA5, 0x5C, 0x00,  // call 0x00A60B98 (SEH setup)
        0x51, 0x51,                    // push ecx; push ecx
        0xA1, 0xD4, 0xDE, 0xBE, 0x00,  // mov eax, dword ptr [0x00BEDED4] (the current stage)
    }};
    return bytes;
}

// call rel32 / test eax, eax / je +0x0C / lea ecx, [eax+0x138]: the thunk returns the field id or 0.
inline const std::array<unsigned char, kGetCurFieldIDLength>& GetCurFieldIDBytes() {
    static const std::array<unsigned char, kGetCurFieldIDLength> bytes = {{
        0xE8, 0x7C, 0x56, 0xA2, 0xFF,  // call 0x00437A0C (get_field)
        0x85, 0xC0,                    // test eax, eax
        0x74, 0x0C,                    // je +0x0C (no field: return 0)
        0x8D, 0x88, 0x38, 0x01, 0x00, 0x00,  // lea ecx, [eax+0x138] (6 bytes)
        0xE8,
    }};
    return bytes;
}

inline bool Matches(const unsigned char* live, const unsigned char* expected, std::size_t length) {
    return live && expected && std::memcmp(live, expected, length) == 0;
}

}  // namespace QaPatch

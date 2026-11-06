#pragma once
// ============================================================
// qamodule_patch.h -- the stock bytes the QA module (ticket 170) stands on. Header-only, no Windows, so
// tests/qamodule_patch_test.cpp can check them against a copy of the client without a build.
//
// The QA module writes no patches of its own: it rides the existing packet detour and reads the client's own
// window list. Two addresses carry the signals, and every address below is checked before anything is used:
//   ProcessPacket        0x004965F1  every server packet passes here; the chat lines and ready signals come from it
//   GetCurFieldID        0x00A1238B  the map the client is in, read when field.ready fires
// P1-Q2 (Zakum run): three of the client's own request senders, called as the item and party windows call them.
//   SendChangeSlotPositionRequest 0x00A0900A  CWvsContext; (type, src, dst, count), ret 0x10. dst 0 drops: ITEM_MOVE
//                                             0x47 [tick][type][src][dst][count], after the client's 500 ms throttle
//   SendCreatePartyRequest        0x0052FCE1  no arguments (reads CWvsContext 0x00BE7918 itself): PARTY_OPERATION 0x7C [1]
//   SendInvitePartyRequest        0x0052FECF  (const ZXString<char>* name), ret 4; callers pass get_field() in ecx and
//                                             it never reads it. Refuses yourself or a full party, else 0x7C [4][name]
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

constexpr std::uint32_t kAddr_SendChangeSlotPositionRequest = 0x00A0900A;
constexpr std::uint32_t kAddr_SendCreatePartyRequest = 0x0052FCE1;
constexpr std::uint32_t kAddr_SendInvitePartyRequest = 0x0052FECF;
constexpr std::uint32_t kAddr_ClientSocket = 0x00BE7914;            // TSingleton<CClientSocket>::ms_pInstance
constexpr std::uint32_t kAddr_ClientSocket_SendPacket = 0x0049637B;  // CClientSocket::SendPacket (dailycheckin.cpp)
constexpr std::uint32_t kAddr_GetField = 0x00437A0C;
constexpr std::uint32_t kVtable_EntrustedShopDlg = 0x00AF3928;   // the hired merchant owner window (built ~0x00517ECA)
constexpr std::uint32_t kOff_ItemCashSN = 0x18;
constexpr std::uint32_t kAddr_CDialog_EndDialog = 0x004244B1;   // sets m_nRet / m_bTerminate (and hides a modeless one)                  // GW_ItemSlotBase::liCashItemSN (0x0046D341 reads it)          // get_field: the current CField or null
constexpr std::uint32_t kAddr_CWvsContext = 0x00BE7918;       // TSingleton<CWvsContext>::ms_pInstance
constexpr std::uint32_t kOff_CharacterData = 0x20B8;          // CWvsContext -> GW_CharacterData* (the invite sender reads it)
constexpr std::uint32_t kOff_ItemTabs = 0x447;                // GW_CharacterData + 0x447 + 4*type -> ZArray<ZRef<item>> (invresize)
constexpr std::uint32_t kOff_ItemId = 0xC;                    // GW_ItemSlotBase::nItemID, a TSecType<long> (coloringprism.cpp)
constexpr std::uint32_t kAddr_TSecTypeLong_GetData = 0x0042873D;  // TSecType<long>::GetData: the plaintext id

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

constexpr std::size_t kSendChangeSlotLength = 17;   // mov eax,imm32 / call SEH / sub esp,0x14 / push esi; push edi / mov edi,ecx
constexpr std::size_t kSendPartyLength = 21;        // mov eax,imm32 / call SEH / sub esp,0x1C / pushes / mov edi,[0xBE7918]

inline const std::array<unsigned char, kSendChangeSlotLength>& SendChangeSlotBytes() {
    static const std::array<unsigned char, kSendChangeSlotLength> bytes = {{
        0xB8, 0x94, 0x9B, 0xAE, 0x00,  // mov eax, 0xAE9B94
        0xE8, 0x84, 0x7B, 0x05, 0x00,  // call 0x00A60B98 (SEH setup)
        0x83, 0xEC, 0x14,              // sub esp, 0x14
        0x56, 0x57,                    // push esi; push edi
        0x8B, 0xF9,                    // mov edi, ecx (this = CWvsContext)
    }};
    return bytes;
}

// The create sender pushes ebx too, so its context load sits one byte later; both are compared over 21 bytes.
inline const std::array<unsigned char, kSendPartyLength>& SendCreatePartyBytes() {
    static const std::array<unsigned char, kSendPartyLength> bytes = {{
        0xB8, 0x48, 0xB3, 0xA8, 0x00,  // mov eax, 0xA8B348
        0xE8, 0xAD, 0x0E, 0x53, 0x00,  // call 0x00A60B98 (SEH setup)
        0x83, 0xEC, 0x1C,              // sub esp, 0x1C
        0x53, 0x56, 0x57,              // push ebx; push esi; push edi
        0x8B, 0x3D, 0x18, 0x79, 0xBE,  // mov edi, dword ptr [0x00BE7918] (first 5 of 6 bytes)
    }};
    return bytes;
}

inline const std::array<unsigned char, kSendPartyLength>& SendInvitePartyBytes() {
    static const std::array<unsigned char, kSendPartyLength> bytes = {{
        0xB8, 0xA0, 0xB3, 0xA8, 0x00,  // mov eax, 0xA8B3A0
        0xE8, 0xBF, 0x0C, 0x53, 0x00,  // call 0x00A60B98 (SEH setup)
        0x83, 0xEC, 0x1C,              // sub esp, 0x1C
        0x56, 0x57,                    // push esi; push edi
        0x8B, 0x3D, 0x18, 0x79, 0xBE, 0x00,  // mov edi, dword ptr [0x00BE7918]
    }};
    return bytes;
}

inline bool Matches(const unsigned char* live, const unsigned char* expected, std::size_t length) {
    return live && expected && std::memcmp(live, expected, length) == 0;
}

}  // namespace QaPatch

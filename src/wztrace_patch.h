#pragma once
// ============================================================
// wztrace_patch.h -- the stock bytes the WZ lookup trace (ticket b251) stands on. Header-only, no Windows, so
// tests/wztrace_patch_test.cpp can check them against a copy of the client without a build.
//
// wztrace.cpp detours two sites and only after the live bytes match OriginalBytes() here; on a mismatch it leaves
// that part switched off and writes one line saying which site, so another client build fails quietly instead of
// hooking the wrong function.
//   _CxxThrowException  0x00A60BB7  every C++ throw passes here (crash_diagnostics.cpp already reads its frames);
//                                   the hook logs the caller and the ring, then calls the original
//   MakeTile            0x0063A5C4  CMapLoadable's tile builder, the call before a Map/Tile lookup fails (field
//                                   research-119-120.md). NOT MakeObj (lamps.cpp owns 0x0063AD16) and NOT MakeBack
//                                   (weather.cpp owns 0x0063CD4E): a second detour on a site another mod owns is a
//                                   chain nobody tests
// Both are verified against MapleStory.exe sha256
// 86e4e6be050caeda4a93443237256a84376ea74781fcba558d2418f0568aac90, image base 0x400000, file offset = VA - 0x400000
// (objdump -d -M intel on the stock exe; the ret bytes below are the end of each function, read to prove the
// argument count the hooks declare).
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (that sha256, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace WzTracePatch {

constexpr std::uint32_t kAddr_CxxThrowException = 0x00A60BB7;   // void __cdecl _CxxThrowException(void*, ThrowInfo*)
constexpr std::uint32_t kAddr_MakeTile = 0x0063A5C4;            // void* __thiscall CMapLoadable::MakeTile(this, a0, a1, a2)

constexpr std::size_t kCxxThrowExceptionLength = 14;   // push ebp / mov ebp,esp / sub esp,0x20 / mov eax,[ebp+8] / push esi;push edi / push 8 / pop ecx
constexpr std::size_t kMakeTileLength = 16;            // mov eax,0xA9D448 / call rel32 (SEH setup) / sub esp,0x5C / push ebx;push esi;push edi

// push ebp / mov ebp, esp / sub esp, 0x20 / mov eax, [ebp+0x08] / push esi / push edi / push 8 / pop ecx
inline const std::array<unsigned char, kCxxThrowExceptionLength>& CxxThrowExceptionBytes() {
    static const std::array<unsigned char, kCxxThrowExceptionLength> bytes = {{
        0x55,                    // push ebp
        0x8B, 0xEC,              // mov ebp, esp
        0x83, 0xEC, 0x20,        // sub esp, 0x20
        0x8B, 0x45, 0x08,        // mov eax, dword ptr [ebp+8]   (the thrown object)
        0x56, 0x57,              // push esi; push edi
        0x6A, 0x08,              // push 8
        0x59,                    // pop ecx
    }};
    return bytes;
}

// mov eax, 0xA9D448 / call 0x00A60B98 (SEH setup) / sub esp, 0x5C / push ebx; push esi; push edi
inline const std::array<unsigned char, kMakeTileLength>& MakeTileBytes() {
    static const std::array<unsigned char, kMakeTileLength> bytes = {{
        0xB8, 0x48, 0xD4, 0xA9, 0x00,  // mov eax, 0xA9D448
        0xE8, 0xCA, 0x65, 0x42, 0x00,  // call 0x00A60B98
        0x83, 0xEC, 0x5C,              // sub esp, 0x5C
        0x53, 0x56, 0x57,              // push ebx; push esi; push edi
    }};
    return bytes;
}

// ret 8 (two dwords: the object and the ThrowInfo) closes _CxxThrowException at 0xA60BEE. Read from the stock exe
// only to keep the hook's signature honest: the hook forwards both, then returns.
constexpr std::uint32_t kAddr_CxxThrowExceptionRet = 0x00A60BEE;
// ret 0xC (three dwords plus this) closes MakeTile at 0x63AA09; 0x63AA07 is its push ebx / leave.
constexpr std::uint32_t kAddr_MakeTileRet = 0x0063AA09;

inline bool Matches(const unsigned char* live, const unsigned char* expected, std::size_t length) {
    return live && expected && std::memcmp(live, expected, length) == 0;
}

}  // namespace WzTracePatch

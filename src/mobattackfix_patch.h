#pragma once
// ============================================================
// mobattackfix_patch.h -- b251: entering Rose Garden 211080100 no longer closes the client (1.6.4).
//
// Header-only so tests/mobattackfix_patch_test.cpp can check the bytes and the decision without the client;
// mobattackfix.cpp installs it.
//
// THE CRASH (from the v4 trace, wztrace-20261104T072629Z.log line 4892: throw 00A605E7 hr=80004003,
// frame[2]=00A5FDEF, frame[3]=00529EB6, and the call-site chain re-checked byte by byte):
//
//   CField entry pass 00531B8D  -> 00A03350 -> 00679138 (calls get_field 00437A0C)
//     -> 006675A8 -> 0066E32D -> 0066A517 (per-object loop over a mob template's attack slots)
//       -> 00529E7B  lazy per-attack cache getter: count at this+0x218, 8-byte entries at this+0x234,
//                    returns 0 for an out-of-range index or an entry that cannot be built
//         -> 0067FD81  the ONLY caller of it in the whole exe: builds the mob's <attackN> attack info.
//                     00529E7B passes the 0-based attack slot; 0067FD99 does
//                       lea eax,[eax*4+0BEC4F8h]   (0BEC4C8 is the 40-entry mob action-name table and
//                       0BEC4F8 = +12 = "attack1", so the slot selects attack1..attackF), then it takes the
//                     mob's animation from the WZ animation cache singleton 0BE78D4 (0040C235) and casts
//                     the node to IWzProperty.
//           -> 004052AD  _com_ptr_t<IWzProperty>::operator=(IUnknown*): pushes &IID 00BD8308 and calls
//                        QueryInterface (vtable[0] at 004052C9). 00BD8308 is
//                        {986515D9-0A0B-4929-8B4F-718682177B92} = IID_IWzProperty (this repo's
//                        external/WzLib/include/WzLib/IWzProperty.h:7).
//                        The WZ object's QueryInterface returns 80004003 E_POINTER.
//
// Stock tolerates ONLY 80004002 E_NOINTERFACE there:
//
//   0067FE33  E8 75 54 D8 FF   call 004052AD
//   0067FE38  3B C3            cmp  eax, ebx          ; ebx == 0
//   0067FE3A  7D 0D            jge  0067FE49          ; S_OK -> continue
//   0067FE3C  3D 02 40 00 80   cmp  eax, 80004002h    ; E_NOINTERFACE tolerated
//   0067FE41  74 06            je   0067FE49
//   0067FE43  50               push eax
//   0067FE44  E8 9B FF 3D 00   call 00A5FDE4          ; _com_issue_error(hr) -> throw 00A605E7
//   0067FE49  8D 85 5C FF FF FF  lea eax,[ebp-0A4h]  ; the tolerated path
//
// Nothing in that chain has an __except for _com_error, so the client exits. The tolerated path is a
// fully supported state: 004052AD clears its destination on failure (004052D1..004052DD), so the copy at
// 0067FE64 tests null and takes 0067FE69 `mov [out+4], 0`; 0067FD81 returns its out object, 00538B3E
// copies a null pointer into the entry, 00529E7B returns 0, and every caller already tests for 0
// (0066A7AD `je 0066AE4D`). No partial object is published and the entry is simply retried later.
//
// THE FIX. The one call at 0067FE33 is routed through a cave that runs the stock cast and, only when the
// HRESULT is exactly 80004003, rewrites the return address to 0067FE49 (the path stock already has for
// E_NOINTERFACE). Every other HRESULT returns to 0067FE38, i.e. the byte-identical stock comparison and
// raise. The shared raiser 00A5FDE4 is never patched, so unrelated _com_errors still throw as before, and
// 0067FD81's other work is never skipped -- only the optional attack-info entry is left empty.
// config.ini [clienthax] mobattackfix=false keeps the stock throw. The bytes below are checked first; on a
// mismatch nothing is patched.
//
// ROOT-CAUSE NOTE (for whoever meets this shape again): the class of bug is "a WZ QueryInterface that
// returns E_POINTER from a _com_ptr_t cast is fatal". Other _com_ptr_t casts of WZ interfaces have the
// same tolerated-HRESULT set, so the same failure shape elsewhere would end the session the same way.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MobAttackFix {

constexpr std::uint32_t kAddr_Builder        = 0x0067FD81;   // builds one <attackN> record; called only from 00529E7B
constexpr std::uint32_t kAddr_GetAttackInfo  = 0x00529E7B;   // lazy per-attack cache getter (this+0x218 / +0x234)
constexpr std::uint32_t kAddr_QueryCast      = 0x004052AD;   // _com_ptr_t<IWzProperty>::operator=(IUnknown*), ret 4
constexpr std::uint32_t kAddr_Site           = 0x0067FE33;   // call 004052AD -- the patched call
constexpr std::uint32_t kAddr_StockCompare   = 0x0067FE38;   // cmp eax,ebx / jge / cmp eax,80004002h / je / throw
constexpr std::uint32_t kAddr_ToleratedPath  = 0x0067FE49;   // where the E_NOINTERFACE je lands
constexpr std::uint32_t kAddr_Raise          = 0x00A5FDE4;   // _com_issue_error(hr)
constexpr std::uint32_t kAddr_ThrowSite      = 0x00A605E7;   // the _CxxThrowException site the trace logged
constexpr std::uint32_t kAddr_ActionTable    = 0x00BEC4C8;   // 40 mob action names; +12 (0BEC4F8) = "attack1"
constexpr std::uint32_t kAddr_AnimationCache = 0x00BE78D4;   // WZ animation/action singleton
constexpr std::uint32_t kAddr_IidWzProperty  = 0x00BD8308;   // &IID_IWzProperty {986515D9-...}
constexpr std::uint32_t kEPointer            = 0x80004003;   // E_POINTER -- the observed, fatal HRESULT
constexpr std::uint32_t kENoInterface        = 0x80004002;   // E_NOINTERFACE -- stock already tolerates it
constexpr std::size_t kSiteLength            = 5;

inline const std::array<unsigned char, kSiteLength>& OriginalBytes() {
    static const std::array<unsigned char, kSiteLength> bytes = {{
        0xE8, 0x75, 0x54, 0xD8, 0xFF,       // call 004052AD
    }};
    return bytes;
}

// call rel32 to the cave. Same length as the stock instruction, so nothing else is overwritten.
inline std::array<unsigned char, kSiteLength> PatchedBytes(std::uint32_t uCave) {
    const std::uint32_t rel = uCave - (kAddr_Site + 5);
    return {{0xE8, static_cast<unsigned char>(rel), static_cast<unsigned char>(rel >> 8),
             static_cast<unsigned char>(rel >> 16), static_cast<unsigned char>(rel >> 24)}};
}

enum class SiteState { Original, Patched, Unknown };

// Patched: a call whose rel32 lands in the cave (this mod, after a restart) -- the tests classify with a
// concrete cave address, the client only ever sees Original or Unknown before it writes.
inline SiteState Classify(const unsigned char* current) {
    if (std::memcmp(current, OriginalBytes().data(), kSiteLength) == 0) return SiteState::Original;
    if (current[0] == 0xE8) return SiteState::Patched;
    return SiteState::Unknown;
}

// What the cave does with the HRESULT the stock cast returned. This is the executable decision the cave
// implements; it is kept here so a test can cover the whole table without a client.
enum class Resume { StockCompare, ToleratedPath };

inline Resume ResumeFor(std::int32_t hr) {
    return static_cast<std::uint32_t>(hr) == kEPointer ? Resume::ToleratedPath : Resume::StockCompare;
}

// The stock branch at kAddr_StockCompare, as a predicate: any negative HRESULT except E_NOINTERFACE raises
// a _com_error. (jge is signed; the cmp/je pair is the E_NOINTERFACE exception.)
inline bool StockRaises(std::int32_t hr) {
    return hr < 0 && static_cast<std::uint32_t>(hr) != kENoInterface;
}

// Bytes the cave relies on, checked by the test against the exe. Preceding pushes: the interface-pointer
// slot is pushed at 0067FE2F and the _com_ptr_t destination is in ECX, which is what makes 004052AD the
// callee and gives the cave the stock stack.
constexpr std::uint32_t kAddr_ContextBefore = 0x0067FE29;    // lea eax,[ebp-84h] / push eax / lea ecx,[ebp-48h]
constexpr std::size_t kContextBeforeLength  = 10;
inline const std::array<unsigned char, kContextBeforeLength>& ContextBefore() {
    static const std::array<unsigned char, kContextBeforeLength> bytes = {{
        0x8D, 0x85, 0x7C, 0xFF, 0xFF, 0xFF,     // lea eax,[ebp-0x84]   (the source pointer slot)
        0x50,                                   // push eax
        0x8D, 0x4D, 0xB8,                       // lea ecx,[ebp-0x48]   (the _com_ptr_t destination)
    }};
    return bytes;
}

// 0067FE38..0067FE49: the stock comparison and raise the cave falls back into, and the je/jge targets.
constexpr std::size_t kStockBranchLength = 17;
inline const std::array<unsigned char, kStockBranchLength>& StockBranch() {
    static const std::array<unsigned char, kStockBranchLength> bytes = {{
        0x3B, 0xC3,                             // cmp eax, ebx
        0x7D, 0x0D,                             // jge 0067FE49
        0x3D, 0x02, 0x40, 0x00, 0x80,           // cmp eax, 80004002h
        0x74, 0x06,                             // je  0067FE49
        0x50,                                   // push eax
        0xE8, 0x9B, 0xFF, 0x3D, 0x00,           // call 00A5FDE4
    }};
    return bytes;
}

// 0067FE49: the tolerated path the cave jumps to. It starts with a lea, so it neither reads nor needs the
// flags the cave left behind (only EBP, which the cave never writes).
constexpr std::size_t kToleratedPathLength = 6;
inline const std::array<unsigned char, kToleratedPathLength>& ToleratedPath() {
    static const std::array<unsigned char, kToleratedPathLength> bytes = {{
        0x8D, 0x85, 0x5C, 0xFF, 0xFF, 0xFF,     // lea eax,[ebp-0xA4]
    }};
    return bytes;
}

// 004052AD is a __thiscall that ends `ret 4`, so it pops its own argument: the stack on entry to the cave's
// continuation is identical to stock. Its epilogue restores EBX/ESI/EDI and puts the HRESULT in EAX, which
// is exactly the contract the cave keeps (it writes only EAX, transiently, and restores it).
constexpr std::uint32_t kAddr_QueryCast_Epilogue = 0x00405305;
constexpr std::size_t kQueryCastEpilogueLength = 8;
inline const std::array<unsigned char, kQueryCastEpilogueLength>& QueryCastEpilogue() {
    static const std::array<unsigned char, kQueryCastEpilogueLength> bytes = {{
        0x5E,                                   // pop esi
        0x8B, 0xC3,                             // mov eax, ebx   (the HRESULT)
        0x5B,                                   // pop ebx
        0x5D,                                   // pop ebp
        0xC2, 0x04, 0x00,                       // ret 4
    }};
    return bytes;
}

}  // namespace MobAttackFix

#pragma once
// ============================================================
// mapobj_guard.h -- what CMapLoadable::MakeObj_hook does when the engine cannot build ONE map object
// because the map's art is not in the client's Data. Header-only and Windows-free, tested by
// tests/mapobj_guard_test.cpp.
//
// Why: map 272000000 (Time Temple) places Obj/acc10.img/timeTemple/pillar_old/7 (17 entries, n =
// 0,1,2,3,4,5,7,9,10), and the client's Data/Map/Obj/acc10.img has no pillar_old under timeTemple.
// Stock CMapLoadable::RestoreObj (0x0063AA7E) walks every layer and, at 0x0063ACB5, calls
// CMapLoadable::MakeObj (0x0063AD16) for each object; the return address is 0x0063ACBA, where the
// routine loads its own local instead of eax -- the object is inserted into the layer by MakeObj
// itself, so its return value is ignored. When the art lookup fails, stock MakeObj ends in
// _com_issue_error(E_POINTER, 0x80004003) thrown out of the map load, nothing catches it, and the
// client closes (QA trace 2026-11-04 07:38:30Z: last lookup Map/Obj/acc10.img/timeTemple/pillar_old/7,
// then throw 00A605E7 hr=80004003, crash_reports/LATEST_CRASH.txt is that run).
//
// The guard is the same trade the linked-canvas guard already makes (inlink_guard.cpp, b103): one
// broken map object costs that object, not the player's session. MakeObj has not inserted the object
// when the art lookup fails (the lookup precedes the insert), so returning nullptr leaves the layer
// one object short instead of half built.
//
// Only the failures that mean "the data this object asked for is not there" are skipped. Anything
// else -- a genuine allocator failure, an unexpected state -- is rethrown, so a guard never hides a
// bug it does not understand. The switch is config.ini [clienthax] mapobjguard (default true).
// ============================================================

#include <cstdint>
#include <string>

namespace MapObjGuard {

// The stock sites this guard stands on, verified against
// client-1.5.0-fixes2/exe-copy/MapleStory.exe (sha256 86E4E6BE...8AAC90, file offset = VA - 0x400000).
constexpr std::uint32_t kAddr_RestoreObj = 0x0063AA7E;         // CMapLoadable::RestoreObj
constexpr std::uint32_t kAddr_MakeObjCall = 0x0063ACB5;        // E8 5C 00 00 00  call CMapLoadable::MakeObj
constexpr std::uint32_t kAddr_MakeObjReturn = 0x0063ACBA;      // 8B 45 F0        mov eax, [ebp-0x10]; eax ignored
constexpr std::uint32_t kAddr_MakeObj = 0x0063AD16;            // B8 BF D7 A9 00  SEH prologue of MakeObj

// kAddr_MakeObjCall: the call that enters MakeObj for one object.
inline constexpr unsigned char kMakeObjCallBytes[5] = {0xE8, 0x5C, 0x00, 0x00, 0x00};
// kAddr_MakeObjReturn: the instruction right after it, which does not read eax.
inline constexpr unsigned char kMakeObjReturnBytes[3] = {0x8B, 0x45, 0xF0};
// kAddr_MakeObj: the prologue the hook detours (lamps.cpp ATTACH_HOOK(CMapLoadable::MakeObj)).
inline constexpr unsigned char kMakeObjPrologue[16] = {0xB8, 0xBF, 0xD7, 0xA9, 0x00, 0xE8, 0x78, 0x5E,
                                                       0x42, 0x00, 0x81, 0xEC, 0x2C, 0x01, 0x00, 0x00};

// A COM HRESULT MakeObj can throw when the object's art is missing or unreadable.
constexpr long kE_POINTER = static_cast<long>(0x80004003u);          // a null WZ node reached a COM wrapper
constexpr long kE_FAIL = static_cast<long>(0x80004005u);             // the lookup failed some other way
constexpr long kStgEFileNotFound = static_cast<long>(0x80030002u);   // STG_E_FILENOTFOUND: no such node
constexpr long kStgEPathNotFound = static_cast<long>(0x80030003u);   // STG_E_PATHNOTFOUND: no such folder

enum class Action {
    Skip,       // the object is missing data: leave it out and keep the map load
    Propagate,  // not a missing-data failure: rethrow untouched
};

// The HRESULTs a missing map object produces. Everything else is rethrown.
inline Action Classify(long hr) {
    switch (hr) {
    case kE_POINTER:
    case kE_FAIL:
    case kStgEFileNotFound:
    case kStgEPathNotFound:
        return Action::Skip;
    default:
        return Action::Propagate;
    }
}

// "acc10", "timeTemple", "pillar_old", "7" -> L"Obj/acc10.img/timeTemple/pillar_old/7".
// Empty parts are left out (a map entry may omit l2), so a log line never names a path the client
// cannot have asked for.
inline std::wstring Describe(const std::wstring& oS, const std::wstring& l0, const std::wstring& l1,
                             const std::wstring& l2) {
    std::wstring path = L"Obj/";
    path += oS.empty() ? L"?" : oS;
    path += L".img";
    for (const std::wstring* part : {&l0, &l1, &l2}) {
        if (part->empty()) continue;
        path += L'/';
        path += *part;
    }
    return path;
}

// The diagnostic label of a skipped HRESULT, for the log line.
inline const char* ReasonLabel(long hr) {
    switch (hr) {
    case kE_POINTER:
        return "E_POINTER (a null WZ node)";
    case kE_FAIL:
        return "E_FAIL";
    case kStgEFileNotFound:
        return "STG_E_FILENOTFOUND";
    case kStgEPathNotFound:
        return "STG_E_PATHNOTFOUND";
    default:
        return "unexpected";
    }
}

}  // namespace MapObjGuard

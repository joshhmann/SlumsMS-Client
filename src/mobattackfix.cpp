// ============================================================
// mobattackfix.cpp: b251 -- entering Rose Garden 211080100 no longer closes the client (1.6.4).
//
// The client lazily builds a mob's per-attack attack info (00529E7B -> 0067FD81) and casts the WZ attack
// node to IWzProperty (_com_ptr_t<IWzProperty>::operator=, 004052AD). For that mob the WZ object's
// QueryInterface returns 80004003 E_POINTER instead of 80004002 E_NOINTERFACE; stock only tolerates
// E_NOINTERFACE (0067FE3C..0067FE41) and otherwise raises a _com_error (0067FE44 -> 00A5FDE4 -> 00A605E7)
// that nothing in the field-entry chain catches, so the session ends. mobattackfix_patch.h has the
// addresses, the checked bytes and the caller chain.
//
// This routes the one call at 0067FE33 through a cave: the stock cast runs, and only an E_POINTER result
// continues at 0067FE49 -- the path stock already takes for E_NOINTERFACE, where the entry is left empty
// (00529E7B returns 0 and its callers skip it). Anything else continues at 0067FE38, byte-identical stock.
// config.ini [clienthax] mobattackfix=false keeps the stock throw.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "constants.h"
#include "attackstop_patch.h"
#include "mobattackfix_patch.h"

namespace {

// Not const: the cave reads them, and a static initialisation order trap here would be a cave reading 0.
uintptr_t g_queryCast = MobAttackFix::kAddr_QueryCast;
uintptr_t g_toleratedPath = MobAttackFix::kAddr_ToleratedPath;

// Entered by the patched `call` at 0067FE33, so the stack is exactly what the stock callee sees:
//   [esp]   = 0067FE38 (the stock continuation)
//   [esp+4] = the interface-pointer slot the builder pushed at 0067FE2F
// ECX still holds the _com_ptr_t destination (0067FE30), which is the only other thing 004052AD takes.
//
// Contract, which mobattackfix_patch.h's test pins down:
//   * EAX (the HRESULT) is preserved for both continuations; no other register is written (EBX, ESI, EDI
//     and EBP are never touched here, and 004052AD restores EBX/ESI/EDI itself).
//   * `ret 4` leaves the stack exactly as 004052AD's own `ret 4` would: the argument is popped and the
//     return address is consumed, so 0067FE38 / 0067FE49 start with the same ESP as stock.
//   * Flags are dead: 0067FE38 begins with `cmp eax,ebx` and 0067FE49 with a `lea`.
//   * No exception can unwind through this frame: the only call it makes (004052AD) does not raise, and
//     the raising instruction (0067FE44) is only reached after we have returned into stock code.
__declspec(naked) void QueryCastCave() {
    __asm {
        mov     eax, dword ptr [esp + 4]
        push    eax
        call    dword ptr [g_queryCast]
        cmp     eax, 80004003h              // E_POINTER: the observed b251 failure
        jne     resumeStock
        push    eax
        mov     eax, dword ptr [g_toleratedPath]
        mov     dword ptr [esp + 4], eax    // [esp+4] is the return address (the push above shifted it)
        pop     eax
    resumeStock:
        ret     4
    }
}

// True = leave the stock throw in place. Absent key means the fix is on; "false"/"0" is the escape hatch.
bool KeepStockThrow() {
    char value[32] = {};
    GetPrivateProfileStringA("clienthax", "mobattackfix", "", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    return !integration::ParseIniBool(value).value_or(true);
}

}  // namespace

void AttachMobAttackFixMod() {
    using namespace MobAttackFix;
    if (KeepStockThrow()) {
        DebugMessage("Mob attack fix: stock throw kept ([clienthax] mobattackfix=false)");
        return;
    }
    const auto* site = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(kAddr_Site));
    switch (Classify(site)) {
    case SiteState::Patched:
        DebugMessage("Mob attack fix: %08X is already patched; leaving it alone", kAddr_Site);
        return;
    case SiteState::Unknown:
        DebugMessage("Mob attack fix: unexpected client bytes at %08X; stock throw kept", kAddr_Site);
        return;
    default:
        break;
    }
    const auto bytes = PatchedBytes(static_cast<std::uint32_t>(reinterpret_cast<uintptr_t>(&QueryCastCave)));
    PatchMemory(reinterpret_cast<void*>(static_cast<uintptr_t>(kAddr_Site)), const_cast<unsigned char*>(bytes.data()),
        bytes.size());
    DebugMessage("Mob attack fix: a mob attack whose WZ node is not an IWzProperty no longer closes the client");
}

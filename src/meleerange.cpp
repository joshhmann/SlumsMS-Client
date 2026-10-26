// ============================================================
// meleerange.cpp: another character's melee attack in a pose its weapon has no range for no longer closes the
// client (1.6.0). meleerange_patch.h has the analysis.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "meleerange_patch.h"

namespace {

// Ztl_bstr_t is one pointer to its Data_t {wchar_t* wide, char* narrow, long refs}; passed by value, the stock
// function releases it.
auto CActionMan__GetMeleeAttackRange = reinterpret_cast<void(__thiscall*)(void*, void*, int, RECT*)>(
    static_cast<uintptr_t>(MeleeRange::kAddr_GetMeleeAttackRange));

int g_logged = 0;   // lines written this session; a bot repeating the pose would flood the log
constexpr int kMaxLogLines = 20;

void CopyAfterImageName(void* bstr, char* out, size_t n) {
    out[0] = 0;
    __try {
        const wchar_t* w = bstr ? *reinterpret_cast<const wchar_t* const*>(bstr) : nullptr;
        if (!w) return;
        size_t i = 0;
        for (; i + 1 < n && w[i]; ++i) out[i] = w[i] < 0x80 ? static_cast<char>(w[i]) : '?';
        out[i] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(out, n, "?");
    }
}

int ThrowFilter(unsigned long code) {
    return code == MeleeRange::kCxxException ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

bool CallStock(void* pThis, void* bstr, int nAction, RECT* rc) {
    __try {
        CActionMan__GetMeleeAttackRange(pThis, bstr, nAction, rc);
        return true;
    } __except (ThrowFilter(GetExceptionCode())) {
        return false;
    }
}

void __fastcall GetMeleeAttackRange_hook(void* pThis, void* /*edx*/, void* bstr, int nAction, RECT* rc) {
    char name[32];
    if (g_logged < kMaxLogLines) CopyAfterImageName(bstr, name, sizeof(name));   // the stock call releases it
    if (CallStock(pThis, bstr, nAction, rc)) return;
    if (rc) SetRectEmpty(rc);
    if (g_logged < kMaxLogLines) {
        ++g_logged;
        DebugMessage("Melee range: afterimage '%s' has no range for pose %d; attack drawn without one", name, nAction);
    }
}

}  // namespace

void AttachMeleeRangeMod() {
    using namespace MeleeRange;
    const auto* site = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(kAddr_GetMeleeAttackRange));
    switch (Classify(site)) {
    case SiteState::Detoured:
        DebugMessage("Melee range: 0x%08X is already detoured; stock behaviour kept", kAddr_GetMeleeAttackRange);
        return;
    case SiteState::Unknown:
        DebugMessage("Melee range: unexpected client bytes at 0x%08X; stock behaviour kept", kAddr_GetMeleeAttackRange);
        return;
    default:
        break;
    }
    ATTACH_HOOK(CActionMan__GetMeleeAttackRange, GetMeleeAttackRange_hook);
    DebugMessage("Melee range: a remote attack in a pose without a range no longer closes the client");
}
